// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "txdb.h"
#include "walletdb.h"
#include "walletdb-recover.h"   // BerkeleyRecoverWallet / BerkeleyZapWalletTx
#include "walletmigrate.h"      // MaybeMigrateBerkeleyWalletToSQLite / IsSQLiteFile
#include "trianglesrpc.h"
#include "net.h"
#include "netbase.h"
#include "init.h"
#include "util.h"
#include "ui_interface.h"
#include "checkpoints.h"
#include "smessage.h"
#include "openssl_compat.h"
#include "bootstrap.h"
#include "utxosnapshot.h"
#include "snapshotnet.h"
#include "tor/tor_embedded.h"
#include "tor/onion_v3.h"
#include "tor/tor_process.h"
#include "i2p/i2p_embedded.h"
#include "i2p/i2pseed.h"
#ifdef ENABLE_ZMQ
#include "zmqpublishnotifier.h"
#endif
#include "notificationqueue.h"
#include "addressindex.h"
#include "chaindb_migrate.h"
#include <memory>
#include <thread>
#include <vector>

// Forward declaration: InitError / InitWarning are defined further down
// in this file but referenced by AppInit (line ~423) before the definition.
static bool InitError(const std::string& str);
static bool InitWarning(const std::string& str);
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <openssl/crypto.h>

#ifndef WIN32
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

// Windows.h (transitively included) defines these as macros, clobbering Checkpoints:: enum values.
#ifdef STRICT
#undef STRICT
#endif
#ifdef ADVISORY
#undef ADVISORY
#endif
#ifdef PERMISSIVE
#undef PERMISSIVE
#endif

using namespace std;
namespace fs = std::filesystem;

namespace {
// Acquire an exclusive, non-blocking advisory lock on the datadir .lock file
// and hold it for the lifetime of the process. Replaces
// boost::interprocess::file_lock. The descriptor/handle is intentionally never
// released — the OS drops the lock automatically when the process exits.
bool LockDataDirectory(const std::filesystem::path& pathLockFile)
{
#ifdef WIN32
    HANDLE hFile = CreateFileA(pathLockFile.string().c_str(),
                               GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                               nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;
    OVERLAPPED ov = {};
    if (!LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                    0, MAXDWORD, MAXDWORD, &ov)) {
        CloseHandle(hFile);
        return false;
    }
    return true; // handle held until process exit
#else
    int fd = open(pathLockFile.string().c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0)
        return false;
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        return false;
    }
    return true; // fd held until process exit
#endif
}
} // namespace

std::unique_ptr<CWallet> pwalletMain;
CClientUIInterface uiInterface;
std::string strWalletFileName;
bool fConfChange;
bool fEnforceCanonical;
unsigned int nNodeLifespan;
unsigned int nDerivationMethodIndex;

bool fUseFastIndex;
enum Checkpoints::CPMode CheckpointsMode;

static CCriticalSection cs_DeferredStartup;
static bool fDeferredStartupRunning = false;
static std::unique_ptr<std::vector<std::thread>> pScriptCheckThreads;

static void ThreadScriptCheck()
{
    RenameThread("Triangles-scrchk");
    if (pScriptCheckQueue)
        pScriptCheckQueue->Thread();
}

static void StartupPerfLog(const char* phase, int64_t elapsedMs)
{
    printf("STARTUP-PERF: %s %" PRId64 "ms\n", phase, elapsedMs);
}

static void StartupPerfLog(const char* phase, int64_t elapsedMs, const std::string& detail)
{
    if (detail.empty())
    {
        StartupPerfLog(phase, elapsedMs);
        return;
    }
    printf("STARTUP-PERF: %s %" PRId64 "ms %s\n", phase, elapsedMs, detail.c_str());
}

//////////////////////////////////////////////////////////////////////////////
//
// Shutdown
//

void ExitTimeout(void* parg)
{
#ifdef WIN32
    MilliSleep(5000);
    ExitProcess(0);
#endif
}

// Wait up to maxWaitSec for at least minPeers peers to have reported their
// chain height via the version handshake. Returns the median peer height, or
// -1 if we couldn't get enough peers (timeout, no peers, all nStartingHeight=-1).
int WaitForPeerHeights(int minPeers, int maxWaitSec)
{
    const int pollIntervalMs = 500;
    const int64_t deadline = GetTimeMillis() + (int64_t)maxWaitSec * 1000;

    while (GetTimeMillis() < deadline && !fRequestShutdown) {
        std::vector<int> heights;
        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes) {
                if (pnode && pnode->nStartingHeight > 0)
                    heights.push_back(pnode->nStartingHeight);
            }
        }
        if ((int)heights.size() >= minPeers) {
            std::sort(heights.begin(), heights.end());
            int median = heights[heights.size() / 2];
            printf("AutoRebuild: got %zu peer heights; median=%d\n", heights.size(), median);
            return median;
        }
        MilliSleep(pollIntervalMs);
    }

    std::vector<int> heights;
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes) {
            if (pnode && pnode->nStartingHeight > 0)
                heights.push_back(pnode->nStartingHeight);
        }
    }
    if (heights.empty()) {
        printf("AutoRebuild: no peers reported heights after %ds\n", maxWaitSec);
        return -1;
    }
    std::sort(heights.begin(), heights.end());
    int median = heights[heights.size() / 2];
    printf("AutoRebuild: timed out with %zu peers; median=%d\n", heights.size(), median);
    return median;
}

// If -autorerebuild is set and our local chain is more than that many blocks
// behind the median peer height, wipe the chain DB (preserving wallet.dat +
// onion + smsg state) and request shutdown. On restart, the daemon sees no
// chain DB and the snapshot path takes over.
void MaybeAutoRebuild(int thresholdBlocks)
{
    if (thresholdBlocks <= 0)
        return;

    if (nBestHeight < 0) {
        printf("AutoRebuild: local nBestHeight unset — skipping\n");
        return;
    }

    printf("AutoRebuild: enabled (threshold=%d blocks). Local chain tip: %d\n",
           thresholdBlocks, nBestHeight);
    int medianPeer = WaitForPeerHeights(/*minPeers=*/3, /*maxWaitSec=*/60);
    if (medianPeer <= 0) {
        printf("AutoRebuild: could not get peer heights — skipping rebuild\n");
        return;
    }

    int lag = medianPeer - nBestHeight;
    printf("AutoRebuild: peer median=%d, local=%d, lag=%d\n",
           medianPeer, nBestHeight, lag);

    if (lag < thresholdBlocks) {
        printf("AutoRebuild: lag %d < threshold %d — no rebuild needed\n",
               lag, thresholdBlocks);
        return;
    }

    printf("\n*** AutoRebuild: chain is %d blocks behind — wiping chain DB ***\n", lag);
    printf("*** Preserving wallet.dat, smsgDB, onion state. ***\n");
    printf("*** Daemon will shutdown; restart to load signed UTXO snapshot. ***\n\n");

    WipeChainDataDir();

    fs::path blkPath = GetDataDir() / "blk0001.dat";
    if (fs::exists(blkPath)) {
        fs::remove(blkPath);
        printf("AutoRebuild: removed stale %s\n", blkPath.string().c_str());
    }

    StartShutdown();
}

void StartShutdown()
{
fRequestShutdown = true;
#ifdef QT_GUI
    // ensure we leave the Qt main loop for a clean GUI exit (Shutdown() is called in triangles.cpp afterwards)
    uiInterface.QueueShutdown();
#else
    // Without UI, Shutdown() can simply be started in a new thread
    NewThread(Shutdown, nullptr);
#endif
}

bool ShutdownRequested()

{
    return fRequestShutdown;
}

// P2P UTXO snapshot fetcher. Started from AppInit2 step 11.6 when the chain
// is empty and snapshot mode is enabled. Saves utxo-snapshot.bin on success
// and requests shutdown so a fresh boot can load it via Step 6c.
static void ThreadSnapshotFetch(void* parg)
{
    RenameThread("Triangles-snapfetch");
    // Give peers ~30s to connect and complete version handshake.
    for (int i = 0; i < 30 && !fRequestShutdown; ++i)
        MilliSleep(1000);
    if (fRequestShutdown) return;

    int snapTimeoutSec = (int)GetArg("-snapshottimeout", 600);
    printf("SnapshotNet: starting P2P snapshot fetch (timeout=%ds)...\n", snapTimeoutSec);

    std::string err;
    if (SnapshotNet::TryFetchSnapshot(GetDataDir(), snapTimeoutSec, err)) {
        printf("SnapshotNet: snapshot saved. Shutting down — restart the daemon to load it.\n");
        uiInterface.InitMessage(_("UTXO snapshot saved. Restart the node to load it."));
        StartShutdown();
    } else {
        printf("SnapshotNet: P2P snapshot fetch failed: %s\n", err.c_str());
        printf("SnapshotNet: falling back to genesis sync. Use -bootstrap for legacy HTTP fallback.\n");
    }
}

void ThreadDeferredStartup(void* parg)
{
    // Make this thread recognisable as the deferred startup worker.
    RenameThread("Triangles-postinit");

    int64_t nTotalStart = GetTimeMillis();
    printf("Starting deferred startup tasks...\n");
    try
    {
        if (!fShutdown)
        {
            int64_t nStart = GetTimeMillis();
            SecureMsgStart(fNoSmsg, GetBoolArg("-smsgscanchain"));
            printf(" securemsg   %15" PRId64 "ms\n", GetTimeMillis() - nStart);
            StartupPerfLog("deferred.securemsg", GetTimeMillis() - nStart);
        }

        if (!fShutdown && pwalletMain)
        {
            int64_t nStart = GetTimeMillis();
            pwalletMain->ReacceptWalletTransactions();
            printf(" reaccept    %15" PRId64 "ms\n", GetTimeMillis() - nStart);
            StartupPerfLog("deferred.reaccept_wallet_transactions", GetTimeMillis() - nStart);
        }

        printf("Deferred startup tasks finished %" PRId64 "ms\n", GetTimeMillis() - nTotalStart);
        StartupPerfLog("deferred.total", GetTimeMillis() - nTotalStart);
    }
    catch (std::exception& e)
    {
        PrintExceptionContinue(&e, "ThreadDeferredStartup()");
    }
    catch (...)
    {
        PrintExceptionContinue(nullptr, "ThreadDeferredStartup()");
    }

    {
        LOCK(cs_DeferredStartup);
        fDeferredStartupRunning = false;
    }
}

void Shutdown(void* parg)
{
    static CCriticalSection cs_Shutdown;
    static bool fTaken;

    // Make this thread recognisable as the shutdown thread
    RenameThread("Triangles-shutoff");

    // Belt-and-suspenders: spawn a watchdog that force-exits if Shutdown()
    // doesn't complete in 30 seconds. This protects against deadlock in the
    // embedded Tor/I2P teardown paths (see notes/wallet-close-hang-fix-2026-07-07.md).
    std::thread([]()
    {
#ifdef WIN32
        Sleep(30000);
        fprintf(stderr, "Shutdown watchdog: 30s elapsed, force-exiting process\n");
        fflush(stderr);
        ExitProcess(1);
#else
        sleep(30);
        fprintf(stderr, "Shutdown watchdog: 30s elapsed, force-exiting process\n");
        fflush(stderr);
        _exit(1);
#endif
    }).detach();

    bool fFirstThread = false;
    {
        TRY_LOCK(cs_Shutdown, lockShutdown);
        if (lockShutdown)
        {
            fFirstThread = !fTaken;
            fTaken = true;
        }
    }
    static bool fExit;
    if (fFirstThread)
    {
        fShutdown = true;

        int64_t nDeferredWaitStart = GetTimeMillis();
        while (true)
        {
            bool fDeferredRunning;
            {
                LOCK(cs_DeferredStartup);
                fDeferredRunning = fDeferredStartupRunning;
            }
            if (!fDeferredRunning || GetTimeMillis() - nDeferredWaitStart > 5000)
                break;
            MilliSleep(50);
        }

        SecureMsgShutdown();

        // Stop network threads FIRST so nothing references Tor objects
        nTransactionsUpdated++;
        StopNode();

        if (pScriptCheckQueue)
        {
            pScriptCheckQueue->Quit();
            if (pScriptCheckThreads)
            {
                for (std::thread& t : *pScriptCheckThreads)
                    if (t.joinable()) t.join();
                pScriptCheckThreads.reset();
            }
            pScriptCheckQueue.reset();
        }

        // Stop the embedded I2P router.
        StopEmbeddedI2P();

        // NOW safe to destroy Tor state - all threads have stopped
        ShutdownTorV3();
        StopEmbeddedTor();

#ifdef ENABLE_ZMQ
        if (pzmqNotifier)
        {
            pzmqNotifier->Shutdown();
            delete pzmqNotifier;
            pzmqNotifier = nullptr;
        }
#endif

        if (pNotificationQueue)
        {
            delete pNotificationQueue;
            pNotificationQueue = nullptr;
        }

//        MakeChainDB()->Close();
        bitdb.Flush(false);
        bitdb.Flush(true);
        fs::remove(GetPidFile());
        UnregisterWallet(pwalletMain.get());
        pwalletMain.reset();
        // DB is flushed and wallet saved - safe to force-exit if something hangs
        NewThread(ExitTimeout, nullptr);
        MilliSleep(50);
        printf("Triangles exited\n\n");
        fExit = true;
#ifndef QT_GUI
        // ensure non-UI client gets exited here, but let Triangles-Qt reach 'return 0;' in triangles.cpp
        exit(0);
#endif
    }
    else
    {
        while (!fExit)
            MilliSleep(500);
        MilliSleep(100);
        ExitThread(0);
    }
}

void HandleSIGTERM(int)
{
    fRequestShutdown = true;
}

void HandleSIGHUP(int)
{
    fReopenDebugLog = true;
}





//////////////////////////////////////////////////////////////////////////////
//
// Start
//
#if !defined(QT_GUI)
bool AppInit(int argc, char* argv[])
{
    bool fRet = false;
    try
    {
        //
        // Parameters
        //
        // If Qt is used, parameters/triangles.conf are parsed in qt/triangles.cpp's main()
        ParseParameters(argc, argv);
        if (!fs::is_directory(GetDataDir(false)))
        {
            fprintf(stderr, "Error: Specified directory does not exist\n");
            Shutdown(nullptr);
        }
        ReadConfigFile(mapArgs, mapMultiArgs);

        // AUDIT: If notorious=1 or -notor was set in triangles.conf, scream
        // loudly.  This is the silent path that put DNS2 on a 5+ day clearnet
        // fork in 2026-06-23 — operator flipped it for troubleshooting, never
        // reverted it, and the daemon happily started in clearnet-only mode.
        // We refuse to proceed unless -recovery-mode=1 is ALSO set, even if
        // the flag was set in the config file rather than on the command line.
        if (mapArgs.count("-notor") && !GetBoolArg("-recovery-mode", false)) {
            return InitError(_(
                "-notor=1 found in triangles.conf or command line.  Triangles is "
                "Tor-native; running without Tor is unsafe and produces silent "
                "clearnet forks (see 2026-06-23 DNS2 incident).  If this is an "
                "explicit recovery operation, pass -recovery-mode=1 on the command "
                "line (in addition to the config file setting) to acknowledge."));
        }

        if (mapArgs.count("-?") || mapArgs.count("--help"))
        {
            // First part of help message is specific to trianglesd / RPC client
            std::string strUsage = _("Triangles version") + " " + FormatFullVersion() + "\n\n" +
                _("Usage:") + "\n" +
                  "  trianglesd [options]                     " + "\n" +
                  "  trianglesd [options] <command> [params]  " + _("Send command to -server or trianglesd") + "\n" +
                  "  trianglesd [options] help                " + _("List commands") + "\n" +
                  "  trianglesd [options] help <command>      " + _("Get help for a command") + "\n";

            strUsage += "\n" + HelpMessage();

            fprintf(stdout, "%s", strUsage.c_str());
            return false;
        }

        // Command-line RPC
        for (int i = 1; i < argc; i++)
            if (!IsSwitchChar(argv[i][0]) && !std::equal(std::begin("Triangles:"), std::end("Triangles:") - 1, argv[i], [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); }))
                fCommandLine = true;

        if (fCommandLine)
        {
            int ret = CommandLineRPC(argc, argv);
            exit(ret);
        }

        fRet = AppInit2();
    }
    catch (std::exception& e) {
        PrintException(&e, "AppInit()");
    } catch (...) {
        PrintException(nullptr, "AppInit()");
    }
    if (!fRet)
        Shutdown(nullptr);
    return fRet;
}

extern void noui_connect();
int main(int argc, char* argv[])
{
    bool fRet = false;

    // Connect trianglesd signal handlers
    noui_connect();

    fRet = AppInit(argc, argv);

    if (fRet && fDaemon)
        return 0;

    return 1;
}
#endif

bool static InitError(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, _("Triangles"), CClientUIInterface::OK | CClientUIInterface::MODAL);
    return false;
}

bool static InitWarning(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, _("Triangles"), CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION | CClientUIInterface::MODAL);
    return true;
}


bool static Bind(const CService &addr, bool fError = true) {
    if (IsLimited(addr))
        return false;
    std::string strError;
    if (!BindListenPort(addr, strError)) {
        if (fError)
            return InitError(strError);
        return false;
    }
    return true;
}

// Core-specific options shared between UI and daemon
std::string HelpMessage()
{
    string strUsage = _("Options:") + "\n" +
        "  -?                     " + _("This help message") + "\n" +
        "  -conf=<file>           " + _("Specify configuration file (default: triangles.conf)") + "\n" +
        "  -pid=<file>            " + _("Specify pid file (default: trianglesd.pid)") + "\n" +
        "  -datadir=<dir>         " + _("Specify data directory") + "\n" +
        "  -wallet=<dir>          " + _("Specify wallet file (within data directory)") + "\n" +
        "  -dbcache=<n>           " + _("Set database cache size in megabytes (default: 25)") + "\n" +
        "  -dblogsize=<n>         " + _("Set database disk log size in megabytes (default: 100)") + "\n" +
        "  -timeout=<n>           " + _("Specify connection timeout in milliseconds (default: 5000)") + "\n" +
        "  -torconnecttimeout=<n> " + _("Max time (ms) for the SOCKS5 handshake with the Tor proxy (send+recv of SOCKS5 init/auth/connect). Bounds how long a dead/slow .onion can stall the connector thread (default: 60000, range 5000-180000)") + "\n" +
        //"  -proxy=<ip:port>       " + _("Connect through socks proxy") + "\n" +
        //"  -socks=<n>             " + _("Select the version of socks proxy to use (4-5, default: 5)") + "\n" +
        "  -tor=<ip:port>         " + _("Use proxy to reach tor hidden services (default: same as -proxy)") + "\n"
        "  -notor                 " + _("Disable Tor - run in clearnet-only mode (no .onion connectivity)") + "\n" +
        "  -torsocks=<port>       " + _("Set embedded or managed Tor SOCKS proxy port (default: 19099)") + "\n" +
        "  -torhiddenservice      " + _("Enable the managed Tor hidden service (default: 1)") + "\n" +
        "  -torhsport=<port>      " + _("Set embedded or managed Tor hidden service port (default: wallet listen port)") + "\n"
        "  -i2p                   " + _("Enable embedded I2P router for .b32.i2p connectivity (default: 1)") + "\n"
        "  -i2psocks=<port>       " + _("Set embedded I2P SOCKS proxy port (default: 19100)") + "\n"
        "  -i2psam=<port>         " + _("Set embedded I2P SAM bridge port (default: 7656)") + "\n"
        "  -i2phsport=<port>      " + _("Set I2P server tunnel forward port (default: wallet listen port)") + "\n" +
        //"  -dns                   " + _("Allow DNS lookups for -addnode, -seednode and -connect") + "\n" +
        "  -port=<port>           " + _("Listen for connections on <port> (default: 24112 or testnet: 24111)") + "\n" +
        "  -maxconnections=<n>    " + _("Maintain at most <n> connections to peers (default: 125)") + "\n" +
        "  -maxoutboundconnections=<n> " + _("Maximum outbound connections (default: 8, range 4-32)") + "\n" +
        "  -addnode=<ip>          " + _("Add a node to connect to and attempt to keep the connection open") + "\n" +
        "  -connect=<ip>          " + _("Connect only to the specified node(s)") + "\n" +
        "  -seednode=<ip>         " + _("Connect to a node to retrieve peer addresses, and disconnect") + "\n" +
        "  -externalip=<ip>       " + _("Specify your own public address") + "\n" +
        //"  -onlynet=<net>         " + _("Only connect to nodes in network <net> (IPv4, IPv6 or Tor)") + "\n" +
        //"  -discover              " + _("Discover own IP address (default: 1 when listening and no -externalip)") + "\n" +
        //"  -irc                   " + _("Find peers using internet relay chat (default: 0)") + "\n" +
        //"  -listen                " + _("Accept connections from outside (default: 1 if no -proxy or -connect)") + "\n" +
        //"  -bind=<addr>           " + _("Bind to given address. Use [host]:port notation for IPv6") + "\n" +
        //  -dnsseed               " + _("Find peers using DNS lookup (default: 1)") + "\n" +
        "  -staking               " + _("Stake your coins to support network and gain reward (default: 1)") + "\n" +
        "  -synctime              " + _("Sync time with other nodes. Disable if time on your system is precise e.g. syncing with NTP (default: 1)") + "\n" +
        "  -cppolicy              " + _("Sync checkpoints policy (default: strict)") + "\n" +
        "  -onionseed             " + _("Find peers using .onion seeds (default: 1 unless -connect)") + "\n" +
        "  -seedurl=<host>        " + _("HTTP seed list host (default: seeds.cryptographic-triangles.org)") + "\n" +
        "  -noseedurl             " + _("Disable HTTP seed list fetch on startup") + "\n" +
        "  -autorerebuild=<n>     " + _("If our chain is more than <n> blocks behind peers, wipe chain DB and shutdown for clean restart (default: 0=disabled)") + "\n" +
        "  -banscore=<n>          " + _("Threshold for disconnecting misbehaving peers (default: 100)") + "\n" +
        "  -bantime=<n>           " + _("Number of seconds to keep misbehaving peers from reconnecting (default: 86400)") + "\n" +
        "  -par=<n>               " + _("Set the number of script verification threads (default: auto, 0 = auto, 1 = single-threaded)") + "\n" +
        "  -maxreceivebuffer=<n>  " + _("Maximum per-connection receive buffer, <n>*1000 bytes (default: 5000)") + "\n" +
        "  -maxsendbuffer=<n>     " + _("Maximum per-connection send buffer, <n>*1000 bytes (default: 1000)") + "\n" +
#ifdef USE_UPNP
#if USE_UPNP
        "  -upnp                  " + _("Use UPnP to map the listening port (default: 1 when listening)") + "\n" +
#else
        "  -upnp                  " + _("Use UPnP to map the listening port (default: 0)") + "\n" +
#endif
#endif
        "  -detachdb              " + _("Detach block and address databases. Increases shutdown time (default: 0)") + "\n" +
        "  -paytxfee=<amt>        " + _("Fee per KB to add to transactions you send") + "\n" +
        //"  -mininput=<amt>        " + _("When creating transactions, ignore inputs with value less than this (default: 0.01)") + "\n" +
#ifdef QT_GUI
        "  -server                " + _("Accept command line and JSON-RPC commands") + "\n" +
#endif
#if !defined(WIN32) && !defined(QT_GUI)
        "  -daemon                " + _("Run in the background as a daemon and accept commands") + "\n" +
#endif
        "  -testnet               " + _("Use the test network") + "\n" +
        "  -debug                 " + _("Output extra debugging information. Implies all other -debug* options") + "\n" +
        "  -debugnet              " + _("Output extra network debugging information") + "\n" +
        "  -logtimestamps         " + _("Prepend debug output with timestamp") + "\n" +
        "  -shrinkdebugfile       " + _("Shrink debug.log file on client startup (default: 1 when no -debug)") + "\n" +
        "  -printtoconsole        " + _("Send trace/debug info to console instead of debug.log file") + "\n" +
#ifdef WIN32
        "  -printtodebugger       " + _("Send trace/debug info to debugger") + "\n" +
#endif
        "  -rpcuser=<user>        " + _("Username for JSON-RPC connections") + "\n" +
        "  -rpcpassword=<pw>      " + _("Password for JSON-RPC connections") + "\n" +
        "  -rpcport=<port>        " + _("Listen for JSON-RPC connections on <port> (default: 19111 or testnet: 19112)") + "\n" +
        "  -rpcallowip=<ip>       " + _("Allow JSON-RPC connections from specified IP address") + "\n" +
        "  -rpcconnect=<ip>       " + _("Send commands to node running on <ip> (default: 127.0.0.1)") + "\n" +
        "  -blocknotify=<cmd>     " + _("Execute command when the best block changes (%s in cmd is replaced by block hash)") + "\n" +
        "  -walletnotify=<cmd>    " + _("Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)") + "\n" +
        "  -confchange            " + _("Require a confirmations for change (default: 0)") + "\n" +
        "  -enforcecanonical      " + _("Enforce transaction scripts to use canonical PUSH operators (default: 1)") + "\n" +
        "  -upgradewallet         " + _("Upgrade wallet to latest format") + "\n" +
        "  -keypool=<n>           " + _("Set key pool size to <n> (default: 100)") + "\n" +
        "  -rescan                " + _("Rescan the block chain for missing wallet transactions") + "\n" +
        "  -postibdrescan         " + _("Run the wallet rescan after initial sync in a background thread (default: 1)") + "\n" +
        "  -zapwallettxes         " + _("Delete all wallet transactions and only recover from blockchain on startup") + "\n" +
        "  -salvagewallet         " + _("Attempt to recover private keys from a corrupt wallet.dat") + "\n" +
        "  -checkblocks=<n>       " + _("How many blocks to check at startup (default: 2500, 0 = all)") + "\n" +
        "  -checklevel=<n>        " + _("How thorough the block verification is (0-6, default: 1)") + "\n" +
        "  -loadblock=<file>      " + _("Imports blocks from external blk000?.dat file") + "\n" +

        "\n" + _("Block creation options:") + "\n" +
        "  -blockminsize=<n>      "   + _("Set minimum block size in bytes (default: 0)") + "\n" +
        "  -blockmaxsize=<n>      "   + _("Set maximum block size in bytes (default: 250000)") + "\n" +
        "  -blockprioritysize=<n> "   + _("Set maximum size of high-priority/low-fee transactions in bytes (default: 27000)") + "\n" +

        "\n" + _("SSL options: (see the Triangles Wiki for SSL setup instructions)") + "\n" +
        "  -rpcssl                                  " + _("Use OpenSSL (https) for JSON-RPC connections") + "\n" +
        "  -rpcsslcertificatechainfile=<file.cert>  " + _("Server certificate file (default: server.cert)") + "\n" +
        "  -rpcsslprivatekeyfile=<file.pem>         " + _("Server private key (default: server.pem)") + "\n" +
        "  -rpcsslciphers=<ciphers>                 " + _("Acceptable ciphers (default: TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!AH:!3DES:@STRENGTH)") + "\n" +

        "\n" + _("REST API options:") + "\n" +
        "  -rest                                    " + _("Enable public REST API on RPC port (default: 0)") + "\n" +
        "  -restcorsorigin=<origin>                 " + _("CORS Access-Control-Allow-Origin header (default: *)") + "\n" +
        "  -restapikey=<key>                        " + _("Bearer token for authenticated wallet endpoints") + "\n" +
        "  -restratelimit=<n>                       " + _("Max requests/sec per IP for public endpoints (default: 30, 0=disabled)") + "\n" +

        "\n" + _("Secure messaging options:") + "\n" +
        "  -nosmsg                                  " + _("Disable secure messaging.") + "\n" +
        "  -debugsmsg                               " + _("Log extra debug messages.") + "\n" +
        "  -smsgscanchain                           " + _("Scan the block chain for public key addresses on startup.") + "\n";

    return strUsage;
}

/** Sanity checks
 *  Ensure that Triangles is running in a usable environment with all
 *  necessary library support.
 */
bool InitSanityCheck(void)
{
    if(!ECC_InitSanityCheck()) {
        InitError("OpenSSL appears to lack support for elliptic curve cryptography. For more "
                  "information, visit https://en.bitcoin.it/wiki/OpenSSL_and_EC_Libraries");
        return false;
    }

    // TODO: remaining sanity checks, see #4081

    return true;
}

/** Initialize Triangles.
 *  @pre Parameters should be parsed and config file should be read.
 */
bool AppInit2()
{
    const int64_t nAppInitStart = GetTimeMillis();
    // ********************************************************* Step 1: setup
#ifdef _MSC_VER
    // Turn off Microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, 0));
#endif
#if _MSC_VER >= 1400
    // Disable confusing "helpful" text message on abort, Ctrl-C
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifdef WIN32
    // Enable Data Execution Prevention (DEP)
    // Minimum supported OS versions: WinXP SP3, WinVista >= SP1, Win Server 2008
    // A failure is non-critical and needs no further attention!
#ifndef PROCESS_DEP_ENABLE
// We define this here, because GCCs winbase.h limits this to _WIN32_WINNT >= 0x0601 (Windows 7),
// which is not correct. Can be removed, when GCCs winbase.h is fixed!
#define PROCESS_DEP_ENABLE 0x00000001
#endif
    typedef BOOL (WINAPI *PSETPROCDEPPOL)(DWORD);
    PSETPROCDEPPOL setProcDEPPol = (PSETPROCDEPPOL)GetProcAddress(GetModuleHandleA("Kernel32.dll"), "SetProcessDEPPolicy");
    if (setProcDEPPol != nullptr) setProcDEPPol(PROCESS_DEP_ENABLE);
#endif
#ifndef WIN32
    umask(077);

    // Clean shutdown on SIGTERM
    struct sigaction sa;
    sa.sa_handler = HandleSIGTERM;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    // Reopen debug.log on SIGHUP
    struct sigaction sa_hup;
    sa_hup.sa_handler = HandleSIGHUP;
    sigemptyset(&sa_hup.sa_mask);
    sa_hup.sa_flags = 0;
    sigaction(SIGHUP, &sa_hup, nullptr);
#endif

    // ********************************************************* Step 2: parameter interactions

    nNodeLifespan = GetArg("-addrlifespan", 7);
    fUseFastIndex = GetBoolArg("-fastindex", true);
    //nMinerSleep = GetArg("-minersleep", 500);

    CheckpointsMode = Checkpoints::STRICT;
    std::string strCpMode = GetArg(std::string_view{"-cppolicy"}, std::string_view{"strict"});

    if(strCpMode == "strict")
        CheckpointsMode = Checkpoints::STRICT;

    if(strCpMode == "advisory")
        CheckpointsMode = Checkpoints::ADVISORY;

    if(strCpMode == "permissive")
        CheckpointsMode = Checkpoints::PERMISSIVE;

    nDerivationMethodIndex = 0;

    fTestNet = GetBoolArg("-testnet");
    if (fTestNet) {
        SoftSetBoolArg("-irc", true);
    }

    if (mapArgs.count("-bind")) {
        // when specifying an explicit binding address, you want to listen on it
        // even when -connect or -proxy is specified
        SoftSetBoolArg("-listen", true);
    }

    if (mapArgs.count("-connect") && mapMultiArgs["-connect"].size() > 0) {
        // when only connecting to trusted nodes, do not seed via .onion, or listen by default
        SoftSetBoolArg("-onionseed", false);
        SoftSetBoolArg("-listen", false);
    }

    if (mapArgs.count("-proxy")) {
        // to protect privacy, do not listen by default if a proxy server is specified
        SoftSetBoolArg("-listen", false);
    }

    //if (!GetBoolArg("-listen", true)) {
        // do not map ports or try to retrieve public IP when not listening (pointless)
        //triangles: never listen, always using tor.
        //SoftSetBoolArg("-upnp", false);
        //SoftSetBoolArg("-discover", false);
    //}

    //if (mapArgs.count("-externalip")) {
        // if an explicit public IP is specified, do not try to find others
    //    SoftSetBoolArg("-discover", false);
    //}

    if (GetBoolArg("-salvagewallet")) {
        // Rewrite just private keys: rescan to find transactions
        SoftSetBoolArg("-rescan", true);
    }

    if (GetBoolArg("-zapwallettxes")) {
        // Zap all tx from wallet: rescan to rebuild from blockchain
        SoftSetBoolArg("-rescan", true);
    }

    // ********************************************************* Step 3: parameter-to-internal-flags

    fDebug = GetBoolArg("-debug");

    // -debug implies fDebug*
    if (fDebug)
    {
        fDebugNet  = true;
        fDebugSmsg = true;
    } else
    {
        fDebugNet  = GetBoolArg("-debugnet");
        fDebugSmsg = GetBoolArg("-debugsmsg");
    }
    fNoSmsg = GetBoolArg("-nosmsg");
    
    bitdb.SetDetach(GetBoolArg("-detachdb", false));

#if !defined(WIN32) && !defined(QT_GUI)
    fDaemon = GetBoolArg("-daemon");
#else
    fDaemon = false;
#endif

    if (fDaemon)
        fServer = true;
    else
        fServer = GetBoolArg("-server");

    /* force fServer when running without GUI */
#if !defined(QT_GUI)
    fServer = true;
#endif
    fPrintToConsole = GetBoolArg("-printtoconsole");
    fPrintToDebugger = GetBoolArg("-printtodebugger");
    fLogTimestamps = GetBoolArg("-logtimestamps");

    if (mapArgs.count("-timeout"))
    {
        int nNewTimeout = GetArg("-timeout", 5000);
        if (nNewTimeout > 0 && nNewTimeout < 600000)
            nConnectTimeout = nNewTimeout;
    }

    // SOCKS5/Tor negotiation timeout. Separate from -timeout (which only covers
    // the instant local connect to the Tor SOCKS proxy); this bounds the
    // SOCKS5 handshake (send+recv of init/auth/connect). On a dead/slow .onion
    // the recv() in Socks5() would otherwise block until Tor's own ~120s
    // SocksTimeout fires, holding an outbound connection slot.
    if (mapArgs.count("-torconnecttimeout"))
    {
        int nTorTimeout = GetArg("-torconnecttimeout", 60000);
        if (IsValidSocksNegotiationTimeout(nTorTimeout))
            nSocksNegotiationTimeout = nTorTimeout;
        else
            InitWarning("Ignoring -torconnecttimeout=" + mapArgs["-torconnecttimeout"] +
                        ": out of range (5000..180000 ms), using default 60000");
    }

    if (mapArgs.count("-paytxfee"))
    {
        if (!ParseMoney(mapArgs["-paytxfee"], nTransactionFee))
            return InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s'"), mapArgs["-paytxfee"].c_str()));
        if (nTransactionFee > 0.25 * COIN)
            InitWarning(_("Warning: -paytxfee is set very high! This is the transaction fee you will pay if you send a transaction."));
    }

    fConfChange = GetBoolArg("-confchange", false);
    fEnforceCanonical = GetBoolArg("-enforcecanonical", true);

    // Validate -maxoutboundconnections (range 4-32, default 8)
    if (mapArgs.count("-maxoutboundconnections"))
    {
        int nMaxOutboundConn = GetArg("-maxoutboundconnections", 8);
        if (nMaxOutboundConn < 4 || nMaxOutboundConn > 32)
            InitWarning("Ignoring -maxoutboundconnections=" + mapArgs["-maxoutboundconnections"] +
                        ": out of range (4..32), using default 8");
    }

    int nScriptCheckThreads = GetArg("-par", 0);
    if (nScriptCheckThreads <= 0)
        nScriptCheckThreads = std::thread::hardware_concurrency();
    if (nScriptCheckThreads > 16)
        nScriptCheckThreads = 16;
    if (nScriptCheckThreads > 1)
    {
        pScriptCheckQueue = std::make_unique<CCheckQueue<CScriptCheck>>(32);
        pScriptCheckThreads = std::make_unique<std::vector<std::thread>>();
        for (int i = 0; i < nScriptCheckThreads - 1; ++i)
            pScriptCheckThreads->emplace_back(&ThreadScriptCheck);
        printf("Script verification threads: %d workers + main thread\n", nScriptCheckThreads - 1);
    }

    fAddressIndex = GetBoolArg("-addressindex", false);
    if (fAddressIndex)
        printf("Address index enabled\n");

    if (mapArgs.count("-mininput"))
    {
        if (!ParseMoney(mapArgs["-mininput"], nMinimumInputValue))
            return InitError(strprintf(_("Invalid amount for -mininput=<amount>: '%s'"), mapArgs["-mininput"].c_str()));
    }

    // ********************************************************* Step 4: application initialization: dir lock, daemonize, pidfile, debug log
    // Sanity check
    if (!InitSanityCheck())
        return InitError(_("Initialization sanity check failed. Triangles is shutting down."));

    std::string strDataDir = GetDataDir().string();
    std::string strWalletFileName = GetArg(std::string_view{"-wallet"}, std::string_view{"wallet.dat"});

    // strWalletFileName must be a plain filename without a directory
    if (strWalletFileName != fs::path(strWalletFileName).stem().string() + fs::path(strWalletFileName).extension().string())
        return InitError(strprintf(_("Wallet %s resides outside data directory %s."), strWalletFileName.c_str(), strDataDir.c_str()));

	// Make sure only a single Triangles process is using the data directory.
    fs::path pathLockFile = GetDataDir() / ".lock";
    FILE* file = fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file) fclose(file);
    if (!LockDataDirectory(pathLockFile))
        return InitError(strprintf(_("Cannot obtain a lock on data directory %s.  Triangles is probably already running."), strDataDir.c_str()));

#if !defined(WIN32) && !defined(QT_GUI)
    if (fDaemon)
    {
        // Daemonize
        pid_t pid = fork();
        if (pid < 0)
        {
            fprintf(stderr, "Error: fork() returned %d errno %d\n", pid, errno);
            return false;
        }
        if (pid > 0)
        {
            CreatePidFile(GetPidFile(), pid);
            return true;
        }

        pid_t sid = setsid();
        if (sid < 0)
            fprintf(stderr, "Error: setsid() returned %d errno %d\n", sid, errno);
    }
#endif

    if (GetBoolArg("-shrinkdebugfile", !fDebug))
        ShrinkDebugFile();
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    printf("Triangles version %s (%s)\n", FormatFullVersion().c_str(), CLIENT_DATE.c_str());
    printf("Using OpenSSL version %s\n", TrianglesOpenSSLVersionString());
    if (!fLogTimestamps)
        printf("Startup time: %s\n", DateTimeStrFormat("%x %H:%M:%S", GetTime()).c_str());
    printf("Default data directory %s\n", GetDefaultDataDir().string().c_str());
    printf("Used data directory %s\n", strDataDir.c_str());
    std::ostringstream strErrors;

    if (fDaemon)
        fprintf(stdout, "Triangles server starting\n");

    int64_t nStart;

    // ********************************************************* Step 5: verify database integrity

    uiInterface.InitMessage(_("Verifying database integrity..."));
    nStart = GetTimeMillis();

    // The pre-rebase Berkeley-only paths (salvagewallet, zapwallettxes,
    // bitdb.Verify, and the Berkeley→SQLite migration hook itself) only
    // apply to a wallet.dat that is still a Berkeley DB file. Once the
    // migration has run — or if the user is starting with a wallet that was
    // already SQLite — those steps would either no-op or (worse) misinterpret
    // the SQLite file as a corrupt Berkeley file and abort startup.
    //
    // The SQLite backend runs its own PRAGMA integrity_check in
    // SQLiteDatabase::Open(), so the wallet is validated against the SQLite
    // schema before the wallet handle is ever constructed downstream.
    //
    // Note: the snapshot is taken AFTER any migration hook below, so that
    // post-migration the verify/salvage paths are skipped automatically.
    bool walletIsSqlite = false;

    if (!bitdb.Open(GetDataDir()))
    {
        string msg = strprintf(_("Error initializing database environment %s!"
                                 " To recover, BACKUP THAT DIRECTORY, then remove"
                                 " everything from it except for wallet.dat."), strDataDir.c_str());
        return InitError(msg);
    }

    if (GetBoolArg("-salvagewallet"))
    {
        // Recover readable keypairs (Berkeley path; only relevant for legacy
        // wallet.dat files that haven't been migrated to SQLite yet):
        if (!BerkeleyRecoverWallet(bitdb, strWalletFileName, true))
            return false;
    }

    if (GetBoolArg("-zapwallettxes") && fs::exists(GetDataDir() / strWalletFileName))
    {
        uiInterface.InitMessage(_("Zapping all transactions from wallet..."));
        if (!BerkeleyZapWalletTx(strWalletFileName))
            return InitError(_("Error: could not zap wallet transactions"));
    }

    // ── Wallet backend migration ──────────────────────────────────────────────
    // The daemon now defaults to SQLite (-walletdb=sqlite). If the wallet file
    // on disk is still a Berkeley DB, convert it non-destructively to a SQLite
    // wallet here, before the CWalletDB handle is opened downstream. The
    // Berkeley original is preserved as "<name>.bdb.bak" alongside.
    if (ResolveWalletDbKind() == WalletDbKind::SQLite &&
        fs::exists(GetDataDir() / strWalletFileName) &&
        !IsSQLiteFile(GetDataDir() / strWalletFileName))
    {
        uiInterface.InitMessage(_("Migrating wallet from Berkeley DB to SQLite..."));
        std::string migErr;
        if (!MaybeMigrateBerkeleyWalletToSQLite(GetDataDir() / strWalletFileName, migErr))
            return InitError(_("Wallet migration failed: ") + migErr);
        // Snapshot AFTER migration so the post-migration verify step below
        // is skipped automatically when the wallet is now SQLite.
        walletIsSqlite =
            fs::exists(GetDataDir() / strWalletFileName) &&
            IsSQLiteFile(GetDataDir() / strWalletFileName);
    }
    else
    {
        walletIsSqlite =
            fs::exists(GetDataDir() / strWalletFileName) &&
            IsSQLiteFile(GetDataDir() / strWalletFileName);
    }

    if (!walletIsSqlite)
    {
        if (fs::exists(GetDataDir() / strWalletFileName))
        {
            CDBEnv::VerifyResult r = bitdb.Verify(strWalletFileName, BerkeleyRecoverWallet);
            if (r == CDBEnv::RECOVER_OK)
            {
                string msg = strprintf(_("Warning: wallet.dat corrupt, data salvaged!"
                                         " Original wallet.dat saved as wallet.{timestamp}.bak in %s; if"
                                         " your balance or transactions are incorrect you should"
                                         " restore from a backup."), strDataDir.c_str());
                uiInterface.ThreadSafeMessageBox(msg, _("Triangles"), CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION | CClientUIInterface::MODAL);
            }
            if (r == CDBEnv::RECOVER_FAIL)
                return InitError(_("wallet.dat corrupt, salvage failed"));
        }
    }
    StartupPerfLog("verify_db", GetTimeMillis() - nStart, strprintf("wallet=%s wallet_is_sqlite=%d", strWalletFileName.c_str(), (int)walletIsSqlite));

    // ********************************************************* Step 6: network initialization
    nStart = GetTimeMillis();

    //int nSocksVersion = GetArg("-socks", 5);
    //
    //if (nSocksVersion != 4 && nSocksVersion != 5)
    //  return InitError(strprintf(_("Unknown -socks proxy version requested: %i"), nSocksVersion));

    // Network selection: Tor-native mode
    // All traffic routes through embedded Tor. Clearnet (IPv4/IPv6) is disabled
    // after Tor starts successfully. Only .onion peers are accepted.
    if (mapArgs.count("-onlynet")) {
        std::set<enum Network> nets;
        for (std::string snet : mapMultiArgs["-onlynet"]) {
            enum Network net = ParseNetwork(snet);
            if (net == NET_UNROUTABLE)
                return InitError(strprintf(_("Unknown network specified in -onlynet: '%s'"), snet.c_str()));
            nets.insert(net);
        }
        for (int n = 0; n < NET_MAX; n++) {
            enum Network net = (enum Network)n;
            if (!nets.count(net))
                SetLimited(net);
        }
    }

    // Tor proxy: always configured for .onion connectivity
    CService addrOnion;
    unsigned short const onion_port = static_cast<unsigned short>(GetArg("-torsocks", 19099));

    if (mapArgs.count("-tor") && mapArgs["-tor"] != "0") {
        addrOnion = CService(mapArgs["-tor"], onion_port);
        if (!addrOnion.IsValid())
            return InitError(strprintf(_("Invalid -tor address: '%s'"), mapArgs["-tor"].c_str()));
    } else {
        addrOnion = CService("127.0.0.1", onion_port);
    }

    SetProxy(NET_TOR, addrOnion, 5);
    SetReachable(NET_TOR);

    // see Step 2: parameter interactions for more information about these
    fNoListen = !GetBoolArg("-listen", true);
    //fDiscover = GetBoolArg("-discover", true);
    //fNameLookup = GetBoolArg("-dns", true);
#ifdef USE_UPNP
    fUseUPnP = GetBoolArg("-upnp", USE_UPNP);
#endif
    bool fBound = false;
    if (true) {
        if (true) {
            do {
                // W1: Bind to all interfaces so external peers can connect.
                //
                // The previous code went through Lookup("0.0.0.0", ...) which
                // hands the literal string to getaddrinfo(). On Windows that
                // resolver can fail to map "0.0.0.0" to INADDR_ANY and the
                // daemon would abort at startup with "Cannot resolve binding
                // address". Construct the CService directly from INADDR_ANY
                // instead — this is the canonical "any-address" binding and
                // works on every platform without consulting the resolver.
                CService addrBind;
                struct in_addr any;
                any.s_addr = htonl(INADDR_ANY);
                addrBind = CService(any, GetListenPort());
                fBound |= Bind(addrBind);
            } while (false);
        }
        if (!fBound)
            return InitError(_("Failed to listen on any port."));
    }


    // Release the old Tor initialization mutex (no longer blocking on embedded Tor)
    triangles_tor_set_initialized();

    if (mapArgs.count("-externalip"))
    {
        for (string strAddr : mapMultiArgs["-externalip"]) {
            CService addrLocal(strAddr, GetListenPort(), fNameLookup);
            if (!addrLocal.IsValid())
                return InitError(strprintf(_("Cannot resolve -externalip address: '%s'"), strAddr.c_str()));
            AddLocal(CService(strAddr, GetListenPort(), fNameLookup), LOCAL_MANUAL);
        }
    }
    // Tor V3 onion address is registered after wallet loads (Step 8.5)

    if (mapArgs.count("-reservebalance")) // triangles: reserve balance amount
    {
        if (!ParseMoney(mapArgs["-reservebalance"], nReserveBalance))
        {
            InitError(_("Invalid amount for -reservebalance=<amount>"));
            return false;
        }
    }

   if (mapArgs.count("-checkpointkey")) // triangles: checkpoint master priv key
    {
        if (!Checkpoints::SetCheckpointPrivKey(GetArg(std::string_view{"-checkpointkey"}, std::string_view{""})))
            InitError(_("Unable to sign checkpoint, wrong checkpointkey?\n"));
    }

    for (string strDest : mapMultiArgs["-seednode"])
        AddOneShot(strDest);
    StartupPerfLog("network_init", GetTimeMillis() - nStart, strprintf("listen=%d seednodes=%" PRIszu, !fNoListen, mapMultiArgs["-seednode"].size()));

    // ********************************************************* Step 6b: bootstrap download (daemon)
    // Automatic: if data dir has no blockchain, bootstrap without asking.
    // Can also be forced with -bootstrap flag, or disabled with -nobootstrap.
    //
    // v5.9.5: P2P UTXO snapshot fetch is the default for fresh installs (Step 11.6).
    // The legacy clearnet HTTP bootstrap only runs when the user explicitly requests
    // it via -bootstrap, or when -snapshot=0 disables the P2P fetcher.
// Bootstrap auto-download works for both GUI and daemon.
    // GUI users get the same automatic bootstrap on fresh installs.
    {
        bool wantsBootstrap = GetBoolArg("-bootstrap", false);
        bool noBootstrap = GetBoolArg("-nobootstrap", false);
        bool snapshotMode = GetBoolArg("-snapshot", true);
        fs::path dataPath = GetDataDir();
        // Load any runtime trusted snapshot publisher override that was
        // persisted by a previous settrustedv2snapshotpublisher call.
        Bootstrap::LoadTrustedSnapshotPublisher();
        bool needsBootstrap = Bootstrap::NeedsBootstrap(dataPath);

        if (needsBootstrap && !noBootstrap) {
            printf("Bootstrap: no blockchain data found — downloading UTXO snapshot automatically.\n");
            printf("Bootstrap: (use -nobootstrap to skip)\n");
            uiInterface.InitMessage(_("Downloading UTXO snapshot..."));
            wantsBootstrap = true;
        }

    if (wantsBootstrap)
    {
        int64_t nBootstrapStart = GetTimeMillis();
        fs::path dataPath = GetDataDir();
        std::string host = Bootstrap::DEFAULT_HOST;
        std::string strError;

        int64_t lastGuiUpdate = 0;
        auto progressFn = [&lastGuiUpdate](int64_t bytesDownloaded, int64_t totalBytes) {
            if (totalBytes > 0) {
                printf("\rBootstrap: %lld / %lld MB (%lld%%)",
                       (long long)(bytesDownloaded / (1024*1024)),
                       (long long)(totalBytes / (1024*1024)),
                       (long long)((bytesDownloaded * 100) / totalBytes));
                fflush(stdout);
                // Update GUI status bar every ~1 MB
                int64_t now = GetTimeMillis();
                if (now - lastGuiUpdate > 1000) {
                    lastGuiUpdate = now;
                    std::string msg = strprintf("Downloading blockchain: %lld / %lld MB (%lld%%)",
                        (long long)(bytesDownloaded / (1024*1024)),
                        (long long)(totalBytes / (1024*1024)),
                        (long long)((bytesDownloaded * 100) / totalBytes));
                    uiInterface.InitMessage(msg);
                }
            }
        };

        // Try UTXO snapshot first (fast: ~2-10 MB download). Only attempted
        // if the configured backend's chain DB doesn't already exist.
        bool success = false;
        bool triedUtxoSnapshot = false;
        if (needsBootstrap && !fs::exists(GetChainDataDir())) {
            uiInterface.InitMessage(_("Downloading UTXO snapshot..."));
            printf("Bootstrap: trying UTXO snapshot from %s (fast path)...\n", host.c_str());

            std::string utxoError;
            if (Bootstrap::DownloadUtxoSnapshot(host, dataPath, progressFn, utxoError)) {
                printf("\nBootstrap: UTXO snapshot loaded — will sync remaining blocks from network.\n");
                success = true;
            } else {
                printf("\nBootstrap: UTXO snapshot unavailable: %s\n", utxoError.c_str());
                printf("Bootstrap: falling back to full bootstrap download...\n");
            }
            triedUtxoSnapshot = true;
        }

        // Fall back to full bootstrap.tar.gz if UTXO snapshot failed
        if (!success) {
            uiInterface.InitMessage(_("Downloading blockchain snapshot..."));
            printf("Bootstrap: contacting %s...\n", host.c_str());

            success = Bootstrap::DownloadBootstrap(host, dataPath, progressFn, strError);

            if (!success) {
                printf("\nBootstrap: failed: %s\n", strError.c_str());
                printf("Bootstrap: skipping, will sync from network.\n");
            } else {
                printf("\nBootstrap: done.\n");
            }
        }

        StartupPerfLog("bootstrap_download", GetTimeMillis() - nBootstrapStart,
            strprintf("host=%s success=%d utxo_snapshot=%d", host.c_str(), success, triedUtxoSnapshot));
    }
    } // end bootstrap scope

    // ********************************************************* Step 6c: manual UTXO snapshot loading
    // If utxo-snapshot.bin exists in data dir and the chain DB hasn't been
    // initialized for the configured backend, load it.
    {
        fs::path dataPath = GetDataDir();
        fs::path snapshotFile = dataPath / "utxo-snapshot.bin";
        fs::path chainDbDir = GetChainDataDir();

        if (fs::exists(snapshotFile) && !fs::exists(chainDbDir)) {
            printf("Found utxo-snapshot.bin — loading UTXO snapshot...\n");
            uiInterface.InitMessage(_("Loading UTXO snapshot..."));

            // Local file load: skip the checkpoint gate. The operator has
            // filesystem access, so the trust model is already equivalent
            // to direct chain state modification — a malicious local file
            // is no worse than a malicious chain DB. P2P-delivered
            // snapshots (SnapshotNet) keep the checkpoint gate on.
            std::string strError;
            if (UtxoSnapshot::LoadSnapshot(snapshotFile, dataPath, strError, /*requireCheckpoint=*/false)) {
                printf("UTXO snapshot loaded successfully.\n");
            } else {
                printf("UTXO snapshot load failed: %s\n", strError.c_str());
                printf("Will proceed with normal sync.\n");
            }
        }
    }

    // ********************************************************* Step 6d: LevelDB -> RocksDB chain DB migration
    // Runs when explicitly requested (-migratechaindb[force]) OR automatically
    // when RocksDB is the active backend and the only chain DB present is a
    // legacy LevelDB (txleveldb). This makes the RocksDB default transparent
    // for existing nodes: their chain state is copied (and verified) into a new
    // rocksdb/ directory on first launch, leaving the LevelDB source untouched
    // as a fallback. MaybeMigrateLevelDbToRocksDb() is a no-op when there is no
    // LevelDB source or a RocksDB directory already exists, so it is safe to
    // call on every startup.
    {
        bool fExplicit = GetBoolArg("-migratechaindb", false) ||
                         GetBoolArg("-migratechaindbforce", false);
        // A rocksdb/ directory containing the MIGRATION_INCOMPLETE marker is a
        // crashed previous migration, NOT a usable chain DB — treat it the same
        // as "no rocksdb yet" so the migration is retried instead of silently
        // opening a truncated database.
        bool fCrashedMigration = fs::exists(GetDataDir() / "rocksdb" / "MIGRATION_INCOMPLETE");
        bool fAuto = IsRocksDbChainBackend() &&
                     fs::exists(GetDataDir() / "txleveldb") &&
                     (!fs::exists(GetDataDir() / "rocksdb") || fCrashedMigration);
        if (fExplicit || fAuto)
        {
            uiInterface.InitMessage(_("Migrating chain database to RocksDB..."));
            if (fAuto && !fExplicit)
                printf("ChainDB: RocksDB backend active with a legacy LevelDB present%s; "
                       "migrating automatically.\n",
                       fCrashedMigration ? " and a previous migration was interrupted" : "");
            std::string strMigrateError;
            bool fForce = GetBoolArg("-migratechaindbforce", false);
            if (!MaybeMigrateLevelDbToRocksDb(fForce, strMigrateError))
                return InitError(strprintf(_("Chain DB migration failed: %s"), strMigrateError.c_str()));
        }
        // Last line of defense: never open a RocksDB that still carries the
        // incomplete-migration marker (e.g. the LevelDB source was deleted so
        // the migration cannot be retried). Opening it would silently run on a
        // partial chain state.
        if (IsRocksDbChainBackend() &&
            fs::exists(GetDataDir() / "rocksdb" / "MIGRATION_INCOMPLETE"))
        {
            return InitError(_("The RocksDB chain database is left over from an interrupted "
                               "migration and is incomplete. Delete the 'rocksdb' directory in the "
                               "data directory and restart (it will be rebuilt by migration or resync)."));
        }
    }

    // ********************************************************* Step 7: load blockchain

    if (!bitdb.Open(GetDataDir()))
    {
        string msg = strprintf(_("Error initializing database environment %s!"
                                 " To recover, BACKUP THAT DIRECTORY, then remove"
                                 " everything from it except for wallet.dat."), strDataDir.c_str());
        return InitError(msg);
    }

    if (GetBoolArg("-loadblockindextest"))
    {
        auto txdb_holder = MakeChainDB("r"); CTxDBBase& txdb = *txdb_holder;
        txdb.LoadBlockIndex();
        PrintBlockTree();
        return false;
    }

    // Handle -reindex: delete the chain DB so it gets rebuilt from the raw
    // blk*.dat files. This recalculates money
    // supply, tx index, and UTXO set from scratch. Backend-agnostic via
    // WipeChainDataDir(), which resolves the directory per the configured
    // -chaindb backend.
    if (GetBoolArg("-reindex", false))
    {
        printf("Reindex requested: removing chain database...\n");
        uiInterface.InitMessage(_("Removing chain database for reindex..."));
        WipeChainDataDir();
    }

    uiInterface.InitMessage(_("Loading block index..."));
    printf("Loading block index...\n");
    nStart = GetTimeMillis();
    if (!LoadBlockIndex())
        return InitError(_("Error loading blkindex.dat"));

    // pindexLastHardenedCheckpoint is initialized from the hardened checkpoint
    // map on startup, BEFORE the daemon opens any peer connections or
    // processes any block messages. It is intentionally NOT advanced at
    // runtime — see fix/consensus-convergence.
    //
    // GetLastCheckpoint(mapBlockIndex) returns the newest compiled
    // checkpoint present in this node's local block index. On current
    // master (2026-07) the newest compiled checkpoint is whatever block
    // hash is highest in src/checkpoints.cpp::mapCheckpoints and present
    // in the local index; it is NOT hardcoded to block 2,205,000 here.
    // The downstream rules that consume this variable are:
    //   - main.cpp Reorganize(): reject reorgs whose fork point is at
    //     or below the checkpoint height (convergence rule, see the
    //     comment block above the rejection in Reorganize()).
    //   - main.cpp getheaders handler: when the peer's locator contains
    //     the checkpoint, serve canonical headers from the checkpoint
    //     forward; otherwise fall back to the last common ancestor (or
    //     genesis if none). This is the recovery path for forked peers.
    {
        CBlockIndex* pCheckpoint = Checkpoints::GetLastCheckpoint(mapBlockIndex);
        if (pCheckpoint && pCheckpoint != pindexLastHardenedCheckpoint)
        {
            pindexLastHardenedCheckpoint = pCheckpoint;
            printf("STARTUP-CHECKPOINT: pindexLastHardenedCheckpoint set to block %d (%s) from compiled hardened checkpoint\n",
                pindexLastHardenedCheckpoint->nHeight, pindexLastHardenedCheckpoint->GetBlockHash().ToString().substr(0,20).c_str());
        }
        else if (!pCheckpoint)
        {
            printf("STARTUP-CHECKPOINT: WARNING — no compiled hardened checkpoint present in local block index, pindexLastHardenedCheckpoint remains NULL\n");
        }
    }

    // AutoRebuild: if -autorerebuild is set and we are behind peers, wipe chain DB
    // and shutdown for clean restart.
    MaybeAutoRebuild(GetArg("-autorerebuild", 0));
    if (fRequestShutdown) {
        printf("AutoRebuild: shutdown requested before chain load complete\n");
        return false;
    }

    // Block index loaded. With fast-import removed, the only supported sync path
    // is the UTXO snapshot (auto-downloaded from bootstrap or placed manually in datadir).

    // as LoadBlockIndex can take several minutes, it's possible the user
    // requested to kill triangles-qt during the last operation. If so, exit.
    // As the program has not fully started yet, Shutdown() is possibly overkill.
    if (fRequestShutdown)
    {
        printf("Shutdown requested. Exiting.\n");
        return false;
    }
    printf(" block index %15" PRId64 "ms\n", GetTimeMillis() - nStart);
    StartupPerfLog("block_index", GetTimeMillis() - nStart, strprintf("bestheight=%d indexsize=%" PRIszu, nBestHeight, mapBlockIndex.size()));

    // Diagnostic: check for blocks in mapBlockIndex above pindexBest
    {
        int nMaxIndexHeight = 0;
        int nAboveBest = 0;
        for (std::map<uint256, CBlockIndex*>::iterator it = mapBlockIndex.begin();
             it != mapBlockIndex.end(); ++it)
        {
            if (it->second->nHeight > nMaxIndexHeight)
                nMaxIndexHeight = it->second->nHeight;
            if (it->second->nHeight > nBestHeight)
                nAboveBest++;
        }
        printf("SYNC-DIAG: mapBlockIndex=%d entries, maxHeight=%d, bestHeight=%d, aboveBest=%d\n",
            (int)mapBlockIndex.size(), nMaxIndexHeight, nBestHeight, nAboveBest);
    }

    if (GetBoolArg("-printblockindex") || GetBoolArg("-printblocktree"))
    {
        PrintBlockTree();
        return false;
    }

    if (mapArgs.count("-printblock"))
    {
        string strMatch = mapArgs["-printblock"];
        int nFound = 0;
        for (map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
        {
            uint256 hash = (*mi).first;
            if (strncmp(hash.ToString().c_str(), strMatch.c_str(), strMatch.size()) == 0)
            {
                CBlockIndex* pindex = (*mi).second;
                CBlock block;
                if (!block.ReadFromDisk(pindex))
                {
                    printf("Error: Failed to read block %s from disk\n", hash.ToString().c_str());
                    continue;
                }
                block.BuildMerkleTree();
                block.print();
                printf("\n");
                nFound++;
            }
        }
        if (nFound == 0)
            printf("No blocks matching %s were found\n", strMatch.c_str());
        return false;
    }

    // ********************************************************* Testing Zerocoin


    // ********************************************************* Step 8: load wallet

    uiInterface.InitMessage(_("Loading wallet..."));
    printf("Loading wallet...\n");
    nStart = GetTimeMillis();
    bool fFirstRun = true;
    pwalletMain = std::make_unique<CWallet>(strWalletFileName);

    // Auto-backup wallet.dat before loading (protects against corruption during load/flush)
    {
        fs::path walletPath = GetDataDir() / strWalletFileName;
        if (fs::exists(walletPath)) {
            uintmax_t wsize = fs::file_size(walletPath);
            printf("Wallet file size: %llu bytes\n", (unsigned long long)wsize);
            if (wsize < 1024) {
                strErrors << _("WARNING: wallet.dat is suspiciously small (") << wsize << _(" bytes). It may be corrupt.\n");
                printf("WARNING: wallet.dat is only %llu bytes - possibly corrupt!\n", (unsigned long long)wsize);
            }
            AutoBackupWallet(walletPath);
        }
    }

    DBErrors nLoadWalletRet = pwalletMain->LoadWallet(fFirstRun);
    if (nLoadWalletRet != DB_LOAD_OK)
    {
        if (nLoadWalletRet == DB_CORRUPT)
            strErrors << _("Error loading wallet.dat: Wallet corrupted") << "\n";
        else if (nLoadWalletRet == DB_NONCRITICAL_ERROR)
        {
            string msg(_("Warning: error reading wallet.dat! All keys read correctly, but transaction data"
                         " or address book entries might be missing or incorrect."));
            uiInterface.ThreadSafeMessageBox(msg, _("Triangles"), CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION | CClientUIInterface::MODAL);
        }
        else if (nLoadWalletRet == DB_TOO_NEW)
            strErrors << _("Error loading wallet.dat: Wallet requires newer version of Triangles") << "\n";
        else if (nLoadWalletRet == DB_NEED_REWRITE)
        {
            strErrors << _("Wallet needed to be rewritten: restart Triangles to complete") << "\n";
            printf("%s", strErrors.str().c_str());
            return InitError(strErrors.str());
        }
        else
            strErrors << _("Error loading wallet.dat") << "\n";
    }

    if (GetBoolArg("-upgradewallet", fFirstRun))
    {
        int nMaxVersion = GetArg("-upgradewallet", 0);
        if (nMaxVersion == 0) // the -upgradewallet without argument case
        {
            printf("Performing wallet upgrade to %i\n", static_cast<int>(WalletFeature::Latest));
            nMaxVersion = CLIENT_VERSION;
            pwalletMain->SetMinVersion(WalletFeature::Latest); // permanently upgrade the wallet immediately
        }
        else
            printf("Allowing wallet upgrade up to %i\n", nMaxVersion);
        if (nMaxVersion < pwalletMain->GetVersion())
            strErrors << _("Cannot downgrade wallet") << "\n";
        pwalletMain->SetMaxVersion(nMaxVersion);
    }

    if (fFirstRun)
    {
        // Create new keyUser and set as default key
        RandAddSeedPerfmon();

        CPubKey newDefaultKey;
        if (!pwalletMain->GetKeyFromPool(newDefaultKey, false))
            strErrors << _("Cannot initialize keypool") << "\n";
        pwalletMain->SetDefaultKey(newDefaultKey);
        if (!pwalletMain->SetAddressBookName(pwalletMain->vchDefaultKey.GetID(), ""))
            strErrors << _("Cannot write default address") << "\n";
    }

    printf("%s", strErrors.str().c_str());
    printf(" wallet      %15" PRId64 "ms\n", GetTimeMillis() - nStart);
    StartupPerfLog("wallet_load", GetTimeMillis() - nStart, strprintf("firstrun=%d", fFirstRun));

    RegisterWallet(pwalletMain.get());

    CBlockIndex *pindexRescan = pindexBest;
    if (GetBoolArg("-rescan"))
        pindexRescan = pindexGenesisBlock;
    else
    {
        int64_t nWalletLocatorStart = GetTimeMillis();
        CWalletDB walletdb(strWalletFileName);
        CBlockLocator locator;
        if (walletdb.ReadBestBlock(locator))
            pindexRescan = locator.GetBlockIndex();
        StartupPerfLog("wallet_bestblock_locator", GetTimeMillis() - nWalletLocatorStart);
    }
    if (pindexBest != pindexRescan && pindexBest && pindexRescan && pindexBest->nHeight > pindexRescan->nHeight)
    {
        uiInterface.InitMessage(_("Rescanning..."));
        printf("Rescanning last %i blocks (from block %i)...\n", pindexBest->nHeight - pindexRescan->nHeight, pindexRescan->nHeight);
        nStart = GetTimeMillis();
        bool fScannedWithIndex = false;
        if (fAddressIndex && !GetBoolArg("-rescan"))
        {
            auto txdb_holder = MakeChainDB("r"); CTxDBBase& txdb = *txdb_holder;
            int nAddressIndexStartHeight = 0;
            uint256 hashAddressIndexBestChain = 0;
            if (txdb.ReadAddressIndexStartHeight(nAddressIndexStartHeight) &&
                txdb.ReadAddressIndexBestChain(hashAddressIndexBestChain) &&
                hashAddressIndexBestChain == hashBestChain &&
                pindexRescan->nHeight >= nAddressIndexStartHeight)
            {
                int nFound = 0;
                fScannedWithIndex = pwalletMain->ScanForWalletTransactionsFromIndex(pindexRescan, true, &nFound);
                if (!fScannedWithIndex)
                    printf("Indexed wallet rescan failed, falling back to full rescan.\n");
            }
            else
            {
                printf("Address index wallet rescan unavailable from block %i.\n", pindexRescan->nHeight);
            }
        }

        if (!fScannedWithIndex)
            pwalletMain->ScanForWalletTransactions(pindexRescan, true);

        printf(" rescan      %15" PRId64 "ms\n", GetTimeMillis() - nStart);
        StartupPerfLog("wallet_rescan", GetTimeMillis() - nStart,
            strprintf("from=%d to=%d indexed=%d", pindexRescan->nHeight, pindexBest->nHeight, fScannedWithIndex));
    }
    else
    {
        StartupPerfLog("wallet_rescan", 0, "skipped");
    }

    // ********************************************************* Step 8.5: start Tor and initialize V3 identity
    {
        uiInterface.InitMessage(_("Starting Tor..."));
        printf("Starting Tor process...\n");

        // Restore hidden service secret key from wallet backup if the key
        // file is missing on disk.  This preserves the .onion identity even
        // if the tor_data directory was deleted.
        if (pwalletMain && !GetBoolArg("-notor", false)) {
            std::string restoreDataPath = GetArg("-tordatadir", (GetDataDir() / "tor_data").string());
            fs::path secretKeyPath = fs::path(restoreDataPath) / "hidden_service" / "hs_ed25519_secret_key";

            if (!fs::exists(secretKeyPath)) {
                CWalletDB walletdb(pwalletMain->strWalletFile);
                std::vector<unsigned char> backedUpKey;

                if (walletdb.ReadSetting("tor_v3_hs_secret_key_backup", backedUpKey) &&
                    backedUpKey.size() == 96) {
                    fs::create_directories(secretKeyPath.parent_path());

                    std::ofstream keyFile(secretKeyPath.string().c_str(), std::ios::binary);
                    if (keyFile.is_open()) {
                        keyFile.write(reinterpret_cast<const char*>(backedUpKey.data()),
                                      backedUpKey.size());
                        keyFile.close();
                        printf("Restored Tor hidden service secret key from wallet backup\n");
                    } else {
                        printf("WARNING: Failed to write restored hs_ed25519_secret_key to %s\n",
                               secretKeyPath.string().c_str());
                    }
                }

                OPENSSL_cleanse(backedUpKey.data(), backedUpKey.size());
            }
        }

        int64_t nTorStart = GetTimeMillis();
        bool torStarted = StartEmbeddedTor();
        StartupPerfLog("tor_start", GetTimeMillis() - nTorStart, strprintf("started=%d", torStarted));
        std::string torDataPath = CTorEmbedded::GetInstance()->GetDataDir();
        if (torDataPath.empty())
            torDataPath = (GetDataDir() / "tor_data").string();

        if (torStarted) {
            printf("Tor process running, SOCKS proxy at %s\n",
                   CTorEmbedded::GetInstance()->GetSocksProxy().c_str());

            // TOR-NATIVE MODE: Force all traffic through embedded Tor
            int socksPort = CTorEmbedded::GetInstance()->GetSocksPort();
            CService torProxyAddr("127.0.0.1", socksPort);

            SetProxy(NET_IPV4, torProxyAddr, 5);
            SetProxy(NET_IPV6, torProxyAddr, 5);
            SetProxy(NET_TOR, torProxyAddr, 5);
            SetNameProxy(torProxyAddr, 5);

            // Disable clearnet reachability - ONION ONLY
            SetReachable(NET_IPV4, false);
            SetReachable(NET_IPV6, false);
            SetReachable(NET_TOR, true);

            printf("TOR-NATIVE MODE: All network traffic forced through Tor\n");
            printf("  Clearnet disabled - .onion addresses only\n");

            #ifdef USE_UPNP
            fUseUPnP = false;
            #endif
        } else if (GetBoolArg("-notor", false)) {
            // -notor: explicit clearnet mode.  Triangles is Tor-native and
            // running without Tor is unsafe for normal operation — it can
            // produce silent clearnet forks (see 2026-06-23 DNS2 incident,
            // 5+ days on a parallel chain because -notor=1 was left on after
            // troubleshooting).  The flag is preserved for explicit recovery
            // workflows (e.g. dumputxoset-from-clearnet when bootstrapping
            // a new node) but requires an additional -recovery-mode=1
            // confirmation flag so it cannot be flipped by accident.
            if (!GetBoolArg("-recovery-mode", false)) {
                return InitError(_(
                    "-notor requires -recovery-mode=1 confirmation.  Triangles is Tor-native; "
                    "running without Tor is unsafe and produces silent clearnet forks.  "
                    "If you need clearnet mode for bootstrap recovery or diagnostics, "
                    "pass BOTH -notor=1 -recovery-mode=1 on the command line."));
            }
            printf("WARNING: Tor disabled via -notor AND -recovery-mode=1 set.  "
                   "Running in clearnet-only mode.\n");
            printf("  .onion connections will NOT be available.\n");
            printf("  This mode is for RECOVERY ONLY — exit and restart without these\n"
                   "  flags as soon as the recovery operation completes.\n");
            SetReachable(NET_IPV4, true);
            SetReachable(NET_IPV6, true);
            SetReachable(NET_TOR, false);
        } else {
            std::string torError = CTorEmbedded::GetInstance()->GetStartupError();
            if (torError.empty())
                torError = "No detailed Tor startup error was recorded.";
            return InitError(strprintf(_("Tor failed to start. Triangles requires Tor to operate.\n\nDetails: %s"), torError.c_str()));
        }

        // ════════════════════════════════════════════════════════════════
        // Embedded I2P (i2pd) startup
        //
        // I2P runs as a co-equal anonymity network alongside Tor. When Tor
        // starts successfully (tor-native mode), I2P provides an alternative
        // anonymous transport via .b32.i2p destinations. When Tor is disabled
        // (-notor recovery mode), I2P is still started to maintain anonymity.
        //
        // I2P's SOCKS proxy (default 19100) handles outbound .i2p connections.
        // A server tunnel forwards incoming I2P connections to the P2P port.
        // ════════════════════════════════════════════════════════════════
        if (torStarted || GetBoolArg("-notor", false)) {
            uiInterface.InitMessage(_("Starting embedded I2P router..."));
            int64_t nI2PStart = GetTimeMillis();
            bool i2pStarted = StartEmbeddedI2P();
            StartupPerfLog("i2p_start", GetTimeMillis() - nI2PStart,
                           strprintf("started=%d", i2pStarted));

            if (i2pStarted) {
                int i2pSocksPort = CI2PEmbedded::GetInstance()->GetSocksPort();
                CService i2pProxyAddr("127.0.0.1", i2pSocksPort);

                // Route I2P traffic through i2pd's SOCKS proxy
                SetProxy(NET_I2P, i2pProxyAddr, 5);
                SetReachable(NET_I2P, true);

                printf("I2P-NATIVE MODE: I2P router running\n");
                printf("  SOCKS proxy at 127.0.0.1:%d for .b32.i2p connections\n",
                       i2pSocksPort);
                printf("  Dual-network anonymity: Tor (.onion) + I2P (.b32.i2p)\n");
            } else {
                // I2P failure is non-fatal — Tor-only operation continues.
                // The daemon still works with .onion peers.
                std::string i2pError = CI2PEmbedded::GetInstance()->GetStartupError();
                printf("WARNING: Embedded I2P did not start. Running Tor-only.\n");
                if (!i2pError.empty())
                    printf("  I2P error: %s\n", i2pError.c_str());
                SetReachable(NET_I2P, false);
            }
        }

        // Initialize Tor V3 identity (Ed25519 keys, onion address)
        uiInterface.InitMessage(_("Initializing Tor V3 identity..."));
        printf("Initializing Tor V3 onion identity...\n");

        int64_t nTorIdentityStart = GetTimeMillis();
        LoadTorV3Config();
        TorV3Config& torConfig = GetTorV3Config();
        torConfig.enableTor = torStarted;
        torConfig.enableHiddenService = torStarted && CTorEmbedded::GetInstance()->IsHiddenServiceEnabled();
        torConfig.hiddenServicePort = CTorEmbedded::GetInstance()->GetHiddenServicePort();
        torConfig.torDataDirectory = torDataPath;
        std::string onionAddr;

        if (torConfig.enableTor && torConfig.enableHiddenService && InitTorV3()) {
            onionAddr = CTorV3Manager::GetInstance()->GetWalletOnionAddress();
            if (!onionAddr.empty()) {
                // Write onion/hostname for compatibility with existing code paths
                fs::path onionDir = GetDataDir() / "onion";
                fs::create_directories(onionDir);
                ofstream hostnameFile((onionDir / "hostname").string().c_str());
                if (hostnameFile.is_open()) {
                    hostnameFile << onionAddr << endl;
                    hostnameFile.close();
                }

                // Register onion address as local address for peer discovery
                AddLocal(CService(onionAddr, torConfig.hiddenServicePort, fNameLookup), LOCAL_MANUAL);
                printf("Tor V3 identity: %s\n", onionAddr.c_str());
            } else {
                printf("WARNING: Tor V3 initialized but no onion address available\n");
            }
        } else if (torStarted && !torConfig.enableHiddenService) {
            printf("Tor hidden service disabled by configuration\n");
        } else if (!torStarted) {
            printf("Skipping Tor V3 identity because the Tor backend is unavailable\n");
        } else {
            printf("WARNING: Failed to initialize Tor V3 identity\n");
        }
        StartupPerfLog("tor_v3_identity", GetTimeMillis() - nTorIdentityStart);

        // Also check if Tor gave us a hidden service hostname
        if (torStarted) {
            fs::path torHsHostname = fs::path(torDataPath) / "hidden_service" / "hostname";
            if (fs::exists(torHsHostname)) {
                ifstream f(torHsHostname.string().c_str());
                string torOnion;
                if (f.is_open() && getline(f, torOnion)) {
                    // Trim whitespace
                    while (!torOnion.empty() && (torOnion.back() == '\n' || torOnion.back() == '\r' || torOnion.back() == ' '))
                        torOnion.pop_back();
                    if (!torOnion.empty()) {
                        if (torOnion != onionAddr) {
                            AddLocal(CService(torOnion, torConfig.hiddenServicePort, fNameLookup), LOCAL_MANUAL);
                        }
                        printf("Tor hidden service (from Tor process): %s\n", torOnion.c_str());
                    }
                }
            }
        }
        StartupPerfLog("tor_setup_total", GetTimeMillis() - nTorStart);

        // Launch background thread for Tor health monitoring and seeder maintenance
        if (torStarted) {
            if (!NewThread(ThreadTorMaintenance, nullptr))
                printf("Warning: ThreadTorMaintenance could not be started\n");
        }

        // Bring up I2P (SAM) transport alongside Tor so the wallet has both a
        // .onion and a .b32.i2p address. On by default; disable with -i2p=0.
        // A bundled i2pd router is launched automatically (mirroring embedded
        // Tor); if -i2psam points at a non-loopback bridge, or a router is
        // already running, we use that instead.
        if (GetBoolArg("-i2p", true)) {
            int64_t nI2PStart = GetTimeMillis();

            uiInterface.InitMessage(_("Starting the I2P router..."));
            bool i2pStarted = StartEmbeddedI2P();
            StartupPerfLog("i2p_start", GetTimeMillis() - nI2PStart, strprintf("started=%d", i2pStarted));
            if (i2pStarted) {
                SetReachable(NET_I2P, true);
                std::string i2pAddr = CI2PEmbedded::GetInstance()->GetI2PAddress();
                printf("I2P network enabled. Our address: %s\n", i2pAddr.c_str());
            } else {
                printf("NOTICE: I2P not available this session; continuing with Tor only\n");
            }
        }
    }

    // ********************************************************* Step 9: import blocks

    if (mapArgs.count("-loadblock"))
    {
        uiInterface.InitMessage(_("Importing blockchain data file."));

        for (string strFile : mapMultiArgs["-loadblock"])
        {
            int64_t nLoadBlockStart = GetTimeMillis();
            FILE *file = fopen(strFile.c_str(), "rb");
            if (file)
                LoadExternalBlockFile(file);
            StartupPerfLog("loadblock_import", GetTimeMillis() - nLoadBlockStart, strprintf("file=%s", strFile.c_str()));
        }
        exit(0);
    }

    fs::path pathBootstrap = GetDataDir() / "bootstrap.dat";
    if (fs::exists(pathBootstrap)) {
        uiInterface.InitMessage(_("Importing bootstrap blockchain data file."));

        int64_t nBootstrapImportStart = GetTimeMillis();
        FILE *file = fopen(pathBootstrap.string().c_str(), "rb");
        if (file) {
            fs::path pathBootstrapOld = GetDataDir() / "bootstrap.dat.old";
            LoadExternalBlockFile(file);
            RenameOver(pathBootstrap, pathBootstrapOld);
        }
        StartupPerfLog("bootstrap_dat_import", GetTimeMillis() - nBootstrapImportStart, strprintf("file=%s", pathBootstrap.string().c_str()));
    }

    // ********************************************************* Step 10: load peers

    uiInterface.InitMessage(_("Loading addresses..."));
    printf("Loading addresses...\n");
    nStart = GetTimeMillis();

    {
        CAddrDB adb;
        if (!adb.Read(addrman))
            printf("Invalid or missing peers.dat; recreating\n");
    }

    printf("Loaded %i addresses from peers.dat  %" PRId64 "ms\n",
           addrman.size(), GetTimeMillis() - nStart);
    StartupPerfLog("peers_load", GetTimeMillis() - nStart, strprintf("count=%d", addrman.size()));

    // Add hardcoded I2P (.b32.i2p) seed addresses to the address manager.
    // This enables cross-network peer discovery: Tor-connected nodes can learn
    // about I2P peers and vice versa. Onion seeds are loaded separately in
    // ThreadOnionSeed (net.cpp), but we add I2P seeds here during init so they
    // are available immediately for the outbound connector.
    {
        static const char *(*strI2PSeed)[1] = fTestNet ? strTestNetI2PSeed : strMainNetI2PSeed;
        int nI2PSeeds = 0;
        for (unsigned int si = 0; strI2PSeed[si][0] != nullptr; si++) {
            CNetAddr parsed;
            if (parsed.SetSpecial(strI2PSeed[si][0])) {
                int nOneDay = 24 * 3600;
                CAddress addr = CAddress(CService(parsed, GetDefaultPort()));
                addr.nTime = GetTime() - 3 * nOneDay - GetRand(4 * nOneDay);
                addrman.Add(addr, parsed);
                nI2PSeeds++;
            }
        }
        if (nI2PSeeds > 0)
            printf("Added %d hardcoded I2P (.b32.i2p) seed addresses to addrman\n", nI2PSeeds);
    }
    
    
    // ********************************************************* Step 11: start node
    nStart = GetTimeMillis();

    if (!CheckDiskSpace())
        return false;

    RandAddSeedPerfmon();

    //// debug print
    printf("mapBlockIndex.size() = %" PRIszu "\n",   mapBlockIndex.size());
    printf("nBestHeight = %d\n",            nBestHeight);
    printf("setKeyPool.size() = %" PRIszu "\n",      pwalletMain->setKeyPool.size());
    printf("mapWallet.size() = %" PRIszu "\n",       pwalletMain->mapWallet.size());
    printf("mapAddressBook.size() = %" PRIszu "\n",  pwalletMain->mapAddressBook.size());

    if (!NewThread(StartNode, nullptr))
        InitError(_("Error: could not start node"));

    if (fServer)
        NewThread(ThreadRPCServer, nullptr);

    // ********************************************************* Step 11.6: P2P UTXO snapshot fetch
    // If the chain is empty and snapshot mode is enabled (default), spawn a
    // background thread that waits for snapshot-capable peers, downloads the
    // canonical snapshot via P2P, and saves it to utxo-snapshot.bin. On
    // success, requests a clean shutdown so the user can restart and have
    // Step 6c load the snapshot in a fresh boot.
    {
        bool snapshotMode = GetBoolArg("-snapshot", true);
        bool needsSnapshot = (nBestHeight <= 0);
        bool haveSnapshotFile = fs::exists(GetDataDir() / "utxo-snapshot.bin");

        if (snapshotMode && needsSnapshot && !haveSnapshotFile &&
            Checkpoints::GetBestSnapshotHeight() > 0)
        {
            NewThread(ThreadSnapshotFetch, nullptr);
        }
    }

    {
        LOCK(cs_DeferredStartup);
        fDeferredStartupRunning = true;
    }
    if (!NewThread(ThreadDeferredStartup, nullptr))
    {
        printf("Warning: deferred startup thread could not be started, running inline\n");
        ThreadDeferredStartup(nullptr);
    }
    StartupPerfLog("start_services", GetTimeMillis() - nStart);

    // ********************************************************* Step 11.5: ZMQ notifications
#ifdef ENABLE_ZMQ
    {
        std::string zmqAddr = GetArg(std::string_view{"-zmqpubhashblock"}, std::string_view{""});
        if (zmqAddr.empty())
            zmqAddr = GetArg(std::string_view{"-zmqpubhashtx"}, std::string_view{""});
        if (zmqAddr.empty())
            zmqAddr = GetArg(std::string_view{"-zmqpub"}, std::string_view{""});
        if (!zmqAddr.empty())
        {
            pzmqNotifier = new CZMQPublishNotifier();
            if (!pzmqNotifier->Initialize(zmqAddr))
            {
                printf("ZMQ: Failed to initialize publisher on %s\n", zmqAddr.c_str());
                delete pzmqNotifier;
                pzmqNotifier = nullptr;
            }
        }
    }
#endif

    // ********************************************************* Step 11.7: SSE notification queue
    if (GetBoolArg("-ssenotify", false))
    {
        pNotificationQueue = new CNotificationQueue();
        printf("SSE: Notification queue initialized (connect to /events on RPC port)\n");
    }

    // ********************************************************* Step 12: finished

    uiInterface.InitMessage(_("Done loading"));
    printf("Done loading\n");
    StartupPerfLog("appinit_total", GetTimeMillis() - nAppInitStart);

    if (!strErrors.str().empty())
        return InitError(strErrors.str());

#if !defined(QT_GUI)
    // Loop until process is exit()ed from shutdown() function,
    // called from ThreadRPCServer thread when a "stop" command is received.
    while (1)
        MilliSleep(5000);
#endif

    return true;
}
