# Polaris

**A safe, evidence-backed Linux system optimizer — measure first, explain why, ask for approval, backup, apply one change at a time, verify.**

> **Why I built Polaris** — *Mehran Qadirian*
>
> *I wanted a tool that could help me optimize my Linux system without blindly applying tweaks or turning system administration into a collection of shell commands.*
>
> *I wanted Polaris to measure first, explain why a change is worth making, show exactly what will change, ask for explicit approval, create a backup, apply one change at a time, verify the result, compare the measured outcome with the expected benefit, and detect regressions.*
>
> *That is why I built Polaris. It is not a debloat script. It never disables services automatically, never runs `curl | bash`, and never assumes “smaller number = better.” The safety framework exists to make optimization trustworthy — the eventual Qt GUI is only another frontend, the CLI remains a first-class interface, and the core engine is shared.*

**Target:** Linux — initial focus Fedora + KDE Plasma | **Version:** 0.1.0 | **Status:** P19 — Optimization Capability Framework | **Tests:** 38/38 passing

---

## The Problem with Traditional “Optimization”

Most Linux optimization tools are shell collections: they list services, suggest `systemctl disable`, and tell you a change is good because a number got smaller. They do not show evidence, do not ask for explicit approval tied to the exact system state they measured, do not create a backup, do not verify the outcome, and do not detect regressions. If something breaks, you are left to debug.

Polaris takes the opposite approach: **if any step cannot be proven safe, it fails closed and does nothing.** The pipeline is:

```
READ → MEASURE → ANALYZE → EXPLAIN → RECOMMEND → PREVIEW → APPROVAL → BACKUP → APPLY → VERIFY → COMPARE → REGRESSION → AUDIT
```

---

## Safety Philosophy

- **One change at a time** — transactions are `PREVIEWED → APPROVED → BACKUP → APPLIED`, never batched, `TransactionLock` `flock` exclusive.
- **Preview is not approval** — viewing a recommendation or running `polaris_p4 recommendations` does not authorize it. You must `polaris_p4 transaction approve <transactionId>` with hash-bound `approvedBeforeHash`/`approvedTarget`/`approvedPreconditions`.
- **Stale-preview protection** — if `beforeHash`, `unitHash`, `kernelVersion`, `packageStateHash`, or any `precondition` (e.g., `flatpak.reclaimableBytes`, `journal.diskUsageBytes`) changes after preview, `TransactionValidator::validateForApply` → `FAILED` `stale_*`/`unverifiable_*`, no mutation, require new preview.
- **Backup before mutation** — versioned `SHA-256` `fsync` no-overwrite (`BackupEngine::create` `is_regular_file` check); if backup fails, no `APPLY`.
- **TOCTOU protection** — `FileSafety::isSymlink` + `canonical` before and after `BACKUP_CREATED` → `toctou.symlink` `FAILED`.
- **File safety** — allowlist `/tmp/polaris-test-root` + `~/.local/state/polaris/profile.json` + `~/.config/autostart` pilot + `/etc/fstab`, rejects `..`, `;|&` `` ` `` `$`, `NUL`, `>4096`, symlink, `canonical` escape, `atomicWrite` `temp+fsync+rename`.
- **No hidden auth** — `execv` fixed paths `/usr/bin/systemctl` `/usr/bin/flatpak` `/usr/bin/journalctl`, bounded `poll` timeout, no `sh -c`; `polkit` `auth_admin_keep` `org.polaris.*`; `IpcProtocol` allowlist `ping`/`info` only (no `exec`), `SO_PEERCRED` kernel creds; `AuditLog` hash chain `previousHash→eventHash` `fsync`.
- **No automatic reboot, no guessing** — `rebootRequired` explicit, `MetricMeta` `available false` `note` never `0`, `expectedBenefit` ≠ `observedBenefit`.

---

## Evidence-Driven Optimization

Polaris does not guess. `BaselineEngine::collect` gathers 18 metrics with `MetricMeta` (`source`/`method`/`confidence`):

- **Providers** `RealOsProvider`, `RealCpuProvider`, `RealMemoryProvider` (`pressure`, `zram`), `RealStorageProvider` (`statvfs`, `/sys/block`), `RealGpuProvider` (`lspci`, `glxinfo`), `RealThermalProvider` (`hwmon`), `RealSystemdProvider` (`systemd-analyze`, `systemctl --failed`, `critical-chain` BLOCKER vs background), `RealKdeProvider` (`plasmashell`, `effects`), `RealProcessProvider` (`/proc`), `RealJournalProvider` (`journalctl -p 3`), `RealFlatpakProvider` (`flatpak list`), `RealJournalDiskProvider` (`journalctl --disk-usage`).

`BottleneckEngine::analyze` emits multi-evidence `Bottleneck` (`severity`/`confidence`/`evidence`/`observedValue`/`expectedValue`/`impact`/`risk`).

`Verification` is `ComparisonEngine::compare(before,after,expectedBenefit)` with stored thresholds (`boot >+10%` `available -1GiB` `thermal +15C` `new_failed` `storage.free >0.5GB`), metric `delta`/`pctDelta`/`available`/`regression` per `MetricComparison`, verdict `SUCCESS`/`IMPROVED`/`NO_CHANGE`/`NO_BENEFIT`/`REGRESSION`/`INCONCLUSIVE` — never claims benefit not measured.

---

## Transaction Model & Explicit Approval

Every mutation is a `safety::Transaction` (`StateMachine` 16 states):

```
PROPOSED → PREVIEWED → APPROVAL_REQUIRED → APPROVED → AUTHORIZATION_REQUIRED → AUTHORIZED → BACKUP_CREATED → APPLYING → APPLIED → VERIFYING → VERIFIED → COMPLETED
                                                              ↘ FAILED → ROLLING_BACK → ROLLED_BACK
```

- `create` duplicate `ALREADY_EXISTS` no overwrite
- `approve` binds `approved*` hashes, idempotent `already approved`
- `canApply` `APPROVAL→VALIDATION→BACKUP→FINAL VALIDATION→APPLY`
- `apply` on `COMPLETED` → `already_completed` no second mutation
- `verify` idempotent

`profile set` (`UserProfile` `UNKNOWN/YES/NO`, `ProfileStore` `0600`, `ProfileAdvisor` `BLOCKED`/`REQUIRES`/`ALLOWED`) is a **constraint, not approval** — `UNKNOWN` never silently becomes `YES`, `BLOCKED_BY_USER_WORKFLOW` (`usesKMail=yes` → `Akonadi will remain enabled...`) never auto-disabled.

---

## Backup / Rollback Philosophy

`BackupEngine::create(transactionId, originalPath)` → `~/.local/state/polaris/backups/<tx>/*.bak` `SHA256` `SIZE` `PERMS` `is_regular_file` `fsync` no overwrite; `restore` `atomicWrite`. For P19 `flatpak-unused` `journal-vacuum`, target is fixture file `/tmp/polaris-test-root/p19/*.state` (so `apply` is `atomicWrite` `temp+fsync+rename` on fixture, not real `flatpak` system); `rollback` concept `flatpak install <runtime>` or `Limited (old logs >14d lost)` is explicit in `ChangePreview`/`Recommendation.rollbackConcept`. Second `create` same id throws `already exists` (proof not overwritten).

---

## Explainability

`ExplanationEngine` answers 14 questions for every candidate and transaction, deterministic `toJson` sorted keys, `toHuman(verbose)` redacted `[REDACTED]` (`containsSecret` `password`/`secret`/`passwd`):

- **WHY NOW?** measured evidence + `ProfileAdvisor` `causingField` + `confidence%` + `risk`
- **WHAT WILL CHANGE?** `target`/`operation`/`diff`/`method`/`privilege`/`reboot`
- **WHAT WILL NOT CHANGE?** explicit invariants scope-aware (`NVIDIA 470xx remains claimed`, `zram remains`, `Akonadi remains`...)
- **EVIDENCE** sorted, `EXPECTED BENEFIT` `benefitStr` (`1.5GB` from `reclaimableBytes/1GB`), `CONFIDENCE` `0.90`, `RISK` `R1`, `REVERSIBILITY`, `REBOOT`, `AUTHORIZATION`, `REJECTION CONDITIONS` `stale beforeHash`/`unverifiable_*`/`insufficient confidence`/`regression`, `ROLLBACK`, `OBSERVED BENEFIT`/`VERDICT`, `LIMITATIONS`, verbose adds `EVIDENCE`/`DEPENDENCIES`/`USER IMPACT`.

`explanation.generated` audit ≠ `transaction.approved` ≠ `authorization.granted` ≠ `apply.completed`.

---

## Capability Registry

P19 replaces hard-coded `if(bn.id==GPU-001)` with an extensible registry:

- **Interface** `IOptimizationCapability` (`CapabilityEvidence` `available`/`confidence`/`benefitGB`/`stateHash`/`preconditions`, `isApplicable`, `collect`, `toRecommendation`, `snapshot` `CurrentState`, `toTransaction`, `verify`, `explain*`)
- **Registry** `OptimizationRegistry` singleton deterministic `sort` by `id`, `lookup`, duplicate reject `runtime_error "duplicate capability id: ..."`, `ensureCapabilitiesRegistered` (idempotent)
- **Adding a capability** is now: `implement IOptimizationCapability` → `registry.registerCapability(make_unique<MyCapability>())` → `add test` — no `RecommendationEngine.cpp` edit.

---

## CLI Usage

**Requirements:** `cmake >=3.28`, `g++ >=14`, `openssl-devel`, `ninja` optional

```bash
git clone https://github.com/MehranQadirian/polaris.git
cd polaris
cmake -S . -B build --fresh -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j$(nproc)
ctest --test-dir build --output-on-failure  # expect 38/38 100%
```

**Primary CLI `polaris_p4` (P19):**

```bash
# 1. Discover (always read-only, no sudo)
./build/polaris_real --json | python3 -m json.tool | head -n 60
./build/polaris_p3 --json | python3 -m json.tool   # Baseline 18 metrics + Bottleneck 10 + Recommendation 8 + Benchmark

# 2. Capabilities & recommendations (read-only, no sudo)
./build/polaris_p4 capabilities list --json | python3 -m json.tool
./build/polaris_p4 recommendations --json | python3 -m json.tool

# 3. Explain (read-only)
./build/polaris_p4 profile show --json
./build/polaris_p4 explain flatpak-unused --json        # WHY NOW flatpak reclaimable + WHAT WILL NOT CHANGE
./build/polaris_p4 explain journal-vacuum --verbose     # human, redacted
./build/polaris_p4 explain akonadi-disable --json       # BLOCKED_BY_USER_WORKFLOW usesKMail=yes

# 4. Profile (tell Polaris about your workflow — writes ~/.local/state/polaris/profile.json 0600, not auth)
./build/polaris_p4 profile set usesKMail yes --json
./build/polaris_p4 profile set usesBluetooth no --json
# fields: usesKMail, usesKontact, usesKOrganizer, usesBluetooth, usesPrinting, usesAvahi, usesCups, usesAkonadi

# 5. Preview & approve (safe, test fixtures TX-TEST-* under /tmp/polaris-test-root, no real /run/polaris)
./build/polaris_p4 transaction preview flatpak-unused   # fixture 1.5GB or real hasFlatpak false → applicable false
./build/polaris_p4 transaction preview journal-vacuum    # fixture 2.7GB or real journal 400M → NOT APPLICABLE <1GB
./build/polaris_p4 transaction preview dummy-test       # legacy fixture /tmp/polaris-test-root/etc/fstab
./build/polaris_p4 transaction list
./build/polaris_p4 transaction show TX-TEST-123 --json
./build/polaris_p4 transaction approve TX-TEST-123      # binds approved* hashes, idempotent
./build/polaris_p4 transaction explain TX-TEST-123 --verbose
./build/polaris_p4 apply --dry-run dummy-test           # verifies dry-run writes nothing
./build/polaris_p4 audit list                           # hash-chained, fsync
```

Full reference: `docs/CLI.md` — distinguishes **READ-ONLY** vs **creates transaction** vs **requires approval** vs **capable of host mutation** (none privileged via helper `ping/info` only).

**Other binaries:** `polaris scan --json` (mock `FakeProviders`), `polaris_real` (real scan), `polaris_p3` (baseline), `polaris_p5` (pilot R1 `Hidden=true`).

---

## Current Capabilities

| Capability | Evidence | Benefit (measured) | Risk | Reboot | Auth | Rollback | Status on This Host |
|------------|----------|-------------------|------|--------|------|----------|---------------------|
| `flatpak-unused` | `flatpak list` `unused --dry-run` `reclaimableBytes` (`hasFlatpak` `available`) | `1.5GB` `0.85` (if `reclaimable≥500MB`, `≥1.5GB` → `0.90`) | `R1` | no | no | `flatpak install <runtime>` | **Fixture `1.5GB` → `RECOMMEND` on host with `flatpak` + `unused≥500MB`; this host `hasFlatpak false` → `NOT APPLICABLE` |
| `journal-vacuum` | `journalctl --disk-usage` `3.2G` `reclaimable 2.7GB` (`diskUsage≥1GB` `reclaimable≥500M`) | `2.7GB` `0.90` ( `usage-500M` ) | `R1` | no | yes `org.polaris.journal.vacuum` | `Limited (logs >14d lost)` | **Fixture `3.2G→400M` `SUCCESS` on fixture; this host `400M-1.1G` `<1GB` → `NOT APPLICABLE` or `PREVIEWED` only (helper `ping/info` only, no privileged `APPLY`)** |

Plus legacy frozen `REC-001` `REC-002` `REC-003` `REC-004` `REC-005` `REC-006` `REC-007` `REC-008` (not growing).

**Two real fixes verified historically:** `mssql-server` `systemctl disable` (713 MB, post-reboot `0 failed`) and `NVIDIA 470xx` `dnf swap` + `akmods --force` + `dracut --force` + reboot `00:36` (`lspci CLAIMED` `driver nvidia` `470.256.02` `nvidia-smi 50C` `PRIME`). Both remain `COMPLETED and VERIFIED` (checked `stat /etc/fstab` `2026-08-31 21:19`, `zramctl` `8G 0B`, `lspci` `CLAIMED`, `modinfo` `470.256.02`, `nvidia-smi`, `akonadictl status` `running`, `systemctl is-enabled mssql-server` `disabled`).

No batch, no auto-reboot, no host mutation during discovery.

---

## Limitations

- Metrics may be `unavailable` (`flatpak` `hasFlatpak false` → `available false`, `journalDisk` `1.1G` `<1GB` → `NOT APPLICABLE`, `storage.free` not guessed)
- Real helper (`/run/polaris/helper.sock`) not yet installed — privileged `APPLY` `journal-vacuum` `org.polaris.journal.vacuum` stays `PREVIEWED`/`APPROVAL_REQUIRED` until helper reviewed (intentional, `IpcProtocol` `ping/info` only)
- Synthetic `cpu_prime` benchmark only, login time not yet in `PerformanceBaseline`
- `zram` stable at `0B` used `lzo-rle`, `glxinfo` `DISPLAY=:0` fragile headless
- Legacy `REC-006` static still emitted alongside registry `REC-flatpak-unused` (frozen, future deprecate)

Details: `docs/P19_IMPLEMENTATION_REPORT.md`, `docs/P18_FINAL_REPORT.md`, `docs/OPTIMIZER_GAP_ANALYSIS.md`.

---

## Architecture

```
Providers → PerformanceBaseline → OptimizationRegistry → RecommendationEngine → ExplanationEngine → TransactionStore → ComparisonEngine → AuditLog
   ↓              ↓                       ↓                     ↓                    ↓                ↓
ReadOnlyGuard  MetricMeta(18)      IOptimizationCapability  Bottleneck  StateMachine  MetricComparison
               Flatpak/JournalDisk   Flatpak/Journal     (10 types)  FileSafety  storage.free
                                                     BackupEngine  flatpak/journal
                                                     AuditLog(hash chain)
```

- **Providers** — read-only (`/proc`, `/sys`, `systemd`, `glxinfo`, `flatpak`, `journalctl`)
- **Engines** — pure (`Baseline`, `Bottleneck`, `Benchmark`, `Comparison`, `Recommendation` registry-driven)
- **Capabilities** — `FlatpakUnusedCapability`, `JournalVacuumCapability` (deterministic `stateHash`, `preconditions`, `benefitGB`)
- **Transaction** — `StateMachine` + `TransactionStore` + `BackupEngine` + `TransactionValidator` (`7` fields + `preconditions` map stale)
- **Security** — `ReadOnlyGuard`, `polkit`, `AuditLog`, `IpcProtocol` (`SO_PEERCRED`, `0600` socket), `TransactionLock`, `RecoveryDetector`

Deep dive: `docs/ARCHITECTURE.md` · `docs/TRANSACTION_MODEL.md` · `docs/SECURITY_AUDIT.md` · `docs/OPTIMIZER_GAP_ANALYSIS.md`

---

## Documentation

| Doc | Purpose |
|-----|---------|
| `docs/CLI.md` | Complete CLI reference + realistic example session (READ-ONLY vs creates vs requires approval vs host-mutating) |
| `docs/ARCHITECTURE.md` | Layers, providers, engines, capabilities, registry |
| `docs/TRANSACTION_MODEL.md` | Transaction lifecycle & hardening (`beforeHash`/`preconditions`/`TOCTOU`) |
| `docs/SECURITY_AUDIT.md` | `SECRET_AUDIT: PASS` `password` field rejected, redaction `[REDACTED]`, `grep 0 hits` |
| `docs/P19_PLAN.md` / `docs/P19_IMPLEMENTATION_REPORT.md` | Capability framework design, providers, baseline, comparison, CLI, tests |
| `docs/RELEASE_READINESS_REPORT.md` | What Polaris is, can optimize, real vs fixture, CLI, approval, limitations, 38/38, security, publish readiness |
| `CONTRIBUTING.md` / `SECURITY.md` | How to contribute / report vulnerabilities |

Engineering history: `docs/P2_REPORT.md` … `docs/P18_FINAL_REPORT.md` `docs/P19_PLAN.md` kept as reproducible docs (not root `p2_scan.json` etc. ignored).

---

## Project Status

P1–P19 complete. `38/38` tests pass (`1.33s`). `P19` adds registry + 2 reference caps proven on fixtures; no privileged real-host mutation. See `docs/ROADMAP.md`. Next is not `P20: add tweaks` — next would be `P20: Helper Wiring for Journal Vacuum` only if privileged helper gap proven.

## Version

`0.1.0` — `CMakeLists.txt:2` `project(polaris VERSION 0.1.0)` (`C++20`, `CMAKE_CXX_STANDARD_REQUIRED ON`). Mirrored in `packaging/polaris.spec` `Version:`, `README.md`, `docs/VERSIONING.md`, `docs/PROJECT_STATE.json` `project.version`. Centralized — do not duplicate. See `docs/VERSIONING.md` for `MAJOR.MINOR.PATCH` policy. `0.1.0` remains appropriate pre-`1.0` (registry is new minor functionality, `1.0.0` when `helper` + `Qt GUI` stable).

## License

**MIT** — see `LICENSE` (`Copyright (c) 2026 Mehran Qadirian — Polaris Project`, `SPDX-License-Identifier: MIT`). Compatible with `OpenSSL` (`Apache-2.0`). No GPL conflict.

## Contributing & Security

- Contributions: see `CONTRIBUTING.md`. CI (`.github/workflows/ci.yml` `cmake --fresh`/`cmake --build`/`ctest`/`test ! -f /run/polaris/*`) must be `100%` green.
- Security: see `SECURITY.md` — do not open a public issue with a `PoC`; `grep -R "password" core/ipc` only `validate` rejection, `AuditLog` never `secret123` (`test_p14_ipc_security` `no password logging`).

*Built on Fedora diagnostics 2026-08-31 to 2026-09-01. CLI is first-class; Qt GUI is future work (`gui/` empty, not logic duplication).*
