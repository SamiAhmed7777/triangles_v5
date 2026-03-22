# TODO/FIXME Documentation

Detailed context for each TODO/FIXME in the codebase.

## Critical (Needs Attention)

### src/rpcmining.cpp:263 - Thread Safety Issue
```cpp
static mapNewBlock_t mapNewBlock;    // FIXME: thread safety
```
**Issue:** Static variable accessed by multiple RPC threads without mutex protection.
**Impact:** Potential race condition in getwork RPC (used for mining).
**Status:** Low priority - PoW mining ended at block 9000, this code path rarely used.
**Fix:** Add std::mutex and lock_guard if getwork usage increases.

### src/qt/walletmodel.cpp:249 - Collision Risk
```cpp
if((total + nFeeRequired) > nBalance) // FIXME: could cause collisions in the future
```
**Issue:** Balance check may have edge case causing transaction collisions.
**Context:** In createTransaction fee calculation loop.
**Status:** Needs investigation - unclear what "collisions" means here.
**Fix:** Review Bitcoin Core's current implementation of this logic.

### src/smessage.cpp - File Size Limits
```cpp
// Lines 863, 2219, 2373: "TODO files must be split if > 2GB"
```
**Issue:** Secure message storage files not split when exceeding 2GB.
**Impact:** May fail on 32-bit systems or with large message volumes.
**Status:** Low priority - unlikely to reach 2GB in practice.
**Fix:** Implement file rotation when approaching 2GB limit.

## Medium Priority (Encapsulation/API)

### src/protocol.h - Make Members Private
```cpp
// Lines 50, 100, 132: "TODO: make private (improves encapsulation)"
```
**Issue:** CAddress, CInv, CMessageHeader have public data members.
**Impact:** Poor encapsulation, harder to maintain invariants.
**Status:** Deferred - would require extensive refactoring.
**Fix:** Add getter/setter methods, make members private, update all call sites.

### src/wallet.h:378 - nOrderPos Calculation
```cpp
nOrderPos = -1; // TODO: calculate elsewhere
```
**Issue:** Transaction ordering position calculated in constructor.
**Impact:** Minor - works but not ideal separation of concerns.
**Status:** Deferred - no functional issue.
**Fix:** Move calculation to WalletDB when transaction is added.

### src/rpcwallet.cpp - SecureString Operator
```cpp
// Lines 1474, 1513, 1569: "TODO: get rid of this .c_str()"
```
**Issue:** SecureString missing operator=(std::string).
**Impact:** Forced to use .c_str() which exposes password temporarily.
**Status:** Deferred - would require SecureString class modification.
**Fix:** Add `SecureString& operator=(const std::string&)` method.

## Low Priority (Nice-to-Have)

### src/util.cpp:1322 - Disabled Feature
```cpp
// TODO: This is currently disabled because it needs to be verified to work
```
**Context:** File descriptor management code.
**Status:** Intentionally disabled pending verification.
**Fix:** Test thoroughly, then enable if needed.

### src/tor/tor_embedded.cpp:209 - Tor Shutdown API
```cpp
// TODO: Tor 0.4.9+ may add tor_api_shutdown(), use it when available
```
**Context:** Embedded Tor cleanup.
**Status:** Waiting for upstream Tor API.
**Fix:** Check Tor 0.4.9+ releases for new API, integrate when stable.

### src/init.cpp:442 - Sanity Checks
```cpp
// TODO: remaining sanity checks, see #4081
```
**Context:** Bitcoin Core issue #4081 - additional startup sanity checks.
**Status:** Deferred - core checks already in place.
**Fix:** Review Bitcoin Core's current sanity check implementation.

### src/rpcmining.cpp:232 - DRM Comment
```cpp
CDataStream(coinbase, SER_NETWORK, PROTOCOL_VERSION) >> pblock->vtx[0]; // FIXME - DRM!
```
**Issue:** Unclear what "DRM" means here - likely "Data Race Maybe"?
**Status:** Needs clarification from original author.
**Fix:** Investigate if there's an actual issue, otherwise remove comment.

## Deferred (External/Low Impact)

### LevelDB TODOs (src/leveldb/*)
**Status:** Upstream LevelDB issues - don't modify embedded library.
**Action:** None - track upstream LevelDB project.

### Qt TODOs (src/qt/*)
**Status:** UI improvements, not critical.
**Action:** Track as nice-to-have enhancements.

### Secure Message TODOs (src/smessage.cpp)
Multiple minor improvements suggested:
- Include hash in certain operations
- Improve thread shutdown
- Set default recv/recvAnon behavior
- Update outbox after PoW completes

**Status:** Non-critical enhancements.
**Action:** Consider for future encrypted messaging upgrades.

## Summary

**Critical:** 3 items (thread safety, balance collision, file limits)
**Medium:** 6 items (encapsulation, SecureString)
**Low:** 5 items (disabled features, upstream APIs)
**Deferred:** ~24 items (external libs, minor enhancements)

**Recommendation:** Focus on documenting critical items in code comments, defer fixes until specific issues arise.
