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

namespace fs = boost::filesystem;

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
    , hProcess(NULL)
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
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
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

    // Data directory for Tor state
    fs::path torStateDir = dataPath / "state";
    fs::create_directories(torStateDir);
    torrc << "DataDirectory " << torStateDir.string() << "\n";

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
        printf("Tor SOCKS port %d already in use - assuming Tor is running\n", socksPort);
        lastError = strprintf("SOCKS port %d is already in use; assuming an existing Tor instance is serving it.", socksPort);
        running = true;
        return true;
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
            NULL,
            (LPSTR)cmdLine.c_str(),
            NULL, NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL, NULL,
            &si, &pi))
    {
        DWORD err = GetLastError();
        lastError = strprintf("CreateProcess failed for Tor binary '%s' with Windows error %lu", torBinaryPath.c_str(), err);
        printf("ERROR: Failed to start Tor process (error %lu)\n", err);
        return false;
    }

    hProcess = pi.hProcess;
    processId = pi.dwProcessId;
    CloseHandle(pi.hThread);

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
              "-f", torrcPath.c_str(), (char*)NULL);
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

            // Defensive check: Tor must not directly occupy the node's hidden-service
            // virtual port. If it does, node startup will fail with a bind collision.
            if (hiddenServiceEnabled && IsPortInUse(hiddenServicePort)) {
                lastError = strprintf("Port %d is already busy while Tor hidden service is enabled. Another process is likely blocking the node listener.", hiddenServicePort);
                printf("ERROR: Tor startup collision: hidden service port %d appears busy before node bind.\n",
                       hiddenServicePort);
                printf("       Refusing to treat Tor as healthy because this would block the node listener.\n");
                Stop();
                running = false;
                return false;
            }

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
    if (hProcess != NULL) {
        printf("Stopping Tor process (PID %lu)...\n", processId);
        TerminateProcess(hProcess, 0);
        WaitForSingleObject(hProcess, 5000);
        CloseHandle(hProcess);
        hProcess = NULL;
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
        waitpid(processId, NULL, 0);
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
    if (hProcess == NULL) return false;
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
