/* Copyright (c) 2009-2010 Satoshi Nakamoto
   Copyright (c) 2009-2012 The Bitcoin developers
   Copyright (c) 2013-2014 The StealthCoin/StealthSend Developers */
/* Copyright (c) 2014-2015, Triangles Developers */
/* See LICENSE for licensing information */

#include "anonymize.h"
#include "util.h"

#include <thread>
#include <mutex>
#include <string>
#include <cstring>
#include <memory>

char const* anonymize_tor_data_directory(
) {
    static std::string const retrieved = (
        GetDataDir(
        ) / "tor"
    ).string(
    );
    return retrieved.c_str(
    );
}

char const* anonymize_service_directory(
) {
    static std::string const retrieved = (
        GetDataDir(
        ) / "onion"
    ).string(
    );
    return retrieved.c_str(
    );
}

int check_interrupted(
) {
    return boost::this_thread::interruption_requested(
    ) ? 1 : 0;
}

static std::mutex initializing;

static std::unique_ptr<std::unique_lock<std::mutex> > uninitialized(
    new std::unique_lock<std::mutex>(
        initializing
    )
);

void set_initialized(
) {
    uninitialized.reset();
}

void wait_initialized(
) {
    std::unique_lock<std::mutex> checking(initializing);
}
