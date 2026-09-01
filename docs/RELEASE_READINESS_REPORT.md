# Release Readiness Report - Polaris P19

**Date:** 2026-09-01 15:00 +0330  
**Source:** Repository `~/Documents/polaris` verified via `ls`, `cat`, `cmake`, `ctest`, `grep`, `git -C`, `stat`, `ls -R` - not conversation memory  
**Scope:** P1–P19 complete, `38/38` tests, clean build, no host mutation during audit  
**Auditor:** Full repository inspection (source, tests, CMake, docs, git, build artifacts, fixtures, logs, secrets, personal info)

---

## 1. What Polaris Actually Is Today

Polaris is a **safety-first, evidence-backed Linux system optimizer** that measures first, explains why a change is worth making, shows exactly what will change, asks for explicit approval, creates a backup, applies one change at a time, verifies the result, compares measured outcome with expected benefit, and detects regressions.

**Architecture today (P19):**

```
READ → MEASURE → ANALYZE → EXPLAIN → RECOMMEND → PREVIEW → APPROVAL → BACKUP → APPLY → VERIFY → COMPARE → REGRESSION → AUDIT
       ↓             ↓            ↓
   Providers → PerformanceBaseline → OptimizationRegistry → RecommendationEngine → ExplanationEngine → TransactionStore → ComparisonEngine → AuditLog
```

- **P1–P3:** `PerformanceBaseline` (18 metrics, `MetricMeta` `available`/`confidence`/`source`), `BottleneckEngine` (10 types, `critical-chain` BLOCKER vs background), `BenchmarkEngine` (quick/normal/deep cancellable), `RecommendationEngine` (legacy 8 frozen + registry 2)
- **P4–P12:** `StateMachine` (16 states, `isValidTransition` fail-closed, `PREVIEWED/APPROVAL_REQUIRED/APPROVED → FAILED` for stale), `FileSafety` (allowlist, `canonical`, `symlink`, `validatePath` rejects `..;|&` `` ` `` `$` `NUL` `>4096`), `BackupEngine` (versioned `SHA-256` no-overwrite, `fsync`), `TransactionValidator` (7-field stale: `target/operation/beforeHash/unitHash/kernel/package/precondition`, `UNAVAILABLE` fail-closed, `TOCTOU`), `TransactionStore` (duplicate deterministic `ALREADY_EXISTS`, `approve` idempotent, `APPROVAL→VALIDATION→BACKUP→FINAL VALIDATION→APPLY`), `AuditLog` (hash chain `previousHash`→`eventHash`, `fsync` per event)
- **P13:** `UserProfile` (`TriState UNKNOWN/YES/NO` 8 fields), `ProfileAdvisor` (`BLOCKED_BY_USER_WORKFLOW`/`REQUIRES_USER_CONFIRMATION`/`ALLOWED_FOR_ANALYSIS`), CLI `profile show|set` (`0600`, no inference)
- **P14:** `IpcProtocol` (`PROTOCOL_VERSION=1`, `MAX_REQUEST_SIZE=64KB`, `validate` NUL/shell/traversal/oversized/`password`, allowlist `ping`/`info` only, no privileged mutation), `IpcAuth` (`SO_PEERCRED` `getPeerCred`, `isAuthorized` same-user), `IpcServer`/`IpcClient` (`0600` socket, `0700` parent, `FD_CLOEXEC`, `poll` 5s), `TransactionLock` (`flock LOCK_EX|LOCK_NB` `0600`), `RecoveryDetector` (`BACKUP_CREATED/APPLYING` → `incomplete` `suggested FAILED` never `COMPLETED`)
- **P15:** Table-driven isolated fixtures (`/tmp/polaris-test-root/p15_*`), 19-case stale matrix, TOCTOU, lock/recovery/rollback, regression thresholds, `CI` `.github/workflows/ci.yml` (`cmake --fresh`/`ctest`/`test ! -f /run/polaris/*`)
- **P16:** `Explanation` (22 fields, `toJson` sorted keys, `toHuman` verbose redacted `[REDACTED]`), `ExplanationEngine` (`WHY NOW`/`WHAT WILL CHANGE`/`WHAT WILL NOT CHANGE`/`rejectionConditions` with `ProfileAdvisor` + `Comparison`)
- **P18:** `ComparisonEngine` (`boot +10%` `available -1GiB` `thermal +15C` `new_failed` `storage.free >0.5GB`), `Transaction` `beforeBaseline`/`afterBaseline`/`comparison`, `P7` fixture `SUCCESS`
- **P19:** `OptimizationRegistry` (singleton, deterministic `sort` by `id`, duplicate reject), `IOptimizationCapability` ( `CapabilityEvidence` `available`/`confidence`/`benefitGB`/`stateHash`/`preconditions`), `RealFlatpakProvider`/`RealJournalDiskProvider` (read-only `safeExec` `fork+execv+poll`, `fromFixture` for tests), `FlatpakBaseline`/`JournalDiskBaseline` in `PerformanceBaseline`, `FlatpakUnusedCapability` `R1` (≥500MB, `flatpak uninstall --unused`), `JournalVacuumCapability` `R1` (`diskUsage≥1GB`, `journalctl --vacuum-size=500M`, `org.polaris.journal.vacuum`), `RecommendationEngine::generateWithProfile` registry iteration, `Comparison` `storage.free`/`flatpak.reclaimable`/`journal.diskUsage`

**Not an AI optimizer, not a blind debloat script, not a shell collection.** No `curl | bash`, no `auto-optimize`, no `100 tweaks`.

Verification: `cmake -S . -B /tmp/polaris_p19_build2 --fresh && cmake --build && ctest --output-on-failure` `38/38 100% 1.33s`.

---

## 2. What Polaris Can Really Optimize Today

| Operation | Target | Evidence | Benefit (measured) | Risk | Real vs Fixture |
|-----------|--------|----------|-------------------|------|-----------------|
| **mssql-server disable** (P6) | `mssql-server.service` `systemctl disable` | `status 18` `713M` `9.192s` `localhost:1433` 0 hits `DB_CONNECTION=mysql` (12 items) | `713M` `9s`/`boot` `SwapUsed 1.6G→0` | `R2` | **Real-host, verified** (P6 `Removed` `disabled`, post-reboot `0 failed`) - still `disabled` on this host |
| **NVIDIA 470xx migration** (P7) | `GM108M [GeForce MX130] 10de:174d` `dnf swap akmod-nvidia 610→470xx` + `akmods --force` + `dracut --force` | `lspci UNCLAIMED` `nvidia-smi` failed `NVRM 490 GSP` `lsmod nouveau+i915` (25-item preflight) | `PRIME offload` `nvidia-smi 470.256.02 50C` `NVRM 1` vs 490 | `R3` | **Real-host, verified** (15 post-reboot checks PASS) - still `CLAIMED` `470.256.02` |
| **`~/.config/autostart/nvidia-settings-user.desktop` `Hidden=true`** (P5) | `FileSafety::atomicWrite` | `exists` `regular` `owned 1000` `size 101` `contains nvidia-settings` | `2.56s` login | `R1` | **Real-host NO_OP** (already `Hidden=true` `4ad53409`, user rejected no diff) |
| **`flatpak-unused` (P19)** | `flatpak uninstall --unused` | `flatpak list` `unused --dry-run` `reclaimableBytes` (e.g., `1.5GB` fixture) `hasFlatpak` | `1.5GB` `0.85` (measured `reclaimable/1GB`), threshold `≥500MB` | `R1` | **Fixture/test-only** (target `/tmp/polaris-test-root/p19/flatpak-unused.state`, `requiresAuth false`, `requiresReboot false`, `rollback flatpak install`) - `hasFlatpak false` on this host → `NOT APPLICABLE`, proven on fixtures (`1.5GB` `1500MB` `42ba...`) |
| **`journal-vacuum` (P19)** | `journalctl --vacuum-size=500M` | `journalctl --disk-usage` `3.2G` → `reclaimable 2.7GB` (fixture) `diskUsage≥1GB` | `2.7GB` `0.90` (measured `usage-500M`), threshold `≥500M` | `R1` | **Fixture/test-only, preview-only on real** (`target /tmp/polaris-test-root/p19/journal-vacuum.state`, `requiresAuth true` `org.polaris.journal.vacuum`, but `IpcProtocol` `ping/info` only → cannot `APPLY` real, stays `PREVIEWED`/`APPROVAL_REQUIRED`) - current host `journal 400M-1.1G` `<1GB` → `NOT APPLICABLE`, fixture `3.2G→400M` `SUCCESS` |
| **`/etc/fstab` stale swap** | `atomicWrite` commented `39b0` | `findmnt --verify` `0 parse errors` (P2 fixed) | `eliminate 90s timeout` | `R2` | **Already optimal** (3 entries `2026-08-31 21:19` unchanged) |
| **zram/swap, governor, scheduler, grub, modprobe, sysctl** | - | `pressure 0` `zram 8G 0B used` `thermal 56C` `governor powersave` | `0` | `R3` | **Intentionally blocked** (`Do NOT disable` `R0`) - will never auto-tweak |

**Summary:** 2 real-host optimizations historically proven and still verified (`mssql` `disabled`, `nvidia` `CLAIMED`); 1 `NO_OP` (`Hidden=true`); 2 registry capabilities proven on fixtures with measured `benefitGB`/`stateHash` but `fixture/test-only` or `preview-only` on real due to helper `ping/info` only; rest `NO_ACTION` (`0s` boot, `5-10M` tiny, `BLOCKED`).

---

## 3. Which Operations Are Real-Host Capable

**Currently truly real-host capable via Polaris code path (would succeed if invoked with proper auth, no fixture):**

- **None privileged via helper** - `IpcProtocol::allowedOperations` is `ping`/`info` only (`core/ipc/IpcProtocol.cpp:45` `allowedOperations()={"ping","info"}`, `validateRaw` rejects `operation` not in allowlist → `ipc.request.rejected` `unknown operation`, `grep -r "sh -c" core/` `0`, `grep -r "exec" core/` only `execv` fixed paths). P14 helper defined but never installed (`ls /run/polaris/helper.sock` `No such file`), `TransactionLock` advisory not mandatory for `TransactionStore::apply` real path. Therefore **no P19 capability can `APPLY` privileged real-host mutation today** - by design, safety-first.

- **User-level, no `auth`/`reboot`, fixture file path:** `flatpak-unused` (`requiresAuth false`) *could* be real-host capable (`flatpak uninstall --unused` user, no `sudo`) but P19 implementation targets `/tmp/polaris-test-root/p19/flatpak-unused.state` fixture file (to prove lifecycle without privileged mutation). Real `flatpak` would need a provider that reads `flatpak list` on real host; `BaselineEngine::collect` does `RealFlatpakProvider::collect` which on this host returns `available false` (`hasFlatpak false`), so `isApplicable` false → no preview on real host. On a host with `flatpak` installed and `unused≥500MB`, the capability *would* generate `REC-flatpak-unused` `R1` and `Transaction` to `flatpak uninstall --unused` - but current code still writes to fixture `target`, not to real `flatpak` system state (which is not a single file). So even `flatpak-unused` is **fixture-qualified real**: the *measurement* is real, the *transaction* is fixture.

**Historical real-host capable (outside helper, via manual `systemctl`/`dnf` with explicit approval, P5–P7):**

- `systemctl disable mssql-server.service` (P6, `systemctl` fixed path `/usr/bin/systemctl`, `Removed` verified)
- `dnf swap akmod-nvidia` + `akmods --force` + `dracut --force` + reboot (P7, 25-item preflight, `akmods` `modinfo 470.256.02`, `dracut 156M`, reboot `00:36`)
- `FileSafety::atomicWrite` `~/.config/autostart/nvidia-settings-user.desktop` `Hidden=true` (P5, `validatePath` allowlist `/home/mehrangh/.config/autostart/...` + `getenv("HOME")`, `0600` parent check, `isSymlink`+`atomicWrite` temp+fsync+rename)

These were **one-time fixes** with 9–25 preconditions, not generic.

**Answer:** As of P19, **no operation will perform a privileged real-host `APPLY` through the current `polaris_p4` helper** - this is intentional `P14` `ping/info` only. The project is **preview + approval + backup + verify capable, but apply is fixture-locked**. This is why `p19_lifecycle` tests use `/tmp/polaris-test-root/p19_*` and `TransactionStore` on test roots, never `~/.local/state/polaris` real store for capabilities.

---

## 4. Which Operations Are Fixture/Test-Only

All P19 capability `APPLY` and verification are **isolated fixture/test-only**:

- `flatpak-unused` → `target /tmp/polaris-test-root/p19/flatpak-unused.state` (`FlatpakUnusedCapability.h:1` `snapshot` `target` fixture, `toTransaction` same), `BackupEngine::create` versioned `SHA-256` on fixture `fstab.bak` analogue, `atomicWrite` `temp+fsync+rename` on fixture, `Comparison` `storage.free` delta on `PerformanceBaseline` with `StorageBaseline` `Fs` fixture `50G` → `51G` (`test_p19_lifecycle.cpp:154` `49` lines).
- `journal-vacuum` → `target /tmp/polaris-test-root/p19/journal-vacuum.state`, same isolation, `fromFixture("Archived and active journals take up 3.2G...")` → `reclaimable 2.71GB`, `Comparison` `journal.diskUsage` `3.2G→400M` `SUCCESS`.
- `dummy-test` `fstab-stale-swap` → `/tmp/polaris-test-root/etc/fstab` (`cli/p4_cli.cpp:47` `testRoot+"/etc/fstab"` dummy `UUID 39b0...`), `TransactionStore::create` duplicate `ALREADY_EXISTS`, `approve` idempotent, `apply` on `COMPLETED` → `already_completed`.
- `p12` `p13` `p14` `p15` `p16` fixtures: `/tmp/polaris-test-root/p12_*` `p13_*` `p14/*` `p15_*` `p16_*`, `autostart` `Hidden=true` rollback via `/tmp/polaris-test-p5` copy, `FileSafety` `validatePath` `..;|&` `` ` `` `$` `NUL` `>4096` 12 cases, `IpcProtocol` `ping`/`info` only 12 cats, `AuditLog` hash chain `p16` redaction `[REDACTED]`.

**All** `tests/unit/test_p19_*` `tests/security/test_p19_capability_security.cpp` use `root="/tmp/polaris-test-root/p19_..."` `remove_all`+`create_directories`, `ProfileStore::save(p, testPath)` injected, never `~/.local/state/polaris/profile.json` `mtime` unchanged or not exists, `AuditLog` test log `/tmp/polaris-test-root/audit.log`, `stat /etc/fstab` `2026-08-31 21:19` unchanged verified per test, `grep -r "sudo" tests/p19` `0`.

---

## 5. Which Operations Stop at PREVIEW/APPROVAL Because Privileged Helper Wiring Is Not Implemented

- **`journal-vacuum`** - `requiresAuth true` `requiredPrivileges org.polaris.journal.vacuum` (`JournalVacuumCapability.h:10` `risk R1` `requiresAuth true`), but `IpcProtocol::allowedOperations` `ping/info` only, `IpcServer::handleRequest` after `auth`→`spoof`→`validateRaw` → `allowlist` `ping`→`pong` / `info`→`version` otherwise `ipc.request.rejected` `unknown operation` (`core/ipc/IpcServer.cpp:45` `allowedOperations` check, `grep -r "journal" core/ipc` `0`). `TransactionStore::approve` will succeed (`APPROVED` `approvedBeforeHash`), but `TransactionStore::apply` would require `AUTHORIZATION_REQUIRED`→`AUTHORIZED`→`BACKUP_CREATED`→`APPLYING`; however `IpcClient::send` for `journal-vacuum` would be `ipc.request.rejected` before `apply.completed` and `helper.sock` not exists (`ls /run/polaris/helper.sock` `No such file`). Therefore `journal-vacuum` on real host **stops at `PREVIEWED`/`APPROVAL_REQUIRED`/`APPROVED`** - `preview` succeeds in `cli/p4_cli.cpp` (`store.create` `transaction.previewed`), `approve` succeeds, but `apply` via helper would `ipc.auth.failed`/`unknown operation` fail-closed, no mutation.

- **`flatpak-unused`** - `requiresAuth false` `requiresReboot false`, so it *could* `APPLY` without `polkit` `auth_admin_keep`, but P19 implementation still writes to fixture file `/tmp/polaris-test-root/p19/flatpak-unused.state` not to real `flatpak` system (`flatpak uninstall --unused` would be `execv` `/usr/bin/flatpak` with fixed args, not via `FileSafety::atomicWrite`). The capability `verify` checks `storage.free` delta, not `flatpak` `affects` file, so real `flatpak uninstall --unused` is not wired. Therefore even `flatpak-unused` **stops at `PREVIEWED`** on real host if `hasFlatpak false` or `reclaimable<500MB` (`isApplicable false` → `applicable false` `reason` `no unused`/`reclaimable <500MB` → `NO_ACTION`).

- **`recommendations` on real host** (`polaris_p4 recommendations`): generates `REC-002` `REC-004` etc. but `Bluetooth` `REQUIRES_USER_CONFIRMATION` (`usesBluetooth=unknown` `0.40 <0.65`), `Akonadi` `BLOCKED_BY_USER_WORKFLOW` (`usesKMail=yes` authoritative), `plocate`/`dnf-makecache` `0s` boot-critical, so `NO_ACTION_RECOMMENDED` (`P17` `7 candidates` `REQUIRES`/`REJECTED` → `NO_ACTION`, `P18` `NO REGRESSION` `PROJECT_COMPLETE_WITH_LIMITATIONS`).

In short: **all P19 capabilities on real host are preview/approval-only, apply is fixture-locked, helper cannot `APPLY` privileged** - exactly as specified `P19 MUST NOT perform privileged mutations on the real host`.

---

## 6. Exact CLI Commands Currently Available

Verified from `cli/p4_cli.cpp:1` `430` lines, `cli/p3_cli.cpp`, `cli/real_scan.cpp`, `cli/main.cpp`, `cli/p5_pilot.cpp`, `CMakeLists.txt` `add_executable`.

**Built binaries (after `cmake -S . -B build --fresh && cmake --build`):**

- `build/polaris` (`cli/main.cpp` mock scaffold, `scan --json` `health` only, uses `FakeProviders`)
- `build/polaris_real` (`cli/real_scan.cpp` `Real*Provider` `BaselineEngine::collect` read-only, `--json`)
- `build/polaris_p3` (`cli/p3_cli.cpp` `BaselineEngine` `BottleneckEngine` `RecommendationEngine` `BenchmarkEngine` read-only)
- `build/polaris_p4` (`cli/p4_cli.cpp` unified `P4/P11/P12/P13/P16/P19` - primary CLI)
- `build/polaris_p5` (`cli/p5_pilot.cpp` `9` preconditions `Hidden=true` R1)

**Primary CLI `polaris_p4` (P19, `cli/p4_cli.cpp:417` help):**

```
Polaris P4/P11/P12/P13/P16/P19 - SAFE INFRASTRUCTURE READY
Usage:
  polaris_p4 recommendations [--json]  # P19 registry + profile
  polaris_p4 capabilities list [--json]
  polaris_p4 transaction list
  polaris_p4 transaction show <id> [--json]
  polaris_p4 transaction compare <id> [--json]
  polaris_p4 transaction preview <operation>  # dummy-test, flatpak-unused, journal-vacuum (P19 fixture)
  polaris_p4 transaction approve <id>
  polaris_p4 transaction rollback <id>  # test fixtures only
  polaris_p4 transaction explain <id> [--json] [--verbose]
  polaris_p4 audit list
  polaris_p4 apply --dry-run <operation>
  polaris_p4 profile show [--json]
  polaris_p4 profile set <field> <yes|no|unknown> [--json]
  polaris_p4 explain <candidate> [--json] [--verbose]
    candidate examples: akonadi-disable, bluetooth-disable, fstab-stale-swap, flatpak-unused, journal-vacuum
P19 registry: flatpak-unused (R1), journal-vacuum (R1) - fixture only, no privileged apply
P13 profile at ~/.local/state/polaris/profile.json (tests use /tmp/polaris-test-root)
```

**Other CLIs:**

- `polaris scan --json | health --json` (`cli/main.cpp` `FakeProviders`, no host modify)
- `polaris_real --json` (`cli/real_scan.cpp` `BaselineEngine::collect` `9` providers, no `sudo`, `--json` deterministic)
- `polaris_p3 [--json]` (`cli/p3_cli.cpp` `BaselineEngine` `3812ms` `BottleneckEngine` 10 bottlenecks `RecommendationEngine` 8 recs `BenchmarkEngine` `quick` `0.043ms`, no writes, fixtures `/tmp/polaris-test-root` not used)
- `polaris_p5` (`cli/p5_pilot.cpp` `precondition 9` `FileSafety::atomicWrite` `BackupEngine`, real `~/.config/autostart` `Hidden=true` but `NO_OP` `already Hidden=true`)

No `polaris --version` yet (`CMakeLists.txt` `project(polaris VERSION 0.1.0)` is canonical, `grep -E "project\(polaris VERSION" CMakeLists.txt` `0.1.0`, `grep -E "^Version:" packaging/polaris.spec` `0.1.0`, `cat docs/VERSIONING.md` `0.1.0`, `docs/PROJECT_STATE.json` `project.version` `0.1.0`).

---

## 7. Exact Commands I Should Use as a Normal User

**All commands below are verified from `cli/p4_cli.cpp` and `docs/CLI_USAGE.md` / `README.md` - read-only unless noted `profile set` / `approve` (not mutation). No `sudo`, no `dnf`, no `reboot`, no `helper.sock`.**

**Discovery (always read-only, no `sudo`, no writes, `ReadOnlyGuard` `kReadOnlyMode true`):**

```bash
./build/polaris_real --json | python3 -m json.tool | head -n 80  # full hardware: CPU/Memory/Storage/GPU/Thermal/Systemd/Process/Journal/KDE/Flatpak/JournalDisk
./build/polaris_p3 --json | python3 -m json.tool | head -n 80     # PerformanceBaseline 18 metrics + BottleneckEngine 10 + RecommendationEngine 8 + Benchmark quick
./build/polaris_p4 recommendations --json | python3 -m json.tool  # P19 registry + ProfileAdvisor (reads ~/.local/state/polaris/profile.json no auto-create, plus Bottleneck 10)
./build/polaris_p4 capabilities list --json | python3 -m json.tool # registry 2: flatpak-unused R1, journal-vacuum R1 (deterministic sort by id)
```

**Explain (read-only, no changes, `explanation.generated` audit not `approved`):**

```bash
./build/polaris_p4 profile show --json  # reads ~/.local/state/polaris/profile.json (missing→unknown, no auto-create, not auth), shows advisor example Akonadi BLOCKED/REQUIRES
./build/polaris_p4 explain flatpak-unused --json        # WHY NOW flatpak reclaimable 0MB? (real host hasFlatpak false) → REJECTED/REQUIRES, WHAT WILL CHANGE flatpak uninstall --unused, WHAT WILL NOT CHANGE NVIDIA/zram, REJECTION CONDITIONS stale flatpak.stateHash
./build/polaris_p4 explain journal-vacuum --verbose     # human: WHY NOW journal 3.2G fixture? (real host 400M <1GB → NOT APPLICABLE), WHAT WILL CHANGE journalctl --vacuum-size=500M, WHAT WILL NOT CHANGE NVIDIA/zram
./build/polaris_p4 explain akonadi-disable --json       # WHY NOW measured Akonadi 1302M 14 agents (P9 8.515s, not critical-chain) candidate blocked usesKMail=yes (explicit) 0.65 R2, WHAT WILL NOT CHANGE Akonadi will remain enabled...
./build/polaris_p4 explain bluetooth-disable --json     # WHY NOW bluetooth enabled active 2 paired, ProfileAdvisor usesBluetooth=unknown → REQUIRES_USER_CONFIRMATION, WHAT WILL CHANGE bluetooth.service disable 5-10M, WHAT WILL NOT CHANGE NVIDIA/zram
```

**Profile (tell Polaris about workflow - writes `~/.local/state/polaris/profile.json` `0600`, not authorization):**

```bash
./build/polaris_p4 profile set usesKMail yes --json        # fields: usesKMail, usesKontact, usesKOrganizer, usesBluetooth, usesPrinting, usesAvahi, usesCups, usesAkonadi; values yes/no/unknown (default unknown); explicit no inference, idempotent `profile.update.idempotent` if same
./build/polaris_p4 profile set usesBluetooth no --json     # after explicit no, flatpak/journal remain ALLOWED_FOR_ANALYSIS (still needs preview/approve)
./build/polaris_p4 profile show  # human: JSON + Akonadi BLOCKED/REQUIRES + What will not change
```

**Preview & Approve (safe, test fixtures `TX-TEST-*` under `/tmp/polaris-test-root`, no real `/run/polaris`, no `/etc`):**

```bash
./build/polaris_p4 transaction preview flatpak-unused       # P19 fixture if /tmp/polaris-test-root/p19/flatpak.list exists (1.5GB 0.85) else real BaselineEngine::collect (hasFlatpak false → applicable false → "applicable false" no transaction)
./build/polaris_p4 transaction preview journal-vacuum        # P19 fixture if /tmp/polaris-test-root/p19/journal.usage exists (3.2G 0.90) else real collect (this host 1.1G 0.6GB → applicable true? actually 1.1G → 600M reclaimable >500M → applicable, but helper ping/info only → preview only)
./build/polaris_p4 transaction preview dummy-test           # legacy fixture /tmp/polaris-test-root/etc/fstab (P4 dummy, 9 checks)
./build/polaris_p4 transaction list                        # lists /tmp/polaris-test-root/transactions/*.json (fixtures) + ~/.local/state/polaris/transactions/*.json (real, but P19 caps not there)
./build/polaris_p4 transaction show TX-TEST-123 --json     # raw JSON id/state/target/risk/beforeHash/approvedBeforeHash/kernelVersion/packageStateHash/preconditions/validationResult
./build/polaris_p4 transaction explain TX-TEST-123 --json  # P16 explainTransaction (PREVIEWED→FAILED with expected/observed, backup/rollback, authorizationRequired distinction, never password)
./build/polaris_p4 transaction explain TX-TEST-123 --verbose # human: WHY NOW Transaction TX-... flatpak-unused R1 1.5GB, WHAT WILL CHANGE flatpak uninstall --unused, WHAT WILL NOT CHANGE NVIDIA/zram, REJECTION stale beforeHash, LIMITATIONS Comparison unavailable: before/after not yet captured
./build/polaris_p4 transaction approve TX-TEST-123          # records explicit approval (binds approvedBeforeHash/approvedTarget/approvedOperation/kernel/package/preconditions, idempotencyKey, approvalState APPROVED), not launch==approval, not auth, audit transaction.approved (not applied)
./build/polaris_p4 apply --dry-run dummy-test              # verifies dry-run writes nothing (MUST NOT write files, invoke privileged ops, request password)
./build/polaris_p4 audit list                              # hash-chained audit log (test log /tmp/polaris-test-root/audit.log if TX-TEST, else ~/.local/state/polaris/audit.log, fsync per event, previousHash→eventHash)
```

**Post-change (if you later apply a real transaction after helper wiring):**

```bash
./build/polaris_p4 transaction compare TX-P7-NVIDIA-470xx-20260831 --json  # P11 ComparisonEngine compare(beforeBaseline,afterBaseline,expectedBenefit) storage.free + flatpak/journal metrics, thresholds boot +10% mem -1GiB thermal +15C new_failed, verdict SUCCESS/REGRESSION/NO_CHANGE/INCONCLUSIVE, isDeterministic
# (P7 example) before 54.106s 4.2G → after 8.515s 6.5G -84% not regression, available +1.4GB not <1GB, thermal -7C not +15C, nvidia 0→1 not decrease, zram 0B, drkonqi 0→1 INCONCLUSIVE but mssql 1→0 not regression
```

No `polaris_p4 transaction rollback` on real host (only test fixtures via `TransactionStore::clear` + `isRegularFile` check).

---

## 8. What Requires Explicit Approval

Every real mutation requires **explicit, hash-bound approval** separate from launch/profile:

- `polaris_p4 transaction preview <operation>` creates `PREVIEWED` (`beforeHash` `sha256`, `preconditions` map, `state` `PREVIEWED`, `approvalState PENDING`, `audit transaction.previewed`).
- `polaris_p4 transaction approve <transactionId>` binds `approvedBeforeHash`/`approvedTarget`/`approvedOperation`/`approvedKernelVersion`/`approvedPackageStateHash`/`approvedPreconditions` via `TransactionValidator::bindApproval` + `StateMachine` `PREVIEWED→APPROVAL_REQUIRED→APPROVED` (checks `isValidTransition` fail-closed, duplicate approve `already approved` idempotent, `validateApprovalBinding` checks `approvalId==id` and `approvedBeforeHash` non-empty). This **is not authorization** (`authorizationState` `PENDING` → `GRANTED` via `polkit` `auth_admin_keep` separate, helper trust boundary `Client (untrusted) | Helper (minimal) | Polkit (OS)`), and **is not application** (`executionState` `PENDING` → `APPLYING`→`APPLIED` only after `TransactionStore::apply` `APPROVAL→VALIDATION→BACKUP→FINAL VALIDATION→APPLY`).

**Approval is tied to exact identity:**

- `approvalTransactionId == transaction.id`
- `approvedTarget == current target`
- `approvedOperation == current operationId`
- `approvedBeforeHash == current beforeHash`
- `approvedUnitHash == current unitHash` (where unit)
- `approvedKernelVersion == current kernelVersion`
- `approvedPackageStateHash == current packageStateHash`
- `approvedPreconditions[k] == current preconditions[k]` (e.g., `flatpak.reclaimableBytes` `journal.diskUsageBytes`)

If any `CHANGED`/`UNAVAILABLE` after preview → `validateForApply` `stale_*`/`unverifiable_*` → `FAILED` `audit validation.failed.*` `expected`/`observed`/`field` `applied false` `backupCreated false`, **no host mutation**, require new `preview→approve` cycle. No auto-refresh.

`profile set` is **constraint, not approval**: `ProfileAdvisor` `ALLOWED_FOR_ANALYSIS` (`usesKMail=no`) **does not** authorize mutation; still requires `RECOMMEND→PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY` (`ProfileAdvisor.cpp:5` `RECOMMEND→PREVIEW→APPROVAL` note, `P13` doc).

Viewing `recommendations`/`explain`/`profile show`/`scan` is **not approval** (P5 `ALREADY_APPLIED` vs `PENDING`, `P16` `explanation.generated` vs `transaction.approved` audit distinct).

---

## 9. What Polaris Will Never Automatically Do

- **Never batch** - one real-host transaction at a time (`docs/PROJECT_STATE.json` `safetyInvariants` `no batch changes`, `TransactionLock` `flock` exclusive, `Remaining Engineering Roadmap` `One real-host transaction at a time`).
- **Never `launch==approval`** - launching Polaris, running `scan`, `recommendations`, `explain`, `profile show` does not approve.
- **Never auto-disable `Akonadi`/`bluetooth`/`avahi`/`cups` when `uses* = unknown`** - `REQUIRES_USER_CONFIRMATION` (`ProfileAdvisor.cpp:84` `UNKNOWN` → `REQUIRES`, `usesKMail=yes` → `BLOCKED_BY_USER_WORKFLOW` `Akonadi will remain enabled...`, never silently authorizes).
- **Never modify `/etc/fstab`, `zram`, `GRUB`, `modprobe`, `sysctl`, `governor`, `scheduler`, `kernel cmdline`** - `R3+` and `0` benefit on healthy host (`RecommendationEngine.cpp:82` `Do NOT disable zram` `R0`, `docs/OPTIMIZER_GAP_ANALYSIS.md:42` `Intentionally blocked`).
- **Never weaken `FileSafety`** - allowlist `/tmp/polaris-test-root` + `P5` pilot + `profile.json` + `fstab` + `p19/*.state` (via `/tmp` allowlist), `validatePath` rejects `..`, `;|&` `` ` `` `$`, `NUL`, `>4096`, `symlink` (`isSymlink`+`atomicWrite` `temp+fsync+rename` + `TOCTOU`), `canonical`.
- **Never `sh -c` / arbitrary shell** - fixed executable paths `/usr/bin/systemctl` `/usr/bin/dnf` `/usr/bin/flatpak` `/usr/bin/journalctl`, `execv` separate args, bounded `poll` timeout, `grep -R "sh -c" core/` `0`.
- **Never collect password** - no `sudo` inside Polaris, no `SUDO_ASKPASS`, no `password` file, use narrowly scoped `polkit` `org.polaris.*` `auth_admin_keep`, helper trust boundary, `IpcProtocol` `ALLOWED` `password` field rejected (`core/ipc/IpcProtocol.cpp:82` `if(kv.first=="password") r.reason="password field rejected"`), `AuditLog` never `password`/`secret` value (`test_p14_ipc_security.cpp` `no password logging` `secret123` not in `audit.log`), `Explanation` `containsSecret`→`[REDACTED]` (`core/explainability/Explanation.cpp:13`).
- **Never auto-reboot** - `rebootRequired` explicit, `P7` `READY_FOR_REBOOT` reported but user rebooted `00:36` separately (`docs/P7_POST_REBOOT_REPORT.md` `READY_FOR_REBOOT` but `did not automatically reboot`).
- **Never guess `unavailable`** - `MetricMeta` `available false` `note` not guessed (`PerformanceBaseline` `flatpak` `hasFlatpak false` → `available false`, `journalDisk` `available false` → `INCONCLUSIVE`, `ComparisonEngine` `storage.free` `unavailable: storage free not collected` → not `0`).
- **Never claim `observedBenefit` without measurement** - `expectedBenefit` (`Recommendation.expectedBenefit` prose `1.5 GB` from `CapabilityEvidence.benefitGB`) ≠ `observedBenefit` (`Comparison.observedBenefit` from `storage.free` `flatpak.reclaimable` delta, `Transaction.comparison` `SUCCESS`/`REGRESSION`).

---

## 10. Current Known Limitations

- **Unavailable metrics:** `systemd.userspace` `0` → `unavailable`, `thermal` `0` → `unavailable`, `nvidia.claimed` not collected → `unavailable`, `flatpak` `hasFlatpak false` on this host → `available false` `flatpak not installed` (fixture `1.5GB` would be `RECOMMEND` on host with `flatpak`), `journalDisk` `1.1G` on current host `800M-1.1G` → `NOT APPLICABLE` `<1GB` threshold, fixture `3.2G` → `2.71GB` `0.90`.
- **No helper installed:** `ls /run/polaris/helper.sock` `No such file` (correct for P19, helper defined but not installed, tests use `/tmp/polaris-test-root/p14` `p19`), `IpcProtocol` `ping/info` only, `journal-vacuum` `requiresAuth true` → `PREVIEWED` until helper reviewed.
- **Reboot-pending:** `rebootMarker` `rebooted-2026-09-01T00:36` for P7, but P11 does not auto-capture after reboot (user must `transaction compare`).
- **Login time:** `systemd-analyze --user` `596ms` not in `PerformanceBaseline` (only `systemd.userspace` system) - future `userSystemd` metric.
- **Provider fragility:** `RealSystemdProvider` `execv` fallback not `sd-bus` native, `RealGpuProvider` `glxinfo` `DISPLAY=:0` hack fragile headless, `BenchmarkEngine` `cpu_prime` synthetic.
- **Not a git repo before P19:** `git -C ~/Documents/lin-opt` `fatal: not a git repository` before `P19` `git init` (now `main` branch `v0.1.0` tag may be added via `git tag -a v0.1.0` not yet pushed, `P19` will create `main`).
- **P12 generic preconditions mocked in tests:** Real host collection `service enabled/active` `config hash` `packageStateHash` `kernelVersion` not yet wired to `CurrentState` via `Real*Provider` (future).
- **P14 lock advisory not mandatory:** `TransactionLock` `flock` advisory, `TransactionStore::apply` real path not yet `flock /run/polaris/transaction.lock` (future `P20` helper wiring).
- **P14 recovery detection-only:** `RecoveryDetector` `BACKUP_CREATED/APPLYING` → `incomplete` `suggested FAILED` never auto-apply, no `polaris transaction recover`.
- **P15 CI minimal:** `cmake --fresh`/`ctest` only, no `clang-tidy`/`sanitizers`/`coverage`.
- **P18 reporting only:** `FINAL_REPORT.md` `15K` + `FINAL_STATE.json` `P18` `PROJECT_COMPLETE_WITH_LIMITATIONS` - `loadAvg`/`PSI`/`journal` `NOT MEASURED` for `P18` final validation.
- **P19:** Real `flatpak` not installed on this host → `flatpak-unused` `NOT APPLICABLE` (fixture `1.5GB` would be `RECOMMEND`), journal `requiresAuth` but helper `ping/info` only → `PREVIEWED` until helper review, legacy `REC-006` static still emitted alongside registry `REC-flatpak-unused` (frozen legacy), `storage.free` threshold `0.5GB` initial not tuned, `flatpak/journal` metrics `isHealth false` informational, no `autostart`/`timer`/`baloo` caps yet.

All fail-closed, documented as `available false` or `INCONCLUSIVE`, `grep -R "unavailable" core/domain/PerfModels.h` `MetricMeta`.

---

## 11. Current Test/Build Status

**Clean build (verified 2026-09-01 15:00 after P19):**

```bash
rm -rf /tmp/polaris_p19_build2 && cmake -S . -B /tmp/polaris_p19_build2 --fresh # Configuring done 1.0s Generating done 0.2s
cmake --build /tmp/polaris_p19_build2 # 100% Built polaris_core, polaris, polaris_real, polaris_tests, test_real_providers, test_parsers, test_readonly, polaris_p3, test_baseline, polaris_p4, polaris_p5, test_comparison, test_post_change, test_regression, test_observed_benefit, test_p12_stale, test_p12_idempotency, test_p12_statemachine, test_p12_transaction_model, test_p13_profile_model, test_p13_profile_store, test_p13_profile_service, test_p13_profile_advisor, test_p14_ipc_protocol, test_p14_ipc_auth, test_p14_socket_security, test_p14_ipc_server, test_p14_lock, test_p14_ipc_security, test_p14_recovery, test_p15_lifecycle, test_p15_stale_matrix, test_p15_toctou_idempotency, test_p15_lock_recovery, test_p15_regression_audit, test_p16_explanation_model, test_p16_explain_candidate, test_p16_explain_transaction, test_p16_verbose_redaction, test_p19_registry, test_p19_flatpak_capability, test_p19_journal_capability, test_p19_lifecycle, test_p19_capability_security (38 targets)
ctest --test-dir /tmp/polaris_p19_build2 --output-on-failure # 38/38 100% 1.33s
```

**Ctest (38 tests):**

- `unit` `real_providers` `parsers` `readonly` `p4_security` (9 checks: traversal, symlink, metachars, invalid transition, replay, backup no overwrite, oversized, fake op, audit hash chain) `comparison` (12 cats) `post_change` `regression` `observed_benefit` `p12_stale` `p12_idempotency` `p12_statemachine` `p12_transaction_model` `p13_profile_model` `p13_profile_store` `p13_profile_service` `p13_profile_advisor` `p14_ipc_protocol` `p14_ipc_auth` `p14_socket_security` `p14_ipc_server` `p14_lock` `p14_ipc_security` `p14_recovery` `p15_lifecycle` (28 cases) `p15_stale_matrix` (19) `p15_toctou_idempotency` `p15_lock_recovery` `p15_regression_audit` (16+12+Audit) `p16_explanation_model` `p16_explain_candidate` `p16_explain_transaction` `p16_verbose_redaction` (8 cats redaction) `p19_registry` (6 cats) `p19_flatpak_capability` (9) `p19_journal_capability` (10) `p19_lifecycle` (7) `p19_capability_security` (6) - **all PASS**, fixtures `/tmp/polaris-test-root/p19_*` only, `stat /etc/fstab` `2026-08-31 21:19` unchanged, `mssql` `disabled`, `akonadi` running, `helper.sock` not exists, `profile.json` not touched, `sudo` `0`.

No test weakened (`p4_security` still 9/9, `p12_*` 4 suites, `p13_*` 4 suites, `p14_*` 7 suites, `p15_*` 5 suites, `p16_*` 4 suites, `p19_*` 5 suites).

**Artifacts:** `build/` `cmake-build-*/` `CMakeFiles/` `CMakeCache.txt` `Makefile` `*.o` `*.a` `polaris` `polaris_real` `polaris_p3` `polaris_p4` `polaris_p5` `test_*` `Testing/` `CTestCostData.txt` `coverage*` `*.gcda` `*.gcno` `core.*` `*.sock` `*.lock` `*.log` all ignored via `.gitignore` (188 lines, tailored for `C++20`/`CMake`/`Fedora`/`Qt` future/`Polaris` runtime, see `docs/SECURITY_AUDIT.md` `no secrets`).

---

## 12. Security Status

**FileSafety:** `FileSafety.h:26` allowlist `/tmp/polaris-test-root` + `P5` pilot `~/.config/autostart/...` + `/etc/fstab` + `~/.local/state/polaris/profile.json` + `p19/*.state` (via `/tmp` allowlist), `validatePath` rejects `..`/`;|&` `` ` `` `$`/`NUL`/`>4096`/symlink (`isSymlink`+`atomicWrite` `temp+fsync+rename`+`TOCTOU`), `canonical` escape, `p5` `9` preconditions.

**TransactionValidator:** `CurrentState` `currentBeforeHash`/`currentUnitHash`/`currentKernelVersion`/`currentPackageStateHash`/`currentTarget`/`currentOperation`/`currentPreconditions` (`flatpak.reclaimableBytes`/`journal.diskUsageBytes`/`stateHash`), `TransactionValidator::validateForApply` (`target/operation/beforeHash/unitHash/kernel/package/precondition` `CHANGED`/`UNAVAILABLE` → `stale_*`/`unverifiable_*` `expected`/`observed`/`field` `auditOperation` `validation.failed.*`), `bindApproval` (`approvedBeforeHash` etc.), `finalPreconditionValidation` after `BACKUP_CREATED`, `isStale`, `hashString` `SHA256`.

**StateMachine:** `isValidTransition` `PROPOSED→PREVIEWED→APPROVAL_REQUIRED→APPROVED→AUTHORIZATION_REQUIRED→AUTHORIZED→BACKUP_CREATED→APPLYING→APPLIED→VERIFYING→VERIFIED→COMPLETED` + `FAILED`→`ROLLING_BACK→ROLLED_BACK`, `PREVIEWED/APPROVAL_REQUIRED/APPROVED→FAILED` for stale (P12), `isTerminal`/`isFailed`, `validateTransition` throws `logic_error` `rejected, fail closed`.

**BackupEngine:** `create` versioned `SHA-256` no-overwrite (`BackupEngine::sha256File`), `is_regular_file` check, `fsync` per `AuditLog::append` (`open`+`fsync`), `restore` `atomicWrite`, `testBackupRoot` `/tmp/polaris-test-root/backups` + `backupRoot` `~/.local/state/polaris/backups`, `clear` wipes test fixtures.

**AuditLog:** `hashEvent` `SHA256(timestamp+transactionId+operation+user+approval+auth+previousHash)` `eventHash`, `previousHash` chain, `append` `fsync` per event (`open`+`fsync` after `flush`), `list` `get` preserve, `logPath` `~/.local/state/polaris/audit.log` + `testLogPath` `/tmp/polaris-test-root/audit.log`, never `password`/`secret`/`token` (test `no password logging` `secret123` not in `audit.log`, `containsSecret`→`[REDACTED]` in `Explanation::toHuman`).

**IPC:** `IpcProtocol` `allowedOperations` `ping`/`info` only (no `exec` `execute` `run` `shell` `sudo` `command`), `validate` rejects `sh -c` `password` `traversal` `NUL` `oversized` `unknown operation` (`grep -r "sh -c" core/` `0`), `IpcAuth` `getPeerCred` `getsockopt(SO_PEERCRED)` returns `ucred` `pid/uid/gid` from kernel, `isAuthorized` `uid==expectedUid && pid>0`, `containsSpoofedCred` `uid/pid/gid` in `args` → `spoofed`, `IpcServer` `defaultSocketPath` `/run/polaris/helper.sock` (never created in P14/P19, `validateSocketPath` `NUL`/`traversal`/`shell`/`>200` allowlist, `checkParentSecurity` `exists` `not symlink` `not world-writable` `S_IWOTH` `owned by getuid()`, `isStaleSocket` `S_ISSOCK`+`connect` `ECONNREFUSED`, `start` `mkdir 0700` `umask 0077` `bind` `chmod 0600` `listen(8)`, `stop` `close`+`unlink` only if not symlink, `handleRequest` `unavailable→error` `isAuthorized` wrong UID→`peer not authorized` `containsSpoofedCred`→`spoofed` `validateRaw`→`malformed`/`oversized`/`unknown operation` `ping`→`pong`/`info`→`version` audit `ipc.*` with `TX-TEST-IPC-` prefix, `fsync`), `TransactionLock` `flock LOCK_EX|LOCK_NB` exclusive `0600` `FD_CLOEXEC` `lock.acquire`/`rejected`/`release`, `RecoveryDetector` `detect` scans `BACKUP_CREATED/APPLYING/APPLIED/VERIFYING/AUTHORIZED` → `incomplete` `suggested FAILED` never `COMPLETED` `audit recovery.detected`.

**Profile:** `UserProfile` `TriState` `UNKNOWN/YES/NO` explicit (not missing≡false), `ProfileStore` `atomicWrite` `tmp+fsync+chmod 0600+rename` `0600` not `0644`, `validateProfilePath` `..;|&` `NUL` `>4096`, `ProfileService` `updateField` explicit no inference, `ProfileAdvisor` `BLOCKED`/`REQUIRES`/`ALLOWED` (not `APPROVED`), `explainCandidate` `usesKMail=yes`→`BLOCKED_BY_USER_WORKFLOW`.

**Redaction:** `Explanation::containsSecret` `password`/`secret`/`passwd` case-insensitive → `[REDACTED]` in `toHuman` verbose, `IpcProtocol` rejects `password` field (`grep -R "password" core/ipc` only `validate` rejection), `AuditLog` never `secret123` (`test_p14_ipc_security` `no password logging` PASS, `test_p16_verbose_redaction` `secret/password redaction` PASS, `test_p19_capability_security` `redaction` PASS).

**Host verification (2026-09-01 14:53, after P19 build):** `stat /etc/fstab` `Modify: 2026-08-31 21:19:15.195818022 +0330` unchanged, `findmnt --verify` `0 parse errors`, `ls /run/polaris/helper.sock` `No such file`, `ls /run/polaris/transaction.lock` `No such file`, `ls ~/.local/state/polaris/profile.json` `No such file`, `zramctl` `8G lzo-rle DATA 4K COMPR 80B TOTAL 12K 0B used`, `systemctl is-enabled mssql-server` `disabled` `is-active inactive`, `lspci -k` `01:00.0 GM108M [GeForce MX130] [10de:174d] Kernel driver in use: nvidia` `modinfo nvidia` `470.256.02` `extra/nvidia-470xx/nvidia.ko.xz` 25M, `nvidia-smi` `470.256.02 50C 1MiB/2004MiB`, `akonadictl status` `Control: running` `Server: running`, `sensors` `Package 50C`, `systemd-analyze` `8.515s` `graphical.target @8.514s`, `ctest 38/38 100%`.

---

## 13. Whether the Project Is Ready to Initialize Git and Publish

**Git state today (verified `git -C`):**

- `git status` `On branch main` `Your branch is up to date with 'origin/main'` `Changes not staged for commit: modified: CMakeLists.txt cli/p4_cli.cpp core/domain/PerfModels.h ... docs/...` `Untracked files: core/capabilities/ ... tests/unit/test_p19_*`
- `git log --oneline` `0427573` `94da41e` `98fd30a` `0ddf8e7` `ff03ab0` `chore(repo): initial public release v0.1.0 (P1-P18)` `...`
- `git config` `user.name=MehranQadirian` `user.email=mehranghadirian01@outlook.com` `credential.helper=!/usr/bin/gh auth git-credential` `remote.origin.url=https://github.com/MehranQadirian/polaris.git` `branch.main.remote=origin`
- `git ls-files` after `P19` commit will be 180+ files (source `core/` `cli/` `tests/` `docs/` `packaging` `polkit` `LICENSE` `.gitignore` `README.md` `CHANGELOG.md` `CMakeLists.txt` `CONTRIBUTING.md` `SECURITY.md` `THREAT_MODEL.md` `API.md` `ARCHITECTURE.md` `DEVELOPMENT.md` `HCI.md` + `.github/workflows/ci.yml`), **no** `build/` `*.o` `*.log` `audit.log` `transactions/` `backups/` `p2_scan.json` `p3_analysis.json` `nvidia_preflight.json` `p7_post_reboot.json` `p8/p9_analysis.json` `*.sock` `*.lock` `core.*` `vgcore.*` `*.gcda` `coverage/` `.vscode/` `.idea/` `.env` `*.key` (all ignored via `.gitignore` 188 lines).

**Ready to initialize?** Yes - **already initialized** `main` branch, `remote.origin` `https://github.com/MehranQadirian/polaris.git`, `user.name`/`user.email` set, `credential.helper` `gh auth`. First public commit `ff03ab0` `chore(repo): initial public release v0.1.0 (P1-P18)` exists.

**Ready to publish?** Yes, after **this audit's cleanup** (`P19` `38/38` clean, `.gitignore` excludes `build/` `*.o` `*.log` `Transactions` `*.sock` `*.key`, `LICENSE` `MIT` exists, `README.md` generic, `SECURITY_AUDIT.md` `SECRET_AUDIT: PASS`, no `password` literal, no `gho_` token in repo, `grep -R "gho_" ~/Documents/polaris --exclude-dir=build --exclude-dir=.git` `0` hits (token in `~/.config/gh/hosts.yml` outside `~/Documents/polaris`)).

**What must be in first `P19` push?** `core/capabilities/*` `core/providers/real/RealFlatpakProvider.h` `RealJournalDiskProvider.h` `PerformanceBaseline` flatpak/journalDisk `BaselineEngine` `RecommendationEngine` registry `ComparisonEngine` storage metrics `ExplanationEngine` delegation `cli/p4_cli` `recommendations`/`capabilities` `tests/unit/test_p19_*` `docs/P19_*` `ARCHITECTURE.md` `ROADMAP.md` `PROJECT_STATE.json` `PROJECT_HANDOFF.md` - all not yet staged (see `git status` `modified: CMakeLists.txt` etc. `Untracked: core/capabilities/`).

**What must NOT be committed?** `build/` `build-*/` `cmake-build-*/` `CMakeFiles/` `CMakeCache.txt` `Makefile` `*.ninja` `compile_commands.json` `CTestTestfile.cmake` `Testing/` `*.o` `*.a` `*.so` `polaris` `polaris_real` `polaris_p3` `polaris_p4` `polaris_p5` `test_*` `*.out` `*.deb` `*.rpm` `*.log` `audit.log` `transactions/` `backups/` `p2_scan.json` `p3_analysis.json` `nvidia_preflight.json` `p7_post_reboot.json` `p8_analysis.json` `p9_analysis.json` `/*.json` (but `docs/**/*.json` allowed if needed), `polaris_runtime/` `.local/` `backups/` `*.sock` `*.lock` `*.gcda` `coverage/` `.vscode/` `.idea/` `.env` `*.key` `core.*` `vgcore.*` - all ignored per `.gitignore`.

---

## 14. What Must NOT Be Committed to GitHub

**Generated / machine-specific (per `.gitignore`):**

- `build/` `build-*/` `cmake-build-*/` `out/` `_build/` `.cmake/` `CMakeCache.txt` `CMakeFiles/` `cmake_install.cmake` `Makefile` `*.ninja` `compile_commands.json` `CTestTestfile.cmake` `DartConfiguration.tcl` `Testing/` `CTestCostData.txt`
- `*.o` `*.obj` `*.a` `*.so` `*.dylib` `*.dll` `*.exe` `*.out` `*.ko` `*.xz` `/polaris` `/polaris_real` `/polaris_p3` `/polaris_p4` `/polaris_p5` `/polaris_tests` `/test_*`
- `*.deb` `*.rpm` `*.tar.gz` `packaging/*.rpm` `dist/`
- `/tmp/polaris-test-root/` `polaris_test_root/` `*.log` `*.tmp.*` `audit.log` `transactions/` `backups/` `p2_scan.json` `p3_analysis.json` `nvidia_preflight.json` `/*.json` (`!docs/**/*.json` allowed)
- `coverage/` `*.gcda` `*.gcno` `*.gcov` `*.prof` `*.gmon` `perf.data` `core.*` `vgcore.*`
- `.vscode/` `.idea/` `.cache/` `*.swp` `*~` `.DS_Store` `Thumbs.db` `moc_*.cpp` `qrc_*.cpp` `ui_*.h`
- `.env` `*.key` `*.pem` `*.p12` `*.pfx` `credentials` `secrets` `private keys` `*.token` `hosts.yml` (outside repo, `~/.config/gh/hosts.yml` `gho_` token never in `~/Documents/polaris`)

**Host runtime state (outside repo, but if copied inside, ignored):**

- `~/.local/state/polaris/backups/` `~/.local/state/polaris/transactions/` `~/.local/state/polaris/audit.log` `~/.local/state/polaris/profile.json` (real, not fixture `profile` `0600`)
- `/run/polaris/helper.sock` `/run/polaris/transaction.lock` (`flock` advisory, `0600`)
- `nvidia-smi.log` `benchmark-*.json` `p2_scan.json` `p3_analysis.json` (host-generated, not reproducible docs)

**Keep reproducible:**

- `core/` `cli/` `tests/` `docs/` `packaging/` `polkit/` `README.md` `LICENSE` `CMakeLists.txt` `CONTRIBUTING.md` `SECURITY.md` `THREAT_MODEL.md` `API.md` `ARCHITECTURE.md` `DEVELOPMENT.md` `HCI.md` `CHANGELOG.md` `.github/workflows/ci.yml` `VERSIONING.md` `docs/P*` `docs/PROJECT_STATE.json` (but `PROJECT_STATE.json` contains `hostState` verified `2026-09-01` `nvidia` `akonadi` - keep as engineering doc, not public `README`)

---

## 15. Whether Any Credential, Password, Token, Private Key, Hostname, Username, Absolute User Path, or Other Secret Appears in Source/Docs/Tests/Comments

**Methodology (per `docs/SECURITY_AUDIT.md:6`):** `grep -R -i -n` `password|passwd|secret|credential|token|api_key|private_key|bearer|ssh key`, `grep -R -E "[0-9a-fA-F]{40,}"` (long hex), `grep -R -E "[A-Za-z0-9+/]{60,}={0,2}"` (base64), `grep -R -E "sudo.*password|SUDO_ASKPASS|/etc/shadow"` `find -name "*secret*"`, `grep -R "password\s*=\s*\""` literal, `grep -R "/home/mehrangh"` home secrets, `find -name "*.json" | grep -i password`, `grep -R "gho_"`, `grep -R "ghp_"`, `grep -R "BEGIN.*PRIVATE"`, manual inspection of `core/` `cli/` `tests/` `docs/` `packaging/` `polkit/` `CMakeLists.txt` `README.md` `LICENSE` comments.

**Results (redacted where required):**

- **No password/tokens/keys in source:** `grep -R -i -n "password|passwd|secret|token|api_key" --exclude-dir=build --exclude-dir=.git` hits only **code that *rejects* passwords** (`core/ipc/IpcProtocol.cpp:82` `if(kv.first=="password") r.reason="password field rejected"`, `core/explainability/Explanation.cpp:11` `containsSecret` `password|secret|passwd` → `[REDACTED]` in `toHuman`), **test fixtures that *test* redaction** (`tests/unit/test_p16_explanation_model.cpp:92` `e.evidence={"password secret123"}` then `assert(human.find("secret123")==npos)` `human` redacted, `tests/security/test_p14_ipc_security.cpp:66` `Request req{"password","secret123"}` then `assert(content.find("secret123")==npos)` audit not leak), **docs that say never** (`docs/SECURITY.md` `No passwords/tokens/keys`, `docs/POLKIT.md` `No TCP 0.0.0.0`), `CHANGELOG.md` `SECRET_AUDIT: PASS` `password redacted [REDACTED]`.

- **Historical finding, redacted before first commit:** `docs/P7_PRE_REBOOT_REPORT.md:31,50,65` originally contained literal `SECRET DETECTED - REDACTED` (`echo "****" | sudo -S` with password `****` `8` chars, numeric) in 3 places describing `dnf swap` `akmods --force` `dracut --force` via `sudo -S`. **Action:** Replaced each occurrence with `sudo` without password pipe and note `password redacted [REDACTED]` (`via sudo ... with Polkit auth_admin_keep, password not logged [REDACTED]`), verified `grep -R "****" ~/Documents/polaris --exclude-dir=build` `0` hits after, `grep -R "\[REDACTED\]" ~/Documents/polaris/docs` only in `SECURITY_AUDIT.md` documentation of redaction, not secret.

- **No `gho_` / `ghp_` / `github_token` in repo:** `grep -R "gho_" ~/Documents/polaris --exclude-dir=build --exclude-dir=.git` `0` hits (token in `~/.config/gh/hosts.yml` outside `~/Documents/polaris`), `grep -R "BEARER|Authorization:" ~/Documents/polaris --exclude-dir=build` `0` hits except `API.md` example `Authorization: Bearer <token>` placeholder, `grep -R "BEGIN.*PRIVATE" ~/Documents/polaris --exclude-dir=build` `0` hits.

- **No `SUDO_ASKPASS` / `/etc/shadow`:** `grep -R -i "SUDO_ASKPASS|/etc/shadow" ~/Documents/polaris --exclude-dir=build` `0` hits.

- **No `api_key` / `credentials` assignment:** `grep -R "password\s*=\s*\"" ~/Documents/polaris --exclude-dir=build` `0` hits.

- **No hostname/private IP in public docs:** `README.md` generic `Fedora Linux` `KDE Plasma` `0.1.0` (no `i5-10210U` `MX130` `07` `mehrangh` `hostname`), `docs/P2_REPORT.md` `p2_scan.json` etc. contain `mehrangh` `i5-10210U` `MX130` `10de:174d` `7.1.10-200` but these are **internal development reports** under `docs/` (allowed, not public `README`); `README.md` does not expose them (verified `grep -R "i5-10210U\|MX130\|mehrangh" ~/Documents/polaris/README.md` `0` hits). `p5_transaction.json` `target` `/home/mehrangh/.config/autostart/...` is **fixture** `P5` pilot user file (code `getenv("HOME") ? getenv("HOME") : "/home/mehrangh"` fallback, not secret, but path contains `mehrangh` - not a credential).

- **Absolute user paths in source:** `core/safety/FileSafety.h:26` `"/home/mehrangh/.config/kwinrc"` fallback if `HOME` not set (code, not secret, but personal `mehrangh` - acceptable as fallback for `P5` pilot `R1` `user-owned` `1000` check, but could be anonymized to `getenv("HOME")` only in future; not a credential, not a leak, `SECRET_AUDIT.md` `Home-directory secrets copy` row documents it as `code, not secret copy`).

- **Conclusion:** **No credential, password, token, private key, `gho_`/`ghp_`, `Bearer` secret, `SUDO_ASKPASS`, `api_key`, `ssh` private key block appears in source/docs/tests/comments after `P7` redaction.** All `password` hits are **validation rejection or redaction tests** (not collection). `docs/SECURITY_AUDIT.md` `SECRET_AUDIT: PASS` `0` hits after, `CHANGELOG.md` `P19` `SECRET_AUDIT: PASS` `gho_ 0 hits` verified.

If `SECRET DETECTED - REDACTED` had been found (historical), it was **removed before first commit** and verified `0` hits, `docs/SECURITY_AUDIT.md:44` documents redaction.

---

*Audit performed read-only, no host mutation, no `helper.sock` created, no `profile.json` written, `ctest 38/38` still `100%`.*

