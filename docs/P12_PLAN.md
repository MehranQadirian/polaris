# P12 - Transaction/State-Machine Hardening (Stale-Preview + Idempotency) - Plan

**Phase:** P12 - Engineering, Not Host Optimization  
**Mode:** READ-ONLY PLANNING + IMPLEMENTATION - no `dnf`, no `systemctl`, no `modprobe`, no `dracut`, no `sudo`, no reboot, no Akonadi/mssql/fstab/zram mutation - only `core/` hardening, `tests/` fixtures in `/tmp/polaris-test-root`, `docs/` updates  
**Date:** 2026-09-01 03:00 +0330  
**Source:** Audit of `core/safety/transaction/Transaction.h`, `StateMachine.h`, `FileSafety.h`, `BackupEngine.*`, `AuditLog.*`, `cli/p4_cli.cpp`, `core/domain/Comparison.h` and P11 baseline  
**Dependency:** P11 `Comparison` (COMPLETED) - P12 adds idle-preview defense on top of post-change measurement

---

## 1. What P11 Left Unprotected

P11 extended `Transaction` with `beforeBaseline/afterBaseline/comparison` and introduced `ComparisonEngine` purity, but `Transaction` still stores only:

- `id`, `target`, `operationId`, `beforeState`, `afterState`, `previews[]`, states (`state`, `approvalState` etc.), `beforeBaseline` optional

**Missing hardening (verified via `cat core/safety/transaction/*`):**

- No hash-bound approval - `beforeState` is raw content, not `beforeHash` + `approvedBeforeHash`; mutating file after `PREVIEWED` does not invalidate `APPROVED` deterministically.
- No `unitHash`/`kernelVersion`/`packageState` snapshot - `mssql` disable and `nvidia` swap rely on package/unit state, but transaction does not snapshot `is-enabled/is-active` or `rpm -qa` or `uname -r`.
- No `approvedTarget/approvedOperation` binding - approval currently keyed only by `transactionId` string in `cli/p4_cli.cpp:approve`, not validated against `target`/`operationId` drift.
- No idempotency key - second `create` with same `TX-TEST-...` silently writes new file `/tmp/polaris-test-root/transactions/<id>.json` (overwrite risk); `COMPLETED → APPLYING` blocked by `StateMachine`, but `COMPLETED.apply()` path is not explicitly guarded with audit reason.
- `StateMachine` is fail-closed for 16 states but has no stale-aware transition (`APPROVED → FAILED` on stale not in table); final precondition validation before `APPLYING` is not enforced by transaction lifecycle code - `TransactionManager.cpp` is stub (`create` returns `{}`).
- `AuditLog::append` does not log stale/idempotency rejection with expected/observed delta; `FileSafety::atomicWrite` checks symlink at write time but no explicit TOCTOU re-validation after `BACKUP_CREATED` before `APPLYING`.

P10 gap analysis flagged stale-preview as **High** priority (row 6) and idempotency as **Medium** - P12 closes both before P13 `UserProfile` (which would otherwise write profile on stale preview) and P14 `helper` (which must trust only fresh previews).

---

## 2. P12 Objectives (Fail-Closed)

1. **Stale-preview protection** - if anything material changes between `PREVIEWED → APPROVAL_REQUIRED → APPROVED → AUTHORIZATION_REQUIRED → AUTHORIZED → BACKUP_CREATED → APPLYING`, reject without mutation, require new preview.
2. **Idempotency** - `COMPLETED` cannot re-apply; `VERIFY` is idempotent; duplicate `transactionId` is deterministic `ALREADY_EXISTS` error, not overwrite; duplicate `approve` on same id does not create second execution.
3. **Validation before mutation** - `APPROVAL → PRECONDITION VALIDATION → BACKUP → FINAL VALIDATION → APPLY` with audit at each gate.
4. **Preserve invariants** - `READ→...→VERIFY` pipeline, `no batch`, `no launch==approval`, `exact id`, `beforeHash`, `unitHash`, `backup-before-mutate`, `rollback`, fail-closed StateMachine, FileSafety, no shell/password/autoreboot/real-host mutation during discovery.

Fail-closed: if a required precondition cannot be verified, **reject** and emit `validation.failed` audit event; never silently continue.

---

## 3. Transaction Data Model Extension (Backward Compatible)

Extend `core/safety/transaction/Transaction.h` **only** with fields needed for deterministic stale/idempotency checks, using existing types and `std::optional` / empty-default for compat (old JSON without these fields still loads, `has_value()==false` → skip check, documented migration).

```cpp
struct Transaction {
  // existing: id, operationId, target, description, riskLevel, expectedBenefit,
  // observedBenefit, requiredPrivileges, state, approvalState, authorizationState,
  // backupState, executionState, verificationState, rollbackState, beforeState,
  // afterState, previews, evidence, rollbackPlan, rebootRequired, status, error,
  // auditReference, timestamp, previousHash/eventHash, beforeBaseline/afterBaseline/comparison

  // P12 hardening - stale-preview (all binding snapshots at preview + approved snapshots at approval)
  std::string beforeHash;                // sha256(beforeState) at preview
  std::string approvedBeforeHash;        // sha256 at approval (binding)
  std::string beforeUnitHash;            // hash(enabled|active|package state) at preview
  std::string approvedUnitHash;          // at approval
  std::string kernelVersion;             // uname -r at preview
  std::string approvedKernelVersion;     // at approval
  std::string packageStateHash;          // hash of relevant package list at preview
  std::string approvedPackageStateHash;  // at approval
  std::string approvedTarget;            // target copy at approval
  std::string approvedOperation;         // operationId copy at approval

  // Generic preconditions: service enabled/active, config hash, dependency state
  std::map<std::string,std::string> preconditions;          // key→snapshot at preview
  std::map<std::string,std::string> approvedPreconditions;  // at approval

  // Idempotency
  std::string idempotencyKey; // == id, explicit
  std::string appliedAt;      // ISO8601 when APPLY succeeded
  std::string completedAt;    // ISO8601 when COMPLETED
  std::string validationResult; // last stale/idempotency rejection reason (also in audit)

  // Helpers: helper to compute hashes, snapshot helpers
};
```

- Keep `beforeState` raw for diff display; add `beforeHash` for machine check (stronger than raw compare, handles whitespace).
- `approved*` fields are set by `approve()` and compared by `validateForApply()` - approval cannot be reused across different target/operation/hash/kernel.
- `preconditions` map covers `service.<unit>.enabled`, `service.<unit>.active`, `config.<path>.hash`, dependency state etc. - only those represented by transaction; not inventing unrelated keys.
- `packageStateHash` is hash of sorted `rpm -qa` subset relevant to operation (e.g., `akmod-nvidia*` for NVIDIA); stored as hex sha256, deterministic, mocked in tests.
- All new fields default `""` / empty map → backward compatible parsing returns `has_value()==false` for old JSON → validation skips that dimension (documented: old JSON cannot be applied without new preview; loading is allowed, applying is rejected until re-previewed).
- Migration note docs: old P4/P7 JSON loads, but any `apply()` without `approvedBeforeHash` set is rejected with `stale: missing approval binding - new preview required`.

Do **not** add fields for appearance; no new GUI deps; stay C++20; same `core` include path.

---

## 4. Approval Binding

Approval MUST be bound to **exact** transaction identity:

- `approvalTransactionId == transaction.id`
- `approvedTarget == current target` (bytes equal)
- `approvedOperation == current operationId`
- `approvedBeforeHash == current beforeHash` (file content hash)
- `approvedUnitHash == current unitHash` (where applicable)
- `approvedKernelVersion == current kernelVersion` (where present)
- `approvedPackageStateHash == current packageStateHash` (where present)
- `approvedPreconditions[k] == current preconditions[k]` for each k

If any mismatches or stale, `validateApprovalBinding()` returns `{valid=false, reason="stale <field> expected <hash> observed <hash>"}`; transaction must transition to safe failure (`FAILED`/`CANCELLED` audit), **no host mutation**, **no apply command**, require new `preview → approve` cycle; no auto-refresh.

Duplicate approval on same id: second `approve("TX-...")` while already `APPROVED` must be idempotent - return `{alreadyApproved=true, deterministic}`; must **not** create second execution or second audit `transaction.approved` with new hash; audit may emit `approval.duplicate` with `already completed` note.

---

## 5. Stale-Preview Protection

Investigate and protect (deterministic, mockable):

| Stale dimension | How observed | Hashing | When checked |
|---|---|---|---|
| `target` drift | transaction target path changed since preview | exact string compare `target != approvedTarget` | `approved → applying` |
| `operation` drift | `operationId` changed | exact compare | same gate |
| `beforeHash` | file content sha256 changed (`BackupEngine::sha256File` or in-memory hash) | `sha256(beforeState)` | primary - before backup and final before apply |
| `unitHash` | `systemctl is-enabled/is-active` tuple hash `sha256(enabled+\|+\active)` | `beforeUnitHash` | where transaction targets unit (mssql, service disable) |
| `kernelVersion` | `uname -r` string | exact compare `kernelVersion != approvedKernelVersion` | before apply (package builds depend on kernel) |
| `packageState` | subset of `rpm -qa` or `dnf list` sorted + sha256 | `packageStateHash` | before apply |
| `config state` | `sha256` of relevant config file (`/etc/fstab`, `/etc/default/grub`) | per-file hash in `preconditions["config.<path>.hash"]` | before apply |
| `service enabled/active` | per-unit `enabled/active` in map | `preconditions["service.<unit>.enabled"]` etc. | before apply |

Add `TransactionValidator` (`core/safety/transaction/TransactionValidator.h/.cpp`):

```cpp
struct CurrentState {
  std::string currentBeforeHash;
  std::string currentUnitHash;
  std::string currentKernelVersion;
  std::string currentPackageStateHash;
  std::string currentTarget;
  std::string currentOperation;
  std::map<std::string,std::string> currentPreconditions;
  // optional file path for TOCTOU re-validation
  std::string filePath; // for canonical/symlink check
};

struct ValidationResult { bool valid; std::string reason, expected, observed; std::string failingField; };

class TransactionValidator {
  static ValidationResult validateForApply(const Transaction&, const CurrentState&);
  static ValidationResult validateApprovalBinding(const Transaction&, const std::string& approvalId, const CurrentState& atApproval);
  static bool isStale(const Transaction&, const CurrentState&);
  static ValidationResult finalPreconditionValidation(const Transaction&, const CurrentState& afterBackup);
};
```

- Pure, deterministic, side-effect-free; providers supply `CurrentState` (real host or fixture mock).
- If `approvedBeforeHash` empty and `currentBeforeHash` non-empty → `valid=false reason="missing approval binding"` - fail closed (forces re-preview, not silent).
- If any `approved*` present but `current*` cannot be observed (e.g., file deleted, `stat` fails) → `valid=false reason="unverifiable precondition: <field>"` - fail closed.
- Explicit `expected vs observed` strings for audit.

TOCTOU: `FileSafety` `canonical` before + after; `TransactionValidator` checks `isSymlink(filePath)` and `canonical` equality immediately before `apply`; if mismatch, stale.

---

## 6. Idempotency Semantics

| Operation | Idempotent? | Repeated invocation result |
|---|---|---|
| `preview` | yes (new id each time) - reading, no mutation |
| `approve <id>` | idempotent - second approve on same id in `APPROVED`/`APPROVED→AUTHORIZATION_REQUIRED` returns `already approved`, no new execution, audit `approval.duplicate` deterministic |
| `apply` on `COMPLETED` | **rejected, idempotent no-mutation** - `COMPLETED → APPLYING` invalid in `StateMachine`; `TransactionStore::apply` returns `rejected: already completed` + audit `apply.rejected.already_completed` |
| `verify` | idempotent - `VERIFYING→VERIFIED→COMPLETED` is one-way; repeated `verify` on `COMPLETED`/`VERIFIED` returns same `verificationState` without re-reading host or mutating |
| `create` with duplicate `transactionId` | **rejected deterministic** - `TransactionStore::create` refuses overwrite, returns `ALREADY_EXISTS`, audit `transaction.create.rejected.duplicate` |
| `rollback` | conditional - if `rollbackState==AVAILABLE` then `ROLLING_BACK→ROLLED_BACK`, else `already rolled back` idempotent |

Do **not** fake idempotency by ignoring errors - `StateMachine::validateTransition` enforces lifecycle; `TransactionStore` adds idempotency checks before mutation; audit records contain `idempotencyKey == transactionId` and `appliedAt/completedAt`.

---

## 7. State-Machine Hardening

Preserve fail-closed behavior; add only transitions needed for safe failure after stale/idempotency.

Existing table (verify in code):

```
PROPOSED → PREVIEWED, CANCELLED
PREVIEWED → APPROVAL_REQUIRED, CANCELLED
APPROVAL_REQUIRED → APPROVED, CANCELLED
APPROVED → AUTHORIZATION_REQUIRED, CANCELLED
AUTHORIZATION_REQUIRED → AUTHORIZED, FAILED, CANCELLED
AUTHORIZED → BACKUP_CREATED, FAILED, CANCELLED
BACKUP_CREATED → APPLYING, FAILED
APPLYING → APPLIED, FAILED
APPLIED → VERIFYING, FAILED
VERIFYING → VERIFIED, FAILED
VERIFIED → COMPLETED, FAILED
FAILED → ROLLING_BACK, CANCELLED
ROLLING_BACK → ROLLED_BACK, FAILED
COMPLETED/ROLLED_BACK/CANCELLED → (terminal)
```

**Hardening delta** (minimal):

- Allow staling failure from approval side without weakening: add `APPROVED → FAILED` (stale after approval, before authorization) - needed for `APPROVAL → PRECONDITION VALIDATION` failure; currently `APPROVED` cannot `FAILED` and would need unnatural `AUTHORIZATION_REQUIRED → FAILED` indirection. Add `APPROVED → FAILED`, `PREVIEWED → FAILED` (optional for early stale) and `APPROVAL_REQUIRED → FAILED` (pre-approval stale) only if validated; otherwise keep existing fail-closed and enforce via store before invoking `StateMachine` (so `StateMachine` itself need not change). To demonstrate hardening, we extend `StateMachine` to include `APPROVED → FAILED` as valid, keep all illegal examples rejected (`COMPLETED→APPLYING`, `COMPLETED→APPROVED`, `PREVIEWED→APPLYING`, `APPROVAL_REQUIRED→APPLYING`, `BACKUP_CREATED→APPLYING without validation` is not a SM transition but a store guard, `APPLYING→APPLYING`).
- Add helpers: `bool isTerminal(TxState)`, `bool isValidIdempotentTransition(TxState from, TxState to)` - same table.
- Keep `validateTransition` throwing `logic_error` with `rejected, fail closed` message.

Validation guards:

- `TransactionStore::canApply` checks `StateMachine::isValidTransition(current, APPLYING)` **and** `TransactionValidator::validateForApply` **and** `backupState == CREATED`.
- `TransactionStore::canVerify` checks `StateMachine` and idempotency.

Illegal transitions test matrix (must remain impossible, assert `!isValidTransition`):

- `COMPLETED → APPLYING`, `COMPLETED → APPROVED`, `FAILED → APPLYING`, `PREVIEWED → APPLYING`, `APPROVAL_REQUIRED → APPLYING`, `APPLYING → APPLYING`, `COMPLETED → CANCELLED`, `ROLLED_BACK → APPLYING`, `CANCELLED → APPLYING`, plus `APPLIED → APPROVED`, `VERIFYING → APPLYING`.

Valid recovery remains:

- `FAILED → ROLLING_BACK → ROLLED_BACK`, `FAILED → CANCELLED`, `APPLIED → VERIFYING` etc. - unchanged; plus new stale paths `APPROVED → FAILED → CANCELLED` / `APPROVED → FAILED → ROLLING_BACK`.

---

## 8. Backup Boundary

Hardened flow:

```
preview → approval (records approved* hashes)
  → PRECONDITION VALIDATION (TransactionValidator::validateForApply against currentState; if stale → FAILED audit, no backup)
  → BACKUP (BackupEngine::create; if fails → FAILED, no apply)
  → FINAL PRECONDITION VALIDATION (re-read currentState after backup, FileSafety canonical/symlink re-check; if stale → FAILED audit, no apply, no partial mutate, backup retained for rollback)
  → APPLY (only if both validations pass, state is BACKUP_CREATED, isValidTransition BACKUP_CREATED→APPLYING true)
  → VERIFY
```

- `BackupEngine` unchanged except ensuring `create` is called only after first validation, and `restore` path unchanged.
- If final validation fails: `StateMachine::validateTransition(BACKUP_CREATED, FAILED)` then audit `validation.failed.final` + `backup.retained`; state `FAILED`, never `APPLIED`.
- Tests verify `stat` / file content not mutated when final validation fails (fixture `/tmp/polaris-test-root` content unchanged).

---

## 9. Auditability

Every stale/idempotency rejection emits structured audit event via `AuditLog::append` with hash chain:

```json
{"timestamp":"<ISO>","transactionId":"TX-P12-001","operation":"validation.failed.stale_beforeHash","previousHash":"<prev>","eventHash":"<sha>","expected":"<beforeHash>","observed":"<curHash>","reason":"beforeHash mismatch","applied":false,"backupCreated":false,"approvalValid":false}
{"operation":"apply.rejected.already_completed", "reason":"COMPLETED cannot re-apply", "applied":false}
{"operation":"transaction.create.rejected.duplicate", "reason":"duplicate id TX-... already exists"}
```

Must answer:

- which transaction, expected vs observed, failing field, was anything applied (no), backup created (before/after), approval still valid (no if stale), rejected because already completed (explicit field).

Extend `AuditEvent` struct with `expectedState`, `observedState`, `failingField`, `applied` bool in serialized JSON (backward compat: extra fields ignored by old parser which only reads `transactionId`+`eventHash`). Preserve `previousHash` chaining and `eventHash = sha256(timestamp+id+operation+user+approval+auth+previousHash)`.

No secrets/passwords logged; hash only.

---

## 10. Security - TOCTOU & Bounded

- **TOCTOU:** check `FileSafety::isSymlink` + `canonical` at preview, after backup, and final before apply; if `canonical` drifts or becomes symlink, reject.
- **Duplicate execution:** `TransactionStore` holds in-memory map + file persistence under `/tmp/polaris-test-root/transactions/<id>.json` (fixtures) or `~/.local/state/polaris/transactions/` (real); `create` uses `exists` guard + `BackupEngine::create` no-overwrite pattern.
- **State-machine tampering:** `Transaction::state` is enum, not raw string; `StateMachine::validateTransition` throws `logic_error` on tamper.
- No `sh -c`, no password collection - preserved.

---

## 11. Implementation Files (Minimal Change Set)

- **Extend:** `core/safety/transaction/Transaction.h` - add hardening fields, serialization helpers (`toJson`/`fromJson` with optional), backward compat.
- **Extend:** `core/safety/transaction/StateMachine.h` - add `APPROVED→FAILED` (and maybe `isTerminal`), keep fail-closed, add doc comments.
- **New:** `core/safety/transaction/TransactionValidator.h/.cpp` - pure stale/approval/idempotency checks, currentState snapshot.
- **New:** `core/safety/transaction/TransactionStore.h/.cpp` - registry, duplicate guard, state-machine + validator gating, backup boundary, audit emission, idempotency, file-backed persistence helpers for tests.
- **Touch:** `core/safety/audit/AuditLog.h/.cpp` - add optional fields for audit of stale/idempotency (`expected`/`observed` in JSON), keep `hashEvent` chaining.
- **Touch:** `core/safety/backup/BackupEngine.*` - no core change, just usage in store (validate order).
- **Touch:** `core/safety/FileSafety.h` - add `revalidate` helper for TOCTOU (canonical compare).
- **CLI:** optional `cli/p4_cli.cpp` expose `validate`/`apply-hardened` dry-run for manual demo - but not required; keep `p4_cli` stable, add `cmd_validate` for `polaris_p4 transaction validate <id>`.
- **Tests:** new `tests/security/test_p12_stale_preview.cpp`, `tests/security/test_p12_idempotency.cpp`, `tests/security/test_p12_statemachine.cpp` combined or split; plus `tests/unit/test_transaction_hardening.cpp` for model/compat; plus integration `tests/integration/test_p12_audit.cpp`.

Keep `core/domain/Transaction.h` as-is (separate domain model); hardening lives in `safety::Transaction` which is the gate.

---

## 12. Testing (18+ Cases, Fixtures in `/tmp`)

Use existing `CMake` + `ctest` arch, `/tmp/polaris-test-root` fixtures, no real-host mutation.

| # | Spec case | Test file | How |
|---|---|---|---|
| 1 | valid transaction proceeds | `test_p12_stale_preview` | preview hash == current, approve, backup, final validate pass → apply succeeds, verify |
| 2 | stale beforeHash rejected | same | preview, mutate file (`echo stale > f`), current hash differs → `validateForApply` false, audit `validation.failed.stale_beforeHash`, no mutate |
| 3 | stale unitHash rejected | same | `beforeUnitHash` `enabled:active` vs `disabled:inactive` → reject |
| 4 | stale kernel version rejected | same | `kernel 7.1.10-200` vs `7.1.11-300` → reject |
| 5 | stale target rejected | same | `target /tmp/.../fstab` vs `/tmp/.../test.conf` → reject |
| 6 | stale operation rejected | same | `operation fstab-stale-swap` vs `other` → reject |
| 7 | approval bound to transaction ID | `test_p12_idempotency` | `approve TX-A` then `apply TX-B` with same approval id → reject `approval mismatch` |
| 8 | approval mismatch rejected | same | as above plus hash mismatch audit |
| 9 | duplicate tx ID rejected | same | `store.create TX-DUP` twice → second `ALREADY_EXISTS`, audit `duplicate` |
| 10 | COMPLETED cannot re-apply | same | `COMPLETED → canApply` false, audit `already_completed`, file not mutated twice |
| 11 | repeated verification does not apply | same | `VERIFIED → verify` idempotent, no `APPLYING` transition |
| 12 | illegal StateMachine transitions rejected | `test_p12_statemachine` | exhaustive `isValidTransition` checks for `COMPLETED→APPLYING` etc., expect `logic_error` |
| 13 | valid recovery remains | same | `FAILED→ROLLING_BACK→ROLLED_BACK`, `BACKUP_CREATED→APPLYING` after validation |
| 14 | final precondition failure blocks APPLY | `test_p12_stale_preview` | mutate after backup but before apply → final validate fails, file unchanged, backup retained |
| 15 | audit event for stale rejection | `test_p12_audit` | after stale rejection, `AuditLog::list(id)` contains `validation.failed` with expected/observed |
| 16 | audit event for idempotency rejection | same | after duplicate/completed rejection, audit contains `already_completed` |
| 17 | backward-compatible JSON parsing | `test_transaction_hardening` | parse old `{"id":"TX-OLD","state":"PREVIEWED","target":...}` without new fields → loads, new fields default, no crash |
| 18 | no mutation when validation fails | `test_p12_stale_preview` | compare file `sha256` before/after rejected apply → identical |
| + | TOCTOU | `test_p12_stale_preview` | preview regular file → replace with symlink → `FileSafety::isSymlink` true → reject, audit `toctou.symlink` |

Each test constructs `Transaction` in memory or via `TransactionStore` file in `/tmp/polaris-test-root/transactions`, mocks `CurrentState` without `dnf`/`systemctl`.

Do **not** delete/weaken existing tests - `test_p4_security` 9 checks, `test_comparison` 12, etc. must still pass.

---

## 13. Build & Validation

After impl:

```
rm -rf /tmp/polaris_p12_build && cmake -S . -B /tmp/polaris_p12_build --fresh && cmake --build /tmp/polaris_p12_build && ctest --test-dir /tmp/polaris_p12_build --output-on-failure
```

Expect `ctest 12+` (existing 9 + 3 P12 suites) 100% pass. `stat /etc/fstab` mtime unchanged, `is-enabled mssql disabled` unchanged, no helper sock.

Docs:

- create `docs/P12_IMPLEMENTATION_REPORT.md`
- update `docs/ROADMAP.md` P12 → COMPLETE
- update `docs/PROJECT_STATE.json` `currentPhase=P12` `nextPhase=P13`
- update `docs/PROJECT_HANDOFF.md` with P12 section + verification table
- keep `docs/TRANSACTION_MODEL.md` extended with hardening fields + state diagram + approval binding

---

## 14. Acceptance Criteria (P12)

Must satisfy all in spec - same checklist as prompt §ACCEPTANCE CRITERIA (stale hashes, kernel/package, approval bound, duplicate deterministic, COMPLETED blocked, StateMachine fail-closed, illegal rejected, final validation before APPLY, zero mutation on fail, audit explain, rollback intact, existing tests pass, new tests pass, clean build, docs accurate, no real-host opt/reboot/unrelated).

---

## 15. Not In Scope

P13 UserProfile, P14 IPC Helper D-Bus, P15 CI expansion beyond P12 needs, P16 HCI, P17 Campaign2, P18 Final Benchmark - all future.

Stop after P12 hardening; no real-host optimization.

