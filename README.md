# Polaris: Linux Performance & System Health Platform

**Target:** Fedora Linux + KDE Plasma | **Version:** 0.1.0 | **Status:** P18 — Complete with limitations | **Tests:** 33/33 passing

> **A note from the author — Mehran Qadirian**
>
> *"I wanted to optimize my Linux system, but I didn't want a tool that blindly disables services, runs random shell commands, or tells me a change is good just because a number got smaller."*
>
> *"So I built Polaris for myself — a safety-first, evidence-driven platform that measures the actual machine, explains what it found, and only changes the host after explicit approval, backup, and verification."*

Polaris is a safety-first, evidence-driven tool to understand and improve Linux system health. It measures your real hardware, explains what it found, and only changes the system after you explicitly approve, with automatic backup and verification.

> **Polaris is not** a blind debloat script. It never disables services automatically, never runs `curl | bash`, and never assumes "smaller number = better." Every recommendation requires evidence, and every change requires your approval.

---

## Why Polaris?

On a daily Fedora KDE machine, Polaris answers:

- How healthy is my system **right now** on **this** hardware?
- What is actually slowing it down — with evidence and confidence?
- What can I safely change, what is the benefit/risk, and can I undo it?
- Did the change actually help, or did it cause a regression?

**Core principle:**

```
READ → MEASURE → ANALYZE → EXPLAIN → RECOMMEND → PREVIEW → APPROVE → BACKUP → APPLY → VERIFY → COMPARE → AUDIT
```

If any step cannot be proven safe, Polaris fails closed and does nothing.

How it evolved: P1–P18 built the platform step-by-step — from read-only measurement (P2), to bottleneck detection (P3), safety/backup/audit (P4), real pilot fixes (P5–P7), regression detection (P11), hardening (P12), user profile awareness (P13), IPC security (P14), and explainability (P16). See `docs/ROADMAP.md` and `docs/P18_FINAL_REPORT.md` for the full history.

---

## Safety at a Glance

- **One change at a time** — transactions are `PREVIEWED → APPROVED → BACKUP → APPLIED`, never batched.
- **Preview is not approval** — viewing a recommendation does not authorize it. You must approve the exact `transactionId`.
- **Backup before mutation** — SHA-256 verified, `fsync`'d backup with no overwrite. If backup fails, no change happens.
- **Fail-closed** — invalid state transitions are rejected. Stale system state (kernel, packages, file hash changed) → `FAILED`.
- **File safety** — allowlisted paths only, no `sh -c`, no shell metachars, symlink and traversal checks.
- **No hidden auth** — `execv` with fixed binary paths, `polkit` `auth_admin_keep`, audit log is hash-chained and `fsync`'d.

---

## Quick Start

**Requirements:** Fedora 44, `cmake >=3.28`, `g++ >=14`, `openssl-devel`, `ninja` (optional)

```bash
git clone https://github.com/MehranQadirian/polaris.git
cd polaris
cmake -S . -B build --fresh -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j$(nproc)
ctest --test-dir build --output-on-failure  # expect 33/33 100%
```

Optional install:
```bash
sudo cmake --install build
# or build RPM: rpmbuild -ba packaging/polaris.spec
```

---

## How to Use

### 1. Discover (always read-only, no sudo)

```bash
./build/polaris_real --json | python3 -m json.tool | head -n 50  # full hardware scan
./build/polaris_p3                                               # baseline + bottlenecks + recommendations
```

### 2. Explain (read-only, no changes)

```bash
./build/polaris_p4 profile show --json
./build/polaris_p4 explain akonadi-disable --json        # why now, what will change/not change
./build/polaris_p4 explain bluetooth-disable --verbose   # human-readable
```

Polaris respects your workflow: e.g. `akonadi-disable` is **BLOCKED** if you use KMail/Kontact (see `profile set` below).

### 3. Preview & Approve (safe, test fixtures)

```bash
./build/polaris_p4 transaction preview dummy-test       # creates PREVIEWED transaction on /tmp fixture
./build/polaris_p4 transaction list
./build/polaris_p4 transaction show TX-TEST-123 --json
./build/polaris_p4 transaction approve TX-TEST-123      # records explicit approval, does not yet apply
./build/polaris_p4 transaction explain TX-TEST-123 --verbose
./build/polaris_p4 apply --dry-run dummy-test           # verifies dry-run writes nothing
./build/polaris_p4 audit list                           # hash-chained audit log
```

### 4. Profile (tell Polaris about your workflow)

```bash
./build/polaris_p4 profile set usesKMail yes --json
./build/polaris_p4 profile set usesBluetooth no --json
# fields: usesKMail, usesKontact, usesKOrganizer, usesBluetooth, usesPrinting, usesAvahi, usesCups, usesAkonadi
# values: yes / no / unknown (default)
```

> **Safe vs. mutating:** All commands above are read-only except `profile set` (writes `~/.local/state/polaris/profile.json` with `0600`) and `transaction approve` (records approval). No privileged system mutation is enabled in P18 — `P14` IPC allowlist is `ping`/`info` only.

Full command reference: `docs/CLI_USAGE.md`

---

## Example Workflow

```bash
# 1. Baseline — no mutation
./build/polaris_p3

# 2. Explain candidate
./build/polaris_p4 explain akonadi-disable --json

# 3. If you use KMail, tell Polaris so it blocks the suggestion
./build/polaris_p4 profile set usesKMail yes

# 4. Preview on test fixture
./build/polaris_p4 transaction preview dummy-test
./build/polaris_p4 transaction explain TX-TEST-xxxxx --verbose

# 5. Approve (explicit) and verify audit
./build/polaris_p4 transaction approve TX-TEST-xxxxx
./build/polaris_p4 audit list
```

See `docs/CLI_USAGE.md` for the complete example with `compare`/`regression` after a reboot (P7 NVIDIA case).

---

## What It Can Do Today (P18)

- **Real read-only scan** of CPU, memory, storage, GPU, thermals, systemd via `/proc`/`sys`/`D-Bus`
- **Baseline & Bottlenecks** — 15 metrics, 10 bottleneck types, critical-chain vs background
- **Recommendations** with evidence, confidence, benefit, risk, rollback
- **Transaction safety** — state machine (16 states), `FileSafety`, `BackupEngine` (SHA-256), `AuditLog` (hash chain)
- **Two real fixes verified:** `mssql-server` disable (713 MB saved) and NVIDIA 470xx driver swap (MX130 `UNCLAIMED` → `CLAIMED`)
- **Post-change comparison** — before/after baseline with regression thresholds (boot +10%, memory -1 GB, thermal +15 °C)
- **Explainability** — `WHY NOW / WHAT WILL CHANGE / WHAT WILL NOT CHANGE / REJECTION CONDITIONS` for every candidate and transaction
- **Profile-aware** — blocks suggestions that conflict with your workflow (e.g. Akonadi if you use KMail)

No batch changes, no automatic reboot, no host mutation during discovery.

---

## Limitations

- Some metrics may be `unavailable` — not guessed
- Real helper (`/run/polaris/helper.sock`) not yet installed — privileged `apply` is disabled (intentional)
- Synthetic `cpu_prime` benchmark only, `login` time not yet in baseline
- `zram` stable at 0 B, `glxinfo` requires `DISPLAY=:0`

Details: `docs/P18_FINAL_REPORT.md` and `docs/P18_FINAL_STATE.json`

---

## Architecture

```
COLLECT → BASELINE → DETECT → CLASSIFY → EXPLAIN → RANK → PREVIEW → APPROVAL → BACKUP → APPLY → VERIFY → COMPARE → AUDIT
```

- **Providers** — read-only collectors (`/proc`, `/sys`, `systemd`, `glxinfo`)
- **Engines** — pure analysis (`Baseline`, `Bottleneck`, `Benchmark`, `Comparison`, `Recommendation`)
- **Transaction** — `StateMachine` + `TransactionStore` + `BackupEngine` + `FileSafety`
- **Security** — `ReadOnlyGuard`, `polkit`, `AuditLog`, `IpcProtocol` (`SO_PEERCRED`, `0600` socket)

Deep dive: `docs/ARCHITECTURE.md` · `docs/TRANSACTION_MODEL.md` · `docs/SECURITY_AUDIT.md`

---

## Documentation

| Doc | Purpose |
|-----|---------|
| `docs/CLI_USAGE.md` | Complete CLI reference |
| `docs/ARCHITECTURE.md` | Layers, providers, engines |
| `docs/TRANSACTION_MODEL.md` | Transaction lifecycle & hardening |
| `docs/P18_FINAL_REPORT.md` | Final validation & evidence |
| `CONTRIBUTING.md` | How to contribute |
| `SECURITY.md` | How to report vulnerabilities |

---

## Project Status

Roadmap P1–P18 complete. `33/33` tests pass (0.70s). No P19 planned — `P18 FINAL_REPORT` recommendation is `STOP` (only `NO_ACTION_RECOMMENDED` candidates remain on current host). See `docs/ROADMAP.md`.

## Version

`0.1.0` — `CMakeLists.txt:2` (`C++20`, `CMAKE_CXX_STANDARD_REQUIRED ON`). See `CHANGELOG.md` and `docs/VERSIONING.md`.

## License

**MIT** — see `LICENSE`. Compatible with `OpenSSL` (`Apache-2.0`). No GPL conflict.

## Contributing & Security

- Contributions: see `CONTRIBUTING.md`. CI (`.github/workflows/ci.yml`) must be `100%` green.
- Security issues: see `SECURITY.md` — do not open a public issue with a PoC.

*Built on Fedora KDE diagnostics 2026-08-31 to 2026-09-01. CLI is the primary interface; Qt GUI is future work (`gui/` empty).*
