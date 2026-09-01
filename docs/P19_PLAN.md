# P19 - Optimization Capability Framework - Plan

**Phase:** P19 - Engineering (Framework, Not Tweaks)  
**Date:** 2026-09-01  
**Source:** `docs/OPTIMIZER_GAP_ANALYSIS.md` - conclusion: safety framework complete (P4–P18), optimizer catalog exhausted, RecommendationEngine hard-coded 8 recs, no registry  
**Mode:** READ-ONLY design + isolated-fixture implementation - no privileged host mutation, no `/run/polaris/helper.sock`, no `~/.local/state/polaris/profile.json` write, no `dnf`/`systemctl`/`reboot`, all under `/tmp/polaris-test-root/p19_*`  
**Objective:** Transform Polaris from “safe transaction framework with a few hard-coded recommendations” into “safe, evidence-backed, extensible optimization engine where capabilities are registered, measured, explained, previewed, approved, applied, verified and evaluated.”

---

## 1. Gap Being Closed

`docs/OPTIMIZER_GAP_ANALYSIS.md:5` audit:

- `RecommendationEngine` (`core/engines/recommend/RecommendationEngine.cpp:1`) is `if(bn.id==GPU-001)` mapper with 8 hard-coded recs, stringly-typed `expectedBenefit` (`"Save 713M"`), literal `0.96f`, no numeric benefit, no scoring.
- `BottleneckEngine` emits 10 types, but only 4 map to recs; `RecommendationEngine` ignores `PerformanceBaseline b` (`void(b)`).
- No `IOptimizationCapability` interface, no `OptimizationRegistry`, no `RealFlatpakProvider`/`RealJournalDiskProvider`, no numeric `benefitGB` in `PerformanceBaseline`.
- `TransactionManager` stub (`core/safety/transaction/TransactionManager.cpp:1`), `IpcProtocol` `ping/info` only - real-host apply disabled (intentional).
- Result: after P6 (`mssql` 713M) and P7 (`nvidia` 470xx) - `P17 NO_ACTION_RECOMMENDED` correct but host idle; framework safe but not useful on generic Fedora/KDE.

**P19 does not add “100 tweaks.”** It adds **extensibility** - a registry where adding a new capability is `implement + register + test`, not editing a giant conditional.

---

## 2. Objectives (Fail-Closed)

1. **Capability abstraction** - `IOptimizationCapability` with deterministic metadata (`id`, `name`, `category`, `risk`, `reversibility`, `requiresReboot/Auth`, `isApplicable`, `collect`, `toRecommendation`, `snapshot`, `toTransaction`, `verify`, `explain*`).
2. **Registry** - `OptimizationRegistry` singleton, deterministic ordering by `id`, lookup, duplicate reject, `ensureCapabilitiesRegistered`.
3. **Refactor RecommendationEngine** - `generateWithProfile(b,bottlenecks,profile)` iterates registry before/after legacy 8, no growing hard-coded list for new caps.
4. **Two reference capabilities** - `flatpak-unused` (R1, 500MB threshold, `flatpak uninstall --unused` via user, rollback `flatpak install`) and `journal-vacuum` (R1, 1GB/500M thresholds, `journalctl --vacuum-size=500M` via helper `org.polaris.journal.vacuum`, bounded).
5. **Providers** - `RealFlatpakProvider` (`/usr/bin/flatpak list --columns`), `RealJournalDiskProvider` (`/usr/bin/journalctl --disk-usage`), read-only, `available false` if not installed, never guess, no `sh -c`.
6. **Baseline extension** - `FlatpakBaseline` (`runtimes`, `unusedRuntimes`, `reclaimableBytes`, `hasFlatpak`, `Meta`), `JournalDiskBaseline` (`diskUsageBytes`, `reclaimableBytes`, `vacuumTarget`, `Meta`) added to `PerformanceBaseline` (backward compatible, defaults `available false`).
7. **Stale protection** - every approval-sensitive value (`reclaimableBytes`, `diskUsageBytes`, `stateHash`) in `CurrentState` `preconditions` map → `validateForApply` fail-closed if `CHANGED`/`UNAVAILABLE` (existing 7-field matrix extended).
8. **Comparison/Verification** - `ComparisonEngine` adds `storage.free` (`decrease >0.5GB regression`), `flatpak.reclaimable`, `journal.diskUsage` metrics; verdict `SUCCESS` when `storage.free` or `flatpak/journal` delta matches `expectedBenefit`; otherwise `NO_CHANGE`/`IMPROVED`/`INCONCLUSIVE`.
9. **Explainability** - `ExplanationEngine` delegates `flatpak-unused`/`journal-vacuum` to capability `explainWhyNow`/`WhatWillChange`/`WhatWillNotChange`/`rejectionConditions`, redaction intact, deterministic.
10. **Profile** - `ProfileAdvisor` not weakened; `flatpak/journal` are not workflow-blocked (`UNKNOWN` → not `BLOCKED`), but `isApplicable` still requires evidence.
11. **CLI** - `polaris_p4 recommendations [--json]`, `polaris_p4 capabilities list [--json]`, `polaris_p4 transaction preview flatpak-unused|journal-vacuum` (fixture mode, reads `/tmp/polaris-test-root/p19/flatpak.list` if present, else real `BaselineEngine::collect`), no `optimize all`, one capability → one transaction.
12. **Tests** - 5 new suites (`p19_registry` 6 cats, `p19_flatpak_capability` 9 cats, `p19_journal_capability` 10 cats, `p19_lifecycle` 7 cats, `p19_capability_security` 6 cats) → 33→38 tests, isolated `/tmp/polaris-test-root/p19_*`, no `/etc`/`/run`/`~/.local/state/polaris` writes, `p4_security` etc. still pass.
13. **Safety preserved** - `FileSafety` traversal/symlink, `TransactionValidator` stale, `StateMachine` fail-closed, `BackupEngine` no-overwrite, `AuditLog` `fsync`+hash chain, `IpcProtocol` `ping/info` only, `TransactionLock`, `RecoveryDetector` unchanged.

---

## 3. Architecture

```
Providers (read-only)          Registry & Capabilities        Engines (pure)          Safety (stateful)
─────────────────              ─────────────────────          ────────────            ──────────────
RealFlatpakProvider ─┐        IOptimizationCapability ─┐
RealJournalDiskProvider ─┤─→ BaselineEngine::collect ─┤─→ OptimizationRegistry ─┤─→ RecommendationEngine::generateWithProfile → Recommendation
                     └→ PerformanceBaseline (+5) ─┘                         └→ ExplanationEngine::explainCandidate (delegates to capability)
                                                                              └→ TransactionStore::create(preview) → TransactionValidator::validateForApply (preconditions) → BackupEngine → apply → ComparisonEngine::compare (storage.free etc.) → AuditLog
```

- `RecommendationEngine` becomes thin iterator: legacy 8 hard-coded (frozen) + `for (cap : registry) if (cap->isApplicable) {ev=cap->collect; if(ev.available) out.push_back(cap->toRecommendation)}` deterministic.
- `Transaction` remains `safety::Transaction` (`target` = fixture file `/tmp/polaris-test-root/p19/*` for P19, `beforeHash` = `ev.stateHash`, `preconditions` = `ev.preconditions`).
- `CLI` `p4_cli.cpp` `cmd_preview` branches on `flatpak-unused`/`journal-vacuum` to use capability flow.

---

## 4. Reference Capabilities Specification

### flatpak-unused

- **Evidence:** `flatpak.list` `unused --dry-run` → `reclaimableBytes`; `hasFlatpak` false → `available false`; `unused 0` → `available false reason “no unused”`; `<500MB` → `isApplicable false`.
- **Confidence:** `≥1.5GB` 0.90, `≥1.0GB` 0.85, `≥0.5GB` 0.75, `<0.5` 0.60.
- **Risk:** `R1`, `requiresAuth false`, `requiresReboot false`, `reversibility High (flatpak install <id>)`.
- **Transaction:** `target /tmp/polaris-test-root/p19/flatpak-unused.state`, `operation flatpak-unused`, `method flatpak uninstall --unused`, `rollback flatpak install`, `beforeHash ev.stateHash`, `preconditions flatpak.unusedCount, reclaimableBytes, stateHash`.
- **Verification:** `storage.free` delta `>0` + `flatpak.reclaimable` decrease → `SUCCESS` (`expected “flatpak reclaim”`), else `IMPROVED`/`NO_CHANGE`.
- **Explain:** `WHY NOW` flatpak reclaimable + `free` %, `WHAT WILL CHANGE` `flatpak uninstall --unused`, `WHAT WILL NOT CHANGE` NVIDIA/zram/Akonadi, `REJECTION` stale `flatpak.stateHash`, `unavailable`, `<500MB`, `already optimal`, etc.

### journal-vacuum

- **Evidence:** `journalctl --disk-usage` `3.2G` → `diskUsageBytes` `3435973836`, `target 500M` → `reclaimable 2.71GB`; `<1GB usage` → not applicable; `<500M reclaimable` → not applicable.
- **Confidence:** `≥2GB` 0.90, `≥1GB` 0.85, else 0.75.
- **Risk:** `R1`, `requiresAuth true` (`org.polaris.journal.vacuum`), `requiresReboot false`, `reversibility Limited (old logs >14d lost)`.
- **Transaction:** `target /tmp/polaris-test-root/p19/journal-vacuum.state`, `operation journal-vacuum`, `method journalctl --vacuum-size=500M` (bounded), `rollback N/A (logs lost)`.
- **Verification:** `journal.diskUsage` delta `< -0.1GB` → `SUCCESS`/`IMPROVED`; `<5MB` → `NO_CHANGE`.
- **Explain:** similar, `WHAT WILL NOT CHANGE` NVIDIA/zram/flatpak.

Both: `unknown` evidence → `available false` → no recommendation, no transaction (fail closed).

---

## 5. Testing (38 tests target)

- `p19_registry` - register, duplicate reject, deterministic ordering, lookup, ensure, ids sorted, risk.
- `p19_flatpak_capability` - unavailable, insufficient 0, insufficient <500MB, measured benefit 1.46GB, large 2.2GB, tx generation, stale, verify success, explain.
- `p19_journal_capability` - unavailable, <1GB, small reclaimable, 2.71GB, medium 1.0GB, tx, stale, verify success/no_change, explain.
- `p19_lifecycle` - flatpak tx lifecycle (create→approve idempotent→apply→already_completed→verify), journal stale via `TransactionStore::apply` fail-closed, duplicate create, comparison storage/journal, explanation integration, fixture isolation, no real-host mutation.
- `p19_capability_security` - `FileSafety` traversal/metachar/NUL/oversized still fail-closed, redaction `[REDACTED]` in `toHuman`, audit hash chain, no `/run/polaris/helper.sock`, no `sh -c` in capabilities, fixture isolation (file content unchanged).

Existing 33 tests must stay 100%.

---

## 6. Non-Goals

No privileged real-host apply, no `helper.sock` install, no batch, no `zram`/`swappiness`/`governor`/`scheduler`/`grub`/`modprobe`, no Qt GUI, no `optimize all`, no weakening of `FileSafety`/`StateMachine`/`AuditLog`/`IpcProtocol`.

---

## 7. Build & Validation

```
rm -rf /tmp/polaris_p19_build2 && cmake -S . -B /tmp/polaris_p19_build2 --fresh && cmake --build /tmp/polaris_p19_build2 && ctest --test-dir /tmp/polaris_p19_build2 --output-on-failure # expect 38/38
stat /etc/fstab | grep Modify  # 2026-08-31 21:19 unchanged
ls /run/polaris/helper.sock # No such file
ls ~/.local/state/polaris/profile.json # No such file
```

Docs: `docs/P19_PLAN.md` (this), `docs/P19_IMPLEMENTATION_REPORT.md`, `docs/ARCHITECTURE.md` P19 layer, `docs/ROADMAP.md` P19 COMPLETE, `docs/PROJECT_STATE.json` P19, `docs/PROJECT_HANDOFF.md` P19.

