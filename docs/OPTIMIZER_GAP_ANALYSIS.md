# Polaris Optimizer Gap Analysis — Read-Only Product Audit

**Date:** 2026-09-01 (UTC)
**Mode:** READ-ONLY — repository inspection only. No host mutation, no `sudo`, no `systemctl`, no `dnf`, no IPC helper, no profile write. Verified `stat /etc/fstab` unchanged, no `/run/polaris/*`, no `~/.local/state/polaris/profile.json` created.
**Scope:** P1–P18 inclusive. Verdict under audit: `PROJECT_COMPLETE_WITH_LIMITATIONS` (`docs/P18_FINAL_REPORT.md:6`, `docs/PROJECT_STATE.json:2`).
**Question:** Is Polaris currently a *real optimizer* or only a *safe framework*?

---

## 1. Executive Summary

**Polaris is not currently a real optimizer. It is a production-grade safety/evidence/transaction framework with a deliberately conservative, hard-coded, and largely exhausted optimization catalog.**

- **What works:** The safety pipeline `READ→MEASURE→ANALYZE→EXPLAIN→RECOMMEND→PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY→COMPARE→REGRESSION→AUDIT` (`docs/ARCHITECTURE.md:5`) is intact, deterministic, and well-tested (33/33). Two historically worthwhile real-host mutations were executed and verified — `mssql-server disable` (P6) and `NVIDIA 470xx migration` (P7) — both one-time, high-signal fixes.
- **What does not:** After those fixes, the remaining optimization surface on a healthy Fedora 44 / KDE 6.7 / MX130 host is near zero. `RecommendationEngine` (`core/engines/recommend/RecommendationEngine.cpp:1`) is a static `if(bn.id==...)` mapper emitting ~8 hard-coded recommendations with stringly-typed benefits (`"Save 713M"`) and literal confidences (`0.96f`). It has no numeric benefit model, no provider registry, and no quantitative scoring. `TransactionManager` (`core/safety/transaction/TransactionManager.cpp:1`) is a stub; real privileged `APPLY` is intentionally disabled — `IpcProtocol::allowedOperations` is `ping`/`info` only (`core/ipc/IpcProtocol.cpp`, `docs/P14_IMPLEMENTATION_REPORT.md:5`). The CLI (`cli/main.cpp:1`, `cli/p4_cli.cpp:1`) cannot preview or apply real-host system optimizations beyond test fixtures (`/tmp/polaris-test-root`) plus two allowlisted user files.
- **`P17 NO_ACTION_RECOMMENDED` was correct.** Not because the engine is buggy, but because (a) the host *is* already optimized for Boot/Memory/NVIDIA, (b) the candidate set is too small and exhausted, and (c) every remaining candidate is either `BLOCKED_BY_USER_WORKFLOW` / `REQUIRES_USER_CONFIRMATION` (`core/profile/ProfileAdvisor.cpp:1`) or has negligible measured benefit (`5–10 MB`, `0 s` boot-critical).
- **Product vs. safety gap:** Safety is solved. Usefulness is not. The framework can guarantee “we will not break you” but cannot currently answer “we found something worth your time.”
- **Recommendation:** A next phase **is justified**, but not as “add 100 tweaks.” The justified phase is a **capability-framework phase** that turns Polaris from a collection of hard-coded cases into an extensible, measurable optimizer that can discover a small number of high-confidence, reversible opportunities on *any* generic Fedora/KDE host. If that framework phase is not pursued, the honest status is: *Polaris should remain a safe measurement/explainability tool, not marketed as an optimizer.*

---

## 2. Original Product Goal

> `Polaris should be a safe, evidence-backed Linux system optimizer that identifies worthwhile optimization opportunities, explains them, lets the user approve them, applies one controlled change at a time, and verifies whether the system actually improved.`

Required properties:

1. **Finds worthwhile opportunities** — not just lists everything; must be worth applying.
2. **Explains** — `WHY NOW`, `WHAT WILL CHANGE`, `WHAT WILL NOT CHANGE`, `REJECTION CONDITIONS` (`core/explainability/ExplanationEngine.h:1`, `docs/P16_IMPLEMENTATION_REPORT.md:3`).
3. **User approves** — `PREVIEW→APPROVAL_REQUIRED→APPROVED` with hash binding (`core/safety/transaction/TransactionValidator.h:114`, `core/safety/transaction/TransactionStore.h:75`).
4. **One change at a time, with backup** — `BackupEngine::create` versioned SHA-256 no-overwrite (`core/safety/backup/BackupEngine.h:22`, `core/safety/FileSafety.h:96`), `TransactionLock` `flock` exclusive (`core/safety/lock/TransactionLock.h:1`).
5. **Verifies** — `ComparisonEngine::compare(before,after,expectedBenefit)` (`core/engines/comparison/ComparisonEngine.h:12`) with stored thresholds (`boot +10%`, `memory -1 GiB`, `thermal +15 C`, `new failed`) and verdict `SUCCESS`/`REGRESSION`/`NO_CHANGE` (`core/domain/Comparison.h:12`).

P18 preserved all five, but property (1) is currently hollow on a host that has already had its two high-signal fixes.

---

## 3. What Polaris Can Optimize Today

### 3.1 Precise Inventory of Real Host Mutations Currently Reachable

| # | Target | Operation (code path) | Preconditions (enforced) | Expected Benefit (as coded) | Risk | Rollback | Verification | CLI Reachable? | Exercised on Real Host? |
|---|--------|------------------------|--------------------------|-----------------------------|------|----------|--------------|---------------|--------------------------|
| **1** | `~/.config/autostart/nvidia-settings-user.desktop` | `FileSafety::atomicWrite` `Hidden=true` (`core/safety/FileSafety.h:96`, `cli/p5_pilot.cpp`) | exists, regular file, not symlink, owned by `1000`, size 101, contains `nvidia-settings`, `allowlist` (`FileSafety.h:18,27,60`) | `2.56 s` faster login (avoid `nvidia-settings` error) — `RecommendationEngine.cpp:25` says “Reclaim …” but P5 pilot says 2.56 s | `R1` | `atomicWrite` temp+fsync+rename; restore `Hidden=false` via same method; `BackupEngine` backup if file existed | `is_regular_file`, `sha256`, `ps aux` no `nvidia-settings` | **Yes** — `polaris_p5` pilot (`cli/p5_pilot.cpp`) + `polaris_p4 transaction preview` test fixture path only | **Yes** but `NO_OP` — file already `Hidden=true` (`docs/P18_FINAL_REPORT.md:18`, `p5_transaction.json`) — no host delta; rollback test via `/tmp` copy passed |
| **2** | `mssql-server.service` | `systemctl disable mssql-server.service` (historical direct `systemctl`; framework now would be `TransactionStore::apply` simulated service state + future `org.polaris.disable.mssql` helper) | `systemctl is-enabled==enabled`, `is-active==failed` (25 boots), `status 18`, `713 M` `9.192 s`, `localhost:1433` 0 hits, `DB_CONNECTION=mysql`, `model.mdf` Windows path — 12 evidences (`p6_mssql_analysis.json`, `docs/P6_REPORT.md`) | `Save 713 M + 9.192 s per boot, clean failed unit` (`RecommendationEngine.cpp:42`) | `R2` | `systemctl enable --now mssql-server` — `rollbackState AVAILABLE` | `is-enabled disabled`, `is-active inactive`, `systemctl --failed` not `mssql`, `ss -tuln` no 1433, `free SwapUsed 1.6→0`, `ComparisonEngine` `boot.userspace` not regressed | **No** — `IpcProtocol::allowedOperations` is `ping`/`info` only; `polaris_p4` cannot `disable` real service today (`core/ipc/IpcProtocol.h`, `docs/P14_PLAN.md:9`). `TransactionStore::apply` would simulate but not call `systemctl` for real host. | **Yes** — P6 real `systemctl disable` (`docs/P18_FINAL_REPORT.md:16`), verified after P7 reboot `0 failed`, `audit backup.created → apply.disable → verification.passed → COMPLETED` |
| **3** | `GM108M [GeForce MX130] [10de:174d] 01:00.0` | `dnf swap --allowerasing akmod-nvidia → akmod-nvidia-470xx` + `dnf swap xorg-x11-drv-nvidia-libs` + `akmods --force` + `dracut --force` + reboot | `lspci UNCLAIMED`, `modinfo` not `nvidia`, `nvidia-smi` failed, `NVRM 490` `GSP`, `lsmod nouveau+i915`, `SecureBoot disabled`, `kernel-devel 7.1.10` can build, `GM108M` in 470xx README `174D`, `rpm -qa` 16pkgs (P7 preflight `nvidia_preflight.json`) | `Restore PRIME offload, eliminate 26–99 journal errors/boot, enable nvidia-powerd, CUDA/Offload` (`RecommendationEngine.cpp:23`) | `R3` | `dnf swap akmod-nvidia-470xx → akmod-nvidia` + rebuild (`RecommendationEngine.cpp:26`, `docs/P7_POST_REBOOT_REPORT.md`) — medium | `modinfo 470.256.02` `extra/nvidia-470xx/nvidia.ko.xz` 25 M, `lspci CLAIMED driver=nvidia`, `lsmod nvidia`, `nvidia-smi 470.256.02 52 C`, `__NV_PRIME=1 glxinfo NVIDIA GeForce MX130`, `journal NVRM 1` vs 490 | **No** — not exposed via `polaris_p4`; would require privileged helper action `org.polaris.swap.nvidia` which does not exist; P7 was manual `dnf`/`akmods`/`dracut` | **Yes** — P7 manual (`docs/P18_FINAL_REPORT.md:17`), 15 post-reboot checks PASS |
| **4** | `/etc/fstab` (`# UUID 39b0 swap` stale) | `FileSafety::atomicWrite` commenting swap line (test fixture only today) | `findmnt --verify` parse errors, swap UUID stale, `FileSafety::validatePath` allowlist `/etc/fstab` (`FileSafety.h:23`), `canonical`/`isSymlink` TOCTOU (`TransactionValidator.h:334`) | `Eliminate swap timeout` (REC-005 `Verify prior fixes remain effective` — already fixed P2) | `R2` (fstab) | `Restore backup ~/.local/state/polaris/backups/<tx>/fstab.bak` `SHA-256` `fsync` `BackupEngine.cpp` | `findmnt --verify 0 parse errors`, `systemd-analyze userspace` not 90 s timeout, `ComparisonEngine` `boot` not regressed | **No for real host** — `polaris_p4 transaction preview fstab-stale-swap` writes only `/tmp/polaris-test-root/etc/fstab`; real `/etc/fstab` would be helper-only and is intentionally not wired | **No** since P2 — already fixed 2026-08-31 21:19 (`docs/P18_FINAL_REPORT.md:20`), verified `stat` unchanged |
| **5** | `/tmp/polaris-test-root/etc/fstab` + `.../etc/test.conf` | `FileSafety::atomicWrite` via `TransactionStore::apply` (`TransactionStore.h:358`) | `FileSafety::isAllowedPath` `/tmp/polaris-test-root/` (`FileSafety.h:20`), `validatePath` no `..`/`;|&` etc. (`FileSafety.h:49`), `isSymlink` check, `StateMachine BACKUP_CREATED→APPLYING` (`TransactionStore.h:338`) | Test only — `Eliminate swap timeout (test)` (`cli/p4_cli.cpp:55`) | `R2` test | `BackupEngine::create` versioned no-overwrite; second `create` same id throws `already exists` | `sha256File`, `is_regular_file`, `audit apply.completed`, `Comparison` fixtures | **Yes** — `polaris_p4 transaction preview dummy-test` / `apply --dry-run` (`cli/p4_cli.cpp:146`) | **Yes** — exercised in every `test_p12_*`, `test_p15_*` via fixtures (`/tmp/polaris-test-root`) — 33/33 pass |
| **6** | `~/.local/state/polaris/profile.json` | `ProfileStore::save` `atomic tmp+fsync+chmod 0600+rename` (`core/profile/ProfileStore.cpp`, `FileSafety.h:27`) | `FileSafety::validatePath` profile allowlist (`FileSafety.h:42,63`), no `..`/`;|&`, symlink check, `create_directories` `0600` | `Avoid breaking KMail/Kontact/Bluetooth` — enables `ProfileAdvisor` (`ProfileAdvisor.cpp:5`) | `R0` (user workflow) | `save` previous JSON (idempotent check `profile.update.idempotent` — `ProfileService.cpp`), `remove` | `load` missing→`unknown` no auto-create (`ProfileStore.cpp`), `toJson` deterministic sorted, `audit profile.updated` | **Yes** — `polaris_p4 profile set <field> yes/no/unknown [--json]` (`cli/p4_cli.cpp:204`) | **No on real host during P18** — `ls ~/.local/state/polaris/profile.json` `No such file` (`docs/P18_FINAL_REPORT.md:58`) — intentionally not mutated; tests use `/tmp/polaris-test-root/profile.json` |
| **7** | `flatpak` runtimes (25.08 + 50 duplicates) | Not implemented — only recommendation text `REC-006` `flatpak list` (`RecommendationEngine.cpp:72`) | `flatpak list` shows duplicates (manual prior, not in `BaselineEngine::collect` — `BaselineEngine.h:1` does not collect `flatpak`) | `Reclaim 1–2 GB disk` (`RecommendationEngine.cpp:75`) | `R1` | `flatpak install` | `flatpak list`, `df -h` free? `StorageBaseline` collects `statvfs` (`BaselineEngine.h:64`) — could verify | **No** — no transaction template, no `flatpak uninstall` helper | **No** |
| **8** | `zram`/`swap` do-not-disable | Negative recommendation `REC-007` — explicitly tells to *not* optimize (`RecommendationEngine.cpp:82`) | `MemAvailable 4.4 GB`, `pressure 0`, `zram disksize 8G lzo-rle` (`BaselineEngine.h:52`) | `Avoid stability regression` | `R0` | `N/A` | `zramctl 8G DATA 4K COMPR 80B TOTAL 12K 0B used`, `free`, `pressure` | N/A | N/A — correctly not applied; verified `zramctl 0B used` (`docs/P18_FINAL_REPORT.md:57`) |
| **9** | Reboot to measure | `REC-008` `reboot` — not a mutation, just measurement (`RecommendationEngine.cpp:91`) | `systemd-analyze userspace 54.106 s` before fixes, fixes applied 2026-08-31 21:15 | `Projected -10 to -15 s userspace` | `R0` | `N/A` | `systemd-analyze` before/after via `ComparisonEngine` | **No** — `polaris_p4` cannot reboot; `rebootRequired` flag only (`Transaction.h:47`) | **Yes** — P7 reboot 00:36 (`docs/P18_FINAL_REPORT.md:17`) was manual, not via Polaris |

**Summary count:** Polaris today can *actually* mutate **2 user-visible host locations** via code (`autostart` file + `profile.json`) plus **1 privileged system file** (`/etc/fstab`) that is allowlisted but deliberately not wired to CLI, plus **2 historically manual service/driver operations** that are not reachable through the current hardened IPC. All other “optimizations” are explanation/recommendation strings, not executable capabilities. Test-fixture mutations (`/tmp/polaris-test-root`) are abundant (33 tests) but do not count as optimizer capability.

Confidence of inventory: **High** — verified via `core/safety/FileSafety.h:18`, `core/ipc/IpcProtocol.cpp` allowlist, `core/safety/transaction/TransactionManager.cpp:1` stub, `cli/p4_cli.cpp:47` fixture-only preview, and `docs/P18_FINAL_STATE.json` real-host state.

---

## 4. What Polaris Cannot Optimize Today

Evaluated against the requested domains. “Cannot” means: no provider collects evidence, no bottleneck emits `Bottleneck`, no `Recommendation` exists, no `Transaction` template, no verification metric.

| Domain | Current Coverage | Why Not Optimizable Today | What Evidence Is Missing |
|--------|----------------|---------------------------|--------------------------|
| **Boot optimization** | Partial — `systemd-analyze` + `critical-chain` + `blameTop` + `classified` (`BaselineEngine.h:132`, `BottleneckEngine.cpp:156`) | Can *detect* `dnf-makecache` `2 m 28 s` but correctly classifies as `parallel/background` not blocker (`BottleneckEngine.cpp:34`). No timer rescheduling, no `systemd mask/disable` for boot units, no `boot loader` tuning. | `systemd-analyze plot`, `unit` `After`/`Wants` graph, `OnCalendar` for timers, `RequiredBy` for disable safety |
| **systemd services** | `getFailedServices` only (`RealSystemdProvider.h:18`). No `is-enabled`/`is-active` collection for arbitrary units. | Only `mssql` and `failed` units examined. No generic service disable candidate. P17 manually did `systemctl is-enabled bluetooth/avahi/cups` outside baseline. | Per-unit `is-enabled`, `is-active`, `unitHash` snapshot (`Transaction.h:63`), dependency graph (`p6_additional_analysis.json` pattern) |
| **User services** (`systemd --user`) | None — `systemd-analyze --user 596 ms` mentioned in reports but not in `PerformanceBaseline` (`PerfModels.h:122`) | No `systemd --user` provider. Akonadi is user-service-like but handled via `ps` + `akonadictl` manual. | `systemctl --user is-enabled`, `akonadi`, `pipewire`, `xdg-desktop-portal` |
| **Timers** | `blameTop` includes timer-triggered services but no `timer` object. `dnf-makecache.timer`, `plocate-updatedb.timer` examined manually in P17 (`docs/P17_REPORT.md:27`). | No `RealTimerProvider`. Cannot propose `TimerAccuracySec` or `OnCalendar` shift. | `systemctl cat <timer>.timer` `OnCalendar`, `AccuracySec`, `Persistent` |
| **Startup/autostart** | `~/.config/autostart/nvidia-settings-user.desktop` only (`docs/P17_REPORT.md:30`, `FileSafety.h:27`). No enumeration of `/etc/xdg/autostart` 30+ entries. | One-file pilot. No generic `.desktop` `Hidden`/`X-KDE-autostart` enumeration. | `RealAutostartProvider` listing `~/.config/autostart/*.desktop` + `/etc/xdg/autostart/*.desktop` with `Exec`, `Hidden`, `OnlyShowIn` |
| **Memory pressure** | `RealMemoryProvider::get` collects `MemAvailable`, `pressureSome10`, `swapUsed`, `zramData` (`RealMemoryProvider.h:10`) — read-only | Correctly reports `pressure 0`, `MemAvailable 5.6 GiB`. No `vm.swappiness`/`vfs_cache_pressure` tuning (intentionally conservative — no benefit). | Per-cgroup pressure, `majorFaultsPerSec` already in `MemoryBaseline` but not populated with real delta |
| **zram/swap** | Collected (`RealMemoryProvider.h:69`), verdict `Do NOT disable` (`RecommendationEngine.cpp:82`) | Intentionally blocked — healthy `8 G lzo-rle DATA 4K 0B used` (`docs/P18_FINAL_REPORT.md:35`). No `zram-generator.conf` tuning. | `zram-generator.conf`, `swappiness 60`, `/etc/fstab` swap line (already fixed) |
| **CPU scheduling** | `RealCpuProvider::getCpu` collects `scaling_driver`, `governor`, `epp`, `no_turbo`, `curMhz` (`RealCpuProvider.h:14`) | No `governor`/`epp` recommendations. Correct — `powersave` `balance_performance` is Fedora default, not bottleneck (`BottleneckEngine.cpp:66`). | `cpupower`, `tuned`, `intel_pstate` `energy_performance_preference` history vs `thermal.throttling` |
| **I/O** | `StorageBaseline` `ioPressureSome10` (`BaselineEngine.h:79`), `block devices` via `/sys/block` (`RealStorageProvider.h:54`) | No `scheduler` tuning; `ioPressure 0` (`docs/P18_FINAL_REPORT.md:37`). `fstrim.timer` check only (`BaselineEngine.h:87`). | `queue/scheduler` `none`/`bfq`/`mq-deadline`, `fstrim` recency, `NVMe` `smart` (requires root, reported as `skipped` — `BottleneckEngine.cpp:93`) |
| **Filesystem** | `getFilesystems` `statvfs` + `scheduler` (`RealStorageProvider.h:16`, `BaselineEngine.h:63`) | No `flatpak`/`dnf cache`/`journal` size collection. `REC-006` flatpak text is not backed by `flatpak list` provider. | `flatpak list --app --runtime`, `du -sh /var/cache/dnf`, `journalctl --disk-usage`, `balooctl status` |
| **GPU/graphics** | `RealGpuProvider` `getGpus` + `getNvidiaState` + `getGlRenderer` (`BaselineEngine.h:92`) | `GPU-001` `UNCLAIMED` detection was excellent (one-time). After P7, `CLAIMED` `470.256.02` `PRIME` verified. No further GPU optimization (e.g., `modeset`, `powerMgt`). | `nvidia-persistenced`, `kwin compositor` `effects` (`KdeBaseline` already collects `effects` — `BaselineEngine.h:198` but not used for optimization) |
| **KDE/desktop startup** | `RealKdeProvider::getPlasmashellVersion`, `effects`, `XDG_SESSION_TYPE` (`BaselineEngine.h:197`) | Collects `plasmashell 6.7.4` but no startup delay analysis beyond `plasmalogin 7.301 s +1.211 s` (`docs/P17_REPORT.md:14`). No `kwinrc` compositing tuning. | `systemd-analyze --user`, `kcmshell` effects state, `~/.config/kwinrc` parser |
| **Networking** | `NetworkManager` via `dbus-broker` → `network.target` in `critical-chain` only. No `nmcli` provider. | P18 marks `NetworkManager` `not_verified` (`docs/P18_FINAL_REPORT.md:51`). No `WPA`/`DNS` optimization. | `nmcli g status`, `resolv.conf`, `ss -tuln` already used for `mssql:1433` but not generalized |
| **Package/cache maintenance** | None — `rpm -qa` subset hashing for `packageStateHash` exists in `TransactionValidator` (`TransactionValidator.h:17`) but not collected via provider. | No `dnf-makecache` disable, no `packagekit` background tuning, no `dnf autoremove`/`clean all`. Correctly avoided — `dnf-makecache` `0 s` boot (`docs/P17_REPORT.md:28`). | `dnf repolist`, `/var/cache/dnf` size, `packagekit` `blame` history (P3 `45 s` already improved) |
| **Background indexing** | No `baloo` provider. `plocate-updatedb` detected via `blame 21.111 s` but classified as `Nice 19 idle` (`docs/P17_REPORT.md:29`, `BottleneckEngine.cpp:177`). | Correct `NO_ACTION` — `plocate` not in `critical-chain` (`BottleneckEngine.cpp:175`). No `balooctl` handling. | `balooctl status`, `updatedb.conf` `PRUNENAMES`, `plocate` `OnCalendar` |
| **Logging** | `RealJournalProvider::getPriorityErrors` `journalctl -p 3 -b` `count 141 vs 254`, families `nvidia` (`BaselineEngine.h:167`) | Reports `p3 254→141` improvement but does not propose `journald` `SystemMaxUse`/`MaxRetentionSec` vacuum. | `journalctl --disk-usage`, `/etc/systemd/journald.conf` `SystemMaxUse` |
| **Power/performance profiles** | `CPU` `governor` `powersave`, `epp` `balance_performance` collected but not `power-profiles-daemon` or `tuned`. | No `powerprofilesctl` provider. Intentionally not tweaking — `thermal 60 C` healthy. | `powerprofilesctl get`, `tuned-adm active`, `thermald` |
| **Kernel-related** | `uname -r 7.1.10-200` in `Transaction` (`Transaction.h:65`), `cmdline rd.driver.blacklist=nouveau` checked in `GPU-001` evidence (`BottleneckEngine.cpp:47`) | No `sysctl` (`vm.swappiness 60`, `vm.vfs_cache_pressure 100` collected but not tuned), no `kernel cmdline` edit, no `modprobe` tuning. All R3+ and correctly avoided. | `sysctl -a`, `/etc/default/grub`, `/etc/modprobe.d` |
| **Application-specific** | Manual `DB_CONNECTION=mysql`, `akonadi 14 agents 1302 M`, `code/opencode` in `top` (`BaselineEngine.h:154`) | No `code`/`opencode` optimization. Correct — user workload, not system. | Per-app config |

**Pattern:** Discovery is broad for *measurement* but narrow for *optimization*. The only domains with evidence → recommendation → transaction wiring are GPU, one failed service, and two file disables. Everything else is “collect and report `INFO`/`R0`/`No action unless repeats.”

---

## 5. RecommendationEngine Audit

**File:** `core/engines/recommend/RecommendationEngine.h:1`, `core/engines/recommend/RecommendationEngine.cpp:1`
**Related:** `core/engines/bottleneck/BottleneckEngine.h:1`, `core/domain/PerfModels.h:124`, `core/domain/Comparison.h:52`

### 5.1 How Many Candidate Types Exist

**Static 8** — hard-coded `add(...)` calls:

- `REC-001` `GPU-001` `Replace NVIDIA open driver 610 with legacy 470xx` — `0.96` `R3`
- `REC-002` `BOOT-001/BOOT-002` `Investigate dnf-makecache timer schedule (do not auto-disable)` — `bn.confidence` passthrough `R2`
- `REC-003` `SVC-001` `Disable or repair mssql-server` — `0.88` `R2`
- `REC-004` `SVC-002` `Review Akonadi PIM (keep if KMail used)` — `0.65` `R2`
- `REC-005` `stale fstab / wait-online` `Verify prior fixes remain effective` — `bn.confidence`/`bn.risk` passthrough
- `REC-006` `Clean unused Flatpak runtimes (safe)` — `0.70` `R1` *always emitted even if not a bottleneck*
- `REC-007` `Do NOT disable zram/swap` — `0.95` `R0` *always emitted*
- `REC-008` `Reboot to measure prior Level2 boot gains` — `0.90` `R0` *always emitted*

Plus *implicit* candidates referenced only in `BottleneckEngine::analyze` but never mapped in `RecommendationEngine`: `MEM-001`, `MEM-002`, `STORAGE-001`, `THERMAL-001`, `BOOT-003`, `GPU-002`, `JOURNAL-001` (`BottleneckEngine.cpp:6–219`) — these produce bottlenecks with `potentialOptimization="DO NOT disable..."` or `"No optimization"` and are intentionally not turned into `Recommendation`s.

**Result:** ~4 actionable + 4 informational. After P6/P7, only `REC-004`, `REC-006`, `REC-007` remain relevant, and two of those are *negative* recommendations.

### 5.2 How Candidates Are Scored

**Not scored.** `generate()` (`RecommendationEngine.cpp:5`) iterates `bottlenecks` and emits `if(bn.id==...) add(...)`. No `score`, no `sort`, no `threshold` filter. Confidence is copied or literal. Risk is literal string `"R3"`/`"R2"`/`"R1"`/`"R0"`. Category is literal.

`docs/P17_REPORT.md:48` shows a *manual* scoring table (`expectedBenefit`, `confidence`, `risk`, `reversibility`, `userImpact`) but that table is **documentation**, not code — `RecommendationEngine` does not compute it. `docs/P18_FINAL_REPORT.md:98` acknowledges `7 rejected candidates` but `RecommendationEngine` itself does not reject; rejection happens downstream in `ProfileAdvisor` + `Comparison` + human judgment.

### 5.3 How expectedBenefit Is Calculated

**Not calculated. It is a static string.**

- `GPU-001`: `"Restore PRIME offload, eliminate 26-99 journal errors per boot, enable nvidia-powerd, allow CUDA/Offload"` (`RecommendationEngine.cpp:23`) — evidence-backed but not quantified (`26–99` came from `journalctl` historical count, not from a `beforeBaseline` delta).
- `SVC-001`: `"Save 713M RAM + 9s CPU per boot, clean failed unit, reduce journal spam"` (`RecommendationEngine.cpp:43`) — `713 M` from `ps` peak, `9.192 s` from `systemd-analyze blame` historical, not from `PerformanceBaseline` `memory.swapUsed` delta.
- `BOOT-001`: `"Clarify if blocking or parallel; if blocking, save ~5-30s boot"` (`RecommendationEngine.cpp:35`) — range estimate, not measured.
- `SVC-002`: `"Save ~600M RAM + I/O if PIM unused"` (`BottleneckEngine.cpp:52`, `RecommendationEngine.cpp:52`) — `600 M` is `akonadi` collective RSS estimate, not `availableKb` delta.
- `REC-006`: `"Reclaim 1-2GB disk, reduce metadata"` (`RecommendationEngine.cpp:75`) — no `statvfs` free delta computed.
- `REC-007`/`REC-008`: `"Avoid stability regression"` / `"Projected -10 to -15s userspace"` — `R0` informational.

**No formula** like `benefit = f(beforeMetric, afterMetric, confidence)`. `ComparisonEngine` *does* compute `delta`/`pctDelta` (`ComparisonEngine.h:30`) but `RecommendationEngine` never calls it; `Comparison` is only used post-change (`Transaction.h:58`).

### 5.4 How Risk Is Calculated

**Static literal, not computed.**

- `GPU-001` `R3` — hard-coded because `dnf swap` + `akmods` + `dracut` + reboot touches kernel driver stack (`RecommendationEngine.cpp:24`).
- `BOOT-001` `R2`, `SVC-001` `R2`, `SVC-002` `R2` — hard-coded “needs approval, reversible via `enable`”.
- `REC-006` `R1`, `REC-007/008` `R0` — hard-coded safe.
- `BottleneckEngine` `risk` field is similarly literal (`BottleneckEngine.cpp:36` `"R2"`).

No `risk = f(rollbackAvailable, requiresReboot, userImpact, dependencyGraph)` — though `p6_additional_analysis.json` *did* manually check dependency graph for `mssql`.

### 5.5 What Thresholds Suppress Recommendations

**None inside `RecommendationEngine` itself.** Suppression happens elsewhere:

- `BottleneckEngine` suppresses by not emitting `HIGH`/`CRITICAL` for healthy metrics: `MEM-001` `"INFO"` `pressure 0` (`BottleneckEngine.cpp:67`), `THERMAL-001` `max 60 C <85` → `"INFO"` (`BottleneckEngine.cpp:109`), `STORAGE-001` `"INFO"` (`BottleneckEngine.cpp:95`), `GPU-002` `"INFO"` `i915 claimed` (`BottleneckEngine.cpp:194`). These never become `Recommendation`s.
- `ProfileAdvisor` suppresses via `BLOCKED`/`REQUIRES` (`ProfileAdvisor.cpp:5`): `usesKMail=yes` → `BLOCKED_BY_USER_WORKFLOW` (`canConsiderAkonadi`), `usesBluetooth=unknown` → `REQUIRES_USER_CONFIRMATION`.
- `ExplanationEngine` *explains* suppression via `rejectionConditions` (`core/explainability/ExplanationEngine.cpp` `buildRejectionConditionsCandidate`) but does not itself suppress.
- `ComparisonEngine` suppresses via regression thresholds `defaultThresholds()` `bootPct 10%`, `availableMemGb 1.0`, `thermalC 15.0` (`ComparisonEngine.h:21`) — but only post-change.
- **Implicit product threshold:** P17 human judgment “`5–10 M` tiny vs `R2` not worthwhile, `0 s` boot-critical not worthwhile” (`docs/P17_REPORT.md:59`) — not coded.

There is no `benefit < threshold → no Recommendation` gate in code. `RecommendationEngine` will happily emit `REC-006`/`REC-007`/`REC-008` even when `availableKb` is `5.6 GiB` and `zram` is healthy.

### 5.6 Whether Candidates Are Hard-Coded

**Yes, entirely.** `RecommendationEngine::generate(const PerformanceBaseline& b, const vector<Bottleneck>& bottlenecks)` (`RecommendationEngine.h:9`) ignores `b` except for `void(b)` (`RecommendationEngine.cpp:6`) and switches on `bn.id` string equality. No registry, no `IProvider` for optimizations, no JSON/YAML, no `Optimization` domain object (`core/domain/Transaction.h:30` `struct Optimization` exists but is *not used* by `RecommendationEngine` — it is a separate P1 domain stub).

Adding a new optimization requires editing `BottleneckEngine.cpp` to emit a new `Bottleneck` *and* `RecommendationEngine.cpp` to map it, plus manually choosing confidence/risk strings.

### 5.7 Whether New Optimization Providers/Actions Can Be Added Cleanly

**No.**

- No `IOptimizationProvider` or `OptimizationCapability` interface exists. Providers are only for *measurement* (`core/providers/IProvider.h`, `Real*Provider`s).
- `Transaction.h:30` defines `struct Optimization { Risk risk; vector<Change> actions; }` and `Transaction.h:43` `struct Transaction { vector<Optimization> }` (P1 scaffold) but these are **not wired** to `RecommendationEngine` or `TransactionStore`. The safety layer uses `safety::Transaction` (`core/safety/transaction/Transaction.h:26`) which has `target`/`operationId` strings, not `Optimization.actions`.
- `TransactionManager` is stubbed (`TransactionManager.cpp:3` `return {}`) — no capability to create provider-driven transactions.
- `IpcServer` would need a new `allowedOperation` and `polkit` action per new privileged mutation — currently locked to `ping`/`info` (fail-closed, correct, but not extensible without design).

A clean extension would require (a) a capability registry, (b) per-capability evidence collector, (c) per-capability `expectedBenefit` numeric estimator, (d) per-capability `CurrentState` snapshotter for `beforeHash`/`unitHash`, and (e) a privileged helper allowlist per capability — none exist.

### 5.8 Whether Recommendations Are Evidence-Backed or Mostly Static Rules

**Mostly static rules with evidence *citation*, not evidence *computation*.**

- Evidence vectors are copied from `Bottleneck.evidence` (`RecommendationEngine.cpp:22` `bn.evidence`) which *does* contain real measurements (`lspci 10de:174d`, `systemd-analyze blame 2m28s`, `MemAvailable …`) collected by `BaselineEngine` (`BaselineEngine.h:30`).
- But `expectedBenefit`, `confidence`, `risk`, `why`, `alternative`, `rollbackConcept` are hand-written prose per `bn.id`.

**Verdict:** Evidence-backed *descriptively*, static *prescriptively*. The system can show you *what it saw* but does not *derive* what it recommends from what it saw beyond string matching on `bn.id`.

---

## 6. Candidate/Transaction Architecture Audit

### 6.1 Boundary Between Discovery / Analysis / Recommendation / Transaction

```
Providers (read-only)          Engines (pure)                 Safety (stateful, privileged)
─────────────────              ──────────────                 ─────────────────────────────
RealOsProvider ─┐
RealCpuProvider ─┤
RealMemoryProvider ─┤─→ BaselineEngine::collect() ─┐
RealStorageProvider ─┤   PerformanceBaseline (15 metrics, MetricMeta)  │
RealGpuProvider ─┤                         └→ BottleneckEngine::analyze() → vector<Bottleneck> (10 types)
RealThermalProvider ─┤                            (if bn.id == GPU-001 …) │
RealSystemdProvider ─┤                         └→ RecommendationEngine::generate(b, bottlenecks) → vector<Recommendation> (8 hard-coded)
RealKdeProvider ─┤                                                    │
RealProcessProvider ─┤                         ┌→ ProfileAdvisor::canConsider(candidateId, profile) → BLOCKED/REQUIRES/ALLOWED
RealJournalProvider ─┘                         └→ ExplanationEngine::explainCandidate → Explanation (22 fields)
                                                              ↓
                                              TransactionStore::create(preview) → FileSafety::validatePath → BackupEngine::create → TransactionValidator::validateForApply → apply → ComparisonEngine::compare(before,after) → AuditLog::hashEvent+fsync
```

**Observations:**

1. **Discovery → Analysis is clean and extensible.** `BaselineEngine::collect()` (`BaselineEngine.h:20`) fuses providers deterministically; adding a new provider (e.g., `RealFlatpakProvider`) is straightforward and already done for 9 providers. `BottleneckEngine::analyze()` (`BottleneckEngine.cpp:6`) is pure but hard-coded to 10 bottleneck types — extensible by editing, not by registration.
2. **Analysis → Recommendation is the narrow waist.** `RecommendationEngine` is the only bridge from measurements to actionable suggestions, and it is a hard-coded `if` chain. It does not consume `ProfileAdvisor` or `ComparisonEngine`; those are layered *outside* via `ExplanationEngine`.
3. **Recommendation → Transaction is disconnected.** `Recommendation` (`PerfModels.h:150`) is a *presentation* object (`title`, `expectedBenefit` string, `riskLevel` string). `safety::Transaction` (`Transaction.h:26`) is a *lifecycle* object (`target`, `beforeHash`, `kernelVersion`, `preconditions` map). There is no `Recommendation → Transaction` factory — `cli/p4_cli.cpp:47` `cmd_preview` manufactures a dummy `fstab` transaction unrelated to any `Recommendation`. Real P6/P7 transactions were built manually, not via engine.
4. **Transaction → Verification is well-wired *after* the fact.** `Transaction` holds `beforeBaseline`/`afterBaseline`/`comparison` (`Transaction.h:56`) and `ComparisonEngine` is pure/deterministic (`ComparisonEngine.h:12`). This was proven for P7 (`MetricComparison` thresholds stored). But `RecommendationEngine` never uses `Comparison` to learn — `observedBenefit` does not feed back into future `expectedBenefit` scoring.

### 6.2 Real Extensible Framework vs. Collection of Hard-Coded Cases

**Currently a collection of hard-coded cases.**

| Criterion | Framework Would Have | Polaris Currently Has |
|-----------|---------------------|----------------------|
| Optimization capability interface | `IOptimizationCapability { isApplicable(baseline, profile) → bool; estimateBenefit(baseline) → numeric; createTransaction() → Transaction; snapshotCurrentState() → CurrentState; verify(before,after) → Comparison }` | No interface. Each capability is an `if` branch with string literals. |
| Registry/discovery | `OptimizationRegistry::register(capability)` iterated by `RecommendationEngine` | `RecommendationEngine::generate` hard-coded 8 `add()` calls |
| Numeric benefit model | `expectedBenefit = f(metric delta, confidence)` e.g., `713 M` from `memory.availableKb` delta, `9.192 s` from `systemd.userspace` delta | `expectedBenefit` is prose string, not `double` |
| Risk model | `risk = f(reversibility, requiresReboot, requiresAuth, userImpact, dependencyCriticality)` | `riskLevel` literal `R0`–`R3` chosen by author |
| Measurement evidence | `evidence` includes `before`/`after` `MetricComparison` with `delta`/`pctDelta` per `Comparison.h:33` | `evidence` is `vector<string>` copied from `Bottleneck`, no numeric delta until *after* `apply` |
| Transaction creation | `recommendation.confidence > threshold && benefit > cost && ProfileAdvisor==ALLOWED → TransactionStore::create` | No automatic creation; `polaris_p4` preview is manual dummy; P17 had `NO_ACTION` with no `PREVIEWED` transaction |
| Verification feedback | `observedBenefit` updates capability’s historical `benefit` prior | `observedBenefit` stored in `Transaction.comparison` (`Transaction.h:58`) but never read by `RecommendationEngine` next run |
| CLI reachability | `polaris scan` lists capabilities, `polaris explain <capability>`, `polaris preview <capability>` | `polaris_p3` shows bottlenecks, `polaris_p4 explain <candidate>` shows `BLOCKED`/`REQUIRES` but no `preview` for real system capabilities beyond `dummy-test` |

**Conclusion:** Polaris has a *strong* safety/transaction *framework* but a *weak* optimization *capability* framework. The safety side is extensible (hardened `TransactionValidator`, `StateMachine`, `BackupEngine`, `AuditLog`, `FileSafety`, `Ipc*`, `RecoveryDetector`). The optimization side is not.

---

## 7. Why P17 Returned NO_ACTION_RECOMMENDED

**Evidence:** `docs/P17_REPORT.md:1` (status `COMPLETED (NO_ACTION_RECOMMENDED)`), `docs/P18_FINAL_REPORT.md:41` (`mssql remains disabled`, `zram 0B`, `NVRM 1`), `docs/P18_FINAL_STATE.json:388` (`optimizationCandidates` 9 entries), and `core/engines/recommend/RecommendationEngine.cpp:72` static flatpak candidate.

### 7.1 Candidate Scoring Table (from P17)

P17 scored 7 candidates (`docs/P17_REPORT.md:48`):

| Candidate | Enabled/Active | Resource | Boot Impact | Workflow (`ProfileAdvisor`) | Expected Benefit | Confidence | Risk | Decision |
|-----------|----------------|----------|-------------|----------------------------|------------------|------------|------|----------|
| Akonadi 14 agents 1302 M `db_data 126M` | `akonadi_control` running | 1302 M | `0 s` boot (not in `critical-chain`) | `usesKMail=yes` (handoff) → `BLOCKED_BY_USER_WORKFLOW` *or* file `unknown` → `REQUIRES_USER_CONFIRMATION` | `~1.3 GB` if disabled | `0.65` (needs `usesKMail=no` to be `0.90`) | `R2` | **REJECTED** |
| bluetooth 2 paired TSCO-TS2343 E7 | `enabled active` | 5–10 M | `0 s` (not in `critical-chain` nor `blame` top) | `usesBluetooth=unknown` → `REQUIRES_USER_CONFIRMATION` + evidence shows **in use** | `5–10 M` | `0.40` `<0.65` threshold | `R2` | **NO_ACTION** |
| avahi-daemon udp 5353/5355 kdeconnect | `enabled active` | 5–10 M | `0 s` | `usesAvahi=unknown` → `REQUIRES` | `5–10 M` | `0.40` | `R2` | **NO_ACTION** |
| cups disabled active socket 127.0.0.1:631 | `disabled` socket-activated | ~0 (not resident) | `0 s` | `usesPrinting=unknown` → `REQUIRES`, already `disabled` | `0` | `0.70` | `R2` | **NO_ACTION** |
| plocate-updatedb.service `Nice 19 idle` | `static inactive` `timer Wed 00:32` | 0 s blocking, 21 s wall parallel | `0 s` boot-critical (not in `critical-chain`) | N/A | `0 s` boot | `0.85` | `R2` (breaks `locate`) | **NO_ACTION** |
| dnf-makecache timer `OnBootSec 10min` | `static inactive` `Next 1h24min` | 0 s | `0 s` (not in top 30 `blame`) | N/A | `0 s` | `0.75` | `R2` | **NO_ACTION** |
| autostart `nvidia-settings-user.desktop` `Hidden=true` 101 `4ad53409` | `Hidden` | 0 | 0 | N/A | 0 | `0.70` | `R1` | **NO_ACTION** |

`journal` `p3 141 vs 254` *was also scored* (`docs/P17_REPORT.md:57`) → `0` benefit, `R0`, `NO_ACTION` (journal is effect, not cause).

### 7.2 Evidence Why Each Threshold Suppressed

- **Akonadi:** Not suppressed by threshold but by *correct* safety constraint — user uses KMail/Kontact (`docs/P18_FINAL_REPORT.md:54`, `ProfileAdvisor.cpp:9`). `confidence 0.65 < 0.90` if `usesKMail=no` would be `0.90`; but with `usesKMail=yes` it is `BLOCKED` regardless of confidence. **This is not “too conservative” — it is correct.** If Polaris disabled Akonadi, KMail would break (`userImpact` `KMail/Kontact would lose PIM` — `ExplanationEngine` `whatWillChange`). The *missing* piece is not lower threshold but *user explicitly declaring* `usesKMail=no` (which would not happen while they use KMail).
- **Bluetooth/avahi/cups:** All `REQUIRES_USER_CONFIRMATION` because `profile.json` is `unknown` default (`ProfileStore::load` missing→`unknown` — `docs/P17_REPORT.md:35`). Even if file were `usesBluetooth=no`, benefit `5–10 M` is tiny vs `R2` (breaks paired devices). `confidence 0.40 < threshold 0.65` (`docs/P17_REPORT.md:59` `score = f(…)`). **Candidate set too small *and* benefit negligible.**
- **plocate/dnf-makecache/autostart:** `0 s` boot-critical because `systemd-analyze critical-chain` is `graphical.target @8.514s → plasmalogin @7.301s +1.211s` and `plocate` not in `critical-chain` (`docs/P17_REPORT.md:14`, `BottleneckEngine.cpp:175`). `BOOT-003` explicitly says `“Background work: dnf-makecache (not blocker)”` `LOW` `0.75` (`BottleneckEngine.cpp:172`). Threshold `boot >+10% relative` (`ComparisonEngine.h:22`) not exceeded — `0%` boot-critical → not worthwhile. `plocate` `Nice 19 idle` already low impact.
- **Journal:** Benefit `0` — journal is result of `nvidia`/`mssql` fixes already done (`docs/P17_REPORT.md:57`).

### 7.3 Was the Machine Already Optimized?

**Yes, for the domains Polaris can measure.** Baseline comparisons prove:

- `boot.userspace` `54.106 s` (P3) → `8.515 s` (P18) `-84%` (`docs/P18_FINAL_REPORT.md:72`) — not regression, threshold `+10%` not exceeded, `abs(delta) 45.6`.
- `memory.available` `4.2 GiB` → `5.6 GiB` `+33%` (`docs/P18_FINAL_REPORT.md:73`) — `availableMemGb 1.0` threshold not exceeded (increase is benefit).
- `memory.swapUsed` `1.6 GiB` → `0` `-100%` (`docs/P18_FINAL_REPORT.md:75`).
- `thermal.cpuMax` `67 C` → `60 C` `-7 C` (`docs/P18_FINAL_REPORT.md:76`) — `thermalC 15.0` not exceeded.
- `nvidia.claimed` `0` → `1` (`docs/P18_FINAL_REPORT.md:77`) — not regression.
- `zram` `8 G lzo-rle DATA 4K 0B used` healthy (`docs/P18_FINAL_REPORT.md:35`).
- `systemd --failed` `1` (`mssql`) → `1` (`drkonqi` unrelated, not `mssql`) — `mssql` `1→0` is improvement, `drkonqi` is `INCONCLUSIVE` not Polaris regression (`docs/P18_FINAL_REPORT.md:77`).

**The two high-signal optimizations that *were* worthwhile (`mssql` 713 M/9 s, `nvidia` PRIME) are done.** P7 preflight had 25 items, P7 post-reboot 15 checks PASS. Remaining boot `userspace 8.515 s` is *not* a bottleneck on i5-10210U + NVMe (`dev-tpm0 5.502 s`, `dev-sdb 5.119 s` hardware — `docs/P18_FINAL_REPORT.md:32`).

### 7.4 Was RecommendationEngine Too Conservative?

**No, it is not too conservative — it is too *sparse* and too *stringly-typed*.**

- **Not too conservative on blocked candidates:** `BLOCKED` for Akonadi is correct; lowering threshold to `REQUIRES→ALLOWED` would break KMail.
- **Not too strict on tiny candidates:** `5–10 M` on `12 GiB` is `0.05%`; `R2` risk (break Bluetooth) is disproportionate. Threshold `benefit/risk` not worthwhile is correct.
- **Too conservative in *not* proposing *other* domains at all:** `RecommendationEngine` does not propose flatpak cleanup as a measurable `statvfs` delta, nor `journal` vacuum as `journalctl --disk-usage` delta, nor autostart `Hidden` for other entries — because it has no evidence collectors for them. The *absence* of candidates is not conservatism but incompleteness.
- **Too static:** `confidence 0.40` for bluetooth is literal; a numeric estimator could compute `confidence = f(pairedDevices>0 ? 0.1 : 0.9, serviceActive? …)` and `expectedBenefit = rssKb` measured via `RealProcessProvider::getTop`. Today it is `0.40` hard-coded.

### 7.5 Root Cause Decomposition

| Hypothesis | Verdict | Evidence |
|-----------|---------|----------|
| Machine already optimized | **Primary** — `True` for Boot/Memory/GPU/NVIDIA/zram | `ComparisonEngine` deltas above; `P18 FINAL_REPORT:72–77` |
| RecommendationEngine too conservative (thresholds too strict) | **False** — thresholds `10%`/`1 GiB`/`15 C`/`new_failed` are correct and not exceeded; `REQUIRES` for `unknown` is correct | `ComparisonEngine.h:21`, `ProfileAdvisor.cpp:84` |
| Candidate set too small | **True** — major domain gap | `RecommendationEngine.cpp:104` only 8 recs; §4 table “Cannot” |
| Evidence collection insufficient | **True** — missing flatpak, journal disk-usage, autostart enumeration, timer `OnCalendar`, `power-profiles` | `BaselineEngine.h:20` no `flatpak`/`journal disk`/`autostart` collection |
| Thresholds too strict | **False** — thresholds are lenient; candidates fail by wide margin (`0 s` vs `10%`, `5 M` vs `1 GiB`) | `docs/P17_REPORT.md:59` `0 s` boot-critical, `5–10 M` tiny |
| User-profile information incomplete | **Partially true** — `profile.json not exists` → `unknown` forces `REQUIRES` for 4 candidates | `docs/P17_REPORT.md:35` `unknown` default, `ProfileStore::load` no auto-create |
| No worthwhile safe optimizations exist on this host | **True for this host today** — `NO_ACTION_RECOMMENDED` correct | `docs/P17_REPORT.md:88`, `docs/P18_FINAL_REPORT.md:108` `7 rejected candidates` |

**Answer to “why P17 NO_ACTION?”:** All of (a) machine already optimized, (c) candidate set too small/exhausted, (d) evidence collection insufficient for other domains, and (g) no worthwhile safe optimizations remaining — **not** (b) or (e). The engine did not “miss” a good candidate; there was none to miss among the domains it understands.

---

## 8. Product-vs-Safety Gap

| Dimension | Safety Answer | Product Answer | Gap |
|-----------|---------------|---------------|-----|
| *Did we avoid harm?* | Yes — `stale-preview` 7 fields (`TransactionValidator.h:114`), `TOCTOU` symlink/canonical (`TransactionValidator.h:334`), `idempotency` `already_completed` (`TransactionStore.h:188`), `FileSafety` allowlist (`FileSafety.h:18`), `flock` exclusive (`TransactionLock.cpp`), `Audit` hash chain `fsync` (`AuditLog.cpp`), `Ipc` `ping/info` only (`IpcProtocol.h`), `ProfileAdvisor` `BLOCKED` (`ProfileAdvisor.cpp:9`), 33/33 tests | Yes — but “avoid harm” is not “create value” | No gap — safety solved |
| *Did we find something worth doing?* | N/A (safety is about not doing unworthy things) | **No** — P17 `7 candidates` all `0 s`/`5–10 M`/`REQUIRES`/`REJECTED` → `NO_ACTION` (`docs/P17_REPORT.md:88`) | **Product gap** — discovery exhausts after 2 fixes |
| *Can we explain why?* | Yes — `ExplanationEngine` `WHY NOW`/`WHAT WILL NOT CHANGE`/`REJECTION CONDITIONS` deterministic (`ExplanationEngine.h:15`, `docs/P16_IMPLEMENTATION_REPORT.md:3`) | Yes — explainability is excellent, but explains *why we did nothing* | Explainability masks thin catalog |
| *Can we verify improvement?* | Yes — `ComparisonEngine` `SUCCESS`/`REGRESSION`/`NO_CHANGE` (`Comparison.h:12`) with stored thresholds | Yes — but `observedBenefit` only for the 2 completed transactions; `P17` has `N/A` (`docs/P17_REPORT.md:131`) | Verification proven but idle |
| *Can we repeat on next Fedora/KDE host?* | Yes — providers are generic (`BaselineEngine.h:20`) | **Uncertain** — next host might have same 2 fixes + same thin catalog; generic domains (flatpak, journal, baloo, autostart) not measured | Portability gap |

**Honest engineering assessment:** `Polaris is safe` ≠ `Polaris is a useful optimizer`. The statement “Polaris does not auto optimize. It measures, explains, asks for approval, then changes one thing with backup and verifies.” is *true* and *valuable*. But the product question “If it measures flawlessly and finds nothing worth changing, is it an optimizer?” is *also* true — and the answer today is **no, it is a safe measurement/explainability platform that happens to have optimized this host twice.**

Do not defend the implementation: the catalog is the problem, not the safety. The roadmap completed the safety architecture (P4–P16) before completing the optimizer catalog — an intentional trade-off that now needs rebalancing.

---

## 9. Missing Optimization Capabilities

Grouped by whether they *should* be added (safe, measurable, reversible, high-confidence) vs. *must not* be added (unsafe one-click tweaker).

### Should Be Added (Small Number, High-Confidence, Measurable, Reversible)

These are not “100 tweaks” but **capability providers** — each is a *measurement + estimation + transaction* triple:

1. **Package/cache maintenance** — `dnf` cache, `flatpak` unused runtimes, `journal` vacuum.
2. **Startup/autostart hygiene** — `~/.config/autostart` + `/etc/xdg/autostart` `Hidden`/`XDG` entries.
3. **Systemd timers & background work** — `plocate-updatedb.timer`, `dnf-makecache.timer`, `fstrim.timer` schedule `AccuracySec`/`OnCalendar` tuning (not disable).
4. **Logging retention** — `systemd-journald` `SystemMaxUse`/`MaxRetentionSec`.
5. **Desktop/baloo indexing** — `baloo` enable/disable, `KDE` effects `blur` etc. (already `KdeBaseline.effects` collected).

### Intentionally Out of Scope (Even Though They Look Optimizable)

- `zram`/`swap`/`swappiness`/`vfs_cache_pressure` tuning — correctly `R0 Do NOT disable` today; any tuning would be `R3+` and not measurable via `pressure 0`.
- `CPU governor`/`epp`/`no_turbo` — Fedora `intel_pstate powersave balance_performance` is correct; `thermal 60 C` healthy.
- `I/O scheduler` (`none`/`bfq`) — NVMe default correct, `ioPressure 0`.
- `GPU` beyond 470xx — `PRIME` already done.
- `Kernel cmdline`/`modprobe`/`grub` — `R3` and correctly not proposed.
- `NetworkManager`/`WPA`/`DNS` — no measurable benefit via `nmcli`.

These are excluded not because they are hard but because they are *unsafe to auto-tweak* and violate “must remain reversible with measurable benefit.”

### Why Current Catalog Misses Them

- No `RealFlatpakProvider` (`flatpak list`), no `RealJournalDiskProvider` (`journalctl --disk-usage`), no `RealAutostartProvider` (`*.desktop` enumeration), no `RealTimerProvider` (`systemctl cat *.timer`), no `RealBalooProvider` (`balooctl status`) — `BaselineEngine::collect()` (`BaselineEngine.h:20`) does not call them.
- `BottleneckEngine::analyze()` does not classify `flatpak duplicates` or `journal > threshold` or `autostart overhead` as bottlenecks.
- `RecommendationEngine::generate()` hard-codes `REC-006` flatpak as *always* emitted string, not as `flatpak.runtimes.size() * avgSize` estimate.

---

## 10. 3–7 Recommended Future Optimization Domains

Each domain below is **safe** per the product goal — reversible, measurable, evidence-backed, one-change-at-a-time, and `MUST refuse` conditions explicit. They are *candidate capabilities* for the product, not recommendations for this specific host (which has `5.6 GiB avail`, `0B zram`, `Hidden=true` already correct).

### Domain 1: Flatpak Unused Runtime Cleanup

- **Problem solves:** Fedora KDE ships multiple `Freedesktop`/`GNOME` runtimes (`25.08` + `50`) plus `org.kde.Platform` duplicates; `flatpak uninstall --unused` can reclaim `1–2 GiB` on hosts where disk is `>80%` or runtimes >5 duplicates. On this host `df 62% used free 128 G` (P3) it was *not* worthwhile — correctly estimated as `1–2 GB` low benefit.
- **Evidence justifies:** `flatpak list --runtime` shows `Freedesktop 25.08` + `GNOME 50` both keeping `org.freedesktop.Platform 23.08` and `24.08`; `flatpak remote-info` for each; `statvfs` `freeBytes < 20%` or `runtimes > 5` or duplicate `branch` entries; `BaselineEngine` would collect `StorageBaseline.flatpakRuntimes`.
- **Exact mutation:** `flatpak uninstall --unused -y` (user-level, no `sudo`) or `flatpak uninstall <runtime-id>`. Single runtime per transaction.
- **Expected benefit:** `1–2 GB` disk (measured `du -sh /var/lib/flatpak` before/after), `~0.2 s` faster `flatpak list` metadata, **no boot benefit**.
- **How benefit measured:** `StorageBaseline.filesystems[] freeBytes` delta via `statvfs` before/after; `ComparisonEngine` `storage.free` `delta > 0.5 GiB` threshold; `flatpak list` count delta.
- **How rollback works:** `flatpak install <runtime>` from `flathub` (deterministic, versioned via `flatpak remote-info` snapshot in `Transaction.preconditions["flatpak.runtime.<id>.version"]`).
- **What could go wrong:** App launched after runtime removed fails to start (e.g., `org.kde.Platform` still referenced); disk full during uninstall leaves dangling refs.
- **When Polaris MUST refuse:** `flatpak list --app` shows app *depends* on candidate runtime (pin check); `freeBytes > 30%` and `runtimes < 3` → benefit negligible vs `R1`; `flatpak` not installed; `precondition flatpak.runtime.<id>.refcount != 0`.

### Domain 2: DNF / Package Cache & Journal Vacuum

- **Problem solves:** `/var/cache/dnf` accumulates `1–3 GB` per Fedora release; `journalctl --disk-usage` can be `2–4 GB` default `SystemMaxUse` unset → slows `journalctl -p 3` and `p3 254` counts.
- **Evidence justifies:** `du -sh /var/cache/dnf 2.1 G`, `journalctl --disk-usage 3.2 G`, `p3count 254` high, `dnf repolist` enabled, `StorageBaseline.filesystems[0].freeBytes` low.
- **Exact mutation:** `dnf clean packages` / `dnf clean all --enablerepo=*` (R1, user via `org.polaris.dnf.clean` helper *or* `journalctl --vacuum-size=500M` / `--vacuum-time=14d`); **one** of dnf *or* journal per transaction, never both.
- **Expected benefit:** `0.5–3 GB` disk (journal) / `0.5–1 GB` (dnf cache), *not* boot or RAM.
- **How benefit measured:** `StorageBaseline` `freeBytes` delta; `JournalBaseline.diskUsage` `count` delta; `journalctl --disk-usage` before/after; `Comparison` `storage.free` `available true` `delta 500 MB+`.
- **How rollback works:** DNF cache redownloaded via `dnf makecache` (no real rollback needed; `rollbackState AVAILABLE` is `dnf makecache`); journal vacuum is *not* reversible (old logs lost) — so Polaris must snapshot `journalctl --disk-usage` and warn `rollback N/A, logs older than 14d lost` — still safe if retention `14d` honored and user approved.
- **What could go wrong:** `dnf clean all` during active `dnf` transaction → `lock` contention; `journal --vacuum-time` deletes logs needed for post-mortem.
- **When Polaris MUST refuse:** `/var/cache/dnf` `<500 MB` *and* `journal <1 GB` → not worthwhile; `dnf` transaction lock held (`/var/cache/dnf/lock`); `journal vacuum` if `SystemMaxUse` already `500M` (already optimal); `backupState` not `CREATED` (need `journalctl --disk-usage` snapshot).

### Domain 3: Autostart Desktop Entry Hygiene (User-Level)

- **Problem solves:** `~/.config/autostart/*.desktop` and `/etc/xdg/autostart/*.desktop` contain `X-GNOME-Autostart-enabled=false` vs `Hidden=true` vs stale entries (e.g., `nvidia-settings`, `akonadi_control` autostart, `baloo_file` previously). User login `596 ms` (`docs/P17_REPORT.md:13`) could be delayed by duplicate or erroring autostarts.
- **Evidence justifies:** Enumeration of `~/.config/autostart/*.desktop` (today only `nvidia-settings-user.desktop Hidden=true` correct — `docs/P17_REPORT.md:30`) plus `/etc/xdg/autostart/*.desktop` 30+ entries via `RealAutostartProvider`; `Exec` check `which <binary>` exists; `systemd-analyze --user blame` shows `akonadi_control 4.799 s` (`docs/P18_FINAL_REPORT.md:32`); `journalctl --user -p 3 -b` shows `autostart` failures.
- **Exact mutation:** Set `Hidden=true` in single `~/.config/autostart/<id>.desktop` overlay (user override, does not edit `/etc/xdg/autostart` directly — copy-on-write). **One entry per transaction.**
- **Expected benefit:** `0.2–1.0 s` faster *user* login (measured `systemd-analyze --user` `blame`), **not** `systemd-analyze userspace 8.515 s` system boot. Memory `5–20 MB` if autostart was `baloo_file` etc.
- **How benefit measured:** `PerformanceBaseline.processes.top` RSS delta after login; `systemd-analyze --user` `userspace` before/after (requires separate `userSystemd` metric not yet in baseline — P11 `login` not in `PerformanceBaseline` per `PROJECT_STATE.json` limitation); manual `journal` autostart error count delta.
- **How rollback works:** `rm ~/.config/autostart/<id>.desktop` or set `Hidden=false` via `FileSafety::atomicWrite` same method; `BackupEngine` copy of original `.desktop` `SHA-256`.
- **What could go wrong:** Disabling `kscreen` or `xdg-desktop-portal` breaks display/portal; overlay masks future package update.
- **When Polaris MUST refuse:** Entry `Exec` binary not found but `OnlyShowIn` excludes `KDE` (not applicable, correct `Hidden=true` already); entry is `kwin`/`plasmashell`/`kded` essential (`/etc/xdg/autostart` `30+ KDE essential` — `docs/P17_REPORT.md:30`); `ProfileAdvisor` `canConsiderAutostart` would need `usesAutostart` tri-state; `unknown` → `REQUIRES_USER_CONFIRMATION`; benefit `0 s` system boot and `login <0.2 s` → not worthwhile.

### Domain 4: systemd Timer Schedule Tuning (Defer, Not Disable)

- **Problem solves:** `plocate-updatedb.timer OnCalendar Tue 00:32`, `dnf-makecache.timer OnBootSec 10min` fire during `graphical.target` parallel (`plasma-login @7.301s`) and consume I/O (`Nice 19 idle` still I/O). On slower HDD (`sda 5400rpm` `docs/P8_REPORT` ) they *would* be blocking; on this NVMe they are background.
- **Evidence justifies:** `systemctl cat plocate-updatedb.timer` `OnCalendar`, `systemd-analyze blame 21.111 s` but `critical-chain` not contains (`docs/P17_REPORT.md:14`), `systemd-analyze critical-chain` `plasmalogin 7.301s +1.211s`, `pressure io avg10` from `BaselineEngine.h:79`.
- **Exact mutation:** Add drop-in ` /etc/systemd/system/<timer>.d/polaris-override.conf` with `TimerAccuracySec=12h` or `OnCalendar=daily 03:00` shift (for `plocate`), or `OnBootSec=1h` for `dnf-makecache` — **not** `disable`, just defer. One timer per transaction, via `systemctl daemon-reload` + `systemctl restart <timer>`.
- **Expected benefit:** `0–0.5 s` boot-critical if timer was on critical path (it is not today → `0 s`), `5–10%` less I/O during first 5 min post-boot (`ioPressureSome10` delta). **Not measurable as `boot.userspace` but as `ioPressure`/`plocate blame` wall**.
- **How benefit measured:** `SystemdBaseline.blameTop` `plocate 21 s` → after defer, `blame` not in top on next boot; `ioPressure` `BaselineEngine` `pressure` before/after; `ComparisonEngine` would need new metric `systemd.timerPlocateSec` (not yet existent — part of framework phase).
- **How rollback works:** `rm /etc/systemd/system/<timer>.d/polaris-override.conf` + `daemon-reload` + `restart timer`; `BackupEngine` backup of original timer drop-in (if existed) `SHA-256`.
- **What could go wrong:** Drop-in syntax error breaks timer; `daemon-reload` fails; `plocate` never updates (stale locate DB).
- **When Polaris MUST refuse:** Timer not in `blameTop` and not in `critical-chain` and `ioPressure 0` → `0 s` benefit (`docs/P17_REPORT.md:54`); timer unit is `fstrim.timer` with `AccuracySec 1d` already optimal; `CurrentState.kernelVersion` changed between preview/apply (stale); `precondition service.<timer>.enabled` drift.

### Domain 5: KDE/Baloo/Indexing — Opt-In Disable

- **Problem solves:** `baloo_file` can consume `200–600 MB` + continuous I/O indexing `/home` (seen on KDE hosts, not this host where `top` shows `code/opencode` dominate). If user does not use `KRunner` file search, disabling saves RAM/I/O.
- **Evidence justifies:** `balooctl status` `File Indexer running`, `baloo_file` `RSS` `400 M` in `RealProcessProvider::getTop(15)` (`BaselineEngine.h:154`), `akonadi 1302 M` pattern similar, `ProfileAdvisor` `usesBaloo` tri-state (new field), `KdeBaseline.effects` already collected but not used.
- **Exact mutation:** `balooctl disable` + `balooctl purge` or `balooctl suspend` or `~/.config/baloofilerc` `Indexing-Enabled=false` via `FileSafety::atomicWrite` (user config, `R1`). One indexing subsystem per transaction.
- **Expected benefit:** `200–600 MB` RAM (if `baloo_file` resident), `5–10%` less `ioPressure`, *no* `boot.userspace` benefit (user session).
- **How benefit measured:** `MemoryBaseline.availableKb` delta `+300 MB`, `ProcessBaseline.top` `baloo_file` RSS `0` after, `ioPressureSome10` delta, `Comparison` `memory.available` `threshold 1 GB` would be `IMPROVED` if `baloo` was `600 M`.
- **How rollback works:** `balooctl enable` + `balooctl resume` or restore `baloofilerc` from `BackupEngine` `SHA-256`.
- **What could go wrong:** `Dolphin`/`KRunner` file search broken; `baloo` disable also disables `tag` search user relies on.
- **When Polaris MUST refuse:** `balooctl status` `disabled` already or `baloo_file` not in `top` / RSS `<100 MB` → tiny benefit; `ProfileAdvisor` `usesBaloo=unknown` → `REQUIRES_USER_CONFIRMATION` (must have explicit `usesBaloo=no`); `usesKOrgana`? similar to `Akonadi` — if `usesFileSearch=yes` → `BLOCKED_BY_USER_WORKFLOW`.

### Domain 6 (Optional, Low Priority): Power/Compositor Effects via User Config

- **Problem solves:** KDE `blur`/`glide` effects (`FakeHardwareProvider` `FakeProviders.h:17` `effects {blur true, glide true}`) cost GPU; `power-profiles-daemon` `balanced` vs `performance` on `i5-10210U` thermal `67→60 C` — not a problem today.
- **Evidence justifies:** `KdeBaseline.effects` map (`BaselineEngine.h:198`), `sensors coretemp 60 C` < `85`, `thermal.throttling false` (`BaselineEngine.h:128`), `GpuBaseline.glRenderer Mesa Intel`.
- **Exact mutation:** `~/.config/kwinrc` `[Compositing] Enabled=false` overlay or `powerprofilesctl set balanced` (if `power-profiles-daemon` active).
- **Expected benefit:** `1–3%` smoother window, `2–5 C` maybe, **not** boot/memory.
- **How benefit measured:** `BenchmarkEngine` `cpu_prime` `quick` before/after; `ThermalBaseline.cpuMaxC` delta `±5 C`; `KdeBaseline.effects` diff.
- **How rollback works:** Restore `kwinrc` from `BackupEngine` `atomicWrite` rollback.
- **What could go wrong:** Disabling compositing breaks `kwin_wayland` `460M` path; `power` profile `performance` increases `thermal +10 C`.
- **When Polaris MUST refuse:** `XDG_SESSION_TYPE=wayland` `kwin_wayland` active + `effects` already off; `thermal 50 C` healthy → no benefit; `REQUIRES_USER_CONFIRMATION` for `usesCompositing`.

**Why these 6 and not 20?** Each is (a) *user-config or cache* (never `zram`/`sysctl`/`grub`/`modprobe`), (b) *one file or one timer* per transaction, (c) *numeric benefit* via `statvfs`/`availableKb`/`blame`/`journal disk-usage` measurable before/after, (d) *rollback* via `BackupEngine` `atomicWrite` or `enable` inverse, (e) *blocked* by explicit `uses*` profile when workflow depends, and (f) **refused when evidence shows `0 s`/`5 M` tiny** — exactly the P17 lesson.

---

## 11. Safety Requirements for Those Domains

All domains must preserve the existing `READ→…→AUDIT` pipeline and add domain-specific hardening. No weakening of `FileSafety`, `StateMachine`, `TransactionValidator`, `BackupEngine`, `AuditLog`, `IpcProtocol`, `TransactionLock`, `RecoveryDetector`, `ProfileAdvisor`.

| Requirement | Generic (already) | Domain-Specific Addition |
|-------------|-------------------|--------------------------|
| **Measurement** | `BaselineEngine::collect` `MetricMeta available` `ComparisonEngine::compare` thresholds | New providers: `RealFlatpakProvider`, `RealJournalDiskProvider`, `RealAutostartProvider`, `RealTimerProvider`, `RealBalooProvider` — each returns `MetricMeta` with `source`/`method`/`confidence`, `available false` if `flatpak` not installed |
| **Evidence** | `Bottleneck.evidence vector<string>` + `Comparison.MetricComparison delta/pctDelta` | `Evidence` must include `before` numeric: `flatpak runtimes 7 duplicate 2`, `journal 3.2G`, `autostart entries 34 enabled 1 Hidden=true`, `timer OnCalendar Tue 00:32 Accuracy 1m`, `baloo RSS 420M` — no prose estimate |
| **Recommendation** | `Recommendation.confidence 0–1` `riskLevel R0–R3` `expectedBenefit` string | `expectedBenefit` becomes *numeric* in new capability: `benefitGB double` + `benefitSec double` + `confidence` computed `f(rss, freePct, pairedDevices)` not literal |
| **Preview** | `ChangePreview diff` `method atomic write via helper` `privilege org.polaris.*` `rollback` | `ChangePreview.target` is user-owned overlay (`~/.config/autostart/...` `0600`) or cache path or `systemd/system/<timer>.d/` drop-in (not `/etc/fstab` direct) |
| **Approval** | `TransactionValidator::bindApproval` hash `beforeHash`/`unitHash`/`kernelVersion`/`packageStateHash` (`TransactionValidator.h:47`) + `StateMachine PREVIEWED→APPROVAL_REQUIRED→APPROVED` | Extend `CurrentState` snapshot: `flatpakStateHash = sha256(sorted flatpak list)`, `journalDiskHash`, `autostartHash = sha256(concatenated .desktop SHA)`, `timerHash = sha256(systemctl cat)`, `balooHash` |
| **Backup** | `BackupEngine::create` versioned `SHA-256` `fsync` no-overwrite (`BackupEngine.cpp:1`) | Backup is user-config file or `drop-in` dir; for `dnf clean`/`journal vacuum` backup is *snapshot file* (`du -sh` + `journal --disk-usage` text) not content backup (logs not restorable — must warn `rollback N/A`) |
| **Stale-preview** | 7 fields `target/operation/beforeHash/unitHash/kernel/package/precondition` (`TransactionValidator.h:114` `validateForApply`) + `TOCTOU symlink/canonical` (`TransactionValidator.h:334`) + `beforeHash empty→unverifiable` fail-closed (`TransactionValidator.h:217`) | Add `flatpakStateHash`, `journalDiskHash`, `autostartHash`, `timerHash` to same matrix — 3 states `UNCHANGED` accepted `CHANGED`/`UNAVAILABLE` rejected, 19-case table (`tests/unit/test_p15_stale_matrix.cpp`) extended |
| **Apply** | `TransactionStore::apply` `APPROVAL→VALIDATION→BACKUP→FINAL VALIDATION→APPLY` (`TransactionStore.h:178`) + `StateMachine BACKUP_CREATED→APPLYING` valid | For `drop-in` `daemon-reload` must be `execv` fixed path `/usr/bin/systemctl` no `sh -c` (`RealSystemdProvider.h:16` `safeExec`) with `timeout 5s` + `lock TransactionLock flock` exclusive (`TransactionLock.h:1`) |
| **Verify** | `TransactionStore::verify` idempotent (`TransactionStore.h:411`) + `ComparisonEngine::compare(beforeBaseline, afterBaseline)` (`ComparisonEngine.h:12`) | Verify is `statvfs` `freeBytes` delta for flatpak/journal, `systemd-analyze --user` delta for autostart, `blameTop` absence for timers, `ProcessBaseline.top` `baloo_file` RSS `0` |
| **Comparison/Regression** | `ComparisonEngine.defaultThresholds boot +10%` `mem 1GB` `thermal 15C` `new_failed` (`ComparisonEngine.h:21`) + `MetricComparison isBootCritical/isHealth` (`Comparison.h:42`) | Add thresholds: `storage.free increase 0.5GB` not regression; `journalDisk decrease 1GB` success; `userSystemd` login `+10%` regression; `baloo RSS` `+200MB` regression vs `mssql` 713M pattern |
| **Rollback** | `rollbackState AVAILABLE` `BackupEngine::restore` (`BackupEngine.h:25`) + `audit rollback` + `StateMachine FAILED→ROLLING_BACK→ROLLED_BACK` | Each capability declares `rollbackConcept`: `flatpak install <id>`, `rm drop-in + daemon-reload`, `rm autostart overlay`, `balooctl enable`; test `rollback` not overwriting backup `SHA-256` stable (`tests/unit/test_p15_lock_recovery.cpp`) |
| **Approval requirements** | `requiresReboot` `requiresAuth` `requiresApproval` (`PerfModels.h:162`) + `Polkit org.polaris.* auth_admin_keep` + `approval ≠ authorization ≠ applied` (`Transaction.h:40`) | `flatpak --unused` `R1` requires **no** `auth`/`reboot`, only `approval` (user cache); `journal vacuum` `R1` no auth; `autostart` `R1` no auth; `timer drop-in` `R2` requires `auth` (`org.polaris.timer.override`) + `requiresReboot false` (daemon-reload) + explicit `approval`; `baloo` `R2` no auth but requires `usesBaloo=no` `ALLOWED_FOR_ANALYSIS` |
| **Verification requirements** | `isDeterministic true` (`Comparison.h:67`) | Each capability must state `observedBenefit` vs `expectedBenefit` separation (`Comparison.h:58`) — e.g., `expected 1.2GB` flatpak vs `observed 1.1GB` via `statvfs` |
| **Regression criteria** | `hasRegression false` if `boot +10%` not exceeded etc. | For these domains: `storage.free` decrease `>1GB` → `REGRESSION`; `journal vacuum` `avail` decrease → `REGRESSION`; `autostart` new `failed` unit → `REGRESSION`; `timer` `new_failed` → `REGRESSION` |

**Non-negotiable:** `IpcProtocol::allowedOperations` remains `ping`/`info` until each new capability gets a narrowly scoped `org.polaris.*` polkit action with separate review — no `org.polaris.all`. No batch — **exactly one** `flatpak` runtime or **one** `autostart` entry or **one** `timer` per `Transaction.id` (`docs/PROJECT_STATE.json` `safetyInvariants: no batch changes`).

---

## 12. Proposed Next Phase — ONLY IF JUSTIFIED

### Is a Next Phase Justified?

**Yes, but not as “P19: Add 100 tweaks.”** Justified as: **“P19: Optimization Capability Framework — turn Polaris from hard-coded cases into an extensible, measurable optimizer.”**

*Why justified:* §3–§4 prove the *safety architecture* is complete but the *optimizer catalog* is exhausted and not extensible. Without a framework, Polaris will `NO_ACTION` forever on healthy hosts and cannot be a product on generic Fedora/KDE.

*Why not automatically P19:* If the goal is *only* to keep this host safe, `PROJECT_COMPLETE_WITH_LIMITATIONS` + `STOP` is correct (`docs/P18_FINAL_REPORT.md:311`). A framework phase is justified only if the original product goal (“safe optimizer worth using on *any* Fedora/KDE”) is still desired.

If pursued, call it **P19 — Optimization Capability Framework (Not a Tweaks Batch)**.

#### P19 Objective

Provide a **provider → bottleneck → capability → transaction** extensibility layer that lets Polaris discover a small number of high-confidence, measurable, reversible optimizations on a *generic* Fedora/KDE host, while preserving every P4–P16 safety invariant and the `READ→…→AUDIT` pipeline.

**This is an engineering phase, not a tuning phase.** It performs **no privileged host mutation** during implementation — only `core/` capability interface + 2–3 reference capabilities on test fixtures, plus `BaselineEngine` provider expansion, plus extended `Comparison` metrics. Real-host `APPLY` via helper remains `ping/info` only until separate per-capability helper review.

#### P19 Scope

**In scope:**

- Define `IOptimizationCapability` interface in `core/engines/capabilities/` (header-only, no Qt, C++20):
  ```cpp
  struct CapabilityState { string id; map<string,string> preconditions; string hash; string curHash; };
  struct CapabilityEvidence { vector<string> evidence; MetricMeta meta; double benefitEstimateGB; double benefitEstimateSec; double confidence; string risk; };
  class IOptimizationCapability {
    virtual string id() const = 0; // "flatpak-unused"
    virtual bool isApplicable(const PerformanceBaseline&, const UserProfile&) const = 0;
    virtual CapabilityEvidence collectEvidence(const PerformanceBaseline&) const = 0;
    virtual Recommendation toRecommendation(const CapabilityEvidence&) const = 0;
    virtual CurrentState snapshot(const PerformanceBaseline&) const = 0; // for TransactionValidator
    virtual Transaction toTransaction(const Recommendation&, const CurrentState&) const = 0; // preview template
    virtual bool verify(const PerformanceBaseline& before, const PerformanceBaseline& after, string& observedBenefit, Verdict&) const = 0;
  };
  ```
- Implement `OptimizationRegistry` singleton iterated by `RecommendationEngine::generate` — replaces hard-coded `if(bn.id==...)` loop. `generate()` becomes `for (auto &cap: registry) if (cap->isApplicable(b, profile)) out.push_back(cap->toRecommendation(cap->collectEvidence(b)))` with sorting by `benefit/risk`.
- Add 5 new read-only providers (test-fixture first, real-host second):
  - `RealFlatpakProvider` `flatpak list --runtime --app` (no shell, `execv` fixed `/usr/bin/flatpak`)
  - `RealJournalDiskProvider` `journalctl --disk-usage` + `journalctl -p 3 --disk-usage` grouping
  - `RealAutostartProvider` enumerates `~/.config/autostart/*.desktop` + `/etc/xdg/autostart/*.desktop` via `openReadOnly` (no `Desktop` exec)
  - `RealTimerProvider` `systemctl cat *.timer` `OnCalendar`/`AccuracySec`
  - `RealBalooProvider` `balooctl status` + `baloo_file` RSS from `ProcessBaseline`
- Extend `PerformanceBaseline` (`PerfModels.h:109`) with `FlatpakBaseline`, `JournalDiskBaseline`, `AutostartBaseline`, `TimerBaseline`, `BalooBaseline` — each with `MetricMeta available`.
- Extend `TransactionValidator` `CurrentState` (`TransactionValidator.h:14`) with `flatpakStateHash`/`journalDiskHash`/`autostartHash`/`timerHash`/`balooHash` in the 19-case stale matrix.
- Implement 2 reference capabilities **on test fixtures only** (`/tmp/polaris-test-root/flatpak/*`, `journal/*`):
  - `FlatpakUnusedCapability` — evidence `duplicates 2`, `freePct 15%`, `benefit 1.2GB`, `confidence 0.85`, `R1`, `preview` creates `~/.local/state/polaris/backups/TX-FLATPAK-.../flatpak-list.bak` `SHA-256`.
  - `JournalVacuumCapability` — evidence `diskUsage 2.8G`, `p3 254`, `benefit 2.0GB`, `confidence 0.90`, `R1`, preview shows `--vacuum-size=500M` diff, rollback warning `logs >14d lost`.
- Add `ComparisonEngine` metric `storage.free` `delta +0.5GB` success, `journal.diskUsage` delta, `userSystemd` login `+10%` regression (new, not in `defaultThresholds`).
- CLI: `polaris scan --capabilities --json` lists registry, `polaris_p4 explain flatpak-unused --json` uses new capability + `ProfileAdvisor`, `polaris_p4 capabilities list` (read-only).

**Out of scope (explicit non-goals):**

- No real privileged `APPLY` via helper — `IpcProtocol` remains `ping/info` (`docs/P14_PLAN.md`).
- No `TransactionManager` activation for real host — stub remains (`TransactionManager.cpp:1`).
- No batch — one capability per transaction.
- No `zram`/`swappiness`/`governor`/`scheduler`/`grub`/`modprobe` capabilities.
- No Qt GUI work.

#### P19 Architecture

```
core/providers/real/RealFlatpakProvider ─┐
RealJournalDiskProvider ─┤─→ BaselineEngine::collect ─→ PerformanceBaseline (+5 baselines)
RealAutostartProvider ─┤                         │
RealTimerProvider ─┤                         └→ OptimizationRegistry::applicable(b, profile) → ranked Recommendations (numeric benefit/risk)
RealBalooProvider ─┘                                   ↓
                                          ExplanationEngine + ProfileAdvisor (unchanged)
                                                       ↓
                                          TransactionValidator (extended stale fields) → TransactionStore (unchanged flow) → ComparisonEngine (extended metrics) → AuditLog (unchanged)
```

- `RecommendationEngine` becomes *thin* registry iterator, no hard-coded `if`.
- `Transaction` remains `safety::Transaction` (`Transaction.h:26`); `OptimizationCapability::toTransaction` fills `target`/`beforeHash`/`preconditions`/`expectedBenefit` numeric.
- `cli/p4_cli.cpp` `explain` dispatches via registry, not hard-coded candidate mock.

#### P19 Candidate Domains (Reference Implementations)

- P19a: `flatpak-unused` (R1, no auth, no reboot, `benefitGB` numeric)
- P19b: `journal-vacuum` (R1, no auth, `benefitGB` numeric)
- Optional stretch (only if a+b complete under scope): `autostart-single-entry` (R1, no auth, user overlay)

Each capped at **one transaction in tests** to prove framework, not to optimize host.

#### P19 Safety / Measurement / Rollback / Approval / Verification / Regression Requirements

See §11. P19 must satisfy all rows; tests must prove `stale`/`idempotency`/`TOCTOU`/`flock`/`audit` for new `hash` fields, plus `FileSafety` `allowlist` for new paths (`flatpak` cache, `autostart` dir), plus `Comparison` new thresholds.

#### P19 Tests

- **Unit:** `test_p19_capability_registry` (5 caps registered, `isApplicable` true/false table, `collectEvidence` deterministic, `toRecommendation` sorted by benefit, no duplicates, JSON deterministic)
- **Unit:** `test_p19_flatpak_evidence` (fixture `flatpak list` duplicates 2 → `benefit 1.2GB` `confidence 0.85` `R1`, `snapshot` hash stable, `verify` `free +1.1GB` `SUCCESS`)
- **Unit:** `test_p19_journal_evidence` (fixture `diskUsage 3.2G` → `benefit 2.0GB` `confidence 0.90`, `stale journalDiskHash` `CHANGED→FAILED`, `UNAVAILABLE→unverifiable`)
- **Security:** `test_p19_stale_extended_matrix` (4 new fields ×3 states 12 cases + existing 19 = 31 total, `expected`/`observed` deterministic, `auditOperation stale_flatpakState`)
- **Integration:** `test_p19_comparison_extended` (12 cats existing + 6 new: `storage.free +0.6GB SUCCESS`, `journal -2.0GB SUCCESS`, `userSystemd +40% REGRESSION`, `unavailable not SUCCESS`)
- **Fixture isolation:** All under `/tmp/polaris-test-root/p19_*`, `ls /run/polaris/helper.sock` not exists, `stat /etc/fstab` `2026-08-31 21:19` unchanged, `ls ~/.local/state/polaris/profile.json` not exists, 33→38/38 `ctest 0.8s 100%`.

#### P19 CLI UX

```
polaris scan --capabilities --json          # → { capabilities: [{id:"flatpak-unused", applicable:true, benefitGB:1.2, confidence:0.85, risk:"R1"}, …] }
polaris_p4 explain flatpak-unused --json     # → WHY NOW: flatpak duplicates 2, free 15%, WHAT WILL CHANGE: flatpak uninstall --unused, WHAT WILL NOT CHANGE: NVIDIA/zram/akonadi, REJECTION CONDITIONS: already optimal / in use / unknown profile
polaris_p4 capabilities list                  # → human table
polaris_p4 transaction preview flatpak-unused # → test fixture preview only (no real DNF)
```

#### P19 Documentation

- `docs/P19_PLAN.md` — interface, registry, providers, thresholds, tests, safety.
- `docs/P19_IMPLEMENTATION_REPORT.md` — files changed, evidence model, `beforeHash` extension, comparison new metrics, CLI, tests, host verification.
- Update `docs/ARCHITECTURE.md` — Provider → Capability → Transaction layer.
- Update `docs/TRANSACTION_MODEL.md` — new `hash` fields.
- Update `docs/ROADMAP.md` — `P19 COMPLETE`, next `P20` (real helper wiring) *not* auto-planned.
- Update `docs/PROJECT_STATE.json` — `currentPhase P19`, tests `38/38`.

#### P19 Explicit Non-Goals (Repeated for Clarity)

See §13. No helper privileged mutation, no batch, no `zram`/`sysctl`/`grub`/`modprobe`/`governor`/`scheduler`/`network` tuning, no `akmods`/`dracut`.

#### P19 Exit Criterion

`RecommendationEngine` is no longer hard-coded; `polaris scan --capabilities --json` lists 5 capabilities with numeric `benefitGB` and `confidence` derived from `PerformanceBaseline`; 2 reference capabilities proven on fixtures (`apply.completed` on `/tmp`, `Comparison SUCCESS`); 38/38 tests, no host mutation, no `helper.sock`.

---

## 13. Explicit Non-Goals

Polaris **must not**:

1. Become an unsafe “one-click system tweaker” — no `Optimize All` button, no batch (`no batch changes` — `docs/PROJECT_STATE.json:554`), no 100 tweaks at once.
2. Auto-disable `Akonadi`/`bluetooth`/`avahi`/`cups` when `uses* = unknown` — `REQUIRES_USER_CONFIRMATION` (`ProfileAdvisor.cpp:84`) must remain; `unknown` ≠ `no`.
3. Modify `/etc/fstab`, `zram`, `GRUB`, `modprobe`, `sysctl`, `governor`, `scheduler`, `kernel cmdline` — these are `R3+` and have no measurable benefit on a healthy host (`RecommendationEngine.cpp:82` `Do NOT disable zram` is correct).
4. Add `zram`/`swap`/`swappiness`/`vfs_cache_pressure` optimizer — `pressure 0`, `zram 0B used` healthy; tuning would be speculative.
5. Introduce `IpcProtocol` `exec`/`run`/`shell`/`sudo`/`dnf`/`systemctl` allowlist entries without per-action `org.polaris.*` polkit review — currently `ping`/`info` only (`core/ipc/IpcProtocol.h`).
6. Weaken `FileSafety` allowlist (`FileSafety.h:18`), `TransactionValidator` fail-closed (`TransactionValidator.h:114`), `StateMachine` fail-closed (`docs/P12_PLAN.md`), `AuditLog` `fsync`+hash chain (`AuditLog.h`), `ReadOnlyGuard`, `TransactionLock` `flock` (`TransactionLock.h`), `RecoveryDetector` detection-only (`RecoveryDetector.h`).
7. Infer user workflow from hardware — `usesBluetooth` not derived from `bluetooth adapter present`; `usesKMail` not from `kmail installed` (`ProfileAdvisor.cpp` comment `do not infer`).
8. Guess `unavailable` metrics (`Comparison.h:38` `available false` with `note`, not `0`).
9. Claim `observedBenefit` without `ComparisonEngine::compare` (`ComparisonEngine.h:12`) — `expectedBenefit` ≠ `observedBenefit` (`Transaction.h:58`).
10. Re-enable `mssql-server` or re-`swap` `akmod-nvidia` 470xx→610 open, modify `fstab`, modify `zram`, create `helper.sock`/`transaction.lock` on real host, or create/modify `~/.local/state/polaris/profile.json` during analysis — protected facts (`docs/P18_FINAL_REPORT.md:283`).

---

## 14. Risk Assessment

| Risk | Likelihood | Impact | Mitigation (Existing) | Mitigation (P19) |
|------|------------|--------|----------------------|------------------|
| Disable `Akonadi` while user uses KMail → break PIM (14 agents 1302 M) | High if auto-tweak | High — mail lost, `akonadictl` down | `ProfileAdvisor::canConsiderAkonadi` `BLOCKED_BY_USER_WORKFLOW` (`ProfileAdvisor.cpp:9`) + P17 `REJECTED` authoritative | Add `usesBaloo` tri-state similarly; registry `isApplicable` checks `profile` first; `ExplanationEngine` `whatWillNotChange "Akonadi will remain"` |
| `flatpak uninstall --unused` removes runtime still needed → app fails | Medium | Medium — app won't launch | Not yet mitigated (no capability) | `RealFlatpakProvider` pins `refcount` check; `precondition flatpak.runtime.<id>.refcount==0`; rollback `flatpak install <id>`; `R1` only |
| `journal --vacuum` deletes needed logs → forensics lost | Medium | Low-Med (if `MaxRetention` 14d) | Not mitigated | Snapshot `journal --disk-usage` + warn `rollback N/A logs >14d lost`; require `approval` + `confidence 0.90` + `benefit >1GB` threshold |
| `autostart Hidden=true` disables `kscreen` → display broken | Medium | Medium | Single-file `nvidia-settings-user.desktop` pilot only (`FileSafety.h:27`) + `allowlist` | Copy-on-write overlay in `~/.config/autostart` (never edit `/etc/xdg/autostart`); `allowlist` `isAllowedPath` + `isSymlink` TOCTOU; blocklist `kwin`/`plasmashell` |
| `timer drop-in` syntax error → timer never fires / `locate` stale | Low | Low | No timer tuning today | Validate `systemctl cat` after write + `systemctl daemon-reload` exit 0 check; `BackupEngine` backup original drop-in; `validation.failed` if `daemon-reload` fails; one timer per transaction |
| Stale preview → apply on wrong state (e.g., kernel `7.1.10→7.1.11` after preview) | Medium | High if mutated | `TransactionValidator::validateForApply` 7 fields + `TOCTOU` (`TransactionValidator.h:114`) + `finalPreconditionValidation` after `BACKUP_CREATED` (`TransactionValidator.h:379`) + `StateMachine BACKUP_CREATED→APPLYING` (`TransactionStore.h:338`) + 33/33 tests | Extend matrix with `flatpakStateHash` etc.; `expected`/`observed` in `AuditLog` (`AuditLog.h:10`) |
| `flock` advisory lock bypass → concurrent apply | Low | Medium | `TransactionLock flock LOCK_EX|LOCK_NB` `0600` `FD_CLOEXEC` (`TransactionLock.h`) + test `concurrent 4 threads → exactly one holds` | P19 tests `test_p19_lock_recovery` + CI `test ! -f /run/polaris/transaction.lock` |
| Helper socket world-writable / symlink attack | Low | High | `IpcServer::checkParentSecurity` not symlink, not `S_IWOTH`, `0700` `0600` `SO_PEERCRED` (`docs/P14_IMPLEMENTATION_REPORT.md:6`) + `FileSafety` `validatePath` rejects `..`; `;|&` | No new helper ops; `allowedOperations ping/info` only |
| Regression false positive (`boot +10%` on noise) | Medium (boot 8.515 s ±0.5) | Low — false `REGRESSION` verdict | `ComparisonEngine::defaultThresholds` stored with result (`ComparisonEngine.h:21`), `isDeterministic true`, `unavailable` handling | Add `storage.free`/`journalDisk` thresholds deterministic `+0.5GB`; boot `relative_pct` requires `isBootCritical true` (`Comparison.h:42`) blame-top vs critical-chain already (`BottleneckEngine.cpp:146`) |
| Product risk — add too many tweaks → become “tweaker” | Medium if P19 becomes “add 100” | High — loses safety reputation | Roadmap `PROJECT_COMPLETE_WITH_LIMITATIONS` + `STOP` (`docs/P18_FINAL_REPORT.md:311`) — correctly not adding tweaks | P19 objective is **framework not tweaks**; cap 2 reference capabilities; `Explicit Non-Goals` forbids `zram`/`sysctl`/`grub` |

Overall product risk if *not* doing P19: **Polaris remains a safe but idle optimizer** — correct but not useful on next host. Overall safety risk if *doing* P19 incorrectly (100 tweaks): **High** — mitigated by framing P19 as extensibility + 2 R1 reference caps, not as tweaks batch.

---

## 15. Final Recommendation

**Short verdict: `PROJECT_COMPLETE_WITH_LIMITATIONS` remains accurate for P1–P18. A narrowly-scoped P19 *is* justified if the original product goal (“safe optimizer worth using on any Fedora/KDE”) is still desired; otherwise `STOP` is the honest final state.**

1. **Whether Polaris is currently a real optimizer:** **No.** It is a **safe measurement/explainability/transaction platform** that *has* optimized this host twice (P6, P7) and now correctly idles. `RecommendationEngine` has 4 actionable + 4 informational hard-coded recs, no numeric benefit model, no registry, no `flatpak`/`journal`/`autostart`/`timer`/`baloo` providers. `TransactionManager` is stub; `IpcProtocol` is `ping/info` only — so even the 2 proven optimizations are not reachable via current CLI.

2. **What it can actually optimize:** See §3 — **2 user-file overlays** (`autostart` `Hidden=true` R1, already `Hidden=true` on this host), **1 privileged file** (`/etc/fstab` stale swap — already fixed, not CLI-reachable), **2 historically manual** privileged operations (`mssql disable` R2, `nvidia 470xx` R3) not reachable via hardened helper, **3 always-emitted informational** (flatpak reclaim string, do-not-disable zram, reboot to measure) not executable. Test fixtures are abundant but not optimizer capability.

3. **What is missing:** See §4 and §9 — **capability framework** (registry, numeric `benefitGB`/`confidence`, `CurrentState` snapshot per domain) plus **5 providers** (flatpak, journal disk, autostart, timers, baloo) and **corresponding bottleneck classification** (not just `systemd-analyze`/`GPU`/`mssql`). Without this, `P17 NO_ACTION` will repeat on every healthy host.

4. **Whether another engineering phase is justified:** **Yes, but only as a framework phase.** “Add 7 more hard-coded `if` branches” is **not** justified and would recreate the problem. “Replace the hard-coded `if` chain with a registry that can derive numeric benefit from real `statvfs`/`journal`/`autostart` evidence” **is** justified.

5. **What that phase should accomplish:** **P19 — Optimization Capability Framework** per §12 — interface `IOptimizationCapability` in `core/engines/capabilities/`, `OptimizationRegistry` replacing `RecommendationEngine` hard-code, 5 new read-only `Real*Provider`s, `PerformanceBaseline` + `TransactionValidator` `hash` extensions, `ComparisonEngine` `storage.free`/`journalDisk` metrics, 2 reference capabilities (`flatpak-unused` R1, `journal-vacuum` R1) proven on fixtures (`/tmp/polaris-test-root/p19_*`), CLI `scan --capabilities`, `29→38` tests, **no real privileged apply**, docs. Total scope ~`core/engines/capabilities/*` + 5 providers + `BaselineEngine` extension + `Comparison` thresholds; explicitly not helper wiring, not batch, not `zram`/`sysctl`/`grub`.

**If you choose `STOP`:** Update `README.md` “What It Can Do Today” to state *“Polaris is a safe, evidence-backed system health measurement and explainability tool with two verified one-time optimizations on this hardware; on a healthy host it correctly recommends no action”* — do not market as “optimizer that continuously finds gains.” Keep the safety pipeline intact.

**If you choose `P19`:** Do **not** immediately implement; first approve `docs/P19_PLAN.md` (interface + registry + provider list + stale matrix + tests) and confirm the *two* reference capabilities are the right first ones for a generic Fedora/KDE host.

---

## Appendix — Audit Evidence

- Repository: `~/Documents/polaris` (11 dirs, `isGitRepo false` per `docs/P18_FINAL_REPORT.md:244`)
- Build: `cmake 3.28`, `C++20`, `OpenSSL` (`CMakeLists.txt:1`), `ctest 33/33 0.70s 100%` (`docs/P18_FINAL_REPORT.md:198`)
- Host verified 2026-09-01 03:25: `systemd-analyze 8.515s userspace` `graphical.target 8.514s`, `free 5.6Gi avail`, `zram 0B`, `lspci CLAIMED driver=nvidia`, `modinfo 470.256.02`, `nvidia-smi 470.256.02`, `akonadictl running 14 agents`, `mssql disabled`, `fstab 3 entries #39b0 swap commented 2026-08-31 21:19`, `helper.sock not exists`, `profile.json not exists` (`docs/P18_FINAL_REPORT.md:22–60`)
- Safety pipeline preserved: `READ→MEASURE→ANALYZE→EXPLAIN→RECOMMEND→PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY→COMPARE→REGRESSION→AUDIT` (`docs/ARCHITECTURE.md:5`)
- FileSafety allowlist `FileSafety.h:18`, `IPC allowlist` `IpcProtocol.h`, `TransactionValidator.h:114`, `TransactionStore.h:178` `APPROVAL→VALIDATION→BACKUP→FINAL VALIDATION→APPLY`, `AuditLog` `fsync`+hash chain, `ProfileAdvisor` TriState, `Comparison` thresholds.

*End of gap analysis. No host mutation performed. No P19 code created.*
