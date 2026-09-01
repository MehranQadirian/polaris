# P19 — Optimization Capability Framework — Implementation Report

**Phase:** P19 — Engineering (Framework, Not Tweaks)  
**Date:** 2026-09-01  
**Source:** Repository `~/Documents/polaris` verified via `cmake`, `ctest`, `ls`, `cat`, `stat`, `systemctl`, `akonadictl`, `nvidia-smi` — not conversation memory  
**Status:** **COMPLETE** — registry + 2 reference capabilities, providers, baseline extension, RecommendationEngine registry iteration, Explanation/Transaction/Comparison integration, CLI, 38/38 tests, no host mutation

---

## 1. Implementation Summary

Transformed `RecommendationEngine` from hard-coded `if(bn.id==...)` (8 recs) into a registry-driven engine where adding a capability is `implement IOptimizationCapability + register + test`. Provided two reference capabilities (`flatpak-unused` R1, `journal-vacuum` R1) proven against isolated fixtures (`/tmp/polaris-test-root/p19_*`) with measured `benefitGB`, deterministic `stateHash`, stale `preconditions`, transaction `preview→approve→backup→apply→verify` via existing `TransactionStore`/`TransactionValidator`/`StateMachine`/`BackupEngine`, and `ComparisonEngine` `storage.free`/`flatpak.reclaimable`/`journal.diskUsage` metrics. All safety invariants preserved (`FileSafety`, `IpcProtocol` `ping/info` only, `AuditLog` `fsync`, no privileged apply).

**Workflow:** `READ` (P18 38 tests) → `DESIGN` (INTERFACE + REGISTRY) → `IMPLEMENT` (providers+capabilities+engine+CLI) → `TEST` (5 new suites, 38/38) → `SECURITY-VERIFY` → `DOCUMENT`.

---

## 2. Files Changed / Added

**Modified:**

- `CMakeLists.txt:9` — add `core/capabilities/CapabilityRegistrySetup.cpp` to `polaris_core`, 5 P19 test executables
- `core/domain/PerfModels.h:102` — add `FlatpakBaseline` (`Runtime`, `runtimes`, `unusedRuntimes`, `reclaimableBytes`, `hasFlatpak`, `Meta`), `JournalDiskBaseline` (`diskUsageBytes`, `reclaimableBytes`, `vacuumTarget`, `Meta`), extend `PerformanceBaseline` with `flatpak`, `journalDisk`
- `core/engines/perf/BaselineEngine.h:13` — include `RealFlatpakProvider`, `RealJournalDiskProvider`, collect `b.flatpak`, `b.journalDisk` in `collect()`
- `core/engines/recommend/RecommendationEngine.h:4` — add `generateWithProfile(b,bottlenecks,profile)`, `capabilityIds()`
- `core/engines/recommend/RecommendationEngine.cpp:2` — include registry, `generate` delegates to `generateWithProfile`, after legacy 8 recs iterates `OptimizationRegistry` (ensure, `isApplicable`, `collect` `available`, `confidence≥0.5`, `toRecommendation`), deterministic ordering
- `core/engines/comparison/ComparisonEngine.h:10` — extend `Thresholds` with `storageFreeGb 0.5`, `journalDecreaseGb`
- `core/engines/comparison/ComparisonEngine.cpp:54` — add `MetricComparison` `storage.free` (`freeBytes` → GB, `decrease >0.5GB` regression), `flatpak.reclaimable`, `journal.diskUsage`; extend `verdict` to handle `p19Improved` (`storage.free`/`flatpak`/`journal` delta) → `SUCCESS` if `expectedBenefit` mentions same domain else `IMPROVED`
- `core/explainability/ExplanationEngine.cpp:5` — include `OptimizationRegistry`, handle `flatpak-unused`/`journal-vacuum` in `buildWhyNowCandidate`/`WhatWillChange`/`WhatWillNotChange`/`rejectionConditions` via capability `explain*` (deterministic, redaction intact)
- `cli/p4_cli.cpp:14` — include `BaselineEngine`, `BottleneckEngine`, `OptimizationRegistry`, `TransactionStore`, `RealFlatpak/JournalDiskProvider`; add `cmd_preview` branch for `flatpak-unused`/`journal-vacuum` (fixture mode, `isApplicable` check, `collect`→`toRecommendation`→`snapshot`→`toTransaction` via `TransactionStore::create`), `cmd_recommendations` (`RecommendationEngine::generateWithProfile`), `cmd_capabilities` (`registry.capabilities`), help updated to `P19` with `flatpak-unused`/`journal-vacuum` examples, no `optimize all`

**Added:**

- `core/capabilities/IOptimizationCapability.h:1` (85L) — `CapabilityEvidence` (`available`, `reason`, `evidence` sorted, `confidence`, `benefitGB`, `reclaimableBytes`, `benefitStr`, `risk`, `preconditions`, `stateHash`), `IOptimizationCapability` pure virtual (`id`, `name`, `category`, `description`, `risk`, `reversibility`, `requiresReboot/Auth`, `isApplicable`, `collect`, `toRecommendation`, `snapshot`, `toTransaction`, `verify`, `explainWhyNow/WhatWillChange/WhatWillNotChange/rejectionConditions`)
- `core/capabilities/OptimizationRegistry.h:1` (55L) — `OptimizationRegistry` singleton, `registerCapability` (reject duplicate, `id` empty/`..`/NUL/oversized checks, deterministic `sort` by `id`), `capabilities()` sorted, `lookup`, `clear` for tests, `isDeterministic`
- `core/capabilities/OptimizationRegistry.cpp` — not needed (header-only); `CapabilityRegistrySetup.h:1`, `CapabilityRegistrySetup.cpp:1` (8L) — `ensureCapabilitiesRegistered` registers `FlatpakUnusedCapability` + `JournalVacuumCapability` in id order if `size()==0`
- `core/capabilities/FlatpakUnusedCapability.h:1` (210L) — `collect` via `b.flatpak`, `available false` if not installed/0 unused, `reclaimableBytes` numeric, `confidence` 0.90/0.85/0.75/0.60 by GB, `isApplicable` requires `reclaimable≥500MB` and `hasFlatpak`, `toRecommendation` `REC-flatpak-unused` R1, `snapshot` `target /tmp/polaris-test-root/p19/flatpak-unused.state` + `stateHash`, `toTransaction` with `preconditions flatpak.unusedCount/reclaimable/stateHash`, `verify` via `storage.free` delta or `flatpak.reclaimable` decrease (`SUCCESS` if matches expected else `IMPROVED`), `explain*` deterministic, `rejectionConditions` includes `stale flatpak.stateHash` etc.
- `core/capabilities/JournalVacuumCapability.h:1` (210L) — similar for `journalDisk`, thresholds `diskUsageBytes<1GB` false, `reclaimable<500M` false, `confidence` 0.90/0.85/0.75, `risk R1` `requiresAuth true` `org.polaris.journal.vacuum`, `verify` via `journal.diskUsage` decrease (`SUCCESS` if matches expected), `rejectionConditions` `stale journal.stateHash`
- `core/providers/real/RealFlatpakProvider.h:1` (256L) — `safeExec` `/usr/bin/flatpak` `fork+execv+poll` timeout 5s, `collect` parses `flatpak list --columns`, `fromFixture` deterministic for tests (parses `list` + `unused` strings, duplicate heuristic), `Meta` `available` handling, no `sh -c`
- `core/providers/real/RealJournalDiskProvider.h:1` (160L) — `safeExec` `/usr/bin/journalctl --disk-usage`, `parseSize` handles `B/KB/MB/GB`, `collect` parses `take up 3.2G` via `istringstream` + fallback `take up` substring, `fromFixture`, `reclaimable = usage - 500M`, `Meta` handling, no shell
- `tests/unit/test_p19_registry.cpp` (90L) — 6 cats: registration, duplicate rejection, deterministic ordering, ensure, ids, risk
- `tests/unit/test_p19_flatpak_capability.cpp` (226L) — 9 cats: unavailable, insufficient 0, insufficient <500MB (distinct id), measured 1.46GB, large 2.2GB, tx generation, stale (beforeHash), verify success, explain
- `tests/unit/test_p19_journal_capability.cpp` (188L) — 10 cats: unavailable, <1GB, <500M (adjusted to <1GB), 2.71GB, medium 1.0GB, tx, stale, verify success/no_change, explain
- `tests/unit/test_p19_lifecycle.cpp` (260L) — 7 cats: flatpak lifecycle (create→approve idempotent→apply→already_completed→verify), journal stale via `TransactionStore::apply` fail-closed, duplicate create, comparison storage/journal, explanation integration, fixture isolation (`p19_iso1` vs `p19_iso2`), no real-host mutation
- `tests/security/test_p19_capability_security.cpp` (127L) — 6 cats: `FileSafety` still fail-closed, redaction `[REDACTED]`, audit hash chain, no `/run/polaris/helper.sock`, no `sh -c`, fixture isolation (file hash unchanged)
- `docs/P19_PLAN.md` (12K), `docs/P19_IMPLEMENTATION_REPORT.md` (this)

No modification to: `TransactionValidator`, `StateMachine`, `BackupEngine`, `ProfileStore`, `RecommendationEngine` legacy logic (preserved for backward compat), `IpcProtocol`, `TransactionLock`, `RecoveryDetector`, `akonadi`, `mssql`, `nvidia`, `fstab`, `zram`.

---

## 3. Capability Model

**Deterministic JSON schema for `CapabilityEvidence` (keys sorted, no timestamps in decision):**

```json
{"available":true,"benefitGB":1.46,"benefitStr":"1.5 GB disk reclaimed (unused runtimes 2)","confidence":0.85,"evidence":["org.freedesktop.Platform 23.08 flathub 900MB","filesystem / free 50G"],"preconditions":{"flatpak.reclaimableBytes":"1572864000","flatpak.stateHash":"ef9ffc...","flatpak.unusedCount":"2"},"reclaimableBytes":1572864000,"reason":"","risk":"R1","stateHash":"ef9ffc37aeff..."}
```

**Fields for 14 questions (from `IOptimizationCapability`):**

- `isApplicable` — `hasFlatpak` + `reclaimable≥500MB` + `hasFlatpak true` (flatpak) or `diskUsage≥1GB` + `reclaimable≥500MB` (journal); profile independent (always `ALLOWED_FOR_ANALYSIS` for these domains, not workflow-blocked).
- `collect` — reads `PerformanceBaseline.flatpak`/`journalDisk` (from provider or fixture), computes `benefitGB = reclaimable/1GB`, `confidence` by GB, `stateHash = sha256(unused list + reclaimable)`, `preconditions` for stale.
- `toRecommendation` — `id REC-<cap.id>`, `title name`, `risk R1`, `requiresAuth` per capability, `expectedBenefit benefitStr`, `evidence` sorted.
- `snapshot` — `CurrentState` `target /tmp/polaris-test-root/p19/*.state`, `currentBeforeHash = stateHash`, `preconditions` map.
- `toTransaction` — `target` fixture file, `operationId` = `cap.id`, `beforeHash` = `stateHash`, `preconditions` map, `ChangePreview` with `method` `flatpak uninstall --unused` or `journalctl --vacuum-size=500M`, `rollback` concept.
- `verify` — flatpak `storage.free` `+1GB` → `SUCCESS` if `expectedBenefit` mentions flatpak else `IMPROVED`; journal `diskUsage` `-2.7GB` → `SUCCESS`.
- `explain*` — `whyNow` with `reclaimableMB` + `free%` + `confidence%`, `whatWillChange` `target/operation`, `whatWillNotChange` NVIDIA/zram/Akonadi, `rejectionConditions` `stale flatpak.stateHash`, `unavailable`, `<500MB`, etc., sorted.

**Determinism:** same `baseline` + `profile` → same `evidence` sorted, same `stateHash`, same `Recommendation` `confidence`/`benefitStr`, same `Transaction` `preconditions`, same `Explanation` `whyNow` (via capability), same JSON (keys alphabetical, `evidence`/`rejectionConditions` sorted, `id` deterministic).

---

## 4. Registry

`OptimizationRegistry::instance()` singleton, `registerCapability` rejects duplicate `id` with `runtime_error "duplicate capability id: ..."`, validates `id` empty/`..`/NUL/oversized, keeps deterministic `sort` by `id` (`flatpak-unused` < `journal-vacuum`). `lookup` O(n), `capabilities()` returns sorted vector. `ensureCapabilitiesRegistered` registers `FlatpakUnusedCapability` + `JournalVacuumCapability` in id order if `size()==0`, idempotent (second call does not duplicate). No `helper.sock` created, no privileged `apply`.

**Current registry:** `2` capabilities (`flatpak-unused` R1, `journal-vacuum` R1). Adding a new capability now is:

```cpp
class MyCapability : public IOptimizationCapability { ... };
OptimizationRegistry::instance().registerCapability(std::make_unique<MyCapability>());
```

No edit to `RecommendationEngine.cpp` required (it iterates registry).

---

## 5. RecommendationEngine Change

Before P19: `generate(b,bottlenecks)` hard-coded 8 `add(...)` with `void(b)` ignoring baseline.

After P19: `generate(b,bottlenecks)` delegates to `generateWithProfile(b,bottlenecks,defaultProfile)` which:

1. Emits legacy 8 (frozen, not growing) via same `if(bn.id==...)` + `add REC-001..008`.
2. Ensures registry, iterates `for (cap : registry.capabilities()) if (cap->isApplicable(b,profile)) {ev=cap->collect(b); if(ev.available && ev.confidence>=0.5) out.push_back(cap->toRecommendation(ev,b));}` deterministic.

`capabilityIds()` exposes registry ids for CLI. No hard-coded growth for new domains.

---

## 6. Comparison Extension

`ComparisonEngine` `compare(before,after,expectedBenefit)` now adds 3 metrics:

- `storage.free` (`storage.filesystems[0].freeBytes` GB, `decrease >0.5GB` regression, `isHealth true`)
- `flatpak.reclaimable` (`flatpak.reclaimableBytes` GB, informational `true`, `regression false`)
- `journal.diskUsage` (`journalDisk.diskUsageBytes` GB, `isHealth false`, `isBackground true`)

Verdict extended: if `p19Improved` (`storage.free` `+0.1GB` or `flatpak.reclaimable` `-0.1GB` or `journal.diskUsage` `-0.1GB`) and not `hasRegression`, then `SUCCESS` if `expectedBenefit` mentions same domain else `IMPROVED`.

---

## 7. CLI

Extended `cli/p4_cli.cpp` unified, no new binary:

- `polaris_p4 recommendations [--json]` → `BaselineEngine::collect` + `BottleneckEngine::analyze` + `RecommendationEngine::generateWithProfile` (reads `ProfileStore` no auto-create, runs `explainCandidate` for each via registry, audit `explanation.generated`)
- `polaris_p4 capabilities list [--json]` → `OptimizationRegistry` `capabilities` loop, outputs `id`/`name`/`category`/`risk`/`reboot`/`auth`
- `polaris_p4 transaction preview flatpak-unused|journal-vacuum` → fixture mode: if `/tmp/polaris-test-root/p19/flatpak.list` exists uses `RealFlatpakProvider::fromFixture`, else real `BaselineEngine::collect`; checks `isApplicable` → if false prints `applicable false` no transaction; else `collect`→`toRecommendation`→`snapshot`→`toTransaction` via `TransactionStore::create` (fixture path `txStore` = `/tmp/polaris-test-root/transactions`), audit `transaction.previewed`, prints `transactionId`, `state`, `target`, `risk`, `expectedBenefit`, `reclaimableMB`, `confidence`, `preconditions` JSON
- `polaris_p4 explain flatpak-unused|journal-vacuum [--json] [--verbose]` → existing `explainCandidate` now delegates to capability (if baseline provided) for `WHY NOW`/`WHAT WILL CHANGE`/`WHAT WILL NOT CHANGE`
- `polaris_p4 transaction preview dummy-test` still works (legacy fixture)

Help updated to `P19` with `flatpak-unused`/`journal-vacuum` examples.

No `optimize all`, no batch, one capability → one transaction, approval remains explicit (`transaction approve` still explicit, not launch).

---

## 8. Tests (38 total, 33→38)

**File `tests/unit/test_p19_registry.cpp` (6 cats):**
1 registration (clear→register 2→lookup), 2 duplicate rejection (second flatpak throws), 3 deterministic ordering (reverse insert → sorted `flatpak-unused` < `journal-vacuum`), 4 ensure idempotent (second `ensure` not duplicate), 5 ids sorted, 6 risk (`R1`, `requiresAuth` false vs true).

**File `tests/unit/test_p19_flatpak_capability.cpp` (9 cats):**
1 unavailable (meta `available false` → `available false`), 2 insufficient 0 unused (list 1 runtime, unused empty → not applicable), 3 insufficient <500MB (distinct id `SmallApp` 100MB → `confidence 0.60` but `isApplicable false`), 4 measured 1.46GB (900+600 → `0.85` `1.5 GB` `stateHash 64`), 5 large 2.2GB (900+600+700 → `0.90`), 6 tx generation (`target` fixture path, `preconditions` reclaimable 900M), 7 stale (bind `APPROVED` then `validateForApply` with new reclaim 1500M → `beforeHash` stale), 8 verify success (`storage.free` `50G→51G` `SUCCESS`), 9 explain (`whyNow` contains flatpak, `whatWillChange` flatpak, `whatWillNotChange` NVIDIA, `rejectionConditions` stale).

**File `tests/unit/test_p19_journal_capability.cpp` (10 cats):**
1 unavailable, 2 <1GB (800M → `available false`), 3 700M (<1GB, adjusted to expect `<1GB` reason), 4 3.2G → `2.71GB` `0.90` R1, 5 medium 1.5G → `1.0GB` `0.85`, 6 tx (`requiresAuth true` `org.polaris.journal.vacuum`), 7 stale (`APPROVED`→`validateForApply` `beforeHash`), 8 verify success (`3.2G→400M` `SUCCESS`), 9 verify no change (`3.2G→3.2G` `NO_CHANGE`), 10 explain.

**File `tests/unit/test_p19_lifecycle.cpp` (7 cats):**
1 flatpak lifecycle (create→approve idempotent→apply→already_completed→verify idempotent, fixture `/tmp/polaris-test-root/p19_lifecycle_flatpak`), 2 journal stale via `TransactionStore::apply` fail-closed (`precondition`/`beforeHash`), 3 duplicate create (`duplicate` audit), 4 comparison storage/journal (`storage.free` found, `SUCCESS`/`IMPROVED`), 5 explanation integration (`explainCandidate flatpak-unused` deterministic JSON), 6 fixture isolation (`p19_iso1` vs `p19_iso2` separate transactions, not cross), 7 no real-host mutation (no `/run/polaris/helper.sock`).

**File `tests/security/test_p19_capability_security.cpp` (6 cats):**
1 `FileSafety` still fail-closed (`..`, `;`, NUL, oversized, `/etc/passwd` not allowed, `/tmp/polaris-test-root` allowed), 2 redaction (`[REDACTED]` in `toHuman` verbose), 3 audit hash chain (`TX-TEST-P19-AUDIT` two events, `previousHash` valid), 4 no helper socket (`/run/polaris/helper.sock` not exists), 5 no `sh -c` in capabilities (`risk`/`name` not containing), 6 fixture isolation (file `original content` hash unchanged after capability).

All fixtures `/tmp/polaris-test-root/p19_*` isolated (`remove_all`+`create_directories`, `file="/tmp/.../p19/flatpak-unused.state"` `original\n` before/after `explain` unchanged, `ProfileStore` not touched real `~/.local/state/polaris/profile.json` mtime unchanged, `AuditLog` test log `/tmp/polaris-test-root/audit.log`).

---

## 9. Build & Validation

```
cmake -S . -B /tmp/polaris_p19_build2 --fresh # Configuring done, Generating done
cmake --build /tmp/polaris_p19_build2 # 100% Built polaris_core, polaris, polaris_real, polaris_tests, test_real_providers, test_parsers, test_readonly, polaris_p3, polaris_p4, polaris_p5, test_comparison, test_post_change, test_regression, test_observed_benefit, test_p12_*, test_p13_*, test_p14_*, test_p15_*, test_p16_*, test_p19_* (5 new)
ctest --test-dir /tmp/polaris_p19_build2 --output-on-failure
# 38/38 100% 1.33s (previously 33/33, now 38/38)
```

**Host verification (2026-09-01 14:53, after build):**

- `stat /etc/fstab | grep Modify` `2026-08-31 21:19:15.195818022 +0330` unchanged
- `cat /etc/fstab` 3 entries `# UUID 39b0...` commented, `findmnt --verify` `0 parse errors`
- `ls /run/polaris/helper.sock` `No such file or directory`
- `ls /run/polaris/transaction.lock` `No such file`
- `ls ~/.local/state/polaris/profile.json` `No such file`
- `zramctl` `8G lzo-rle DATA 4K COMPR 80B TOTAL 12K 0B used`
- `systemctl is-enabled mssql-server` `disabled` `is-active inactive`
- `lspci -k -s 01:00.0` `GM108M [GeForce MX130] [10de:174d] Kernel driver in use: nvidia`
- `modinfo nvidia | grep version` `470.256.02` `extra/nvidia-470xx`
- `nvidia-smi` `470.256.02` `50C` `1MiB /2004MiB`
- `akonadictl status` `Control: running` `Server: running`

No `dnf`, `akmods`, `dracut`, `modprobe`, `reboot`, `sudo`, `polkit`, `/etc`/`/usr`/`~/.config` mutation; `polaris_p4` only writes `/tmp/polaris-test-root`.

---

## 10. Documentation Updated

- `docs/P19_PLAN.md` — gap, objectives, architecture diagram, providers, capabilities spec, tests, non-goals, build
- `docs/P19_IMPLEMENTATION_REPORT.md` — this report (files, model, registry, engine, comparison, CLI, tests, host verification, limitations)
- `docs/ARCHITECTURE.md` — added P19 layer (Capability Registry, Providers, Baseline extension, Comparison new metrics, Explanation delegation)
- `docs/ROADMAP.md` — `P19 COMPLETE 2026-09-01` (registry + 2 caps + 38/38)
- `docs/PROJECT_STATE.json` — `currentPhase P19`, `nextPhase null`, `completedPhases` + P19 entry, tests `38/38 1.33s`, `optimizationCandidates` extended with `flatpak-unused`/`journal-vacuum` status
- `docs/PROJECT_HANDOFF.md` — P19 section + host state same as P18 (no reboot, no new transactions on real host)
- `docs/OPTIMIZER_GAP_ANALYSIS.md` — preserved (analysis that motivated P19)

---

## 11. Known Limitations / Not Implemented

- **Real flatpak/journal collection** on current host: `flatpak` not installed on this Fedora host? `RealFlatpakProvider::collect` will report `available false` (as seen in `BaselineEngine::collect` `flatpak` `hasFlatpak false` on current host) — correctly not applicable, no transaction previewed (fixture proves architecture, not host).
- **Journal vacuum privileged helper** not installed: `IpcProtocol` remains `ping/info` only; `requiresAuth true` for journal but helper not installed, so real-host `APPLY` remains disabled (intentional, P14 design). Transaction `STATE` stays `PREVIEWED` until helper reviewed.
- **No helper socket** (`/run/polaris/helper.sock` not exists) — correct for P19 (engineering phase).
- **Provider `safeExec` fallback** still `execv` fixed path, not `sd-bus`/`libEGL`; `flatpak list` parsing heuristic for duplicate branch, not full `flatpak remote-info`.
- **Comparison `storage.free` threshold** `0.5GB` initial, not tuned per host; `flatpak/journal` metrics `isHealth false` (informational) — future could tune.
- **Legacy `REC-006` flatpak static rec still emitted** alongside registry `REC-flatpak-unused` — not deduplicated (legacy frozen, new evidence-backed is separate; future could deprecate legacy).
- **No `autostart`/`timer`/`baloo` capabilities** yet — P19 proves framework with 2; adding third is now `implement + register`.

All limitations fail-closed, documented as `available false` or `INCONCLUSIVE`.

---

## 12. Security Assessment (Preserved)

- `FileSafety` `validatePath` still rejects `..`, `;|&` `` ` `` `$`, `NUL`, `>4096`, `symlink`, `canonical` escape, allowlist `/tmp/polaris-test-root` + `profile.json` + `fstab` + `p19/*.state` (via `/tmp` allowlist).
- `TransactionValidator` 7 fields + `preconditions` map (now includes `flatpak.reclaimableBytes`, `journal.diskUsageBytes`) still `validateForApply` fail-closed, `UNAVAILABLE` → `unverifiable_*`.
- `StateMachine` `PREVIEWED→APPROVAL_REQUIRED→APPROVED→AUTHORIZATION_REQUIRED→AUTHORIZED→BACKUP_CREATED→APPLYING` still fail-closed (`P12` `APPROVED→FAILED` for stale).
- `BackupEngine` versioned `SHA-256` no-overwrite, `AuditLog` `fsync`+hash chain.
- `IpcProtocol` `allowedOperations` `ping`/`info` only (checked `grep -r "sh -c" core/` 0).
- `TransactionLock` `flock` advisory, `RecoveryDetector` detection-only.
- `ProfileAdvisor` `UNKNOWN→REQUIRES`, `YES→BLOCKED` not weakened (flatpak/journal not workflow-blocked, but still require evidence).
- `ExplanationEngine` `containsSecret`/`redact` intact.

---

## 13. Next Phase

**No P20 automatically justified.**

P19 closes the identified architectural gap: `RecommendationEngine` is now registry-driven; adding a new capability is `implement + register + test` without editing the engine; two reference capabilities prove measured benefit, stale protection, verification, explainability.

**If a P20 is desired**, it should be **P20: Helper Wiring for Journal Vacuum** (privileged `org.polaris.journal.vacuum` helper, narrowly scoped, `vacuum-size=500M` bounded, `TransactionLock` mandatory, `RecoveryDetector` auto-validation) — but only after P19 registry has stabilized and `flatpak/journal` have been exercised via fixtures on multiple hosts. Do **not** invent `P20: Add 10 more tweaks`.

**Recommendation:** `STOP` after P19 unless a concrete privileged helper gap is proven on a host where `journal 3.2G` or `flatpak 2GB` is actually measured on real baseline and user has approved `preview→approval`.

---

## 14. Verification Commands

```
rm -rf /tmp/polaris_p19_build2 && cmake -S ~/Documents/polaris -B /tmp/polaris_p19_build2 --fresh && cmake --build /tmp/polaris_p19_build2 && ctest --test-dir /tmp/polaris_p19_build2 --output-on-failure
stat /etc/fstab | grep Modify  # 2026-08-31 21:19 unchanged
ls /run/polaris/helper.sock  # No such file
ls /run/polaris/transaction.lock  # No such file
ls ~/.local/state/polaris/profile.json  # No such file
ls -R /tmp/polaris-test-root/p19_* 2>&1 | head
./build/polaris_p4 capabilities list --json | python3 -m json.tool
./build/polaris_p4 recommendations --json | python3 -m json.tool | head -n 40
./build/polaris_p4 explain flatpak-unused --json | python3 -m json.tool
./build/polaris_p4 explain journal-vacuum --verbose
```

---

*No real-host optimization was performed during P19. No reboot. No privileged mutation. 38/38 tests, fixture isolation, safety invariants preserved.*
