# Transaction Model - Polaris P12 (Hardened)

See `core/safety/transaction/Transaction.h:1`, `TransactionValidator.h:1`, `TransactionStore.h:1` and `StateMachine.h:1`.

## Fields

transactionId `TX-2026-000001`, timestamp, operationId, target, description, riskLevel R0-3, expectedBenefit, **observedBenefit** (P11), requiredPrivileges (polkit action), approvalState, authorizationState, backupState, executionState, verificationState, rollbackState, beforeState, afterState, previews `ChangePreview[]`, evidence, rollbackPlan, rebootRequired, status, error, auditReference, previousHash/eventHash, **beforeBaseline** (P11, optional), **afterBaseline** (P11, optional), **comparison** (P11, optional), **P12 hardening**: `beforeHash` (sha256 at preview), `approvedBeforeHash` (at approval, binding), `beforeUnitHash`/`approvedUnitHash` (service enabled/active hash), `kernelVersion`/`approvedKernelVersion` (`uname -r`), `packageStateHash`/`approvedPackageStateHash` (hash of relevant `rpm -qa` subset), `approvedTarget`/`approvedOperation` (exact target/operation at approval), `preconditions`/`approvedPreconditions` (`map<key,snapshot>` for service states, config hashes, dependency state), `idempotencyKey` (==id), `appliedAt`/`completedAt` (ISO8601), `validationResult` (last rejection reason, also in audit).

**Backward compatibility:** Old JSON without `beforeBaseline`/`afterBaseline`/`comparison`/`observedBenefit` still loads (`std::optional` false). Old JSON without P12 fields (`beforeHash` etc.) still loads (empty string / empty map → skip check, but `apply` will be rejected as `missing approval binding - new preview required`, never silently reinterpreted). New completed transactions persist `beforeBaseline` + `afterBaseline` + `comparison` + P12 hardening fields for `MEASURE AGAIN` and stale defense.

**P11:** `comparison` is generated **only after** required post-change measurements are available (for `rebootRequired`, after reboot; for `loginRequired`, after next login) - **do not fake after measurements**.

**P12:** `approved*` fields are snapshot at `approve()` via `TransactionValidator::bindApproval` + `CurrentState`; `validateForApply` compares `approved*` vs current `CurrentState` (beforeHash, unitHash, kernel, package, target, operation, preconditions, TOCTOU); if mismatch → `FAILED` audit `validation.failed.stale_*` with `expected`/`observed`/`applied=false`/`backupCreated`/`approvalValid`; if `approvedBeforeHash` empty → `missing approval binding` fail-closed.

## ChangePreview

target, beforeState hash, afterState, diff (unified), method (atomic write via helper FileModify), privilege, risk, benefit, rollback, rebootRequired.

## States

`PROPOSED → PREVIEWED → APPROVAL_REQUIRED → APPROVED → AUTHORIZATION_REQUIRED → AUTHORIZED → BACKUP_CREATED → APPLYING → APPLIED → VERIFYING → VERIFIED → COMPLETED`
Failure: `APPROVED/PREVIEWED/APPROVAL_REQUIRED/AUTHORIZATION_REQUIRED/AUTHORIZED/BACKUP_CREATED/APPLYING/APPLIED/VERIFYING/VERIFIED → FAILED → ROLLING_BACK → ROLLED_BACK` (P12 added `PREVIEWED/APPROVAL_REQUIRED/APPROVED → FAILED` for stale)
Terminal: `COMPLETED`, `ROLLED_BACK`, `CANCELLED` (`isTerminal()`)

## Validation

`StateMachine::isValidTransition()` rejects invalid jumps e.g., `PROPOSED → APPLYING` or `COMPLETED → APPLYING`. Throws `logic_error` fail closed. `TransactionValidator::validateForApply` (pure) checks stale `target`/`operation`/`beforeHash`/`unitHash`/`kernelVersion`/`packageStateHash`/`preconditions`/`TOCTOU` (symlink/canonical) + `TransactionStore::apply` enforces `APPROVAL → PRECONDITION VALIDATION → BACKUP → FINAL VALIDATION → APPLY` with audit `expected`/`observed` and `applied=false` on rejection. `validateApprovalBinding` ensures `approvalId == transactionId` and `approved*` present.

## Dry-Run

`polaris transaction preview <op>` and `polaris apply --dry-run <op>` show exact target, files/resources affected, before/after, commands/API calls that WOULD be used, required privilege, risk, benefit, rollback, reboot. No writes, no privileged ops.

## Preconditions (Before APPLY) - P12 Hardened

Check `target` == `approvedTarget`, `operation` == `approvedOperation`, `beforeHash` == `approvedBeforeHash` (sha256 file content), `unitHash` == `approvedUnitHash` (enabled|active), `kernelVersion` == `approvedKernelVersion`, `packageStateHash` == `approvedPackageStateHash`, `preconditions[k]` == `approvedPreconditions[k]` (service enabled/active, config hash, dependency), TOCTOU `isSymlink` false and `canonical` unchanged, OS/component/filesystem, backup exists, disk space, lock. **Two gates:** `APPROVAL → PRECONDITION VALIDATION (before backup) → BACKUP → FINAL PRECONDITION VALIDATION (after backup, re-reads file) → APPLY`. If any mismatch or unverifiable → `FAILED` audit `validation.failed.stale_*` with `expected`/`observed`/`applied=false`/`backupCreated`/`approvalValid`, **no mutation**, require new `preview → approve` cycle (no auto-refresh). See `TransactionValidator::CurrentState` and `TransactionStore`.

## Post-conditions (After APPLY)

Re-read via same read-only providers, verify requested change happened, no unintended changes, subsystem healthy, no side effects. If fails and operation declares rollback-safe → auto rollback, else stop.

## Before/After Snapshots

Only declared affected resources + safety health indicators, not entire filesystem.

## Concurrency

File lock `/run/polaris/transaction.lock` (or `/tmp/polaris-test-root/lock` for tests). Only one mutation per resource at a time. Two instances contend → second fails with `conflicting transaction`.

## Crash Recovery

State persisted to `~/.local/state/polaris/transactions/<id>.json` (or test root). After restart `polaris transaction recover` detects `BACKUP_CREATED`, `APPLYING`, `VERIFYING` incomplete → requires re-validation, never auto-continue without explicit policy, fail closed if inconsistent.

## Reboot Handling

`rebootRequired=true` shown as “Reboot required to complete this change.” No auto reboot. Future GUI offers reboot separately.

## Observability

Events: `transaction.created`, `previewed`, `approved`, `authorization.requested/granted/denied`, `backup.created`, `started`, `completed`, `verification.passed/failed`, `rollback.started/completed` - consumable by Qt/QML via `GET /api/v1/transactions/{id}/audit`.
