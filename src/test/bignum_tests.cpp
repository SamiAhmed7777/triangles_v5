#include <boost/test/unit_test.hpp>
#include <limits>
#include <string>

#include "bignum.h"
#include "util.h"

BOOST_AUTO_TEST_SUITE(bignum_tests)

#if defined(__GNUC__)
#define NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE
#endif

NOINLINE void mysetint64(CBigNum& num, int64_t n)
{
    num.setint64(n);
}

BOOST_AUTO_TEST_CASE(bignum_setint64)
{
    const int64_t values[] = {
        0,
        1,
        -1,
        5,
        -5,
        std::numeric_limits<int64_t>::min(),
        std::numeric_limits<int64_t>::max(),
    };

    for (int64_t value : values) {
        CBigNum num(value);
        BOOST_CHECK_EQUAL(num.ToString(), std::to_string(value));
        num.setulong(0);
        BOOST_CHECK_EQUAL(num.ToString(), "0");
        mysetint64(num, value);
        BOOST_CHECK_EQUAL(num.ToString(), std::to_string(value));
    }
}

BOOST_AUTO_TEST_CASE(bignum_uint64_roundtrip_boundaries)
{
    const uint64_t values[] = {
        0,
        1,
        0x7f,
        0x80,
        uint64_t{1} << 32,
        uint64_t{1} << 63,
        std::numeric_limits<uint64_t>::max(),
    };

    for (uint64_t value : values) {
        CBigNum num(value);
        BOOST_CHECK_EQUAL(num.getuint64(), value);
    }

    CBigNum negative(-1);
    BOOST_CHECK_EQUAL(negative.getuint64(), uint64_t{1});
}

BOOST_AUTO_TEST_CASE(bignum_rejects_invalid_output_base)
{
    CBigNum value(42);
    BOOST_CHECK_THROW(value.ToString(0), bignum_error);
    BOOST_CHECK_THROW(value.ToString(1), bignum_error);
    BOOST_CHECK_THROW(value.ToString(17), bignum_error);
}

BOOST_AUTO_TEST_SUITE_END()
