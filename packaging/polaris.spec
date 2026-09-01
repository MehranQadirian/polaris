Name: polaris
Version: 0.1.0
Release: 1%{?dist}
Summary: Linux Performance & System Health Platform (Fedora+KDE)
License: MIT
# SPDX-License-Identifier: MIT (see LICENSE, docs/VERSIONING.md, README.md License section)
# Previous GPL-3.0-or-later was packaging placeholder; MIT chosen for P19 final release as permissive, compatible with OpenSSL Apache-2.0 + LGPL sdbus-c++ + MIT libdrm - see LICENSE
BuildRequires: gcc-c++ cmake ninja-build
Requires: polkit
%description
Polaris - evidence-driven diagnostics, benchmarking, safe optimization. API-first, Polkit, transaction+rollback.
%prep
%autosetup
%build
%cmake -GNinja
%cmake_build
%install
%cmake_install
%files
%{_bindir}/polaris
%{_datadir}/polkit-1/actions/org.polaris.*.policy
%doc README.md ARCHITECTURE.md API.md
