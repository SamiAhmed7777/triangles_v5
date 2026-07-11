//
// script_stress_tests.cpp — Stack / multisig / opcode-count boundary stress.
//
// These tests guard against regressions in EvalScript's resource limits:
//   - script.size() > 10000          → reject (script.h MAX_SCRIPT_SIZE)
//   - vchPushValue.size() > 520      → reject (MAX_SCRIPT_ELEMENT_SIZE)
//   - nOpCount > 201                 → reject (MAX_OPS_PER_SCRIPT)
//
// What's NOT bounded (and what this file watches):
//   - stack size (no MAX_STACK_SIZE, only nOpCount caps stack-pushing ops
//     at ~201 entries)
//   - OP_CHECKMULTISIG's FindAndDelete loop does O(sigs × script_size) work
//     per invocation — a script full of bytes that looks like a sig pattern
//     amplifies the cost linearly in sig count.
//
// See script.cpp:332 (EvalScript), script.cpp:1045 (CHECKMULTISIG),
// script.cpp:485 (FindAndDelete inline in script.h).
//
#include <chrono>
#include <vector>

#include <boost/test/unit_test.hpp>

#include "script.h"
#include "main.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(script_stress_tests)

// Helper: build an empty transaction usable for EvalScript/VerifyScript calls.
static CTransaction MakeDummyTx()
{
    CTransaction tx;
    tx.vin.resize(1);
    tx.vout.resize(1);
    tx.vout[0].nValue = 1;
    return tx;
}

// 1. Deep stack via OP_DUP should hit the nOpCount limit and return false
//    rather than allocating unbounded memory or running forever.
BOOST_AUTO_TEST_CASE(deep_dup_stack_hits_opcount_limit)
{
    // Build script: 250 OP_DUP ops. Each OP_DUP is opcode > OP_16, so each
    // increments nOpCount. With MAX_OPS_PER_SCRIPT = 201, this must reject.
    CScript script;
    for (int i = 0; i < 250; i++)
        script << OP_DUP;

    vector<vector<unsigned char> > stack;
    CTransaction tx = MakeDummyTx();

    auto t0 = chrono::steady_clock::now();
    bool fOk = EvalScript(stack, script, tx, 0, 0);
    auto t1 = chrono::steady_clock::now();

    BOOST_CHECK(!fOk);  // must reject via nOpCount > 201
    BOOST_CHECK_LT(chrono::duration_cast<chrono::milliseconds>(t1 - t0).count(),
                  1000);  // must be fast

    // Stack should be bounded by however many DUPs executed before the cap.
    // With 201-op cap and 0 starting entries, max ~201 entries.
    BOOST_CHECK_LE(stack.size(), 201u);
}

// 2. 20-of-20 multisig: the maximum legal configuration. Must execute to
//    completion (or fail gracefully) without OOM or pathologically slow
//    FindAndDelete. nKeysCount > 20 is rejected; nKeysCount == 20 is fine.
//
//    CScript::operator<<(int) calls push_int64(), which serializes as
//    minimal push data. So `script << 20` becomes the 2-byte sequence
//    {0x01, 0x14} (PUSHDATA1 prefix + value 20).
BOOST_AUTO_TEST_CASE(max_keys_multisig_20_of_20)
{
    CScript script;
    // Push 20 dummy pubkeys (33 bytes each — compressed pubkey size).
    for (int i = 0; i < 20; i++)
    {
        vector<unsigned char> pk(33, 0x02);
        pk[1] = (unsigned char)i;  // make each distinct
        script << pk;
    }
    script << 20;  // num_of_pubkeys = 20
    // Push 20 dummy signatures
    for (int i = 0; i < 20; i++)
    {
        vector<unsigned char> sig(72, 0x30);  // DER sig-ish
        sig[1] = (unsigned char)(i + 1);
        script << sig;
    }
    script << 20;  // num_of_signatures = 20
    script << OP_CHECKMULTISIG;

    vector<vector<unsigned char> > stack;
    CTransaction tx = MakeDummyTx();

    auto t0 = chrono::steady_clock::now();
    EvalScript(stack, script, tx, 0, 0);  // returns false (sigs are garbage)
    auto t1 = chrono::steady_clock::now();

    // Sigs are garbage, so verification fails — script returns false.
    // EvalScript doesn't roll back the stack on failure (the contract is
    // "on false, stack state is undefined"). The point of this test is
    // that the script must terminate quickly and not OOM, not the stack
    // contents. Stack should be bounded by the inputs we pushed (~42).
    BOOST_CHECK_LT(chrono::duration_cast<chrono::milliseconds>(t1 - t0).count(),
                  2000);
    BOOST_CHECK_LE(stack.size(), 100u);  // bounded by inputs, not unbounded growth
}

// 3. nKeysCount > 20 must reject (guard against the canonical limit).
BOOST_AUTO_TEST_CASE(multisig_rejects_21_keys)
{
    CScript script;
    for (int i = 0; i < 21; i++)
    {
        vector<unsigned char> pk(33, 0x02);
        pk[1] = (unsigned char)i;
        script << pk;
    }
    script << 21;  // num_of_pubkeys = 21 — over the limit
    script << OP_CHECKMULTISIG;

    vector<vector<unsigned char> > stack;
    CTransaction tx = MakeDummyTx();

    BOOST_CHECK(!EvalScript(stack, script, tx, 0, 0));
}

// 4. Pushdata > 520 bytes must reject at parse time.
BOOST_AUTO_TEST_CASE(pushdata_over_520_rejected)
{
    CScript script;
    vector<unsigned char> big(521, 0xAA);
    script << big;  // single push > MAX_SCRIPT_ELEMENT_SIZE
    vector<vector<unsigned char> > stack;
    CTransaction tx = MakeDummyTx();
    BOOST_CHECK(!EvalScript(stack, script, tx, 0, 0));
}

// 5. Script size > 10000 must reject at parse time.
BOOST_AUTO_TEST_CASE(script_size_over_10000_rejected)
{
    CScript script;
    // Fill with 11000 bytes of OP_NOP (each is 1 byte) — exceeds MAX_SCRIPT_SIZE.
    for (int i = 0; i < 11000; i++)
        script << OP_NOP;

    BOOST_CHECK_GT(script.size(), 10000u);

    vector<vector<unsigned char> > stack;
    CTransaction tx = MakeDummyTx();
    BOOST_CHECK(!EvalScript(stack, script, tx, 0, 0));
}

// 6. Disabled opcodes (OP_CAT, OP_MUL, OP_LSHIFT etc.) must reject.
//    These are the "upgrades Bitcoin wisely never shipped" — they were
//    disabled in Bitcoin Core 0.3.x because they make quadratic-work
//    attacks trivial. Confirm Triangles still rejects them all.
BOOST_AUTO_TEST_CASE(disabled_opcodes_rejected)
{
    // Sample of the disabled set from script.cpp:363-378.
    const opcodetype disabled[] = {
        OP_CAT, OP_SUBSTR, OP_LEFT, OP_RIGHT,
        OP_INVERT, OP_AND, OP_OR, OP_XOR,
        OP_2MUL, OP_2DIV, OP_MUL, OP_DIV, OP_MOD,
        OP_LSHIFT, OP_RSHIFT
    };

    for (size_t i = 0; i < sizeof(disabled) / sizeof(disabled[0]); i++)
    {
        CScript script;
        script << disabled[i];
        vector<vector<unsigned char> > stack;
        CTransaction tx = MakeDummyTx();
        BOOST_CHECK_MESSAGE(!EvalScript(stack, script, tx, 0, 0),
            "Disabled opcode " << GetOpName(disabled[i]) << " was accepted!");
    }
}

BOOST_AUTO_TEST_SUITE_END()