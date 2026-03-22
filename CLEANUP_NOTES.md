# Triangles Codebase Cleanup Notes

## Overview
Systematic code quality improvements for the Triangles cryptocurrency codebase (v5.3.4+).

**Goal:** Improve maintainability without changing behavior or breaking consensus.

## Inventory

### TODOs/FIXMEs Found (38 total)

#### High Priority (Affects Safety/Correctness)
- `rpcmining.cpp:263` - **Thread safety issue** in mapNewBlock (static variable, no mutex)
- `walletmodel.cpp:249` - **Potential collision** in balance calculation
- `smessage.cpp:863, 2219, 2373` - **File size limit** (files must be split if >2GB)

#### Medium Priority (Encapsulation/Security)
- `protocol.h:50, 100, 132` - Public members should be private (3 locations)
- `wallet.h:378` - nOrderPos calculation should move elsewhere
- `wallet.cpp:733, 1732` - Change output handling needs improvement
- `rpcwallet.cpp:1474, 1513, 1569` - SecureString operator= missing (forced .c_str())

#### Low Priority (Nice-to-Have)
- `util.cpp:1322` - Disabled feature needs verification
- `tor/tor_embedded.cpp:209` - Tor 0.4.9+ shutdown API upgrade
- `init.cpp:442` - Remaining sanity checks (see Bitcoin issue #4081)
- `rpcmining.cpp:232` - DRM comment (unclear what it means)
- `smessage.cpp:*` - Various improvements (hash inclusion, thread safety, defaults)
- `qt/*` - UI improvements (decrypt not supported, message filtering, OSX startup)

#### External/Third-Party (Don't Touch)
- `leveldb/*` - LevelDB library TODOs (upstream issues)

## Code Quality Issues

### Using namespace std (37 files)
All in .cpp files - **this is fine for .cpp**, problematic only in headers.
No headers have this issue, so **no action needed**.

### Printf/Cout Usage (56 files)
Most cryptocurrency code uses printf for early init/error handling before logging is available.
**Review needed:** Check if these are legitimate early-init cases or should use LogPrintf.

## Cleanup Plan (Safest → Riskiest)

### Phase 1: Documentation & Comments ✅ SAFE
1. Document all TODOs with context (why deferred, what's needed)
2. Add function-level comments for complex logic
3. Improve inline comments for clarity

### Phase 2: Low-Risk Code Quality 🟨 MEDIUM RISK
4. Fix compiler warnings (-Wall -Wextra)
5. Add const correctness where missing
6. Remove commented-out dead code
7. Standardize code formatting (if inconsistent)

### Phase 3: Functional Improvements 🟥 HIGH RISK (Skip for now)
8. Fix thread safety issue in rpcmining.cpp (requires testing)
9. Improve protocol.h encapsulation (may affect other code)
10. Address >2GB file handling in smessage.cpp

## Decisions

### What NOT to Change
- **Consensus code** - main.cpp (validation), kernel.cpp (PoS), miner.cpp (staking)
- **Serialization** - Any READWRITE, serialize/deserialize code
- **Protocol constants** - Network message types, version numbers
- **Third-party code** - leveldb/, tor/, sph_types.h, xxhash/, lz4/

### What's Safe to Change
- Comments and documentation
- Variable names (in non-consensus code)
- Code organization (splitting large functions)
- Logging statements
- UI code (qt/)
- RPC interface (as long as API contract preserved)

## Initial Cleanup (2026-03-22)

### Actions Taken
1. Created this documentation file
2. Created cleanup/desloppify branch
3. Inventoried all TODOs/FIXMEs

### Next Steps
1. Add documentation comments to TODO items
2. Review printf/cout usage patterns
3. Check for compiler warnings
4. Consider low-risk improvements

## Notes
- This is a Bitcoin-derived codebase, so many patterns follow Bitcoin Core conventions
- Recent v5.3.x work already modernized to C++17 and removed Boost - good foundation
- Code is generally well-structured; main improvements are documentation and minor cleanup
