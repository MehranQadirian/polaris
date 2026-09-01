# P15 - Test / CI / Fixture Expansion - Plan

**Phase:** P15 - Engineering/Testing, Not Host Optimization  
**Mode:** READ-ONLY PLANNING + IMPLEMENTATION - no `dnf`, `akmods`, `dracut`, `sudo`, `reboot`, no Akonadi/NVIDIA/mssql/fstab/zram mutation - only `tests/` expansion, `core/` test helpers, `docs/` updates, optional `.github/workflows/ci.yml`  
**Date:** 2026-09-01 06:00 +0330  
**Source:** Inspect `~/Documents/lin-opt` P14 state (`core/safety/transaction/StateMachine.h` 16 states, `TransactionValidator.h` stale matrix, `TransactionStore.h` idempotent, `BackupEngine` no-overwrite, `AuditLog` fsync, `core/ipc/IpcProtocol` bounded, `TransactionLock` flock, `RecoveryDetector` detection-only, `core/profile/UserProfile` 8 fields, `docs/PROJECT_HANDOFF.md` 24/24 tests, `CMakeLists.txt` 118L, no CI)  
**Dependency:** P14 `IPC/Security` (COMPLETED, 24/24) - P15 adds deterministic verification, not new privileged logic

---

## 1. What P14 Left & Why P15 Is Next

P14 added `IpcProtocol` (64KB bounded, `ping`/`info` only), `IpcAuth` `SO_PEERCRED`, `IpcServer` 0600 not world-writable, `TransactionLock` `flock`, `RecoveryDetector` `BACKUP_CREATED→FAILED` detection-only, but:
- No table-driven **stale-preview matrix** (7 fields × 3 states) - existing `test_p12_stale` has 10 cases but not parametrized, missing deterministic error `expected`/`observed` table
- No explicit **TOCTOU between validation gates** with backup preserved verification
- No **rollback preservation** test (backup hash stable, not overwritten, survives reload)
- No **concurrent lock** with deterministic `exactly one` semantics (existing `test_p14_lock` has 2 threads but not table-driven)
- No **regression threshold boundaries** (`+10%` exactly, just below/above) for `ComparisonEngine` beyond P11's single boundary
- No **observedBenefit** mismatch matrix (expected positive vs observed zero/negative)
- No **FileSafety** regression table (11 protections × allowlist)
- No **AuditLog integrity** chain test (previousHash→eventHash deterministic, fsync, no secrets, idempotent distinguishable)
- No **CI** (`cmake -S . -B build --fresh && cmake --build && ctest`) - `ls .github/` not exists, `ls .gitlab-ci.yml` not exists

P15 value: make `ctest 100%` **regression-proof** for future `P16`/`P17` (which will propose `akonadi`/`bluetooth` optimizations) by providing deterministic fixtures and CI.

---

## 2. Objectives (Deterministic, Fixture-Isolated)

1. **State-machine lifecycle** - valid `PREVIEWED→APPROVAL_REQUIRED→APPROVED→AUTHORIZATION_REQUIRED→BACKUP_CREATED→APPLYING→APPLIED→VERIFYING→VERIFIED→COMPLETED` and rejected `COMPLETED→APPLYING`, `FAILED→APPLYING`, etc., fail-closed.
2. **Stale matrix** - table-driven for 7 fields (`target`, `operation`, `beforeHash`, `beforeUnitHash`, `kernelVersion`, `packageStateHash`, `preconditions`) × 3 states (`UNCHANGED→accepted`, `CHANGED→rejected`, `UNAVAILABLE→rejected`) plus multi-field and deterministic `expected`/`observed` error.
3. **TOCTOU** - `preview→approval→first validation→backup→second validation→apply` with change between gates → fail-closed, no mutation, backup intact, `FAILED`, audit `expected`/`observed`.
4. **Idempotency** - `create` twice, `approve` twice, `apply` twice on `COMPLETED`, `verify` twice, reload from JSON → deterministic, no duplicate mutation, no backup overwrite.
5. **Concurrency/lock** - isolated `flock` `tryLock` exclusive, second fails deterministically, release→reacquire, concurrent 4 threads → exactly one holds at a time, `FD_CLOEXEC`, stale/invalid parent behavior.
6. **Crash/recovery** - `BACKUP_CREATED`/`APPLYING`/`APPLIED`/`VERIFYING`/`AUTHORIZED` → incomplete detected, `COMPLETED` not, backup existence, corrupted state fails safely, never auto-apply, no host mutation.
7. **Rollback** - backup `sha256` stable, not overwritten on second `create`, survives reload, `FAILED` preserves `BACKUP_CREATED` rollback capability.
8. **Post-change/regression** - improvement, no change, regression, threshold `==` boundary (10% → not regression), just below (9.9% → not), just above (10.1% → regression), unavailable, multi-regression, boot-critical vs background, `expected` vs `observed`.
9. **IPC security** - malformed, oversized, truncated, unknown op, `exec`, shell, traversal, NUL, oversized arg, password, spoofed UID, invalid socket path/symlink/stale, timeout, concurrent.
10. **FileSafety** - `..`, `;|&` `` ` `` `$`, NUL, oversized, symlink, canonical escape, profile allowlist, fixture transaction path.
11. **Audit integrity** - every rejection auditable, `previousHash` chain valid, `eventHash` deterministic SHA256, `fsync` intact, no secrets, duplicate/idempotent distinguishable, stale `expected`/`observed`.

---

## 3. Design Constraints

- Keep `core` no Qt, offline-first, deterministic, no network, no `sudo`/`dnf`.
- Tests **must** use isolated roots: `/tmp/polaris-test-root/p15` or unique `mkdtemp`-like subdir per test, clean up after, never `/run/polaris`, never `~/.local/state/polaris/profile.json`, never `/etc`.
- Prefer table-driven `struct TestCase {string name; Request req; bool expectValid; string expectedField;}` over copy-pasted cases.
- Prefer deterministic fixtures (`original="content A"`, `stale="content B"`, `after="content C"` with known SHA256) over `sleep`.
- Do not weaken `ReadOnlyGuard`, `FileSafety`, `StateMachine`, `TransactionValidator`, `BackupEngine` (`no overwrite`), `AuditLog` (`fsync`), `IpcProtocol` (`64KB`), `TransactionLock` (`flock`), `RecoveryDetector` (detection-only) merely to make tests pass.

---

## 4. Test Areas (Detailed)

**A. Lifecycle (StateMachine)**
- Table valid: `PROPOSED→PREVIEWED`, `PREVIEWED→APPROVAL_REQUIRED`, `APPROVAL_REQUIRED→APPROVED`, `APPROVED→AUTHORIZATION_REQUIRED`, `AUTHORIZATION_REQUIRED→AUTHORIZED`, `AUTHORIZED→BACKUP_CREATED`, `BACKUP_CREATED→APPLYING`, `APPLYING→APPLIED`, `APPLIED→VERIFYING`, `VERIFYING→VERIFIED`, `VERIFIED→COMPLETED`, `FAILED→ROLLING_BACK→ROLLED_BACK`, `PREVIEWED/APPROVAL_REQUIRED/APPROVED→FAILED` (P12).
- Table rejected: `COMPLETED→APPLYING`, `COMPLETED→APPROVED`, `FAILED→APPLYING`, `PREVIEWED→APPLYING`, `APPROVAL_REQUIRED→APPLYING`, `APPLYING→APPLYING`, `COMPLETED→CANCELLED`, `ROLLED_BACK→APPLYING`, plus `unverifiable→FAILED`.

**B. Stale Matrix**
- For each field in `TransactionValidator::CurrentState` (`currentTarget`, `currentOperation`, `currentBeforeHash`, `currentUnitHash`, `currentKernelVersion`, `currentPackageStateHash`, `currentPreconditions` map), create `Request`/`Transaction` with `approved*` snapshot, then `CurrentState` with `UNCHANGED` (=approved → `valid`), `CHANGED` (different → `stale_<field>`), `UNAVAILABLE` (empty where approved non-empty → `unverifiable` → fail-closed). Deterministic `expected=approvedHash`, `observed=currentHash`.

**C. TOCTOU**
- Fixture file `/tmp/polaris-test-root/p15_toctou/etc/fstab` `original`, `Preview` binds `beforeHash=sha256(original)`, `first validation` with `currentBeforeHash=sha256(original)` → pass, `BackupEngine::create` copies `original`, then mutate file to `stale` before `second validation` (`finalPreconditionValidation` re-reads `sha256(stale)` → `stale_beforeHash` → `FAILED`, no `atomicWrite`, backup `sha256(original)` unchanged, file remains `stale` not `after`, `TransactionStore` state `FAILED`.

**D. Idempotency**
- `TransactionStore::create` same `id` twice → second `valid=false` `already exists`, file not overwritten (hash same), audit `duplicate`.
- `approve` same `id` twice with same `CurrentState` → second `valid=true` `idempotent` `approval.duplicate`, no second `backup`.
- `apply` on `COMPLETED` twice → second `apply.rejected.already_completed`, file hash after first `sha256(after)` unchanged after second, audit `already_completed`.
- `verify` twice on `COMPLETED` → both `valid=true` `idempotent`, no mutation.
- Reload: `save` transaction JSON to `storePath`, `load` via `RecoveryDetector` or `TransactionStore::get` → state preserved, `isDefaultUnknown` not affected.

**E. Concurrency/Lock**
- `TransactionLock::testLockPath()` isolated per test dir, `tryLock` exclusive, second `tryLock` fails `EWOULDBLOCK` → `lock.rejected`, `unlock` → second then succeeds, concurrent 4 threads `tryLock` → exactly one holds at any moment (count successes sequential, no deadlock), `FD_CLOEXEC` set, parent symlink/world-writable rejected.

**F. Crash/Recovery**
- Create fixture transactions `BACKUP_CREATED`, `APPLYING`, `APPLIED`, `VERIFYING`, `AUTHORIZED` in `/tmp/polaris-test-root/p15_recovery/transactions` with `*.json` `state` field, `RecoveryDetector::detect` → `incomplete` true, `COMPLETED` → false, corrupted `state: "INVALID"` → fail-safe (treated as `PROPOSED` not incomplete, or audit `malformed`), `backupExists` true if `testBackupRoot/<id>/fstab.bak` exists, recovery never calls `FileSafety::atomicWrite` (file hash before/after same).

**G. Rollback**
- `BackupEngine::create` before mutation, `sha256File(backupPath)` stable, second `create` same `id` → throw `already exists` (no overwrite), reload via `RecoveryDetector` still finds backup, `FAILED` preserves `backupState=CREATED`.

**H. Post-Change/Regression**
- `ComparisonEngine::compare` with explicit `PerformanceBaseline` fixtures: `boot 50→54` (+8% not regression), `50→55` (+10% not), `50→55.5` (+11% regression), `available 8→6.5` (-1.5GB regression), `thermal 50→70` (+20C regression), `failed 0→1` regression, `unavailable` (0→0) not regression, multi-regression (`boot` + `thermal` → `hasRegression` true), `expectedBenefit` vs `observedBenefit` deterministic.

**I. ObservedBenefit**
- `expectedBenefit="restore NVIDIA"` `observedBenefit` derived from `nvidia.claimed 0→1` + no regression → `SUCCESS`, `expected` positive `observed` zero → `NO_BENEFIT`, mismatch → `NO_CHANGE`/`REGRESSION`, missing metrics → `INCONCLUSIVE`, deterministic.

**J. IPC Security**
- `IpcProtocol::validateRaw` table: malformed (missing `}`), oversized (65KB), truncated (no closing `"`), unknown op (`unknownOp`), arbitrary `exec`, shell `; rm`, traversal `..`, NUL `\0`, oversized arg 4097, password field, spoofed `uid`, unauthorized peer, missing cred, invalid socket path, symlink socket, insecure parent, stale socket.

**K. FileSafety**
- `FileSafety::validatePath` table: `..`, `;`, `|`, `&`, `` ` ``, `$`, `\0`, `>4096`, symlink (`isSymlink` true → `atomicWrite` throw), `canonical` escape (path `/tmp/polaris-test-root/etc/../etc/passwd` → canonical `/etc/passwd` not under allowlist → reject), profile allowlist `~/.local/state/polaris/profile.json` allowed, fixture transaction path `/tmp/polaris-test-root/p15/...` allowed.

**L. Audit Integrity**
- For each security rejection, `AuditLog::list` or direct `audit.log` contains `operation` `ipc.request.rejected`/`validation.failed.stale_*`/`lock.rejected` with `expected`/`observed`, `previousHash` chain valid (each `eventHash = sha256(timestamp+transactionId+operation+user+approval+auth+previousHash)`), `eventHash` deterministic (same inputs → same hash), `fsync` intact (file exists after `append`), no `password`/`secret` value in audit (only field name), duplicate vs idempotent distinguishable (`create` duplicate `already exists` vs `approve` idempotent `already approved`).

---

## 5. Fixture Isolation

- Each test defines `std::string root = "/tmp/polaris-test-root/p15_" + testName;` `remove_all(root)` `create_directories(root)`; all files (`profile.json`, `transactions/*.json`, `backups/*`, `audit.log`, `helper.sock`, `transaction.lock`, `etc/fstab`) under `root`; never `~/.local/state/polaris` (real profile), never `/run/polaris` (real socket/lock), never `/etc` (real fstab). Cleanup after (or leave for inspection, but not required).
- Use deterministic `original="original\n"`, `stale="stale\n"`, `after="after\n"` with known `hashString` (e.g., `sha256("original\n")` known constant) for `expected`/`observed` asserts.
- Avoid flaky `sleep` for concurrency: use `flock` non-blocking `EWOULDBLOCK` deterministic, not `sleep 1`.

---

## 6. CI / Build

- Inspect current CI: `ls .github/` not exists, `ls .gitlab-ci.yml` not exists → add minimal `.github/workflows/ci.yml` if appropriate for CMake/Ninja project.
- CI steps: `cmake -S . -B build --fresh`, `cmake --build build`, `ctest --test-dir build --output-on-failure`, fail on non-zero, no `sudo`/`dnf`/`systemctl`/`reboot`, no network.
- Do not introduce external services, do not require `Qt`, do not break offline-first.
- Also consider `CMakeLists.txt` `enable_testing()` already present, so CI just runs that.

---

## 7. Test Quality

- Table-driven: `std::vector<TestCase>` loops.
- Deterministic: no `rand`, no `sleep` for correctness, only for concurrency `tryLock` timing (50ms sleep while holding lock, deterministic).
- Explicit `expected`/`observed` in asserts.
- Negative/security tests: each invalid case asserts `!valid` and `field`/`auditOperation` contains `rejected`/`stale`.
- Reload tests: `save` then `load` then `==`.
- Concurrency without flaky: `TransactionLock` non-blocking ensures deterministic `second fails` while first holds (no race on `tryLock` order, but both threads try, one wins, other fails `EWOULDBLOCK`).

---

## 8. Implementation Files

- **New tests (P15):** `tests/unit/test_p15_lifecycle.cpp` (state-machine lifecycle valid/rejected table), `tests/unit/test_p15_stale_matrix.cpp` (7 fields ×3 states + multi), `tests/unit/test_p15_toctou.cpp` (TOCTOU with backup), `tests/unit/test_p15_idempotency.cpp` (create/approve/apply/verify idempotent), `tests/unit/test_p15_lock_concurrent.cpp` (flock table), `tests/unit/test_p15_recovery_rollback.cpp` (crash/rollback), `tests/unit/test_p15_regression.cpp` (threshold boundaries, unavailable, multi), `tests/unit/test_p15_ipc_filesafety_audit.cpp` (IPC/FileSafety/Audit integrity)
- Or consolidated into fewer files with same coverage: at least 4 new executables to keep `ctest` readable, total new tests ≥15, existing tests untouched.
- **CI:** `.github/workflows/ci.yml` (if not exists, add minimal)

---

## 9. Validation (Before COMPLETE)

- Clean `cmake -S . -B /tmp/polaris_p15_build --fresh && cmake --build && ctest` - must be `ctest 100%` (existing 24 + new P15 ≥8 → ≥32).
- Verify `p4_security` 9/9, `p12_*` 4 suites, `p13_*` 4 suites, `p14_*` 7 suites still pass.
- Verify `grep -r "sudo" tests/p15` 0, `grep -r "dnf" tests/p15` 0, `grep -r "/run/polaris"` tests/p15 0 (only `/tmp/polaris-test-root`), `grep -r "/etc/fstab"` tests/p15 only read via `FileSafety` not mutate.
- Verify `stat /etc/fstab` mtime unchanged, `ls /run/polaris/helper.sock` not exists, `ls ~/.local/state/polaris/profile.json` mtime unchanged or not exists, `ls /tmp/polaris-test-root` contains `p15_*` fixtures only.
- Verify `python3 -m json.tool` on `profile.json` fixtures deterministic.
- Verify docs: `P15_PLAN.md`, `P15_IMPLEMENTATION_REPORT.md`, `ROADMAP.md` P15 COMPLETE.

---

## 10. Documentation to Update

- `docs/P15_PLAN.md` (this)
- `docs/P15_IMPLEMENTATION_REPORT.md` (post-impl: files, tests, counts, build, host verification, limitations, next P16)
- `docs/ARCHITECTURE.md` - add Test/CI layer note if needed
- `docs/ROADMAP.md` - P15 COMPLETE, next P16
- `docs/PROJECT_STATE.json` - currentPhase P15, next P16, completedPhases + P15, tests 32+ total
- `docs/PROJECT_HANDOFF.md` - P15 section, host state still no reboot, candidate Akonadi still REJECTED

---

## 11. Safety Boundaries Summary (What Is NOT Implemented)

- **No host mutation:** P15 does not `dnf`, `systemctl`, `akmods`, `dracut`, `modprobe`, `fstab`, `zram`, `reboot`, helper install, profile write to real home.
- **No privileged IPC mutation:** `IpcProtocol` allowlist remains `ping`/`info` only (P14), P15 does not add `transaction.apply` via IPC.
- **No business logic in helper:** `TransactionValidator`/`StateMachine` remain in `core/safety`, not duplicated.
- **No network:** `AF_UNIX` only.
- **No password handling.**

---

## 12. Not In Scope (P16 onward)

- No Explainability/UI (P16), no Campaign2 (P17), no Final Benchmark (P18) - those remain future.

