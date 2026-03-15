{ lib
, stdenv
, fetchurl
, autoPatchelfHook
, qt5
, openssl
, boost
, db
, leveldb
, libevent
, miniupnpc
, makeWrapper
, makeDesktopItem
}:

let
  version = "5.1.4";

  desktopItem = makeDesktopItem {
    name = "triangles-qt";
    desktopName = "Cryptographic Triangles";
    genericName = "TRI Wallet";
    comment = "Cryptographic Triangles cryptocurrency wallet";
    exec = "triangles-qt %u";
    icon = "triangles";
    categories = [ "Finance" "Network" "P2P" "Qt" ];
    mimeTypes = [ "x-scheme-handler/triangles" ];
    startupNotify = true;
  };
in
stdenv.mkDerivation {
  pname = "triangles";
  inherit version;

  srcs = [
    (fetchurl {
      url = "https://github.com/SamiAhmed7777/triangles_v5/releases/download/v${version}/triangles-qt-linux";
      sha256 = "19eaadfdf18b899ce8434fe714e690e2db0546597e36037de37a29854fc23aeb";
      name = "triangles-qt-linux";
    })
    (fetchurl {
      url = "https://github.com/SamiAhmed7777/triangles_v5/releases/download/v${version}/trianglesd-linux";
      sha256 = "6f5c19d34a2e1f6cdadee095d9e11b25d18b41a0d1602a163ffca7ec80b3da37";
      name = "trianglesd-linux";
    })
  ];

  sourceRoot = ".";
  dontUnpack = true;

  nativeBuildInputs = [
    autoPatchelfHook
    makeWrapper
    qt5.wrapQtAppsHook
  ];

  buildInputs = [
    qt5.qtbase
    openssl
    boost
    db
    leveldb
    libevent
    miniupnpc
  ];

  installPhase = ''
    runHook preInstall

    install -Dm755 ${builtins.elemAt srcs 0} $out/bin/triangles-qt
    install -Dm755 ${builtins.elemAt srcs 1} $out/bin/trianglesd
    install -Dm644 ${desktopItem}/share/applications/* $out/share/applications/

    runHook postInstall
  '';

  meta = with lib; {
    description = "Cryptographic Triangles (TRI) cryptocurrency wallet";
    longDescription = ''
      Privacy-focused cryptocurrency featuring Proof-of-Stake consensus
      with 33% annual staking rewards, Tor v3 onion routing, and built-in
      encrypted peer-to-peer messaging. Originally launched in July 2014,
      featuring the unique Hash9 algorithm (13-step hash cascade).
    '';
    homepage = "https://cryptographic-triangles.org";
    license = licenses.mit;
    platforms = [ "x86_64-linux" ];
    mainProgram = "triangles-qt";
  };
}
