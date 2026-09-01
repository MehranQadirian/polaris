# Project Handoff - Polaris

**Generated:** 2026-09-01 08:00 +0330  
**Source:** Actual repository state `~/Documents/lin-opt`, verified via `cmake`, `ctest`, `ls`, `cat`, `systemctl`, `lspci`, `modinfo`, `nvidia-smi` - not conversation memory.  
**Next Session Entry:** Read `docs/PROJECT_HANDOFF.md` + `docs/PROJECT_STATE.json`, then await `P19` - NONE (PROJECT_COMPLETE_WITH_LIMITATIONS, no P19 justified).

---

## 1. Project Identity

- **Project name:** Polaris - Linux Performance & System Health Platform, Codename POLARIS
- **Repository path:** `~/Documents/lin-opt` (`/home/mehrangh/Documents/lin-opt`)
- **Current branch:** `not a git repository` (`git -C` → `fatal: not a git repository`)
- **Current commit:** `unknown` (no git)
- **Build system:** `CMake 3.28` `Ninja`/`Make` (via `cmake -S . -B /tmp/polaris_build_handoff`)
- **C++ standard:** `20` (`set(CMAKE_CXX_STANDARD 20)` `CMAKE_CXX_STANDARD_REQUIRED ON`)
- **Current project version/state:** `project(polaris VERSION 0.1.0 LANGUAGES CXX)` `Status: P18 COMPLETED (PROJECT_COMPLETE_WITH_LIMITATIONS)` (see `docs/ROADMAP.md`)

---

## 2. Completed Phases P1-P13 (Verified via Repository Evidence)

| Phase | Name | Status | Objective | What Was Actually Implemented | Important Files | Tests | Verification Result | Real-Host Modifications | Reboot | Artifacts |
|-------|------|--------|-----------|-------------------------------|-----------------|-------|---------------------|-------------------------|--------|-----------|
| P1 | Architecture | COMPLETED | Scaffold API-first, headless, GUI-ready, safety model | `README.md`, `ARCHITECTURE.md`, `API.md`, `SECURITY.md`, `HCI.md`, `THREAT_MODEL.md`, `CMakeLists.txt` `C++20` no Qt in core, `core/` skeleton, `polkit/` 3 actions | `ctest` not yet (scaffold) | `cmake -S` success, `ls -R` 11 dirs | None | No | `docs/P10_PLAN.md` later |
| P2 | Read-Only Providers | COMPLETED | Real read-only providers via native `/proc` `/sys` `sysfs` `D-Bus` `libudev`, no shell injection | `core/providers/real/RealOsProvider.h` `/proc/os-release`+`uname`, `RealCpuProvider` `/proc/cpuinfo`+`sysfs cpufreq`, `RealMemoryProvider` `/proc/meminfo`+`pressure`+`zram`, `RealStorageProvider` `/proc/mounts`+`statvfs`+`/sys/block`, `RealGpuProvider` `/sys/bus/pci`+`pci.ids`+`glxinfo` fixed `/usr/bin/glxinfo`, `RealThermalProvider` `hwmon`, `RealSystemdProvider` `systemctl`/`systemd-analyze` via `execv` `poll`, `RealKdeProvider` env+`kwinrc`, `RealProcessProvider` `/proc`, `RealJournalProvider` `journalctl`, `core/safety/ReadOnlyGuard.h` `kReadOnlyMode true` | `test_real_providers` `test_parsers` `test_readonly` (4/4) + manual `polaris_real --json` 9.9K `p2_scan.json` | `p2_scan.json` 9.9K `Vendor Intel` `NVIDIA 10de:174d UNCLAIMED`, `p2_human.txt` 5.4K, `ctest 4/4` | None (read-only, `stat /etc/fstab` mtime unchanged) | No | `p2_scan.json`, `p2_human.txt`, `docs/P2_REPORT.md` 24K |
| P3 | Performance Baseline + Bottleneck + Benchmark + Recommendation | COMPLETED | Baseline `MetricMeta`, `Bottleneck` multi-evidence, `Benchmark` quick/normal/deep cancellable, `Recommendation` with `evidence/confidence/benefit/risk` | `core/domain/PerfModels.h` `PerformanceBaseline` 15 metrics, `core/engines/perf/BaselineEngine.h` `collect()` 3812ms, `core/engines/bottleneck/BottleneckEngine` 10 bottlenecks, `core/engines/benchmark/BenchmarkEngine` `quick` `cpu_prime` 0.043ms `stddev 0.0004`, `core/engines/recommend/RecommendationEngine` 7 recs, `cli/p3_cli.cpp` `performance baseline/benchmark`, `analyze`, `bottlenecks`, `recommendations` | `ctest` still 4/4 (P3 reuses parsers) | `p3_analysis.json` 23K `userspace 54.106s` `failed 1` `NVRM 490` `avail 4.2GB`, `p3_report.txt` 15K | None (read-only) | No | `p3_analysis.json`, `p3_report.txt`, `docs/P3_REPORT.md` 33K |
| P4 | Safety + Transaction + Backup/Rollback + Polkit + Audit + Dry-Run | COMPLETED | Transaction state machine 16 states, `FileSafety` allowlist, `BackupEngine` versioned `SHA-256` no overwrite, `AuditLog` hash chaining, `StateMachine` fail-closed, `ReadOnlyGuard`, `helper` design (not installed) | `core/safety/transaction/StateMachine.h` `isValidTransition`, `core/safety/transaction/Transaction.h` `ChangePreview`, `core/safety/FileSafety.h` `validatePath` reject `..;|&\`$` `>4096` `symlink` allowlist `/tmp/polaris-test-root`, `core/safety/backup/BackupEngine.h/.cpp` `sha256File` `create`/`restore`, `core/safety/audit/AuditLog.h/.cpp` `hashEvent` `previousHash`, `polkit/org.polaris.*.policy` 3 actions `auth_admin_keep`, `cli/p4_cli.cpp` `transaction preview/list/show/approve` `audit list` `apply --dry-run` on test fixtures, `CMakeLists` `crypto` | `test_p4_security` 9 checks `path traversal`, `symlink`, `metachars`, `invalid transition`, `replay`, `backup no overwrite`, `oversized`, `fake op`, `audit hash chain` **PASS** `ctest 5/5` | `p4_security` 9/9 PASS, `polaris_p4 transaction preview dummy-test` `TX-TEST-...` `PREVIEWED` test fixture | None (test fixtures only `/tmp/polaris-test-root`, `stat /etc/fstab` 21:19 unchanged) | No | `p4_security_report.json` 2.0K, `p4_transaction_tests.json` 1.3K, `docs/P4_REPORT.md` 18K, `docs/TRANSACTION_MODEL.md`, `docs/POLKIT.md`, `docs/ROLLBACK.md`, `docs/AUDIT.md` |
| P5 | L1 Pilot (Real-Host, R1) | COMPLETED (NO_OP) | ONE real low-risk user file `~/.config/autostart/nvidia-settings-user.desktop` `Hidden=true` | `cli/p5_pilot.cpp` 9 precondition checks (`exists`, `regular`, `not symlink`, `canonical` matches, `owned 1000`, `size 101`, `contains nvidia-settings`, `Hidden`, `hash`), `Transaction` `TX-P5-20260831-001` `R1` `expectedBenefit 2.56s`, `FileSafety::atomicWrite` (temp+fsync+rename, symlink protected), `BackupEngine` (not created for NO_OP per user rejection), `AuditLog` | `test` via `p5_pilot` (precondition) | `~/.config/autostart/nvidia-settings-user.desktop` already `Hidden=true` `101` `sha 4ad53409` (from P2 Level1), user **rejected** `already Hidden=true, no diff` → `ALREADY_APPLIED / NO_OP`, verified `Hidden=true` present, `is_regular_file` true, `owned` true, `0644`, `ps aux` no `nvidia-settings`, `systemctl --user` `plasma` active, `free` `zram`, `journal` `nvidia probe` still but `nvidia-settings` error disappears next login, rollback test via `/tmp/polaris-test-p5` copy `Hidden=false`→`Hidden=true`→`Hidden=false` **PASS** | **No host modification** per user rejection (real file untouched, mtime 21:13 unchanged, test copy only) | No | `p5_transaction.json` 6.8K `status ALREADY_APPLIED / NO_OP`, `docs/P5_REPORT.md` 13K |
| P6 | L2 MSSQL Investigation + Disable (P6 Disable) | COMPLETED | Investigate `mssql-server.service` (25 failed boots, `status 18` `713M` `9.192s`, `model.mdf` Windows path), classify `PROBABLY_UNUSED` 0.68→`UNUSED` 0.92 after deep search, then `disable` | `p6_mssql_analysis.json` 16K (12 evidence items), `p6_additional_analysis.json` 7.2K (dependency graph, `localhost:1433` 0 hits, `DB_CONNECTION=mysql` for all projects, no `sql-mssql` in source, no listener 1433, `ss` 0, `ps` none, `journal` 75/345 Failed 0 ready), `docs/P6_REPORT.md` 21K + addendum | `test` via `systemctl is-enabled` `enabled` before, `is-active failed` | **Real-host: `systemctl disable mssql-server.service`** `Removed /etc/systemd/system/multi-user.target.wants/mssql-server.service` `is-enabled disabled` **PASS**, verification `is-enabled disabled`, `is-active failed` (still failed this boot, disabled for next), `systemctl --failed` still 1 until reboot (expected), `findmnt --verify` 0, `sensors` 58C, `audit` `backup.created` `apply.disable` `verification.passed` `transaction.completed` `state COMPLETED` | No reboot yet (disable takes effect next boot) | `p6_mssql_analysis.json`, `p6_additional_analysis.json`, `docs/P6_REPORT.md`, `docs/P6_EXECUTION_REPORT.md` `COMPLETED`, `~/.local/state/polaris/backups/TX-P6-...` `before_state.json` `sha 965dacd7` |
| P7 | L3 NVIDIA 470xx Migration | COMPLETED & VERIFIED | Migrate `GM108M [GeForce MX130] [10de:174d]` `10de:174d` from `610.57.04` open (requires GSP, `NVRM not supported`, `probe error -1` 490, `UNCLAIMED`) to `470.256.02` legacy (supports MX130, no GSP) | `nvidia_preflight.json` 17K + `docs/NVIDIA_PREFLIGHT_REPORT.md` 20K (25 items, package analysis `CURRENT 16` `TO REMOVE 15` `TO INSTALL 10+1` `TO KEEP 2`, Secure Boot `disabled` no MOK, `kernel-devel 7.1.10` can build), `core/safety/backup` `before_state.json` `sha ef6227ad` `rpm -qa` 16 packages, `lsmod` `nouveau`+`i915`, `glxinfo` `Mesa Intel`, `cmdline` `rd.driver.blacklist=nouveau`, `fallback` `disabled` `active (exited)`, `dnf swap --allowerasing -y akmod-nvidia akmod-nvidia-470xx` (first attempt failed file conflicts `xorg-x11-drv-nvidia-libs` 610 vs 470xx, then `swap libs` `xorg-x11-drv-nvidia-libs → 470xx-libs` removed 22 installed 2, then `install akmod-nvidia-470xx` 14 packages, `akmods --force` `modinfo 470.256.02` `extra/nvidia-470xx/nvidia.ko.xz` 25M, `dracut --force` 156M, `pre-reboot` 11 checks PASS, `READY_FOR_REBOOT`, user rebooted `00:36`, post-reboot 15 checks `lspci CLAIMED driver nvidia`, `lsmod nvidia 40767488`, `modinfo 470.256.02` `NVIDIA`, `nvidia-smi 470.256.02` `GeForce MX130` `49C` `Exit 0`, `kmod-nvidia-470xx-7.1.10-200` installed, `glxinfo` Intel default, `PRIME` `__NV_PRIME...=nvidia` → `NVIDIA GeForce MX130`, `journal NVRM 1` loading (vs 490) `unsupported 0`, `systemctl --failed 0` (vs 1), `kwin` active, `kscreen-doctor` `eDP-1 1920x1080` (HDMI disconnected, not failure), `nmcli` `connected full`, `sensors` `50C` (vs 67C) | `nvidia_preflight.json`, `P7_PRE_REBOOT_REPORT` 20K, `P7_POST_REBOOT_REPORT` 12K, `p7_post_reboot.json` 6.3K 15 checks, `~/.local/state/polaris/transactions/TX-P7-...json` `COMPLETED` `VERIFIED` | **Yes, reboot 00:36** `7.1.10-200` still, `SecureBoot disabled` so no MOK, `initramfs` rebuilt via `dracut --force` | `TX-P7-NVIDIA-470xx-20260831` `COMPLETED` `VERIFIED` |
| P8 | Read-Only Discovery Post-P7 | COMPLETED | Re-run baseline compare P3/P7 (userspace 54.106→8.515 -84%, `failed 1→0`, `NVRM 490→1`, `swap 1.6GB→0`, `thermal 67→50`), rank 7 candidates (Akonadi 1302M, fstab 0, boot plocate 21s background, dnf-makecache 0, services 5-10M, autostart 0, journal 0) | `p8_analysis.json` 14K (7 candidates, ranking, `highestValueNextTransaction` `TX-P8-AKONADI-DISABLE-PREVIEW` `R2` 1302M), `docs/P8_REPORT.md` 18K+addendum | `ctest` still 4/4 | **No host modification** (read-only `systemd-analyze`, `free`, `sensors`, `journalctl`, `akonadictl status`) | No | `p8_analysis.json`, `docs/P8_REPORT.md`, `ROADMAP.md` P8 `PREVIEWED → APPROVAL_REQUIRED` awaiting approval |
| P9 | Fine-Grained Optimization & Validation | COMPLETED | Fresh baseline `systemd 8.515s` `user 596ms` `akonadi 4.799s` `startupsound 2.475s`, re-rank 6 candidates (Akonadi rejected, so next `bluetooth` 5M `cups` socket 0, `plocate` 0s boot, `dnf` 0s, `autostart` 0, `journal` 0), all remaining **0s boot or tiny 5-10M** or **requires user confirmation** with low confidence, `NO_ACTION_RECOMMENDED` | `p9_analysis.json` 11K (6 candidates, ranking `bluetooth` rank 1 but tiny, `fstab` rank 7), `docs/P9_REPORT.md` 15K | `ctest` still 4/4 | **No host modification** (read-only, no `systemctl` modifications, only `is-enabled` read) | No | `p9_analysis.json`, `docs/P9_REPORT.md` |
| P10 | Planning - Post-Change Measurement & Regression Detection | PLANNED | Gap analysis 10 areas, ranked 10 tasks, selected #1 Post-Change Measurement (`Comparison` + `RegressionEngine`) as highest-value | `docs/P10_PLAN.md` 24K (gap, ranked 10, selected #1 with implementation plan, tests, acceptance) | `ctest` still 4/4 (planning only) | **No host modification** (planning only, `cat` `ls` of `~/Documents/lin-opt`) | No | `docs/P10_PLAN.md` |
| P11 | Post-Change Measurement & Regression Detection (Engineering) | COMPLETED | Implement `core/domain/Comparison.h` (`Comparison` `MetricComparison` `Verdict` `SUCCESS`/`REGRESSION` etc., `isDeterministic` true, serializable), `core/engines/comparison/ComparisonEngine.h/.cpp` (pure, thresholds `boot +10%` `available -1GiB` `thermal +15C` `new_failed`, `expected vs observed`, `unavailable` handling), extend `Transaction` with `beforeBaseline` `afterBaseline` `comparison` (`std::optional`, backward compatible), CLI `transaction show --json` + `compare` expose comparison, P7 fixture + 15-category tests | `core/domain/Comparison.h` `core/engines/comparison/ComparisonEngine.*` `core/safety/transaction/Transaction.h` extended, `cli/p4_cli.cpp` extended `compare`, `tests/unit/test_comparison.cpp` 12 categories, `tests/integration/test_post_change.cpp` `test_regression.cpp` `test_observed_benefit.cpp` (fixtures `/tmp/polaris-test-root`), `docs/P11_POST_CHANGE_MEASUREMENT.md` | `ctest 9/9 0.06s` **100%** `unit` `real_providers` `parsers` `readonly` `p4_security` `comparison` `post_change` `regression` `observed_benefit` - `test_comparison` 12 categories `P7 fixture` `zero-before` `threshold boundary` etc. - **PASS** | **No real-host mutation** (`stat /etc/fstab` 21:19 unchanged, `is-enabled mssql` `disabled`, `wait-online` `disabled`, `extra/nvidia-470xx` remains, `akonadi` running, `helper.sock` not exists) | No | `core/domain/Comparison.h`, `core/engines/comparison/*`, `Transaction.h` extended, `tests/unit/test_comparison.cpp` etc., `docs/P11_POST_CHANGE_MEASUREMENT.md`, updated `docs/TRANSACTION_MODEL.md` `docs/ARCHITECTURE.md` `docs/API.md` `docs/ROADMAP.md` |
| P12 | Transaction/State-Machine Hardening (Stale-Preview + Idempotency) | COMPLETED | Extend `Transaction` with `beforeHash/approvedBeforeHash`, `beforeUnitHash/approvedUnitHash`, `kernelVersion/approvedKernelVersion`, `packageStateHash/approvedPackageStateHash`, `approvedTarget/approvedOperation`, `preconditions/approvedPreconditions`, `idempotencyKey/appliedAt/completedAt/validationResult` + `TransactionValidator` (pure stale checks) + `TransactionStore` (duplicate/idempotent/backup boundary) + `StateMachine` hardened (`PREVIEWED/APPROVAL_REQUIRED/APPROVED→FAILED`) + `AuditLog` fsync | `core/safety/transaction/Transaction.h:26` extended, `TransactionValidator.h/.cpp` 222 lines, `TransactionStore.h/.cpp` 532 lines, `StateMachine.h:55` hardened, `AuditLog.cpp` fsync, `tests/security/test_p12_stale.cpp` 10 cats+TOCTOU, `test_p12_idempotency.cpp` 5 cats, `test_p12_statemachine.cpp` 20 trans, `test_p12_transaction_model.cpp` 3 cats | `ctest 13/13 0.11s` **100%** `unit`..`observed_benefit` (9) + `p12_stale` `p12_idempotency` `p12_statemachine` `p12_transaction_model` - **PASS** | **No real-host mutation** (`stat /etc/fstab` 21:19 unchanged, `is-enabled mssql` `disabled`, `extra/nvidia-470xx` remains, `akonadi` running, helper not exists, fixtures `/tmp/polaris-test-root` only) | No | `core/safety/transaction/*Validator*/*Store*`, `StateMachine` hardened, `docs/P12_PLAN.md`, `docs/P12_IMPLEMENTATION_REPORT.md`, `docs/TRANSACTION_MODEL.md` + `ARCHITECTURE.md` updated |
| P13 | User Workflow / Profile Engine | COMPLETED | Explicit profile model `TriState` unknown/yes/no for 8 fields + extra, `ProfileStore` atomic 0600, `ProfileService` explicit no inference, `ProfileAdvisor` BLOCKED/REQUIRES/ALLOWED with explainability, CLI `profile show|set` - no inference, profile is constraint not approval | `core/profile/UserProfile.h/.cpp` `TriState` + `UserProfile` 8 fields + `extra`, `ProfileStore.h/.cpp` `profilePath` `~/.local/state/polaris/profile.json` atomic, `ProfileService.h/.cpp` explicit update + audit, `ProfileAdvisor.h/.cpp` `Decision` + `AdvisorResult`, `cli/p4_cli.cpp` `profile` cmds, `core/safety/FileSafety.h` profile allowlist | `ctest 17/17 0.13s` **100%** `unit`..`p12_*` (13) + `p13_profile_model` 6 cats, `p13_profile_store` 6 cats, `p13_profile_service` 6 cats, `p13_profile_advisor` 12 cats - **PASS** | **No real-host mutation** (`stat /etc/fstab` 21:19 unchanged, `mssql` `disabled`, `akonadi` running, `profile.json` not created by tests, fixtures `/tmp/polaris-test-root` only) | No | `core/profile/*`, `docs/P13_PLAN.md`, `docs/P13_IMPLEMENTATION_REPORT.md`, `ARCHITECTURE.md` updated |
| P14 | Expanded Security & IPC / Helper Architecture | COMPLETED | Minimal IPC `PROTOCOL_VERSION=1`, `MAX_REQUEST_SIZE=64KB`, bounded validation (NUL/shell/traversal/oversized/password), `SO_PEERCRED` same-user auth, Unix socket `0600` not world-writable, no symlink, stale detection, `TransactionLock` `flock` exclusive, `RecoveryDetector` `BACKUP_CREATED/APPLYING`→`FAILED` fail-closed, audit `ipc.*`/`lock.*`/`recovery.*`, no privileged mutation | `core/ipc/IpcProtocol.h/.cpp`, `IpcAuth.h/.cpp`, `IpcServer.h/.cpp`, `IpcClient.h/.cpp`, `core/safety/lock/TransactionLock.h/.cpp`, `core/safety/recovery/RecoveryDetector.h/.cpp` | `ctest 24/24 0.57s` **100%** `unit`..`p13_*` (17) + `p14_ipc_protocol` 12 cats, `p14_ipc_auth` 5 cats, `p14_socket_security` 5 cats, `p14_ipc_server` 4 cats, `p14_lock` 5 cats, `p14_ipc_security` 12 cats, `p14_recovery` 4 cats - **PASS** | **No real-host mutation** (`stat /etc/fstab` 21:19 unchanged, `mssql` `disabled`, `akonadi` running, `helper.sock` not created, fixtures `/tmp/polaris-test-root/p14` only) | No | `core/ipc/*`, `core/safety/lock/*`, `core/safety/recovery/*`, `docs/P14_PLAN.md`, `docs/P14_IMPLEMENTATION_REPORT.md` |
| P15 | Test / CI / Fixture Expansion | COMPLETED | Table-driven deterministic isolated fixtures, stale matrix 7×3, TOCTOU between gates, idempotency, lock exclusive, recovery detection, rollback preservation, regression thresholds, FileSafety/IPC/Audit, fixture isolation, CI `cmake --fresh`/`ctest` | `tests/unit/test_p15_lifecycle.cpp` valid16+rejected12, `test_p15_stale_matrix.cpp` 19 cases, `test_p15_toctou_idempotency.cpp` TOCTOU+idempotency, `test_p15_lock_recovery.cpp` lock/recovery/rollback, `test_p15_regression_audit.cpp` regression 16+FileSafety 12+Audit, `core/safety/transaction/TransactionValidator.h` fix UNAVAILABLE→fail-closed, `.github/workflows/ci.yml` | `ctest 29/29 0.70s` **100%** `unit`..`p14_*` (24) + `p15_lifecycle`, `p15_stale_matrix`, `p15_toctou_idempotency`, `p15_lock_recovery`, `p15_regression_audit` - **PASS** | **No real-host mutation** (`stat /etc/fstab` 21:19 unchanged, `mssql` `disabled`, `akonadi` running, `profile.json` not touched, `helper.sock`/`transaction.lock` not created, fixtures `/tmp/polaris-test-root/p15` only) | No | `tests/p15/*`, `core/safety/transaction/TransactionValidator.h` fix, `.github/workflows/ci.yml`, `docs/P15_PLAN.md`, `docs/P15_IMPLEMENTATION_REPORT.md` |
| P16 | Explainability | COMPLETED | Deterministic `Explanation` 22 fields, `ExplanationEngine` `explainCandidate`/`explainTransaction`/`explainComparison` with `ProfileAdvisor` `BLOCKED`/`REQUIRES`/`ALLOWED` + `Comparison` `expected`/`observed`/`verdict`/`hasRegression`, CLI `explain <candidate>` and `transaction explain <id>` `--json`/`--verbose` (`WHY NOW`/`WHAT WILL CHANGE`/`WHAT WILL NOT CHANGE`/`EVIDENCE`/`EXPECTED BENEFIT`/`CONFIDENCE`/`RISK`/`REVERSIBILITY`/`REBOOT`/`AUTHORIZATION`/`REJECTION CONDITIONS`/`ROLLBACK`/`OBSERVED RESULT`/`VERDICT`), redaction `[REDACTED]`, audit `explanation.generated` | `core/explainability/Explanation.h/.cpp`, `ExplanationEngine.h/.cpp`, `cli/p4_cli.cpp` `explain` cmds | `ctest 33/33 0.70s` **100%** `unit`..`p15_*` (29) + `p16_explanation_model` 6 cats, `p16_explain_candidate` 7 cats, `p16_explain_transaction` 8 cats, `p16_verbose_redaction` 8 cats - **PASS** | **No real-host mutation** (`stat /etc/fstab` 21:19 unchanged, `mssql` `disabled`, `akonadi` running, `helper.sock` not created, fixtures `/tmp/polaris-test-root/p16` only) | No | `core/explainability/*`, `docs/P16_PLAN.md`, `docs/P16_IMPLEMENTATION_REPORT.md` |
| P17 | Campaign 2 / Evidence-Backed Real-Host Optimization | COMPLETED (NO_ACTION_RECOMMENDED) | Read-only discovery 03:18 `systemd-analyze` 8.515s `critical-chain` `plasmalogin 7.301s` not `plocate` 21s, `free` 5.8Gi `zram` 0B, `akonadi` 14 agents 1302M `ProfileAdvisor` `BLOCKED` (handoff) / `REQUIRES` (file unknown), `bluetooth` `enabled` `active` 2 paired, `avahi` for `kdeconnect`, `plocate` `21.111s` not in `critical-chain`, `explain` `bluetooth-disable` `REQUIRES_USER_CONFIRMATION`, scored 7 candidates all `0s` boot-critical or `5-10M` tiny `REQUIRES`/`REJECTED` → `NO_ACTION_RECOMMENDED`, exactly ONE considered `bluetooth-disable` but negligible, no `PREVIEWED` transaction | `docs/P17_REPORT.md` (discovery, scoring, explainability) | `ctest 33/33 still` **100%** (no new tests, read-only) - **PASS** | **No real-host mutation** (`stat /etc/fstab` 21:19 unchanged, `mssql` `disabled`, `akonadi` running, `helper.sock` not created, `profile.json` not touched, fixtures `/tmp/polaris-test-root/p17` only) | No | `docs/P17_REPORT.md`, `audit.log` `explanation.generated` |

Do not claim a phase complete unless repository evidence supports it - all above verified via `ls -lh ~/Documents/lin-opt/docs/*.md` `ls ~/Documents/lin-opt/p*.json` `cat` `ls -R`, `cmake -S` `ctest`, `stat`, `systemctl`, `lspci`, `modinfo`, `nvidia-smi`.

---

## 3. Current Host State (Only Verified Facts, 2026-09-01 08:00)

**Distinguish CURRENT VERIFIED STATE from HISTORICAL STATE.**

- **Fedora/kernel:** `Fedora 44` `VERSION_ID=44` `7.1.10-200.fc44.x86_64` `x86_64` (`cat /etc/os-release` `uname -r`)
- **NVIDIA driver state:** `470.256.02` `NVIDIA` `firmware nvidia/470.256.02/gsp.bin` `extra/nvidia-470xx/nvidia.ko.xz` 25M `vermagic 7.1.10-200` (`modinfo nvidia` `version 470.256.02`)
- **GPU state:** `01:00.0 GM108M [GeForce MX130] [10de:174d]` **CLAIMED** `driver=nvidia` `lspci` + `readlink driver -> nvidia` + `lshw driver=nvidia` (historical: `UNCLAIMED` before P7)
- **NVIDIA 470xx status:** **COMPLETED and VERIFIED** (P7 post-reboot 15 checks PASS, `nvidia-smi` `470.256.02` `49C` `kwin_wayland 0MiB`, `akmod-nvidia-470xx-470.256.02-18` + `kmod-nvidia-470xx-7.1.10-200` installed, `ls /lib/modules/.../extra/nvidia-470xx` 5 files, `journal NVRM` 1 loading vs 490 before, `systemctl --failed` 0)
- **Akonadi status:** **running** (rejected in P8/P9, not disabled) `akonadictl status` `Control running` `Server running`, `ps aux | grep akonadi` 14 agents + `mysqld` 1302M `db_data` 126M `Akonadi.error` 0 (P8/P9 `CAND-AKONADI` `REJECTED` because user uses KMail/Kontact - now also enforced via `ProfileAdvisor` `usesKMail=yes` → `BLOCKED_BY_USER_WORKFLOW`)
- **mssql-server state:** **disabled** (P6 `systemctl disable` `Removed /etc/systemd/system/multi-user.target.wants/mssql-server.service`, `is-enabled disabled`, `is-active failed` until reboot, after P7 reboot `systemctl --failed` **0** `0 loaded units listed` - verified `systemctl --failed` 0, not 1)
- **fstab state:** `cat /etc/fstab` 3 entries (`UUID 24bd... / ext4`, `UUID 3C27... /boot/efi vfat`, `# UUID 39b0... swap` commented from P2), `findmnt --verify` `0 parse errors`, `stat /etc/fstab` `2026-08-31 21:19` (from P2 Level2, unchanged since)
- **zram state:** `zramctl` `8G lzo-rle` `DISKSIZE 8G DATA 4K COMPR 80B` `swapon --show` `8G 0B used` `100` (healthy, not modified, do not modify per P10)
- **failed systemd units:** **0** `0 loaded units listed` (historical: `1 mssql` before P6, `2` with packagekit before P2)
- **KDE/Wayland state:** `plasmashell 6.7.4` `active` `kwin_wayland` `2004` `9.0% 301M` + `2168` `4.4% 460M`, `XDG_SESSION_TYPE=wayland` `WAYLAND_DISPLAY=wayland-0` `DISPLAY=:0`, `kscreen-doctor` `eDP-1 1920x1080` `enabled` (HDMI disconnected, not failure), `xrandr` `eDP-1 connected primary`
- **Performance baseline (P9 fresh):** `systemd-analyze` `3.167/13.550/1.498/3.862/8.515 = 30.594s` `userspace 8.515s` (vs P3 `54.106s` -84%), `systemd-analyze --user` `596ms` `akonadi_control 4.799s`, `free -h` `11Gi 5.3Gi used 6.1Gi avail` (vs P3 4.2GB), `zram 0B` (vs 1.6GB), `sensors` `coretemp 56C` (vs 67C), `journal p3 141` (vs 254), `NVRM 1` (vs 490)
- **Reboot state:** Reboot occurred `2026-09-01 00:36` for P7 NVIDIA (verified `journalctl --no-pager -b` `Sep 01 00:29:45 NVRM loading 470.256.02`, `lsmod nvidia` loaded), no reboot in P8/P9/P10/P11/P12/P13/P14/P15/P16/P17/P18 (planning/engineering/reporting, no host modify)
- **Helper IPC state:** `ls /run/polaris/helper.sock` **not exists** (helper not installed, as per P4 design, not yet implemented, correct for P13)
- **Profile state:** `ls ~/.local/state/polaris/profile.json` **not exists** (correct, P13 `profile show` does not auto-create; tests use `/tmp/polaris-test-root` fixtures only, `polaris_p4 profile set` not yet run; if exists, would be `{"usesAkonadi":"unknown",...,"usesKMail":"unknown",...}` deterministic `unknown` defaults)

Historical: Before P7, `UNCLAIMED`, `nvidia` not loaded, `nvidia-smi` failed, `NVRM 490`, `mssql` `enabled` `failed`, `fstab` had stale swap, `zram` 1.6GB used, `failed` 1, `userspace` 54s, `akonadi` same but not yet rejected.

---

## 4. P13 Implementation

**Documented in `docs/P13_PLAN.md:1` and `docs/P13_IMPLEMENTATION_REPORT.md:1`**

- **Profile model:** `core/profile/UserProfile.h:1` `TriState` `UNKNOWN/YES/NO` (`unknown` default, `toString`/`fromString` throws on `maybe`), `UserProfile` 8 fields (`usesKMail`, `usesKontact`, `usesKOrganizer`, `usesBluetooth`, `usesPrinting`, `usesAvahi`, `usesCups`, `usesAkonadi`) + `extra` map, `isDefaultUnknown`, `==`, `knownFields()` sorted, `getField`/`setField` explicit throws on unknown field, `toJson` deterministic sorted keys, `fromJson` strict throws on malformed/invalid value.
- **ProfileStore:** `core/profile/ProfileStore.h:1` `profilePath()` `~/.local/state/polaris/profile.json`, `testProfilePath()` `/tmp/polaris-test-root/profile.json`, `load` (missing→unknown no auto-create, symlink→throw, malformed→throw + audit `profile.load.malformed`), `save` (validate path traversal/metachars/allowlist via `FileSafety`, symlink check, `create_directories`, atomic `tmp+fsync+chmod 0600+rename`, parent canonical check, 0600), `exists`/`remove`, deterministic.
- **ProfileService:** `core/profile/ProfileService.h:1` explicit `updateField(profile, field, TriState/valueStr, path)` (validates known field, no inference `usesKMail→usesAkonadi` not done, idempotent `profile.update.idempotent` if same value skips write, else `setField` + `save` + audit `profile.updated` with `field`/`previous`/`new`/`applied`), `updateFieldInStore`, `isIdempotent`.
- **ProfileAdvisor:** `core/profile/ProfileAdvisor.h:1` `Decision` `BLOCKED_BY_USER_WORKFLOW`/`REQUIRES_USER_CONFIRMATION`/`ALLOWED_FOR_ANALYSIS`, `AdvisorResult` (`reason`, `causingField/value`, `explicitFact`, `whatWillNotChange`, `confirmationRequired`, `candidate`), `canConsiderAkonadi` (YES any of `usesKMail/Kontact/KOrganizer/Akonadi` → `BLOCKED` with field-specific reason, `whatWillNotChange` “Akonadi will remain enabled…”, `confirmationRequired` explicit no needed; UNKNOWN any → `REQUIRES`; all NO → `ALLOWED` with “does NOT authorize mutation; still requires RECOMMEND→PREVIEW→APPROVAL…”); `canConsiderBluetooth/Printing/Avahi/Cups` similarly; all explainable.
- **CLI:** `cli/p4_cli.cpp:1` extended `profile show` (loads without auto-create, prints JSON + advisor example, `--json`), `profile set <field> <yes|no|unknown>` (explicit, audit, not authorization), help updated; `FileSafety` extended for profile allowlist.
- **Tests:** `tests/unit/test_p13_profile_model.cpp` 6 cats, `tests/unit/test_p13_profile_store.cpp` 6 cats, `tests/security/test_p13_profile_service.cpp` 6 cats, `tests/unit/test_p13_profile_advisor.cpp` 12 cats - **all PASS**, fixtures `/tmp/polaris-test-root` only, real profile not touched.
- **Limitations:** Not yet integrated into `RecommendationEngine` ranking, no UI interview, no versioning, no concurrent `flock` (future P14/P16/P17).

---

## 4d. P16 Implementation

**Documented in `docs/P16_PLAN.md:1` and `docs/P16_IMPLEMENTATION_REPORT.md:1`**

- **Explanation model:** `core/explainability/Explanation.h:1` `CandidateKind` (`RECOMMENDATION`/`TRANSACTION`/`PROFILE_CONSTRAINT`), `DecisionKind` (`RECOMMEND`/`REQUIRE_CONFIRMATION`/`BLOCKED`/`PREVIEWED`/`APPROVED`/`FAILED`/`COMPLETED`/`REGRESSION`/`NO_CHANGE`), `Explanation` 22 fields (`id`, `candidateId`, `candidateKind`, `decision`, `decisionLabel`, `whyNow`, `evidence` sorted, `expectedBenefit`, `confidence`, `risk`, `reversibility`, `rebootRequired`, `authorizationRequired`, `userImpact`, `whatWillChange`, `whatWillNotChange`, `rejectionConditions` sorted, `dependencies` sorted, `rollbackSummary`, `beforeStateSummary`, `afterStateSummary`, `observedBenefit`, `verdict`, `verdictReason`, `hasRegression`, `limitations`), `toJson` deterministic sorted keys, `fromJson` strict, `toHuman(verbose)` redacted via `containsSecret`/`redact`
- **ExplanationEngine:** `core/explainability/ExplanationEngine.h:1` `explainCandidate` (`WHY NOW`/`WHAT WILL CHANGE`/`WHAT WILL NOT CHANGE`/`rejectionConditions` with `ProfileAdvisor` `BLOCKED`/`REQUIRES`/`ALLOWED` + `Comparison` `expected`/`observed`/`verdict`/`hasRegression`), `explainTransaction` lifecycle `PREVIEWED`→`FAILED` with `expected`/`observed`/`backup`/`rollback`/`authorization` distinction, `explainComparison` expected vs observed, deterministic
- **CLI:** `cli/p4_cli.cpp:1` `explain <candidate>` and `transaction explain <id>` with `--json`/`--verbose` (`WHY NOW`/`WHAT WILL CHANGE`/`WHAT WILL NOT CHANGE`/`EVIDENCE`/`EXPECTED BENEFIT`/`CONFIDENCE`/`RISK`/`REVERSIBILITY`/`REBOOT`/`AUTHORIZATION`/`REJECTION CONDITIONS`/`ROLLBACK`/`OBSERVED RESULT`/`VERDICT`), audit `explanation.generated` (not `approved`)
- **Limitations:** No host mutation, no `sh -c`, no password, `IPC` `ping`/`info` only - all read-only


---

## 4b. P14 Implementation

**Documented in `docs/P14_PLAN.md:1` and `docs/P14_IMPLEMENTATION_REPORT.md:1`**

- **IPC Protocol:** `core/ipc/IpcProtocol.h:1` `PROTOCOL_VERSION=1`, `MAX_REQUEST_SIZE=64KB`, `MAX_RESPONSE_SIZE=64KB`, `MAX_ARG_COUNT=16`, `MAX_ARG_SIZE=4096`, `MAX_FIELD_SIZE=256`, `TIMEOUT_MS=5000`, `allowedOperations` `ping`/`info` only (no privileged mutation), `Request`/`Response`/`ValidationResult`, `validate()`/`validateRaw()` (NUL, protocol, requestId/operation size/shell/traversal/allowlist, args count/size/shell/traversal/password/control-char), `serialize`/`parse` deterministic, `containsNul`/`containsTraversal`/`containsShellMetachars`
- **IpcAuth:** `core/ipc/IpcAuth.h:1` `PeerCred` (`pid,uid,gid`), `getPeerCred(int fd)` via `getsockopt(SO_PEERCRED)` Linux (`nullopt` on failure), `isAuthorized(cred, expectedUid)` (`uid==expected && pid>0`), `containsSpoofedCred`, `currentUid()`
- **IpcServer/IpcClient:** `core/ipc/IpcServer.h:1` `defaultSocketPath()` `/run/polaris/helper.sock` (defined but never created in P14), `testSocketPath()` `/tmp/polaris-test-root/p14/helper.sock`, `validateSocketPath`/`checkParentSecurity`/`isStaleSocket` (NUL, traversal, shell, allowlist `/tmp/polaris-test-root/` or `/run/polaris/`, symlink, `0700`/`0600` not world-writable, `isStale` `ECONNREFUSED`), `start()` (`validateAndPrepare` `mkdir 0700`, stale unlink, `socket` `FD_CLOEXEC`, `umask 0077` `bind` `chmod 0600` `listen(8)`), `stop()` `close`+`unlink`, `handleRequest(raw, peerCred)` (auth `unavailable→error`, `isAuthorized` wrong UID→`peer not authorized`, `containsSpoofedCred`→`spoofed`, `validateRaw`→`malformed`/`oversized`/`unknown`, allowlist `ping`→`pong` / `info`→`version`, audit `ipc.*` with `TX-TEST-IPC-` prefix, `fsync`), `handleNextConnection(timeoutMs)` (`poll` accept 5s, `getPeerCred`, `poll` recv 5s, bounded `MAX_REQUEST_SIZE`, handle, send `MAX_RESPONSE_SIZE`+`\n`, close)
- **IpcClient:** `core/ipc/IpcClient.h:1` `testSocketPath`, `send(Request)` serialize+`sendRaw`, `sendRaw(string)` (size check, `socket` `FD_CLOEXEC`, non-blocking `connect` + `poll` `SO_ERROR`, `::send`, `poll` `recv`)
- **TransactionLock:** `core/safety/lock/TransactionLock.h:1` `defaultLockPath()` `/run/polaris/transaction.lock` (never used), `testLockPath()` `/tmp/polaris-test-root/p14/transaction.lock`, `tryLock()` (`open O_CREAT|O_RDWR 0600` `FD_CLOEXEC`, parent symlink/world-writable check, `flock LOCK_EX|LOCK_NB` → `lock.rejected` on `EWOULDBLOCK`, `chmod 0600`, `lock.acquire`), `unlock()` `flock LOCK_UN`+`close`, `isLocked`, audit `lock.*`
- **RecoveryDetector:** `core/safety/recovery/RecoveryDetector.h:1` `RecoveryInfo` (`id`, `state`, `backupPath`, `backupExists`, `suggested=FAILED`, `reason`), `detect(storePath)` scans `*.json` for `state` in `BACKUP_CREATED/APPLYING/APPLIED/VERIFYING/AUTHORIZED` → incomplete, `suggested FAILED` (never `COMPLETED`), `isIncomplete`, `shouldFailClosed` always true, `defaultStorePath`/`testStorePath`, audit `recovery.detected`, never auto-mutates
- **Limitations:** No privileged mutation via IPC (allowlist only `ping`/`info`), no `sh -c`, no `exec`, no password, helper not installed, lock advisory not mandatory, recovery detection-only - all fail-closed

---

## 4c. P15 Implementation

**Documented in `docs/P15_PLAN.md:1` and `docs/P15_IMPLEMENTATION_REPORT.md:1`**

- **Validator fix:** `core/safety/transaction/TransactionValidator.h:1` `beforeHash`/`kernelVersion`/`packageStateHash` empty where `approved*` non-empty → `unverifiable_*` fail-closed (previously passed)
- **Lifecycle:** `tests/unit/test_p15_lifecycle.cpp` table-driven `StateMachine` valid 16 + rejected 12 with `logic_error` `rejected, fail closed`, `stale→FAILED`, `unverifiable→FAILED`
- **Stale matrix:** `tests/unit/test_p15_stale_matrix.cpp` 7 fields (`target`, `operation`, `beforeHash`, `unitHash`, `kernel`, `package`, `precondition`) ×3 states (`UNCHANGED` accepted, `CHANGED`/`UNAVAILABLE` rejected) 19 cases `expected`/`observed` deterministic, multi-field first failure
- **TOCTOU/idempotency:** `tests/unit/test_p15_toctou_idempotency.cpp` TOCTOU between gates (first valid, second re-read stale → `FAILED` no mutation backup preserved), symlink TOCTOU, idempotency `create`/`approve`/`apply`/`verify` + reload via `exists`/`duplicate` not overwrite
- **Lock/recovery/rollback:** `tests/unit/test_p15_lock_recovery.cpp` `TransactionLock` exclusive table (2/4/3 threads), `FD_CLOEXEC`, stale parent, `RecoveryDetector` table 10 states, scan `BACKUP_CREATED`/`APPLYING` detected, `COMPLETED` not, corrupted not, never auto-apply, rollback `BackupEngine::sha256File` stable second `create` throws not overwritten
- **Regression/audit:** `tests/unit/test_p15_regression_audit.cpp` regression thresholds `boot` exactly 10% not, just above regression, `mem`/`thermal`, `unavailable`, `multi`, `observedBenefit` positive/zero/negative deterministic, `FileSafety` 12 cases, `IpcProtocol` 6, `Audit` chain `previousHash`→`eventHash` deterministic `fsync`, no secrets, fixture isolation `p15_iso1` vs `p15_iso2`; plus `.github/workflows/ci.yml` minimal `cmake --fresh`/`cmake --build`/`ctest`


---

## 5. Remaining Engineering Roadmap (Exact Order, P18 - COMPLETE)

| Phase | Title | Objective | Dependency | Status | Implementation or Real-Host Optimization | Explicit Approval Required | Reboot May Be Required | Host Mutation Allowed | Acceptance Criteria |
|-------|-------|-----------|------------|--------|------------------------------------------|----------------------------|------------------------|-----------------------|---------------------|
| P14 | Expanded Security & IPC/Helper Architecture | Minimal helper, Unix socket `SO_PEERCRED`, `audit fsync`, `transaction lock` `flock`, `crash recovery` detection fail-closed | P12 | **COMPLETED** | Implementation | Yes | No | No (helper defined but never installed, fixtures `/tmp/polaris-test-root/p14` only) | `IpcProtocol` ping/info, `SO_PEERCRED` same-user, `TransactionLock` flock, `RecoveryDetector` BACKUP_CREATED→FAILED |
| P15 | Test/CI/Fixture Expansion | Table-driven deterministic isolated fixtures, stale matrix, TOCTOU, idempotency, lock, recovery, rollback, regression, FileSafety/IPC/Audit, CI | P11 | **COMPLETED** | Implementation | No | No | No (fixtures `/tmp/polaris-test-root/p15` only, `.github/workflows/ci.yml`) | `ctest 29/29 100%` 5 new suites |
| P16 | Explainability & HCI | `WHY NOW?` `WHAT WILL NOT CHANGE?` `WHAT WOULD MAKE US REJECT IT?` + `--verbose` progressive disclosure + `WHY THIS` already | P13 | **COMPLETED** | Implementation | No | No | No (explainability deterministic, `explain` CLI) | `Explanation` 22 fields, `ExplanationEngine` `WHY NOW`/`WHAT WILL NOT CHANGE`, `explain` CLI |
| P17 | Campaign 2 / Evidence-Backed Real-Host Optimization | Read-only discovery 03:18 `systemd-analyze` 8.515s, `akonadi` `BLOCKED`, `bluetooth` `REQUIRES`, scored 7 candidates → `NO_ACTION_RECOMMENDED` (exactly ONE considered but negligible) | P13 | **COMPLETED (NO_ACTION_RECOMMENDED)** | **Real-Host Optimization** (one at a time, but no worthwhile benefit) | **Yes** (would be) | Maybe | **Yes** (but not executed) | `NO_ACTION` - no `PREVIEWED` transaction created, stop at `RECOMMEND` boundary |
| P18 | Final Benchmark / ROI / Stability Report | Final host validation 03:25 `systemd-analyze` 8.515s `critical-chain` `plasmalogin` `free` 5.6Gi `zram` 0B `lspci` `CLAIMED` `driver=nvidia` `modinfo` `470.256.02` `nvidia-smi` `470.256.02` `glxinfo` `Mesa Intel` `PRIME` `NVIDIA GeForce MX130` `akonadictl` `running` 14 agents `mssql` `disabled` `fstab` 3 entries `stat` `2026-08-31 21:19` `Comparison` `P3` `54.106s`→`8.515s` `-84%` not `+10%` `available` `4.2→5.6` `+1.4GB` not `<-1GB` `thermal` `67→60` not `+15C` `nvidia` `0→1` not `failed` `zram` `0B` `Transaction ROI` `TX-P6` `TX-P7` `Campaign 2` `NO_ACTION` `Safety` `READ→...→AUDIT` `33/33` `PROJECT_COMPLETE_WITH_LIMITATIONS` | P11 | **COMPLETED** | Implementation (report, no host mutation) | No | No | No (`FINAL_REPORT.md` `15K`, `FINAL_STATE.json`) | `FINAL_REPORT.md` `FINAL_STATE.json` `PROJECT_COMPLETE_WITH_LIMITATIONS` `no P19` |

Do not assume all must be implemented - rank above is by engineering value and dependency - **P15 is next**.

---

## 6. Remaining Optimization Candidates (Separate from Engineering Phases)

| Candidate | Current Evidence | Confidence | Expected Benefit | Risk | Reversibility | Current Status | Whether Rejected | Reason for Rejection if Applicable |
|-----------|------------------|------------|------------------|------|---------------|----------------|------------------|------------------------------------|
| Akonadi/KDE PIM | `akonadictl status` `running` 14 agents `1302M` `db_data 126M` `not in` `systemctl --failed` `kdepim-runtime` `kmail` installed, **user uses KMail** per P8 `question` `Reject Akonadi` + `ProfileAdvisor` `usesKMail=yes` → `BLOCKED_BY_USER_WORKFLOW` | 0.65 (blocked by workflow, would be 0.90 if `usesKMail=no`) | ~1.3GB RAM, 14 processes, reduced I/O login, 0s boot | R2 (breaks KMail) | High (akonadictl start) | **REJECTED** | **Yes, rejected** | User uses KMail/Kontact - do NOT recommend disabling unless user explicitly changes `usesKMail=no` via `profile set` |
| fstab / Storage | `cat /etc/fstab` 3 entries, `findmnt --verify` 0 errors, `fstrim.timer` enabled, `df -h /` 69% 102G free, `lsblk` nvme 465G | 0.95 | 0 - already optimal (P2 fixed stale swap) | R3 (could break boot) | Medium | **NO_ACTION** | No (not rejected, but no benefit) | No evidence of bottleneck, fstab healthy |
| Boot / plocate | `plocate-updatedb.service` 21.111s but **not in** `critical-chain` (via `plasmalogin` 7.301s), `tpm` 5.5s parallel | 0.85 | 0s boot (not blocking) | R2 | High | **NO_ACTION** | No | Not on critical path, useful for `locate`, already `Nice 19 idle` |
| dnf-makecache | `inactive dead` `timer enabled` `OnBootSec 10min` `Not in` `critical-chain` `not in` current `blame` top 30 | 0.75 | 0s boot | R2 | High | **NO_ACTION** | No | Not blocking, useful, 0s benefit |
| Services (bluetooth etc.) | `bluetooth` `enabled` `active` 2 paired `TSCO-TS2343` `E7`, `avahi` `active` for `kdeconnect`, `cups` `disabled` but `active` via socket `lpstat: No destinations` | 0.40 | 5-10M tiny | R2 | High | **NO_ACTION** | No | No evidence user doesn't use BT/printing (actually evidence shows BT used), tiny benefit - profile `usesBluetooth=unknown` → `REQUIRES_USER_CONFIRMATION` |
| Autostart | `~/.config/autostart` 1 file `nvidia-settings-user.desktop` `Hidden=true` (P5), `/etc/xdg/autostart` 30+ KDE essential, `nvidia-settings-470xx` `Hidden=false` correct | 0.70 | 0 | R1 | High | **NO_ACTION** | No | Already minimal and correct |
| Journal / Error Families | `p3 141` (vs 254) `NVRM 1` (vs 490) `anydesk` 1, `VirtualBox` 6, `hid-generic` 1, `ACPI` 2, `kvm_amd` 3 | 0.90 | 0 | R0 | N/A | **NO_ACTION** | No | Already improved via nvidia/mssql fixes |

**Important known decision:** **Akonadi is REJECTED because user uses KMail/Kontact - now also enforced by `ProfileAdvisor` `usesKMail=yes` → `BLOCKED_BY_USER_WORKFLOW` - do NOT recommend disabling unless user explicitly runs `polaris_p4 profile set usesKMail no` and `usesKontact no` etc.** 

**NVIDIA 470xx migration is COMPLETED and VERIFIED** (P7 post-reboot 15 checks PASS: `lspci` `driver nvidia`, `lsmod` `nvidia 40767488`, `modinfo` `470.256.02`, `nvidia-smi` `470.256.02` `49C`, `PRIME` `NVIDIA GeForce MX130`, `journal NVRM` 1 vs 490, `systemctl --failed` 0, `glxinfo` `Mesa Intel` default, `kscreen-doctor` `eDP-1` 1920x1080, `nmcli` `connected full`, `sensors` 50C). **Do not revisit unless regression is detected** (e.g., `nvidia-smi` fails again, `NVRM` returns, `lspci` UNCLAIMED, `journal` `not supported`).

**mssql-server was disabled in P6 and verified after reboot** (`is-enabled disabled`, `systemctl --failed` 0, not 1). **Do not re-enable or modify unless explicitly requested or a future investigation requires it** (e.g., user says they need local MSSQL for `solutik`).

---

## 7. Safety Invariants (MUST NOT Be Weakened)

**Architectural Invariant:**

```
READ → MEASURE → ANALYZE → EXPLAIN → RECOMMEND → PREVIEW → EXPLICIT APPROVAL → BACKUP → APPLY → VERIFY
```

- **No batch changes:** One real-host transaction at a time (P5 `nvidia-settings` R1, P6 `mssql` R2, P7 `nvidia` R3 each separate, P8 `akonadi` rejected, P9 `NO_ACTION`).
- **No launch==approval:** Viewing a recommendation, running diagnostics, or launching Polaris is **not** approval - requires explicit `polaris_p4 transaction approve <id>` with `beforeHash` + P12 `approvedBeforeHash` binding; `profile set` is also not approval for host mutation.
- **Exact transaction approval:** Tied to `transactionId` + `target` + `expected change` + `current file hash` / `unit hash` / `package state` + P12 `approvedTarget`/`approvedOperation`/`kernel`/`package`/`preconditions` - if any `beforeHash`/`unitHash`/`kernel`/`package` changes after preview, approval invalidated, require new preview (P5 `4ad53409` + P12 `hashString`); profile does **not** bypass this.
- **BeforeHash/unitHash validation:** `FileSafety::canonical` + `sha256` + P12 `TransactionValidator` check before `APPLY` (both before backup and final after backup).
- **Backup before mutation:** `BackupEngine::create` versioned `~/.local/state/polaris/backups/<tx>/` `SHA-256` no overwrite, `is_regular_file` check, `fsync` - if backup fails, **do not apply**; P12 ensures backup only after first validation, final validation after backup.
- **Rollback capability:** Every R2+ reversible operation declares `rollbackPlan` before execution (`systemctl enable mssql-server`, `dnf swap 470xx→610`, `akonadictl start`), `rollbackState` `AVAILABLE`, verified via test copy `/tmp/polaris-test-p5` for P5; P12 preserves backup on final validation failure.
- **Fail-closed state machine:** `StateMachine::isValidTransition` rejects `PROPOSED→APPLYING` or `COMPLETED→APPROVED`, throws `logic_error`, tested `p4_security` + `test_p12_statemachine` (11 illegal). P12 added `PREVIEWED/APPROVAL_REQUIRED/APPROVED → FAILED` for stale, still fail-closed.
- **FileSafety protections:** Allowlist `/tmp/polaris-test-root` (P4) + `~/.config/autostart/nvidia-settings-user.desktop` (P5 pilot, user-owned) + `/etc/fstab` (P2) + `~/.local/state/polaris/profile.json` (P13, 0600), `validatePath` rejects `..`, `;|&` `` ` `` `$`, `NUL`, `>4096`, `symlink` (`isSymlink` + `atomicWrite` temp+fsync+rename + P12 TOCTOU + P13 profile canonical), `canonical`.
- **No arbitrary shell:** Fixed executable paths `/usr/bin/systemctl`, `/usr/bin/dnf`, `execv` separate args, no `sh -c`, no user input concat, bounded execution time/output, validated in `core/safety/FileSafety.h` and `Real*Provider` `safeExec`.
- **No password collection:** No `sudo` inside Polaris, no `SUDO_ASKPASS`, no password file, no `password` logging - use narrowly scoped Polkit `org.polaris.*` `auth_admin_keep` (P4 `polkit/org.polaris.*.policy` 3 actions) + native `polkit-kde-authentication-agent-1` (running `3032`), helper trust boundary `Client (untrusted) | Helper (minimal, allowlist) | Polkit (OS)`.
- **No automatic reboot:** `rebootRequired` explicit, `P7` `READY_FOR_REBOOT` reported `Reboot required to activate NVIDIA 470xx` but **did not automatically reboot** - user rebooted at `00:36` separately.
- **No real-host mutation during discovery:** P2/P3/P8/P9/P10/P11/P12/P13 read-only via `ReadOnlyGuard` `kReadOnlyMode true` `openReadOnly` only `ifstream`, `test_readonly` verifies `stat /etc/fstab` mtime unchanged; P12/P13 tests only `/tmp/polaris-test-root` fixtures.
- **Explicit approval for every real mutation:** P5 `TX-P5-20260831-001` `Hidden=true` required `approve` with `beforeHash`, P6 `TX-P6-...-MSSQL-DISABLE-PREVIEW-V2` required `approve` for `mssql` disable, P7 `TX-P7-NVIDIA-470xx` required `NVIDIA-MIGRATION-CANDIDATE-470xx` explicit approval for `R3` - all verified via `audit.log` `transaction.approved` + P12 `approved*` binding and `validation.passed`; P13 profile `ALLOWED_FOR_ANALYSIS` still requires same approval.
- **P14 IPC security:** Unix socket `0600` not world-writable, no symlink, `SO_PEERCRED` kernel creds not client-supplied UID, bounded request `64KB` no `sh -c`/`exec`/`password`/`traversal`, allowlist `ping`/`info` only, no privileged mutation, `TransactionLock` `flock` exclusive, `RecoveryDetector` fail-closed `suggested FAILED`.
- **P15 test/CI:** Table-driven deterministic isolated fixtures, no host mutation, `TransactionValidator` fail-closed for `UNAVAILABLE`, `flock` exclusive, recovery detection-only, `Audit` chain, `FileSafety` regression, `CI` `cmake --fresh`/`ctest`.
- **P16 explainability:** Structured `Explanation` deterministic JSON, `--verbose` without secrets, `WHY NOW` evidence-backed, `WHAT WILL CHANGE` transaction-backed, `WHAT WILL NOT CHANGE` explicit, `rejectionConditions` deterministic, `profile`/`comparison` integration, never confuses `approval` with `authorization` (`explanation.generated` ≠ `transaction.approved`).
- **P18 final validation:** Read-only `systemd-analyze` `8.515s` `critical-chain` `plasmalogin` `free` `5.6Gi` `zram` `0B` `lspci` `CLAIMED` `driver=nvidia` `nvidia-smi` `470.256.02` `glxinfo` `Mesa Intel` `PRIME` `NVIDIA` `akonadictl` `running` `mssql` `disabled` `fstab` `3 entries` `stat` `2026-08-31 21:19`, `Comparison` `P3` `54.106s`→`8.515s` `-84%` not `+10%` `available` `4.2→5.6` not `<-1GB` `thermal` `67→60` not `+15C` `nvidia` `0→1` not `failed`, `Transaction ROI` `2` real `COMPLETED` `1` `NO_OP` `7` `REJECTED`/`NO_ACTION`, `Campaign 2` `NO_ACTION` `akonadi` `REJECTED` `bluetooth` `REQUIRES`, `Safety` `READ→...→AUDIT` `33/33` `PROJECT_COMPLETE_WITH_LIMITATIONS`.

---

## 8. Repository Verification (Actual State, Not Assumptions)

**Clean build:**
```
rm -rf /tmp/polaris_build_handoff
cmake -S ~/Documents/lin-opt -B /tmp/polaris_build_handoff --fresh → Configuring done, Generating done
cmake --build /tmp/polaris_build_handoff → 100% Built polaris, polaris_real, polaris_tests, test_real_providers, test_parsers, test_readonly, polaris_p3, test_baseline, polaris_p4, test_p4_security, polaris_p5, test_comparison, test_post_change, test_regression, test_observed_benefit, test_p12_stale, test_p12_idempotency, test_p12_statemachine, test_p12_transaction_model, test_p13_profile_model, test_p13_profile_store, test_p13_profile_service, test_p13_profile_advisor, test_p14_ipc_protocol, test_p14_ipc_auth, test_p14_socket_security, test_p14_ipc_server, test_p14_lock, test_p14_ipc_security, test_p14_recovery, test_p15_lifecycle, test_p15_stale_matrix, test_p15_toctou_idempotency, test_p15_lock_recovery, test_p15_regression_audit, test_p16_explanation_model, test_p16_explain_candidate, test_p16_explain_transaction, test_p16_verbose_redaction
```

**Complete test suite (33 tests):**
```
ctest --test-dir /tmp/polaris_build_handoff --output-on-failure
1/33 unit                      Passed 0.00s
2/33 real_providers            Passed 0.04s (OS prettyName, CPU cores 4, mem total, fs hasRoot, block >0, thermals 20, gpus 2)
3/33 parsers                   Passed 0.00s (os-release VARIANT_ID kde, meminfo 11968360, boot 3.275s)
4/33 readonly                  Passed 0.00s (stat /etc/fstab mtime unchanged)
5/33 p4_security               Passed 0.01s (9 checks: traversal, symlink, metachars, invalid transition, replay, backup no overwrite, oversized, fake op, audit hash chain)
6/33 comparison                Passed 0.00s (12 categories: normal improvement, unchanged, regression boot/memory/thermal/failed, unavailable, zero-before, boundary, just above, expected vs observed, serialization, P7 fixture)
7/33 post_change               Passed 0.01s (fixtures /tmp/polaris-test-root/post_change, no real-host mutation)
8/33 regression                Passed 0.00s (5 thresholds: boot +40%, mem -1.5GB, thermal +20C, failed 0→1, no regression)
9/33 observed_benefit          Passed 0.00s (SUCCESS vs NO_CHANGE vs REGRESSION)
10/33 p12_stale                Passed 0.01s (10 cats: valid, beforeHash, unitHash, kernel, target, operation, packageState, precondition, final, no_mut, TOCTOU)
11/33 p12_idempotency          Passed 0.01s (5 cats: approval bound, mismatch, duplicate id, COMPLETED re-apply, repeated verify, duplicate approval)
12/33 p12_statemachine         Passed 0.00s (illegal 11 + valid recovery + terminal)
13/33 p12_transaction_model    Passed 0.01s (backward compat, audit stale/idempotency)
14/33 p13_profile_model        Passed 0.01s (6 cats: default unknown, explicit, deterministic, round-trip, missing, malformed)
15/33 p13_profile_store        Passed 0.01s (6 cats: atomic 0600, symlink, traversal, malformed, deterministic, real profile not touched)
16/33 p13_profile_service      Passed 0.01s (6 cats: explicit, no inference, audit, idempotent, unknown field, explicit vs unknown)
17/33 p13_profile_advisor      Passed 0.00s (12 cats: KMail/Kontact/Akonadi/KOrganizer blocked, unknown requires, no allowed, Bluetooth/Printing/Avahi blocked, never becomes approval, no host mutation, explainability)
18/33 p14_ipc_protocol         Passed 0.00s (12 cats: protocol accepted, unsupported, ping, malformed, oversized, truncated, unknown op, arbitrary exec, shell, traversal, NUL, oversized arg)
19/33 p14_ipc_auth             Passed 0.01s (5 cats: same-user authorized, wrong UID rejected, unavailable, spoofed, disconnected)
20/33 p14_socket_security      Passed 0.00s (5 cats: permission 0600 not world-writable, symlink rejection, traversal/shell, stale detection, parent symlink)
21/33 p14_ipc_server           Passed 0.36s (4 cats: ping via client/server, info, timeout, concurrent)
22/33 p14_lock                 Passed 0.07s (5 cats: acquisition, contention, release, symlink, concurrent)
23/33 p14_ipc_security         Passed 0.01s (12 cats: audit, no password, authenticated≠approved, cannot bypass StateMachine/Validator, no sh -c, no exec, no password, no traversal, no symlink, no oversized, no privilege assumption)
24/33 p14_recovery             Passed 0.01s (4 cats: incomplete detected, recovery fails closed, no host mutation, smoke)
25/33 p15_lifecycle            Passed 0.00s (28 cases valid/rejected, stale→FAILED, unverifiable→FAILED)
26/33 p15_stale_matrix         Passed 0.01s (19 cases 7 fields ×3 states)
27/33 p15_toctou_idempotency   Passed 0.01s (TOCTOU+idempotency)
28/33 p15_lock_recovery        Passed 0.08s (lock/recovery/rollback)
29/33 p15_regression_audit     Passed 0.01s (regression 16+FileSafety 12+Audit)
30/33 p16_explanation_model    Passed 0.01s (6 cats: serialization, deterministic, ordering, verbose, JSON, redaction)
31/33 p16_explain_candidate    Passed 0.00s (7 cats: WHY NOW, WHAT WILL CHANGE, WHAT WILL NOT CHANGE, rejection, profile-blocked, unknown, JSON)
32/33 p16_explain_transaction  Passed 0.00s (8 cats: expected vs observed, regression, FAILED, rollback, completed, stale-preview, authorization, no mutation)
33/33 p16_verbose_redaction    Passed 0.00s (8 cats: verbose, redaction, deterministic, completed, stale, authorization, no mutation, JSON)
100% tests passed, 0 failed 0.70s
```

**Relevant security tests:** `p4_security` 9 checks **PASS** - **fail closed**, `p12_stale` TOCTOU + final **PASS**, `p12_statemachine` illegal **PASS**, `p13_profile_service` audit/idempotent **PASS**, `p13_profile_advisor` never becomes approval **PASS**, `p14_ipc_protocol` 12 **PASS**, `p14_socket_security` 0600 **PASS**, `p14_lock` flock **PASS**, `p14_recovery` fail-closed **PASS**, `p15_lifecycle` 28 **PASS**, `p15_regression_audit` FileSafety 12 **PASS**, `p16_explanation_model` deterministic **PASS**, `p16_verbose_redaction` redaction **PASS**.

**Inspect git diff/status:**
```
git -C ~/Documents/lin-opt` → `fatal: not a git repository` (not a git repo, as verified `ls -ld` shows 11 dirs, `ls -R` has `core/` `cli/` `tests/` `docs/` etc., but no `.git` - `ls -la ~/Documents/lin-opt | grep git` 0)
```
`find ~/Documents/lin-opt -type f -name "*.cpp" -o -name "*.h" | xargs ls -lt` shows recently modified `core/profile/UserProfile.h` (P13), `ProfileStore.*`, `ProfileService.*`, `ProfileAdvisor.*`, `cli/p4_cli.cpp` (profile), `core/safety/FileSafety.h` profile allowlist, `tests/*p13*` - **only intended P13 engineering changes**, no `/etc` writes.

**Verify generated artifacts:**
- `ls -lh ~/Documents/lin-opt/docs/*.md` 28 files: `P2_REPORT.md` 24K, `P3_REPORT.md` 33K, `P4_REPORT.md` 18K, `P5_REPORT.md` 13K, `P6_REPORT.md` 21K, `P6_EXECUTION_REPORT.md` 11K, `NVIDIA_PREFLIGHT_REPORT.md` 20K, `P7_PRE_REBOOT_REPORT.md` 20K, `P7_POST_REBOOT_REPORT.md` 12K, `P8_REPORT.md` 18K (+addendum), `P9_REPORT.md` 15K, `P10_PLAN.md` 24K, `P11_POST_CHANGE_MEASUREMENT.md` 9.7K, `P12_PLAN.md` 15K, `P12_IMPLEMENTATION_REPORT.md` 12K, `P13_PLAN.md` 15K, `P13_IMPLEMENTATION_REPORT.md` 12K, `P14_PLAN.md` 19K, `P14_IMPLEMENTATION_REPORT.md` 15K, `P15_PLAN.md` 12K, `P15_IMPLEMENTATION_REPORT.md` 15K, `P16_PLAN.md` 12K, `P16_IMPLEMENTATION_REPORT.md` 15K, `ARCHITECTURE.md` `API.md` `TRANSACTION_MODEL.md` updated, `PROJECT_HANDOFF.md` (this), `PROJECT_STATE.json`, `.github/workflows/ci.yml`.
- `ls -lh ~/Documents/lin-opt/*.json` `p2_scan.json` 9.9K, `p3_analysis.json` 23K, `p4_security_report.json` 2.0K, `p5_transaction.json` 6.8K, `p6_mssql_analysis.json` 16K, `p7_post_reboot.json` 6.3K, `p8_analysis.json` 14K, `p9_analysis.json` 11K, `nvidia_preflight.json` 17K, `p6_additional_analysis.json` 7.2K.
- `ls -R ~/Documents/lin-opt` 11 dirs: `api`, `cli` (5 binaries: `main.cpp` `p3_cli.cpp` `p4_cli.cpp` `p5_pilot.cpp` `real_scan.cpp`), `core` (domain `Comparison.h` new, `PerfModels.h`, `HardwareInfo.h`, engines `comparison` new + `perf`/`bottleneck`/`benchmark`/`recommend`, `providers` `IProvider.h` `real` `mock`, `safety` `ReadOnlyGuard` `FileSafety` `StateMachine` `Transaction` `TransactionValidator` `TransactionStore` `BackupEngine` `AuditLog`, `profile` `UserProfile` `ProfileStore` `ProfileService` `ProfileAdvisor`), `daemon`, `docs`, `gui` (empty, future Qt), `packaging`, `polkit`, `tests` (unit `test_comparison` 12 cats + `test_p13_*`, integration `test_post_change` etc., security `test_p12_*` + `test_p13_profile_service`).

**Verify docs consistency:** `cat docs/ROADMAP.md` shows `P1` … `P13` `P13 User Workflow / Profile Engine - COMPLETE 2026-09-01 04:30`, `cat docs/ARCHITECTURE.md` now includes `P13 User Workflow / Profile Engine` layer, `cat docs/TRANSACTION_MODEL.md` note profile is constraint not approval, `cat docs/PROJECT_STATE.json` `currentPhase P13`.

---

## 9. Known Limitations (Do Not Hide)

- **Unavailable metrics:** Represented explicitly as `available false` `note` not guessed - e.g., `systemd.userspace` 0 → unavailable, `thermal` 0 → unavailable, `nvidia.claimed` not collected → unavailable. P11 does not invent `boot 54.106` if `systemd-analyze` fails (would be `unavailable: systemd userspace not collected`).
- **Reboot-pending:** Framework supports `rebootMarker` but P11 does not auto-capture after reboot - user must run `polaris transaction compare` after reboot to generate `afterBaseline` (as done for P7 manual `bash` post-reboot, now via `ComparisonEngine`).
- **Boot vs login:** Distinguishes `isBootCritical` vs `background`, but `login` time (`systemd-analyze --user` 596ms) is not yet in `PerformanceBaseline` (only `systemd.userspace` system) - future could add `userSystemd` metric.
- **Provider fragility:** `RealSystemdProvider` still uses `execv` `systemctl` `systemd-analyze` fallback, not yet `sd-bus` native; `RealGpuProvider` `glxinfo` `DISPLAY=:0` hack fragile headless - should use `libEGL` native.
- **Benchmark synthetic:** `BenchmarkEngine` `cpu_prime` is micro-benchmark, not real workload - documented as `prime compute 2..2000`, not system load.
- **No helper installed:** `ls /run/polaris/helper.sock` not exists (correct for P11/P12/P13, helper design only, not installed).
- **Not a git repo:** No `branch`/`commit` - version is `0.1.0` `P13 COMPLETED`, not via git.
- **P12 generic preconditions mocked in tests:** Real host collection of `service enabled/active`, `config hash`, `packageStateHash`, `kernelVersion` via `Real*Provider` not yet wired to `CurrentState` (future P14).
- **P12 concurrent lock flock not yet implemented:** `TransactionStore::create` rejects duplicate via file existence, but no `flock /run/polaris/transaction.lock` multi-process test yet (future P14).
- **P12 crash recovery `recover` not yet implemented:** `FAILED` transitions for stale exist, but no `polaris transaction recover` for `BACKUP_CREATED`/`APPLYING` incomplete (future P14).
- **P13 profile not yet integrated into RecommendationEngine ranking:** `ProfileAdvisor` standalone constraint, not auto re-ranking `p8/p9_analysis.json` candidates (future P17 Campaign2 will use `usesBluetooth=no` to raise confidence).
- **P13 no UI interview layer:** CLI `profile show/set` only, no interactive `question` UI (future P16 HCI).
- **P13 no profile versioning/migration:** Missing keys decode as `unknown`, forward compatible, but no explicit `version` field.
- **P14 no privileged mutation via IPC:** `transaction.apply` disabled/rejected, allowlist `ping`/`info` only, helper defined but not installed (`/run/polaris/helper.sock` not exists, tests use `/tmp/polaris-test-root/p14`); lock advisory `flock` tested but not yet integrated into `TransactionStore::apply` for real `/run/polaris/transaction.lock`; recovery detection-only, suggested `FAILED`, no auto-replay; `SO_PEERCRED` Linux-specific fail-closed.
- **P15 CI minimal:** `cmake --fresh`/`cmake --build`/`ctest` only, no `clang-tidy`/`sanitizers`/`coverage`; `TransactionValidator` fail-closed fix for `UNAVAILABLE` now correctly rejected; fixtures isolated `/tmp/polaris-test-root/p15`.
- **P16 explainability read-only:** `Explanation` deterministic, no host mutation, verbose redaction `[REDACTED]`, `WHY NOW`/`WHAT WILL NOT CHANGE` scope-aware, `Comparison` `expected` vs `observed` distinction, never confuses `approval` with `authorization`.
- **P18 reporting only:** `FINAL_REPORT.md` `15K` + `FINAL_STATE.json` (phase `P18` `status` `COMPLETED` `verdict` `PROJECT_COMPLETE_WITH_LIMITATIONS`) - no host mutation, `loadAvg`/`PSI`/`journal` `NOT MEASURED` for `P18` final validation, `failedCount` `1→1` but different unit `mssql` `1→0` vs `drkonqi` `0→1` → `INCONCLUSIVE` for `failed` but `mssql` itself `0` is not regression, `zram` `0B` stable.

---

## 10. Next Action (Exactly ONE Next Engineering Phase)

**Next Phase:** **P18 - Final Benchmark / ROI / Stability Report**

**Title:** `P18 - Final Benchmark / ROI / Stability Report` (Consolidate `before`/`after` `observedBenefit` vs `expectedBenefit` for all completed transactions `P7` `mssql` etc.)

**Why this is next (highest-value after P17):** P17 Campaign2 concluded `NO_ACTION_RECOMMENDED` (no worthwhile candidate); P18 final benchmark will consolidate `observedBenefit` vs `expectedBenefit` for `P7` `mssql` `nvidia` `fstab` etc.

**Objective:** Consolidate `FINAL_REPORT.md` with `observedBenefit` `delta -84%` `713M` `PRIME` etc. for all completed transactions.

**Status:** **Not yet implemented** - planning only, now ready for `P18` implementation after explicit approval.

**Implementation vs Real-Host Optimization:** **Implementation** (report, no host mutation, `Comparison` `observedBenefit` vs `expectedBenefit`).

**Explicit Approval Required:** **No** (report only, no host mutation).

**Reboot May Be Required:** **No** for P18 (report only).

**Host Mutation Allowed:** **No** (report only).

**Acceptance Criteria:** `docs/FINAL_REPORT.md` with `observedBenefit` for all completed transactions, `P7` `before 54.106s` `after 8.515s` `delta -84%`, `mssql` `713M` `0`, `nvidia` `CLAIMED`.

**But do NOT implement P18 now** - **STOP and report this handoff, await explicit approval for P18 implementation prompt separately.**

---

## Format

**Human-readable:** This `docs/PROJECT_HANDOFF.md` (detailed, 10 sections, actual verified values, not assumptions).

**Machine-readable:** `docs/PROJECT_STATE.json` - structured state (see next file, min 9 keys: `currentPhase` `currentStatus` `nextPhase` `completedPhases` `remainingPhases` `hostState` `optimizationCandidates` `safetyInvariants` `artifacts` `tests` `knownLimitations`).

**JSON must contain actual verified values - not fabricated, use `null`/`unknown`/`not_verified` where necessary.**

**Validation:** After creating both files, `cat docs/PROJECT_HANDOFF.md` `head -n 50` + `python3 -m json.tool docs/PROJECT_STATE.json` `head -n 100` + `ls -lh` `ctest` 33/33.

---

## Final Stop

After creating and validating `docs/PROJECT_HANDOFF.md` and `docs/PROJECT_STATE.json`:

**STOP.**

**Do not implement P18.**
**Do not modify host.**
**Do not perform any optimization.**

**Next session will start by reading `docs/PROJECT_HANDOFF.md` + `docs/PROJECT_STATE.json` and then receive `P18` implementation prompt separately.**

