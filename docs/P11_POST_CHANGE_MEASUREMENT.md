# P11 - Post-Change Measurement & Regression Detection

**Phase:** P11 - Engineering, Not Host Optimization  
**Mode:** READ-ONLY PLANNING + IMPLEMENTATION - no `/etc` writes, no `systemd`, no `dnf`, no `akmods`, no `dracut`, no `sudo`, no helper, no reboot - only `core/` engineering, `tests/` fixtures in `/tmp/polaris-test-root`, `docs/` updates  
**Date:** 2026-09-01 01:20 +0330  
**Source:** `core/domain/Comparison.h` `core/engines/comparison/ComparisonEngine.*` `core/safety/transaction/Transaction.h` extended

---

## 1. Lifecycle

**Before:** `COLLECT → BASELINE → DETECT → CLASSIFY → EXPLAIN → RANK → PREVIEW → APPROVAL → AUTHORIZATION → BACKUP → APPLY → VERIFY`  
**After P11:** `... → APPLY → VERIFY → MEASURE AGAIN → LEARN FROM RESULT` - new `MEASURE AGAIN` is structured `Comparison`.

For every completed transaction:

- `beforeBaseline` captured at `BACKUP_CREATED` (timestamp `beforeTimestamp`)
- `change` applied
- `reboot/login` marker if `rebootRequired` or `loginMarker`
- `afterBaseline` captured at `VERIFYING` after reboot/login
- `delta` `pctDelta` `confidence` `regression` `threshold` stored with `Comparison`
- `expectedBenefit` vs `observedBenefit` compared
- `verdict` deterministic: `SUCCESS` `IMPROVED` `NO_CHANGE` `NO_BENEFIT` `REGRESSION` `INCONCLUSIVE`
- `audit` records `comparison.created` `comparison.verdict` `regression detected/not detected`

**Never claim** `operation completed successfully` == `optimization produced expected benefit`.

---

## 2. Metric Semantics

Initially support metrics already in `PerformanceBaseline`:

- `boot.userspace` (s, `systemd-analyze`, `isBootCritical true`)
- `memory.available` (GiB, `available memory`, `isHealth true`)
- `memory.swapUsed` (GiB, background, not health)
- `thermal.cpuMax` (°C, `isHealth true`)
- `systemd.failedCount` (count, `isHealth true`, `new_failed`)
- `nvidia.claimed` (0/1, `isHealth true`, custom `nvidia_claimed` threshold: `after < before` is regression)

Unavailable metrics represented as `available false`, `note "unavailable: ... not collected"`, not guessed, `delta` `nullopt`, `regression false`.

Distinguish:
- **Boot-critical:** `boot.userspace` (threshold `> +10% relative` → regression)
- **Background/runtime:** `memory.swapUsed` (informational, not health)
- **Resource:** `memory.available` (threshold `> 1 GiB decrease` → regression), `thermal` (`> +15°C`)
- **Health/regression:** `failedCount` (`any new failed unit` → regression), `nvidia.claimed` (decrease is regression)

Do not treat `systemd-analyze blame` alone as proof of benefit - `ComparisonEngine` uses `before` vs `after` measured, not `blame` number.

---

## 3. Regression Thresholds (Stored with Result, Explainable)

Initial acceptance thresholds (`ComparisonEngine::defaultThresholds()`):

- **Boot time regression:** `> +10%` relative (`thresholdType: relative_pct`, `thresholdValue: 10.0`) - e.g., `50s → 55s` (+10% exactly) is **not** regression, `55.5s` (+11%) is regression. Avoids false positives from floating noise (`abs(delta) < 1e-6` → not regression).
- **Available memory regression:** `> 1 GiB` decrease (`absolute_gb`, `delta < -1.0` → regression) - e.g., `8 GiB → 6.5 GiB` (-1.5GB) is regression.
- **Thermal regression:** `> +15°C` (`absolute_c`, `delta > 15`) - e.g., `50°C → 70°C` (+20°C) is regression.
- **Failed-unit regression:** `any new failed unit` (`new_failed`, `delta > 0` → regression) - e.g., `0 → 1` is regression.
- **Transaction-specific:** `nvidia.claimed` decrease `1 → 0` is regression (custom `nvidia_claimed` type).

Threshold **stored** in `MetricComparison.thresholdDesc` + `thresholdValue` + `thresholdType` so result remains explainable, not hard-coded in CLI.

Do not hard-code verdict in CLI - `ComparisonEngine` owns logic.

**Fail-closed:** If any `isHealth` or `isBootCritical` metric indicates `regression`, overall `verdict` must not be `SUCCESS` - it becomes `REGRESSION`.

---

## 4. Expected vs Observed Benefit

Support `expectedBenefit` (string, from `Recommendation`) and `observedBenefit` (string, from `Comparison` + `metrics`).

Example P7:
- **Expected:** `restore NVIDIA Maxwell support and PRIME offload`
- **Observed:** `MX130 claimed, nvidia module loaded, nvidia-smi successful, PRIME offload successful` (derived from `nvidia.claimed 0→1` + no regression)
- **Verdict:** `SUCCESS` (observed matches expected, no regression)
- **If mismatch:** `expected: restore NVIDIA` but `observed: nvidia still not claimed` → `verdict: NO_BENEFIT` or `REGRESSION` if new failed.

Separate:
- `operation succeeded` (`APPLIED`)
- `verification succeeded` (`VERIFYING` → `VERIFIED` via file hash, lsmod)
- `expected benefit observed` (`observedBenefit` matches `expectedBenefit`)
- `regression detected` (`hasRegression` true)

Never claim `optimization succeeded` merely because `command completed`.

---

## 5. Transaction Integration

Extended `core/safety/transaction/Transaction.h` with `std::optional<PerformanceBaseline> beforeBaseline`, `afterBaseline`, `Comparison comparison` - **backward compatible**: old JSON without these fields still loads (optional, `has_value()` false → `available false`).

For new completed transactions: `beforeBaseline` + `afterBaseline` + `comparison` must be persisted (in `~/.local/state/polaris/transactions/<id>.json`).

**Reboot-pending:** Framework supports `rebootMarker` (`none` vs `rebooted-<timestamp>`) and `loginMarker`. `comparison` is generated **only after** required post-change measurements are available - **do not fake after measurements**. For `rebootRequired` (e.g., NVIDIA), `afterBaseline` is captured **after reboot** via `polaris_p7` post-reboot verification (already manual, now via `ComparisonEngine`). For `loginRequired` (e.g., `nvidia-settings` autostart), `afterBaseline` after next login.

---

## 6. CLI

Extended `polaris_p4` (now `P11`):

```
polaris transaction show <id> --json
  → exposes beforeBaseline, afterBaseline, comparison, expectedBenefit, observedBenefit, regression, verdict (structured JSON from domain model, not ad-hoc strings)

polaris transaction compare <id> [--json]
  → shows beforeBaseline/afterBaseline/comparison if present, else "not yet available - reboot-pending"
```

Prefer structured JSON from `domain::Comparison` rather than ad-hoc CLI strings. No new standalone binary (as required).

---

## 7. P7 Fixture (Deterministic)

**Before:** `boot/userspace 54.106s`, `available memory 4.2 GiB`, `swap used 1.6 GiB`, `thermal 67°C`, `failed 1`, `nvidia claimed 0`

**After:** `boot/userspace 54.106s` (unchanged, not an optimization benefit), `available 6.5 GiB`, `swap 0 GiB`, `thermal 50°C`, `failed 0`, `nvidia claimed 1`

**Expected:** `restore NVIDIA Maxwell support and PRIME offload`

**Comparison correctly concludes:**
- `boot regression false` (0% delta, < +10%)
- `memory regression false` (available increase, not decrease)
- `thermal regression false` (decrease, not increase)
- `health regression false` (failed 1→0, not new)
- `observed NVIDIA functional benefit achieved` (claimed 0→1)
- **Overall verdict `SUCCESS` / `IMPROVED`** (per `toString` `SUCCESS`)

**Do not claim** unchanged boot time itself is benefit - correctly **not** claimed.

---

## 8. Regression Fixtures (Deterministic)

- **Boot 50s → 70s (+40%, >10%)** → `regression true`
- **Available 8 GiB → 6.5 GiB (-1.5 GiB, >1GB)** → `regression true`
- **Thermal 50°C → 70°C (+20°C >15°C)** → `regression true`
- **Failed 0 → 1 (new failed unit)** → `regression true`

Also tested:
- **Unavailable metric:** `boot 0→0` → `available false`, `note unavailable`, `regression false`
- **Zero-before:** `boot 0→10s` → `pct 100%` → `regression true` (100% >10%)
- **Threshold boundary:** `50→55` (+10% exactly) → `regression false` (needs >10%)
- **Just above:** `50→55.5` (+11%) → `regression true`
- **New failed unit:** `0→1` → `regression true`
- **Expected vs observed:** `expected restore NVIDIA` `observed MX130 claimed` → `SUCCESS`; `expected restore` but `observed no change` → `NO_CHANGE`/`NO_BENEFIT`; `operation succeeded but regression` → `REGRESSION`

---

## 9. Limitations

- **Unavailable metrics:** Represented explicitly as `available false` with `note`, not guessed. If `systemd.userspace` not collected (0), `boot` is unavailable, not 0.
- **Reboot-pending:** Framework supports `rebootMarker` but P11 does not auto-capture after reboot - user must run `polaris transaction compare` after reboot to generate `afterBaseline`.
- **Boot vs login:** Distinguished via `isBootCritical` flag, but `login` time (user systemd 596ms) is not yet in `PerformanceBaseline` (only `systemd.userspace` system). Future P12 could add `userSystemd` metric.

---

## 10. Security / Safety

- `ComparisonEngine` is **pure** / side-effect-free - does not execute commands, only compares two `PerformanceBaseline` structs.
- `ReadOnlyGuard` remains intact, `FileSafety` intact, `StateMachine` fail-closed intact, no shell execution, no password collection, no `sudo`, no `Polkit`, no real-host writes, no reboot, no approval bypass - all verified via `test_readonly` and `test_p4_security`.
- Providers remain responsible for measurement collection, `ComparisonEngine` only compares.

---

## 11. Limitations Statement

> "operation completed successfully" != "optimization produced the expected benefit"

P11 enforces this by separating `executionState: APPLIED` vs `verificationState: VERIFIED` vs `comparison.verdict: SUCCESS` vs `regression`.

---

## 12. P11 Example

See `tests/unit/test_comparison.cpp` P7 fixture and `tests/integration/test_observed_benefit.cpp` expected vs observed.

---

## 13. Next Steps

See `docs/ROADMAP.md` P11 as next engineering phase (now implemented), next would be P12 Stale-Preview etc.
