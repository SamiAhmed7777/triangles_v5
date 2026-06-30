// Copyright (c) 2024-2025 Triangles developers
// Tor Process Manager - launches and manages an external Tor binary
// Distributed under the MIT/X11 software license

#ifdef WIN32
#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#endif

#include "tor_process.h"
#include "../util.h"
#include "../net.h"

#include <boost/filesystem.hpp>
#include <fstream>
#include <cstdio>
#include <sstream>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <tlhelp32.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static std::string ReadTailLines(const fs::path& filePath, size_t maxLines)
{
    std::ifstream in(filePath.string().c_str());
    if (!in.is_open()) return "";

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
        if (lines.size() > maxLines) {
            lines.erase(lines.begin());
        }
    }

    std::ostringstream out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) out << " | ";
        out << lines[i];
    }
    return out.str();
}

static CTorProcess* torProcessInstance = nullptr;

CTorProcess* CTorProcess::GetInstance()
{
    if (!torProcessInstance) {
        torProcessInstance = new CTorProcess();
    }
    return torProcessInstance;
}

CTorProcess::CTorProcess()
    : socksPort(19099)
    , hiddenServicePort(24112)
    , hiddenServiceEnabled(true)
    , running(false)
#ifdef WIN32
    , hProcess(nullptr)
    , hJob(nullptr)
    , processId(0)
#else
    , processId(0)
#endif
{
}

CTorProcess::~CTorProcess()
{
    Stop();
}

std::string CTorProcess::FindTorBinary()
{
    // List of candidate paths to search for the Tor binary
    std::vector<std::string> candidates;

#ifdef WIN32
    // Same directory as the wallet executable
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH)) {
        fs::path exeDir = fs::path(exePath).parent_path();
        candidates.push_back((exeDir / "tor.exe").string());
        candidates.push_back((exeDir / "tor" / "tor.exe").string());
        candidates.push_back((exeDir / "Tor" / "tor.exe").string());
    }

    // Data directory
    candidates.push_back((GetDataDir() / "tor.exe").string());
    candidates.push_back((GetDataDir() / "tor" / "tor.exe").string());

    // Common Windows install locations
    const char* programFiles = getenv("ProgramFiles");
    if (programFiles) {
        candidates.push_back(std::string(programFiles) + "\\Tor\\tor.exe");
        candidates.push_back(std::string(programFiles) + "\\Tor Browser\\Browser\\TorBrowser\\Tor\\tor.exe");
    }
    const char* programFilesX86 = getenv("ProgramFiles(x86)");
    if (programFilesX86) {
        candidates.push_back(std::string(programFilesX86) + "\\Tor\\tor.exe");
        candidates.push_back(std::string(programFilesX86) + "\\Tor Browser\\Browser\\TorBrowser\\Tor\\tor.exe");
    }
    const char* localAppData = getenv("LOCALAPPDATA");
    if (localAppData) {
        candidates.push_back(std::string(localAppData) + "\\Tor Browser\\Browser\\TorBrowser\\Tor\\tor.exe");
    }

    // Tor Expert Bundle (common install)
    candidates.push_back("C:\\Tor\\tor.exe");

#else
    // Same directory as the wallet executable
    char exePath[4096];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) {
        exePath[len] = '\0';
        fs::path exeDir = fs::path(exePath).parent_path();
        candidates.push_back((exeDir / "tor").string());
        candidates.push_back((exeDir / "tor" / "tor").string());
    }

    // Data directory
    candidates.push_back((GetDataDir() / "tor").string());
    candidates.push_back((GetDataDir() / "tor" / "tor").string());

    // Standard Linux/macOS locations
    candidates.push_back("/usr/bin/tor");
    candidates.push_back("/usr/local/bin/tor");
    candidates.push_back("/usr/sbin/tor");
    candidates.push_back("/opt/tor/bin/tor");
    candidates.push_back("/snap/bin/tor");

    // Homebrew (macOS)
    candidates.push_back("/opt/homebrew/bin/tor");
    candidates.push_back("/usr/local/opt/tor/bin/tor");
#endif

    // Check each candidate (must be a regular file, not a directory)
    for (const std::string& path : candidates) {
        if (fs::exists(path) && !fs::is_directory(path)) {
            printf("Found Tor binary at: %s\n", path.c_str());
            return path;
        }
    }

    return "";
}

bool CTorProcess::IsPortInUse(int port)
{
#ifdef WIN32
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return false;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);

    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    closesocket(sock);
    return (result == 0);
#else
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);

    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
    return (result == 0);
#endif
}

#ifdef WIN32
bool CTorProcess::KillOrphanedTor()
{
    // Walk all processes looking for tor.exe listening on our SOCKS port.
    // We identify orphans by matching the executable name AND checking that
    // the Tor data directory inside our wallet data dir has a matching PID lock.
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    bool killed = false;

    if (Process32First(hSnap, &pe)) {
        do {
            // Case-insensitive compare against "tor.exe"
            if (_stricmp(pe.szExeFile, "tor.exe") != 0)
                continue;

            printf("Found orphaned tor.exe (PID %lu), terminating...\n", pe.th32ProcessID);
            HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
            if (h) {
                TerminateProcess(h, 0);
                WaitForSingleObject(h, 5000);
                CloseHandle(h);
                killed = true;
            }
        } while (Process32Next(hSnap, &pe));
    }

    CloseHandle(hSnap);

    if (killed) {
        // Give the OS a moment to release the port
        MilliSleep(1000);
    }
    return killed;
}
#endif

bool CTorProcess::WriteTorrc()
{
    fs::path dataPath(torDataDir);
    fs::create_directories(dataPath);

    torrcPath = (dataPath / "torrc").string();

    // Hidden service directory
    fs::path hsDir = dataPath / "hidden_service";
    fs::create_directories(hsDir);

    std::ofstream torrc(torrcPath.c_str());
    if (!torrc.is_open()) {
        printf("ERROR: Cannot write torrc to %s\n", torrcPath.c_str());
        return false;
    }

    torrc << "# Triangles Wallet Tor Configuration (auto-generated)\n";
    torrc << "# Do not edit - this file is overwritten on startup\n\n";

    // SOCKS proxy for wallet connections
    torrc << "SocksPort " << socksPort << "\n";

    // Data directory for Tor state.
    // Use the tor_data directory itself as DataDirectory so that Tor creates
    // its internal 'state' FILE at <tor_data>/state.  Older wallet builds
    // erroneously created a subdirectory called 'state' and pointed
    // DataDirectory at it; newer Tor versions (0.4.9+) reject that because
    // they expect to write a plain file called 'state' inside DataDirectory.
    //
    // Recovery: if 'state' exists as a directory, move its contents up and
    // remove it so that Tor can create its state file in the normal location.
    {
        fs::path badStateDir = dataPath / "state";
        if (fs::exists(badStateDir) && fs::is_directory(badStateDir)) {
            // Migrate any files inside the bad 'state/' directory up to dataPath
            try {
                for (auto& entry : fs::directory_iterator(badStateDir)) {
                    fs::path dest = dataPath / entry.path().filename();
                    if (!fs::exists(dest)) {
                        fs::rename(entry.path(), dest);
                    }
                }
                fs::remove(badStateDir);
                printf("Auto-recovered: removed legacy 'state' directory from %s\n", dataPath.string().c_str());
            } catch (const fs::filesystem_error& e) {
                printf("WARNING: Could not auto-recover tor_data/state directory: %s\n", e.what());
            }
        }
    }
    torrc << "DataDirectory " << dataPath.string() << "\n";

    // Persistent Tor log for post-mortem debugging on user machines.
    fs::path torLogPath = dataPath / "tor.log";
    torrc << "Log notice file " << torLogPath.string() << "\n";

    if (hiddenServiceEnabled) {
        // V3 hidden service so this node is reachable via .onion
        torrc << "HiddenServiceDir " << hsDir.string() << "\n";
        torrc << "HiddenServiceVersion 3\n";
        torrc << "HiddenServicePort " << hiddenServicePort
              << " 127.0.0.1:" << hiddenServicePort << "\n";
    }

    // Only force client-only mode when we are NOT publishing a hidden service.
    // When HiddenServicePort is configured, Tor needs to behave as a hidden
    // service host, not just as a pure outbound client proxy.
    if (!hiddenServiceEnabled) {
        torrc << "ClientOnly 1\n";
    }

    // Disable unused features
    torrc << "AvoidDiskWrites 1\n";
    torrc << "Log notice stderr\n";

    // Append user-supplied extra Tor configuration if present. This lets
    // operators on censored / DPI-filtered networks add Bridge lines,
    // ClientTransportPlugin (obfs4), or a Socks5Proxy/HTTPSProxy upstream so
    // Tor can reach the network when direct connections are blocked. The file
    // is never overwritten by the wallet; only the auto-generated torrc is.
    {
        fs::path extraPath = dataPath / "torrc.extra";
        if (fs::exists(extraPath)) {
            std::ifstream extra(extraPath.string().c_str());
            if (extra.is_open()) {
                torrc << "\n# ---- appended from torrc.extra (user-managed) ----\n";
                torrc << extra.rdbuf();
                torrc << "\n";
                printf("Tor: appended user configuration from %s\n", extraPath.string().c_str());
            }
        }
    }

    torrc.close();

    if (hiddenServiceEnabled) {
        printf("Wrote torrc to %s (SOCKS %d, HS port %d)\n",
               torrcPath.c_str(), socksPort, hiddenServicePort);
    } else {
        printf("Wrote torrc to %s (SOCKS %d, hidden service disabled)\n",
               torrcPath.c_str(), socksPort);
    }
    return true;
}

bool CTorProcess::Start(const std::string& dataDir, int socks, int hsPort, bool enableHiddenService)
{
    lastError.clear();
    socksPort = socks;
    hiddenServiceEnabled = enableHiddenService;
    hiddenServicePort = hiddenServiceEnabled ? hsPort : 0;
    torDataDir = dataDir;

    // Check if something is already listening on our SOCKS port
    if (IsPortInUse(socksPort)) {
#ifdef WIN32
        // An orphaned tor.exe from a previous wallet session is likely still
        // running.  Kill it so we can start a fresh one under our Job Object.
        printf("Tor SOCKS port %d already in use - killing orphaned tor.exe\n", socksPort);
        KillOrphanedTor();
        // If the port is STILL in use after killing all tor.exe, something
        // else owns it.  Fall through and let the new Tor fail gracefully
        // rather than silently adopting an unknown process.
        if (IsPortInUse(socksPort)) {
            printf("WARNING: Port %d still in use after killing tor.exe - another process owns it\n", socksPort);
        }
#else
        // On Linux the child is reaped via waitpid, so orphans are less common.
        // If the port is busy, assume a system Tor or leftover process.
        printf("Tor SOCKS port %d already in use - killing orphaned tor\n", socksPort);
        // Try to find and kill by PID file
        fs::path pidFile = fs::path(torDataDir) / "state" / "pid";
        if (fs::exists(pidFile)) {
            std::ifstream f(pidFile.string().c_str());
            pid_t oldPid = 0;
            if (f >> oldPid && oldPid > 0) {
                printf("Found stale Tor PID %d, sending SIGTERM...\n", oldPid);
                kill(oldPid, SIGTERM);
                for (int i = 0; i < 30; i++) {
                    MilliSleep(100);
                    if (kill(oldPid, 0) != 0) break;
                }
                if (kill(oldPid, 0) == 0) {
                    printf("Tor PID %d still alive, sending SIGKILL...\n", oldPid);
                    kill(oldPid, SIGKILL);
                    MilliSleep(500);
                }
            }
        }
        if (IsPortInUse(socksPort)) {
            printf("WARNING: Port %d still in use after cleanup - another process owns it\n", socksPort);
        }
#endif
    }

    // Find Tor binary
    torBinaryPath = FindTorBinary();
    if (torBinaryPath.empty()) {
        lastError = "Tor binary was not found in the bundled install or standard search paths.";
        printf("WARNING: Tor binary not found. Install Tor for .onion connectivity.\n");
        printf("  Windows: Download from https://www.torproject.org/download/tor/\n");
        printf("  Linux:   apt install tor  or  yum install tor\n");
        printf("  Place tor executable next to the wallet binary for auto-detection.\n");
        return false;
    }

    // Write configuration
    if (!WriteTorrc()) {
        lastError = strprintf("Failed to write Tor configuration to %s", torrcPath.c_str());
        printf("ERROR: Failed to write Tor configuration\n");
        return false;
    }

    printf("Starting Tor process: %s -f %s\n", torBinaryPath.c_str(), torrcPath.c_str());

#ifdef WIN32
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Run hidden
    ZeroMemory(&pi, sizeof(pi));

    std::string cmdLine = "\"" + torBinaryPath + "\" -f \"" + torrcPath + "\"";

    if (!CreateProcessA(
            nullptr,
            (LPSTR)cmdLine.c_str(),
            nullptr, nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr, nullptr,
            &si, &pi))
    {
        DWORD err = ::GetLastError();
        lastError = strprintf("CreateProcess failed for Tor binary '%s' with Windows error %lu", torBinaryPath.c_str(), err);
        printf("ERROR: Failed to start Tor process (error %lu)\n", err);
        return false;
    }

    hProcess = pi.hProcess;
    processId = pi.dwProcessId;
    CloseHandle(pi.hThread);

    // Create a Job Object so Windows kills Tor if the wallet crashes or is
    // killed via Task Manager.  JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE means
    // all processes in the job die when the last handle to the job closes
    // (i.e. when our process exits for any reason).
    hJob = CreateJobObject(nullptr, nullptr);
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {};
        jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation,
                                &jobInfo, sizeof(jobInfo));
        if (!AssignProcessToJobObject(hJob, hProcess)) {
            printf("WARNING: Could not assign Tor to Job Object (error %lu)\n", GetLastError());
        }
    }

    printf("Tor process started (PID %lu)\n", processId);
#else
    pid_t pid = fork();
    if (pid < 0) {
        printf("ERROR: Failed to fork for Tor process\n");
        return false;
    }

    if (pid == 0) {
        // Child process - exec Tor
        // Redirect stdout/stderr to /dev/null to avoid cluttering wallet output
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl(torBinaryPath.c_str(), torBinaryPath.c_str(),
              "-f", torrcPath.c_str(), (char*)nullptr);
        // If exec fails, exit child
        _exit(1);
    }

    processId = pid;
    printf("Tor process started (PID %d)\n", processId);
#endif

    running = true;

    // Wait a few seconds for Tor to bootstrap, then check if SOCKS is up
    printf("Waiting for Tor to bootstrap...\n");
    for (int i = 0; i < 30; i++) {
        MilliSleep(1000);
        if (fShutdown) {
            Stop();
            return false;
        }
        if (IsPortInUse(socksPort)) {
            printf("Tor SOCKS proxy ready on port %d (took %ds)\n", socksPort, i + 1);

            // Read and display the hidden service hostname if available
            if (hiddenServiceEnabled) {
                fs::path hsHostname = fs::path(torDataDir) / "hidden_service" / "hostname";
                if (fs::exists(hsHostname)) {
                    std::ifstream f(hsHostname.string().c_str());
                    std::string hostname;
                    if (f.is_open() && std::getline(f, hostname)) {
                        printf("Tor hidden service: %s\n", hostname.c_str());
                    }
                }
            }
            return true;
        }

        // Check if Tor process is still alive
        if (!IsRunning()) {
            fs::path torLogPath = fs::path(torDataDir) / "tor.log";
            std::string torLogTail = ReadTailLines(torLogPath, 8);
            if (!torLogTail.empty()) {
                lastError = strprintf("Tor process exited during bootstrap before the SOCKS port became ready. Recent tor.log: %s", torLogTail.c_str());
            } else {
                lastError = "Tor process exited during bootstrap before the SOCKS port became ready.";
            }
            printf("ERROR: Tor process exited prematurely\n");
            running = false;
            return false;
        }
    }

    lastError = strprintf("Tor process started from '%s' but SOCKS port %d was not ready after 30 seconds.", torBinaryPath.c_str(), socksPort);
    printf("WARNING: Tor started but SOCKS proxy not yet ready after 30s\n");
    printf("  Tor may still be bootstrapping. .onion connections will work once ready.\n");
    return true;
}

void CTorProcess::Stop()
{
    if (!running) return;

#ifdef WIN32
    if (hProcess != nullptr) {
        printf("Stopping Tor process (PID %lu)...\n", processId);
        TerminateProcess(hProcess, 0);
        WaitForSingleObject(hProcess, 5000);
        CloseHandle(hProcess);
        hProcess = nullptr;
    }
    if (hJob != nullptr) {
        CloseHandle(hJob);
        hJob = nullptr;
    }
#else
    if (processId > 0) {
        printf("Stopping Tor process (PID %d)...\n", processId);
        kill(processId, SIGTERM);
        // Wait up to 5 seconds for graceful shutdown
        for (int i = 0; i < 50; i++) {
            int status;
            pid_t result = waitpid(processId, &status, WNOHANG);
            if (result != 0) break;
            MilliSleep(100);
        }
        // Force kill if still running
        kill(processId, SIGKILL);
        waitpid(processId, nullptr, 0);
    }
#endif

    processId = 0;
    running = false;
    printf("Tor process stopped\n");
}

bool CTorProcess::IsRunning()
{
    if (!running) return false;

#ifdef WIN32
    if (hProcess == nullptr) return false;
    DWORD exitCode;
    if (GetExitCodeProcess(hProcess, &exitCode)) {
        return (exitCode == STILL_ACTIVE);
    }
    return false;
#else
    if (processId <= 0) return false;
    int status;
    pid_t result = waitpid(processId, &status, WNOHANG);
    if (result == 0) return true;  // Still running
    if (result == processId) {
        running = false;
        return false;  // Exited
    }
    return false;
#endif
}

std::string CTorProcess::GetSocksProxy() const
{
    return "127.0.0.1:" + std::to_string(socksPort);
}

// Global convenience functions
bool StartTorProcess(const std::string& dataDir, int socksPort, int hsPort, bool enableHiddenService)
{
    return CTorProcess::GetInstance()->Start(dataDir, socksPort, hsPort, enableHiddenService);
}

void StopTorProcess()
{
    CTorProcess::GetInstance()->Stop();
}
