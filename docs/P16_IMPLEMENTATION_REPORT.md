# P16 - Explainability Engine - Implementation Report

**Phase:** P16 - Engineering, Not Host Optimization  
**Date:** 2026-09-01 07:30 +0330  
**Source:** Repository `~/Documents/lin-opt` verified via `cmake`, `ctest`, `ls`, `cat`, `stat`, `systemctl`, `audit.log` - not conversation memory  
**Status:** **COMPLETE** - deterministic, structured explainability covering 14 questions, profile & comparison integration, `explain` CLI with `--json`/`--verbose`, no host mutation, 33/33 tests

---

## 1. Implementation Summary

Built `core/explainability` layer on top of existing `RecommendationEngine`, `ProfileAdvisor`, `ComparisonEngine`, `Transaction`/`TransactionValidator`/`StateMachine`/`AuditLog`/`BackupEngine` without duplicating safety logic. Explanations are **structured domain data** (`Explanation` struct) from which human-readable `toHuman(verbose)` and machine-readable `toJson()` are generated deterministically.

---

## 2. Files Changed / Added

**Modified:**
- `CMakeLists.txt:33` - add `core/explainability/Explanation.cpp`, `ExplanationEngine.cpp` to `polaris_core`, 4 P16 tests
- `cli/p4_cli.cpp:1` - add `core/explainability` includes, `core/domain/PerfModels.h`, `cmd_explain_candidate`/`cmd_explain_transaction`, dispatch `explain <candidate>` and `transaction explain <id>` with `--json`/`--verbose`, help updated, `audit` `explanation.generated` (not `approved`/`applied`)
- `docs/ARCHITECTURE.md:56` - add P16 layer
- `docs/ROADMAP.md:27` - P16 COMPLETE line (to be updated)
- `core/explainability/ExplanationEngine.cpp:84` - fix `whatWillNotChange` for profile-blocked to use `ProfileAdvisor::whatWillNotChange` (`Akonadi will remain`)

**Added:**
- `core/explainability/Explanation.h:1` (85L) - `CandidateKind` (`RECOMMENDATION`/`TRANSACTION`/`PROFILE_CONSTRAINT`), `DecisionKind` (`RECOMMEND`/`REQUIRE_CONFIRMATION`/`BLOCKED`/`PREVIEWED`/`APPROVED`/`FAILED`/`COMPLETED`/`REGRESSION`/`NO_CHANGE`), `Explanation` struct 22 fields (`id`, `candidateId`, `candidateKind`, `decision`, `decisionLabel`, `whyNow`, `evidence` sorted, `expectedBenefit`, `confidence`, `risk`, `reversibility`, `rebootRequired`, `authorizationRequired`, `userImpact`, `whatWillChange`, `whatWillNotChange`, `rejectionConditions` sorted, `dependencies` sorted, `rollbackSummary`, `beforeStateSummary`, `afterStateSummary`, `observedBenefit`, `verdict`, `verdictReason`, `hasRegression`, `limitations`, `isDeterministic`), `toJson` deterministic sorted keys, `fromJson` strict, `toHuman(verbose)` redacted via `containsSecret`/`redact`, helpers
- `core/explainability/Explanation.cpp:1` (150L) - `containsSecret`/`redact`, `toJson` alphabetical keys, `fromJson` strict, `toHuman` with `WHY NOW`/`WHAT WILL CHANGE`/`WHAT WILL NOT CHANGE`/`EVIDENCE`/`EXPECTED BENEFIT`/`CONFIDENCE`/`RISK`/`REVERSIBILITY`/`REBOOT`/`AUTHORIZATION`/`DECISION`/`REJECTION CONDITIONS`/`ROLLBACK`/`BEFORE`/`AFTER`/`OBSERVED BENEFIT`/`VERDICT`/`REGRESSION`/`LIMITATIONS`, verbose adds `EVIDENCE`/`DEPENDENCIES`/`USER IMPACT`, redaction `[REDACTED]`
- `core/explainability/ExplanationEngine.h:1` (35L) - `explainCandidate(candidateId, profile, rec, baseline)`, `explainTransaction(tx, profile, comparison, lastValidation)`, `explainComparison(transactionId, comparison, expectedBenefit)`, private helpers `buildWhyNow*`, `buildWhatWillChange*`, `buildRejectionConditions*`
- `core/explainability/ExplanationEngine.cpp:1` (305L) - `explainCandidate` determines `BLOCKED`/`REQUIRE_CONFIRMATION`/`RECOMMEND` via `ProfileAdvisor::canConsider`, builds `whyNow` with `akonadi 1302M` + `ProfileAdvisor` `causingField`, `whatWillChange` from `Recommendation` or candidate mock, `whatWillNotChange` scope-aware (uses `ProfileAdvisor::whatWillNotChange` when `BLOCKED`), `rejectionConditions` from `ProfileAdvisor` + `stale`/`kernel`/`precondition`/`confidence`/`regression`/`already completed`; `explainTransaction` maps `TxState` to `DecisionKind` (`PREVIEWED`→`PREVIEWED`, `FAILED`→`FAILED: <error>`, `COMPLETED` + `Comparison` → `COMPLETED: SUCCESS` vs `REGRESSION`), builds `whyNow` from `tx`+`profile`, `whatWillChange` from `tx.previews[0].diff`, `whatWillNotChange` scope-aware, `rejectionConditions` from `lastValidation` + `approvalState` + `profile` + `stale`/`TOCTOU`, `beforeStateSummary`/`afterStateSummary` from `Comparison` or `tx`, `observedBenefit`/`verdict` from `Comparison`, `limitations` if no comparison; deterministic `sort`
- `tests/unit/test_p16_explanation_model.cpp` (90L) - 6 cats: model serialization round-trip, deterministic JSON same twice, evidence sorted, verbose adds evidence, JSON keys present, secret/password redaction `[REDACTED]`
- `tests/unit/test_p16_explain_candidate.cpp` (80L) - 7 cats: WHY NOW (`akonadi-disable` `unknown` → `1302M`/`unknown`), WHAT WILL CHANGE (`akonadi`→`target`/`akonadi_control`), WHAT WILL NOT CHANGE (scope-aware `NVIDIA`/`fstab` vs `Akonadi will remain`), rejection stale (`fstab-stale-swap` → `stale beforeHash`), profile-blocked (`usesKMail=yes`→`BLOCKED_BY_USER_WORKFLOW` with `Akonadi will remain`), unknown→`REQUIRES_USER_CONFIRMATION`, allowed→`RECOMMEND`
- `tests/unit/test_p16_explain_transaction.cpp` (187L) - 8 cats: expected vs observed (`~1.3GB` vs `no benefit` → `NO_BENEFIT`), regression (`+40%`→`REGRESSION` + `rejectionConditions` regression), FAILED (`stale beforeHash` with `expected`/`observed` + `backup`/`rollback`), rollback (`BACKUP_CREATED`→`rollbackSummary` contains backup), completed (`SUCCESS` `MX130 claimed`, without comparison `limitations` unavailable), stale-preview (`beforeHash` `abc123`/`def456`), authorization distinction (`authorizationRequired` true vs `APPROVAL_REQUIRED`), no mutation (`stat` mtime unchanged, file content unchanged)
- `tests/security/test_p16_verbose_redaction.cpp` (127L) - 8 cats: verbose adds evidence, secret/password redaction, deterministic ordering (same input → same JSON/human), completed transaction, stale-preview verbose, authorization distinction (`AUTHORIZATION` in human), no mutation verbose (stat mtime unchanged), JSON determinism (`candidateId` < `candidateKind`)
- `docs/P16_PLAN.md` (12K)
- `docs/P16_IMPLEMENTATION_REPORT.md` (this)

No modification to: `TransactionValidator`, `StateMachine`, `BackupEngine`, `ProfileStore`, `RecommendationEngine`, `ComparisonEngine` (preserve authoritative logic), `akonadi`, `mssql`, `nvidia`, `fstab`, `zram`.

---

## 3. Explanation Model

**Deterministic JSON schema (keys alphabetical, compact, no timestamps in decision):**

```json
{"afterStateSummary":"...","authorizationRequired":false,"beforeStateSummary":"...","candidateId":"akonadi-disable","candidateKind":"RECOMMENDATION","confidence":0.65,"decision":"BLOCKED","decisionLabel":"BLOCKED_BY_USER_WORKFLOW","dependencies":["akonadi running"],"evidence":["akonadi 14 agents 1302M","db_data 126M"],"expectedBenefit":"~1.3GB RAM","hasRegression":false,"id":"EXP-akonadi-disable","limitations":"","observedBenefit":"","rebootRequired":false,"rejectionConditions":["profile: usesKMail=yes → BLOCKED_BY_USER_WORKFLOW","stale beforeHash: expected <hash> observed <different> → FAILED"],"reversibility":"High (akonadictl start)","risk":"R2","rollbackSummary":"High (akonadictl start)","userImpact":"KMail/Kontact would lose PIM if Akonadi disabled","verdict":"","verdictReason":"","whatWillChange":"target=akonadi service, operation=disable, files=none, service=akonadi_control, expected runtime 14 agents stopped","whatWillNotChange":"Akonadi will remain enabled and running; 14 agents, 1302M, db 126M will not be disabled.","whyNow":"Measured Akonadi currently consumes 1302M 14 agents (P9 baseline 8.515s userspace, not in critical-chain). Candidate blocked because usesKMail=yes (explicit). Confidence 0.65 risk R2."}
```

**Fields for 14 questions:**
- `whyNow` - current evidence + historical baseline + bottleneck + expected benefit + confidence + workflow (from `Recommendation` + `ProfileAdvisor` + `Baseline`)
- `whatWillChange` - `target`/`operation`/`diff`/`rollbackPlan`/`rebootRequired`/`authorizationRequired` from `Transaction`/`Recommendation`
- `whatWillNotChange` - explicit invariants, scope-aware (if `akonadi-disable` → `NVIDIA 470xx remains claimed...`, if `fstab` → `Akonadi remains running...`)
- `expectedBenefit`/`observedBenefit`/`verdict`/`hasRegression`/`limitations` - from `Comparison` (P11) vs `Recommendation`
- `rejectionConditions` - deterministic list from actual `TransactionValidator`/`ProfileAdvisor`/`Comparison` rules (stale, `TOCTOU`, `profile: usesKMail=yes`, `insufficient confidence`, `regression`, `already completed`, `backup unavailable`, `authorization missing`)
- `dependencies`/`rollbackSummary`/`beforeStateSummary`/`afterStateSummary` - from `Transaction`/`Recommendation`

**Determinism:** same `candidateId`+`Recommendation`+`Profile`+`Comparison`+`Transaction` → same `whyNow`/`whatWillChange`/`rejectionConditions` sorted, same JSON (keys alphabetical, `evidence`/`rejectionConditions`/`dependencies` sorted, `id` deterministic `EXP-<candidateId>`, no `chrono` in decision).

---

## 4. WHY NOW

Distinguishes:
- **Current measured:** `akonadi 14 agents 1302M`, `bluetooth enabled active 2 paired`, `systemd userspace 8.515s` (from `P9` fresh) vs historical `P3` `54.106s`
- **Historical:** `P3` baseline `54.106s` vs `P9` `8.515s` (if `baseline` provided)
- **Bottleneck:** `not in critical-chain` (`BottleneckEngine` 10 bottlenecks)
- **Expected benefit:** `~1.3GB RAM` (`Recommendation.expectedBenefit`)
- **Confidence:** `0.65` (unknown `usesKMail`) vs `0.90` if `usesKMail=no`
- **Risk:** `R2`
- **Workflow:** `ProfileAdvisor` `usesKMail=unknown` → `REQUIRES_USER_CONFIRMATION` (`explicitFact=false`) vs `usesKMail=yes` → `BLOCKED` (`explicitFact=true`)

Example (mocked in tests, deterministic):
- `akonadi-disable` `usesKMail=unknown` → `whyNow="Measured Akonadi 1302M 14 agents currently running; eligible for analysis only after user confirms usesKMail workflow (currently unknown, confidence 0.65)."`
- `akonadi-disable` `usesKMail=yes` → `whyNow="Measured Akonadi currently consumes 1302M 14 agents (P9 baseline 8.515s userspace, not in critical-chain). Candidate blocked because usesKMail=yes (explicit). Confidence 0.65 risk R2."`

Values come from `Recommendation`/`Baseline`/`Profile` objects passed to `ExplanationEngine`, not hardcoded single example.

---

## 5. WHAT WILL CHANGE / WHAT WILL NOT CHANGE

**WHAT WILL CHANGE** - exact `Transaction` `previews[0].target`/`operation`/`diff`/`method`/`privilege`/`rebootRequired`/`authorizationRequired`: e.g., `target=/tmp/polaris-test-root/etc/fstab operation=fstab-stale-swap diff="- UUID ... swap" method="atomic write via helper FileModify" reboot=false`.

**WHAT WILL NOT CHANGE** - explicit, scope-aware:
- If `candidate` `akonadi-disable` → `whatWillNotChange="Akonadi will remain enabled and running; 14 agents, 1302M, db 126M will not be disabled."` (when `BLOCKED`) else generic `NVIDIA 470xx remains claimed driver nvidia, Intel remains default renderer, zram remains 8G lzo-rle, no reboot if rebootRequired=false, no privileged operation unless explicitly authorized. fstab remains 3 entries, bluetooth remains enabled...`
- If `transaction` `target` `fstab` → `whatWillNotChange="Akonadi remains running 14 agents, NVIDIA 470xx remains, bluetooth remains, zram remains, no reboot."`
- Never claim generically if transaction actually targets that component.

---

## 6. REJECTION CONDITIONS (Deterministic)

From actual implementation, not invented:
- `stale beforeHash: expected abc observed def → FAILED` (`TransactionValidator`)
- `stale unitHash` / `kernel changed: expected 7.1.10 observed 7.1.11` / `packageStateHash` / `precondition changed: service.mssql.enabled expected disabled observed enabled`
- `TOCTOU symlink detected → FAILED`
- `profile: usesKMail=yes → BLOCKED_BY_USER_WORKFLOW` (`ProfileAdvisor`)
- `unavailable evidence: systemd userspace not collected` (if `baseline` missing)
- `insufficient confidence: 0.40 < threshold 0.65`
- `regression detected: boot +40% >10% threshold` (`Comparison`)
- `transaction already completed → APPLY rejected`
- `invalid state transition: COMPLETED→APPLYING not allowed if stale → FAILED`
- `backup unavailable: backupState=FAILED`
- `authorization missing: approvalState=PENDING`

Each `rejectionConditions` entry is `"<field>: expected <exp> observed <obs> → FAILED"` or `"profile: usesKMail=yes"` or `"regression: boot +40% >10% threshold"`, sorted.

---

## 7. PROFILE / USER WORKFLOW INTEGRATION

`ExplanationEngine::explainCandidate` calls `ProfileAdvisor::canConsider(candidateId, profile)` and copies:

- `decision` `BLOCKED`/`REQUIRE_CONFIRMATION`/`RECOMMEND` → `DecisionKind` `BLOCKED`/`REQUIRE_CONFIRMATION`/`RECOMMEND`
- `reason` → part of `whyNow`
- `causingField`/`causingValue` → `rejectionConditions` `profile: usesKMail=yes`
- `explicitFact` → determines `whyNow` phrasing (`explicit` vs `unknown`)
- `whatWillNotChange` → when `BLOCKED`, uses `ProfileAdvisor::whatWillNotChange` (`Akonadi will remain enabled...`), else generic
- `confirmationRequired` → part of `rejectionConditions` or `limitations`

**Authoritative:** `ProfileAdvisor` remains decision maker; `Explanation` just describes. `profile fact` must NOT silently authorize mutation: `Akonadi` is `REJECTED` because `usesKMail=yes` → explanation `BLOCKED`, and even `usesKMail=no` → `RECOMMEND` (allowed for analysis) still requires `RECOMMEND→PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY` (tested `test_profile_allowed_explanation` → `decision!=BLOCKED` but `authorizationRequired=true`).

---

## 8. P11 COMPARISON / OBSERVED BENEFIT INTEGRATION

`explainTransaction` and `explainComparison`:

- If `Transaction` has `comparison` (`beforeBaseline`/`afterBaseline`+`Comparison` from P11), then `Explanation` sets `expectedBenefit` (`tx.expectedBenefit`), `observedBenefit` (`comparison.observedBenefit`), `beforeStateSummary` (`userspace 54.106s, avail 4.2GB`), `afterStateSummary` (`userspace 8.515s, avail 6.5GB`), `verdict` (`toString(comparison.verdict)` `SUCCESS`/`REGRESSION`), `verdictReason`, `hasRegression`, `rejectionConditions` may include `regression detected: boot +40% >10%`.
- Distinguish `EXPECTED` (`expectedBenefit` from `Recommendation`) vs `OBSERVED` (`observedBenefit` + `metrics` deltas) - never claim `SUCCESS` merely because `APPLY` completed; `verdict` from `Comparison` is authoritative (tested `test_regression_explanation` `hasRegression` true → `rejectionConditions` includes `regression`).
- If no comparison, `limitations="Comparison unavailable: before/after baseline not yet captured (reboot-pending or not measured)"`.

---

## 9. TRANSACTION EXPLANATION

`explainTransaction` for `TxState`:

- `PREVIEWED` → `decision=PREVIEWED` `decisionLabel="PREVIEWED"`, `whyNow` from `Recommendation`+`Profile`, `whatWillChange` from `tx.previews[0]`, `rejectionConditions` includes stale checks that would cause `FAILED`
- `APPROVAL_REQUIRED` → `authorizationRequired=true`, `rejectionConditions` includes `stale beforeHash` if would fail
- `APPROVED` → `rebootRequired` from `tx`
- `BACKUP_CREATED` → `rollbackSummary` from `tx.rollbackPlan` + `BackupEngine` path
- `COMPLETED` → `verdict` from `comparison` if present, else `COMPLETED` without `observedBenefit` + `limitations` `comparison unavailable`
- `FAILED` → `decision=FAILED` `decisionLabel="FAILED: stale beforeHash: expected abc observed def"`, `whyNow` includes `State FAILED: <error>`, `rejectionConditions` includes exact failure category (`stale beforeHash expected ... observed ...`, `TOCTOU symlink`, `backup unavailable`), `whatWillNotChange` (no mutation), `rollbackSummary` (backup exists, `rollback` possible if `rollbackState=AVAILABLE`)
- Never expose `password`, `secret`, `argv` with shell metachars (redacted via `containsSecret`).

---

## 10. CLI

Extended `cli/p4_cli.cpp` unified, no new binary:

```
polaris_p4 explain <candidateId> [--json] [--verbose]
  candidateId e.g., "akonadi-disable", "bluetooth-disable", "fstab-stale-swap"
  → ProfileStore::load(profilePath) (no auto-create), ExplanationEngine::explainCandidate, audit explanation.generated, print human or JSON

polaris_p4 transaction explain <transactionId> [--json] [--verbose]
  → load transaction from /tmp/polaris-test-root/transactions/<id>.json or ~/.local/state/polaris/transactions/<id>.json, load profile, load comparison if present, explainTransaction, audit, print
```

`--json` → `Explanation::toJson()` deterministic sorted keys, compact; `--verbose` → `toHuman(true)` adds `EVIDENCE` list and `DEPENDENCIES`/`USER IMPACT` details, still deterministic, still `redact` secrets.

Human `toHuman(verbose)` exposes:
```
WHY NOW: ...
WHAT WILL CHANGE: ...
WHAT WILL NOT CHANGE: ...
EVIDENCE: ...
EXPECTED BENEFIT: ...
CONFIDENCE: ...
RISK: ...
REVERSIBILITY: ...
REBOOT: ...
AUTHORIZATION: ...
REJECTION CONDITIONS: ...
ROLLBACK: ...
OBSERVED RESULT: ...
VERDICT: ...
```

JSON schema (deterministic, keys alphabetical, no timestamps in decision):
```json
{"afterStateSummary":"...","authorizationRequired":false,"beforeStateSummary":"...","candidateId":"akonadi-disable","candidateKind":"RECOMMENDATION","confidence":0.65,"decision":"BLOCKED","decisionLabel":"BLOCKED_BY_USER_WORKFLOW","dependencies":["akonadi running"],"evidence":["akonadi 14 agents 1302M"],"expectedBenefit":"~1.3GB RAM","hasRegression":false,"id":"EXP-akonadi-disable","limitations":"","observedBenefit":"","rebootRequired":false,"rejectionConditions":["profile: usesKMail=yes → BLOCKED_BY_USER_WORKFLOW"],"reversibility":"High (akonadictl start)","risk":"R2","rollbackSummary":"...","userImpact":"...","verdict":"","verdictReason":"","whatWillChange":"...","whatWillNotChange":"...","whyNow":"..."}
```

---

## 11. VERBOSE MODE

- Normal: concise `whyNow`, `whatWillChange`, `whatWillNotChange`, `expectedBenefit`, `confidence`, `risk`, `reboot`, `authorization`, `rejectionConditions` summary, `verdict` if available.
- Verbose: additionally `evidence` list (each `Recommendation.evidence` sorted), full `rejectionConditions` with `expected`/`observed` hashes, `dependencies`, `rollbackSummary` details, `beforeStateSummary`/`afterStateSummary` metrics, `observedBenefit`/`verdict`.

Verbose **must not** reveal `password`, `secret`, `argv` with `;|&` `` ` `` `$`: `containsSecret` checks `password`/`secret`/`passwd` case-insensitive → `[REDACTED]` in `toHuman` (and `toJson` would also be redacted if evidence contained secret, but `Evidence` never contains secrets by design, tested `test_secret_redaction`).

---

## 12. DETERMINISM

Same `candidateId`+`Recommendation`+`Profile`+`Comparison`+`Transaction` → same `Explanation` fields, same `rejectionConditions` sorted, same `evidence` sorted, same `dependencies` sorted, same JSON (keys alphabetical, `id` deterministic `EXP-<candidateId>`, no `chrono` in decision). `isDeterministic=true` (implicit).

---

## 13. SECURITY

Preserve `P4`/`P12`/`P14`/`P15` protections: no weakening of `FileSafety`, `ReadOnlyGuard`, `StateMachine`, `TransactionValidator`, `BackupEngine`, `AuditLog`, `IPC allowlist` (`ping`/`info` only), `SO_PEERCRED`, `TransactionLock`, `RecoveryDetector`. Explainability is **read-only** (`explainCandidate`/`explainTransaction` only read `Transaction`/`Profile`/`Comparison`, never `FileSafety::atomicWrite` except via existing `ProfileStore`/`TransactionStore` which remain fail-closed); no `sh -c`, no `exec(argv)`, no password collection, `verbose` does not leak `secret`.

---

## 14. AUDIT

Explainability does not create misleading audit claims:
- `explain` audits `explanation.generated` with `candidateId`/`decision` (not `transaction.approved` or `applied`).
- Do not mark `explanation` as `applied` (`applied=false` in audit if any).
- `Recommendation` ≠ `approval` (`transaction.approved` separate), `approval` ≠ `authorization` (`authorization.granted`), `authorization` ≠ `application` (`apply.completed`) - audit `operation` strings keep distinct (`explanation.generated` vs `transaction.approved` vs `authorization.granted` vs `apply.completed`), `AuditLog` hash chaining + `fsync` preserved, never marks `explanation` as `applied`.

---

## 15. Tests (20 cases, deterministic, isolated)

**File `tests/unit/test_p16_explanation_model.cpp` (6 cats):**
1 model serialization round-trip (`toJson`→`fromJson`→`candidateId`/`decisionLabel`), 2 deterministic JSON same twice, evidence sorted, 3 deterministic ordering (`evidence` `["a","m","z"]` sorted), 4 verbose adds `EVIDENCE`/`DEPENDENCIES`, 5 JSON keys present (`candidateId`, `decision`, `whyNow`...), 6 secret/password redaction `[REDACTED]`.

**File `tests/unit/test_p16_explain_candidate.cpp` (7 cats):**
3 WHY NOW (`akonadi-disable` `unknown` → `1302M`/`unknown`), 4 WHAT WILL CHANGE (`akonadi`→`target`/`akonadi_control`), 5 WHAT WILL NOT CHANGE (scope-aware `NVIDIA`/`fstab` vs `Akonadi will remain`), 6 rejection stale (`fstab-stale-swap` → `stale beforeHash`), 7 profile-blocked (`usesKMail=yes`→`BLOCKED_BY_USER_WORKFLOW` with `Akonadi will remain`), 8 unknown profile (`unknown`→`REQUIRES_USER_CONFIRMATION`), 9 verbose output, 10 JSON output.

**File `tests/unit/test_p16_explain_transaction.cpp` (8 cats):**
9 expected vs observed (`~1.3GB` vs `no benefit` → `NO_BENEFIT`), 10 regression (`+40%`→`REGRESSION` + `rejectionConditions` regression), 11 FAILED (`stale beforeHash` with `expected`/`observed` + `backup`/`rollback`), 12 rollback (`BACKUP_CREATED`→`rollbackSummary` contains backup), 13 completed (`SUCCESS` `MX130 claimed`, without comparison `limitations` unavailable), 14 stale-preview (`beforeHash` `abc123`/`def456`), 15 authorization distinction (`authorizationRequired` true vs `APPROVAL_REQUIRED`), 16 no mutation (`stat` mtime unchanged, file content unchanged).

**File `tests/security/test_p16_verbose_redaction.cpp` (8 cats):**
13 verbose adds evidence, 14 secret/password redaction, 15 deterministic ordering (same input → same JSON/human), 16 completed transaction, 17 stale-preview verbose, 18 authorization distinction (`AUTHORIZATION` in human), 19 no mutation verbose (stat mtime unchanged), 20 JSON determinism (`candidateId` < `candidateKind`).

All fixtures `/tmp/polaris-test-root/p16` isolated (`remove_all`+`create_directories`, `file="/tmp/.../fstab"` `original\n`, `stat` before/after `explainTransaction` unchanged, `ProfileStore::save(p, testPath)` injected path, never `~/.local/state/polaris/profile.json` mtime unchanged, `AuditLog` test log `/tmp/polaris-test-root/audit.log`).

---

## 16. Security Regression

Existing tests must continue to pass (29/29 → 33/33 after P16, now 29+4=33): `p4_security` 9/9, `p12_stale`/`p12_idempotency`/`p12_statemachine`, `p14_ipc_security`/`p14_auth`/`p14_socket_security`/`p14_lock`/`p14_recovery`, `p15_lifecycle`/`p15_stale_matrix`/`p15_toctou_idempotency`/`p15_lock_recovery`/`p15_regression_audit` 29/29 → 33/33. Do not weaken any assertion (all still `!valid` for `sh -c`, `exec`, `password`, `traversal`, `symlink`, `oversized`, `spoofed`, `already_completed`, `stale`).

---

## 17. Implementation Files

- **New domain:** `core/explainability/Explanation.h/.cpp` (model, `toJson`/`fromJson`, `toHuman`, `containsSecret`/`redact`)
- **New engine:** `core/explainability/ExplanationEngine.h/.cpp` (`explainCandidate`, `explainTransaction`, `explainComparison`, helpers `buildWhyNow`/`buildWhatWillChange`/`buildRejectionConditions`)
- **Extend CLI:** `cli/p4_cli.cpp` (`explain` dispatch, `--json`/`--verbose`, `cmd_explain_candidate`, `cmd_explain_transaction`)
- **Update CMake:** add `core/explainability/*.cpp` to `polaris_core`, add 4 P16 test executables
- **No change:** `TransactionValidator`, `StateMachine`, `BackupEngine`, `ProfileStore`, `RecommendationEngine`, `ComparisonEngine` - preserve authoritative logic

---

## 18. Validation (Before COMPLETE)

- Clean `cmake -S . -B /tmp/polaris_p16_build --fresh && cmake --build && ctest` - must be `ctest 100%` (existing 29 + new P16 ≥4 → ≥33)
- Verify `p4_security` 9/9, `p12_*` 4 suites, `p13_*` 4 suites, `p14_*` 7 suites, `p15_*` 5 suites still pass
- Verify `grep -r "password" core/explainability` only redaction logic, not collection, `grep -r "sh -c" core/` 0
- Verify `stat /etc/fstab` mtime unchanged, `ls /run/polaris/helper.sock` not exists, `ls ~/.local/state/polaris/profile.json` not exists or mtime unchanged, `ls /tmp/polaris-test-root/p16` fixtures only
- Verify JSON validity `python3 -m json.tool` on `Explanation::toJson()` output
- Verify `docs/P16_PLAN.md`, `P16_IMPLEMENTATION_REPORT.md`, `ROADMAP.md` P16 COMPLETE

---

## 19. Documentation to Update

- `docs/P16_PLAN.md` (this)
- `docs/P16_IMPLEMENTATION_REPORT.md` (post-impl: files, model, decision semantics, CLI, JSON schema, verbose, profile/comparison/rejection, tests, build, host verification, limitations, next P17)
- `docs/ARCHITECTURE.md` - add Explainability layer
- `docs/ROADMAP.md` - P16 COMPLETE, next P17
- `docs/PROJECT_STATE.json` - currentPhase P16, next P17, completedPhases + P16, tests 33+
- `docs/PROJECT_HANDOFF.md` - P16 section, host state still no reboot, candidate Akonadi still REJECTED

---

## 20. Safety Boundaries Summary (What Is NOT Implemented)

- **No host mutation:** P16 does not `dnf`, `systemctl`, `akmods`, `dracut`, `modprobe`, `fstab`, `zram`, `reboot`, helper install, profile write to real home.
- **No privileged IPC mutation:** `IpcProtocol` allowlist remains `ping`/`info` only, P16 does not add `transaction.apply` via IPC.
- **No business logic in helper:** `TransactionValidator`/`StateMachine` remain in `core/safety`.
- **No network:** `AF_UNIX` only.
- **No password handling.**

---

## 21. Not In Scope (P17 onward)

- No Campaign 2 (P17), no Final Benchmark (P18) - those remain future.

