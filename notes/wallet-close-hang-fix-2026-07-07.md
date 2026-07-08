# Wallet close hang fix (2026-07-07)

**Reported by:** Sami
**Branch:** TBD (off `ui/overview-color-rework` or new `fix/close-hang`)
**Severity:** High — wallet process can't be closed by user on Windows
**Consensus-affecting:** No (threading/process lifecycle only)

## Symptom

- User clicks X on Qt wallet
- Wallet appears to hang
- Task Manager → End Task does not close the process (on Windows)
- No new `debug.log` output after the click

## Root cause (Phase 1)

`src/tor/tor_embedded.cpp:205-206`:

```cpp
std::thread torThread(TorThreadFunc, argv);
torThread.detach();
```

The Tor thread is **detached** at startup and never joined. `CTorEmbedded::Stop()` at line 266-278 only flips a `running` atomic — it has no real teardown on either platform:

- **Linux:** `#ifndef WIN32` block is a no-op (comment-only TODO)
- **Windows:** no block at all — function body ends after `running.store(false)`

`tor_run_main()` blocks in the Tor event loop indefinitely. The OS process **cannot exit** while that thread is alive, regardless of `main()` returning 0. `Shutdown()` in `init.cpp` finishes its bookkeeping and sets `fExit = true`, `main()` returns, but the process keeps running because the detached Tor thread is still in the event loop.

`ExitTimeout` (init.cpp:143) is only useful for deadlock *after* `Shutdown()` returns — it doesn't help here.

## Fix plan

### 1. `src/tor/tor_embedded.cpp` — actually stop the Tor thread

Two-part fix:

a) Store the `std::thread` handle (not detached):
```cpp
std::thread torThread(TorThreadFunc, argv);
// do NOT detach
torThreadHandle = std::move(torThread);
```

b) In `Stop()`, on the **main thread**, send a Tor control command to ask the daemon to shut down. Tor's `tor_api` doesn't expose this in 0.4.x but the embedded Tor opens a control port by default OR we can use the simpler approach: send `SIGTERM` to ourselves on Linux, and on Windows post a custom event to the Tor thread.

For Windows specifically: the cleanest approach is to use Tor's `tor_api_shutdown()` if available, OR fall back to `TerminateThread` after a 5-second grace period. Since the wallet is going to exit anyway, `TerminateThread` is acceptable here as a last-resort — we mark the thread as unjoinable and let OS clean it up.

### 2. `src/i2p/i2p_embedded.cpp` — same fix for I2P

I2P has a cleaner API: `i2p::api::StopI2P()` and `i2p::api::TerminateI2P()` exist (line 643, 646). The background thread in the lambda at line 527 is also detached. Same fix pattern: capture the thread handle, join it (with a 5s timeout fallback) in `Stop()`.

### 3. `src/init.cpp` `Shutdown()` — add an overall watchdog

Wrap the shutdown sequence in a timed watchdog. If `Shutdown()` doesn't return within 30 seconds, log where it got stuck and call `ExitProcess(0)` (Windows) / `_exit(0)` (Linux) to force-exit. This is the belt-and-suspenders that ensures the wallet ALWAYS closes, even if the I2P/Tor stop is partially broken in a future release.

### 4. Belt-and-suspenders: pre-`Shutdown` user signal handler

Add a `WM_CLOSE` handler that, on second close attempt (when one is already in progress), immediately force-exits. This is a UX improvement so users with a stuck wallet can force-close via X.

## Files to modify

- `src/tor/tor_embedded.cpp` (Stop() implementation)
- `src/tor/tor_embedded.h` (thread member + Stop() signature)
- `src/i2p/i2p_embedded.cpp` (Stop() implementation, thread capture)
- `src/i2p/i2p_embedded.h` (thread member)
- `src/init.cpp` (Shutdown() watchdog, force-exit on timeout)

## Test plan

1. Build CI green
2. Manual Windows test: open wallet, wait for Tor/I2P ready, close, verify < 5s shutdown
3. Manual Windows test: open wallet, immediately close, verify no hang
4. Manual Linux test: same as #2, verify clean exit
5. Stress test: open + close 5 times in a row, no resource leak

## Risk

- `TerminateThread` on Tor is unsafe but happens only on graceful timeout path
- The watchdog `_exit(0)` skips destructors; acceptable because the wallet is exiting anyway
- i2pd internals may have already-closed state; guarded with try/catch
