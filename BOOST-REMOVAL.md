# Boost removal — progress

Goal: drop the Boost dependency in favor of C++17 std. No consensus or wire
behavior changes.

## Done

**Triangles' own code (daemon + GUI) is now completely Boost-free.** All nine
translation units that used Boost have been migrated. The only remaining Boost
usage in the tree is (1) the Boost.Test unit-test framework under `src/test/`,
and (2) Boost as a *transitive link dependency of the bundled embedded i2pd
router* (`libi2pd.a`) — not of any Triangles source. See "Remaining" below.

| File | Boost removed | Replacement |
|------|---------------|-------------|
| `txdb-leveldb.cpp` | `boost/version.hpp` (unused include) | deleted |
| `txdb-rocksdb.cpp` | `boost/version.hpp` (unused include) | deleted |
| `walletdb.cpp` | `boost/version.hpp` + `BOOST_VERSION` guard | unconditional `std::filesystem` branch |
| `util.cpp` | `boost::program_options` config-file parser + `to_internal` workaround | small C++17 INI parser in `ReadConfigFile` |
| `init.cpp` | `boost::interprocess::file_lock` + `using namespace boost` | portable `LockDataDirectory()` (`flock` POSIX / `LockFileEx` Win32) |
| `rpcdump.cpp` | `boost::posix_time` + `boost::gregorian` | `std::get_time` + `timegm`/`_mkgmtime` |

`wallet.cpp` and `triangles-cli.cpp` only ever *mentioned* Boost in comments —
no code change needed.

### Behavior notes for review
- **Config parser**: `name = value`; a line whose first non-whitespace char is
  `#` is a comment; blank lines ignored; inline `#` is NOT a comment (so
  `rpcpassword` may contain `#`). First value wins for single-valued settings;
  `-name` keying and `nofoo=` negative-setting interpretation preserved.
- **File lock**: exclusive, non-blocking; the fd/handle is held for process
  lifetime and released by the OS on exit (matches the old file_lock lifetime).
- **Dump time parser**: same five accepted formats, parsed as UTC.

### CMake note
`program_options` is no longer used by any source file and can be dropped from
the `find_package(Boost ... COMPONENTS ...)` list once the remaining two files
are migrated. It is left in place for now because removing it before the Asio
migration provides no benefit and the component is harmless if installed.

### RPC server (done — `trianglesrpc.cpp`)

The JSON-RPC/HTTP server previously used `boost::asio` (async sockets +
`boost::asio::ssl`), `boost::bind`, `boost::iostreams`,
`boost::shared_ptr`/`weak_ptr`, and `boost::system::error_code`. It was
rewritten onto **raw BSD sockets** behind a small `std::iostream`
(`src/rpc_httpsocket.h`), preserving the thread-per-connection model so the
HTTP parser, JSON-RPC dispatch, REST handler, and the blocking SSE handler are
all unchanged.

- New `src/rpc_httpsocket.h`: `CSocketIOStream` (a `std::iostream` over a
  `SOCKET`), `ConnectRPCSocket()`, `BindRPCSockets()` (separate IPv4/IPv6
  listeners, loopback unless `-rpcallowip`), `SockaddrToString()`.
- `ThreadRPCServer2` now binds sockets and runs a `select()`-based accept loop
  that spawns `ThreadRPCServer3` per connection.
- `ClientAllowed` takes a numeric IP string.
- `CallRPC` connects via a raw socket.
- **`-rpcssl` is removed.** RPC TLS was a rarely used Asio::ssl feature; for
  remote access, front the port with stunnel/nginx or reach it over SSH/Tor
  (the same decision Bitcoin Core made). A warning is logged if `-rpcssl` is set.

### Qt URI handler (done — `qt/qtipcserver.cpp`)

The `triangles:` single-instance URI handoff used
`boost::interprocess::message_queue` + `boost::posix_time`. Rewritten onto
`QLocalServer` / `QLocalSocket` (QtNetwork), keeping the existing polling-thread
model via the blocking `waitForNewConnection` / `waitForReadyRead` /
`waitForConnected` methods (no Qt event loop required). `Qt5::Network` added to
the Qt find_package and the `triangles-qt` link.

### CMake
- `Boost::program_options`, `Boost::thread`, `Boost::chrono` removed from the
  `triangles_common` link — Triangles' own objects reference no Boost symbols.

## Remaining

Two things still pull Boost into the build; neither is Triangles source:

1. **Embedded i2pd router.** When built with the embedded I2P router, the
   bundled `libi2pd.a` / `libi2pdclient.a` link Boost
   (`program_options`, `thread`, `chrono`, `filesystem`, `system`). The
   i2pd-specific link block (and the top-level `find_package(Boost ...)`) are
   therefore left intact. Fully dropping Boost from the build requires either a
   Boost-free i2pd build or disabling the embedded router. This is an upstream
   i2pd concern, not Triangles code.

2. **Unit tests.** `src/test/*` use the Boost.Test framework
   (`Boost::unit_test_framework`). Optional follow-up: port to a header-only
   framework (e.g. Catch2/doctest) to remove the last first-party Boost use.

When both are addressed, `find_package(Boost ...)` can be removed entirely.
