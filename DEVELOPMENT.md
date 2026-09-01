# Development - Polaris

## Setup (Fedora 44)
```bash
dnf install gcc-c++ cmake ninja-build nlohmann-json-devel sdbus-c++-devel libdrm-devel lm_sensors-devel sqlite-devel
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

No sudo needed for read-only MVP.

## Phases
P1 Arch (now) → P2 Read-Only → P3 Perf → P4 Safety → P5 L1 → P6 L2 → P7 L3 → P8 Hardening → P9 RPM → P10 GUI.

## Coding
C++20, clang-format, sanitizers, no global mutable state, providers as interfaces for mocks.
