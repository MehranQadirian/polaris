# Changelog - Polaris

All notable changes to Polaris from `P1` Architecture to `P18` `PROJECT_COMPLETE_WITH_LIMITATIONS` `33/33` tests. Dates are `+0330` `Asia/Tehran` `Fedora 44` `7.1.10-200`.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) and [Semantic Versioning](https://semver.org/spec/v2.0.0.html) (`MAJOR.MINOR.PATCH` `docs/VERSIONING.md` `0.1.0` `CMakeLists.txt:2`).

## [0.1.0] - 2026-09-01 - P18 `PROJECT_COMPLETE_WITH_LIMITATIONS` `P1-P18` `33/33` `8.515s` `5.6Gi` `NVIDIA 470.256.02` `CLAIMED` `PRIME` `akonadi` `running` `mssql` `disabled`

**Status:** `P18` `COMPLETED` `PROJECT_COMPLETE_WITH_LIMITATIONS` `33/33` `0.70s` `100%` `cmake -S . -B build --fresh && cmake --build && ctest` `P18` `FINAL_REPORT.md` `52K` `P18_FINAL_STATE.json` `27K` `verdict` `PROJECT_COMPLETE_WITH_LIMITATIONS` `currentHostState` `baselineComparison` `completedTransactions` `rejectedCandidates` `regressions` `safetyAssessment` `tests` `knownLimitations` `recommendation: STOP` `no P19` justified.

### Added
- `P11` `core/domain/Comparison.h` `Comparison` `MetricComparison` `Verdict` `SUCCESS`/`REGRESSION` `isDeterministic` `core/engines/comparison/ComparisonEngine.h/.cpp` pure `compare` `before`/`after` `expectedBenefit` `observedBenefit` `hasRegression` thresholds `boot +10%` `available -1GB` `thermal +15C` `new_failed`, `Transaction` `beforeBaseline`/`afterBaseline`/`comparison` (`P11` 12 cats `test_comparison` `P7 fixture` `zero-before` `threshold` `expected` vs `observed` `serialization` `P7 fixture`)
- `P12` `core/safety/transaction/Transaction.h` `beforeHash`/`approvedBeforeHash` 15 fields `TransactionValidator` pure stale `TOCTOU` `TransactionStore` duplicate `ALREADY_EXISTS` idempotent `COMPLETED→already_completed` `StateMachine` `PREVIEWED→FAILED` `AuditLog` `fsync` `13/13` (`p12_stale` `TOCTOU` `p12_idempotency` `p12_statemachine` `p12_transaction_model`)
- `P13` `core/profile/UserProfile.h` `TriState` `UNKNOWN/YES/NO` 8 fields + `extra` `ProfileStore` `~/.local/state/polaris/profile.json` `0600` `ProfileService` explicit `updateField` no inference `ProfileAdvisor` `BLOCKED`/`REQUIRES`/`ALLOWED` `17/17` (`p13_profile_model` `p13_profile_store` `p13_profile_service` `p13_profile_advisor`)
- `P14` `core/ipc/IpcProtocol` `PROTOCOL_VERSION=1` `MAX_REQUEST_SIZE=64KB` `allowedOperations` `ping`/`info` only `NO PRIVILEGED MUTATION`, `IpcAuth` `SO_PEERCRED` same-user `ucred` `pid/uid/gid` from kernel not client, `IpcServer` `0600` `0700` `FD_CLOEXEC` `poll` 5s, `TransactionLock` `flock` `LOCK_EX|LOCK_NB` `0600`, `RecoveryDetector` `BACKUP_CREATED`→`incomplete` `suggested FAILED` never `COMPLETED` `24/24` (`p14_ipc_protocol` 12 `p14_ipc_auth` 5 `p14_socket_security` 5 `p14_ipc_server` 4 `p14_lock` 5 `p14_ipc_security` 12 `p14_recovery` 4)
- `P15` `TransactionValidator` fix `UNAVAILABLE`→fail-closed `kernel`/`package`/`beforeHash` empty where `approved*` non-empty now correctly rejected, 5 table-driven `P15` suites `test_p15_lifecycle` `test_p15_stale_matrix` `test_p15_toctou_idempotency` `test_p15_lock_recovery` `test_p15_regression_audit` + `.github/workflows/ci.yml` `cmake --fresh` `ctest` `29/29`
- `P16` `core/explainability/Explanation.h` 22 fields deterministic sorted JSON `toHuman` redacted `WHY NOW` evidence-backed `WHAT WILL CHANGE` transaction-backed `WHAT WILL NOT CHANGE` explicit `rejectionConditions` deterministic `profile`/`comparison` integration `never confuses approval with authorization`, `ExplanationEngine` `explainCandidate`/`explainTransaction`, `cli/p4_cli.cpp` `explain <candidate>` and `transaction explain <id>` `--json`/`--verbose` `audit` `explanation.generated`, `33/33` (`p16_explanation_model` 6 `p16_explain_candidate` 7 `p16_explain_transaction` 8 `p16_verbose_redaction` 8)
- `P17` read-only `Campaign 2` `systemd-analyze` `8.515s` `akonadi` `BLOCKED` `bluetooth` `2 paired` `REQUIRES` `5-10M` tiny → `NO_ACTION_RECOMMENDED` exactly ONE `bluetooth-disable` considered but negligible, no `PREVIEWED` transaction `audit` `explanation.generated` (`P17` `docs/P17_REPORT.md`)
- `P18` `docs/P18_FINAL_REPORT.md` `52K` `docs/P18_FINAL_STATE.json` `27K` `systemd-analyze` `8.515s` `free` `5.6Gi` `zram` `0B` `lspci` `CLAIMED` `driver=nvidia` `modinfo` `470.256.02` `nvidia-smi` `470.256.02` `glxinfo` `Mesa Intel` `PRIME` `NVIDIA GeForce MX130` `akonadictl` `running` 14 agents `mssql` `disabled` `fstab` 3 entries `stat` `2026-08-31 21:19` `Comparison` `P3` `54.106s`→`8.515s` `-84%` not `+10%` `Transaction ROI` `TX-P6` `TX-P7` `Campaign 2` `NO_ACTION` `Safety` `READ→...→AUDIT` `33/33` `PROJECT_COMPLETE_WITH_LIMITATIONS`

### Changed
- `P6` `mssql-server.service` `enabled`→`disabled` `Removed /etc/systemd/system/multi-user.target.wants/mssql-server.service` `systemctl --failed` `1` (`mssql`) → `0` after `P7` reboot `0` `failed` (now `1` `drkonqi` unrelated `2026-09-01 03:25`), `713M` `9.192s` saved per boot `0.92` `UNUSED` `75/345 Failed 0 ready` `localhost:1433` 0 hits `DB_CONNECTION=mysql` (1 real host mutation)
- `P7` `GM108M [GeForce MX130] [10de:174d]` `UNCLAIMED` `610.57.04` open `GSP` `probe error -1` `490` → `CLAIMED` `driver=nvidia` `470.256.02` `extra/nvidia-470xx` `25M` `nvidia-smi` `470.256.02` `PRIME` `NVIDIA GeForce MX130` `journal NVRM` `490→1` (1 real host mutation + `reboot 00:36` `7.1.10-200` still, `SecureBoot disabled` `initramfs` `dracut --force` `156M`)

### Fixed
- `P2` stale swap `UUID 39b0b8c8-58b6-4136-ad6a-7c3b1cf1f45d none swap` `sw 0 0` commented `DISABLED 2026-08-31 stale swap - system uses zram, device not found, was causing 90s timeout` `cat /etc/fstab` 3 entries `findmnt --verify` `0 parse errors` `stat` `2026-08-31 21:19` unchanged since (read-only fix, no `P5` host mutation for `Hidden=true` already correct `101` `sha 4ad53409` `ALREADY_APPLIED / NO_OP`)
- `P7` `nvidia` `UNCLAIMED` `probe failed` `490` → `CLAIMED` `1` loading `470.256.02` `unsupported 0`
- `P15` `TransactionValidator` `UNAVAILABLE` (`kernel`/`package`/`beforeHash` empty where `approved*` non-empty → `unverifiable_*` fail-closed, previously passed)

### Security
- `P4` `StateMachine` 16 states `isValidTransition` fail-closed, `FileSafety` allowlist `validatePath` `..;|&` `` ` `` `$` `NUL` `>4096` `isSymlink`, `BackupEngine` `SHA-256` `is_regular_file` `fsync` no overwrite, `AuditLog` `hashEvent` `SHA256` `previousHash` chain `fsync`, `polkit` `auth_admin_keep` `org.polaris.*.policy` 3 actions, `ReadOnlyGuard` `kReadOnlyMode true`
- `P12` `Transaction` `beforeHash`/`approvedBeforeHash` + `TransactionValidator` pure stale `TOCTOU` + `TransactionStore` idempotent `BackupEngine` `P4` + `StateMachine` `PREVIEWED→FAILED` + `AuditLog` `fsync`
- `P14` `IpcProtocol` `ping`/`info` only `PROTOCOL_VERSION=1` `MAX_REQUEST_SIZE=64KB` `validate` `NUL`/`shell`/`traversal`/`oversized`/`password` `allowedOperations` `ping`/`info` only `NO PRIVILEGED MUTATION`, `IpcAuth` `SO_PEERCRED` same-user `ucred` `pid/uid/gid` from kernel not client, `IpcServer` `0600` `0700` `FD_CLOEXEC` `poll` 5s, `TransactionLock` `flock` `LOCK_EX|LOCK_NB` `0600`, `RecoveryDetector` `BACKUP_CREATED`→`incomplete` `suggested FAILED` never `COMPLETED`
- `P19` `SECRET_AUDIT: PASS` after `docs/P7_PRE_REBOOT_REPORT.md` `echo "****" | sudo -S` literal `[REDACTED]` redacted `sudo` `password redacted [REDACTED]` before first commit, `grep -R "[REDACTED]" ~/Documents/lin-opt` `0` hits after (literal not printed), `grep -R "gho_" ~/Documents/lin-opt` `0` hits (token in `~/.config/gh/hosts.yml` outside `~/Documents/lin-opt`)

---

## [0.1.0] - 2026-08-31 - P1 Architecture / MVP Read-Only Scaffold

- `P1` `README.md` `ARCHITECTURE.md` `API.md` `SECURITY.md` `HCI.md` `THREAT_MODEL.md` `CMakeLists.txt` `C++20` `CMAKE_CXX_STANDARD 20` `CMAKE_CXX_STANDARD_REQUIRED ON` `C++` `20` `project(polaris VERSION 0.1.0)` `core/` `11 dirs` `polkit/` 3 actions
- `P2` `Real*Provider` `proc` `sys` `D-Bus` `libudev` `glxinfo` `/usr/bin/glxinfo` `DISPLAY=:0` `hwmon` `systemctl` `execv` `poll` `RealKdeProvider` `kwinrc` `ReadOnlyGuard` `kReadOnlyMode true` `4/4` `test_real_providers` `test_parsers` `test_readonly` `p2_scan.json` `9.9K` `Vendor Intel` `NVIDIA 10de:174d UNCLAIMED` `p2_human.txt` `5.4K` `docs/P2_REPORT.md` `24K`
- `P3` `PerfModels.h` `PerformanceBaseline` 15 metrics `BaselineEngine` `3812ms` `BottleneckEngine` 10 `BenchmarkEngine` `quick` `prime` `0.043ms` `RecommendationEngine` 7 `cli/p3_cli.cpp` `p3_analysis.json` `23K` `userspace 54.106s` `failed 1` `NVRM 490` `avail 4.2GB`
- `P4` `Transaction` `ChangePreview` `polkit` `3 actions` `polaris_p4` `transaction preview` on `/tmp/polaris-test-root` `5/5` `test_p4_security` 9 checks `path traversal` `symlink` `metachars` `invalid transition` `replay` `backup no overwrite` `oversized` `fake op` `audit hash chain`
- `P5` pilot `~/.config/autostart/nvidia-settings-user.desktop` `Hidden=true` already `101` `sha 4ad53409` → `ALREADY_APPLIED / NO_OP` (no host mutation)
- `P2` stale swap fix `fstab` `# UUID 39b0...` commented `2026-08-31 21:19` (read-only `stat` `21:19` unchanged since, verified `test_readonly`)

[Unreleased] - No `P19` justified after `P18` `PROJECT_COMPLETE_WITH_LIMITATIONS` `33/33` `no worthwhile` `no regression` `no P19` `P18` `FINAL_REPORT.md` `P18` `COMPLETE`.

