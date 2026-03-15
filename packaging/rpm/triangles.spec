Name:           triangles
Version:        5.1.4
Release:        1%{?dist}
Summary:        Cryptographic Triangles (TRI) cryptocurrency wallet
License:        MIT
URL:            https://cryptographic-triangles.org
Source0:        https://github.com/SamiAhmed7777/triangles_v5/releases/download/v%{version}/triangles-qt-linux
Source1:        https://github.com/SamiAhmed7777/triangles_v5/releases/download/v%{version}/trianglesd-linux
Source2:        triangles-qt.desktop

BuildArch:      x86_64
ExclusiveArch:  x86_64

Requires:       qt5-qtbase
Requires:       qt5-qtbase-gui
Requires:       openssl-libs
Requires:       boost-system
Requires:       boost-filesystem
Requires:       boost-program-options
Requires:       boost-thread
Requires:       boost-chrono
Requires:       libevent
Requires:       miniupnpc-libs
Requires:       libdb-cxx
Recommends:     tor

%description
Privacy-focused cryptocurrency featuring Proof-of-Stake consensus
with 33% annual staking rewards, Tor v3 onion routing, and built-in
encrypted peer-to-peer messaging. Originally launched in July 2014,
featuring the unique Hash9 algorithm (13-step hash cascade).

This package contains the Qt GUI wallet (triangles-qt) and the
headless daemon (trianglesd).

%install
install -Dm755 %{SOURCE0} %{buildroot}%{_bindir}/triangles-qt
install -Dm755 %{SOURCE1} %{buildroot}%{_bindir}/trianglesd
install -Dm644 %{SOURCE2} %{buildroot}%{_datadir}/applications/triangles-qt.desktop

%files
%{_bindir}/triangles-qt
%{_bindir}/trianglesd
%{_datadir}/applications/triangles-qt.desktop
