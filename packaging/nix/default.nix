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
  version = "5.5.5";

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
      url = "https://github.com/SamiAhmed7777/triangles_v5/releases/download/v${version}/Cryptographic-Triangles-v${version}-linux-x64-qt";
      sha256 = "ed220eb8d0b403f62cdac28988541fd1a27864491e233216f9c00a4c2537b4a3";
      name = "triangles-qt-linux";
    })
    (fetchurl {
      url = "https://github.com/SamiAhmed7777/triangles_v5/releases/download/v${version}/Cryptographic-Triangles-v${version}-linux-x64-daemon";
      sha256 = "4d2ab25d61127d6aff3e6f3069556d04f4b823f8849e97629c12871ad4779517";
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
