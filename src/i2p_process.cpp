// Copyright (c) 2024 Triangles developers
// I2P Router Process Manager - launches and manages a bundled i2pd binary
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

#include "i2p_process.h"
#include "util.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

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
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static CI2PProcess* i2pProcessInstance = nullptr;

CI2PProcess* CI2PProcess::GetInstance()
{
    if (!i2pProcessInstance)
        i2pProcessInstance = new CI2PProcess();
    return i2pProcessInstance;
}

CI2PProcess::CI2PProcess()
    : samPort(7656)
    , running(false)
    , fExternal(false)
#ifdef WIN32
    , hProcess(nullptr)
    , hJob(nullptr)
    , processId(0)
#else
    , processId(0)
#endif
{
}

CI2PProcess::~CI2PProcess()
{
    Stop();
}

// Try a quick TCP connect; success means something is already listening
// (e.g. the SAM bridge is up, or an external router is running).
bool CI2PProcess::CanConnect(const std::string& host, int port)
{
#ifdef WIN32
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
#else
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) return false;
#endif
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    bool ok = (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == 0);
#ifdef WIN32
    closesocket(s);
#else
    close(s);
#endif
    return ok;
}

std::string CI2PProcess::FindI2pdBinary()
{
    std::vector<std::string> candidates;

#ifdef WIN32
    const char* exeName = "i2pd.exe";
#else
    const char* exeName = "i2pd";
#endif

    // 1. Next to the wallet executable (this is how tor.exe is shipped).
    try {
        fs::path exeDir;
#ifdef WIN32
        char buf[MAX_PATH];
        if (GetModuleFileNameA(nullptr, buf, MAX_PATH) > 0)
            exeDir = fs::path(buf).parent_path();
#else
        exeDir = fs::current_path();
#endif
        if (!exeDir.empty()) {
            candidates.push_back((exeDir / exeName).string());
            candidates.push_back((exeDir / "i2pd" / exeName).string());
            candidates.push_back((exeDir / "I2P" / exeName).string());
        }
    } catch (...) {}

    // 2. In / next to the data directory.
    candidates.push_back((GetDataDir() / exeName).string());
    candidates.push_back((GetDataDir() / "i2pd" / exeName).string());

    // 3. Common system locations.
#ifdef WIN32
    if (const char* pf = getenv("ProgramFiles"))
        candidates.push_back(std::string(pf) + "\\i2pd\\" + exeName);
    if (const char* pfx = getenv("ProgramFiles(x86)"))
        candidates.push_back(std::string(pfx) + "\\i2pd\\" + exeName);
    candidates.push_back(std::string("C:\\i2pd\\") + exeName);
#else
    candidates.push_back("/usr/bin/i2pd");
    candidates.push_back("/usr/local/bin/i2pd");
    candidates.push_back("/opt/i2pd/bin/i2pd");
    candidates.push_back("/opt/homebrew/bin/i2pd");
    candidates.push_back("/usr/local/opt/i2pd/bin/i2pd");
#endif

    for (const std::string& c : candidates) {
        try {
            if (fs::exists(c) && fs::is_regular_file(c)) {
                printf("I2P: found i2pd binary at %s\n", c.c_str());
                return c;
            }
        } catch (...) {}
    }

    return "";
}

bool CI2PProcess::WriteConfig()
{
    fs::path dir(dataDir);
    try {
        fs::create_directories(dir);
    } catch (const std::exception& e) {
        lastError = std::string("Cannot create i2pd data directory: ") + e.what();
        return false;
    }

    confPath = (dir / "i2pd.conf").string();
    fs::path logPath = dir / "i2pd.log";

    std::ofstream conf(confPath.c_str(), std::ios::trunc);
    if (!conf.is_open()) {
        lastError = "Cannot write i2pd.conf to " + confPath;
        return false;
    }

    conf << "# Triangles Wallet I2P configuration (auto-generated)\n";
    conf << "# Do not edit - this file is overwritten on startup\n\n";
    conf << "daemon = false\n";
    conf << "log = file\n";
    conf << "logfile = " << logPath.string() << "\n";
    conf << "datadir = " << dir.string() << "\n\n";

    // The bridge our SAM client talks to.
    conf << "[sam]\n";
    conf << "enabled = true\n";
    conf << "address = 127.0.0.1\n";
    conf << "port = " << samPort << "\n\n";

    // We only need SAM; keep everything else off to minimise footprint.
    conf << "[httpproxy]\nenabled = false\n\n";
    conf << "[socksproxy]\nenabled = false\n\n";
    conf << "[http]\nenabled = false\n\n";
    conf << "[i2pcontrol]\nenabled = false\n";

    conf.close();
    printf("I2P: wrote i2pd config to %s (SAM port %d)\n", confPath.c_str(), samPort);
    return true;
}

bool CI2PProcess::Start(const std::string& dataDirIn, int samPortIn)
{
    dataDir = dataDirIn;
    samPort = samPortIn;
    fExternal = false;
    lastError.clear();

    // If a SAM bridge is already up, use it instead of launching our own.
    if (CanConnect("127.0.0.1", samPort)) {
        printf("I2P: detected an I2P router already listening on SAM port %d; using it\n", samPort);
        fExternal = true;
        return true;
    }

    binaryPath = FindI2pdBinary();
    if (binaryPath.empty()) {
        lastError = "No i2pd binary found (ship i2pd alongside the wallet, like tor)";
        printf("I2P: %s\n", lastError.c_str());
        return false;
    }

    if (!WriteConfig())
        return false;

    printf("I2P: starting i2pd: %s --conf %s\n", binaryPath.c_str(), confPath.c_str());

#ifdef WIN32
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    std::string cmdLine = "\"" + binaryPath + "\" --conf \"" + confPath + "\"";

    if (!CreateProcessA(nullptr, (LPSTR)cmdLine.c_str(), nullptr, nullptr,
                        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        DWORD err = ::GetLastError();
        lastError = strprintf("CreateProcess failed for i2pd '%s' (Windows error %lu)", binaryPath.c_str(), err);
        printf("I2P: ERROR %s\n", lastError.c_str());
        return false;
    }

    hProcess = pi.hProcess;
    processId = pi.dwProcessId;
    CloseHandle(pi.hThread);

    // Kill i2pd if the wallet dies (matches the embedded Tor behaviour).
    hJob = CreateJobObject(nullptr, nullptr);
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {};
        jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jobInfo, sizeof(jobInfo));
        if (!AssignProcessToJobObject(hJob, hProcess))
            printf("I2P: WARNING could not assign i2pd to Job Object (error %lu)\n", GetLastError());
    }

    printf("I2P: i2pd started (PID %lu)\n", processId);
#else
    pid_t pid = fork();
    if (pid < 0) {
        lastError = "Failed to fork for i2pd process";
        printf("I2P: ERROR %s\n", lastError.c_str());
        return false;
    }
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl(binaryPath.c_str(), binaryPath.c_str(),
              "--conf", confPath.c_str(), (char*)nullptr);
        _exit(1);
    }
    processId = pid;
    printf("I2P: i2pd started (PID %d)\n", processId);
#endif

    running = true;

    // Wait for the SAM bridge to come up. The bridge opens quickly; tunnel
    // build (needed for actual connectivity) continues in the background.
    printf("I2P: waiting for SAM bridge on port %d...\n", samPort);
    for (int i = 0; i < 45; i++) {
        MilliSleep(1000);
        if (fShutdown) {
            Stop();
            return false;
        }
        if (CanConnect("127.0.0.1", samPort)) {
            printf("I2P: SAM bridge ready on port %d (took %ds)\n", samPort, i + 1);
            return true;
        }
        if (!IsRunning()) {
            lastError = "i2pd exited during start-up before the SAM bridge became ready";
            printf("I2P: ERROR %s\n", lastError.c_str());
            running = false;
            return false;
        }
    }

    lastError = strprintf("i2pd started but SAM port %d not ready after 45s", samPort);
    printf("I2P: WARNING %s (it may still be building tunnels)\n", lastError.c_str());
    return true;
}

void CI2PProcess::Stop()
{
    if (fExternal) {
        // We never launched it; leave the user's router running.
        running = false;
        return;
    }
    if (!running) return;

#ifdef WIN32
    if (hProcess != nullptr) {
        printf("I2P: stopping i2pd (PID %lu)...\n", processId);
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
        printf("I2P: stopping i2pd (PID %d)...\n", processId);
        kill(processId, SIGTERM);
        for (int i = 0; i < 50; i++) {
            int status;
            pid_t result = waitpid(processId, &status, WNOHANG);
            if (result != 0) break;
            MilliSleep(100);
        }
        kill(processId, SIGKILL);
        waitpid(processId, nullptr, 0);
    }
#endif

    processId = 0;
    running = false;
    printf("I2P: i2pd stopped\n");
}

bool CI2PProcess::IsRunning()
{
    if (fExternal) return true;
    if (!running) return false;

#ifdef WIN32
    if (hProcess == nullptr) return false;
    DWORD exitCode;
    if (GetExitCodeProcess(hProcess, &exitCode))
        return (exitCode == STILL_ACTIVE);
    return false;
#else
    if (processId <= 0) return false;
    int status;
    pid_t result = waitpid(processId, &status, WNOHANG);
    return (result == 0); // 0 => still running
#endif
}

bool StartEmbeddedI2P(const std::string& dataDir, int samPort)
{
    return CI2PProcess::GetInstance()->Start(dataDir, samPort);
}

void StopEmbeddedI2P()
{
    CI2PProcess::GetInstance()->Stop();
}
