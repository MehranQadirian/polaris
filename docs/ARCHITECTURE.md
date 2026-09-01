# Architecture - Polaris

## High-Level

```
COLLECT → BASELINE → DETECT → CLASSIFY → EXPLAIN → RANK → PREVIEW → APPROVAL → AUTHORIZATION → BACKUP → APPLY → VERIFY → MEASURE AGAIN → LEARN
```

P11 adds `MEASURE AGAIN` (ComparisonEngine) and `LEARN` (observedBenefit vs expectedBenefit + regression).

## Layers

- **Providers** collect facts (`RealOsProvider`, `RealCpuProvider`, etc.) - read-only, no policy.
- **Engines** analyze facts (`BaselineEngine`, `BottleneckEngine`, `BenchmarkEngine`, `ComparisonEngine`, `RecommendationEngine`) - pure, side-effect-free.
- **Transaction Engine** (`StateMachine`, `Transaction` with `beforeBaseline`/`afterBaseline`/`comparison`, `FileSafety`, `BackupEngine`) - executes only approved operations.
- **Security Layer** (`ReadOnlyGuard`, `Polkit`, `AuditLog` hash chaining) - authorizes.
- **Verification Layer** (`ComparisonEngine` + `RegressionEngine`) - measures outcome, not just `APPLIED`.

## P4 Safety Layer (Added)

Safety/Transaction Layer now includes StateMachine (16 states, `isValidTransition` fail-closed), FileSafety (allowlist, canonical, symlink, `validatePath`), BackupEngine (versioned `SHA-256` no overwrite), AuditLog (hash chaining), Polkit policies (narrowly scoped, `auth_admin_keep`), CLI `polaris_p4` on `/tmp/polaris-test-root`.

## P11 Post-Change Measurement (Added)

- `core/domain/Comparison.h` (`Comparison`, `MetricComparison`, `Verdict` `SUCCESS`/`REGRESSION` etc., `isDeterministic` true, serializable)
- `core/engines/comparison/ComparisonEngine.h/.cpp` (pure, `compare(before, after, expectedBenefit)` with `Thresholds` `boot +10%`, `available -1GiB`, `thermal +15C`, `new_failed`, stored with result, `expectedBenefit` vs `observedBenefit` separation)
- `Transaction` extended with `beforeBaseline`, `afterBaseline`, `comparison` (`std::optional`, backward compatible)
- CLI `polaris_p4 transaction show --json` now exposes `beforeBaseline`/`afterBaseline`/`comparison` if present, `compare` command
- No new standalone binary (as required), no host writes.

See `docs/P11_POST_CHANGE_MEASUREMENT.md` for lifecycle, metric semantics, thresholds, P7 fixture.

## P12 Transaction/State-Machine Hardening (Added)

- `core/safety/transaction/Transaction.h` extended with P12 hardening: `beforeHash/approvedBeforeHash`, `beforeUnitHash/approvedUnitHash`, `kernelVersion/approvedKernelVersion`, `packageStateHash/approvedPackageStateHash`, `approvedTarget/approvedOperation`, `preconditions/approvedPreconditions`, `idempotencyKey/appliedAt/completedAt/validationResult` (backward compatible, empty=not set, never silently reinterpreted)
- `core/safety/transaction/TransactionValidator.h/.cpp` (pure, deterministic, side-effect-free): `CurrentState` snapshot, `hashString`, `bindApproval`, `validateApprovalBinding` (id + hash + target/operation binding), `validateForApply`/`finalPreconditionValidation` (stale checks for target/operation/beforeHash/unitHash/kernel/package/precondition, TOCTOU symlink/canonical, unverifiable → fail-closed, expected/observed/auditOperation)
- `core/safety/transaction/TransactionStore.h/.cpp` (registry): `create` duplicate deterministic `ALREADY_EXISTS` (no overwrite), `approve` binds and is idempotent, `canApply`/`apply` enforce `APPROVAL→VALIDATION→BACKUP→FINAL VALIDATION→APPLY` with `StateMachine` gate and `FileSafety` reuse, `COMPLETED`/`APPLIED` idempotent rejection with audit `already_completed`, `verify` idempotent, `clear` wipes test fixtures and test backup root
- `core/safety/transaction/StateMachine.h` hardened: `PREVIEWED/APPROVAL_REQUIRED/APPROVED → FAILED` for stale (fail-closed), `isTerminal`/`isFailed` helpers, illegal transitions still rejected (`COMPLETED→APPLYING` etc.), `BACKUP_CREATED→APPLYING` still gated by store validation
- `core/safety/audit/AuditLog.cpp` hardened: `fsync` per event (open+fsync) for crash safety, audit `expected/observed/field/applied/backupCreated/approvalValid` in `error` JSON, preserves `previousHash` hash chain
- No new Polkit actions, no `sh -c`, no password, no host mutation during discovery - preserves P4 invariants; tests only on `/tmp/polaris-test-root`.

See `docs/P12_PLAN.md` and `docs/P12_IMPLEMENTATION_REPORT.md`.

## P13 User Workflow / Profile Engine (Added)

- `core/profile/UserProfile.h/.cpp` - `TriState` `UNKNOWN/YES/NO` (`unknown` default, `toString`/`fromString`), `UserProfile` 8 fields (`usesKMail`, `usesKontact`, `usesKOrganizer`, `usesBluetooth`, `usesPrinting`, `usesAvahi`, `usesCups`, `usesAkonadi`) + `extra` map, `isDefaultUnknown`, `==`, `getField`/`setField` explicit (throws on unknown field), `toJson` deterministic sorted keys, `fromJson` strict (throws on malformed/invalid value), `knownFields()` sorted
- `core/profile/ProfileStore.h/.cpp` - `profilePath()` `~/.local/state/polaris/profile.json`, `testProfilePath()` `/tmp/polaris-test-root/profile.json`, `load` (missing→unknown no auto-create, symlink→throw, malformed→throw + audit `profile.load.malformed`), `save` (validate path traversal/metachars/allowlist via `FileSafety`, symlink check, `create_directories`, atomic `tmp+fsync+chmod 0600+rename`, parent canonical check, 0600), `exists`/`remove`, deterministic
- `core/profile/ProfileService.h/.cpp` - explicit `updateField(profile, field, TriState/valueStr, path)` (validates known field, no inference, idempotent `profile.update.idempotent` if same value skips write, else `setField` + `save` + audit `profile.updated` with `field`/`previous`/`new`/`applied`), `updateFieldInStore`, `isIdempotent`
- `core/profile/ProfileAdvisor.h/.cpp` - `Decision` `BLOCKED_BY_USER_WORKFLOW`/`REQUIRES_USER_CONFIRMATION`/`ALLOWED_FOR_ANALYSIS`, `AdvisorResult` (`reason`, `causingField/value`, `explicitFact`, `whatWillNotChange`, `confirmationRequired`, `candidate`), `canConsiderAkonadi` (YES any of `usesKMail/Kontact/KOrganizer/Akonadi` → `BLOCKED`; UNKNOWN any → `REQUIRES`; all NO → `ALLOWED` with `does NOT authorize mutation` note), `canConsiderBluetooth/Printing/Avahi/Cups` similarly, `canConsider(candidateId)` generic, all explainable strings
- `core/safety/FileSafety.h` extended: allowlist `~/.local/state/polaris/profile.json` and `~/.local/state/polaris/` dir, `validatePath` permits profile (P13)
- `cli/p4_cli.cpp` extended: `profile show` (loads without auto-create, prints JSON + advisor example, `--json`), `profile set <field> <yes|no|unknown>` (explicit, audit, not authorization), help updated
- No Qt, no network, no inference (`KMail→Akonadi` not inferred, hardware→`usesBluetooth` not inferred), profile is constraint, not approval - preserves `StateMachine`/`TransactionValidator` invariants; offline-first deterministic; tests only `/tmp/polaris-test-root`.

See `docs/P13_PLAN.md` and `docs/P13_IMPLEMENTATION_REPORT.md`.

## P14 Expanded Security & IPC / Helper Architecture (Added)

- `core/ipc/IpcProtocol.h/.cpp` - `PROTOCOL_VERSION=1`, `MAX_REQUEST_SIZE=64KB`, `MAX_RESPONSE_SIZE=64KB`, `MAX_ARG_COUNT=16`, `MAX_ARG_SIZE=4096`, `MAX_FIELD_SIZE=256`, `TIMEOUT_MS=5000`, `allowedOperations` `ping`/`info` only (no privileged mutation), `Request`/`Response`/`ValidationResult`, `validate()`/`validateRaw()` (NUL, protocol version, requestId/operation size/shell/traversal/allowlist, args count/size/shell/traversal/password/control-char, fail-closed), `serialize`/`parse` deterministic JSON + `serializeResponse`/`parseResponse`, `containsNul`/`containsTraversal`/`containsShellMetachars`
- `core/ipc/IpcAuth.h/.cpp` - `PeerCred` (`pid,uid,gid`), `getPeerCred(int fd)` via `getsockopt(SO_PEERCRED)` Linux (`nullopt` on `fd<0`/failure/unavailable), `isAuthorized(cred, expectedUid)` (`uid==expected && pid>0`), `containsSpoofedCred(args)` (keys `uid/pid/gid/peer_uid`), `currentUid()`, documented Linux-specific
- `core/ipc/IpcServer.h/.cpp` - `defaultSocketPath()` `/run/polaris/helper.sock` (defined but never created in P14), `testSocketPath()` `/tmp/polaris-test-root/p14/helper.sock`, `validateSocketPath` (NUL, traversal, shell metachars, `>200`, allowlist `/tmp/polaris-test-root/` or `/run/polaris/`, symlink), `checkParentSecurity` (exists, not symlink, not world-writable `S_IWOTH`, owned by `getuid()`), `isStaleSocket` (`S_ISSOCK` + `connect` `ECONNREFUSED`), `start()` (`validateAndPrepare` `mkdir 0700`, stale unlink if owned, `socket` `FD_CLOEXEC`, `umask 0077` `bind` `chmod 0600` `listen(8)`), `stop()` `close`+`unlink` only if not symlink, `handleRequest(raw, peerCred)` (auth `unavailable→error`, `isAuthorized` wrong UID → `peer not authorized`, `containsSpoofedCred` → `spoofed credentials rejected`, `validateRaw` → `malformed`/`oversized`/`unknown operation`, allowlist `ping`→`pong` / `info`→`version`, audit `ipc.*` with `TX-TEST-IPC-` prefix for test log, `fsync` via `AuditLog`), `handleNextConnection(timeoutMs)` (`poll` accept 5s, `getPeerCred`, `poll` recv 5s, bounded `MAX_REQUEST_SIZE`, handle, send bounded response + `\n`, close)
- `core/ipc/IpcClient.h/.cpp` - `testSocketPath`, `send(Request)` serialize+`sendRaw`, `sendRaw(string)` (size check, `socket` `FD_CLOEXEC`, non-blocking `connect` + `poll` `SO_ERROR`, `::send`, `poll` `recv`, size checks, `parseResponse`)
- `core/safety/lock/TransactionLock.h/.cpp` - `defaultLockPath()` `/run/polaris/transaction.lock` (never used in tests), `testLockPath()` `/tmp/polaris-test-root/p14/transaction.lock`, `tryLock()` (`open O_CREAT|O_RDWR 0600` `FD_CLOEXEC`, parent symlink/world-writable/ownership check, `flock LOCK_EX|LOCK_NB` → `lock.rejected` on `EWOULDBLOCK`, `chmod 0600`, `lock.acquire`), `unlock()` `flock LOCK_UN`+`close`, `isLocked`, audit `lock.acquire`/`rejected`/`release`, `FD_CLOEXEC`
- `core/safety/recovery/RecoveryDetector.h/.cpp` - `RecoveryInfo` (`id`, `state`, `backupPath`, `backupExists`, `suggested=FAILED`, `reason`), `detect(storePath)` scans `*.json` for `state` in `BACKUP_CREATED/APPLYING/APPLIED/VERIFYING/AUTHORIZED` → incomplete, `COMPLETED` not flagged, `suggested FAILED` (never `COMPLETED`), `isIncomplete`, `shouldFailClosed` always true, `defaultStorePath`/`testStorePath`, checks `BackupEngine::testBackupRoot`/`backupRoot` for `backupExists`, audit `recovery.detected`, never auto-mutates
- `core/safety/FileSafety.h` already allows `/tmp/polaris-test-root` (no change needed for IPC socket, but socket security uses `isSymlink`/`canonical` directly)
- No new Polkit, no `sh -c`, no password, no helper install - preserves invariants; tests only `/tmp/polaris-test-root/p14`.

See `docs/P14_PLAN.md` and `docs/P14_IMPLEMENTATION_REPORT.md`.

## P15 Test / CI / Fixture Expansion (Added)

- `core/safety/transaction/TransactionValidator.h` fix - `UNAVAILABLE` (`kernelVersion`/`packageStateHash`/`beforeHash` empty where `approved*` non-empty → `unverifiable_*` fail-closed, previously passed)
- `tests/unit/test_p15_lifecycle.cpp` - table-driven `StateMachine` valid 16 + rejected 12 with `logic_error` `rejected, fail closed`, `stale→FAILED`, `unverifiable→FAILED` (deterministic)
- `tests/unit/test_p15_stale_matrix.cpp` - 7 fields (`target`, `operation`, `beforeHash`, `unitHash`, `kernel`, `package`, `precondition`) ×3 states (`UNCHANGED` accepted, `CHANGED`/`UNAVAILABLE` rejected) 19 cases `expected`/`observed` deterministic, multi-field first failure
- `tests/unit/test_p15_toctou_idempotency.cpp` - `TOCTOU` between gates (first valid, second re-read stale → `FAILED` no mutation backup preserved), symlink TOCTOU, idempotency `create`/`approve`/`apply`/`verify` + reload via `exists`/`duplicate` not overwrite
- `tests/unit/test_p15_lock_recovery.cpp` - `TransactionLock` exclusive table (2/4/3 threads), `FD_CLOEXEC`, stale parent, `RecoveryDetector` table 10 states, scan `BACKUP_CREATED`/`APPLYING` detected, `COMPLETED` not, corrupted not, never auto-apply, rollback backup stable not overwritten (`sha256File` stable, second `create` throws)
- `tests/unit/test_p15_regression_audit.cpp` - regression thresholds `boot` exactly 10% not, just above regression, `mem`/`thermal`, `unavailable`, `multi`, `observedBenefit` positive/zero/negative deterministic, `FileSafety` 12 cases, `IpcProtocol` 6, `Audit` chain `previousHash`→`eventHash` deterministic `fsync`, no secrets, fixture isolation `p15_iso1` vs `p15_iso2`; plus `.github/workflows/ci.yml` minimal `cmake --fresh`/`cmake --build`/`ctest`/`test ! -f /run/polaris/*`
- Fixture isolation: each test `root="/tmp/polaris-test-root/p15_"+name`, `remove_all`+`create_directories`, all `profile.json`/`transactions`/`backups`/`audit.log` under root, never `~/.local/state/polaris`/`/run/polaris`/`/etc`
- No privileged mutation, no `dnf`/`systemctl`/`reboot`, table-driven deterministic.

See `docs/P15_PLAN.md` and `docs/P15_IMPLEMENTATION_REPORT.md`.

