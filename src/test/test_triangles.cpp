#define BOOST_TEST_MODULE Triangles Test Suite
#include <boost/test/unit_test.hpp>

#include "db.h"
#include "main.h"
#include "wallet.h"
#include "checkpoints.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <unistd.h>

CWallet* pwalletMain;
CClientUIInterface uiInterface;

// Globals normally defined in init.cpp (excluded from test build)
bool fConfChange;
bool fEnforceCanonical;
unsigned int nNodeLifespan;
unsigned int nDerivationMethodIndex;
bool fUseFastIndex;
enum Checkpoints::CPMode CheckpointsMode;

extern bool fPrintToConsole;
extern void noui_connect();

struct TestingSetup {
    std::filesystem::path pathTemp;
    TestingSetup() {
        fPrintToDebugger = true; // don't want to write to debug.log file
        noui_connect();
        // Isolate the chain DB in a fresh temp datadir so the unit tests
        // never open (and lock) the PRODUCTION chain DB at the default
        // datadir. Mirrors the standalone fixtures; lets ctest run safely
        // even when a live daemon holds the default datadir.
        pathTemp = std::filesystem::temp_directory_path() /
                   (std::string("triangles_test_") + std::to_string(::getpid()));
        std::error_code ec;
        std::filesystem::remove_all(pathTemp, ec);
        std::filesystem::create_directories(pathTemp, ec);
        mapArgs["-datadir"] = pathTemp.string();
        bitdb.MakeMock();
        LoadBlockIndex(true);
        bool fFirstRun;
        pwalletMain = new CWallet("wallet.dat");
        pwalletMain->LoadWallet(fFirstRun);
        RegisterWallet(pwalletMain);
    }
    ~TestingSetup()
    {
        delete pwalletMain;
        pwalletMain = NULL;
        bitdb.Flush(true);
        std::error_code ec;
        std::filesystem::remove_all(pathTemp, ec);
    }
};

BOOST_GLOBAL_FIXTURE(TestingSetup);

void Shutdown(void* parg)
{
  exit(0);
}

void StartShutdown()
{
  exit(0);
}

