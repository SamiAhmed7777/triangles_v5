# Triangles Cleanup Strategy - Safe Improvements

**Branch:** `cleanup/safe-improvements`  
**Goal:** Improve code quality without touching consensus-critical code

## ✅ SAFE TO FIX

### 1. Compiler Warnings (Non-Consensus)
- **C++11 literal-suffix warnings** - Add spaces between literals and suffixes
- **Unused variables/functions** - Remove dead code (verify not consensus-critical first)
- **Deprecated-copy warnings** - Fix CScript assignment operator if safe

### 2. Code Style Improvements
- Remove `using namespace std` from headers (keep in .cpp files)
- Standardize logging patterns
- Improve code comments (remove unclear/misleading ones)
- Add context to TODOs/FIXMEs

### 3. Documentation
- Add inline comments for thread safety concerns
- Document collision vulnerabilities
- Improve function/class documentation

## ❌ DO NOT TOUCH

### Consensus-Critical Code
- **OpenSSL SHA256/RIPEMD160 usage** - Deprecated warnings OK, do not change
- **BN_is_prime_ex** - Crypto library deprecation, leave as-is
- **Hash algorithms** - Third-party libraries with warnings, consensus-critical
- **Block validation logic** - Any code affecting block/transaction validation
- **Merkle tree construction** - Core consensus
- **Proof-of-Work/Proof-of-Stake** - Staking/mining algorithms

### How to Identify Consensus Code
- Files in `src/` related to: `main.cpp`, `main.h`, block validation, transaction validation
- Anything in hash algorithm libraries
- Cryptographic primitives
- Network protocol message formats (version, serialization)

## Incremental Testing Strategy

1. **One warning category at a time**
2. **Compile after each change**
3. **Test basic functionality:**
   - `trianglesd getinfo`
   - `trianglesd getblockchaininfo`
   - Verify block sync works
4. **Commit incrementally** with clear messages

## Warning Categories (From Build Output)

```
1. C++11 literal-suffix: ~20 instances (util.h, net.h, alert.cpp)
2. OpenSSL deprecation: SHA256, RIPEMD160 (DO NOT FIX)
3. BN_is_prime_ex: crypto library (DO NOT FIX)
4. Deprecated-copy: CScript assignment (REVIEW CAREFULLY)
5. Unused variables/functions: Various (SAFE IF NOT CONSENSUS)
```

## Branch History

- Previous work: `cleanup/desloppify` (documentation improvements, merged to master)
- This branch: Focus on safe compiler warnings and code quality

## Verification Checklist

Before pushing each commit:
- [ ] Code compiles successfully
- [ ] No new warnings introduced
- [ ] trianglesd runs without errors
- [ ] getinfo/getblockchaininfo work
- [ ] No consensus-critical code touched

---

**Principle:** When in doubt, don't touch it. A clean codebase is worthless if the blockchain forks.
