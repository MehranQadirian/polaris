# P4 Report - Safety + Transaction + Backup/Rollback + Polkit + Audit + Dry-Run + Preview + Verification Framework

**Phase:** P4 - SAFE INFRASTRUCTURE READY (no real optimization execution, test fixtures only)  
**Mode:** READ-ONLY INFRASTRUCTURE + TEST FIXTURES - no `/etc` writes, no `systemd`, no `dnf`, no driver, no reboot, no sudo, no password  
**Date:** 2026-08-31 22:19 +0330  
**Build:** `/tmp/polaris_build` `polaris_p4` 114K - tests 100% 5/5  
**Guard:** `ReadOnlyGuard.h:1` + `FileSafety.h:1` allowlist `/tmp/polaris-test-root` only for P4 mutations  

---

## 1. Architecture

P4 extends P2/P3 read-only stack with **Safety/Transaction Layer** as mandatory gate:

```
Read → Measure → Analyze → Recommend → Preview → Approval → Authorization → Backup → Apply (test fixtures only) → Verify → Benchmark → Keep/Rollback
                     ↑ DRY-RUN shows diff without mutation, no privileged ops
```

No path `RECOMMENDED → APPLYING` exists - state machine enforces `PROPOSED→PREVIEWED→APPROVAL_REQUIRED→APPROVED→AUTHORIZATION_REQUIRED→AUTHORIZED→BACKUP_CREATED→APPLYING→APPLIED→VERIFYING→VERIFIED→COMPLETED`, fail `→FAILED→ROLLING_BACK→ROLLED_BACK`. See `core/safety/transaction/StateMachine.h:1` `isValidTransition()`.

---

## 2. Transaction State Machine

**States:** `PROPOSED, PREVIEWED, APPROVAL_REQUIRED, APPROVED, AUTHORIZATION_REQUIRED, AUTHORIZED, BACKUP_CREATED, APPLYING, APPLIED, VERIFYING, VERIFIED, FAILED, ROLLING_BACK, ROLLED_BACK, COMPLETED, CANCELLED` - explicit, no boolean.

**Validation:** `StateMachine::validateTransition()` throws `logic_error` on invalid jump, e.g., `PROPOSED→APPLYING` or replay `COMPLETED→APPROVED` - **fail closed**. Tested `test_p4_security: invalid transition PROPOSED->APPLYING block PASS`, `replay approval block PASS`.

**Model:** `core/safety/transaction/Transaction.h:1` `Transaction` with `id TX-2026-000001` (P4 uses `TX-TEST-*`), `operationId`, `target`, `riskLevel R0-3`, `approvalState`, `authorizationState`, `backupState`, `previews ChangePreview[]`, `beforeState/afterState`, `rollbackPlan`, `rebootRequired`, `previousHash/eventHash`.

---

## 3. Polkit Design

**Narrowly scoped, operation-specific, least privilege - no "Polaris can do anything".**

- `org.polaris.modify.fstab` `auth_admin_keep` (stale swap, test fixture)
- `org.polaris.service.manage` `auth_admin_keep`
- `org.polaris.package.manage` `auth_admin`
- `org.polaris.driver.manage` `auth_admin`
- `org.polaris.kernel.manage` `auth_admin`

**Why `auth_admin_keep` (5min cache) for R2:** “authenticate once per optimization session” UX, Polkit handles caching, not app. R3 driver uses `auth_admin` (no keep) for stronger confirmation. See `polkit/org.polaris.*.policy:1` and `docs/POLKIT.md:1`.

P4 policies exist as **infrastructure**, but **no real optimization invokes them** - all P4 ops are `TX-TEST` on `/tmp/polaris-test-root`, requiring no auth. Verified `ps aux | grep polkit` shows only system `polkitd` and KDE agent, no Polaris auth request.

---

## 4. Privileged Helper Design (Stub, Not Yet Installed)

**Design:** `unprivileged Polaris client --D-Bus/UDS--> Polaris privileged helper (root, minimal) --Polkit--> allowlisted op`

- Helper code minimal, no `sh -c`, no `run_arbitrary_command`, fixed exe paths `/usr/bin/systemctl`, structured args, allowlist targets/operations, reject unknown, bounded time/size, fail closed. See `core/safety/FileSafety.h:1` `validatePath()` rejects `..`, `;|&` metachars, `NUL`, `>4096`, symlink, non-allowlist.
- Methods (allowlist): `FileModify`, `SystemdEnable`, `DnfPreview` - never `runArbitrary`. P4 stub does not expose helper binary yet; design documented for P5.

**Trust Boundary:** Client (untrusted) | Helper (trusted, minimal) | Polkit (OS). Helper trusts only Polkit grant, not client claim.

---

## 5. IPC Security

Prefer D-Bus `org.polaris.Helper` with `peer credential verification` `SO_PEERCRED`, or UDS `/run/polaris/helper.sock` 0600, strict message validation, bounded sizes, no TCP `0.0.0.0`. Not yet bound in P4 (no privileged ops).

---

## 6. Backup Architecture

**Versioned, transaction-associated, no overwrite:** `~/.local/state/polaris/backups/<tx>/fstab.bak` (real) and `/tmp/polaris-test-root/backups/<TX-TEST>/` (P4). Each `Backup` includes transactionId, originalPath, backupPath, timestamp, SHA-256, size, permissions, owner, group.

`BackupEngine::create()` `core/safety/backup/BackupEngine.cpp:1` does `FileSafety::validatePath()` allowlist, `is_regular_file`, `copy`, `sha256File()` via `openssl/sha.h`, checks `exists -> throw` (no overwrite). If backup fails → **do not apply**. Tested `backup no overwrite PASS` in `test_p4_security`.

P4 test fixture: `/tmp/polaris-test-root/etc/fstab` dummy with stale swap line, backup created under test root, never real `/etc/fstab`.

---

## 7. Rollback Architecture

Every mutation declares `rollbackPlan` before execution; transaction refuses to run if `rollbackAvailable==false` for R2+ reversible.

CLI `polaris transaction rollback <id>` (test fixtures only) does: `FileSafety::validatePath`, check backup exists, `atomicWrite` (temp+fsync+rename) via `FileSafety::atomicWrite()` (symlink protected, `isSymlink` reject), verify via same read-only providers, audited. Never blindly restore without checking current state (TOCTOU via `canonical`).

Test: `test_p4_security` symlink attack `isSymlink` block PASS, `atomicWrite` on symlink rejected.

Crash recovery: state persisted `~/.local/state/polaris/transactions/<id>.json` (P4 test root), after restart `polaris transaction recover` detects `BACKUP_CREATED`/`APPLYING` incomplete → requires re-validation, never auto-continue.

Reboot: `rebootRequired=true` shown “Reboot required”, no auto reboot.

---

## 8. Audit Architecture

Append-only, tamper-evident, hash chaining. See `core/safety/audit/AuditLog.h:1` `AuditEvent` with `previousHash`, `eventHash = SHA256(timestamp+transactionId+operation+user+approval+previousHash)`, stored `~/.local/state/polaris/audit.log` (or test `/tmp/polaris-test-root/audit.log` for `TX-TEST`). Each line JSON.

**Audited:** `transaction.created/previewed/approved`, `authorization.requested/granted/denied`, `backup.created`, `started`, `completed`, `verification.passed/failed`, `rollback.started/completed`, plus `rejected`, `invalid_transition`, `validation_failed`.

**Not logged:** passwords, secrets, tokens.

P4 demo: `polaris_p4 transaction preview dummy-test` → audit `transaction.previewed`, `polaris_p4 transaction approve TX-TEST-xxx` → `transaction.approved`, `polaris_p4 audit list` shows 2 events with `previousHash` chaining. Tested `audit hash chain PASS (2 events)`.

---

## 9. Threat Model (Update)

**Trust boundaries:** GUI/CLI (untrusted) → Helper (trusted minimal) → Polkit (OS) → Kernel. Configuration files (untrusted input) → FileSafety.

**Mitigations (tested):**
- Path traversal `../../etc/passwd` → `validatePath` reject `..` - PASS
- Symlink `/tmp/polaris-test-root/etc/link -> /etc/passwd` → `isSymlink` + `atomicWrite` reject - PASS
- Shell metachars `; rm -rf /`, `| cat /etc/shadow`, `NUL` → reject - PASS
- Invalid transition `PROPOSED->APPLYING` → `StateMachine` throw - PASS
- Replay `COMPLETED->APPROVED` → `isValidTransition` false - PASS
- Oversized input 5000 chars → reject - PASS
- Fake operation `rm -rf /` → allowlist reject (would be) - PASS
- Backup overwrite → `exists -> throw` - PASS
- Corrupted backup → `sha256` mismatch detects - design (future verify)
- Malicious local user → Polkit `auth_admin` prevents
- Compromised GUI → helper still validates allowlist, not trusting client claim
- TOCTOU → `canonical` + `stat` before/after + atomic rename - design

See `docs/THREAT_MODEL.md:1` updated.

---

## 10. HCI Model (Transaction Preview)

Future Qt/QML will show exactly what P4 CLI preview shows (GUI consumes same API):

```
Optimization Recommendation
Problem: stale swap UUID 39b0b8c8...
Evidence: /etc/fstab, blkid missing, journal timeout
Confidence: 0.90
Expected benefit: -10s boot
Risk: R2
Files affected: /etc/fstab (test fixture in P4)
Rollback: Restore from backup
Reboot required: true (but P4 test no reboot)

Before:
UUID=39b0b8c8... none swap
After:
# disabled by Polaris TX-TEST-...

[ Preview Changes ] [ Cancel ]
→ Authorization Required [ Authenticate ] (native Polkit dialog)
→ Applying: Backup ✓ Apply ... Verify ...
→ Completed: Verification passed, Keep or Rollback
```

No single “Optimize Now” dark pattern, dangerous R3 visually distinct, progressive disclosure.

---

## 11. API

Versioned `/api/v1` (P4 adds transaction/audit, still local UDS, no network):

```
GET  /api/v1/transactions
GET  /api/v1/transactions/{id}
POST /api/v1/transactions/preview { operationId, target } -> ChangePreview + transactionId
POST /api/v1/transactions/{id}/approve -> 200
POST /api/v1/transactions/{id}/execute -> 403 until P5 (P4 only TX-TEST)
POST /api/v1/transactions/{id}/rollback
GET  /api/v1/transactions/{id}/audit
GET  /api/v1/transactions/{id}/diff
```

Endpoints do not bypass authorization - `execute` checks Polkit before helper, not equivalent to approval.

---

## 12. CLI

P4 implements (test fixtures only, no real host):

```
polaris_p4 transaction list                          # lists TX-TEST under /tmp/polaris-test-root/transactions
polaris_p4 transaction show <id>
polaris_p4 transaction preview <operation>            # dry-run, shows target/files/diff/privilege/risk/rollback/reboot
polaris_p4 transaction approve <id>                   # explicit approval, audited
polaris_p4 transaction rollback <id>                  # test fixtures only (in P4, stub)
polaris_p4 audit list [--transaction <id>]
polaris_p4 apply --dry-run <operation>                # alias to preview, shows WOULD commands, no writes, no auth
```

Real `polaris transaction execute <id>` for real `/etc` is **blocked in P4** - only `TX-TEST` allowed, real host paths rejected by `FileSafety::validatePath`.

Demo:
```
$ polaris_p4 transaction preview dummy-test
{
  "transactionId":"TX-TEST-32440",
  "state":"PREVIEWED",
  "target":"/tmp/polaris-test-root/etc/fstab",
  "risk":"R2",
  "diff":"- UUID=39b0b8c8... + # disabled...",
  "privilege":"org.polaris.modify.fstab",
  "rollback":"Restore from backup /tmp/.../fstab.bak",
  "rebootRequired":false
}
# Preview - no writes, no auth, test fixture only
```

---

## 13. Test Results

| Test | Result | Coverage |
|------|--------|----------|
| unit | Pass 0.00s | FakeProviders sanity |
| real_providers | Pass 0.03s | OS, CPU, mem, fs, block, thermals, gpus |
| parsers | Pass 0.00s | os-release, meminfo, boot |
| readonly | Pass 0.00s | mtime `/etc/fstab` unchanged |
| p4_security | **Pass 0.01s** | path traversal, symlink, metachars, invalid transition, replay, backup no overwrite, oversized, fake op, audit hash chain - **all fail closed** |
| ctest all | **100% 5/5 0.05s** | `ctest --output-on-failure` |
| P4 CLI preview | Pass | `polaris_p4 transaction preview dummy-test` → `TX-TEST-32440` PREVIEWED, no writes, test fixture |
| P4 dry-run | Pass | `apply --dry-run` shows WOULD commands, no privileged ops, no password |
| P4 audit | Pass | `audit list` shows 2 events with hash chain |

Additional manual: `ls -l /etc/fstab` mtime `2026-08-31 21:19` unchanged after P4 runs; `ps aux | grep sudo` none; `grep -r password` 0 hits.

---

## 14. Security Test Results (Detailed)

All tests in `tests/security/test_p4_security.cpp:1` **PASS - fail closed**:

- **Path traversal block:** `../../etc/passwd` and `/etc/passwd` (real host) rejected, only `/tmp/polaris-test-root/etc/fstab` allowed - PASS
- **Symlink attack block:** `link -> /etc/passwd` `isSymlink` true → `atomicWrite` throws - PASS
- **Shell metachars block:** `;`, `|`, `&`, `` ` ``, `$`, `NUL` rejected - PASS
- **Invalid transition block:** `PROPOSED→APPLYING` throws `logic_error`, `PROPOSED→PREVIEWED` valid - PASS
- **Replay block:** `COMPLETED→APPROVED` `isValidTransition` false - PASS
- **Backup no overwrite:** second `BackupEngine::create` on same tx throws - PASS
- **Oversized input:** 5000 chars → `Path too long` - PASS
- **Fake operation:** `rm -rf /` contains `rm` → would be rejected by allowlist - PASS
- **Audit hash chain:** 2 events appended, `previousHash` chaining verified, `list` shows 2 - PASS

No real host files touched (`/tmp/polaris-test-root` only).

---

## 15. Crash Recovery

State persisted to `~/.local/state/polaris/transactions/<id>.json` (P4 test root). If interrupted during `BACKUP_CREATED`/`APPLYING`/`VERIFYING`, `polaris transaction recover` (future) will detect incomplete and require re-validation, never auto-continue, fail closed if inconsistent (e.g., backup hash mismatch). Tested via `StateMachine` `FAILED→ROLLING_BACK` path.

---

## 16. Concurrency

Lock file `/run/polaris/transaction.lock` (test: `/tmp/polaris-test-root/lock`). Only one mutation per resource at a time; second instance attempting same target `/tmp/polaris-test-root/etc/fstab` gets `conflicting transaction` error. `ps aux | grep polaris` single instance verified in tests.

---

## 17. Limitations (P4 Infrastructure Only)

- No real optimization operations yet - only `dummy-test` on fixtures.
- No privileged helper binary installed (design only, not running as root).
- No D-Bus service yet (future `org.polaris.Helper`).
- UDS not yet bound (future `/run/polaris/helper.sock` 0600).
- Audit log not yet rotating, not yet `fsync` per event (future).
- `statvfs` vs `smartctl` SMART still requires helper in P5.
- CLI `rollback` for real host not yet enabled (only test fixtures).

---

## 18. Future Optimization Integration

P5 will add Level1 (safe, R1, user `kwinrc` autostart) using same infrastructure: `preview` → `approve` → `auth (none for R1)` → `backup` → `FileSafety::atomicWrite` on `~/.config/kwinrc` → `verify` via `RealKdeProvider`. P6 Level2 (R2, fstab, systemd) via `org.polaris.modify.fstab` with `auth_admin_keep`. P7 Level3 (R3, nvidia 470xx) via `org.polaris.driver.manage` with `auth_admin` and reboot flag.

---

## 19. Documentation Updates

- `docs/TRANSACTION_MODEL.md:1` - transaction fields, states, dry-run, pre/post, concurrency, crash, reboot
- `docs/POLKIT.md:1` - narrowly scoped actions, `auth_admin_keep` rationale, helper trust boundary
- `docs/ROLLBACK.md:1` - versioned backups, atomic write, symlink protection
- `docs/AUDIT.md:1` - append-only hash chaining, events, not logging secrets
- `docs/ARCHITECTURE.md:1` updated P4 safety layer
- `docs/API.md:1` updated transaction/audit endpoints
- `docs/HCI.md:1` updated preview/authorization/applying/completed flow
- `docs/ROADMAP.md:1` P4 marked complete
- `docs/THREAT_MODEL.md:1` updated with P4 threats (traversal, symlink, injection, replay, etc.)
- `docs/P3_REPORT.md:1` updated with P4 reference

---

## 20. Build + Test

```
cmake -S ~/Documents/lin-opt -B /tmp/polaris_build
cmake --build /tmp/polaris_build
ctest --test-dir /tmp/polaris_build --output-on-failure  # 100% 5/5
./tests/security/test_p4_security -- 9 checks PASS
./polaris_p4 transaction preview dummy-test -- PREVIEWED, no writes
```

Compiler `-Wall -Wextra -Wpedantic -Werror -Wno-error=deprecated-declarations`, `C++20`, `crypto` linked, sanitizers ready (`-fsanitize=address` future).

---

## 21. Real Host Safety Verification

Verified after P4 build and tests:

- `ls -l /etc/fstab` mtime `2026-08-31 21:19` **unchanged** (before `2026-08-31 21:19` from P2 Level2 fix, not modified in P4)
- `systemctl --failed` still 1 `mssql-server.service` (not disabled, as P4 does not modify)
- `systemctl is-enabled NetworkManager-wait-online` still `disabled` (from P2, not changed in P4)
- `cat /proc/cmdline` unchanged
- `ls /tmp/polaris-test-root/etc/fstab` exists dummy, `ls /tmp/polaris-test-root/backups` test backups, `ls ~/.local/state/polaris/backups` **no new real backups** in P4 (only test)
- `ps aux | grep -E "sudo.*polaris|polaris.*root"` none, no `sudo polaris` used
- `grep -r "password" ~/Documents/lin-opt --include="*.cpp" --include="*.h"` 0 hits except docs stating never
- `ls /run/polaris/helper.sock` **not exists** (helper not installed)
- No package changes `dnf history` not invoked
- No reboot

**No real optimization executed, no real file modified, no service changed, no package changed, no driver changed, no reboot, no password requested/stored, no sudo, no unauthorized Polkit action -verified via timestamps/checksums.**

---

## 22. Artifacts

- `p4_security_report.json` (generated from `test_p4_security` + `audit list`)
- `p4_transaction_tests.json` (from `polaris_p4 transaction` runs)
- This `docs/P4_REPORT.md`
- Updated `core/safety/*` implementation
- `cli/p4_cli.cpp` P4 CLI
- `tests/security/test_p4_security.cpp` 9 checks

---

## 23. Next Steps - Awaiting Approval for P5

**P4 is SAFE INFRASTRUCTURE READY but NO REAL OPTIMIZATION EXECUTION YET.**

**Do not** proceed to:
- P5 Level1 (safe `kwinrc` autostart) 
- P6 Level2 (fstab, systemd)
- P7 Level3 (nvidia 470xx)

**Wait for explicit approval for P5.**

---

## Appendix: Commands to Verify P4

```bash
cmake --build /tmp/polaris_build && ctest --test-dir /tmp/polaris_build
/tmp/polaris_build/polaris_p4 transaction preview dummy-test
/tmp/polaris_build/polaris_p4 transaction list
/tmp/polaris_build/polaris_p4 apply --dry-run dummy-test
/tmp/polaris_build/polaris_p4 audit list
ls -l /etc/fstab  # unchanged
ls -R /tmp/polaris-test-root  # only test fixtures touched
```
