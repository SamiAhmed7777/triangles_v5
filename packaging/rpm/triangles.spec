Name:           triangles
Version:        6.2.0
Release:        1%{?dist}
Summary:        Cryptographic Triangles (TRI) cryptocurrency wallet
License:        MIT
URL:            https://cryptographic-triangles.org
Source0:        https://github.com/SamiAhmed7777/triangles_v5/releases/download/v%{version}/Cryptographic-Triangles-v%{version}-linux-x64-qt
Source1:        https://github.com/SamiAhmed7777/triangles_v5/releases/download/v%{version}/Cryptographic-Triangles-v%{version}-linux-x64-daemon
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

%changelog
* Wed Jul 08 2026 Sami Ahmed <sami@cryptographic-triangles.org> - 6.1.7-1
- 6.1.7 release. UI: Overview Total label font-weight bumped from 75
  to 900 so the Total actually reads as bold against Spendable/Stake.
  Transactions amount column Confirming-tier color changed from pale
  mint (#C5EBC9) to mid green (#4A8C5E) so it reads as visibly
  different from the bright Confirmed green. Both paint sites now
  read confirmation depth via a new DepthRole on the table model
  instead of the status enum, so the color fires on every block
  increment.

* Wed Jul 08 2026 Sami Ahmed <sami@cryptographic-triangles.org> - 6.1.6-1
- 6.1.6 release. UI: Overview Total label now conditional (green when
  total > 0, red when empty), Transactions amount column now 3-tier
  (grey / pale mint / money-green) by confirmation depth. Plus
  sigcache entry-size fix and Polish CI/build fixes.

* Wed Jul 08 2026 Sami Ahmed <sami@cryptographic-triangles.org> - 6.1.5-1
- 6.1.5 release. UI: olive-green for unconfirmed/immature stake balances.
  Wallet: close-hang on Windows from detached Tor/I2P threads fixed.
  Consensus: live PoS checks during stale-tip IBD. Plus release
  infrastructure (reproducible builds, signed release pipeline) and
  audit follow-ups.

* Sat Jul 04 2026 Sami Ahmed <sami@cryptographic-triangles.org> - 6.1.4-1
- 6.1.4 release. CI: Tor bundle download resilience. NeedsBootstrap
  flag now correctly persists across rocksdb/ restarts. CI reliability
  only; no protocol/wallet/chain format changes.

* Thu Jul 02 2026 Sami Ahmed <sami@cryptographic-triangles.org> - 6.1.3-1
- 6.1.3 release. Chain-DB migration hardening, BIP39 passphrase
  support, HD-wallet indicator, test isolation improvements.
  Supersedes the broken v6.1.2 hotfix.

* Wed Jul 01 2026 Sami Ahmed <sami@cryptographic-triangles.org> - 6.1.1-1
- 6.1.1 release. v3 snapshot support, portable x86-64-v2 baseline,
  anti-spam fix, continuous finality checkpoints.

* Tue Jun 30 2026 Sami Ahmed <sami@cryptographic-triangles.org> - 6.1.0-1
- 6.1.0 release. SQLite wallet backend, RocksDB default, Boost
  removal, I2P startup fix. Initial 6.x line with C++20 modernization
  and embedded Tor/I2P support.

* Tue Mar 24 2026 Sami Ahmed <sami@cryptographic-triangles.org> - 5.3.7-1
- 5.3.7 release.
