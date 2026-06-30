// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Standalone test driver for chaindb equivalence tests.
//
// Runs WITHOUT the TestingSetup global fixture from test_triangles.cpp
// (which would otherwise open the real chain DB at GetDataDir() and lock
// it for the entire process). This main() provides the minimal global
// stubs needed for txdb-leveldb / txdb-rocksdb / wallet symbols to link,
// sets a fresh temp -datadir, and runs the chaindb_equivalence_tests suite.

#define BOOST_TEST_MODULE chaindb_equivalence_tests_standalone
#include <boost/test/unit_test.hpp>

#include "../util.h"
#include "../wallet.h"
#include "../checkpoints.h"

#include <filesystem>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

// ─── Globals normally defined in init.cpp / wallet.cpp ─────────────────────
CWallet* pwalletMain = nullptr;
CClientUIInterface uiInterface;
bool fConfChange = false;
bool fEnforceCanonical = false;
unsigned int nNodeLifespan = 0;
unsigned int nDerivationMethodIndex = 0;
bool fUseFastIndex = false;
enum Checkpoints::CPMode CheckpointsMode = Checkpoints::STRICT;

void StartShutdown() { /* no-op for tests */ }

namespace {

struct DataDirSetup
{
    DataDirSetup()
    {
        fs::path tmp = fs::temp_directory_path() /
            ("triangles_chaindb_test_" + std::to_string(getpid()));
        std::error_code ec;
        fs::remove_all(tmp, ec);
        fs::create_directories(tmp);
        mapArgs["-datadir"] = tmp.string();
        // Default -dbcache is 2048 MB; the test host may have far less
        // memory. Use a small cache (16 MB) to keep the test self-contained.
        mapArgs["-dbcache"] = "16";
    }
};

BOOST_GLOBAL_FIXTURE(DataDirSetup);

} // anonymous namespace

// Test bodies are in this TU so the global fixture runs before any
// CTxDB / CRocksTxDB constructor.
#include "chaindb_equivalence_tests.inc"
