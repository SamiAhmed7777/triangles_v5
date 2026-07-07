# Hermes handoff — picking up from Krystie (2026-07-04, 04:10 PDT)

Sami asked me to carry forward Krystie's autonomous test-structure audit.
Currently 04:10 PDT, target end ~12:00 PDT = ~7h50m budget.

## What Krystie did (verified)

- **T003 (FIXED)** — Caddy vhost for `seeds.cryptographic-triangles.org`
- **T001 (FALSE ALARM)** — RPC thread crash verified not reproducing
- **T002 (FALSE ALARM)** — wallet 0 balance is operational, not code
- **REAL BUG #1 (FIXED)** — `src/script.cpp` `CheckSig` cache Set/Get asymmetry:
  - Line 1306 was `Set(sighash, vchSig, vchPubKey)` while line 1296 Get used `vchSigCopy`
  - vchSig includes trailing hashtype byte, vchSigCopy doesn't → cache key mismatch → silent no-op
  - Fixed to `Set(sighash, vchSigCopy, vchPubKey)` (cross-checked with GLM-5.2, confirmed upstream Bitcoin Core pattern)
- **Sub-bug (FIXED)** — `ComputeKey` line 1234 had `(k & 0xffffffff00000000ULL) | (k & 0x00000000ffffffffULL)` which is a NO-OP
  - Fixed to `(k >> 32) | (k << 32)` — proper 32-bit rotation
- **Test fixes in progress** — updated `DoS_tests.cpp`, `http_seed_tests.cpp`, `multisig_tests.cpp`,
  `onion_v3_tests.cpp`, `script_tests.cpp`, `staking_tests.cpp`, `time_drift_tests.cpp`
  to match the new behavior. NOT yet verified by build.

## What I'm doing next

1. Build `test_triangles` binary with the current working tree, capture pass/fail
2. Independently verify the script.cpp fix by reading the actual code, not trusting Krystie's claim
3. Cross-check main.cpp PoS reward change with z.ai — was the proportionality bug real?
4. Verify time_drift 180→90 change against `GetMaxTimeDrift` source
5. Wire `consensus_safety_tests.cpp` into CMakeLists (untracked, 361 lines)
6. Read every line of consensus_safety_tests.cpp and verify against actual code constants
7. Continue audit while build runs in background

## Ping protocol (Hermes ↔ Krystie)

We share `notes/audit-progress.md` (append-only) + this file. When one of us finds
something that contradicts the other's findings, write it under a "## CONFLICT"
heading here. When we agree on a fix, the notes file is the canonical record.
When we disagree and can't reconcile in 2 rounds, write a "## ESCALATE" block
and surface to Sami.

z.ai guard at `http://127.0.0.1:8767/v1` (glm-5.2 model) — same model Krystie used.

## Hard rules

- Never commit `.md` files (Sami's rule). These notes live in `notes/` which is
  already `.gitignore`'d / untracked.
- Never push to `origin/master` — only local + drafts.
- Never tag a release.
- Never touch the production daemon (`/root/.triangles/`).
- Build is read-only verification, but writing to `/root/triangles_v5/` is fine.