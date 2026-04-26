// Local Tor embedding hooks used by the wallet.
// These are Triangles-specific helpers and intentionally live outside the
// vendored Tor source tree to make upstream rebases less painful.

#include "tor_embed_hooks.h"
#include "util.h"

#include <mutex>
#include <thread>
#include <memory>
#include <string>

const char* triangles_tor_data_directory()
{
    static const std::string retrieved = (GetDataDir() / "tor").string();
    return retrieved.c_str();
}

const char* triangles_onion_service_directory()
{
    static const std::string retrieved = (GetDataDir() / "onion").string();
    return retrieved.c_str();
}

int triangles_tor_check_interrupted()
{
    return fShutdown ? 1 : 0;
}

static std::mutex g_torInitializing;

static std::unique_ptr<std::unique_lock<std::mutex> > g_torUninitialized(
    new std::unique_lock<std::mutex>(g_torInitializing));

void triangles_tor_set_initialized()
{
    g_torUninitialized.reset();
}

void triangles_tor_wait_initialized()
{
    std::unique_lock<std::mutex> checking(g_torInitializing);
}
