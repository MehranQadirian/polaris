# P10 Plan - Polaris Maturity: From Prototype to Human-in-the-Loop System Optimizer

**Phase:** P10 - Software Engineering, Not Host Modification  
**Mode:** READ-ONLY PLANNING - no `dnf`, no `systemctl`, no `modprobe`, no `dracut`, no `sudo`, no Polkit, no helper install, no reboot - only inspect existing implementation, design, document  
**Date:** 2026-09-01 01:00 +0330  
**Source:** Inspect `~/Documents/lin-opt` current implementation (P1-P9) - `core/`, `cli/`, `tests/`, `docs/`, `polkit/`, `packaging/`  
**Artifacts:** This `docs/P10_PLAN.md`, updated `docs/ROADMAP.md`, `docs/ARCHITECTURE.md` gap analysis

---

## 1. What Already Exists (P1-P9, Verified)

- **Core domain models** `core/domain/*` `SystemInfo`, `HardwareInfo`, `Health`, `PerfModels`, `Transaction` - versioned, JSON-serializable, independent of CLI/GUI.
- **Providers (real, read-only, no shell injection):** `RealOsProvider` `/etc/os-release`+`uname`, `RealCpuProvider` `/proc/cpuinfo`+`sysfs`, `RealMemoryProvider` `/proc/meminfo`+`pressure`+`zram`, `RealStorageProvider` `/proc/mounts`+`statvfs`+`/sys/block`, `RealGpuProvider` `/sys/bus/pci`+`pci.ids`+`glxinfo` (fixed `/usr/bin/glxinfo` `DISPLAY=:0`), `RealThermalProvider` `hwmon`+`thermal_zone`, `RealSystemdProvider` `systemctl`/`systemd-analyze` via `execv` separate args `poll` timeout, `RealKdeProvider` env+`kwinrc`+`plasmashell --version`, `RealProcessProvider` `/proc`, `RealJournalProvider` `journalctl` - all with `FileSafety` allowlist, `ReadOnlyGuard` `kReadOnlyMode true`.
- **Engines:** `BaselineEngine` (3812ms collect, 15 metrics with `MetricMeta` timestamp/unit/source/method/confidence), `BottleneckEngine` (10 bottlenecks, multi-evidence, `critical-chain` BLOCKER vs background), `BenchmarkEngine` (quick/normal/deep, `min/max/avg/median/stddev`, cancellable, `expectedLoad`), `RecommendationEngine` (7 recs with `evidence/confidence/benefit/risk/rollback/reboot/auth`), `Health` (explainable, not arbitrary 82).
- **Safety:** `StateMachine` 16 states `PROPOSED→PREVIEWED→APPROVAL_REQUIRED→APPROVED→AUTHORIZATION_REQUIRED→AUTHORIZED→BACKUP_CREATED→APPLYING→APPLIED→VERIFYING→VERIFIED→COMPLETED` with `isValidTransition` fail-closed, `FileSafety` allowlist + `canonical` + `isSymlink` + `validatePath` (reject `..`, `;|&`, `NUL`, `>4096`), `BackupEngine` versioned `~/.local/state/polaris/backups/<tx>/` `SHA-256` no overwrite, `AuditLog` hash chaining `previousHash`+`eventHash` via `openssl/sha.h`.
- **CLI:** `polaris` (mock), `polaris_real` (P2), `polaris_p3` (P3 `performance baseline/benchmark`, `analyze`, `bottlenecks`, `recommendations` `--json/--human`), `polaris_p4` (P4 `transaction preview/list/show/approve`, `audit list`, `apply --dry-run` on `/tmp/polaris-test-root`), `polaris_p5` (P5 pilot `preview/approve/apply` for `~/.config/autostart/nvidia-settings-user.desktop` with 9 precondition checks).
- **Tests:** `unit` (FakeProviders), `real_providers` (OS/CPU/mem/fs/block/thermal/gpu), `parsers` (os-release/meminfo/boot), `readonly` (stat mtime unchanged), `p4_security` 9 checks (traversal, symlink, metachars, invalid transition, replay, backup no overwrite, oversized, fake op, audit chain) - **100% 5/5 0.05s**.
- **Docs:** `README.md`, `ARCHITECTURE.md`, `API.md` (`/api/v1` UDS 0600), `SECURITY.md` (no password storage, Polkit `auth_admin_keep`), `HCI.md` (progressive disclosure), `THREAT_MODEL.md`, `DEVELOPMENT.md`, `TRANSACTION_MODEL.md`, `POLKIT.md`, `ROLLBACK.md`, `AUDIT.md`, `P2_REPORT.md` 24K, `P3_REPORT.md` 33K, `P4_REPORT.md` 18K, `P5_REPORT.md` 13K, `P6_REPORT.md` 15K+addendum, `NVIDIA_PREFLIGHT_REPORT.md` 20K, `P7_PRE_REBOOT_REPORT.md`, `P7_POST_REBOOT_REPORT.md`, `P8_REPORT.md` 18K, `P9_REPORT.md` 15K - all with evidence, confidence, rollback.
- **Packaging:** `CMakeLists.txt` `C++20` `-Wall -Wextra -Wpedantic -Werror -Wno-error=deprecated-declarations` `crypto` linked, `polkit/org.polaris.*.policy` 3 actions, `packaging/polaris.spec`.

---

## 2. What Is Missing (Gap Analysis)

| Area | Current | Missing / Fragile | Priority |
|------|---------|-------------------|----------|
| **Post-Change Measurement** | `BaselineEngine` collects before, but P7 post-reboot verification was manual `bash` (`nvidia-smi`, `journalctl`, `free`), not via `BaselineEngine` delta with `expected vs observed` structured comparison. | Structured `PostChangeMeasurement` with `beforeBaseline`, `afterBaseline`, `delta`, `confidence`, `expectedBenefit`, `observedBenefit`, `regression detection` (boot/login/memory/CPU/thermal/GPU/display/network/failed/journal). No `Comparison` model yet. | **High** |
| **Optimization Score** | `RecommendationEngine` generates 7 recs but ranking is manual in `p8_analysis.json`/`p9_analysis.json`, no transparent `score = f(benefit, confidence, risk, reversibility, user-impact, reboot, auth, evidence)` with explainability. | Explainable `OptimizationScore` model, not opaque ML, with `score` + `contributors` traceable. | **High** |
| **Regression Detection** | P7 post-reboot checked 15 items manually, but no `RegressionEngine` with thresholds relative to baseline (e.g., `boot +10%`, `memory -1GB`, `thermal +15C`, `failed +1`). | Read-only `RegressionEngine` that compares `after` vs `before` with `MetricMeta` confidence, flags `regression` vs `improvement`. | **High** |
| **Transaction Idempotency** | `FileSafety::atomicWrite` is idempotent for `Hidden=true`, but no explicit `idempotent` flag per operation. `akmod` and `dnf` are not idempotent. | Every `ChangePreview` should declare `idempotent: true/false/conditional` + `repeated execution` test. | **Medium** |
| **Transaction Dependencies** | No explicit `dependsOn` (e.g., `NVIDIA migration → reboot → verification → PRIME validation`). P7 had `READY_FOR_REBOOT` state but not formal `dependsOn` graph. | `Transaction.dependencies: [TX-...]` + `dependsOn` check `stalePreview` if dependency state changed. | **Medium** |
| **Stale-Preview Protection** | `polaris_p5` checks `beforeHash` for `nvidia-settings` but not for `mssql`/`nvidia` package state, unit hash, kernel version, transaction age. `StateMachine` validates transitions but not `target hash` staleness. | Track `targetHash`, `packageState`, `unitState`, `kernelVersion`, `timestamp/age`, `dependencyState` in `Transaction`, `isStale()` check, `approval` invalidated if stale. | **High** |
| **User Workflow / Profile** | No interview layer. P8/P9 required manual user confirmation for `akonadi` (user uses KMail) via `question` tool, but not persisted as auditable profile (`uses KMail?`, `uses Bluetooth?`, `uses printing?`, `uses external monitors?`, `uses VPN?`, `uses Docker?`, `uses local DB?`, `uses virtualization?`). | Safe `UserProfile` layer: `usesKMail: bool` `usesBluetooth: bool` etc., stored `~/.local/state/polaris/profile.json` auditable, reversible, **never silently authorizes** - only constrains recommendations, not authorizations. | **Medium** |
| **Explainability** | P3/P4 `bottleneck` has `evidence/confidence/impact`, `recommendation` has `why/alternative/rollback`, but no `WHY NOW?`, `WHAT WILL NOT CHANGE?`, `WHAT WOULD MAKE US REJECT IT?` as required in P10 §8. | Enhance `Recommendation` with `whyNow`, `whatWillNotChange`, `whatWouldMakeUsReject`, plus CLI `--verbose` progressive disclosure. | **Medium** |
| **Security Hardening** | `FileSafety` covers path allowlist, traversal, metachars, symlink, NUL, oversized, but not fully `TOCTOU`, `Bounded output/time`, `IPC SO_PEERCRED`, `helper` not yet installed, `audit` not `fsync` per event, `transaction lock` file `/run/polaris/transaction.lock` not yet implemented (only checked for existence). | Threat-model every new privileged op (currently only `mssql` disable and `nvidia` swap), implement `helper` minimal, `IPC` D-Bus `SO_PEERCRED`, `audit fsync`, `lock` via `flock`, `crash recovery` via `transactions/<id>.json` + `recover` command. | **High** |
| **Testing** | Unit/integration/security: 5 tests, but no `transaction state tests`, `rollback tests`, `crash/recovery tests`, `stale-preview tests`, `concurrent tests`, `idempotency tests`, `regression tests`, `fixture-based real-provider tests` with isolated fixtures. | Expand to 15+ tests covering state machine, rollback, crash, stale, concurrent, idempotency, regression - all on `/tmp/polaris-test-root` fixtures unless explicitly approved real-host. | **High** |
| **Observability** | `audit.log` hash chaining exists, but no structured `events` for `transaction.created/previewed/approved/...` consumable by Qt/QML, no `GET /api/v1/transactions/{id}/events` SSE. | Structured `JobEvent` `STARTED/PROGRESS/WARNING/ERROR/COMPLETED` with `progress` % and `currentPhase` for GUI. | **Medium** |
| **Dry-Run / Simulation** | `polaris_p4 apply --dry-run` shows `target/diff/privilege/risk/rollback` but not `BEFORE → PROPOSED AFTER → DIFF → COMMAND/OPERATION CLASS` fully. | Stronger `DryRun` layer: `polaris transaction preview` should show `BEFORE` `PROPOSED AFTER` `DIFF` `COMMAND CLASS` `PRIVILEGE` `RISK` `ROLLBACK` with **no hidden mutation** during preview (verified via `test_readonly`). | **Medium** |
| **Rollback Verification** | Stores `rollbackPlan` string, but not `rollback preconditions/transaction/verification` with fixture test. P5 rollback tested via `/tmp/polaris-test-p5` copy, but not formalized. | For high-risk R3, define `rollbackPreconditions`, `rollbackTransaction`, `rollbackVerification`, `post-rollback health check` and verify via fixtures (as done for P5 test copy, but formalized). | **Medium** |
| **Provider/Engine Separation** | Providers collect facts, engines analyze, but `RealSystemdProvider` still uses `execv` `systemctl` `systemd-analyze` instead of `sd-bus` `org.freedesktop.systemd1` native. | Migrate `RealSystemdProvider` to `sd-bus` D-Bus native for `blame`, `critical-chain`, `failed`, not just safe exec fallback. Keep `exec` as fallback for P2, but primary should be native. | **Low** (works, but less native) |
| **Documentation Gap** | `docs/P10` missing, `ARCHITECTURE.md` has P4 safety layer but not `POST-CHANGE MEASUREMENT` lifecycle `COLLECT→BASELINE→DETECT→CLASSIFY→EXPLAIN→RANK→PREVIEW→APPROVAL→AUTHORIZATION→BACKUP→APPLY→VERIFY→MEASURE AGAIN→LEARN`. | Update `ARCHITECTURE.md` with P10 lifecycle diagram, `API.md` with `POST /api/v1/performance/baseline` etc., `HCI.md` with `WHY NOW` etc., plus new `docs/P10_PLAN.md`. | **Medium** |

**What Is Fragile:**
- `RealSystemdProvider` `safeExec` with `poll` timeout 5-10s may truncate large `journalctl` (P3 had `-n 500` limit to avoid, but full `wc -l` would need 10s). Should use `sd_journal` native.
- `RealGpuProvider` `glxinfo` requires `DISPLAY=:0` setenv hack, fragile headless - should use `libEGL` native query.
- `BenchmarkEngine` `statvfs` is read-only but `cpu_prime` is synthetic, not real workload - should be documented as micro-benchmark, not system load.

**What Is Redundant:**
- `polaris` (mock) + `polaris_real` + `polaris_p3` + `polaris_p4` + `polaris_p5` - 5 binaries for CLI, should be unified `polaris` with subcommands `scan`, `performance`, `transaction`, `audit` via single entry.

**What Should NOT Be Changed:**
- `ReadOnlyGuard` + `FileSafety` allowlist + `StateMachine` fail-closed - core safety, not to be weakened.
- `core` no Qt - keep headless, GUI as client.
- Offline-first, no telemetry - keep.
- `C++20` + `CMake` + `crypto` - keep, incremental.

---

## 3. Ranked Engineering Tasks (By Value & Dependency)

| Rank | Task | Value | Dependency | Effort |
|------|------|-------|------------|--------|
| 1 | **Post-Change Measurement + Regression Detection** (before baseline + after baseline + delta + expected vs observed + regression thresholds relative to baseline) | **High** - every completed transaction currently lacks structured `observedBenefit` vs `expectedBenefit`, and no `regression` flag (e.g., boot +10% would not be flagged). Directly addresses P10 §1 and §3, dependency for all future optimizations. | Depends on `BaselineEngine` (done) + `BenchmarkEngine` (done) | Medium |
| 2 | **Stale-Preview Protection + Transaction Idempotency** (track `targetHash`, `packageState`, `unitState`, `kernelVersion`, `timestamp`, `dependencyState`, `isStale()`; declare `idempotent` per operation) | **High** - prevents `APPROVED` stale preview from being applied after `beforeHash` changed (P5 `nvidia-settings` had `beforeHash` check, but `mssql` and `nvidia` did not fully track `unitHash` staleness). Fail-closed safety. | Depends on `StateMachine` | Medium |
| 3 | **Optimization Score (Explainable)** (`score = f(benefit, confidence, risk, reversibility, user-impact, reboot, auth, evidenceQuality)` with `score` + `contributors` traceable, not opaque ML) | **High** - current ranking in `p8_analysis.json` is manual, not transparent. Needed for GUI `RANK` → `PREVIEW` flow. | Depends on `RecommendationEngine` | Medium |
| 4 | **Security Hardening + Helper IPC** (path allowlist already, but add `TOCTOU` `canonical` check, `bounded output/time`, `IPC SO_PEERCRED`, `helper` minimal with `FileSafety`, `audit fsync`, `transaction lock` `flock`, `crash recovery` `recover` command) | **High** - `FileSafety` covers 9 checks but not `TOCTOU` full, `lock` file not implemented, `audit` not `fsync`, `helper` not installed. | Depends on `FileSafety` | High |
| 5 | **Testing Expansion** (15+ tests: `transaction state`, `rollback`, `crash/recovery`, `stale-preview`, `concurrent`, `idempotency`, `regression`, `fixture-based real-provider`) | **High** - currently 5 tests, need coverage for P4/P5/P7 state machines. | Depends on all | High |
| 6 | **User Workflow / Profile Engine** (`usesKMail`, `usesBluetooth`, etc., stored `~/.local/state/polaris/profile.json` auditable, reversible, never silently authorizes, only constrains recommendations) | **Medium** - would have raised `akonadi` confidence from 0.65 to 0.90 if profile `usesKMail=false` persisted, but requires UI. | Depends on `RecommendationEngine` | Medium |
| 7 | **Explainability Enhancement** (`WHY NOW`, `WHAT WILL NOT CHANGE`, `WHAT WOULD MAKE US REJECT` per recommendation, plus `WHY THIS` already, and `--verbose` progressive disclosure) | **Medium** - P3/P4 already have `WHY`, `WHAT WILL CHANGE`, `BENEFIT`, `RISK`, `ROLLBACK`, but missing `WHY NOW` and `WHAT WOULD MAKE US REJECT`. | Depends on `RecommendationEngine` | Low |
| 8 | **Observability + Audit Structured Events** (`transaction.created` → `COMPLETED` with `progress` % and `currentPhase` for Qt/QML `GET /api/v1/transactions/{id}/events` SSE) | **Medium** - `audit.log` has hash chaining but not structured `JobEvent` `STARTED/PROGRESS`. | Depends on `AuditLog` | Medium |
| 9 | **Dry-Run / Simulation Stronger** (`polaris transaction preview` should show `BEFORE` `PROPOSED AFTER` `DIFF` `COMMAND CLASS` `PRIVILEGE` `RISK` `ROLLBACK` with no hidden mutation, verified via `test_readonly`) | **Medium** - P4 `apply --dry-run` shows `target/diff/privilege` but not `BEFORE` `PROPOSED AFTER` fully. | Depends on `Transaction` | Low |
| 10 | **Provider/Engine Separation + D-Bus Migration** (`RealSystemdProvider` to `sd-bus` `org.freedesktop.systemd1` native, `RealGpuProvider` to `libEGL` native, keep `exec` fallback) | **Low** - works but less native, P2 had safe exec, native would be more robust. | Depends on `providers` | High |

**Do not assume all must be implemented.** Rank by engineering value and dependency - above ranking already prioritized.

---

## 4. Selected ONE Highest-Value Engineering Task

**Selected:** **#1 Post-Change Measurement + Regression Detection**

**Why this is highest-value:**
- **Directly addresses P10 §1 and §3** - the complete Polaris lifecycle `COLLECT → BASELINE → DETECT → CLASSIFY → EXPLAIN → RANK → PREVIEW → APPROVAL → AUTHORIZATION → BACKUP → APPLY → VERIFY → MEASURE AGAIN → LEARN FROM RESULT` is currently broken at `MEASURE AGAIN` and `LEARN`. P7 post-reboot verification was manual `bash` (`nvidia-smi`, `journalctl`, `free`, `sensors`) not via `BaselineEngine` delta. Every future transaction (including P5 `nvidia-settings` 2.56s, P6 `mssql` 713M, P7 `nvidia` 470xx) needs structured `observedBenefit` vs `expectedBenefit` to avoid claiming `command succeeded` as `optimization worked`.
- **Dependency for all future optimizations:** `Optimization Score` (#3) needs `observedBenefit` to learn, `Regression Detection` (#3) needs `delta` thresholds, `User Workflow` needs `LEARN` to adjust confidence.
- **Safety:** Detects `boot regression` `+10%` (e.g., if 470xx had caused boot 54s → 60s), `memory regression` (if akonadi disable had leaked), `thermal` `+15C`, `GPU` `nvidia-smi` fails again, `display` lost, `failed` +1 - **fail-closed** if regression.
- **Incremental over rewrite:** Builds on `BaselineEngine` (already collects `PerformanceBaseline` with `MetricMeta`) and `BenchmarkEngine` (already `min/max/avg/median/stddev`), just adds `Comparison` model and `RegressionEngine`.

**What should NOT be changed:** Keep `BaselineEngine` collection logic, keep `ReadOnlyGuard`, keep `FileSafety`, not rewrite `StateMachine`.

---

## 5. Implementation Plan (For Selected Task #1)

**Model (new `core/domain/Comparison.h`):**
```cpp
struct Comparison {
  PerformanceBaseline before, after;
  struct Delta { std::string metric; double before, after, delta, pct; double confidence; bool regression; std::string threshold; };
  std::vector<Delta> deltas; // e.g., userspace 54.106 → 8.515 delta -45.591 -84% threshold boot +10% => no regression
  double expectedBenefit; // e.g., 2.56s for nvidia-settings
  double observedBenefit; // e.g., measured after login 0s vs 2.56s
  std::string verdict; // "benefit observed", "no benefit", "regression"
  BenchmarkResult beforeBench, afterBench;
};
```

**Engine (`core/engines/comparison/ComparisonEngine.h`):**
- `compare(before: PerformanceBaseline, after: PerformanceBaseline, expected: Recommendation) -> Comparison`
- For each metric: `delta = after - before`, `pct = delta/before*100`, `regression = (metric == boot && pct > +10%) || (memory avail < -1GB) || (thermal > +15C) || (failed +1) || (journal p3 +20) || (nvidia-smi fails)` - thresholds **relative to baseline**, not global `85C` (as required).
- `observedBenefit` from `after` vs `before` (e `free -h` `available` 4.2GB → 6.5Gi = +2.3GB, but `swap 1.6GB → 0` = -1.6GB, `thermal 67→50` = -17C).

**Integration:**
- `Transaction` gets `beforeBaseline` (captured at `BACKUP_CREATED`) + `afterBaseline` (captured at `VERIFYING` after `APPLY` + reboot/login marker if `rebootRequired` or `loginRequired`).
- For `rebootRequired` (e.g., NVIDIA), `afterBaseline` is captured **after reboot** via `polaris_p7` post-reboot verification (already manual, now via `ComparisonEngine`).
- For `loginRequired` (e.g., nvidia-settings autostart), `afterBaseline` captured after next login.

**Tests:**
- `test_comparison` - `before` 54.106s `after` 8.515s → `delta -45.591 -84%` `regression false` (threshold +10%)
- `test_regression` - `before thermal 50C` `after 70C` delta `+20C` `pct +40%` threshold `+15C` → `regression true`
- `test_observed_vs_expected` - `expected 2.56s` `observed 0s` → `verdict benefit observed`, `expected 2.56s` `observed 2.56s` → `no benefit` → suggest rollback.

**Acceptance Criteria:**
- Every completed transaction in `~/.local/state/polaris/transactions/*.json` has `beforeBaseline` + `afterBaseline` + `deltas` + `observedBenefit` vs `expectedBenefit` + `regression` flag - **not** just `command succeeded`.
- `polaris transaction show <id> --json` includes `comparison` field.
- `docs/P10` example shows `P7` `before userspace 54.106s` `after 8.515s` `delta -84%` `expectedBenefit` `restore PRIME` `observedBenefit` `nvidia-smi works` `regression false`.
- Read-only `RegressionEngine` never writes, only reads baselines.

**Effort:** Medium (2-3 files + tests), incremental.

---

## 6. Tests for Selected Task

- `unit/test_comparison` - delta + regression thresholds.
- `integration/test_post_change` - fixture `before.json` `after.json` with `userspace` `54.106` → `8.515` → `PASS`.
- `integration/test_regression` - `thermal 50` → `70` → `regression true`.
- `integration/test_observed_benefit` - `expected 2.56s` `observed 0s` → `benefit observed`.
- All mutation tests still on `/tmp/polaris-test-root` fixtures unless explicitly approved real-host transaction (as in P5/P7).

---

## 7. Acceptance Criteria (For This Task)

- `core/domain/Comparison.h` exists, `ComparisonEngine` compares two `PerformanceBaseline` with `MetricMeta` confidence.
- `Transaction` has `beforeBaseline` and `afterBaseline` fields (or `comparison`).
- `polaris transaction show <id> --json` includes `observedBenefit` vs `expectedBenefit`.
- `docs/P10` includes example `P7` delta `-84%` and `regression false`.
- `ctest` 6+ tests pass (existing 5 + new 3).
- `docs/ROADMAP.md` updated with P10 task #1 as highest-value.

---

## 8. ROADMAP Update (Proposed Future Phases Ranked)

- **P11 - Regression Detection & Post-Change Measurement** (this task #1, **highest-value, do next**) - dependency for all future optimizations.
- **P12 - Transaction/State-Machine Hardening** (task #2 Stale-Preview + #4 Security Hardening) - second, fail-closed safety.
- **P13 - User Workflow/Profile Engine** (task #6) - third, would have raised `akonadi` confidence from 0.65 to 0.90 if `usesKMail=false` persisted.
- **P14 - Expanded Security & IPC/Helper** (task #4 helper, #5 testing) - fourth, needed before more privileged ops.
- **P15 - Test/CI/Fixture Expansion** (task #5) - fifth, 15+ tests.
- **P16 - Explainability & HCI** (task #7 + #8 + #9) - sixth, `WHY NOW` etc.
- **P17 - Optimization Campaign 2** (revisit P8 `CAND-SERVICES` with `UserProfile` `usesBluetooth=false` etc.) - seventh, after profile.
- **P18 - Final Benchmark / ROI / Stability Report** - last, with `observedBenefit` for all.

**Do not assume all must be implemented.** Rank above is by engineering value and dependency - **P11 is next, P12 second**.

---

## 9. What Should NOT Be Changed

- `ReadOnlyGuard` + `FileSafety` allowlist - core safety, not to be weakened for P10.
- `core` no Qt - keep headless, GUI as client.
- Offline-first, no telemetry - keep.
- `StateMachine` 16 states - keep, just add `Comparison` fields, not rewrite.
- `BaselineEngine` collection logic - keep, just add `ComparisonEngine` on top.

---

## 10. Do NOT Implement Unrelated Tasks in P10 Planning Phase

- Do **not** create `comparison` code yet - **this plan is planning only**, as per P10 `STOP and report the P10 plan before implementing the next engineering task`.
- Do **not** modify host (`/etc`, `systemd`, `dnf`, etc.) - planning only, read-only inspection of `~/Documents/lin-opt` existing implementation.

---

## 11. STOP and Report

**STOP** after this plan - **do not** implement the next engineering task (Post-Change Measurement) until explicit approval. The plan above is **ONE highest-value engineering task** (#1) with `implementation plan` `tests` `acceptance criteria`.

**Artifacts:** This `docs/P10_PLAN.md`, updated `docs/ROADMAP.md` (proposed ranking), `docs/ARCHITECTURE.md` gap analysis (next update).

**Next Approval Requested:** Approve **P11 - Regression Detection & Post-Change Measurement** (task #1) as the next engineering task, or select a different ranked task.

