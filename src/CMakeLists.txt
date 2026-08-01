# src/CMakeLists.txt
# Defines all build targets: libraries and executables.

# ═══════════════════════════════════════════════════════════════════════════════
# 1. Hash9 cryptographic primitives (pure C)
# ═══════════════════════════════════════════════════════════════════════════════
add_library(hash9_crypto STATIC
    blake.c
    groestl.c
    jh.c
    keccak.c
    skein.c
    aes_helper.c
    bmw.c
    cubehash.c
    echo.c
    fugue.c
    hamsi.c
    hamsi_helper.c
    luffa.c
    shavite.c
    simd.c
)
target_include_directories(hash9_crypto PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(hash9_crypto PROPERTIES LINKER_LANGUAGE C)
# Hash9 C files have colliding static symbols (IV512, DECL_STATE, etc.) — skip unity
set_target_properties(hash9_crypto PROPERTIES UNITY_BUILD OFF)

# ═══════════════════════════════════════════════════════════════════════════════
# 2. JSON library (header-only nlohmann/json via json_compat.h shim)
# ═══════════════════════════════════════════════════════════════════════════════
add_library(json_compat INTERFACE)
target_include_directories(json_compat INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/json")

# ═══════════════════════════════════════════════════════════════════════════════
# 3. Common core library (shared between daemon, Qt, and tests)
#
#    EXCLUDES init.cpp, wallet.cpp (QT_GUI-conditional), noui.cpp (target-specific)
# ═══════════════════════════════════════════════════════════════════════════════
set(CORE_SOURCES
    addrman.cpp
    bootstrap.cpp
    checkpointpublisher.cpp
    checkpoints.cpp
    crypter.cpp
    hdwallet.cpp
    crypto_ecdh.cpp
    crypto_ecdsa.cpp
    db.cpp
    key.cpp
    keystore.cpp
    main.cpp
    miner.cpp
    net.cpp
    net_bootstrap.cpp
    netbase.cpp
    protocol.cpp
    script.cpp
    sync.cpp
    util.cpp
    version.cpp
    walletdb.cpp
    kernel.cpp
    pbkdf2.cpp
    scrypt.cpp
    smessage.cpp
    syncmanager.cpp
    chaindb_migrate.cpp
    tor_embed_hooks.cpp
    rest.cpp
    trianglesrpc.cpp
    rpcdump.cpp
    rpcnet.cpp
    rpcmining.cpp
    rpcwallet.cpp
    rpcblockchain.cpp
    rpcrawtransaction.cpp
    rpcsmessage.cpp
    zmqpublishnotifier.cpp
    txdb-base.cpp
    txdb-factory.cpp
    txdb-leveldb.cpp
    utxosnapshot.cpp
    snapshotnet.cpp
    lz4/lz4.c
    tor/onion_v3.cpp
    tor/tor_process.cpp
    tor/tor_embedded.cpp
    i2p/i2p_embedded.cpp
)

# Scrypt assembly — platform-specific
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    enable_language(ASM)
    list(APPEND CORE_SOURCES scrypt-x86_64.S)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|x86")
    enable_language(ASM)
    list(APPEND CORE_SOURCES scrypt-x86.S)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    enable_language(ASM)
    list(APPEND CORE_SOURCES scrypt-arm.S)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|ARM")
    enable_language(ASM)
    list(APPEND CORE_SOURCES scrypt-arm.S)
endif()

# RocksDB chain database backend (always built; see top-level CMakeLists.txt
# for the rationale — RocksDB also backs the smessage store).
list(APPEND CORE_SOURCES txdb-rocksdb.cpp)

# Modernization: SQLite wallet DB backend + Berkeley→SQLite migration.
# Built unconditionally; selection happens at runtime via -walletdb.
list(APPEND CORE_SOURCES
    walletdb-factory.cpp
    walletdb-sqlite.cpp
    walletdb-recover.cpp
    walletmigrate.cpp
)

add_library(triangles_common OBJECT ${CORE_SOURCES})

# When BUILD_FUZZ=ON, the fuzz target links these .o files directly into
# bin/fuzz_script. The link line enables -fsanitize=fuzzer,address,undefined
# so EVERY .o referenced from the fuzz binary must also be compiled with the
# matching -fsanitize=address,undefined,fuzzer-no-link. Without this, gcc-
# built triangles_common objects reference libstdc++-injected ubsan runtime
# symbols (e.g. __ubsan_handle_function_type_mismatch_v1_abort) that clang's
# libubsan_standalone runtime doesn't provide, and the link fails with
# "undefined reference to __ubsan_handle_function_type_mismatch_v1_abort".
if(BUILD_FUZZ)
    target_compile_options(triangles_common PRIVATE
        -fsanitize=address,undefined,fuzzer-no-link
        -fno-omit-frame-pointer
        -fno-sanitize-recover=undefined
        -fno-sanitize=alignment,signed-integer-overflow,vptr
    )
endif()

target_include_directories(triangles_common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/json"
    "${CMAKE_CURRENT_SOURCE_DIR}/tor"
    "${CMAKE_CURRENT_SOURCE_DIR}/i2p"
    "${CMAKE_BINARY_DIR}/generated"       # for build.h
)

target_compile_definitions(triangles_common PUBLIC HAVE_BUILD_INFO)

target_link_libraries(triangles_common PUBLIC
    hash9_crypto
    json_compat
    leveldb_bundled
    OpenSSL::SSL
    OpenSSL::Crypto
    BerkeleyDB::BerkeleyDB
    Libevent::Libevent
    ZLIB::ZLIB
    Threads::Threads
    SQLite::SQLite3
)

# Optional: UPnP
if(USE_UPNP)
    target_compile_definitions(triangles_common PUBLIC USE_UPNP=1 STATICLIB MINIUPNP_STATICLIB)
    target_link_libraries(triangles_common PUBLIC Miniupnpc::Miniupnpc)
    if(WIN32)
        target_link_libraries(triangles_common PUBLIC iphlpapi)
    endif()
endif()

# Optional: IPv6
if(USE_IPV6)
    target_compile_definitions(triangles_common PUBLIC USE_IPV6=1)
endif()

# Optional: ZMQ
if(USE_ZMQ)
    target_compile_definitions(triangles_common PUBLIC ENABLE_ZMQ)
    target_link_libraries(triangles_common PUBLIC PkgConfig::ZMQ)
endif()

# libsecp256k1 (mandatory) — ECDH / ECDSA replacement for OpenSSL EC.
# Provided by add_subdirectory(src/secp256k1) in the top-level CMakeLists.
target_link_libraries(triangles_common PUBLIC secp256k1)

# RocksDB (mandatory)
if(TARGET RocksDB::rocksdb)
    target_link_libraries(triangles_common PUBLIC RocksDB::rocksdb)
elseif(TARGET PkgConfig::RocksDB)
    target_link_libraries(triangles_common PUBLIC PkgConfig::RocksDB)
endif()

# Optional: Embedded Tor
if(USE_TOR_EMBEDDED)
    if(TOR_SOURCE_ROOT STREQUAL "")
        set(TOR_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/tor/tor-src")
    endif()
    target_compile_definitions(triangles_common PUBLIC ENABLE_TOR_EMBEDDED)
    target_include_directories(triangles_common PUBLIC "${TOR_SOURCE_ROOT}/src/feature/api")
    target_link_directories(triangles_common PUBLIC "${TOR_SOURCE_ROOT}")
    # libtor.a has circular deps with libevent/openssl/zlib
    # OpenSSL and zlib already linked via imported targets above, so only add
    # libevent and compression libs that libtor needs but aren't yet linked.
    # --start-group / --end-group resolves circular references between libtor
    # and its dependencies.
    # Use --allow-multiple-definition because libtor.a may pull in static
    # OpenSSL objects that duplicate the DLL import lib already linked above.
    # These GNU ld options are not supported on macOS (which uses lld) —
    # guard with NOT APPLE so the build still works on macOS.
    # On macOS, the libevent/openssl/zlib install paths are not on the
    # default linker search path. Pull them in from the standard
    # homebrew locations so -levent / -lssl / -lssl etc. resolve.
    if(APPLE)
        target_link_directories(triangles_common PUBLIC
            /opt/homebrew/opt/libevent/lib
            /opt/homebrew/opt/openssl@3/lib
            /opt/homebrew/opt/zlib/lib
        )
    endif()
    if(NOT APPLE)
        target_link_libraries(triangles_common PUBLIC
            -Wl,--allow-multiple-definition
            -Wl,--start-group
        )
    endif()
    target_link_libraries(triangles_common PUBLIC
        -ltor
        -levent -levent_core -levent_extra -levent_openssl
        -lssl -lcrypto -lz -llzma -lzstd
    )
    if(NOT APPLE)
        target_link_libraries(triangles_common PUBLIC
            -Wl,--end-group
        )
    endif()
    if(WIN32)
        target_link_libraries(triangles_common PUBLIC iphlpapi shlwapi crypt32)
    endif()
endif()

# Optional: Embedded I2P (i2pd)
if(USE_I2P_EMBEDDED)
    if(I2P_SOURCE_ROOT STREQUAL "")
        set(I2P_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/i2p/i2pd-src")
    endif()
    if(NOT EXISTS "${I2P_SOURCE_ROOT}/libi2pd/Crypto.h")
        message(FATAL_ERROR
            "USE_I2P_EMBEDDED=ON but i2pd source not found at ${I2P_SOURCE_ROOT}.\n"
            "Run: git submodule update --init --recursive\n"
            "Or set -DI2P_SOURCE_ROOT=/path/to/i2pd")
    endif()
    target_compile_definitions(triangles_common PUBLIC ENABLE_I2P_EMBEDDED)
    target_include_directories(triangles_common PUBLIC
        "${I2P_SOURCE_ROOT}"
        "${I2P_SOURCE_ROOT}/libi2pd"
        "${I2P_SOURCE_ROOT}/libi2pd_client"
        "${I2P_SOURCE_ROOT}/i18n"
    )
    # i2pd builds as two static libraries: libi2pd.a (core router) and
    # libi2pd_client.a (SAM, SOCKS, tunnels, client context). Both are needed.
    # i2pd's own Makefile.mingw links by full static .a paths rather than
    # -l flags because MinGW's linker is single-pass and CMake imported
    # targets (Boost::) may not exist on MSYS2. We follow the same pattern:
    # link the archives, then their Boost/zlib deps as full paths, then
    # the archives again to resolve the second-pass references.
    target_link_libraries(triangles_common PUBLIC
        "${I2P_SOURCE_ROOT}/libi2pdclient.a"
        "${I2P_SOURCE_ROOT}/libi2pd.a"
        "${I2P_SOURCE_ROOT}/libi2pdlang.a"
    )
    if(WIN32)
        # MinGW/MSYS2: Boost:: CMake imported targets are unreliable here.
        # Use find_library to locate the actual .a/.dll files. Some Boost
        # libs (e.g. boost_system) are header-only in newer versions and
        # won't have a .a file at all — that's fine, we skip them.
        if(NOT MINGW_PREFIX)
            if(DEFINED ENV{MINGW_PREFIX})
                set(MINGW_PREFIX "$ENV{MINGW_PREFIX}")
            else()
                set(MINGW_PREFIX "/mingw64")
            endif()
        endif()
        find_library(I2P_BOOST_FS  NAMES boost_filesystem-mt boost_filesystem libboost_filesystem-mt  HINTS "${MINGW_PREFIX}/lib")
        find_library(I2P_BOOST_PO  NAMES boost_program_options-mt boost_program_options libboost_program_options-mt  HINTS "${MINGW_PREFIX}/lib")
        find_library(I2P_BOOST_SYS NAMES boost_system-mt boost_system libboost_system-mt  HINTS "${MINGW_PREFIX}/lib")
        find_library(I2P_SSL       NAMES ssl libssl  HINTS "${MINGW_PREFIX}/lib")
        find_library(I2P_CRYPTO    NAMES crypto libcrypto  HINTS "${MINGW_PREFIX}/lib")
        find_library(I2P_Z         NAMES z libz zlib  HINTS "${MINGW_PREFIX}/lib")
        set(I2P_WIN_LIBS "")
        foreach(lib I2P_BOOST_FS I2P_BOOST_PO I2P_BOOST_SYS I2P_SSL I2P_CRYPTO I2P_Z)
            if(${lib})
                list(APPEND I2P_WIN_LIBS "${${lib}}")
                message(STATUS "  I2P link: ${lib} = ${${lib}}")
            else()
                message(STATUS "  I2P link: ${lib} = (not found, header-only?)")
            endif()
        endforeach()
        target_link_libraries(triangles_common PUBLIC ${I2P_WIN_LIBS} -Wl,--allow-multiple-definition)
    else()
        target_link_libraries(triangles_common PUBLIC
            Boost::program_options Boost::thread Boost::chrono
            OpenSSL::SSL OpenSSL::Crypto
            ZLIB::ZLIB
        )
        if(TARGET Boost::filesystem)
            target_link_libraries(triangles_common PUBLIC Boost::filesystem)
        endif()
        if(TARGET Boost::system)
            target_link_libraries(triangles_common PUBLIC Boost::system)
        endif()
    endif()
    # Second pass: list archives again so linker resolves i2pd→Boost refs
    # that were unsatisfied in the first left-to-right pass.
    target_link_libraries(triangles_common PUBLIC
        "${I2P_SOURCE_ROOT}/libi2pd.a"
        "${I2P_SOURCE_ROOT}/libi2pdclient.a"
    )
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(triangles_common PUBLIC
        ws2_32 shlwapi mswsock ole32 oleaut32 uuid gdi32 crypt32)
elseif(APPLE)
    target_link_libraries(triangles_common PUBLIC
        "-framework Foundation"
        "-framework ApplicationServices"
        "-framework AppKit")
else()
    # Linux
    target_link_libraries(triangles_common PUBLIC rt dl)
endif()

add_dependencies(triangles_common generate_build_info build_leveldb)

# ── Precompiled header (heavy STL + Boost + OpenSSL includes, C++ only) ──
target_precompile_headers(triangles_common PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:<string$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<vector$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<map$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<deque$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<algorithm$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<sstream$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<stdexcept$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<cstdint$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<cstring$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<memory$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<functional$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<filesystem$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<fstream$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<thread$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<mutex$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<condition_variable$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<boost/algorithm/string.hpp$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<openssl/sha.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<openssl/crypto.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<openssl/rand.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<openssl/evp.h$<ANGLE-R>>"
)

# ═══════════════════════════════════════════════════════════════════════════════
# 4. Headless daemon (trianglesd)
# ═══════════════════════════════════════════════════════════════════════════════
# `trianglesd` is normally an add_executable, but the libFuzzer build only
# needs the daemon's object files (init/wallet/noui). Building the executable
# under clang-15 with -fsanitize=fuzzer+address+undefined pulls in
# undefined references to the libstdc++ runtime built by gcc, which fails
# the link step. So we expose the daemon's sources as an OBJECT library and
# only attach them to trianglesd when we're not in a fuzz build.
set(DAEMON_SOURCES
    noui.cpp
    init.cpp
    wallet.cpp
)
if(BUILD_FUZZ)
    add_library(trianglesd_objects OBJECT ${DAEMON_SOURCES})
    target_link_libraries(trianglesd_objects PRIVATE triangles_common)
    target_precompile_headers(trianglesd_objects REUSE_FROM triangles_common)
    # Match triangles_common's sanitizer instrumentation so noui.cpp / init.cpp
    # / wallet.cpp .o files don't reference the gcc libstdc++ ubsan runtime
    # when linked into the fuzz binary (see triangles_common compile-options
    # comment above for the full rationale).
    target_compile_options(trianglesd_objects PRIVATE
        -fsanitize=address,undefined,fuzzer-no-link
        -fno-omit-frame-pointer
        -fno-sanitize-recover=undefined
        -fno-sanitize=alignment,signed-integer-overflow,vptr
    )
    if(WIN32)
        set_target_properties(trianglesd_objects PROPERTIES SUFFIX ".obj")
    endif()
elseif(BUILD_DAEMON)
    add_executable(trianglesd ${DAEMON_SOURCES})
    # No QT_GUI define — daemon gets the #if !defined(QT_GUI) code paths
    target_link_libraries(trianglesd PRIVATE triangles_common)
    target_precompile_headers(trianglesd REUSE_FROM triangles_common)

    if(WIN32)
        set_target_properties(trianglesd PROPERTIES SUFFIX ".exe")
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# 4b. JSON-RPC client (triangles-cli)
#
# Self-contained: only links univalue + boost::asio + boost::program_options
# + boost::filesystem + OpenSSL (for base64 / future TLS). Does NOT link
# triangles_common, wallet, or net — keeps the binary small.
# ═══════════════════════════════════════════════════════════════════════════════
if(BUILD_CLI)
    add_executable(triangles-cli
        triangles-cli.cpp
    )
    # No Boost dependency: uses raw POSIX/Winsock sockets for HTTP. Only links
    # the json_compat header-only shim and the platform's native socket lib
    # (Winsock ws2_32 on Windows; libc on POSIX). Keeps the binary small and
    # avoids per-platform Boost linking pain (MSYS2 uses versioned -mt- names;
    # Homebrew doesn't ship the boost_system CMake config).
    target_link_libraries(triangles-cli
        PRIVATE
            json_compat
    )

    if(WIN32)
        set_target_properties(triangles-cli PROPERTIES SUFFIX ".exe")
        target_link_libraries(triangles-cli PRIVATE ws2_32)
    endif()

    if(MSVC)
        set_target_properties(triangles-cli PROPERTIES
            VS_WINRT_COMPONENT "console"
        )
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# 5. Qt5 GUI wallet (triangles-qt)
# ═══════════════════════════════════════════════════════════════════════════════
if(BUILD_QT)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)

    set(CMAKE_AUTOUIC_SEARCH_PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/qt/forms"
        "${CMAKE_CURRENT_SOURCE_DIR}/qt/plugins/mrichtexteditor"
    )

    # ui_interface.h is a hand-written header (Bitcoin convention), NOT a Qt
    # Designer file. Disable AutoUic globally and run UIC manually for real .ui files.
    set(CMAKE_AUTOUIC OFF)

    # Collect all .ui files and run UIC on them explicitly
    file(GLOB_RECURSE UI_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/qt/forms/*.ui"
        "${CMAKE_CURRENT_SOURCE_DIR}/qt/plugins/mrichtexteditor/*.ui"
    )
    qt5_wrap_ui(UI_HEADERS ${UI_FILES})

    set(QT_SOURCES
        qt/triangles.cpp
        qt/trianglesgui.cpp
        qt/transactiontablemodel.cpp
        qt/addresstablemodel.cpp
        qt/optionsdialog.cpp
        qt/sendcoinsdialog.cpp
        qt/coincontroldialog.cpp
        qt/coincontroltreewidget.cpp
        qt/addressbookpage.cpp
        qt/aboutdialog.cpp
        qt/introdialog.cpp
        qt/editaddressdialog.cpp
        qt/trianglesaddressvalidator.cpp
        qt/clientmodel.cpp
        qt/guiutil.cpp
        qt/transactionrecord.cpp
        qt/optionsmodel.cpp
        qt/monitoreddatamapper.cpp
        qt/transactiondesc.cpp
        qt/transactiondescdialog.cpp
        qt/trianglesstrings.cpp
        qt/trianglesamountfield.cpp
        qt/transactionfilterproxy.cpp
        qt/transactionview.cpp
        qt/walletmodel.cpp
        qt/overviewpage.cpp
        qt/csvmodelwriter.cpp
        qt/sendcoinsentry.cpp
        qt/qvalidatedlineedit.cpp
        qt/trianglesunits.cpp
        qt/qvaluecombobox.cpp
        qt/askpassphrasedialog.cpp
        qt/hdseeddialog.cpp
        qt/outlinedlabel.cpp
        qt/notificator.cpp
        qt/qtipcserver.cpp
        qt/rpcconsole.cpp
        qt/messagepage.cpp
        qt/dialog_move_handler.cpp
        qt/signmessagepage.cpp
        qt/verifymessagepage.cpp
        qt/messagemodel.cpp
        qt/sendmessagesdialog.cpp
        qt/sendmessagesentry.cpp
        qt/qvalidatedtextedit.cpp
        qt/plugins/mrichtexteditor/mrichtextedit.cpp
    )

    set(QT_RESOURCES qt/triangles.qrc)

    set(QT_FORMS
        qt/forms/coincontroldialog.ui
        qt/forms/sendcoinsdialog.ui
        qt/forms/addressbookpage.ui
        qt/forms/aboutdialog.ui
        qt/forms/editaddressdialog.ui
        qt/forms/transactiondescdialog.ui
        qt/forms/overviewpage.ui
        qt/forms/sendcoinsentry.ui
        qt/forms/askpassphrasedialog.ui
        qt/forms/rpcconsole.ui
        qt/forms/optionsdialog.ui
        qt/forms/messagepage.ui
        qt/forms/sendmessagesentry.ui
        qt/forms/sendmessagesdialog.ui
        qt/plugins/mrichtexteditor/mrichtextedit.ui
        qt/forms/mainwindow.ui
        qt/forms/signmessagepage.ui
        qt/forms/verifymessagepage.ui
        qt/forms/transactionspage.ui
    )

    # Optional QR code dialog
    if(USE_QRCODE)
        list(APPEND QT_SOURCES qt/qrcodedialog.cpp)
        list(APPEND QT_FORMS qt/forms/qrcodedialog.ui)
    endif()

    # macOS Objective-C++ sources
    if(APPLE)
        list(APPEND QT_SOURCES
            qt/macdockiconhandler.mm
            qt/macnotificationhandler.mm
        )
    endif()

    add_executable(triangles-qt WIN32 MACOSX_BUNDLE
        ${QT_SOURCES}
        ${QT_RESOURCES}
        ${QT_FORMS}
        ${UI_HEADERS}
        # Per-target: compiled with QT_GUI define
        init.cpp
        wallet.cpp
        noui.cpp
    )

    target_compile_definitions(triangles-qt PRIVATE
        QT_GUI
        QT_DISABLE_DEPRECATED_BEFORE=0
    )

    target_include_directories(triangles-qt PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/qt"
        "${CMAKE_CURRENT_SOURCE_DIR}/qt/plugins/mrichtexteditor"
        "${CMAKE_CURRENT_BINARY_DIR}"
    )

    target_link_libraries(triangles-qt PRIVATE
        triangles_common
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Network
    )

    # Optional: D-Bus notifications (Linux)
    if(USE_DBUS)
        target_compile_definitions(triangles-qt PRIVATE USE_DBUS)
        target_link_libraries(triangles-qt PRIVATE Qt5::DBus)
    endif()

    # Optional: QR code
    if(USE_QRCODE)
        target_compile_definitions(triangles-qt PRIVATE USE_QRCODE)
        target_link_libraries(triangles-qt PRIVATE QRencode::QRencode)
    endif()

    # Windows resource file (.rc with version info and icon)
    if(WIN32)
        target_sources(triangles-qt PRIVATE qt/res/triangles-qt.rc)
        # Ensure RC compiler can find clientversion.h
        if(MINGW)
            set_source_files_properties(qt/res/triangles-qt.rc PROPERTIES
                COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}"
            )
        endif()
    endif()

    # macOS bundle settings
    if(APPLE)
        set_target_properties(triangles-qt PROPERTIES
            OUTPUT_NAME "Triangles-Qt"
            MACOSX_BUNDLE_ICON_FILE triangles.icns
            MACOSX_BUNDLE_BUNDLE_NAME "Triangles-Qt"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
        )
        set_source_files_properties(
            "${CMAKE_CURRENT_SOURCE_DIR}/qt/res/icons/triangles.icns"
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
        )
        target_sources(triangles-qt PRIVATE qt/res/icons/triangles.icns)
    endif()

    # Translations (optional — requires LinguistTools)
    if(TARGET Qt5::lrelease)
        file(GLOB TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/qt/locale/triangles_*.ts")
        if(TS_FILES)
            set_source_files_properties(${TS_FILES} PROPERTIES
                OUTPUT_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/qt/locale"
            )
            qt5_add_translation(QM_FILES ${TS_FILES})
            target_sources(triangles-qt PRIVATE ${QM_FILES})
        endif()
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# 6. Unit tests (test_triangles)
# ═══════════════════════════════════════════════════════════════════════════════
if(BUILD_TESTS)
    enable_testing()

    file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
    # Exclude miner_tests.cpp (never ported from Bitcoin)
    list(FILTER TEST_SOURCES EXCLUDE REGEX "miner_tests\\.cpp$")
    # Exclude the standalone chaindb test driver — it gets its own target
    # because it needs to run without the TestingSetup global fixture.
    list(FILTER TEST_SOURCES EXCLUDE REGEX "chaindb_equivalence_tests_main\\.cpp$")
    # These two are standalone test drivers: each #defines its own
    # BOOST_TEST_MODULE and redefines the wallet/UI globals, and each has
    # a dedicated executable + add_test below. They must NOT also be
    # globbed into test_triangles, or the duplicate module/main and global
    # symbols only link by virtue of -Wl,--allow-multiple-definition (which
    # silently drops duplicates and can run their suites under the wrong
    # global fixture). Excluding them keeps each standalone module isolated.
    list(FILTER TEST_SOURCES EXCLUDE REGEX "chaindb_runtime_tests\\.cpp$")
    list(FILTER TEST_SOURCES EXCLUDE REGEX "snapshotnet_tests\\.cpp$")

    add_executable(test_triangles
        ${TEST_SOURCES}
        # Per-target: wallet without QT_GUI, noui for noui_connect()
        wallet.cpp
        noui.cpp
    )
    # No init.cpp — test_triangles.cpp provides its own StartShutdown() stub

    target_compile_definitions(test_triangles PRIVATE
        "TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/test/data"
    )

    target_include_directories(test_triangles PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test"
    )

    target_link_libraries(test_triangles PRIVATE
        triangles_common
        Boost::unit_test_framework
    )

    add_test(NAME triangles_unit_tests COMMAND test_triangles --log_level=test_suite)

    # ── Standalone chaindb equivalence tests ─────────────────────────────────
    # Runs without the TestingSetup global fixture (which would otherwise
    # open the real chain DB and lock it for the process). Sets a fresh
    # temp -datadir via its own global fixture, then runs the
    # chaindb_equivalence_tests suite.
    add_executable(test_chaindb_equivalence
        "${CMAKE_CURRENT_SOURCE_DIR}/test/chaindb_equivalence_tests_main.cpp"
        # wallet.cpp provides the CWallet symbols that triangles_common
        # (txdb-rocksdb, net, etc.) references, even though the chaindb
        # tests themselves don't use the wallet.
        wallet.cpp
    )
    target_include_directories(test_chaindb_equivalence PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test"
        "${CMAKE_CURRENT_SOURCE_DIR}/leveldb/include"
    )
    target_link_libraries(test_chaindb_equivalence PRIVATE
        triangles_common
        Boost::unit_test_framework
    )
    add_test(NAME chaindb_equivalence_tests
             COMMAND test_chaindb_equivalence --log_level=test_suite)

    # ── Standalone snapshotnet P2P tests ────────────────────────────────────
    # Same rationale as test_chaindb_equivalence: snapshotnet needs filesystem
    # and threading globals and its own tmp datadir fixture, which would
    # conflict with test_triangles' heavy TestingSetup. Runs independently.
    add_executable(test_snapshotnet
        "${CMAKE_CURRENT_SOURCE_DIR}/test/snapshotnet_tests.cpp"
        wallet.cpp
    )
    target_include_directories(test_snapshotnet PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test"
        "${CMAKE_CURRENT_SOURCE_DIR}/leveldb/include"
    )
    target_link_libraries(test_snapshotnet PRIVATE
        triangles_common
        Boost::unit_test_framework
    )
    add_test(NAME snapshotnet_tests
             COMMAND test_snapshotnet --log_level=test_suite)

    # ── Standalone chaindb runtime tests (CRocksTxDB wrapper layer) ─────────
    # Exercises MakeChainDB / WipeChainDataDir / IsRocksDbChainBackend and
    # the CRocksTxDB write/read/batch/iterator wrapper — the same code path
    # the daemon uses when launched with `-chaindb=rocksdb`. The
    # chaindb_equivalence_tests (above) only verify the byte-copy migration
    # via the raw leveldb/rocksdb APIs; this one verifies the wrapper class.
    add_executable(test_chaindb_runtime
        "${CMAKE_CURRENT_SOURCE_DIR}/test/chaindb_runtime_tests.cpp"
        wallet.cpp
    )
    target_include_directories(test_chaindb_runtime PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test"
        "${CMAKE_CURRENT_SOURCE_DIR}/leveldb/include"
    )
    target_link_libraries(test_chaindb_runtime PRIVATE
        triangles_common
        Boost::unit_test_framework
    )
    add_test(NAME chaindb_runtime_tests
             COMMAND test_chaindb_runtime --log_level=test_suite)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# 7. Fuzz harness (script interpreter) — opt-in via -DBUILD_FUZZ=ON
# ═══════════════════════════════════════════════════════════════════════════════
# LibFuzzer is built into clang since version 6; gcc doesn't support
# -fsanitize=fuzzer. We compile script_fuzz.cpp + script.cpp with clang++
# (so the interpreter itself is ASan/UBSan-instrumented) and link against
# the full triangles_common OBJECT library + the same library set trianglesd
# uses. Default build (gcc, no sanitizer) is unaffected.
#
# Build:
#   cmake -G Ninja -DBUILD_TESTS=ON -DBUILD_FUZZ=ON -DBUILD_DAEMON=ON ..
#   ninja fuzz_script
#
# Run:
#   ./bin/fuzz_script -max_total_time=300 corpus/
#
# See src/test/fuzz/README.md for corpus seeding and what it covers.
option(BUILD_FUZZ "Build libFuzzer harness for the script interpreter" OFF)
if(BUILD_FUZZ)
    find_program(CLANGXX clang++)
    if(NOT CLANGXX)
        message(FATAL_ERROR "BUILD_FUZZ=ON requires clang++; not found in PATH")
    endif()

    set(FUZZ_OBJ_DIR "${CMAKE_CURRENT_BINARY_DIR}/fuzz_objs")
    file(MAKE_DIRECTORY "${FUZZ_OBJ_DIR}")
    set(FUZZ_OBJ_SCRIPT_FUZZ "${FUZZ_OBJ_DIR}/script_fuzz.cpp.o")
    set(FUZZ_OBJ_SCRIPT      "${FUZZ_OBJ_DIR}/script.cpp.o")
    set(FUZZ_OBJ_FUZZ_STUBS  "${FUZZ_OBJ_DIR}/fuzz_stubs.cpp.o")
    set(FUZZ_FUZZ_STUBS_SRC  "${FUZZ_OBJ_DIR}/fuzz_stubs.cpp")
    set(FUZZ_BIN_DIR         "${CMAKE_BINARY_DIR}/bin")
    file(MAKE_DIRECTORY "${FUZZ_BIN_DIR}")
    set(FUZZ_BIN             "${FUZZ_BIN_DIR}/fuzz_script")
    set(FUZZ_SRC_FUZZ        "${CMAKE_CURRENT_SOURCE_DIR}/test/fuzz/script_fuzz.cpp")
    set(FUZZ_SRC_SCRIPT      "${CMAKE_CURRENT_SOURCE_DIR}/script.cpp")

    # --- Second fuzz target: transaction_deserialize_fuzz ---
    # CTransaction is declared in main.h and implemented in main.cpp, which is
    # part of triangles_common. The harness only needs the transaction
    # deserialize/serialize surface, not the script interpreter, so we don't
    # need a separate clang-instrumented copy of any .cpp file — we just link
    # the gcc-built triangles_common .o files directly. libFuzzer's link line
    # is compatible with gcc .o files for the non-instrumented units; only the
    # harness entry point itself needs clang + -fsanitize=fuzzer.
    set(FUZZ_TX_DESER_OBJ       "${FUZZ_OBJ_DIR}/transaction_deserialize_fuzz.cpp.o")
        set(FUZZ_TX_DESER_BIN_DIR   "${CMAKE_BINARY_DIR}/bin")
        set(FUZZ_TX_DESER_BIN       "${FUZZ_TX_DESER_BIN_DIR}/transaction_deserialize_fuzz")
        set(FUZZ_TX_DESER_SRC       "${CMAKE_CURRENT_SOURCE_DIR}/test/fuzz/transaction_deserialize_fuzz.cpp")
        set(FUZZ_TX_DESER_LINK_WRAPPER "${FUZZ_OBJ_DIR}/link_txdeser.sh")
        set(FUZZ_TX_DESER_LINK_WRAPPER_CONTENT [=[#!/bin/bash
    # Auto-generated by CMake (BUILD_FUZZ block). Link wrapper for the
    # transaction_deserialize_fuzz target. Discovers triangles_common +
    # trianglesd .o files at link time and exec's the clang++ link line.
    #
    # Differs from link.sh: this wrapper does NOT exclude script.cpp.o, because
    # wallet.cpp.o (in trianglesd_objects) calls ExtractDestination,
    # SignSignature, Solver, IsMine — all defined in script.cpp.o. We only exclude
    # init.cpp.o (which defines daemon main(), would conflict with libFuzzer's
    # main). See the BUILD_FUZZ block in src/CMakeLists.txt for full rationale.
    #
    # Usage: link_txdeser.sh clang++ [link-args...]
    # Final exec: clang++ <each .o> <each original link-arg>
    set -euo pipefail
    PROG="$1"
    shift
    TRIANGLES_COMMON_DIR="@CMAKE_CURRENT_BINARY_DIR@/CMakeFiles/triangles_common.dir"
    TRIANGLESD_DIR="@CMAKE_CURRENT_BINARY_DIR@/CMakeFiles/trianglesd_objects.dir"
    declare -a OBJS=()
    for f in "$TRIANGLES_COMMON_DIR"/*.o "$TRIANGLES_COMMON_DIR"/*/*.o; do
        [ -f "$f" ] || continue
        OBJS+=("$f")
    done
    if [ -d "$TRIANGLESD_DIR" ]; then
        for f in "$TRIANGLESD_DIR"/*.o; do
            [ -f "$f" ] || continue
            case "$f" in
                */init.cpp.o) continue ;;
            esac
            OBJS+=("$f")
        done
    fi
    exec "$PROG" "${OBJS[@]}" "$@"
    ]=])
        string(CONFIGURE "${FUZZ_TX_DESER_LINK_WRAPPER_CONTENT}"
               FUZZ_TX_DESER_LINK_WRAPPER_CONTENT @ONLY)
        file(WRITE "${FUZZ_TX_DESER_LINK_WRAPPER}" "${FUZZ_TX_DESER_LINK_WRAPPER_CONTENT}")
        file(CHMOD "${FUZZ_TX_DESER_LINK_WRAPPER}" PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    # Compile flags shared by both .cpp files. Pull in script.h, secp256k1,
    # leveldb. Same flags gcc uses for triangles_common (the project defines
    # HAVE_BUILD_INFO, LINUX, BOOST_THREAD_USE_LIB, etc.) so we don't hit
    # redefinition errors when linking against the rest of triangles_common.
    set(FUZZ_COMMON_FLAGS
        -std=c++20 -g -O1
        -fsanitize=fuzzer,address,undefined
        -DHAVE_CONFIG_H
        -DHAVE_BUILD_INFO
        -DLINUX
        -DUSE_IPV6=1
        -DBOOST_SPIRIT_THREADSAFE
        -DBOOST_THREAD_PROVIDES_GENERIC_SHARED_MUTEX_ON_WIN
        -DBOOST_THREAD_USE_LIB
        -DENABLE_TOR_EMBEDDED
        -DENABLE_I2P_EMBEDDED
        -DMINIUPNP_STATICLIB
        -DSTATICLIB
        -I${CMAKE_CURRENT_SOURCE_DIR}
        -I${CMAKE_CURRENT_SOURCE_DIR}/secp256k1/include
        -I${CMAKE_CURRENT_SOURCE_DIR}/leveldb/include
        -Wno-unused-parameter
        -Wno-deprecated-declarations
    )

    add_custom_command(
        OUTPUT "${FUZZ_OBJ_SCRIPT_FUZZ}"
        COMMAND ${CLANGXX} ${FUZZ_COMMON_FLAGS}
                -c ${FUZZ_SRC_FUZZ} -o ${FUZZ_OBJ_SCRIPT_FUZZ}
        DEPENDS ${FUZZ_SRC_FUZZ}
        COMMENT "[fuzz] clang++ script_fuzz.cpp"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${FUZZ_OBJ_SCRIPT}"
        COMMAND ${CLANGXX} ${FUZZ_COMMON_FLAGS}
                -c ${FUZZ_SRC_SCRIPT} -o ${FUZZ_OBJ_SCRIPT}
        DEPENDS ${FUZZ_SRC_SCRIPT}
        COMMENT "[fuzz] clang++ script.cpp"
        VERBATIM
    )

    # fuzz_stubs.cpp — satisfies globals owned by the excluded init.cpp that
    # triangles_common and trianglesd_objects reference (pwalletMain,
    # uiInterface, etc.). Keeping these as null/no-ops is the standard fuzzer
    # pattern — see src/test/test_triangles.cpp and
    # src/test/snapshotnet_tests.cpp for the same approach.
    file(MAKE_DIRECTORY "${FUZZ_OBJ_DIR}")
    file(WRITE "${FUZZ_FUZZ_STUBS_SRC}"
"#include <memory>
#include <set>
#include <string>
#include <vector>

#include \"checkpoints.h\"
#include \"key.h\"
#include \"keystore.h\"
#include \"script.h\"
#include \"ui_interface.h\"
#include \"wallet.h\"

class CBlockIndex;

bool fUseFastIndex = false;
unsigned int nDerivationMethodIndex = 0;
bool fEnforceCanonical = true;
bool fConfChange = false;

class CWalletStub : public CKeyStore
{
public:
    bool GetPubKey(const CKeyID&, CPubKey&) const override { return false; }
    bool GetKey(const CKeyID&, CKey&) const override { return false; }
    bool HaveKey(const CKeyID&) const override { return false; }
    void GetKeys(std::set<CKeyID>& setAddress) const override { setAddress.clear(); }
    bool AddKey(const CKey&) override { return false; }
    bool AddCScript(const CScript&) override { return false; }
    bool HaveCScript(const CScriptID&) const override { return false; }
    bool GetCScript(const CScriptID&, CScript&) const override { return false; }
};
static CWalletStub g_wallet_stub;
CWallet* pwalletMain = nullptr;

CClientUIInterface uiInterface;

// Checkpoints::CPMode defined in checkpoints.h; default to ADVISORY so the
// fuzz target never complains about the missing init.cpp value.
enum Checkpoints::CPMode CheckpointsMode = Checkpoints::ADVISORY;

// Defined in init.cpp; reasonable default so the fuzz link succeeds.
unsigned int nNodeLifespan = 7;

void StartShutdown() {}
void MarkShutdownFailure() {}
")

    add_custom_command(
        OUTPUT "${FUZZ_OBJ_FUZZ_STUBS}"
        COMMAND ${CLANGXX} ${FUZZ_COMMON_FLAGS}
                -c ${FUZZ_FUZZ_STUBS_SRC} -o ${FUZZ_OBJ_FUZZ_STUBS}
        DEPENDS ${FUZZ_FUZZ_STUBS_SRC}
        COMMENT "[fuzz] clang++ fuzz_stubs.cpp"
        VERBATIM
    )

    # Link using the same library set as trianglesd, but:
    #   - exclude script.cpp.o (we provide our own clang-instrumented one)
    #   - swap gcc for clang++ with -fsanitize=fuzzer,address,undefined
    #   - drop -Wl,-z,relro -Wl,-z,now (incompatible with sanitizer link)
    # The triangles_common / trianglesd .o file lists are discovered at link
    # time via the FUZZ_LINK_WRAPPER shell script (defined below). We do NOT
    # use file(GLOB) here — it runs at configure time when no .o files exist
    # on a fresh build dir, so the resulting list would always be empty.
    # The wrapper script does the find at link time and exec's clang++.

    # Build the link command. The triangles_common and trianglesd .o files
    # are discovered at link time via shell `find` because file(GLOB) only
    # runs at cmake configure time, when no .o files exist yet on a fresh
    # build dir. We invoke a small shell wrapper script that does the find
    # and exec's the link line with all .o files as args. We exclude
    # script.cpp.o from the triangles_common dir so we don't pull our
    # standalone copy of script.cpp in twice (we already have it in
    # ${FUZZ_OBJ_SCRIPT}).
    set(FUZZ_LINK_WRAPPER "${CMAKE_CURRENT_BINARY_DIR}/fuzz_objs/link.sh")
    # The wrapper script is invoked with the full link arg list as its
    # own argv. We pass it via ninja's COMMAND expansion with @{args}.
    # Strategy: write a here-doc style wrapper that uses bash-style
    # "$@" preservation. We use bash explicitly (not sh) for "$@" array
    # semantics — paths may contain spaces, so word-splitting on IFS
    # would corrupt them.
    set(FUZZ_LINK_WRAPPER_CONTENT [=[#!/bin/bash
# Auto-generated by CMake (BUILD_FUZZ block). Discovers triangles_common +
# trianglesd .o files at link time and exec's the clang++ link line.
#
# Usage: link.sh clang++ [link-args...]
# Final exec: clang++ <each .o> <each original link-arg>
set -euo pipefail
PROG="$1"
shift
TRIANGLES_COMMON_DIR="@CMAKE_CURRENT_BINARY_DIR@/CMakeFiles/triangles_common.dir"
TRIANGLESD_DIR="@CMAKE_CURRENT_BINARY_DIR@/CMakeFiles/trianglesd_objects.dir"
# Discover .o files into a bash array. Exclude script.cpp.o (we have our
# own clang-instrumented copy in fuzz_objs/ that we want to keep separate
# from the main build's copy).
declare -a OBJS=()
for f in "$TRIANGLES_COMMON_DIR"/*.o "$TRIANGLES_COMMON_DIR"/*/*.o; do
    [ -f "$f" ] || continue
    case "$f" in
        */script.cpp.o) continue ;;
    esac
    OBJS+=("$f")
done
if [ -d "$TRIANGLESD_DIR" ]; then
    for f in "$TRIANGLESD_DIR"/*.o; do
        [ -f "$f" ] || continue
        # init.cpp defines the daemon's main(); the fuzz harness has its own
        # (libFuzzer's). wallet.cpp, noui.cpp etc. are safe — they don't
        # define main and their external references (pwalletMain,
        # uiInterface, nDerivationMethodIndex) are satisfied by the stub
        # object file we add at the end of the link line.
        case "$f" in
            */init.cpp.o) continue ;;
        esac
        OBJS+=("$f")
    done
fi
# Final arg list: PROG, then all .o files, then all original link args.
exec "$PROG" "${OBJS[@]}" "$@"
]=])
    string(CONFIGURE "${FUZZ_LINK_WRAPPER_CONTENT}"
           FUZZ_LINK_WRAPPER_CONTENT @ONLY)
    file(WRITE "${FUZZ_LINK_WRAPPER}" "${FUZZ_LINK_WRAPPER_CONTENT}")
    file(CHMOD "${FUZZ_LINK_WRAPPER}" PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)
    set(FUZZ_LINK_CMD
        "${CLANGXX}"
        "-fsanitize=fuzzer,address,undefined"
        "${FUZZ_OBJ_SCRIPT_FUZZ}"
        "-o" "${FUZZ_BIN}"
        "${FUZZ_OBJ_SCRIPT}"
        "${FUZZ_OBJ_FUZZ_STUBS}"
        "${CMAKE_BINARY_DIR}/lib/libhash9_crypto.a"
        "${CMAKE_BINARY_DIR}/lib/libleveldb_memenv.a"
        "${CMAKE_BINARY_DIR}/lib/libleveldb_lib.a"
        "-lssl" "-lcrypto" "-ldb_cxx" "-levent" "-lsqlite3" "-lminiupnpc"
        "${CMAKE_BINARY_DIR}/lib/libsecp256k1.a"
        # RocksDB: build-rocksdb.sh installs librocksdb.so.8.9.1 to
        # /usr/local (CI) or the user has it via the distro package
        # (DNS2 has librocksdb-dev). The library search path picks up
        # either /usr/local/lib or /usr/lib automatically, so a bare
        # "-lrocksdb" works on both. The previous generator expression
        # ($<IF:$<TARGET_EXISTS:RocksDB::rocksdb>,-lrocksdb,${ROCKSDB_LIBRARY}>)
        # failed on CI because:
        #   1. CMake's find_package(RocksDB CONFIG) does NOT find the .cmake
        #      config RocksDB 8.9.1 ships, only the .pc file.
        #   2. The pkg-config path exposes PkgConfig::RocksDB (NOT
        #      RocksDB::rocksdb), so $<TARGET_EXISTS:RocksDB::rocksdb> is
        #      FALSE.
        #   3. The fallback ${ROCKSDB_LIBRARY} is only set inside the manual
        #      find_library() probe at CMakeLists.txt:170-190, which is
        #      skipped when EITHER target exists.
        # Result on CI: an empty string landed in the link line, and the
        # fuzz binary linked against every RocksDB symbol it referenced
        # turned into "undefined reference" errors.
        "-lrocksdb"
        "-lz" "-lgflags" "-lsnappy" "-lbz2" "-llz4" "-lzstd"
        # i2p is inlined into triangles_common as i2p_embedded.cpp.o and is a
        # NO-OP when USE_I2P_EMBEDDED=OFF (which is the CI default; the
        # workflow only builds libtor, not libi2pd). Do NOT link any
        # src/i2p/i2pd-src/lib*.a here — those files are produced by a
        # separate `make` step in src/i2p/build-libi2pd.sh that the fuzz
        # job does NOT run, and clang aborts the link with
        # "no such file or directory" when they're absent.
        "${CMAKE_CURRENT_SOURCE_DIR}/tor/tor-src/libtor.a"
        "-lpthread" "-llzma" "-lubsan"
    )
    # Boost target names need real paths on the link line; generator
    # expressions don't get evaluated by the bash wrapper, so resolve
    # the imported-target paths at configure time and append them.
    foreach(_target Boost::program_options Boost::thread Boost::chrono
                     Boost::atomic Boost::filesystem Boost::system)
        if(TARGET "${_target}")
            get_target_property(_path "${_target}" IMPORTED_LOCATION_RELEASE)
            if(NOT _path)
                get_target_property(_path "${_target}" IMPORTED_LOCATION)
            endif()
            if(_path AND EXISTS "${_path}")
                list(APPEND FUZZ_LINK_CMD "${_path}")
            endif()
        endif()
    endforeach()
    # Invoke the link wrapper script, passing the actual link line as
    # args. The wrapper script discovers .o files at link time via find
    # (file(GLOB) would evaluate empty at configure time when no .o files
    # exist yet on a fresh build dir) and exec's clang++ with all the
    # discovered objects prepended to its arg list.
    add_custom_command(
        OUTPUT "${FUZZ_BIN}"
        COMMAND "${FUZZ_LINK_WRAPPER}" ${FUZZ_LINK_CMD}
        DEPENDS
            "${FUZZ_OBJ_SCRIPT_FUZZ}"
            "${FUZZ_OBJ_SCRIPT}"
            "${FUZZ_OBJ_FUZZ_STUBS}"
            "${FUZZ_LINK_WRAPPER}"
            # Static libs the link line references at ${CMAKE_BINARY_DIR}/lib/.
            # Without these deps, fuzz_script's link step races and fails with
            # "no such file" errors on first clean build.
            hash9_crypto
            leveldb_lib
            leveldb_memenv
            secp256k1
            # trianglesd_objects emits the daemon .o files (noui/init/wallet)
            # that the link wrapper discovers via find. triangles_common emits
            # the rest of the .o files we need. Without these deps the wrapper
            # finds no .o files on first build → undefined references like
            # CKey::GetPubKey.
            trianglesd_objects
            triangles_common
        COMMENT "[fuzz] clang++ link fuzz_script"
    )
    add_custom_target(fuzz_script ALL DEPENDS "${FUZZ_BIN}")

    # ==========================================================================
    # transaction_deserialize_fuzz — second fuzz target
    # ==========================================================================
    # Compile the harness with clang + libFuzzer instrumentation. The harness
    # only links against the already-instrumented triangles_common /
    # trianglesd .o files (for CTransaction, CDataStream, etc.) — we do NOT
    # compile a separate clang-instrumented copy of any .cpp file the way
    # fuzz_script does for script.cpp.
    #
    # Uses its OWN link wrapper (link_txdeser.sh) because the fuzz_script
    # wrapper excludes script.cpp.o from triangles_common (we replace it
    # with our own clang-instrumented copy there). For transaction_deserialize
    # we need script.cpp.o: wallet.cpp.o (in trianglesd_objects) calls
    # ExtractDestination, SignSignature, Solver, IsMine — all defined in
    # script.cpp.o. Excluding it produces "undefined reference" link errors.
    # The new wrapper excludes only init.cpp.o (which defines daemon main()
    # and would conflict with libFuzzer's main).
    add_custom_command(
        OUTPUT "${FUZZ_TX_DESER_OBJ}"
        COMMAND ${CLANGXX} ${FUZZ_COMMON_FLAGS}
                -c ${FUZZ_TX_DESER_SRC} -o ${FUZZ_TX_DESER_OBJ}
        DEPENDS ${FUZZ_TX_DESER_SRC}
        COMMENT "[fuzz] clang++ transaction_deserialize_fuzz.cpp"
        VERBATIM
    )

    # Link command — same library set as fuzz_script, but no
    # ${FUZZ_OBJ_SCRIPT} or ${FUZZ_OBJ_SCRIPT_FUZZ} (we didn't compile
    # our own clang-instrumented copy). The wrapper script discovers
    # .o files via find at link time.
    set(FUZZ_TX_DESER_LINK_CMD
        "${CLANGXX}"
        "-fsanitize=fuzzer,address,undefined"
        "${FUZZ_TX_DESER_OBJ}"
        "-o" "${FUZZ_TX_DESER_BIN}"
        "${FUZZ_OBJ_FUZZ_STUBS}"
        "${CMAKE_BINARY_DIR}/lib/libhash9_crypto.a"
        "${CMAKE_BINARY_DIR}/lib/libleveldb_memenv.a"
        "${CMAKE_BINARY_DIR}/lib/libleveldb_lib.a"
        "-lssl" "-lcrypto" "-ldb_cxx" "-levent" "-lsqlite3" "-lminiupnpc"
        "${CMAKE_BINARY_DIR}/lib/libsecp256k1.a"
        "-lrocksdb"
        "-lz" "-lgflags" "-lsnappy" "-lbz2" "-llz4" "-lzstd"
        "${CMAKE_CURRENT_SOURCE_DIR}/tor/tor-src/libtor.a"
        "-lpthread" "-llzma" "-lubsan"
    )
    foreach(_target Boost::program_options Boost::thread Boost::chrono
                     Boost::atomic Boost::filesystem Boost::system)
        if(TARGET "${_target}")
            get_target_property(_path "${_target}" IMPORTED_LOCATION_RELEASE)
            if(NOT _path)
                get_target_property(_path "${_target}" IMPORTED_LOCATION)
            endif()
            if(_path AND EXISTS "${_path}")
                list(APPEND FUZZ_TX_DESER_LINK_CMD "${_path}")
            endif()
        endif()
    endforeach()

    add_custom_command(
        OUTPUT "${FUZZ_TX_DESER_BIN}"
        COMMAND "${FUZZ_TX_DESER_LINK_WRAPPER}" ${FUZZ_TX_DESER_LINK_CMD}
        DEPENDS
            "${FUZZ_TX_DESER_OBJ}"
            "${FUZZ_OBJ_FUZZ_STUBS}"
            "${FUZZ_TX_DESER_LINK_WRAPPER}"
            hash9_crypto
            leveldb_lib
            leveldb_memenv
            secp256k1
            trianglesd_objects
            triangles_common
        COMMENT "[fuzz] clang++ link transaction_deserialize_fuzz"
    )
    add_custom_target(transaction_deserialize_fuzz ALL
                      DEPENDS "${FUZZ_TX_DESER_BIN}")

    message(STATUS "Fuzz targets enabled:")
    message(STATUS "  ${FUZZ_BIN}")
    message(STATUS "  ${FUZZ_TX_DESER_BIN}")
endif()
