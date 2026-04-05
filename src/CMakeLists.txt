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
    alert.cpp
    addrman.cpp
    bootstrap.cpp
    checkpoints.cpp
    crypter.cpp
    db.cpp
    irc.cpp
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
    txdb-leveldb.cpp
    lz4/lz4.c
    tor/onion_v3.cpp
    tor/tor_process.cpp
    tor/tor_embedded.cpp
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

add_library(triangles_common OBJECT ${CORE_SOURCES})

target_include_directories(triangles_common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/json"
    "${CMAKE_CURRENT_SOURCE_DIR}/tor"
    "${CMAKE_BINARY_DIR}/generated"       # for build.h
)

target_compile_definitions(triangles_common PUBLIC HAVE_BUILD_INFO)

target_link_libraries(triangles_common PUBLIC
    hash9_crypto
    json_compat
    leveldb_bundled
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::filesystem
    Boost::program_options
    Boost::thread
    Boost::chrono
    BerkeleyDB::BerkeleyDB
    Libevent::Libevent
    ZLIB::ZLIB
    Threads::Threads
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

# Optional: Embedded Tor
if(USE_TOR_EMBEDDED)
    if(TOR_SOURCE_ROOT STREQUAL "")
        set(TOR_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/tor/tor-src")
    endif()
    target_compile_definitions(triangles_common PUBLIC ENABLE_TOR_EMBEDDED)
    target_include_directories(triangles_common PUBLIC "${TOR_SOURCE_ROOT}/src/feature/api")
    target_link_directories(triangles_common PUBLIC "${TOR_SOURCE_ROOT}")
    if(UNIX)
        target_link_libraries(triangles_common PUBLIC -Wl,--start-group -ltor -Wl,--end-group lzma zstd)
    else()
        target_link_libraries(triangles_common PUBLIC tor lzma zstd)
    endif()
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

# ═══════════════════════════════════════════════════════════════════════════════
# 4. Headless daemon (trianglesd)
# ═══════════════════════════════════════════════════════════════════════════════
if(BUILD_DAEMON)
    add_executable(trianglesd
        noui.cpp
        init.cpp
        wallet.cpp
    )
    # No QT_GUI define — daemon gets the #if !defined(QT_GUI) code paths
    target_link_libraries(trianglesd PRIVATE triangles_common)

    if(WIN32)
        set_target_properties(trianglesd PROPERTIES SUFFIX ".exe")
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
    )

    target_link_libraries(triangles-qt PRIVATE
        triangles_common
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
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

    add_executable(test_triangles
        ${TEST_SOURCES}
        # Per-target: wallet without QT_GUI, noui for noui_connect()
        wallet.cpp
        noui.cpp
    )
    # No init.cpp — test_triangles.cpp provides its own StartShutdown() stub

    target_compile_definitions(test_triangles PRIVATE
        "TEST_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/test/data\""
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
endif()
