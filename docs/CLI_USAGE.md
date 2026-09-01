# Polaris CLI Usage Guide

**Status:** Engineering `P18` `PROJECT_COMPLETE_WITH_LIMITATIONS` `33/33` tests `0.70s` `100%` `C++20` `CMake 3.28`  
**Version:** `0.1.0` (`project(polaris VERSION 0.1.0 LANGUAGES CXX)` `CMakeLists.txt:2`)  
**Build:** `cmake -S . -B build --fresh && cmake --build build && ctest`  
**Primary Interface:** CLI is the primary user interface; Qt GUI is `future direction` (`P10` `gui/` empty) not implemented

Polaris is **not** a blind `debloat` script. It is a safety-oriented Linux performance platform that follows:

```
READ → MEASURE → ANALYZE → EXPLAIN → RECOMMEND → PREVIEW → EXPLICIT APPROVAL → BACKUP → APPLY → VERIFY → COMPARE → REGRESSION → AUDIT
```

---

## 1. How to Build From Source

**Dependencies (Fedora 44, P18 `ci.yml`):**
```bash
sudo dnf install -y cmake ninja-build gcc-c++ openssl-devel # libssl-dev on Ubuntu
# Qt not required for core (core is no Qt, gui future)
```

**Build:**
```bash
cmake -S . -B build --fresh -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j$(nproc)
ctest --test-dir build --output-on-failure
# Expected: 33/33 100% 0.70s
```

**Binaries produced (P18 `build/`):**
- `polaris` - mock scaffold (`-l` `FakeProviders`, `scan --json`, `health --json`, `baseline create` stub)
- `polaris_real` - `P2` real read-only scan (`--json` 9.9K, `noSudo` `readOnlyGuard` true, `stat /etc/fstab` mtime unchanged)
- `polaris_p3` - `P3` analyze (`performance` `baseline`/`benchmark`, `analyze`, `bottlenecks`, `recommendations` read-only)
- `polaris_p4` - `P4`/`P11`/`P12`/`P13`/`P16` **safe infrastructure** `transaction` `audit` `apply --dry-run` `profile` `explain` (primary for `P16` explainability)
- `polaris_p5` - `P5` pilot `preview|approve|apply` for `~/.config/autostart/nvidia-settings-user.desktop` `Hidden=true` (9 preconditions)

**No `sudo` inside Polaris, no `SUDO_ASKPASS`, no password file.**

---

## 2. How to Run the CLI

**Help:**
```bash
./build/polaris --help
# Usage: polaris scan --json | health --json | baseline create

./build/polaris_p4 --help
# Polaris P4/P11/P12/P13/P16 - SAFE INFRASTRUCTURE READY
# Usage:
#   polaris_p4 transaction list
#   polaris_p4 transaction show <id> [--json]
#   polaris_p4 transaction compare <id> [--json]
#   polaris_p4 transaction preview <operation>
#   polaris_p4 transaction approve <id>
#   polaris_p4 transaction rollback <id>  # test fixtures only
#   polaris_p4 transaction explain <id> [--json] [--verbose]
#   polaris_p4 audit list
#   polaris_p4 apply --dry-run <operation>
#   polaris_p4 profile show [--json]
#   polaris_p4 profile set <field> <yes|no|unknown> [--json]
#   polaris_p4 explain <candidate> [--json] [--verbose]
#     candidate examples: akonadi-disable, bluetooth-disable, fstab-stale-swap
```

**Discovery vs Mutation:**
- **Safe/read-only** (no `FileSafety::validatePath` mutation, no `BackupEngine`, no `AuditLog` `apply`): `polaris scan`, `polaris_real`, `polaris_p3` `performance`/`analyze`/`bottlenecks`/`recommendations`, `polaris_p4 transaction list`/`show`/`compare`/`preview`, `polaris_p4 audit list`, `polaris_p4 profile show`, `polaris_p4 explain`, `polaris_p4 apply --dry-run`
- **Can mutate host** (only via `P12` hardened `TransactionStore::apply` `APPROVAL→VALIDATION→BACKUP→FINAL→APPLY`): `polaris_p4 transaction approve <id>` (records `APPROVED` + `approvedBeforeHash` binding, not `APPLY`), future `transaction apply` would be helper `org.polaris.*` via `IpcProtocol` `SO_PEERCRED` `flock` (but `P14` allowlist `ping`/`info` only, **no privileged mutation** `NO PRIVILEGED MUTATION OPERATION IS ENABLED BY P14/P16`, `P18` still `24/24` `p14_*` tests use `/tmp/polaris-test-root/p14` fixtures only)

---

## 3. Every Currently Supported CLI Command

**`polaris` (mock scaffold, `P1`):**
- `polaris` / `polaris scan --json` → `{"system":{"os":{"distro":"fedora"...},"kernel":"7.1.10-200"...},"hardware":{"cpu":"i5-10210U","memory":"12GB","gpu":"MX130 UNCLAIMED"},"health":{"score":0,"issues":[{"id":"GPU-001","severity":"HIGH"}]}}` `# Polaris MVP read-only scaffold - uses FakeProviders`
- `polaris health --json` → `Health: see scan --json`
- `polaris baseline create` → stub

**`polaris_real` (`P2` real read-only, `noSudo`):**
- `polaris_real --json` → `{"meta":{"mode":"P2_READ_ONLY","elapsedMs":3813.04,"readOnlyGuard":true,"noSudo":true},"system":{...},"cpu":{...},"memory":{...},"filesystems":[...],"blockDevices":[...],"thermals":[...],"gpus":[...],"nvidia":{...},"glRenderer":"...","boot":{...},"failedServices":[...]}` `9.9K` `p2_scan.json`

**`polaris_p3` (`P3` read-only analyze, `3144ms` `BaselineEngine`):**
- `polaris_p3` (no args) → `Polaris P3 Analyze - READ-ONLY, no modifications` `Baseline: 2026-09-01...` `CPU: ...` `Memory: avail 5759MB` `Boot: firmware 3.167 ... userspace 8.515 failed 1` `Bottlenecks: 6 (see bottlenecks --human)` `Recommendations: 4 (see recommendations --human) - INFORMATION ONLY` `Benchmark quick: 3 results`
- `polaris_p3 performance baseline` / `benchmark` / `analyze` / `bottlenecks` / `recommendations` (`--json`/`--human` via `p3_cli.cpp` `recommend` engine, read-only, not `P17` `NO_ACTION`)

**`polaris_p4` (primary, `P4`/`P11`/`P12`/`P13`/`P16`):**
- `polaris_p4 transaction list` → `[\n  {"id":"TX-TEST-...","state":"PREVIEWED","target":"/tmp/polaris-test-root/etc/fstab","risk":"R2"}\n]` (test fixtures `txStore` `/tmp/polaris-test-root/transactions`)
- `polaris_p4 transaction show <id> [--json]` → `{"id":"TX-TEST-...","state":"...","target":"...","risk":"...","beforeHash":"...","approvedBeforeHash":"...","beforeBaseline":{...},"afterBaseline":{...},"comparison":{...}}` ( `P11` `beforeBaseline`/`afterBaseline`/`comparison` if present, `P12` `beforeHash`/`approvedBeforeHash` etc.)
- `polaris_p4 transaction compare <id> [--json]` → `P11` `ComparisonEngine` `{"transactionId":"...","comparison":"present ..."/"not yet available - reboot-pending"}` (structured `Comparison` `metrics` `delta` `hasRegression` `verdict` `beforeTimestamp`/`afterTimestamp`)
- `polaris_p4 transaction preview <operation>` → `{"transactionId":"TX-TEST-...","state":"PREVIEWED","target":"/tmp/polaris-test-root/etc/fstab","risk":"R2","before":"...","after":"...","diff":"- UUID ... swap\n+ # disabled...","privilege":"org.polaris.modify.fstab","rollback":"Restore from backup /tmp/polaris-test-root/backups/TX-TEST-.../fstab.bak","rebootRequired":false}` `# Preview - no writes, no auth, test fixture only`
- `polaris_p4 transaction approve <id>` → `{"transactionId":"...","approval":"APPROVED","state":"APPROVED"}` `# Explicit approval recorded - not equivalent to launch, preview required first` (`AuditLog` `transaction.approved` `previousHash` chain, `TransactionValidator::bindApproval` `approvedBeforeHash`/`approvedTarget` for `P12` stale protection)
- `polaris_p4 transaction rollback <id>` → `{"error":"P4 rollback on test fixtures only - use TX-TEST id"}` (test fixtures, `BackupEngine::restore`)
- `polaris_p4 transaction explain <id> [--json] [--verbose]` → `P16` `Explanation` `WHY NOW`/`WHAT WILL CHANGE`/`WHAT WILL NOT CHANGE`/`EVIDENCE`/`EXPECTED BENEFIT`/`CONFIDENCE`/`RISK`/`REVERSIBILITY`/`REBOOT`/`AUTHORIZATION`/`REJECTION CONDITIONS`/`ROLLBACK`/`OBSERVED RESULT`/`VERDICT` (`ExplanationEngine::explainTransaction` `TxState` `PREVIEWED`→`FAILED` `expected`/`observed`/`backup`/`rollback`, `Comparison` `expected` vs `observed`, never `password`)
- `polaris_p4 audit list` → `[{"transactionId":"TX-TEST-...","hash":"..."}]` (`AuditLog` `hashEvent` `SHA256` `previousHash` chain, `fsync` per `append`)
- `polaris_p4 apply --dry-run <operation>` → `# Dry-run for operation: dummy (test fixture)` + `preview` output `# Dry-run MUST NOT write files, invoke privileged ops, or request password - verified`
- `polaris_p4 profile show [--json]` → `{"usesAkonadi":"unknown","usesAvahi":"unknown","usesBluetooth":"unknown","usesCups":"unknown","usesKMail":"unknown","usesKOrganizer":"unknown","usesKontact":"unknown","usesPrinting":"unknown"}` `# Profile: ~/.local/state/polaris/profile.json (not exists, default unknown, not created)` ` # Akonadi: REQUIRES_USER_CONFIRMATION - ...` (`ProfileStore::load` missing→`unknown` no auto-create, `ProfileAdvisor::canConsiderAkonadi`)
- `polaris_p4 profile set <field> <yes|no|unknown> [--json]` → `{"field":"usesKMail","previousValue":"unknown","newValue":"yes","status":"updated"|"idempotent"}` `# Updated usesKMail from unknown to yes` (`ProfileService::updateField` explicit no inference `usesKMail→usesAkonadi` not inferred, `ProfileStore::save` atomic `tmp+fsync+chmod 0600+rename` `0600`, `validateProfilePath` `..;|&` etc., `audit` `profile.updated` `field`/`previous`/`new`, `FileSafety` allowlist `~/.local/state/polaris/profile.json`)
  - Fields: `usesKMail`, `usesKontact`, `usesKOrganizer`, `usesBluetooth`, `usesPrinting`, `usesAvahi`, `usesCups`, `usesAkonadi` (`TriState` `unknown`/`yes`/`no`, `UserProfile` 8 fields + `extra`, `knownFields()` sorted)
- `polaris_p4 explain <candidate> [--json] [--verbose]` → `P16` `Explanation` `WHY NOW` evidence-backed (`akonadi 1302M` + `ProfileAdvisor` + `Baseline` `userspace 8.515s`), `WHAT WILL CHANGE` `target`/`operation`/`diff`, `WHAT WILL NOT CHANGE` explicit invariants (`NVIDIA 470xx remains claimed...`), `rejectionConditions` deterministic `stale`/`profile`/`kernel`/`regression`/`already completed` sorted, `expectedBenefit` vs `observedBenefit` (`Comparison` `verdict` `SUCCESS`/`REGRESSION`), `confidence`/`risk`/`reversibility`/`reboot`/`authorization`, `rollbackSummary`, `beforeStateSummary`/`afterStateSummary`, `verdict`, `hasRegression`, `limitations`, `human` `WHY NOW:`/`WHAT WILL CHANGE:`/`WHAT WILL NOT CHANGE:`/`EVIDENCE:`/`EXPECTED BENEFIT:`/`CONFIDENCE:`/`RISK:`/`REVERSIBILITY:`/`REBOOT:`/`AUTHORIZATION:`/`REJECTION CONDITIONS:`/`ROLLBACK:`/`OBSERVED RESULT:`/`VERDICT:` with `verbose` adds `EVIDENCE` list sorted, redaction `[REDACTED]` for `password`/`secret`, `audit` `explanation.generated` not `approved`
  - Candidates: `akonadi-disable`, `bluetooth-disable`, `fstab-stale-swap`, `avahi-disable`, `cups-disable`, `plocate-updatedb`, `dnf-makecache`, `autostart`, `journal` (as in `p8_analysis.json` `p9_analysis.json` `RECOMMEND` vs `BLOCKED` vs `REQUIRES`)

**`polaris_p5` (`P5` pilot, `~/.config/autostart/nvidia-settings-user.desktop` `Hidden=true`):**
- `polaris_p5` → `# P5 Pilot Precondition Check - /home/.../nvidia-settings-user.desktop` `[OK] file exists` `[OK] file is regular file` `[OK] file is not symlink` `[OK] canonical` `[OK] file owned by current user` `[OK] file size within safe limits (101 bytes)` `[OK] file contains expected NVIDIA...` `[OK] file contains Hidden` `[OK] current file hash recorded: 4ad5340929...` `All preconditions PASS - proceed to transaction` `Usage: polaris_p5 <preview|approve <id>|apply>`

**Not available / not invented:**
- `polaris transaction apply <id>` (real privileged `apply` would be helper `org.polaris.*` `IpcProtocol` `SO_PEERCRED` `flock` but `P14` allowlist `ping`/`info` only, **no privileged mutation** `NO PRIVILEGED MUTATION OPERATION IS ENABLED BY P14/P16`, `P18` still `33/33` no `transaction apply` via CLI)
- `polaris gui` (Qt GUI `gui/` empty, future `P10` `Qt6 Widgets/Quick as API client only` not implemented)
- `polaris profile set` with password/secret field (rejected `password field rejected` `validate` `FileSafety`)

---

## 4. Read-Only Discovery Commands

All `P2` `Real*Provider` are `readOnlyGuard` `kReadOnlyMode true` `openReadOnly` `ifstream`, no `FileSafety::validatePath` mutation, `test_readonly` verifies `stat /etc/fstab` mtime unchanged.

- `polaris_real --json` `P2` `3813ms` `RealOsProvider` `/proc/os-release`+`uname`, `RealCpuProvider` `/proc/cpuinfo`+`sysfs cpufreq`, `RealMemoryProvider` `/proc/meminfo`+`pressure`+`zram`, `RealStorageProvider` `/proc/mounts`+`statvfs`+`/sys/block`, `RealGpuProvider` `/sys/bus/pci`+`pci.ids`+`glxinfo` `/usr/bin/glxinfo` `DISPLAY=:0`, `RealThermalProvider` `hwmon`, `RealSystemdProvider` `systemctl` `systemd-analyze` via `execv` `poll` (timeout 5s), `RealKdeProvider` env+`kwinrc`, `RealProcessProvider` `/proc`, `RealJournalProvider` `journalctl`
- `polaris_p3` `performance` `baseline` `collect()` + `bottleneck` `10 bottlenecks` `critical-chain` `BLOCKER` vs `background` + `benchmark` `quick`/`normal`/`deep` `prime compute 2..2000` `min/max/avg/median/stddev` + `recommend` `7 recs` `evidence`/`confidence`/`benefit`/`risk`/`rollback` (all read-only, `p2_scan.json` `9.9K`, `p3_analysis.json` `23K`)
- `polaris_p4` `transaction list`/`show`/`compare`/`preview` `audit list` `apply --dry-run` `profile show` `explain` - all on `txStore` `/tmp/polaris-test-root/transactions` fixtures `stat /etc/fstab` `2026-08-31 21:19` unchanged, `systemctl is-enabled` read-only (via `RealSystemdProvider` `safeExec` `/usr/bin/systemctl` separate args, no `sh -c`)

---

## 5. Baseline Commands

- `polaris_real --json` → `P2` `9.9K` `Vendor Intel` `NVIDIA 10de:174d UNCLAIMED` (before `P7`) vs `CLAIMED` after `P7` `nvidia-smi` `470.256.02`
- `polaris_p3` `performance` `baseline` → `PerformanceBaseline` `15 metrics` `MetricMeta` `timestamp`/`unit`/`source`/`method`/`confidence` `available`/`note` (if `systemd.userspace` `0` → `unavailable: systemd userspace not collected` not guessed)

---

## 6. Bottleneck/Recommendation Commands

- `polaris_p3 bottlenecks --human` → `Bottleneck` `10` `category` `Boot`/`GPU`/`Memory` `severity` `LOW/MEDIUM/HIGH/CRITICAL` `confidence` `0-1` `evidence` `userspace 54.106s` `failed 1` `NVRM 490` `avail 4.2GB` `impact` `possibleCause` `investigation` `potentialOptimization` `risk` `R0-3`
- `polaris_p3 recommendations --human` → `Recommendation` `7` `id` `REC-001` `title` `problem` `evidence` `confidence` `expectedBenefit` `riskLevel` `affectedComponent` `why` `alternative` `rollbackConcept` `requiresReboot`/`requiresAuth`/`requiresApproval` `category`

---

## 7. Profile Commands

- `polaris_p4 profile show` → `{"usesAkonadi":"unknown",...,"usesKMail":"unknown",...}` `8` fields + `extra` sorted `unknown` default, `isDefaultUnknown` true, `deterministic` `FileSafety` `validateProfilePath` `..;|&` etc., `load` missing→`unknown` **no auto-create** (`ProfileStore::save` only on `profile set`), `symlink→throw`, `malformed→throw` `audit` `profile.load.malformed`
- `polaris_p4 profile show --json` → same JSON compact
- `polaris_p4 profile set usesKMail yes` → `{"field":"usesKMail","previousValue":"unknown","newValue":"yes","status":"updated"}` `# Updated usesKMail from unknown to yes` `ProfileStore` `atomic` `tmp+fsync+chmod 0600+rename` `0600` `validateProfilePath` `..;|&` etc. `isSymlink` `canonical` `create_directories`, `audit` `profile.updated` `field`/`previous`/`new` `applied=true` (`AuditLog` `hashEvent` `SHA256` `previousHash` chain `fsync`), `FileSafety` allowlist `~/.local/state/polaris/profile.json`
- `polaris_p4 profile set usesKMail yes` again → `{"field":"usesKMail","previousValue":"yes","newValue":"yes","status":"idempotent"}` `audit` `profile.update.idempotent` `applied=false` `isIdempotent` true, no `mmap` rewrite `stat` mtime unchanged
- `polaris_p4 profile set usesUnknownField yes` → `{"error":"Unknown profile field: usesUnknownField"}` `invalid_argument` `audit` `profile.update.rejected.unknown_field`, no inference `usesKMail→usesAkonadi` not inferred (`test_p13_profile_service` `no inference` `KMail→Akonadi` still `UNKNOWN`)

---

## 8. Explainability Commands

- `polaris_p4 explain akonadi-disable --json` → `{"candidateId":"akonadi-disable","candidateKind":"RECOMMENDATION","decision":"REQUIRE_CONFIRMATION"|`BLOCKED_BY_USER_WORKFLOW`|`RECOMMEND`,"decisionLabel":"...","whyNow":"Measured Akonadi 1302M 14 agents (P9 baseline 8.515s userspace, not in critical-chain). Candidate blocked because usesKMail=yes...","whatWillChange":"target=akonadi service, operation=disable...","whatWillNotChange":"Akonadi will remain enabled... / NVIDIA 470xx remains claimed...","evidence":["akonadi 14 agents 1302M"], "expectedBenefit":"~1.3GB RAM","confidence":0.65,"risk":"R2","reversibility":"High (akonadictl start)","rebootRequired":false,"authorizationRequired":true,"userImpact":"KMail would lose PIM","rejectionConditions":["profile: usesKMail=yes → BLOCKED","stale beforeHash..."],...}` `deterministic` sorted keys `isDeterministic` true, `P13` `ProfileAdvisor` `BLOCKED`/`REQUIRES`/`ALLOWED` + `Comparison` `expected`/`observed`/`verdict`
- `polaris_p4 explain akonadi-disable --verbose` → `WHY NOW: ...` `WHAT WILL CHANGE: ...` `WHAT WILL NOT CHANGE: ...` `EVIDENCE:` sorted `  - akonadi 14 agents...` `EXPECTED BENEFIT:` `CONFIDENCE:` `RISK:` `REVERSIBILITY:` `REBOOT:` `AUTHORIZATION:` `REJECTION CONDITIONS:` sorted `[REDACTED]` for `password`/`secret`, `ROLLBACK:` `BEFORE:` `AFTER:` `OBSERVED BENEFIT:` `VERDICT:` `LIMITATIONS:` `DEPENDENCIES:` `USER IMPACT:` `toHuman(verbose)` `containsSecret` `password`→`[REDACTED]`
- `polaris_p4 transaction explain TX-TEST-001 --json` → `explainTransaction` `TxState` `PREVIEWED`→`FAILED: stale beforeHash: expected abc observed def` `whyNow` `Transaction TX-TEST...` `whatWillChange` `target=... diff`, `whatWillNotChange` scope-aware, `rejectionConditions` `stale beforeHash` `expected abc` `observed def` `→ FAILED` `expected`/`observed` `auditOperation` `validation.failed.stale_beforeHash`, `rollbackSummary` `Restore from backup .../fstab.bak`, `beforeStateSummary`/`afterStateSummary` `userspace 54.106s`→`8.515s`, `observedBenefit` `MX130 claimed...` `verdict` `SUCCESS`/`REGRESSION` `hasRegression`, `limitations` `Comparison unavailable: ...`
- `polaris_p4 transaction explain TX-TEST-001 --verbose` → same plus `EVIDENCE` `DEPENDENCIES` `USER IMPACT`

---

## 9. Transaction Commands

- `polaris_p4 transaction preview dummy-test` → `PREVIEWED` `ChangePreview` `target` `/tmp/polaris-test-root/etc/fstab` `beforeState` `afterState` `diff` unified `method` `atomic write via helper FileModify` `privilege` `org.polaris.modify.fstab` `risk` `R2` `benefit` `Eliminate timeout` `rollback` `Restore from backup` `rebootRequired` false, `AuditLog` `transaction.previewed` `previousHash` chain
- `polaris_p4 transaction list` → `[{"id":"TX-TEST-...","state":"PREVIEWED","target":"...","risk":"R2"}]` (test fixtures)
- `polaris_p4 transaction show TX-TEST-001 [--json]` → `{"id":"TX-TEST-001","state":"PREVIEWED",...,"beforeHash":"...","approvedBeforeHash":"...","beforeBaseline":{...},"comparison":{...}}` (`P11` `beforeBaseline`/`afterBaseline`/`comparison` `P12` `beforeHash`/`approvedBeforeHash` etc.)
- `polaris_p4 transaction approve TX-TEST-001` → `{"transactionId":"TX-TEST-001","approval":"APPROVED","state":"APPROVED"}` `# Explicit approval recorded - not equivalent to launch, preview required first` `AuditLog` `transaction.approved` `TransactionValidator::bindApproval` `approvedBeforeHash`/`approvedTarget` binding for `P12` stale protection
- `polaris_p4 transaction compare TX-TEST-001 [--json]` → `P11` `Comparison` `{"transactionId":"TX-P7-...","comparison":"present"}` or `"not yet available - reboot-pending"` (if `rebootRequired` `true` and `afterBaseline` not yet captured)
- `polaris_p4 transaction rollback TX-TEST-001` → `{"error":"P4 rollback on test fixtures only - use TX-TEST id"}` (`BackupEngine::restore` test fixtures)

---

## 10. JSON Output Usage

- All `transaction`/`profile`/`explain` commands support `--json` for machine-readable deterministic `toJson()` (keys alphabetical, `evidence`/`rejectionConditions` sorted, `isDeterministic` true)
- Human-readable `toHuman(false)` is concise, `toHuman(true)` `--verbose` adds `EVIDENCE`/`DEPENDENCIES` but never `password`/`secret` (`containsSecret` → `[REDACTED]`)
- Parsers should depend on JSON, not human formatting; human prose is generated from structured fields, not sole truth
- `python3 -m json.tool` validates `Explanation::toJson()` output `{"candidateId":"akonadi-disable",...}`

---

## 11. Preview/Approval Workflow

1. `READ` (`polaris_real --json` `P2` `3813ms` `readOnlyGuard` true)
2. `MEASURE` (`polaris_p3` `performance` `baseline` `15 metrics` `MetricMeta`)
3. `ANALYZE` (`polaris_p3` `analyze`/`bottlenecks`/`recommendations` `7 recs`)
4. `EXPLAIN` (`polaris_p4 explain akonadi-disable --json` `WHY NOW` `usesKMail` `1302M` + `RECOMMEND`/`BLOCKED`)
5. `RECOMMEND` (`RecommendationEngine` `7 recs` `evidence`/`confidence`/`benefit`/`risk`)
6. `PREVIEW` (`polaris_p4 transaction preview dummy-test` → `TX-TEST-...` `PREVIEWED` `diff` `rollback` `rebootRequired` `privilege`)
7. **EXPLICIT APPROVAL** (`polaris_p4 transaction approve TX-TEST-...` → `APPROVED` `approvedBeforeHash`/`approvedTarget` binding, `AuditLog` `transaction.approved` `previousHash` chain, **not** `launch==approval`)
8. `BACKUP` (`TransactionStore::apply` `APPROVAL→VALIDATION→BACKUP→FINAL VALIDATION→APPLY` with `BackupEngine::create` versioned `SHA-256` `is_regular_file` `fsync` before `atomicWrite`)
9. `APPLY` (`FileSafety::atomicWrite` `tmp+fsync+rename` `chmod 0600` `isSymlink` check, only if both validations passed and `StateMachine` `BACKUP_CREATED→APPLYING` valid)
10. `VERIFY` (`is_regular_file` `sha256` check, `systemctl is-enabled`/`is-active` re-read via same `Real*Provider`)
11. `COMPARE` (`ComparisonEngine::compare` `before`/`after` `expectedBenefit` `observedBenefit` `metrics` `delta` `hasRegression` `threshold` `boot +10%` `available -1GB` `thermal +15C` `new_failed`, `isDeterministic` true)
12. `REGRESSION` (`hasRegression` `true` if `isHealth`/`isBootCritical` metric exceeds threshold, `verdict` `SUCCESS`/`REGRESSION` not `SUCCESS` merely because `command completed`)
13. `AUDIT` (`AuditLog` `hashEvent` `SHA256` `previousHash` chain `fsync` per `append`)

---

## 12. How Explicit Transaction Approval Works

- Approval is **tied** to `transactionId` + `target` + `expected change` + `current file hash`/`unit hash`/`package state` (`P5` `nvidia-settings` `beforeHash` `4ad53409` check, `P12` `beforeHash`/`approvedBeforeHash` `sha256` + `approvedTarget`/`approvedOperation` + `kernel`/`package`/`preconditions`, `TOCTOU` `canonical`)
- If `beforeHash` changes after `PREVIEW`, approval **invalidated**, require new `preview` ( `TransactionValidator::validateForApply` `stale beforeHash: expected <hash> observed <different> → FAILED` `audit` `validation.failed.stale_beforeHash` `expected`/`observed`/`applied=false` `backupCreated` `approvalValid`)
- `TransactionStore::approve` `bindApproval` snapshots `curAtApproval` `currentBeforeHash` → `approvedBeforeHash`, `currentTarget` → `approvedTarget`, etc., sets `approvalState=APPROVED` `idempotencyKey=id`
- `approval` **not** `authorization` (`AUTHORIZATION_REQUIRED` `AUTHORIZED` via `org.polaris.*` `auth_admin_keep` future helper `SO_PEERCRED` `flock`), `approval` not `application` (`apply.completed`), `application` not `optimization success` (`observedBenefit` vs `expectedBenefit`)
- `StateMachine::isValidTransition` `PROPOSED→PREVIEWED`→`APPROVAL_REQUIRED`→`APPROVED`→`AUTHORIZATION_REQUIRED`→`AUTHORIZED`→`BACKUP_CREATED`→`APPLYING`→`APPLIED`→`VERIFYING`→`VERIFIED`→`COMPLETED`, `PREVIEWED/APPROVAL_REQUIRED/APPROVED→FAILED` for stale, `COMPLETED`→`APPLYING` `COMPLETED`→`APPROVED` `FAILED→APPLYING` `PREVIEWED→APPLYING` etc. **rejected** `logic_error` `rejected, fail closed`

---

## 13. How Verification Works

- **Re-read** via same read-only providers (`RealOsProvider` `/proc/os-release` `uname`, `RealCpuProvider` `cpuinfo`, `RealMemoryProvider` `proc/meminfo`, `RealSystemdProvider` `systemctl`/`systemd-analyze` `execv` separate args `poll`, `RealGpuProvider` `pci.ids`+`glxinfo` `/usr/bin/glxinfo` `DISPLAY=:0`)
- Verify `requested change happened` (`sha256File` `beforeHash` vs `afterHash`), `no unintended changes`, `subsystem healthy` (`systemd --failed` `0` after `P7` reboot, `sensors` `50C` not `67C`), `no side effects` (`ss` `1716` `kdeconnect` still `avahi` `5353`)
- If `fails` and `rollbackPlan` `AVAILABLE` → `auto rollback` (future `TransactionStore::rollback` via `BackupEngine::restore`), else `stop`
- `P11` `Comparison` separates `operation succeeded` (`APPLIED`) vs `verification succeeded` (`VERIFYING→VERIFIED` via file hash) vs `expected benefit observed` (`observedBenefit` `MX130 claimed`) vs `regression detected` (`hasRegression` `false`)

---

## 14. How Comparison/Regression Information Is Displayed

- `polaris_p4 transaction compare TX-P7-... --json` → `{"transactionId":"TX-P7-...","comparison":"present ..."}`
- `Comparison` `metrics` `boot.userspace` (`isBootCritical` `relative_pct` `+10%` `thresholdValue 10.0`), `memory.available` (`isHealth` `absolute_gb` `-1GiB`), `swapUsed` (`isBackground`), `thermal.cpuMax` (`absolute_c` `+15C`), `systemd.failedCount` (`new_failed` any new), `nvidia.claimed` (`nvidia_claimed` `after < before` → `regression`), `loadAvg`, `zram`, `journal`
- `threshold` stored with result `thresholdDesc` `thresholdValue` `thresholdType` explainable, not hard-coded in CLI (`ComparisonEngine` owns logic, `regression` fail-closed: if `isHealth`/`isBootCritical` `regression` true → `verdict` not `SUCCESS`, avoids false positives `abs(delta) <1e-6` not regression)
- `expectedBenefit` vs `observedBenefit` separate, `observedBenefit` derived from `nvidia.claimed 0→1` + `no regression` → `SUCCESS` (`P7` fixture `54.106s` `4.2GB` `67C` `1` `0` → `8.515s` `6.5GB` `50C` `0` `1` `verdict SUCCESS`), `unavailable` `available false` `note` not guessed

---

## 15. How to Inspect Transaction State

- `polaris_p4 transaction list` → `[{"id":"TX-P5-20260831-001","state":"COMPLETED",...}]` (test `txStore` `/tmp/polaris-test-root/transactions` + real `~/.local/state/polaris/transactions`)
- `polaris_p4 transaction show TX-TEST-001` → raw JSON `{"id":"TX-TEST-001","state":"PREVIEWED",...,"beforeHash":"...","approvedBeforeHash":"...","beforeBaseline":{...},"comparison":{...}}` (`P12` `beforeHash`/`approvedBeforeHash` + `P11` `beforeBaseline`/`comparison`)
- `polaris_p4 transaction show TX-TEST-001 --json` → `beforeBaseline`/`afterBaseline`/`comparison` if present, `StateMachine::toString` `state`
- `polaris_p4 transaction explain TX-TEST-001 --json --verbose` → `P16` `Explanation` `WHY NOW`/`WHAT WILL CHANGE`/`WHAT WILL NOT CHANGE`/`rejectionConditions`/`rollbackSummary`/`observedBenefit`/`verdict`

---

## 16. How to Inspect Backups/Rollback Information

- `BackupEngine::backupRoot()` `~/.local/state/polaris/backups` real, `testBackupRoot()` `/tmp/polaris-test-root/backups` test
- `BackupEngine::create(transactionId, originalPath)` → `Backup` `transactionId`/`originalPath`/`backupPath` `~/.local/state/polaris/backups/<tx>/fstab.bak`/`timestamp`/`sha256` `is_regular_file` `fsync` versioned `no overwrite` (`exists` → throw `Backup already exists, refusing to overwrite`), `permissions` `0644` `owner` `group`, `size`
- `BackupEngine::restore(Backup)` → `FileSafety::validatePath` `~/.local/state/polaris/backups` `is_regular_file`, `ifstream` `src` → `ofstream` `dst` `atomicWrite` `tmp+fsync+rename`
- `Transaction` `rollbackPlan` `rollbackState` `AVAILABLE` (`P6` `systemctl enable mssql-server`, `P7` `dnf swap 470xx→610` `akmods --force` `dracut --force`, `P5` `nvidia-settings` `Hidden=false`), `docs/ROLLBACK.md`

---

## 17. What Commands Are Safe/Read-Only

**Safe/read-only** (no `FileSafety::validatePath` mutation, no `BackupEngine`, no `AuditLog` `apply`, `test_readonly` verifies `stat /etc/fstab` mtime unchanged):
- `polaris scan --json`
- `polaris health --json`
- `polaris_real --json`
- `polaris_p3` `performance` `baseline`/`benchmark`/`analyze`/`bottlenecks`/`recommendations`
- `polaris_p4 transaction list`
- `polaris_p4 transaction show`
- `polaris_p4 transaction compare`
- `polaris_p4 transaction preview`
- `polaris_p4 audit list`
- `polaris_p4 apply --dry-run`
- `polaris_p4 profile show`
- `polaris_p4 explain` / `polaris_p4 transaction explain`

---

## 18. What Commands Can Eventually Mutate the Host

**Can mutate** (only via `P12` hardened `TransactionStore::apply` `APPROVAL→VALIDATION→BACKUP→FINAL VALIDATION→APPLY` with `StateMachine` `validateTransition`, `TransactionValidator` `validateForApply` `stale`/`TOCTOU`, `BackupEngine` backup, `FileSafety` `atomicWrite`, `AuditLog` `apply.completed`):
- `polaris_p4 transaction approve <id>` (records `APPROVED` `approvedBeforeHash` binding, **not** `APPLY` itself, but required for `APPLY`)
- Future `polaris_p4 transaction apply <id>` would be helper `org.polaris.*` `IpcProtocol` `SO_PEERCRED` `flock` `RecoveryDetector` `BACKUP_CREATED→APPLYING` (but `P14` allowlist `ping`/`info` only, **no privileged mutation** `NO PRIVILEGED MUTATION OPERATION IS ENABLED BY P14/P16`, `P18` still `33/33` no `transaction apply` via CLI)
- `polaris_p5` `apply` for `~/.config/autostart/nvidia-settings-user.desktop` `Hidden=true` (P5 `FileSafety::atomicWrite` `temp+fsync+rename` `isSymlink` check, `BackupEngine` `is_regular_file`, `AuditLog` `fsync`, `9` preconditions)
- `polaris_p4 profile set` (writes `~/.local/state/polaris/profile.json` `0600` `FileSafety` allowlist `profile.json`, `atomicWrite` `tmp+fsync+rename`, `audit` `profile.updated`, **not** host `systemd`/`dnf`/`fstab` mutation)

All such mutations require **explicit approval for every real mutation** (`P5` `TX-P5-20260831-001` `Hidden=true` required `approve` with `beforeHash`, `P6` `TX-P6-...-MSSQL-DISABLE-PREVIEW-V2` required `approve` for `mssql` disable, `P7` `TX-P7-NVIDIA-470xx` required `NVIDIA-MIGRATION-CANDIDATE-470xx` explicit approval for `R3`).

---

## 19. Exact Safety Workflow

```
READ → MEASURE → ANALYZE → EXPLAIN → RECOMMEND → PREVIEW → EXPLICIT APPROVAL → BACKUP → APPLY → VERIFY → COMPARE → REGRESSION → AUDIT
```

1. `READ` (`polaris_real` `P2` `3813ms` `readOnlyGuard` true, `openReadOnly` `ifstream`, no `sh -c`)
2. `MEASURE` (`BaselineEngine` `collect()` `3812ms` `15 metrics` `MetricMeta` `timestamp`/`unit`/`source`/`method`/`confidence` `available` `note`, `BenchmarkEngine` `quick` `cpu_prime` `0.043ms` `stddev 0.0004`)
3. `ANALYZE` (`BottleneckEngine` `10` bottlenecks `critical-chain` `BLOCKER` vs `background`, `RecommendationEngine` `7` recs `evidence`/`confidence`/`benefit`/`risk`/`rollback`/`reboot`/`auth`)
4. `EXPLAIN` (`ExplanationEngine` `explainCandidate`/`explainTransaction` `WHY NOW` evidence-backed `WHAT WILL CHANGE` transaction-backed `WHAT WILL NOT CHANGE` explicit `rejectionConditions` deterministic, `ProfileAdvisor` `BLOCKED`/`REQUIRES`/`ALLOWED` + `Comparison` `expected` vs `observed`)
5. `RECOMMEND` (`Recommendation` `7` `RANK` `PREVIEW` not `APPLY`, `ProfileAdvisor` constrains `akonadi` `REJECTED` if `usesKMail=yes`)
6. `PREVIEW` (`polaris_p4 transaction preview dummy-test` → `TX-TEST-...` `PREVIEWED` `ChangePreview` `diff` `rollback` `rebootRequired` `privilege` `risk`, `AuditLog` `transaction.previewed` `previousHash` chain)
7. **EXPLICIT APPROVAL** (`polaris_p4 transaction approve TX-TEST-...` → `APPROVED` `approvedBeforeHash` `approvedTarget` `TransactionValidator::bindApproval`, `AuditLog` `transaction.approved` `previousHash` `fsync`, **not** `launch==approval`)
8. `BACKUP` (`TransactionStore::apply` `APPROVAL→VALIDATION→BACKUP` `BackupEngine::create` versioned `~/.local/state/polaris/backups/<tx>/` `SHA-256` `is_regular_file` `fsync` `no overwrite`, if fails `FAILED` do not `APPLY`)
9. `APPLY` (`FileSafety::atomicWrite` `tmp+fsync+rename` `chmod 0600` `isSymlink` check, only if both validations passed and `StateMachine` `BACKUP_CREATED→APPLYING` valid, `IpcProtocol` `SO_PEERCRED` `flock` future)
10. `VERIFY` (re-read via same `Real*Provider` `sha256File` `beforeHash` vs `afterHash`, `systemctl --failed` `0` after `P7` reboot, `sensors` `50C` not `67C`, `lsmod` `nvidia`, `kwin` active)
11. `COMPARE` (`ComparisonEngine::compare` `before`/`after` `expectedBenefit` `observedBenefit` `metrics` `delta` `hasRegression` `threshold` `boot +10%` `available -1GB` `thermal +15C` `new_failed`, `isDeterministic` true)
12. `REGRESSION` (`hasRegression` `true` if `isHealth`/`isBootCritical` metric exceeds threshold `>10%` `>1GB` `>15C` `new_failed`, `verdict` not `SUCCESS` if `hasRegression`, avoids false positives `abs(delta) <1e-6` not regression, `zram` `isBackground` not `isHealth`)
13. `AUDIT` (`AuditLog` `hashEvent` `SHA256(timestamp+transactionId+operation+user+approval+auth+previousHash)` `eventHash`, `previousHash` chain, `fsync` per `append` `open`+`fsync` after `flush`, `list` `get` preserve, never `password` logging, `explain` `explanation.generated` not `applied`)

---

## 20. Practical Example Sessions

**Session 1: Read-only discovery, no mutation**
```bash
cmake -S . -B build --fresh && cmake --build build && ctest --test-dir build --output-on-failure # 33/33 100%
./build/polaris_real --json | python3 -m json.tool | head -n 100 # P2 9.9K Vendor Intel NVIDIA 10de:174d UNCLAIMED (before P7) vs CLAIMED after
./build/polaris_p3 --help # actually ./build/polaris_p3 with no args shows P3 Analyze - READ-ONLY
./build/polaris_p4 profile show --json # {"usesAkonadi":"unknown",...} # not exists, default unknown, not created
./build/polaris_p4 explain akonadi-disable --json # {"candidateId":"akonadi-disable","decision":"REQUIRE_CONFIRMATION",...} whyNow 1302M
./build/polaris_p4 explain bluetooth-disable --verbose # WHY NOW: Measured bluetooth enabled active 2 paired... WHAT WILL NOT CHANGE: NVIDIA remains...
stat /etc/fstab | grep Modify # 2026-08-31 21:19:15 unchanged
```

**Session 2: Profile explicit update (auditable, reversible, not authorization)**
```bash
./build/polaris_p4 profile show --json # unknown
./build/polaris_p4 profile set usesKMail yes --json # {"field":"usesKMail","previousValue":"unknown","newValue":"yes","status":"updated"} # AuditLog profile.updated field usesKMail previous unknown new yes applied true, FileSafety atomicWrite tmp+fsync+chmod 0600+rename 0600
cat ~/.local/state/polaris/profile.json # {"usesAkonadi":"unknown",...,"usesKMail":"yes",...} sorted
./build/polaris_p4 explain akonadi-disable --json # now decision BLOCKED_BY_USER_WORKFLOW whyNow usesKMail=yes whatWillNotChange Akonadi will remain enabled...
./build/polaris_p4 profile set usesKMail unknown --json # reversible
```

**Session 3: Transaction preview → approval → verification (test fixtures only, no real host mutation)**
```bash
./build/polaris_p4 transaction preview dummy-test # PREVIEWED TX-TEST-12345 target /tmp/polaris-test-root/etc/fstab diff "- UUID ... swap"
./build/polaris_p4 transaction list # [{"id":"TX-TEST-12345","state":"PREVIEWED"}]
./build/polaris_p4 transaction show TX-TEST-12345 --json # {"id":"TX-TEST-12345","state":"PREVIEWED","beforeHash":"...","approvedBeforeHash":"",...}
./build/polaris_p4 transaction approve TX-TEST-12345 # {"transactionId":"TX-TEST-12345","approval":"APPROVED","state":"APPROVED"} # Explicit approval recorded - not equivalent to launch
./build/polaris_p4 transaction show TX-TEST-12345 --json # approvedBeforeHash now present, StateMachine APPROVED
./build/polaris_p4 apply --dry-run dummy-test # Dry-run MUST NOT write files, invoke privileged ops, or request password - verified
./build/polaris_p4 transaction explain TX-TEST-12345 --json # WHY NOW: Transaction TX-TEST-12345 (dummy-test) target ... STATE PREVIEWED ... WHAT WILL CHANGE: target=... diff ... WHAT WILL NOT CHANGE: Akonadi remains ... REJECTION CONDITIONS: stale beforeHash ... profile: usesKMail=yes ... verbose adds EVIDENCE
./build/polaris_p4 audit list # [{"transactionId":"TX-TEST-12345","hash":"..."}, {"transactionId":"TX-TEST-12345","hash":"..."}] previousHash chain
# Real host mutation would be via TransactionStore::apply with SO_PEERCRED flock BackupEngine (but P14 allowlist ping/info only, NO PRIVILEGED MUTATION OPERATION IS ENABLED BY P14/P16, P18 still 33/33 no transaction apply via CLI)
```

**Session 4: Comparison / regression (after hypothetical APPLY with reboot)**
```bash
./build/polaris_p4 transaction compare TX-P7-NVIDIA-470xx-20260831 --json # P11 ComparisonEngine: boot 54.106→8.515 -84% not regression, available 4.2→6.5GB not regression, thermal 67→50 not regression, failed 1→0 not regression, nvidia 0→1 not regression, verdict SUCCESS hasRegression false, expectedBenefit restore NVIDIA vs observedBenefit MX130 claimed, rebootMarker rebooted-2026-09-01T00:36
```

**Session 5: Verify no host mutation during discovery**
```bash
stat /etc/fstab | grep Modify # 2026-08-31 21:19:15 (from P2 Level2, unchanged since)
systemctl is-enabled mssql-server # disabled (P6)
ls /run/polaris/helper.sock # No such file (helper not installed, P14 defined but never created, tests use /tmp/polaris-test-root/p14)
ls /run/polaris/transaction.lock # No such file
```

---

*No real-host optimization was performed in P16 explainability. CLI remains unified, no new binary. Qt GUI is future direction, `gui/` empty.*

