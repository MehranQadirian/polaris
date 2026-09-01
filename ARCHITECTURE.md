# Architecture - Polaris

Full design as approved 2026-08-31. API-first, headless, GUI-ready, safety-gated.

## 1. High-Level

Presentation (CLI, Qt Widgets, Qt Quick) → API (UDS /api/v1) → Application Orchestrator (Job System) → Engines (Diagnostics, Benchmark, Analysis, Optimize) → Safety/Transaction (Validation, Compat, Risk, Backup, Auth, Apply, Verify, Compare, Rollback, Audit) → Linux HAL (Providers).

Core library `libpolaris_core` has zero Qt dependency.

## 2. Module Map

See README layout.

## 3. Domain Model

Versioned JSON schema in `core/domain/*.h` (C++20 structs + nlohmann-json). Key: SystemInfo, HardwareInfo, CpuInfo (intel_pstate, governor, EPP, boost), MemoryInfo (zram, pressure), StorageDevice (NVMe/SATA, SMART, TRIM), GpuInfo (vendor/model/pciId/driver/module/claimed/glRenderer/vulkan/power), BootAnalysis (blame/critical-chain), HealthIssue (id/cat/sev/evidence/confidence/impact/reco/risk/rollback), PerformanceMetric (value/unit/ts/source/confidence), Optimization (id/cat/problem/evidence/benefit/risk/compat/actions/validation/rollback/reboot/auth), Transaction (id, state PLANNED..ROLLED_BACK, changes, backup, audit).

## 4. API

UDS `/run/polaris/polaris.sock` 0600, optional `127.0.0.1:11447` token. See `API.md`.

## 5. Safety

11-step gate before every mutate: detect → validate prereq → compat → conflicts → risk → reversibility → backup → tx → approve → auth → apply → verify → benchmark → compare → keep/rollback. Backup targeted file only, checksum, restore. Dry-run same path stops before BACKING_UP.

## 6. Quest

Progressive disclosure, explainability, user control. See `HCI.md`.

## 7. Build

C++20, CMake, clang-format, sanitizers, SQLite for persistence (not secrets), sdbus-c++ for systemd/D-Bus, libdnf5 read-only.

## 8. Phases

P1 Arch (now) → P2 Read-Only → P3 Perf → P4 Safety → P5 L1 → P6 L2 → P7 L3 → P8 Hardening → P9 RPM → P10 GUI.
