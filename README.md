# Polaris: Linux Performance & System Health Platform

**Codename: POLARIS** | **Target: Fedora Linux + KDE Plasma** | **Version: 0.1.0** | **Status: P18 PROJECT_COMPLETE_WITH_LIMITATIONS - 33/33 tests, no outstanding approved transaction**

I wanted to optimize my Linux system, but I didn't want a tool that blindly disables services, runs random shell commands, or tells me a change is good just because a number got smaller.

So I built Polaris for myself, a safety-first, evidence-driven platform that measures the actual machine, explains what it found, and only changes the host after explicit approval, backup, and verification.

> **Polaris is not** a blind `debloat` script, not an automatic service killer, not a collection of `curl | bash`, not a benchmark-only tool, and not an optimizer that changes everything automatically. It is a safety-oriented optimization framework that follows `READ → MEASURE → ANALYZE → EXPLAIN → RECOMMEND → PREVIEW → APPROVE → BACKUP → APPLY → VERIFY → COMPARE → REGRESSION → AUDIT` and fails closed whenever state cannot be proven valid.

---

## Why Polaris Exists

On my daily Fedora KDE machine, I wanted to answer:

- How healthy is my system **right now** on **this** hardware?
- What is actually slowing it down, with evidence, confidence, and `critical-chain` vs `background` distinction?
- What should I change, with expected benefit, risk, reboot/rollback, and user-workflow compatibility?
- Can I undo it?
- What happened after I applied it, did the observed benefit match the expected benefit, and did any regression occur?

Early prototypes just ran `systemd-analyze blame` and suggested disabling whatever was on top. That is not evidence. Polaris was designed to require **before/after measurement**, **threshold-based regression detection**, and **explicit approval** before any host mutation.

The evolution was natural:

- **P1 Architecture**: API-first, headless `libpolaris_core` (C++20, no Qt), GUI-ready, safety model `READ→...→AUDIT`
- **P2 Read-Only**: real `proc`/`sys`/`D-Bus` providers via `execv` separate args, no `sh -c`, no password, `ReadOnlyGuard`
- **P3 Performance**: `BaselineEngine` 15 metrics `MetricMeta` `available`/`confidence`, `BottleneckEngine` 10 bottlenecks `critical-chain` `BLOCKER` vs `background`, `BenchmarkEngine` `quick` `prime compute 2..2000`, `RecommendationEngine` 7 recs `evidence/confidence/benefit/risk/rollback`
- **P4 Safety**: `StateMachine` 16 states `isValidTransition` fail-closed, `FileSafety` allowlist `validatePath` `..;|&` `` ` `` `$` `NUL` `>4096` `isSymlink`, `BackupEngine` `SHA-256` `is_regular_file` `fsync` no overwrite, `AuditLog` `hashEvent` `previousHash` chain `fsync`, `polkit` `auth_admin_keep`
- **P5 L1 Pilot**: `~/.config/autostart/nvidia-settings-user.desktop` `Hidden=true` already `101` `sha 4ad53409` → correctly `ALREADY_APPLIED / NO_OP` (no host mutation, 9 preconditions, `atomicWrite` `temp+fsync+rename`)
- **P6 L2**: `mssql-server.service` `enabled` `failed` `713M` `9.192s` `model.mdf` Windows path → `systemctl disable` `Removed` `disabled` verified after reboot `0` `failed` `713M` `0` (1 real host mutation)
- **P7 L3**: `GM108M [GeForce MX130] [10de:174d]` `UNCLAIMED` `610.57.04` open `GSP` `probe error -1` `490` → `dnf swap` `akmod-nvidia-470xx` `akmods --force` `modinfo 470.256.02` `extra/nvidia-470xx` `dracut --force` `reboot 00:36` `lspci` `CLAIMED` `driver=nvidia` `nvidia-smi` `470.256.02` `PRIME` `NVIDIA GeForce MX130` `journal NVRM` `490→1` (1 real host mutation + reboot)
- **P8/P9 Discovery**: `systemd-analyze` `30.594s` `userspace 8.515s` vs `P3` `54.106s` `-84%`, re-ranked 7→6 candidates, `akonadi` `1302M` `REJECTED` because user uses `KMail/Kontact`, remaining `0s` boot-critical or `5-10M` tiny → `NO_ACTION_RECOMMENDED`
- **P11 Post-Change**: `Comparison` `SUCCESS`/`REGRESSION` `isDeterministic`, `ComparisonEngine` thresholds `boot +10%` `available -1GB` `thermal +15C` `new_failed`, `expected` vs `observed`, `Transaction` `beforeBaseline`/`afterBaseline`/`comparison`
- **P12 Hardening**: `Transaction` `beforeHash`/`approvedBeforeHash` `beforeUnitHash` `kernelVersion` `packageStateHash` `preconditions` + `TransactionValidator` pure stale `TOCTOU` `beforeHash`/`unitHash`/`kernel`/`package` + `TransactionStore` duplicate `ALREADY_EXISTS` idempotent `COMPLETED→already_completed` + `StateMachine` `PREVIEWED→FAILED` for stale + `AuditLog` `fsync`
- **P13 Profile**: `UserProfile` `UNKNOWN/YES/NO` for 8 fields `usesKMail`/`usesKontact`/`usesBluetooth` etc. `ProfileStore` `~/.local/state/polaris/profile.json` atomic `tmp+fsync+chmod 0600+rename` `0600` `ProfileService` explicit `updateField` no inference `usesKMail→usesAkonadi` not inferred, `ProfileAdvisor` `BLOCKED`/`REQUIRES`/`ALLOWED` (not `APPROVED`) + `Explain` `akonadi` `BLOCKED` because `usesKMail=yes`
- **P14 IPC/Security**: `IpcProtocol` `PROTOCOL_VERSION=1` `MAX_REQUEST_SIZE=64KB` `validate` `NUL`/`shell`/`traversal`/`oversized`/`password` `allowedOperations` `ping`/`info` only `NO PRIVILEGED MUTATION`, `IpcAuth` `SO_PEERCRED` same-user `ucred` `pid/uid/gid` from kernel not client, `IpcServer` Unix socket `0600` not `S_IWOTH` `0700` `FD_CLOEXEC` `poll` 5s, `TransactionLock` `flock` `LOCK_EX|LOCK_NB` `0600`, `RecoveryDetector` `BACKUP_CREATED`/`APPLYING`→`incomplete` `suggested FAILED` never `COMPLETED` `audit` `recovery.detected`
- **P15 CI/Test**: `TransactionValidator` fix `UNAVAILABLE`→fail-closed, 5 table-driven deterministic isolated `P15` suites `test_p15_lifecycle` `test_p15_stale_matrix` `test_p15_toctou_idempotency` `test_p15_lock_recovery` `test_p15_regression_audit` + `.github/workflows/ci.yml` `cmake --fresh` `ctest` `33→29` `0.70s`
- **P16 Explainability**: `Explanation` 22 fields deterministic sorted JSON `toHuman(verbose)` redacted `[REDACTED]` `WHY NOW` evidence-backed `WHAT WILL CHANGE` transaction-backed `WHAT WILL NOT CHANGE` explicit `rejectionConditions` deterministic `profile`/`comparison` integration `never confuses approval with authorization`, `ExplanationEngine` `explainCandidate`/`explainTransaction`/`explainComparison` + `TransactionValidator` `expected`/`observed` + `ProfileAdvisor` `BLOCKED` + `Comparison` `expected` vs `observed` `verdict`, CLI `polaris_p4 explain <candidate> [--json] [--verbose]` and `transaction explain <id>`
- **P17 Campaign 2**: `systemd-analyze` `8.515s` `graphical.target 8.514s` `critical-chain` `plasmalogin 7.301s` not `plocate` `21s`, `free` `5.8Gi` `zram` `0B`, `akonadi` `14 agents 1302M` `BLOCKED` (handoff) `bluetooth` `enabled` `active` 2 paired `REQUIRES` `5-10M` tiny, scored 7 candidates all `0s` boot-critical or `5-10M` `REQUIRES`/`REJECTED` → `NO_ACTION_RECOMMENDED`, exactly ONE `bluetooth-disable` considered but negligible, no `PREVIEWED` transaction, `audit` `explanation.generated`
- **P18 Final Report**: `systemd-analyze` `8.515s` `critical-chain` `plasmalogin` `free` `5.6Gi` `zram` `0B` `lspci` `CLAIMED` `driver=nvidia` `modinfo` `470.256.02` `nvidia-smi` `470.256.02` `glxinfo` `Mesa Intel` `PRIME` `NVIDIA GeForce MX130` `akonadictl` `running` 14 agents `mssql` `disabled` `fstab` 3 entries `stat 2026-08-31 21:19`, `Comparison` `P3` `54.106s`→`8.515s` `-84%` not `+10%` `available` `4.2→5.6` `+1.4GB` not `<-1GB` `thermal` `67→60` not `+15C` `nvidia` `0→1` not `failed`, `Transaction ROI` `TX-P6` `mssql` `713M` `0` `TX-P7` `nvidia` `470.256.02` `PRIME` `TX-P5` `NO_OP`, `Campaign 2` `NO_ACTION`, `Safety` `READ→...→AUDIT` `stale`/`idempotency`/`flock`/`recovery`/`FileSafety`/`Ipc`/`Audit` `fsync`/`Profile`/`Explainability` `33/33` `PROJECT_COMPLETE_WITH_LIMITATIONS` `no P19` justified

---

## Design Philosophy

**Safety over speed, evidence over blame, explainability over automation:**

- **READ → MEASURE → ANALYZE → EXPLAIN → RECOMMEND → PREVIEW → EXPLICIT APPROVAL → BACKUP → APPLY → VERIFY → COMPARE → REGRESSION → AUDIT**: never `CHANGE → HOPE`
- **No batch changes**: one real-host `Transaction` at a time (`TxState` `PREVIEWED→...→COMPLETED`, `TransactionLock` `flock` if ever real)
- **No `launch==approval`**: viewing a recommendation or `explain` is not `APPROVED` (`TransactionValidator::bindApproval` `approvedBeforeHash`/`approvedTarget` binding)
- **Exact approval**: `transactionId` + `target` + `operation` + `beforeHash`/`unitHash`/`kernel`/`package`/`preconditions` + `TOCTOU` `canonical` before and after `BackupEngine` → `FAILED` `stale` with `expected`/`observed` `applied=false` `backupCreated`
- **Backup before mutation**: `BackupEngine::create` versioned `~/.local/state/polaris/backups/<tx>/` `SHA-256` `is_regular_file` `fsync` `no overwrite` (`exists`→throw), if fails `FAILED` do not `APPLY`
- **Fail-closed `StateMachine`**: `isValidTransition` `PROPOSED→APPLYING` `COMPLETED→APPROVED` `FAILED→APPLYING` `PREVIEWED→APPLYING` `APPLYING→APPLYING` all `logic_error` `rejected, fail closed`, `PREVIEWED/APPROVAL_REQUIRED/APPROVED→FAILED` for stale
- **FileSafety**: allowlist `/tmp/polaris-test-root` + `~/.config/autostart/nvidia-settings-user.desktop` (`P5`) + `/etc/fstab` + `~/.local/state/polaris/profile.json` (`P13`) + `~/.local/state/polaris/` `0600` `FileSafety::validatePath` `..;|&` `` ` `` `$` `NUL` `>4096` `isSymlink` `canonical`
- **No `sh -c`, no password**: `execv` separate args `/usr/bin/systemctl` `/usr/bin/dnf` `/usr/bin/glxinfo` fixed paths, `grep -r "sh -c" core/` `0`, `grep -r "password" core/ipc` only `password field rejected` validation, `AuditLog` never `password`/`secret123` (tested `test_no_password_logging`)
- **No automatic reboot**: `rebootRequired` explicit `READY_FOR_REBOOT` reported, user rebooted `00:36` separately
- **No host mutation during discovery**: `ReadOnlyGuard` `kReadOnlyMode true` `openReadOnly` `ifstream`, `test_readonly` `stat /etc/fstab` mtime unchanged

---

## Architecture Overview

```
COLLECT → BASELINE → DETECT → CLASSIFY → EXPLAIN → RANK → PREVIEW → APPROVAL → AUTHORIZATION → BACKUP → APPLY → VERIFY → MEASURE AGAIN → LEARN
  P11 adds MEASURE AGAIN (ComparisonEngine) and LEARN
  P16 adds EXPLAIN (ExplanationEngine) with WHY NOW/WHAT WILL NOT CHANGE
```

**Layers:**
- **Providers** (`core/providers/real` `RealOsProvider` `/proc/os-release`+`uname`, `RealCpuProvider` `/proc/cpuinfo`+`sysfs`, `RealMemoryProvider` `proc/meminfo`+`pressure`+`zram`, `RealStorageProvider` `/proc/mounts`+`statvfs`+`/sys/block`, `RealGpuProvider` `/sys/bus/pci`+`pci.ids`+`glxinfo` `/usr/bin/glxinfo` `DISPLAY=:0`, `RealThermalProvider` `hwmon`, `RealSystemdProvider` `systemctl`/`systemd-analyze` `execv` `poll`, `RealKdeProvider` `kwinrc`, `RealProcessProvider` `/proc`, `RealJournalProvider` `journalctl`) - read-only, no policy
- **Engines** (`BaselineEngine` `3812ms` `15 metrics` `MetricMeta`, `BottleneckEngine` `10` `critical-chain` `BLOCKER` vs `background`, `BenchmarkEngine` `quick` `prime compute 2..2000` `min/max/avg/median/stddev`, `ComparisonEngine` `compare` `before`/`after` `expectedBenefit` `observedBenefit` `hasRegression` `threshold` `boot +10%` `available -1GB` `thermal +15C` `new_failed`, `RecommendationEngine` `7` `evidence`/`confidence`/`benefit`/`risk`)
- **Profile** (`UserProfile` `UNKNOWN/YES/NO` 8 fields, `ProfileStore` `~/.local/state/polaris/profile.json` `0600`, `ProfileService` explicit `updateField` no inference, `ProfileAdvisor` `BLOCKED`/`REQUIRES`/`ALLOWED` `whatWillNotChange`)
- **Transaction Engine** (`StateMachine` 16 states, `Transaction` `beforeHash`/`approvedBeforeHash` 15 fields `comparison` `beforeBaseline`/`afterBaseline`, `TransactionValidator` pure `stale`/`TOCTOU`, `TransactionStore` `create` `ALREADY_EXISTS` `approve` `already_approved` `apply` `APPROVAL→VALIDATION→BACKUP→FINAL→APPLY` `already_completed`, `FileSafety`, `BackupEngine` `SHA-256`, `TransactionLock` `flock`, `RecoveryDetector` `BACKUP_CREATED`→`incomplete` `suggested FAILED`)
- **Explainability** (`Explanation` 22 fields deterministic `toJson` sorted `toHuman` redacted, `ExplanationEngine` `explainCandidate`/`explainTransaction` `WHY NOW`/`WHAT WILL CHANGE`/`WHAT WILL NOT CHANGE`/`rejectionConditions` with `ProfileAdvisor` + `Comparison`)
- **Security/IPC** (`IpcProtocol` `PROTOCOL_VERSION=1` `MAX_REQUEST_SIZE=64KB` `allowedOperations` `ping`/`info` only, `IpcAuth` `SO_PEERCRED` same-user `ucred` `pid/uid/gid` from kernel not client, `IpcServer` `Unix socket` `0600` `0700` `FD_CLOEXEC` `poll` 5s, `IpcClient` `poll` `timeout`)
- **Verification** (`ComparisonEngine` + `Regression` `hasRegression` `verdict` `SUCCESS`/`REGRESSION`/`NO_CHANGE`/`NO_BENEFIT`/`INCONCLUSIVE` `isDeterministic`)
- **Security Layer** (`ReadOnlyGuard`, `Polkit` `auth_admin_keep` `org.polaris.*.policy` 3 actions, `AuditLog` `hashEvent` `SHA256` `previousHash` chain `fsync` per `append`)
- **API** (`api/` `GET /api/v1/transactions/{id}` with `comparison` `beforeBaseline`/`afterBaseline`, `GET /api/v1/transactions/{id}/audit`, `POST /api/v1/transactions/preview` `ChangePreview`, `POST /api/v1/transactions/{id}/approve` `org.polaris.*`, `GET /api/v1/performance/baseline`)

See `docs/ARCHITECTURE.md` (P16 `Explainability` layer, `P15` `Test/CI`, `P14` `IPC/Security`), `docs/TRANSACTION_MODEL.md` (`beforeHash`/`approvedBeforeHash` + `P12` two gates `APPROVAL→VALIDATION→BACKUP→FINAL→APPLY`), `docs/CLI_USAGE.md` (this CLI guide).

---

## Current CLI Usage

**Build** (`C++20` `CMake 3.28`):
```bash
cmake -S . -B build --fresh -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j$(nproc)
ctest --test-dir build --output-on-failure # 33/33 0.70s 100%
```

**Primary `polaris_p4` (safe infrastructure `P4`/`P11`/`P12`/`P13`/`P16`):**
```bash
# Read-only discovery (no mutation, no sudo, stat /etc/fstab mtime unchanged)
./build/polaris_real --json | python3 -m json.tool | head -n 50 # P2 9.9K Vendor Intel NVIDIA 10de:174d
./build/polaris_p3 # P3 Analyze - READ-ONLY: Baseline 2026-09-01... CPU ... Memory avail 5759MB Boot 8.515s failed 1
./build/polaris_p4 profile show --json # {"usesAkonadi":"unknown",...,"usesKMail":"unknown",...} (not exists, default unknown, not created)
./build/polaris_p4 explain akonadi-disable --json # {"candidateId":"akonadi-disable","decision":"REQUIRE_CONFIRMATION","whyNow":"Measured Akonadi 1302M...","whatWillChange":"target=akonadi...","whatWillNotChange":"Akonadi will remain...","rejectionConditions":["profile: usesKMail=unknown"]}
./build/polaris_p4 explain bluetooth-disable --verbose # WHY NOW: Measured bluetooth enabled active 2 paired... WHAT WILL NOT CHANGE: NVIDIA remains...
./build/polaris_p4 transaction preview dummy-test # PREVIEWED TX-TEST-... target /tmp/polaris-test-root/etc/fstab diff "- UUID ... swap" privilege org.polaris.modify.fstab rollback Restore from backup .../fstab.bak rebootRequired false
./build/polaris_p4 transaction list # [{"id":"TX-TEST-...","state":"PREVIEWED"}]
./build/polaris_p4 transaction show TX-TEST-123 --json # {"id":"TX-TEST-...","state":"PREVIEWED","beforeHash":"...","approvedBeforeHash":"",...}
./build/polaris_p4 transaction approve TX-TEST-123 # {"transactionId":"TX-TEST-...","approval":"APPROVED"} # Explicit approval recorded, not equivalent to launch
./build/polaris_p4 transaction explain TX-TEST-123 --json --verbose # P16 WHY NOW Transaction TX-TEST-... WHAT WILL CHANGE target=... diff ... WHAT WILL NOT CHANGE Akonadi remains... REJECTION CONDITIONS stale beforeHash...
./build/polaris_p4 apply --dry-run dummy-test # Dry-run MUST NOT write files, invoke privileged ops, or request password - verified
./build/polaris_p4 audit list # [{"transactionId":"TX-TEST-...","hash":"..."}] previousHash chain
./build/polaris_p4 profile set usesKMail yes --json # {"field":"usesKMail","previousValue":"unknown","newValue":"yes","status":"updated"} # FileSafety atomicWrite tmp+fsync+chmod 0600+rename 0600, audit profile.updated
```

**`polaris` mock scaffold (FakeProviders):**
```bash
./build/polaris scan --json | jq | head -n 50 # {"system":{"os":{"distro":"fedora"...},"kernel":"7.1.10-200"...},"hardware":{"cpu":"i5-10210U","memory":"12GB","gpu":"MX130 UNCLAIMED"},"health":{"score":0,"issues":[{"id":"GPU-001","severity":"HIGH"}]}}
./build/polaris health --json
```

**`polaris_p5` pilot (P5 `Hidden=true` 9 preconditions):**
```bash
./build/polaris_p5 # P5 Pilot Precondition Check - /home/.../nvidia-settings-user.desktop [OK] file exists [OK] file is regular file [OK] file is not symlink [OK] canonical [OK] file owned by current user [OK] file size 101 [OK] file contains ... [OK] current file hash recorded: 4ad53409...
```

**What commands are safe/read-only (no `FileSafety::validatePath` mutation, no `BackupEngine`, `stat /etc/fstab` unchanged):**
`polaris scan`, `polaris health`, `polaris_real --json`, `polaris_p3` all, `polaris_p4 transaction list`/`show`/`compare`/`preview`, `polaris_p4 audit list`, `polaris_p4 apply --dry-run`, `polaris_p4 profile show`, `polaris_p4 explain` / `transaction explain`

**What commands can eventually mutate host (only via `P12` hardened `TransactionStore::apply` `APPROVAL→VALIDATION→BACKUP→FINAL→APPLY`):**
`polaris_p4 transaction approve <id>` (records `APPROVED` `approvedBeforeHash` binding, not `APPLY`), future `transaction apply` would be helper `org.polaris.*` `IpcProtocol` `SO_PEERCRED` `flock` (but `P14` allowlist `ping`/`info` only, **no privileged mutation** `NO PRIVILEGED MUTATION OPERATION IS ENABLED BY P14/P16`, `P18` still `33/33` no `transaction apply` via CLI), `polaris_p5 apply` for `Hidden=true` (`FileSafety::atomicWrite` `9` preconditions), `polaris_p4 profile set` (writes `~/.local/state/polaris/profile.json` `0600`, not `systemd`/`dnf`/`fstab`)

**Safety workflow:**
```
READ (polaris_real) → MEASURE (BaselineEngine 3812ms 15 metrics) → ANALYZE (BottleneckEngine critical-chain) → EXPLAIN (ExplanationEngine whyNow/whatWillNotChange) → RECOMMEND (RecommendationEngine 7 recs) → PREVIEW (transaction preview) → EXPLICIT APPROVAL (transaction approve TX-TEST-... with approvedBeforeHash binding, AuditLog transaction.approved) → BACKUP (BackupEngine SHA-256 is_regular_file fsync no overwrite) → APPLY (FileSafety atomicWrite tmp+fsync+rename isSymlink) → VERIFY (is_regular_file sha256, systemctl is-enabled re-read) → COMPARE (ComparisonEngine before/after expectedBenefit observedBenefit hasRegression threshold boot +10% available -1GB thermal +15C new_failed) → REGRESSION (hasRegression true → verdict not SUCCESS) → AUDIT (AuditLog hashEvent SHA256 previousHash fsync)
```

---

## Installation

**From source (Fedora 44, P18 `ci.yml`):**
```bash
git clone https://github.com/MehranQadirian/polaris.git
cd polaris
cmake -S . -B build --fresh
cmake --build build -- -j$(nproc)
ctest --test-dir build --output-on-failure # 33/33 100%
sudo cmake --install build # installs polaris to bin, docs to share/doc/polaris
# Or RPM: rpmbuild -ba packaging/polaris.spec (see packaging/polaris.spec)
```

**Dependencies:** `cmake >=3.28`, `g++ >=14`, `openssl-devel` (`libssl-dev` `crypto` for `SHA256`), `ninja` optional, `sdbus-c++` read-only `D-Bus`, `libdrm`, `lm_sensors` headers (as `P2` `Real*Provider` `execv` fallback, not `sd-bus` native yet).

---

## Example Workflow (Evidence → Explain → Preview → Approve → Verify)

```bash
# 1. Read-only baseline (no mutation)
./build/polaris_real --json > p2_scan.json
./build/polaris_p3 # P3 Analyze 3144ms - INFORMATION ONLY

# 2. Explain candidate (read-only, ProfileAdvisor + Comparison)
./build/polaris_p4 explain akonadi-disable --json | python3 -m json.tool
# {"candidateId":"akonadi-disable","decision":"REQUIRE_CONFIRMATION","whyNow":"Measured Akonadi 1302M...","whatWillNotChange":"Akonadi will remain...","rejectionConditions":["profile: usesKMail=unknown"]}
# With explicit profile:
./build/polaris_p4 profile set usesKMail yes --json
./build/polaris_p4 explain akonadi-disable --verbose # WHY NOW: ... usesKMail=yes (explicit) Confidence 0.65 ... WHAT WILL NOT CHANGE: Akonadi will remain enabled...

# 3. Preview (test fixtures, no auth, no writes to real host)
./build/polaris_p4 transaction preview dummy-test # PREVIEWED TX-TEST-12345 diff rollback rebootRequired false

# 4. Explicit approval (exact transactionId, beforeHash binding, not launch==approval)
./build/polaris_p4 transaction approve TX-TEST-12345 # {"approval":"APPROVED"} # Explicit approval recorded

# 5. Show and explain transaction (read-only)
./build/polaris_p4 transaction show TX-TEST-12345 --json | python3 -m json.tool | head -n 30
./build/polaris_p4 transaction explain TX-TEST-12345 --json | python3 -m json.tool | head -n 30 # WHY NOW Transaction TX-TEST... WHAT WILL CHANGE target=... REJECTION CONDITIONS stale beforeHash...

# 6. Dry-run (must not write, not invoke privileged ops)
./build/polaris_p4 apply --dry-run dummy-test # Dry-run MUST NOT write files

# 7. Audit (hash chain, fsync, not password)
./build/polaris_p4 audit list | head -n 20 # [{"transactionId":"TX-TEST-...","hash":"..."}]
cat ~/.local/state/polaris/audit.log | head -n 5 # {"timestamp":"...","transactionId":"TX-P7-...","operation":"transaction.previewed","previousHash":"",...}
cat /tmp/polaris-test-root/audit.log | head -n 5 # TX-TEST fixtures

# 8. After hypothetical APPLY with reboot (P7 example, not in P18):
# ./build/polaris_p4 transaction compare TX-P7-NVIDIA-470xx-20260831 --json # Comparison boot 54.106→8.515 -84% not regression, available 4.2→6.5GB not regression, thermal 67→50 not regression, nvidia 0→1 not regression, verdict SUCCESS hasRegression false
```

---

## Current Capabilities (P18 `PROJECT_COMPLETE_WITH_LIMITATIONS`)

- `P2` real read-only `proc`/`sys` 9.9K `Vendor Intel` `NVIDIA 10de:174d`
- `P3` `BaselineEngine` `3812ms` `15 metrics` `MetricMeta` `BottleneckEngine` `10` `BenchmarkEngine` `quick` `prime` `0.043ms`
- `P4` `StateMachine` 16 states `FileSafety` `BackupEngine` `AuditLog` `polkit` `auth_admin_keep` `polaris_p4` `transaction preview` on `/tmp/polaris-test-root`
- `P5` `TX-P5-20260831-001` `ALREADY_APPLIED / NO_OP` `Hidden=true` `101` `sha 4ad53409` (pilot, 9 preconditions)
- `P6` `TX-P6-20260831-MSSQL-DISABLE-PREVIEW-V2` `systemctl disable` `723f` `Removed` `disabled` verified after reboot `0` `failed` `713M` `0` (1 real)
- `P7` `TX-P7-NVIDIA-470xx-20260831` `dnf swap` `akmod-nvidia-470xx` `akmods --force` `modinfo 470.256.02` `extra/nvidia-470xx` `dracut --force` `reboot 00:36` `lspci` `CLAIMED` `driver=nvidia` `nvidia-smi` `470.256.02` `PRIME` `NVIDIA GeForce MX130` `journal NVRM` `490→1` (1 real + reboot)
- `P8`/`P9` `p8_analysis.json` `14K` `p9_analysis.json` `11K` `NO_ACTION` `akonadi` `REJECTED` `0s`/`5-10M` tiny
- `P11` `Comparison` `SUCCESS`/`REGRESSION` `isDeterministic` `ComparisonEngine` thresholds `boot +10%` `available -1GB` `thermal +15C` `new_failed`, `Transaction` `beforeBaseline`/`afterBaseline`/`comparison` `P7` fixture `54.106s` `4.2GB` `67C` `1` `0` → `8.515s` `6.5GB` `50C` `0` `1` `SUCCESS`
- `P12` `Transaction` `beforeHash`/`approvedBeforeHash` + `TransactionValidator` pure stale `TOCTOU` + `TransactionStore` duplicate `ALREADY_EXISTS` idempotent `COMPLETED→already_completed` + `StateMachine` `PREVIEWED→FAILED` + `AuditLog` `fsync` `13/13`
- `P13` `UserProfile` `UNKNOWN/YES/NO` 8 fields `ProfileStore` `~/.local/state/polaris/profile.json` `0600` `ProfileService` explicit `updateField` no inference `ProfileAdvisor` `BLOCKED`/`REQUIRES`/`ALLOWED` `17/17`
- `P14` `IpcProtocol` `PROTOCOL_VERSION=1` `MAX_REQUEST_SIZE=64KB` `allowedOperations` `ping`/`info` only `NO PRIVILEGED MUTATION`, `IpcAuth` `SO_PEERCRED` same-user `ucred` `pid/uid/gid` from kernel not client, `IpcServer` `0600` `0700` `FD_CLOEXEC` `poll` 5s, `TransactionLock` `flock` `LOCK_EX|LOCK_NB` `0600`, `RecoveryDetector` `BACKUP_CREATED`→`incomplete` `suggested FAILED` never `COMPLETED` `24/24`
- `P15` `TransactionValidator` fix `UNAVAILABLE`→fail-closed, 5 table-driven `P15` suites `test_p15_lifecycle` `test_p15_stale_matrix` `test_p15_toctou_idempotency` `test_p15_lock_recovery` `test_p15_regression_audit` + `.github/workflows/ci.yml` `cmake --fresh` `ctest` `29/29`
- `P16` `Explanation` 22 fields deterministic sorted JSON `toHuman` redacted `WHY NOW` evidence-backed `WHAT WILL CHANGE` transaction-backed `WHAT WILL NOT CHANGE` explicit `rejectionConditions` deterministic `profile`/`comparison` integration `never confuses approval with authorization`, `ExplanationEngine` `explainCandidate`/`explainTransaction`, CLI `polaris_p4 explain <candidate> [--json] [--verbose]` and `transaction explain <id>` `33/33`
- `P17` `Campaign 2` read-only `systemd-analyze` `8.515s` `akonadi` `BLOCKED`/`REQUIRES` `bluetooth` `2 paired` `REQUIRES` `5-10M` tiny → `NO_ACTION_RECOMMENDED` exactly ONE `bluetooth-disable` considered but negligible, no `PREVIEWED` transaction `audit` `explanation.generated`
- `P18` `FINAL_REPORT.md` `52K` `FINAL_STATE.json` `27K` `systemd-analyze` `8.515s` `free` `5.6Gi` `zram` `0B` `lspci` `CLAIMED` `driver=nvidia` `modinfo` `470.256.02` `nvidia-smi` `470.256.02` `glxinfo` `Mesa Intel` `PRIME` `NVIDIA GeForce MX130` `akonadictl` `running` 14 agents `mssql` `disabled` `fstab` 3 entries `stat` `2026-08-31 21:19` `Comparison` `P3` `54.106s`→`8.515s` `-84%` not `+10%` `available` `4.2→5.6` `+1.4GB` not `<-1GB` `32/33`? Actually `33/33` `0.70s` `100%` `P5` `NO_OP` `P6` `mssql` `713M` `0` `P7` `nvidia` `470.256.02` `PRIME` `P17` `NO_ACTION` `akonadi` `REJECTED` `bluetooth` `REQUIRES` `Safety` `READ→...→AUDIT` `stale`/`idempotency`/`flock`/`recovery` `33/33` `PROJECT_COMPLETE_WITH_LIMITATIONS` `no P19` justified

---

## Current Limitations

`Unavailable metrics` `available false` not guessed, `rebootMarker` not auto-captured, `login` `596ms` not in `PerformanceBaseline`, `RealSystemdProvider` `execv` fallback not `sd-bus`, `RealGpuProvider` `glxinfo DISPLAY=:0` hack, `Benchmark` synthetic `cpu_prime`, no helper installed (`/run/polaris/helper.sock` not exists correct, `P14` defined but not installed), not `git` repo `0.1.0`, no telemetry, `P12` generic preconditions mocked, `P14` `flock` advisory not mandatory for `TransactionStore::apply` real path, `P14` recovery detection-only, `P15` CI minimal, `P16` read-only explainability, `P18` `loadAvg`/`PSI`/`journal` `NOT MEASURED` for `P18` final validation, `failedCount` `1→1` different unit `mssql` `1→0` vs `drkonqi` `0→1` → `INCONCLUSIVE` for `failed` but `mssql` itself `0` is **not regression**, `zram` `0B` stable.

See `docs/P18_FINAL_REPORT.md` `15K` + `docs/P18_FINAL_STATE.json` for `baselineComparison` `completedTransactions` `rejectedCandidates` `regressions` `safetyAssessment` `tests` `knownLimitations` `recommendation: STOP`.

---

## Test Status

`cmake -S . -B build --fresh && cmake --build build && ctest --test-dir build --output-on-failure`

**P18 baseline:** `33/33` `0.70s` `100%` (`unit` `real_providers` `parsers` `readonly` `p4_security` 9 `comparison` 12 `post_change` `regression` `observed_benefit` `p12_stale` 10 `p12_idempotency` 5 `p12_statemachine` 20 `p12_transaction_model` 3 `p13_profile_model` 6 `p13_profile_store` 6 `p13_profile_service` 6 `p13_profile_advisor` 12 `p14_ipc_protocol` 12 `p14_ipc_auth` 5 `p14_socket_security` 5 `p14_ipc_server` 4 `p14_lock` 5 `p14_ipc_security` 12 `p14_recovery` 4 `p15_lifecycle` 28 `p15_stale_matrix` 19 `p15_toctou_idempotency` 3 `p15_lock_recovery` 8 `p15_regression_audit` `p16_explanation_model` 6 `p16_explain_candidate` 7 `p16_explain_transaction` 8 `p16_verbose_redaction` 8).

No test weakening permitted.

---

## Project Status

`P1` Architecture `COMPLETE` scaffold → `P18` `Final Benchmark` `COMPLETED` `PROJECT_COMPLETE_WITH_LIMITATIONS` `P18` `FINAL_REPORT.md` `15K` `FINAL_STATE.json` `27K` `verdict` `PROJECT_COMPLETE_WITH_LIMITATIONS` `currentHostState` `baselineComparison` `completedTransactions` `rejectedCandidates` `regressions` `safetyAssessment` `tests` `33/33` `knownLimitations` `recommendation: STOP`. Roadmap `P1-P18` completed, no `P19` justified (only `P19` would be `Further Engineering Required` if new evidence-backed gap, none exists). See `docs/ROADMAP.md` `P18` `FINAL REPORT` `P18` `COMPLETE`.

---

## Version

`0.1.0` (`project(polaris VERSION 0.1.0 LANGUAGES CXX)` `CMakeLists.txt:2` `CMAKE_CXX_STANDARD 20` `CMAKE_CXX_STANDARD_REQUIRED ON`, `docs/VERSIONING.md` `MAJOR.MINOR.PATCH` `Semantic Versioning` where compatible, `CHANGELOG.md` `P1-P18` high-level, centralized rather than duplicated).

---

## License

**MIT**: permissive, suitable for `OpenSSL` `crypto` `libssl-dev` dependency (`OpenSSL` `Apache-2.0` compatible), no `GPL`/`LGPL` conflict (`sdbus-c++` read-only `D-Bus` not linked as `GPL`, `libdrm` `MIT`, `lm_sensors` `LGPL` headers). `LICENSE` canonical `MIT` text, `README` `SPDX-License-Identifier: MIT`, `packaging/polaris.spec` `License: MIT`.

---

## Contributing

See `CONTRIBUTING.md` (if present, else `docs/CONTRIBUTING.md` or `README` `Contributing` section). `P15` `CI` `.github/workflows/ci.yml` `cmake --fresh` `ctest` `100%` required. Security issues → `SECURITY.md` `SECURITY_AUDIT.md` (do not open public issue with `PoC`).

---

## Security Reporting

See `SECURITY.md` `SECURITY_AUDIT.md` (`P19` `SECRET_AUDIT: PASS` after `docs/P7_PRE_REBOOT_REPORT.md` `echo "****" | sudo -S` literal `[REDACTED]` redacted `sudo` `password redacted [REDACTED]` before first commit, `grep -R "[REDACTED]" ~/Documents/lin-opt` `0` hits after for historical literal). Do not open public issue with `PoC` if `helper` `SO_PEERCRED` `TransactionLock` `RecoveryDetector` has `POC`.

---

*Built on actual Fedora KDE diagnostics 2026-08-31 to 2026-09-01, evidence for bottleneck & NVIDIA compat engines. CLI is primary interface; Qt GUI is future direction, `gui/` empty, `P10` `Qt6 Widgets/Quick as API client only` not implemented.*

GUI coming soon
