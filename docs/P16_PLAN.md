# P16 - Explainability Engine - Plan

**Phase:** P16 - Engineering, Not Host Optimization  
**Mode:** READ-ONLY PLANNING + IMPLEMENTATION - no `dnf`, `akmods`, `dracut`, `sudo`, `reboot`, no Akonadi/NVIDIA/mssql/fstab/zram mutation - only `core/explainability` engineering, `tests/` fixtures `/tmp/polaris-test-root`, `docs/` updates  
**Date:** 2026-09-01 07:00 +0330  
**Source:** Inspect `~/Documents/lin-opt` P15 state (`core/domain/Comparison.h` verdict, `core/safety/transaction/Transaction.h` 16 states + `beforeHash` + `comparison`, `core/profile/ProfileAdvisor.h` `BLOCKED/REQUIRES/ALLOWED`, `core/engines/recommend/RecommendationEngine.h` 7 recs, `docs/PROJECT_HANDOFF.md` 29/29, `CMakeLists.txt` 164L, `cli/p4_cli.cpp` `profile show/set`)  
**Dependency:** P15 `Test/CI` (COMPLETED, 29/29 deterministic fixtures) - P16 adds deterministic explainability on top, not new safety logic

---

## 1. What P15 Left & Why P16 Is Next

P15 made `ctest 29/29` deterministic table-driven and added CI, but explanations are still scattered:
- `Recommendation` has `evidence/confidence/benefit/risk` but no `WHY NOW?` `WHAT WILL NOT CHANGE?` `WHAT WOULD MAKE US REJECT?` as required by P16 spec.
- `ProfileAdvisor::canConsiderAkonadi` returns `BLOCKED` with `reason` but not in a unified `Explanation` model that also covers `Comparison` `expected vs observed` and `Transaction` `FAILED` `expected/observed` and `StateMachine` `COMPLETED` vs `FAILED`.
- `polaris_p4` CLI has `transaction preview/list/show/approve`, `profile show/set`, `audit list` but no `explain` with `--json`/`--verbose` that answers the 14 questions.
- No redaction guarantee (`verbose` must not leak `password`/`secret`).

P16 value: make every recommendation, preview, rejection, verification **explainable** from structured fields, deterministic, auditable, without moving authorization logic into explainability.

---

## 2. Objectives (Deterministic, Structured)

Build `core/explainability` layer on top of existing:
- `RecommendationEngine` (7 recs)
- `ProfileAdvisor` (8 fields, `TriState`)
- `ComparisonEngine` (thresholds `boot +10%`, `mem 1GB`, `thermal 15C`, `failed new`)
- `Transaction` + `TransactionValidator` + `StateMachine` + `AuditLog` + `BackupEngine` (16 states, `beforeHash`, `comparison`)

Do NOT duplicate safety decisions; explain them.

---

## 3. Core Explainability Model

`core/explainability/Explanation.h`:

```cpp
enum class CandidateKind { RECOMMENDATION, TRANSACTION, PROFILE_CONSTRAINT };
enum class DecisionKind { RECOMMEND, REQUIRE_CONFIRMATION, BLOCKED, PREVIEWED, APPROVED, FAILED, COMPLETED, REGRESSION, NO_CHANGE };

struct Explanation {
  std::string id; // e.g., "EXP-akonadi-001" or "EXP-TX-001"
  std::string candidateId; // e.g., "akonadi-disable" or "TX-TEST-001"
  CandidateKind candidateKind;
  DecisionKind decision;
  std::string decisionLabel; // "BLOCKED_BY_USER_WORKFLOW", "REQUIRES_USER_CONFIRMATION", "PREVIEWED", "FAILED: stale beforeHash", "COMPLETED: SUCCESS"
  std::string whyNow; // deterministic, backed by evidence: e.g., "Measured Akonadi 1302M 14 agents currently running; user workflow not yet declared usesKMail=unknown"
  std::vector<std::string> evidence; // from Recommendation.evidence + ProfileAdvisor + Comparison metrics
  std::string expectedBenefit; // from Recommendation.expectedBenefit
  double confidence = 0; // e.g., 0.65
  std::string risk; // R0-R3
  std::string reversibility; // e.g., "High (akonadictl start)"
  bool rebootRequired = false;
  bool authorizationRequired = false;
  std::string userImpact; // e.g., "KMail would lose PIM if Akonadi disabled"
  std::string whatWillChange; // exact transaction scope: target, operation, files, service, package
  std::string whatWillNotChange; // explicit invariants: "NVIDIA 470xx remains unchanged, Intel remains default, fstab remains unchanged, zram remains unchanged, no reboot"
  std::vector<std::string> rejectionConditions; // deterministic list: "stale beforeHash expected ... observed ...", "user workflow usesKMail=yes", "insufficient confidence", "regression boot +40%"
  std::vector<std::string> dependencies; // e.g., "requires akonadi running"
  std::string rollbackSummary; // e.g., "Restore from backup /tmp/.../fstab.bak, systemctl enable akonadi"
  std::string beforeStateSummary; // e.g., "Akonadi 14 agents 1302M, failed 0, userspace 8.515s"
  std::string afterStateSummary; // e.g., after apply (if available)
  std::string observedBenefit; // from Comparison.observedBenefit (if available)
  std::string verdict; // from Comparison.toString(verdict) if available
  std::string verdictReason; // from Comparison
  bool hasRegression = false;
  std::string limitations; // e.g., "Comparison unavailable: reboot-pending"
  // Human-readable helpers (generated from structured, not sole truth)
  std::string toHuman(bool verbose) const;
  std::string toJson() const; // deterministic sorted keys, compact
  static Explanation fromJson(const std::string&); // strict
};
```

- Deterministic: same structured input → same `whyNow`/`whatWillChange`/`rejectionConditions` ordering (sorted), same JSON (keys alphabetical, no timestamps in decision semantics, `id` deterministic from `candidateId`).
- Avoid `unordered_map` in serialization; use `std::map` or sorted vector.
- Human-readable `toHuman(verbose)` generates prose from structured fields, not free-form sole truth; `verbose` adds `evidence` list and `rejectionConditions` details but not secrets.
- No timestamps as decision inputs; `id` deterministic.

---

## 4. WHY NOW (Evidence-Backed)

`WHY NOW` must distinguish:
- **Current measured evidence** - `PerformanceBaseline` `memory.available`, `systemd.failedCount`, `gpu.nvidia.claimed`, `ProcessBaseline` `akonadi 1302M`, `SystemdBaseline` `userspace` vs `P3` baseline `54.106s` etc.
- **Historical baseline** - `p3_analysis.json` `userspace 54.106s` vs `p9` `8.515s`.
- **Current bottleneck** - `BottleneckEngine` `10 bottlenecks` or `candidate` specific evidence (e.g., `akonadi 14 agents`).
- **Expected benefit** - `Recommendation.expectedBenefit` `~1.3GB`.
- **Confidence** - `0.65` (unknown `usesKMail`) vs `0.90` if `usesKMail=no`.
- **Risk** - `R2`.
- **User workflow constraint** - `ProfileAdvisor` `usesKMail=unknown` → `REQUIRES_USER_CONFIRMATION` (explicit, not inferred).

Example generation (P13 data):
- Candidate `akonadi-disable` with `usesKMail=unknown` → `whyNow="Measured Akonadi currently consumes 1302M 14 agents (P9 baseline 8.515s userspace, not in critical-chain). Eligible for analysis only after user confirms usesKMail workflow (currently unknown, confidence 0.65, risk R2). No host mutation yet."`
- Candidate `akonadi-disable` with `usesKMail=yes` → `whyNow="Measured Akonadi 1302M but user workflow protects it: usesKMail=yes (explicit). Candidate blocked, confidence would be 0.90 if usesKMail=no, but currently not recommended."`

Values come from underlying `Recommendation`/`Baseline`/`Profile` objects passed to `ExplanationEngine`, not hardcoded.

---

## 5. WHAT WILL CHANGE / WHAT WILL NOT CHANGE

**WHAT WILL CHANGE** - exact `Transaction` `target`/`operation`/`previews[0].diff`/`rollbackPlan`/`rebootRequired`/`authorizationRequired`: e.g., `target=/tmp/polaris-test-root/etc/fstab, operation=fstab-stale-swap, diff "- UUID... swap"→"# disabled", method="atomic write via helper FileModify", privilege="org.polaris.modify.fstab", rebootRequired=false`.

**WHAT WILL NOT CHANGE** - explicit invariants, transaction-scope aware:
- If `candidate` is `akonadi-disable` → `whatWillNotChange="NVIDIA 470xx remains claimed driver nvidia, Intel remains default renderer, fstab remains 3 entries, zram remains 8G lzo-rle, no reboot, no privileged operation unless approved"`.
- If `candidate` is `fstab-stale-swap` → `whatWillNotChange="Akonadi remains running 14 agents, NVIDIA remains 470.256.02, mssql remains disabled, zram remains, no reboot"`.
- Generic fallback: `whatWillNotChange` includes `no reboot if rebootRequired=false`, `no privileged operation unless explicitly authorized`, plus `ProfileAdvisor` `whatWillNotChange` if applicable.

Do not claim generically if transaction actually targets that component (e.g., `fstab` transaction's `whatWillNotChange` should not say `fstab remains unchanged`).

---

## 6. REJECTION CONDITIONS (Deterministic)

Expose list from actual implementation, not invented:

- `stale beforeHash: expected <hash> observed <hash>` (from `TransactionValidator`)
- `stale unitHash` / `kernelVersion` / `packageStateHash`
- `precondition changed: service.mssql.enabled expected disabled observed enabled`
- `TOCTOU symlink detected`
- `user workflow blocks candidate: usesKMail=yes → Akonadi BLOCKED` (from `ProfileAdvisor`)
- `unavailable evidence: systemd userspace not collected`
- `insufficient confidence: 0.40 < threshold 0.65`
- `expected benefit below threshold` (if `expectedBenefit` empty)
- `regression detected: boot +40% >10%` (from `Comparison`)
- `transaction already completed` / `invalid state transition: COMPLETED→APPLYING`
- `backup unavailable: backupState=FAILED`
- `authorization missing: approvalState=PENDING`
- `duplicate id` / `idempotent already_approved`

Each `rejectionConditions` entry is deterministic string `"<field>: expected <exp> observed <obs>"` or `"profile: usesKMail=yes"` or `"regression: boot +40% >10% threshold"`.

---

## 7. PROFILE / USER WORKFLOW INTEGRATION

Integrate `P13 ProfileAdvisor` without moving decision:

- `ExplanationEngine::explainCandidate(candidateId, profile)` calls `ProfileAdvisor::canConsiderAkonadi/BT/etc.` and copies `reason`, `causingField/value`, `explicitFact`, `whatWillNotChange`, `confirmationRequired` into `Explanation`.
- If `BLOCKED_BY_USER_WORKFLOW` → `decision=BLOCKED`, `whyNow` includes `ProfileAdvisor` `reason`, `whatWillNotChange` includes advisor's `whatWillNotChange`.
- If `REQUIRES_USER_CONFIRMATION` → `decision=REQUIRE_CONFIRMATION`, `whyNow` includes `explicitFact=false`.
- If `ALLOWED_FOR_ANALYSIS` → `decision=RECOMMEND` or `PREVIEWED` (still not `APPROVED`), `whyNow` includes `explicitFact=true` and `does NOT authorize mutation`.

The `Recommendation` + `ProfileAdvisor` remains authoritative; `Explanation` just describes.

---

## 8. P11 COMPARISON / OBSERVED BENEFIT INTEGRATION

Integrate `ComparisonEngine`:

- If `Transaction` has `comparison` optional (`beforeBaseline`/`afterBaseline`+`Comparison`), then `Explanation` sets:
  - `expectedBenefit` from `Transaction.expectedBenefit` or `Recommendation`
  - `observedBenefit` from `Comparison.observedBenefit`
  - `beforeStateSummary` `userspace 54.106s, avail 4.2GB, thermal 67C, failed 1, nvidia 0`
  - `afterStateSummary` `userspace 8.515s, avail 6.5GB, thermal 50C, failed 0, nvidia 1`
  - `verdict` `toString(Comparison.verdict)` (`SUCCESS`/`REGRESSION` etc.)
  - `verdictReason` `Comparison.verdictReason`
  - `hasRegression` `Comparison.hasRegression`
  - `rejectionConditions` may include `regression detected: boot +40% >10%`
  - `limitations` `rebootMarker` if `reboot-pending` else none

Distinguish `EXPECTED` (`expectedBenefit`) from `OBSERVED` (`observedBenefit` + `metrics` deltas). Never claim `SUCCESS` merely because `APPLY` completed; `verdict` from `Comparison` is authoritative.

---

## 9. TRANSACTION EXPLANATION

`ExplanationEngine::explainTransaction(tx, profile, comparison)` covers lifecycle:

- `PREVIEWED` → `decision=PREVIEWED`, `whyNow` from `Recommendation`+`Profile`, `whatWillChange` from `tx.previews[0]`, `rejectionConditions` includes stale checks that would cause `FAILED`
- `APPROVAL_REQUIRED` → `authorizationRequired=true`, `rejectionConditions` includes `stale beforeHash` if would fail
- `APPROVED` → `rebootRequired` from `tx`
- `BACKUP_CREATED` → `rollbackSummary` from `tx.rollbackPlan` + `BackupEngine` path
- `APPLYING`/`APPLIED`/`VERIFYING`/`VERIFIED` → `beforeStateSummary`/`afterStateSummary` if available
- `COMPLETED` → `verdict` from `comparison` if present, else `COMPLETED` without `observedBenefit` (explain `limitations` `comparison unavailable`)
- `FAILED` → `decision=FAILED`, `reason` from `tx.error`/`validationResult`/`audit` `expected`/`observed`/`field`, `whatWillNotChange` (no mutation occurred), `rollbackSummary` (backup exists, `rollback` possible if `rollbackState=AVAILABLE`), `rejectionConditions` includes exact failure category (`stale beforeHash expected ... observed ...`, `TOCTOU symlink`, `backup unavailable`), deterministic `expected`/`observed` from `ValidationResult`.

Never expose `password`, `secret`, `argv`.

---

## 10. CLI

Extend `cli/p4_cli.cpp` unified CLI without new binary:

```
polaris_p4 explain <candidateId> [--json] [--verbose]
  → candidateId e.g., "akonadi-disable", "bluetooth-disable", "fstab-stale-swap"
  → builds Recommendation + Profile + Baseline mock or real, calls ExplanationEngine::explainCandidate, prints human or JSON

polaris_p4 transaction explain <transactionId> [--json] [--verbose]
  → loads transaction from /tmp/polaris-test-root/transactions/<id>.json or ~/.local/state/polaris/transactions/<id>.json, plus profile, plus comparison if present, calls explainTransaction, prints

--json → structured JSON via Explanation::toJson() (deterministic sorted keys)
--verbose → additional evidence list and rejectionConditions details, still deterministic, still no secrets
```

Human-readable `toHuman(verbose)` exposes:
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

JSON schema (deterministic):
```json
{"candidateId":"akonadi-disable","candidateKind":"RECOMMENDATION","decision":"BLOCKED","decisionLabel":"BLOCKED_BY_USER_WORKFLOW","whyNow":"...","evidence":["akonadi 1302M 14 agents"],"expectedBenefit":"~1.3GB RAM","confidence":0.65,"risk":"R2","reversibility":"High (akonadictl start)","rebootRequired":false,"authorizationRequired":true,"userImpact":"KMail would lose PIM","whatWillChange":"target=/... operation=...","whatWillNotChange":"NVIDIA 470xx remains claimed...","rejectionConditions":["profile: usesKMail=yes","stale beforeHash expected ..."],"dependencies":["akonadi running"],"rollbackSummary":"akonadictl start","beforeStateSummary":"...","afterStateSummary":"...","observedBenefit":"...","verdict":"SUCCESS","verdictReason":"...","hasRegression":false,"limitations":""}
```

Do not make parsers depend on human-readable formatting; JSON is canonical.

---

## 11. VERBOSE MODE

- Normal: concise `whyNow`, `whatWillChange`, `whatWillNotChange`, `expectedBenefit`, `confidence`, `risk`, `reboot`, `authorization`, `rejectionConditions` summary.
- Verbose: additionally `evidence` list (each `Recommendation.evidence` string), full `rejectionConditions` with `expected`/`observed` hashes, `dependencies`, `rollbackSummary` details, `beforeStateSummary`/`afterStateSummary` metrics, `observedBenefit`/`verdict` if available.

Verbose must NOT reveal `password`, `secret`, `argv` with shell metachars, `private credentials`. It remains deterministic (same verbose output for same structured input).

---

## 12. DETERMINISM

- Same `Recommendation` + `Profile` + `Comparison` + `Transaction` → same `Explanation` fields, same ordering (sorted `evidence`, sorted `rejectionConditions`), same JSON (keys alphabetical, no timestamps in decision, `id` deterministic `EXP-<candidateId>`).
- Avoid `unordered_map` in serialization; use `std::map` or sorted vector.
- No `rand`, no `chrono` in decision (timestamps only in `audit` metadata outside `Explanation`).
- `toJson` deterministic; `fromJson` strict.

---

## 13. SECURITY

Preserve `P4`/`P12`/`P14`/`P15` protections: no weakening of `FileSafety`, `ReadOnlyGuard`, `StateMachine`, `TransactionValidator`, `BackupEngine`, `AuditLog`, `IPC allowlist`, `SO_PEERCRED`, `TransactionLock`, `RecoveryDetector`. Explainability is read-only, no `FileSafety::atomicWrite` except via existing `ProfileStore`/`TransactionStore`; no shell execution (`sh -c` 0); no password collection; verbose does not leak `secret`; IPC remains `ping`/`info` only.

---

## 14. AUDIT

Explainability itself does not create misleading audit claims:
- `explain` command may audit `explanation.generated` with `candidateId`/`decision` (not `transaction.approved` or `applied`).
- Do not mark `explanation` as `applied` (`applied=false` in audit if any).
- `Recommendation` ≠ `approval`, `approval` ≠ `authorization`, `authorization` ≠ `application` - audit `operation` strings keep distinct (`explanation.generated` vs `transaction.approved` vs `authorization.granted` vs `apply.completed`).
- `AuditLog` hash chaining + `fsync` preserved.

---

## 15. Tests (20 cases, deterministic, isolated)

**File `tests/unit/test_p16_explanation_model.cpp`:**
1 model serialization deterministic (`toJson` same twice, `fromJson` round-trip, keys sorted)
2 deterministic ordering (evidence sorted, rejectionConditions sorted)

**File `tests/unit/test_p16_explain_candidate.cpp`:**
3 WHY NOW generation (evidence-backed, includes `ProfileAdvisor` `usesKMail` fact)
4 WHAT WILL CHANGE (target/operation/diff from `Transaction`/`Recommendation`)
5 WHAT WILL NOT CHANGE (explicit invariants, scope-aware)
6 rejection explanation (stale `beforeHash` `expected`/`observed`)
7 profile-blocked explanation (`usesKMail=yes` → `BLOCKED_BY_USER_WORKFLOW`, `whatWillNotChange` contains `Akonadi will remain`)
8 unknown profile explanation (`usesKMail=unknown` → `REQUIRES_USER_CONFIRMATION`)
9 verbose output (normal vs verbose evidence count)
10 JSON output (keys sorted, `candidateId`/`decision`/`whyNow` present)

**File `tests/unit/test_p16_explain_transaction.cpp`:**
11 expected vs observed benefit (`expectedBenefit` vs `Comparison.observedBenefit`, `verdict` `SUCCESS`/`REGRESSION`)
12 regression explanation (`hasRegression` true → `rejectionConditions` includes `regression: boot +40% >10%`)
13 transaction FAILED explanation (`FAILED: stale beforeHash` with `expected`/`observed`, `backupExists`, `rollbackSummary`)
14 rollback explanation (`BACKUP_CREATED` → `rollbackSummary` contains `fstab.bak`, `FAILED` preserves `rollback`)
15 completed transaction explanation (`COMPLETED` with `verdict` `SUCCESS` or `NO_CHANGE`, `limitations` if comparison unavailable)
16 stale-preview explanation (`approvedBeforeHash` vs `currentBeforeHash`)
17 authorization distinction (`authorizationRequired` vs `approvalState`, `explain` does not imply `APPROVAL`)

**File `tests/security/test_p16_verbose_redaction.cpp`:**
18 secret/password redaction (verbose must not contain `secret123`, `password`)
19 deterministic ordering (two `Explanation` same input → same JSON, same human)
20 no mutation during explanation (file `stat` mtime unchanged before/after `explainCandidate`/`explainTransaction`)

All fixtures `/tmp/polaris-test-root/p16` isolated, no real GPU/systemd/dnf/KDE.

---

## 16. Security Regression

Existing tests must continue to pass: `p4_security` 9/9, `p12_stale`/`p12_idempotency`/`p12_statemachine`, `p14_ipc_security`/`p14_auth`/`p14_socket_security`/`p14_lock`/`p14_recovery`, `p15_lifecycle`/`p15_stale_matrix`/`p15_toctou_idempotency`/`p15_lock_recovery`/`p15_regression_audit`. Do not weaken any assertion.

---

## 17. Implementation Files

- **New domain:** `core/explainability/Explanation.h/.cpp` (model, `toJson`/`fromJson`, `toHuman`, `isDeterministic`)
- **New engine:** `core/explainability/ExplanationEngine.h/.cpp` (`explainCandidate`, `explainTransaction`, `explainComparison`, helpers for `whyNow`, `whatWillChange`, `whatWillNotChange`, `rejectionConditions`)
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

