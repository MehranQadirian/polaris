# P12 - Transaction/State-Machine Hardening - Implementation Report

**Phase:** P12 - Engineering, Not Host Optimization  
**Date:** 2026-09-01 03:30 +0330  
**Source:** Repository `~/Documents/lin-opt` verified via `cmake`, `ctest`, `ls`, `cat`, `stat`, `systemctl` (read-only), `sha256` - not conversation memory  
**Status:** **COMPLETE** - stale-preview protection + idempotency + fail-closed validation + audit explainability, all on fixtures `/tmp/polaris-test-root`, no real-host mutation, no reboot

---

## 1. Implementation Summary

Hardened the existing transaction lifecycle without redesigning architecture. Added deterministic staleness detection between `PREVIEWED → APPROVAL_REQUIRED → APPROVED → AUTHORIZATION_REQUIRED → AUTHORIZED → BACKUP_CREATED → APPLYING` and enforced idempotency at the store and StateMachine layers. Flow is now:

```
PREVIEW (snapshot beforeHash/unitHash/kernel/package/preconditions)
→ APPROVAL (bind approved* via TransactionValidator::bindApproval, record approvedTarget/approvedOperation/approvedBeforeHash/approvedUnitHash/approvedKernelVersion/approvedPackageStateHash/approvedPreconditions)
→ PRECONDITION VALIDATION (validateForApply against CurrentState, fail-closed if any mismatch or unverifiable)
→ BACKUP (BackupEngine::create versioned SHA256, no overwrite, only if first validation passed)
→ FINAL PRECONDITION VALIDATION (re-reads file, TOCTOU symlink/canonical, backupState==CREATED, same stale checks post-backup)
→ APPLY (FileSafety::atomicWrite only if both validations passed and StateMachine BACKUP_CREATED→APPLYING valid)
→ VERIFY/COMPLETED
```

If any validation fails: **do not apply, do not partially mutate, transition to FAILED** (via `StateMachine` `PREVIEWED/APPROVAL_REQUIRED/APPROVED/AUTHORIZATION_REQUIRED/AUTHORIZED/BACKUP_CREATED → FAILED` where needed), record `validationResult` and audit event with `expected`/`observed`/`failingField`/`applied=false`/`backupCreated`/`approvalValid`.

All checks are pure, deterministic, side-effect-free; providers (real or mock) supply `CurrentState`, validator compares.

---

## 2. Files Changed / Added

**Modified:**
- `core/safety/transaction/Transaction.h:26` - added 15 fields: `beforeHash`, `approvedBeforeHash`, `beforeUnitHash`, `approvedUnitHash`, `kernelVersion`, `approvedKernelVersion`, `packageStateHash`, `approvedPackageStateHash`, `approvedTarget`, `approvedOperation`, `preconditions`, `approvedPreconditions`, `idempotencyKey`, `appliedAt`, `completedAt`, `validationResult` (empty default → backward compatible)
- `core/safety/transaction/StateMachine.h:55` - extended allowed map with `PREVIEWED → FAILED`, `APPROVAL_REQUIRED → FAILED`, `APPROVED → FAILED` (fail-closed stale paths), added `isTerminal`/`isFailed` helpers; kept all illegal examples rejected
- `core/safety/audit/AuditLog.cpp:1` - added `unistd.h`/`fcntl.h`, `fsync` per `append` (open RO + fsync after flush) for crash safety, preserves hash chain
- `docs/TRANSACTION_MODEL.md:1` - updated fields, states, validation, preconditions (two gates), backward compat note
- `docs/ARCHITECTURE.md:1` - added P12 section (validator/store/SM/audit)
- `docs/ROADMAP.md:23` - P12 COMPLETE line, updated ranked list (P11/P12 COMPLETE, P13 NEXT)
- `CMakeLists.txt:9` - add `TransactionValidator.cpp` + `TransactionStore.cpp` to `polaris_core`, add 4 P12 test executables

**Added:**
- `core/safety/transaction/TransactionValidator.h:1` (222 lines) + `.cpp` (stub) - `CurrentState`, `ValidationResult`, `hashString` (OpenSSL SHA256), `bindApproval`, `validateApprovalBinding`, `isStale`, `validateForApply`, `finalPreconditionValidation` (target/operation/beforeHash/unitHash/kernel/package/precondition/TOCTOU, unverifiable → fail-closed, `auditOperation` strings)
- `core/safety/transaction/TransactionStore.h:1` (532 lines) + `.cpp` (stub) - `create` (duplicate deterministic, no overwrite, persist JSON, audit `transaction.create.rejected.duplicate`), `get`/`exists`, `approve` (bind, state transitions PREVIEWED→APPROVAL_REQUIRED→APPROVED, idempotent `approval.duplicate.already_approved`), `canApply`, `apply` (idempotency COMPLETED→reject `already_completed`, first validation → backup → final validation with re-read hash/canonical → StateMachine `BACKUP_CREATED→APPLYING` → `atomicWrite` → `APPLIED→VERIFYING→VERIFIED→COMPLETED`, fail-closed to FAILED on any stale, audit `validation.failed.stale_*` with expected/observed), `verify` (idempotent), `clear` (wipes transactions + `testBackupRoot`), `persist`/`advanceToBackupCreated`/`appendAudit`
- `tests/security/test_p12_stale.cpp` (495 lines) - 10 categories: valid proceeds + 5 stale dims (beforeHash, unitHash, kernel, target, operation) + packageState + precondition + final precondition blocks apply + no mutation + TOCTOU symlink
- `tests/security/test_p12_idempotency.cpp` (210 lines) - 5 categories: approval bound to id + mismatch + duplicate id + COMPLETED cannot re-apply + repeated verify + duplicate approval idempotent
- `tests/security/test_p12_statemachine.cpp` (110 lines) - illegal transitions rejected (11 cases) + valid recovery (FAILED→ROLLING_BACK→ROLLED_BACK, APPROVED→FAILED) + terminal + backup_created
- `tests/unit/test_p12_transaction_model.cpp` (226 lines) - backward compat + audit stale + audit idempotency/duplicate
- `docs/P12_PLAN.md` (plan, 15 sections, ranking, model, binding, stale matrix, state delta, backup boundary, audit, files, 18 tests)
- `docs/P12_IMPLEMENTATION_REPORT.md` (this, verified)

No modification to: `Real*Provider`, `BaselineEngine`/`ComparisonEngine`, `polkit`, `fstab`, `zram`, `akonadi`, `mssql`, `nvidia 470xx`, `gui`, `packaging` - preserves invariants.

---

## 3. StateMachine Changes

**Before (P11):**
```
PROPOSED → PREVIEWED, CANCELLED
PREVIEWED → APPROVAL_REQUIRED, CANCELLED
APPROVAL_REQUIRED → APPROVED, CANCELLED
APPROVED → AUTHORIZATION_REQUIRED, CANCELLED
...
COMPLETED → (terminal)
```

**After (P12):**
```
PROPOSED → PREVIEWED, CANCELLED
PREVIEWED → APPROVAL_REQUIRED, FAILED, CANCELLED         // +FAILED
APPROVAL_REQUIRED → APPROVED, FAILED, CANCELLED          // +FAILED
APPROVED → AUTHORIZATION_REQUIRED, FAILED, CANCELLED    // +FAILED
AUTHORIZATION_REQUIRED → AUTHORIZED, FAILED, CANCELLED
AUTHORIZED → BACKUP_CREATED, FAILED, CANCELLED
BACKUP_CREATED → APPLYING, FAILED
...
FAILED → ROLLING_BACK, CANCELLED
COMPLETED/ROLLED_BACK/CANCELLED → (terminal, isTerminal()=true)
```

**Why:** Allows stale detection immediately after `PREVIEWED`/`APPROVAL_REQUIRED`/`APPROVED` to transition to safe `FAILED` without needing unnatural `AUTHORIZATION_REQUIRED` indirection. Still fail-closed: no new path weakens illegal examples. Helpers `isTerminal`/`isFailed` aid store logic.

**Verification:** `test_p12_statemachine` asserts `!isValidTransition(COMPLETED→APPLYING)`, `COMPLETED→APPROVED`, `FAILED→APPLYING`, `PREVIEWED→APPLYING`, `APPROVAL_REQUIRED→APPLYING`, `APPLYING→APPLYING`, plus additional `COMPLETED→CANCELLED`, `ROLLED_BACK→APPLYING` etc. - all remain false; valid happy-path and recovery remain true; new `APPROVED→FAILED` true.

---

## 4. Transaction Model Changes

**Added fields (all string/map, empty=not set, JSON persists minimal `beforeHash` etc.):**

- `beforeHash` (preview sha256)
- `approvedBeforeHash` (binding at approval)
- `beforeUnitHash`/`approvedUnitHash` (service enabled/active)
- `kernelVersion`/`approvedKernelVersion` (`uname -r`)
- `packageStateHash`/`approvedPackageStateHash` (sorted `rpm -qa` subset sha256)
- `approvedTarget`/`approvedOperation` (exact target/operation at approval)
- `preconditions`/`approvedPreconditions` (`map<string,string>` for `service.*.enabled/active`, `config.<path>.hash`, dependency)
- `idempotencyKey`/`appliedAt`/`completedAt`/`validationResult`

**Serialization:** `TransactionStore::persist` writes minimal JSON `id/state/target/operationId/beforeHash/approvedBeforeHash/kernelVersion/approvedKernelVersion/backupState/validationResult`; old JSON without these fields loads (constructor leaves empty) → backward compatible. `TransactionValidator` treats empty `approved*` as missing binding → `validation.failed.missing_approval_binding` fail-closed (never silently reinterpret old data). Documented in `TRANSACTION_MODEL.md`.

**No redesign:** Keeps `beforeState` raw for diff, `previews[]`, `beforeBaseline`/`comparison` optional, states `TxState`; no external dep, C++20, same include path.

---

## 5. Stale-Preview Protection

**Dimensions (deterministic, mockable via `CurrentState`):**

| Field | Source | Hash | Check |
|---|---|---|---|
| target | `tx.target` vs `approvedTarget` vs `cur.currentTarget` | exact string | `validateForApply` target |
| operation | `operationId` vs `approvedOperation` vs `cur.currentOperation` | exact | operation |
| beforeHash | `sha256(file content)` at preview vs approval vs current | `hashString` / `sha256File` | primary beforeHash |
| unitHash | `sha256(enabled\|active)` at preview/approval/current | `hashString` | unitHash where relevant (e.g., mssql) |
| kernelVersion | `uname -r` | exact | kernelVersion |
| packageStateHash | `hash(sorted rpm -qa subset)` | `hashString` | packageState (e.g., nvidia) |
| preconditions map | per-key snapshot (service enabled, config hash) | exact per key | `precondition:<key>` |
| TOCTOU symlink | `FileSafety::isSymlink` | - | `toctou.symlink` |
| TOCTOU canonical | `FileSafety::canonical` before vs after | exact | `toctou.canonical` |

**Implementation:** `TransactionValidator::validateForApply` checks in order: terminal state → state for apply → approvalState → missing binding → target → operation → beforeHash → unitHash (incl. unverifiable) → kernel → package → preconditions (missing/unverifiable) → TOCTOU. Each failure returns `ValidationResult{valid=false, reason, expected, observed, failingField, auditOperation="validation.failed.stale_..."}`. `finalPreconditionValidation` adds `backupState==CREATED` and re-reads file for TOCTOU after backup. Unverifiable → `unverifiable_*` fail-closed.

**Binding:** `bindApproval` copies `tx.target/operationId/beforeHash/etc.` or `cur` snapshot into `approved*` and sets `approvalState=APPROVED`, `idempotencyKey=id`. Approval cannot be reused across different target/operation/hash/kernel/package - must `preview→approve` again, no auto-refresh.

**Tests:** `test_p12_stale` covers all dims + package + precondition + final failure + no mutation + TOCTOU; each mutates fixture after approval then `store.apply` asserts `!valid`, `failingField` matches, file content unchanged (`hashStr` before==after), audit contains stale.

---

## 6. Idempotency Behavior

| Operation | Semantics | Enforcement |
|---|---|---|
| `create` duplicate id | Rejected deterministic `transaction.create.rejected.duplicate`, no overwrite, audit duplicate | `TransactionStore::create` checks `exists` (in-memory + file) |
| `approve` duplicate same binding | Idempotent `approval.duplicate.already_approved` (same hashes), no second execution; if stale after, not considered duplicate (stale check fails) | `approve` checks `approvalState==APPROVED` + hashes + `validateForApply` |
| `apply` on `COMPLETED` | Rejected `apply.rejected.already_completed`, `applied=false`, no mutation, file unchanged, audit explains | `apply` checks `state==COMPLETED` before validation |
| `apply` on `APPLIED/VERIFYING/VERIFIED` | Rejected `already_applied` | same |
| `verify` repeated | Idempotent `verify.idempotent.already_verified`, returns same state without re-apply, no file read mutation | `verify` checks `COMPLETED/VERIFIED` first |
| `rollback` conditional | `FAILED→ROLLING_BACK→ROLLED_BACK` valid; already `ROLLED_BACK` idempotent | `StateMachine` |

Never fakes idempotency by ignoring errors - `StateMachine::validateTransition` throws `logic_error` on illegal, store returns structured `ValidationResult` with `auditOperation`.

**Tests:** `test_p12_idempotency` (5): approval bound to id, mismatch rejected, duplicate id deterministic + audit, COMPLETED cannot re-apply + no mutation, repeated verify idempotent, duplicate approval idempotent.

---

## 7. Approval Binding

- Bound to exact `id`, `target`, `operation`, `beforeHash`, `unitHash`, `kernelVersion`, `packageStateHash`, `preconditions` - all snapshot at `approve`.
- Transfer attempt `validateApprovalBinding(txA, txB.id)` → `validation.failed.approval_mismatch` with `expected=tx.id` `observed=otherId`.
- After approval, `beforeHash` drift → `stale_beforeHash` with `expected=approvedBeforeHash` `observed=currentBeforeHash`.
- Same for `target`/`operation`/kernel/package/precondition - each fails closed with field name.
- Second `approve(same id, same cur)` → duplicate idempotent, not second execution; `store.approve` returns `already_approved` audit, does not create new `BackupEngine` or `apply`.

---

## 8. Backup Boundary

```
approve → store.approve (bind)
  → store.apply: first validateForApply (before backup) → if stale → FAILED audit, no backup
  → BackupEngine::create(tx.id, filePath or target) versioned SHA256, no overwrite, only if first validation passed; sets backupState=CREATED, state → BACKUP_CREATED
  → finalPreconditionValidation (re-reads file hash + canonical, checks backupState==CREATED, same stale checks) → if stale → FAILED audit, no apply, backup retained
  → StateMachine BACKUP_CREATED→APPLYING valid → APPLYING → atomicWrite(target, afterState) → APPLIED→VERIFYING→VERIFIED→COMPLETED
```

If final fails: `applied=false`, `backupCreated=true`, `approvalValid=false`, state `FAILED`, backup retained for rollback.

**Tests:** `test_final_precondition_failure_blocks_apply` (disk stale after preview, first cur valid, final re-read fails, file stays stale, backup retained) and `test_no_mutation_when_validation_fails` (hash compare before/after).

---

## 9. Audit Changes

- `AuditLog::append` now `fsync` per event (open RO + `fsync` after flush) - crash-safe hash chain.
- `AuditEvent.error` now structured `reason + expected=... observed=... field=... applied=true/false backupCreated=true/false approvalValid=true/false` for every stale/idempotency.
- Operations: `transaction.created`, `transaction.approved`, `approval.duplicate.already_approved`, `validation.failed.stale_target`, `stale_operation`, `stale_beforeHash`, `stale_unitHash`, `stale_kernelVersion`, `stale_packageState`, `stale_precondition`, `toctou.symlink/canonical`, `validation.failed.missing_approval_binding`, `backup.created/failed`, `apply.started/completed/failed`, `apply.rejected.already_completed/duplicate/not_found`, `verify.idempotent.already_verified`, `transaction.create.rejected.duplicate`, `validation.failed.final_*`.
- Preserves `previousHash` chaining (`hashEvent` SHA256 of timestamp+id+operation+user+approval+auth+previousHash) and `eventHash`; old parser (reads `transactionId`+`eventHash`) ignores extra fields - backward compatible.
- Tests assert audit `list(id)` contains `expected=`, `observed=`, `applied=false`, `backupCreated=`, `approvalValid=`, `stale` or `already_completed`/`duplicate`.

---

## 10. Tests and Exact Results

**New tests (P12, fixtures `/tmp/polaris-test-root` only):**

- `test_p12_stale` - 10 categories + TOCTOU + final: valid proceeds, stale beforeHash, unitHash, kernel, target, operation, packageState, precondition, final blocks apply, no mutation, TOCTOU symlink
- `test_p12_idempotency` - 5 categories: approval bound, mismatch, duplicate id, COMPLETED cannot re-apply, repeated verify, duplicate approval
- `test_p12_statemachine` - illegal transitions (11) + valid recovery (FAILED→ROLLING_BACK, APPROVED→FAILED) + terminal + backup_created
- `test_p12_transaction_model` - backward compat (old JSON loads, missing binding fail-closed, bind produces valid), audit stale (expected/observed/applied), audit idempotency/duplicate

**Total:** `ctest 13/13 0.10-0.11s 100%`

```
1/13 unit                      Passed 0.00s
2/13 real_providers            Passed 0.04s
3/13 parsers                   Passed 0.00s
4/13 readonly                  Passed 0.00s
5/13 p4_security               Passed 0.01s (9 checks)
6/13 comparison                Passed 0.00s (12 cats)
7/13 post_change               Passed 0.00s
8/13 regression                Passed 0.00s
9/13 observed_benefit          Passed 0.00s
10/13 p12_stale                Passed 0.01s
11/13 p12_idempotency          Passed 0.01s
12/13 p12_statemachine         Passed 0.00s
13/13 p12_transaction_model    Passed 0.01s
```

Existing security/read-only tests still pass (`p4_security` 9/9, `readonly` mtime unchanged, `comparison` 12/12). No test weakened/deleted.

**TOCTOU security test:** `p12_stale` `TOCTOU symlink rejected` - preview regular file, replace with symlink to `/etc/passwd`, `apply` detects `isSymlink` → `toctou.symlink` audit, no write, file restored.

**Idempotency security:** `p12_idempotency` `COMPLETED cannot re-apply` - file hash before re-apply == after (no mutation), audit `already_completed`.

**Final precondition security:** `p12_stale` `final precondition failure` - backup created then final re-read stale → no apply, backup retained.

---

## 11. Security Verification

- No `sh -c`, no shell concat, no password collection - preserved (grep `sh -c` 0, `SUDO_ASKPASS` 0).
- `FileSafety::validatePath` still rejects `..`, `;|&`, `` ` ``, `$`, `NUL`, `>4096`, symlink, non-allowlist - not weakened.
- `StateMachine` still throws `logic_error` on illegal → fail-closed.
- `BackupEngine` still `no overwrite` (`exists` → throw), `sha256File`, `is_regular_file` - preserved.
- `ReadOnlyGuard` `kReadOnlyMode true` intact - no real-host mutation during discovery (verified `stat /etc/fstab` mtime unchanged before/after tests, `systemctl is-enabled mssql` still `disabled`, `ls /run/polaris/helper.sock` not exists, `extra/nvidia-470xx` unchanged).
- `AuditLog` hash chain verified (`test_p4_security` audit hash chain PASS + P12 audit stale/idempotency).

---

## 12. Host-Modification Verification

P12 is engineering phase - **no real-host mutation**:

- Checked before/after: `stat /etc/fstab` mtime `2026-08-31 21:19` unchanged
- `systemctl is-enabled mssql-server` `disabled` (P6) unchanged
- `systemctl is-enabled bluetooth` still `enabled` (not modified)
- `ls /lib/modules/*/extra/nvidia-470xx/nvidia.ko.xz` still 25M 470.256.02
- `akonadictl status` still running (not disabled)
- No `dnf`, `systemctl disable/enable`, `akmods`, `dracut`, `modprobe`, GRUB, KDE, fstab, zram, reboot - grep `dnf ` `systemctl ` in test logs 0 (except read-only `is-enabled` checks in fixtures, not host)
- Tests only write to `/tmp/polaris-test-root` (verified `ls /tmp/polaris-test-root` contains `p12_*` fixtures, transactions, backups, audit.log)

---

## 13. Known Limitations / Not Implemented

- **Generic preconditions:** `TransactionStore` uses `CurrentState.currentPreconditions` map; real-host collection of `service.enabled/active`, `config.hash`, dependency state is mocked in tests - real `RealSystemdProvider`/`Real*` integration not yet wired to `CurrentState` (would require `systemctl is-enabled` parsing + `rpm -qa` hashing; left for P13/P14 helper).
- **Lock `flock`:** Plan mentions `/run/polaris/transaction.lock` concurrent control; P12 implements deterministic duplicate-id rejection via file existence but not `flock` file lock - concurrent `apply` from two processes not yet tested with `flock`; acceptable for single-process store, but multi-process race still possible (future P14).
- **Crash recovery `recover`:** Plan mentions `polaris transaction recover` for `BACKUP_CREATED`/`APPLYING` incomplete; P12 added `FAILED` transitions for stale but not automatic `recover` command - state persists to `~/.local/state/polaris/transactions/*.json`, manual re-validation required.
- **IPC `SO_PEERCRED`:** Helper not yet installed (`/run/polaris/helper.sock` not exists) - correct for P11/P12 (engineering, no helper install); only `FileSafety`/`AuditLog` hardening done.
- **Package state scope:** `packageStateHash` is generic hash of relevant subset; not yet scoping to per-operation (e.g., `xorg-x11-drv-nvidia-libs` for NVIDIA) - mocked as whole string hash in tests.
- **Kernel version source:** Mocked `uname -r` string; real `uname` integration not yet in `BaselineEngine` (could be added via `RealOsProvider` kernel).

All limitations are documented and do not weaken fail-closed guarantees - unverifiable → fail-closed.

---

## 14. Build Result

```
cmake -S . -B /tmp/polaris_p12_build --fresh → Configuring done, Generating done
cmake --build /tmp/polaris_p12_build → 100% Built polaris, polaris_real, polaris_tests, test_real_providers, test_parsers, test_readonly, polaris_p3, polaris_p4, polaris_p5, test_comparison, test_post_change, test_regression, test_observed_benefit, test_p12_stale, test_p12_idempotency, test_p12_statemachine, test_p12_transaction_model
ctest → 13/13 100% 0.11s
```

No warnings treated as errors except deprecated `SHA256_*` (Wno-error=deprecated-declarations already).

---

## 15. Compatibility Impact

- **Backward compatible JSON:** Old `~/.local/state/polaris/transactions/*.json` (P4/P7/P11) still loads (empty hardening fields → skip, but `apply` will be rejected until new preview with binding). No silent reinterpretation.
- **Forward compatible audit:** Old audit parser (`list` reads `transactionId`+`eventHash`) ignores new `expected/observed` fields; new audit writer preserves `previousHash` chain.
- **API:** No breaking `core` include changes; new headers are additive, same `polaris::safety` namespace, C++20.
- **No reboot, no helper install** - safe to deploy on host without migration.

---

## 16. Next Phase

**P13 - User Workflow/Profile Engine** (as per `ROADMAP.md` ranked). P12 is dependency for P13 (profile `usesKMail` etc. must be validated on fresh preview, not stale). Do NOT implement P13 now - STOP after P12 hardening, await explicit P13 prompt.

---

## 17. Verification Commands

```
rm -rf /tmp/polaris_p12_build && cmake -S ~/Documents/lin-opt -B /tmp/polaris_p12_build --fresh && cmake --build /tmp/polaris_p12_build && ctest --test-dir /tmp/polaris_p12_build --output-on-failure
stat /etc/fstab  # mtime unchanged 2026-08-31 21:19
systemctl is-enabled mssql-server  # disabled
systemctl --failed  # 0
ls /run/polaris/helper.sock  # not exists
ls -R /tmp/polaris-test-root | head
```

---

*No real-host optimization was performed during P12. No reboot occurred. No unrelated project area was modified.*

