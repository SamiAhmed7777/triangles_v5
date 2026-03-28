// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "txdb.h"
#include "walletdb.h"
#include "trianglesrpc.h"
#include "net.h"
#include "init.h"
#include "util.h"
#include "ui_interface.h"
#include "checkpoints.h"
#include "smessage.h"
#include "openssl_compat.h"
#include "bootstrap.h"
#include "tor/tor_embedded.h"
#include "tor/onion_v3.h"
#include "tor/tor_process.h"
#ifdef ENABLE_ZMQ
#include "zmqpublishnotifier.h"
#endif
#include "notificationqueue.h"
#include "addressindex.h"
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
// boost/filesystem/convenience.hpp removed in modern Boost; functionality is in filesystem.hpp
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <openssl/crypto.h>

#ifndef WIN32
#include <signal.h>
#endif


using namespace std;
using namespace boost;
namespace fs = boost::filesystem;

CWallet* pwalletMain;
CClientUIInterface uiInterface;
std::string strWalletFileName;
bool fConfChange;
bool fEnforceCanonical;
unsigned int nNodeLifespan;
unsigned int nDerivationMethodIndex;
//unsigned int nMinerSleep;
bool fUseFastIndex;
enum Checkpoints::CPMode CheckpointsMode;

static CCriticalSection cs_DeferredStartup;
static bool fDeferredStartupRunning = false;

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

void StartShutdown()
{
fRequestShutdown = true;
#ifdef QT_GUI
    // ensure we leave the Qt main loop for a clean GUI exit (Shutdown() is called in triangles.cpp afterwards)
    uiInterface.QueueShutdown();
#else
    // Without UI, Shutdown() can simply be started in a new thread
    NewThread(Shutdown, NULL);
#endif
}

bool ShutdownRequested()

{
    return fRequestShutdown;
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
        PrintExceptionContinue(NULL, "ThreadDeferredStartup()");
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

        // NOW safe to destroy Tor state - all threads have stopped
        ShutdownTorV3();
        StopEmbeddedTor();

#ifdef ENABLE_ZMQ
        if (pzmqNotifier)
        {
            pzmqNotifier->Shutdown();
            delete pzmqNotifier;
            pzmqNotifier = NULL;
        }
#endif

        if (pNotificationQueue)
        {
            delete pNotificationQueue;
            pNotificationQueue = NULL;
        }

//        CTxDB().Close();
        bitdb.Flush(false);
        bitdb.Flush(true);
        fs::remove(GetPidFile());
        UnregisterWallet(pwalletMain);
        delete pwalletMain;
        // DB is flushed and wallet saved - safe to force-exit if something hangs
        NewThread(ExitTimeout, NULL);
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
            Shutdown(NULL);
        }
        ReadConfigFile(mapArgs, mapMultiArgs);

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
            if (!IsSwitchChar(argv[i][0]) && !boost::algorithm::istarts_with(argv[i], "Triangles:"))
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
        PrintException(NULL, "AppInit()");
    }
    if (!fRet)
        Shutdown(NULL);
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
        //"  -proxy=<ip:port>       " + _("Connect through socks proxy") + "\n" +
        //"  -socks=<n>             " + _("Select the version of socks proxy to use (4-5, default: 5)") + "\n" +
        "  -tor=<ip:port>         " + _("Use proxy to reach tor hidden services (default: same as -proxy)") + "\n"
        "  -notor                 " + _("Disable Tor startup and .onion connectivity") + "\n" +
        "  -torsocks=<port>       " + _("Set embedded or managed Tor SOCKS proxy port (default: 19099)") + "\n" +
        "  -torhiddenservice      " + _("Enable the managed Tor hidden service (default: 1)") + "\n" +
        "  -torhsport=<port>      " + _("Set embedded or managed Tor hidden service port (default: wallet listen port)") + "\n" +
        //"  -dns                   " + _("Allow DNS lookups for -addnode, -seednode and -connect") + "\n" +
        "  -port=<port>           " + _("Listen for connections on <port> (default: 24112 or testnet: 24111)") + "\n" +
        "  -maxconnections=<n>    " + _("Maintain at most <n> connections to peers (default: 125)") + "\n" +
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
        "  -banscore=<n>          " + _("Threshold for disconnecting misbehaving peers (default: 100)") + "\n" +
        "  -bantime=<n>           " + _("Number of seconds to keep misbehaving peers from reconnecting (default: 86400)") + "\n" +
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
        "  -alertnotify=<cmd>     " + _("Execute command when a relevant alert is received (%s in cmd is replaced by message)") + "\n" +
        "  -upgradewallet         " + _("Upgrade wallet to latest format") + "\n" +
        "  -keypool=<n>           " + _("Set key pool size to <n> (default: 100)") + "\n" +
        "  -rescan                " + _("Rescan the block chain for missing wallet transactions") + "\n" +
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
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0));
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
    if (setProcDEPPol != NULL) setProcDEPPol(PROCESS_DEP_ENABLE);
#endif
#ifndef WIN32
    umask(077);

    // Clean shutdown on SIGTERM
    struct sigaction sa;
    sa.sa_handler = HandleSIGTERM;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // Reopen debug.log on SIGHUP
    struct sigaction sa_hup;
    sa_hup.sa_handler = HandleSIGHUP;
    sigemptyset(&sa_hup.sa_mask);
    sa_hup.sa_flags = 0;
    sigaction(SIGHUP, &sa_hup, NULL);
#endif

    // ********************************************************* Step 2: parameter interactions

    nNodeLifespan = GetArg("-addrlifespan", 7);
    fUseFastIndex = GetBoolArg("-fastindex", true);
    //nMinerSleep = GetArg("-minersleep", 500);

    CheckpointsMode = Checkpoints::STRICT;
    std::string strCpMode = GetArg("-cppolicy", "strict");

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

    if (mapArgs.count("-paytxfee"))
    {
        if (!ParseMoney(mapArgs["-paytxfee"], nTransactionFee))
            return InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s'"), mapArgs["-paytxfee"].c_str()));
        if (nTransactionFee > 0.25 * COIN)
            InitWarning(_("Warning: -paytxfee is set very high! This is the transaction fee you will pay if you send a transaction."));
    }

    fConfChange = GetBoolArg("-confchange", false);
    fEnforceCanonical = GetBoolArg("-enforcecanonical", true);

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
    std::string strWalletFileName = GetArg("-wallet", "wallet.dat");

    // strWalletFileName must be a plain filename without a directory
    if (strWalletFileName != fs::path(strWalletFileName).stem().string() + fs::path(strWalletFileName).extension().string())
        return InitError(strprintf(_("Wallet %s resides outside data directory %s."), strWalletFileName.c_str(), strDataDir.c_str()));

	// Make sure only a single Triangles process is using the data directory.
    fs::path pathLockFile = GetDataDir() / ".lock";
    FILE* file = fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file) fclose(file);
    static boost::interprocess::file_lock lock(pathLockFile.string().c_str());
    if (!lock.try_lock())
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

    if (!bitdb.Open(GetDataDir()))
    {
        string msg = strprintf(_("Error initializing database environment %s!"
                                 " To recover, BACKUP THAT DIRECTORY, then remove"
                                 " everything from it except for wallet.dat."), strDataDir.c_str());
        return InitError(msg);
    }

    if (GetBoolArg("-salvagewallet"))
    {
        // Recover readable keypairs:
        if (!CWalletDB::Recover(bitdb, strWalletFileName, true))
            return false;
    }

    if (fs::exists(GetDataDir() / strWalletFileName))
    {
        CDBEnv::VerifyResult r = bitdb.Verify(strWalletFileName, CWalletDB::Recover);
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
    StartupPerfLog("verify_db", GetTimeMillis() - nStart, strprintf("wallet=%s", strWalletFileName.c_str()));

    // ********************************************************* Step 6: network initialization
    nStart = GetTimeMillis();

    //int nSocksVersion = GetArg("-socks", 5);
    //
    //if (nSocksVersion != 4 && nSocksVersion != 5)
    //  return InitError(strprintf(_("Unknown -socks proxy version requested: %i"), nSocksVersion));

    // Network selection: enable all networks (IPv4, IPv6, Tor)
    // Tor is always enabled; clearnet is also allowed for seed node discovery
    // Users can restrict to Tor-only with -onlynet=tor
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
                // Bind to all interfaces so external peers can connect
                CService addrBind;
                if (!Lookup("0.0.0.0", addrBind, GetListenPort(), false))
                    return InitError(strprintf(_("Cannot resolve binding address: '%s'"),  "0.0.0.0"));
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
        if (!Checkpoints::SetCheckpointPrivKey(GetArg("-checkpointkey", "")))
            InitError(_("Unable to sign checkpoint, wrong checkpointkey?\n"));
    }

    for (string strDest : mapMultiArgs["-seednode"])
        AddOneShot(strDest);
    StartupPerfLog("network_init", GetTimeMillis() - nStart, strprintf("listen=%d seednodes=%" PRIszu, !fNoListen, mapMultiArgs["-seednode"].size()));

    // ********************************************************* Step 6b: bootstrap download (daemon)
#ifndef QT_GUI
    if (GetBoolArg("-bootstrap", false))
    {
        int64_t nBootstrapStart = GetTimeMillis();
        fs::path dataPath = GetDataDir();
        std::string host = Bootstrap::DEFAULT_HOST;
        std::string strError;

        uiInterface.InitMessage(_("Downloading blockchain snapshot..."));
        printf("Bootstrap: contacting %s...\n", host.c_str());

        auto progressFn = [](int64_t bytesDownloaded, int64_t totalBytes) {
            if (totalBytes > 0) {
                printf("\rBootstrap: %lld / %lld MB (%lld%%)",
                       (long long)(bytesDownloaded / (1024*1024)),
                       (long long)(totalBytes / (1024*1024)),
                       (long long)((bytesDownloaded * 100) / totalBytes));
                fflush(stdout);
            }
        };

        bool success = Bootstrap::DownloadBootstrap(host, dataPath, progressFn, strError);
        if (!success) {
            host = Bootstrap::FALLBACK_HOST;
            printf("\nBootstrap: primary host failed, trying fallback %s...\n", host.c_str());
            success = Bootstrap::DownloadBootstrap(host, dataPath, progressFn, strError);
        }

        if (!success) {
            printf("\nBootstrap: failed: %s\n", strError.c_str());
            printf("Bootstrap: skipping, will sync from network.\n");
        } else {
            printf("\nBootstrap: done.\n");
        }
        StartupPerfLog("bootstrap_download", GetTimeMillis() - nBootstrapStart,
            strprintf("host=%s success=%d", host.c_str(), success));
    }
#endif

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
        CTxDB txdb("r");
        txdb.LoadBlockIndex();
        PrintBlockTree();
        return false;
    }

    uiInterface.InitMessage(_("Loading block index..."));
    printf("Loading block index...\n");
    nStart = GetTimeMillis();
    if (!LoadBlockIndex())
        return InitError(_("Error loading blkindex.dat"));

    // If the block index is empty but blk0001.dat exists (bootstrap download),
    // fast-import: build the index directly from the block file without re-writing
    // data. Batches LevelDB commits every 200K blocks for speed.
    if (nBestHeight == 0 && boost::filesystem::exists(GetDataDir() / "blk0001.dat")
        && mapBlockIndex.size() <= 1)
    {
        uiInterface.InitMessage(_("Importing bootstrap blocks..."));
        printf("Block index empty but blk0001.dat exists - running fast import...\n");
        int64_t nFastImportStart = GetTimeMillis();
        FastImportBlockFile();
        StartupPerfLog("bootstrap_fast_import", GetTimeMillis() - nFastImportStart, strprintf("bestheight=%d", nBestHeight));
    }

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
                block.ReadFromDisk(pindex);
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
    pwalletMain = new CWallet(strWalletFileName);
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
            printf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
            nMaxVersion = CLIENT_VERSION;
            pwalletMain->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
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

    RegisterWallet(pwalletMain);

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
        if (GetBoolArg("-addressindex", false) && !GetBoolArg("-rescan"))
        {
            CTxDB txdb("r");
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
        } else {
            printf("WARNING: Tor not available. .onion peers will not be reachable.\n");
            printf("  Clearnet connections will still work normally.\n");
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
            if (!NewThread(ThreadTorMaintenance, NULL))
                printf("Warning: ThreadTorMaintenance could not be started\n");
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

    if (!NewThread(StartNode, NULL))
        InitError(_("Error: could not start node"));

    if (fServer)
        NewThread(ThreadRPCServer, NULL);

    {
        LOCK(cs_DeferredStartup);
        fDeferredStartupRunning = true;
    }
    if (!NewThread(ThreadDeferredStartup, NULL))
    {
        printf("Warning: deferred startup thread could not be started, running inline\n");
        ThreadDeferredStartup(NULL);
    }
    StartupPerfLog("start_services", GetTimeMillis() - nStart);

    // ********************************************************* Step 11.5: ZMQ notifications
#ifdef ENABLE_ZMQ
    {
        std::string zmqAddr = GetArg("-zmqpubhashblock", "");
        if (zmqAddr.empty())
            zmqAddr = GetArg("-zmqpubhashtx", "");
        if (zmqAddr.empty())
            zmqAddr = GetArg("-zmqpub", "");
        if (!zmqAddr.empty())
        {
            pzmqNotifier = new CZMQPublishNotifier();
            if (!pzmqNotifier->Initialize(zmqAddr))
            {
                printf("ZMQ: Failed to initialize publisher on %s\n", zmqAddr.c_str());
                delete pzmqNotifier;
                pzmqNotifier = NULL;
            }
        }
    }
#endif

    // ********************************************************* Step 11.6: Address index
    fAddressIndex = GetBoolArg("-addressindex", false);
    if (fAddressIndex)
        printf("Address index enabled\n");

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
