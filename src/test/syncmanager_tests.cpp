// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license.

#include <boost/test/unit_test.hpp>

#include "../syncmanager.h"

BOOST_AUTO_TEST_SUITE(syncmanager_tests)

BOOST_AUTO_TEST_CASE(empty_header_planner_enables_bounded_legacy_fallback)
{
    const int64_t interval = CSyncManager::LEGACY_BLOCK_FALLBACK_INTERVAL_SECONDS;

    BOOST_CHECK(!CSyncManager::ShouldRequestLegacyBlocks(0, 0, interval - 1));
    BOOST_CHECK(CSyncManager::ShouldRequestLegacyBlocks(0, 0, interval));

    // Headers-first remains authoritative whenever it has useful work.
    BOOST_CHECK(!CSyncManager::ShouldRequestLegacyBlocks(1, 0, interval));
    BOOST_CHECK(!CSyncManager::ShouldRequestLegacyBlocks(0, 1, interval));
}

BOOST_AUTO_TEST_SUITE_END()
