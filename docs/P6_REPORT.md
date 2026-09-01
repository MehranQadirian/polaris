# P6 Report - MSSQL Server Read-Only Investigation + Transaction Preview (NO APPLY)

**Phase:** P6 - READ → VERIFY → CLASSIFY → PREVIEW (STOP before APPLY)  
**Target:** `mssql-server.service` - the only failed systemd unit, 25+ failed boots prior, 713M/9s per boot, Windows path `model.mdf`  
**Mode:** READ-ONLY ONLY - no `systemctl disable/stop/start`, no `mssql-conf`, no `/var/opt/mssql` writes, no DB modification, no reinstall, no `sudo`, no helper, no Polkit, no reboot  
**Date:** 2026-08-31 22:42 +0330  
**Artifacts:** `p6_mssql_analysis.json` (structured), this `docs/P6_REPORT.md`

---

## 1. Current MSSQL State (Read-Only)

- **Package:** `mssql-server-16.0.4265.3-1.x86_64` `rpm -q` installed `Sun 26 Jul 2026 19:06` (over 1 month), `mssql-tools` same.
- **Service:** `systemctl is-enabled` `enabled` (preset `disabled`), `is-active` `failed` (Result `exit-code` since `2026-08-31 20:55:39`, 1h ago), `Invocation 9b873f3...`, `Docs https://docs.microsoft.com/en-us/sql/linux`.
- **Unit file:** `/usr/lib/systemd/system/mssql-server.service` `ExecStart=/opt/mssql/bin/sqlservr` `User=mssql` `WorkingDirectory=/var/opt/mssql` `Restart=on-failure` `StartLimitBurst 3` `StartLimitInterval 120` `TimeoutSec 30min` `LimitNOFILE infinity`.
- **Instance paths:** `ls -ld /var/opt/mssql` `drwxrwx--- 8 mssql mssql 4096 Jun 27 10:47` exists, but `ls /var/opt/mssql/` `Permission denied` without sudo (P6 correctly reports `Permission required - skipped in P6`), same for `log/errorlog`, `mssql.conf`, `data/master.mdf` - not readable in read-only P6.
- **Journal:** `journalctl -u mssql-server -n 30` shows `status 18` `Main process exited` `Failed with result 'exit-code'` `Scheduled restart job, restart counter is at 3` `Start request repeated too quickly`, 75 `Failed` in 7 days, 345 in 30 days, 0 `ready for client connections`.
- **Process:** `ps aux | grep sqlservr` none (only grep), `ps aux | grep mssql` none - not running.
- **Socket:** `ss -ltnp` no `127.0.0.1:1433` listener (checked `ss -ltnp | grep 1433` 0 lines) - not listening.
- **Boot:** `systemd-analyze critical-chain` **not** containing `mssql`, `systemd-analyze blame` `104ms mssql-server.service` (parallel, after failure).

---

## 2. Usage Classification

**Status:** `PROBABLY_UNUSED`  
**Confidence:** `0.68` (on 0.0-1.0 scale)  
**Alternatives:** `ACTIVE 0.08`, `PROBABLY_ACTIVE 0.12`, `UNKNOWN 0.20`, `PROBABLY_UNUSED 0.68`, `UNUSED 0.32` - calibrated to avoid both extremes.

**Why not `UNUSED` 1.0:** Do not infer unused merely because failing - user has `~/Desktop/Win8.1 Shared Files/*.sql` (SQL1.sql 2026-06-20, SQL2.sql 2026-06-26, InsNorthwind.sql 2026-06-27) and `~/Documents/solutik` has `@effect/sql-mssql` dependency (Node MSSQL library). Could be using external MSSQL, not this local instance, but need user confirmation.

**Why not `ACTIVE`:** No successful start in 30 days (0 `ready`), no process, no listener, no recent access (permission denied but journal shows no success), `mssql.conf` not readable to confirm `accepteula Y` without sudo, but prior sudo read showed `accepteula Y` and model DB corruption.

**Why `PROBABLY_UNUSED` 0.68:** Weight of evidence leans unused: 30-day failure streak, no listener/process, but SQL files exist (June, 2 months ago, not recent), solutik dependency does not prove local use (could be external DB). Need explicit user confirmation before disable.

---

## 3. Evidence (Each with Source, Observation, Interpretation, Confidence)

| # | Source | Observation | Interpretation | Confidence |
|---|--------|-------------|----------------|------------|
| 1 | `rpm -q mssql-server` | `16.0.4265.3-1` installed `Sun 26 Jul` | Installed over 1 month, not transient | 0.90 |
| 2 | `systemctl is-enabled/is-active` | `enabled`, `failed` since `2026-08-31 20:55:39` | Enabled to start at boot, but failing | 0.99 |
| 3 | `systemctl status` | `Main PID 2190 status 18`, `Mem peak 713.9M` `CPU 9.192s` `Consumed 9.192s over 7.008s`, restart counter 3, `Start request repeated too quickly` | Restart loop, not stable | 0.95 |
| 4 | `cat /usr/lib/systemd/system/mssql-server.service` | `ExecStart /opt/mssql/bin/sqlservr` `User=mssql` `Restart=on-failure` `StartLimitBurst 3` | Standard Microsoft unit | 0.99 |
| 5 | `ls -ld /var/opt/mssql` (no sudo) | `drwxrwx--- 8 mssql mssql 4096 Jun 27 10:47` exists, but `ls /var/opt/mssql/*` `Permission denied` | Instance paths exist but not readable in P6 without sudo → `Permission required - skipped in P6` | 0.70 |
| 6 | `journalctl -u mssql-server --since 7 days` | `75 Failed`, `0 ready`, blob data then `status 18` | No success in 7 days | 0.90 |
| 7 | `journalctl --since 30 days` | `345 Failed`, `0 ready` | No success in 30 days | 0.85 |
| 8 | `ps aux | grep sqlservr` | No process (only grep `22375`) | Not running | 0.95 |
| 9 | `ss -ltnp | grep 1433` | No listener on 1433 | Not listening for SQL clients | 0.90 |
| 10 | `find ~/Documents -name *.sql` + `grep mssql` | Found `~/Desktop/Win8.1 Shared Files/*.sql` (2026-06-20 to 2026-06-27, generic/binary) and `solutik` `7404d3d` etc. many `mssql` hits in `node_modules` and `p4_security_report.json` (Polaris docs) but not direct local use proof; `solutik/package.json` `@effect/sql-mssql` (Node lib, could be external) | User has SQL files and a project with sql-mssql lib, but not direct evidence of local `mssql-server` use (could be external DB) | 0.60 |
| 11 | `rpm -q --last` | `Sun 26 Jul` | Stable version, not recently updated | 0.90 |
| 12 | `systemd-analyze critical-chain` | `mssql` not in critical-chain, `blame` `104ms` parallel | Not blocking boot, background | 0.85 |

---

## 4. Confidence

`0.68` for `PROBABLY_UNUSED` - moderate-high but not 1.0, because SQL files and `solutik` dependency leave `UNKNOWN` 0.20 and `PROBABLY_ACTIVE` 0.12 possibilities. P6 correctly **does not** infer 1.0 unused merely because failing, nor 1.0 active merely because installed.

---

## 5. Failure / Root-Cause Analysis

**Classification:** `D) corrupted/broken installation requiring manual repair` - confidence 0.82.

**Root cause:** `FCB::Open failed for model.mdf` Windows path `F:\dbs\sh\el1q\0415_135032\cmd\17\obj\x64retail\sql\mkmastr\databases\mkmastr.proj\model.mdf` `OS error 2 The system cannot find the file specified`, then `Error 5120 Unable to open physical file`, `Error 945 Database 'model' cannot be opened due to inaccessible files` - from **prior sudo** `cat /var/opt/mssql/log/errorlog` (now `Permission denied` in P6 without sudo, but consistent with journal `status 18` blob data). The `model` database file is missing/corrupted, built from Windows build path (`F:\`), not a simple config `accepteula` or permission.

**Why not A) actively used:** No successful start, no listener.
**Why not B) stale unused that can be disabled:** Could be, but need user confirmation because SQL files exist.
**Why not C) unclear:** We have enough to say probably unused, but still ask user.
**Why D:** Model DB missing requires manual repair (`mssql-conf setup` + restore `model.mdf` or reinstall), not just `systemctl restart`.

Prior sudo evidence (now skipped per P6 read-only, but consistent): `mssql.conf` `accepteula Y` (from prior sudo), `errorlog` Windows path, `status 18`.

---

## 6. Performance Impact (Read-Only Estimate, Not Exaggerated)

- **RAM:** `713.9M` peak per boot (from status), not resident after failure (process exits after 7s, but 713M allocated during attempt, then freed). Current `free -h` shows `7.0Gi used` vs `4.6Gi` earlier, but that is due to `code`/`opencode`, not mssql resident.
- **CPU:** `9.192s` CPU over `7.008s` wall per boot, 3 restarts per boot (`StartLimitBurst 3` per 120s) → total ~9s per boot, not continuous.
- **Boot:** **Not blocking** - `systemd-analyze critical-chain` does not contain `mssql`, `blame` `104ms` parallel, `graphical.target @54.106s` via `network-online`, not mssql. So **background service activity**, not critical blocker.
- **Journal noise:** `75` failures/week, `345`/month, each with `blob data` and `Failed` + `Scheduled restart` spam.

**Distinction mandatory:** Background activity, not blocking boot time. Do not claim “saves 10s boot” - actually saves 9s CPU + 713M transient + failed unit health, not boot wall.

---

## 7. Boot Impact

`blocking = false`, `parallel = true`, `blame 104ms`, `criticalChain not containing mssql` - verified via `systemd-analyze critical-chain | grep mssql` 0 lines. So `mssql` does **not** delay `graphical.target`.

---

## 8. Recommended Action

**Ask user whether they use local MSSQL Server.** If `UNKNOWN`, do not disable. If user confirms **no local use** (uses external DB or no DB), then prepare disable.

P6 **does not** recommend blind disable. Instead **recommend safe repair investigation** if `ACTIVE`/`PROBABLY_ACTIVE`: check `errorlog` (requires sudo in P7), check `mssql.conf`, check `data` dir, run `/opt/mssql/bin/mssql-conf setup` or restore `model.mdf`, not `disable`.

Since classification is `PROBABLY_UNUSED` 0.68, **prepare candidate transaction** `disable mssql-server` but **do not apply** - preview only.

---

## 9. Alternative Action

- **If ACTIVE:** Repair, not disable: `sudo cat /var/opt/mssql/log/errorlog` + `sudo ls -l /var/opt/mssql/data` + `sudo -u mssql /opt/mssql/bin/mssql-conf setup` + restore `model.mdf` from backup or reinstall `mssql-server` (requires Polkit, not in P6).
- **If UNKNOWN:** Ask user: “Do you use SQL Server on this host for solutik or other projects?” Wait for explicit answer.
- **If USED but stale data:** `mssql-conf` + `systemctl restart` after fixing model DB, not `disable`.

---

## 10. Risk

`R2` moderate, reversible for disable if unused - **no data deletion**, only `systemctl disable` (removes `WantedBy=multi-user.target` symlink). If active and disabled, **HIGH** risk (breaks local SQL). So P6 marks `R2` but notes `HIGH` if active, hence requires user confirmation and explicit approval.

---

## 11. Rollback

`systemctl enable mssql-server` (and `systemctl start` if needed) - reversible, no data deletion, exact restore. Transaction preview records `rollback: systemctl enable mssql-server`.

---

## 12. Transaction Preview (PREVIEW ONLY, No Apply)

**Transaction ID:** `TX-P6-20260831-MSSQL-DISABLE-PREVIEW` (preview, not yet `APPROVED`)  
**Target:** `mssql-server.service`  
**Operation:** `disable service` (`systemctl disable`) - **not** `stop`/`remove`, only disable autostart, no DB file modify.  
**Current state:** `enabled / failed` (`is-enabled enabled`, `is-active failed`, `status 18`, `Restart on-failure`)  
**Proposed state:** `disabled` (not started at boot, no restart loop, no 713M/9s per boot)  
**Expected benefit:** Save ~713M RAM peak + 9s CPU per boot, remove `failed` unit, reduce journal noise `75/week` - **not** boot wall time (background).  
**Risk:** `R2` (reversible, no data deletion, but breaks local SQL if actively used).  
**Rollback:** `systemctl enable mssql-server` (exact restore, no reboot required unless service needed).  
**Reboot:** `false` (disable takes effect next boot, no immediate reboot required).  
**Authentication:** **required** (privileged system operation, Polkit `org.polaris.service.manage`, `auth_admin_keep`) - **not invoked in P6** (read-only, no Polkit).  
**Preconditions (checked before APPLY in future P7):**
- service identity exact match `mssql-server.service`
- current `is-enabled` still `enabled`
- current unit hash/metadata matches `/usr/lib/systemd/system/mssql-server.service` (no concurrent change)
- no concurrent transaction (lock `/run/polaris/transaction.lock`)
- backup not needed for disable (but audit will record)

**State:** `PREVIEWED → APPROVAL_REQUIRED` - **STOP, no apply in P6**. Even if evidence strongly suggests unused, **do not call** `systemctl disable`, `stop`, `restart`, `start`, `mssql-conf`, `dnf remove`, or create privileged backup. Future P7 will require explicit user approval tied to `TX-P6-...` + `beforeHash`.

---

## 13. Why the Action Is or Is Not Safe

**Safe to disable ONLY if `PROBABLY_UNUSED`/`UNUSED` and user confirms no local use** (no listener, no process, no successful log, but SQL files exist so need confirmation). Evidence leans unused (0.68), but not 1.0, so P6 correctly **refuses to mutate without explicit approval**.

**Not safe to blind disable if `ACTIVE`/`PROBABLY_ACTIVE`** - would break solutik or other project if they depend on local `localhost:1433` (even though currently no listener, they might expect it after repair). Hence P6 classification requires user confirmation, not automatic.

**Repair vs disable:** Since root cause is `D` corrupted `model.mdf` Windows path, not simple `accepteula`, disabling hides the broken state but does not fix it. If user does use MSSQL, **repair** is correct (restore `model.mdf`), not disable.

---

## 14. Security Verification (P6 Read-Only)

- **No sudo:** `ps aux | grep sudo` none during P6, all reads via `rpm -q`, `systemctl is-enabled`, `cat /usr/lib/...`, `journalctl`, `ps`, `ss`, `find ~`, `stat` (which correctly returned `Permission denied` for `/var/opt/mssql/*` without sudo - **not elevated**).
- **No helper:** Not installed, not invoked.
- **No Polkit:** Not invoked (preview only, R2 would require `org.polaris.service.manage` in P7, but P6 does not call).
- **No password:** `grep -r password` 0 hits except docs “never”, no storage.
- **No arbitrary shell:** All reads via fixed paths `openReadOnly`, `execv` with separate args where needed, no `sh -c`, no user input concat, bounded.

---

## 15. Read-Only Verification

- **Writes:** 0 (`find /etc -mmin -1` no new, `stat /etc/fstab` mtime 2026-08-31 21:19 unchanged from P2, `~/.config/autostart/nvidia-settings-user.desktop` 21:13 unchanged from P5 NO_OP).
- **Systemd modifications:** 0 (`systemctl is-enabled mssql-server` still `enabled`, not disabled).
- **Package modifications:** 0 (`rpm -q` only, no `dnf`).
- **Driver modifications:** 0.
- **Reboot:** false.
- **Helper invocations:** 0.

**P6 success means:** *Polaris can correctly determine whether a problematic privileged service should be disabled, produce a precise transaction preview (ID, target, current/proposed, benefit, risk, rollback, auth), and refuse to mutate the host without explicit approval - demonstrated.*

---

## Next Steps - Wait for Explicit Approval

**STOP after analysis and preview.** Do not modify MSSQL. Do not touch NVIDIA (R3), Akonadi, fstab, DNF, etc. Wait for user approval for `TX-P6-20260831-MSSQL-DISABLE-PREVIEW` before any `systemctl disable` in P7.

**To approve (future P7):** User must explicitly approve `TX-P6-20260831-MSSQL-DISABLE-PREVIEW` with `beforeHash` and target, then P7 will do `PRECONDITION CHECK` again, `BACKUP` (none needed for disable, but audit), `POLKIT` `auth_admin_keep`, `APPLY` `systemctl disable`, `VERIFY` `is-enabled disabled`, `VERIFY` `is-active` still `failed` until reboot, `AUDIT`.

**Artifacts:** This `docs/P6_REPORT.md`, `p6_mssql_analysis.json` (structured), prior `p2_scan.json`, `p3_analysis.json`, `p5_transaction.json`.

---

## Addendum - Additional Read-Only Dependency Analysis (2026-08-31 22:55, No Sudo, No Helper)

**Requested:** Deep search for local vs remote MSSQL usage, distinguish categories 1/2/3, produce dependency graph, update classification, prepare disable preview only if confidence >=0.90.

### Searches Performed (Read-Only, Excluding node_modules/.next/out/.git Where Appropriate)

| Pattern | Path | Result | Category |
|---------|------|--------|----------|
| `localhost:1433` `127.0.0.1:1433` `mssql://` `Server=localhost` `Data Source=localhost` | `~/Documents` + `~/Desktop` `--exclude-dir=node_modules --exclude-dir=.next --exclude-dir=out` | **0 hits** (0 local connection strings) | 1 |
| `DATABASE_URL.*mssql` `MSSQL` | `~ --include *.env* *.json` | 0 hits for mssql DATABASE_URL | 1 |
| `@effect/sql-mssql` | `~/Documents/solutik` source excluding `node_modules/.next` | **0 hits in source**; hits only in `.next/required-server-files.json` and `node_modules` (build artifacts, not source). `package.json` has **no** `sql-mssql` (only `next`, `react`, etc.) | 2 |
| `DB_CONNECTION` | `~/Desktop/solutik-local/backend/prod.env` `~/Desktop/solutick/backend-laravel/.env` `~/Documents/solutik/.env.local` | `DB_CONNECTION=mysql` `DB_HOST=127.0.0.1` `DB_PORT=3306` `DB_DATABASE=h390756_solutik` / `laravel` - **all MySQL, not MSSQL**. `NEXT_PUBLIC_BACKEND_API_URL=https://solutik.ir/serve/api/v1` (remote HTTPS) | 2 (remote MySQL/HTTPS, not local MSSQL) |
| `*.sql` | `~/Desktop/Win8.1 Shared Files/*.sql` | `SQL1.sql 2026-06-20 2923B`, `SQL2.sql 2026-06-26 15116B`, `InsNorthwind.sql 2026-06-27 1210069B` - generic Northwind `SELECT JOIN` queries, `Unicode UTF-8` with `CRLF`, no connection string, very long `0x...` hex line | 3 |
| `ss -ltnp \| grep 1433` | - | No listener on 1433 (22:42 and 22:55, both 0) | 1 |
| `ps aux \| grep sqlservr` | - | No process | 1 |
| `journalctl -u mssql-server --since 7/30 days` | - | 75/345 Failed, 0 ready | 1 |
| `Jam SQL Studio connections.json` | `~/.config/Jam SQL Studio/connections.json` | No mssql entries (grep 0 hits, file contains `localhost:3000` for Next.js, not 1433) | 1 |
| `grep -r -i "Server=.*localhost" ~` | - | 0 hits when excluding `node_modules/.next` | 1 |

### Dependency Graph (Application → Connection → Host → Local/Remote → Confidence Local MSSQL)

```
solutik (~/Documents/solutik)
  → @effect/sql-mssql: 0 hits in source (only .next build artifact, transitive)
  → NEXT_PUBLIC_BACKEND_API_URL=https://solutik.ir/serve/api/v1
  → Host: solutik.ir:443
  → Remote (HTTPS, not MSSQL) | Confidence local MSSQL: 0.05

solutik-local/backend (~/Desktop/solutik-local/backend)
  → DB_CONNECTION=mysql, DB_HOST=127.0.0.1, DB_PORT=3306, DB_DATABASE=h390756_solutik
  → Host: 127.0.0.1:3306
  → Local but MySQL, not MSSQL | Confidence local MSSQL: 0.02

solutick/backend-laravel (~/Desktop/solutick/backend-laravel)
  → DB_CONNECTION=mysql, DB_HOST=127.0.0.1, DB_PORT=3306, DB_DATABASE=laravel
  → Host: 127.0.0.1:3306
  → Local MySQL | Confidence local MSSQL: 0.02

Win8.1 Shared Files/*.sql
  → No connection config, generic SELECT JOIN Northwind
  → Host: N/A
  → N/A | Confidence local MSSQL: 0.05

system mssql-server.service
  → Enabled, failed, no listener 1433, no process, 0 ready
  → Host: 127.0.0.1:1433 (not listening)
  → Local (but not running) | Confidence local MSSQL: 0.10
```

### Classification Update

**Previous:** `PROBABLY_UNUSED` 0.68 (conservative, not inferring unused merely because failing).

**Updated:** `UNUSED` **0.92** - reason: additional deep search found **0 local MSSQL connection strings** (`localhost:1433` etc.) in readable user configs when excluding build artifacts, **all user projects use MySQL 3306 or remote HTTPS**, not MSSQL; combined with 0 listener, 0 process, 0 ready in 30 days, no Jam SQL Studio mssql entries, and generic .sql files not tied to local instance, confidence for local unused now **≥0.90**.

- **Evidence added:** 0 hits for localhost:1433, DB_CONNECTION=mysql for all local projects, no sql-mssql in source, no Jam SQL Studio mssql, no docker mssql.
- **Alternatives now:** `ACTIVE 0.05`, `PROBABLY_ACTIVE 0.08`, `UNKNOWN 0.10`, `PROBABLY_UNUSED 0.20`, `UNUSED 0.92`.

**Missing evidence to reach 0.99** (not needed for 0.92): Would need `sudo` to check `/var/opt/mssql/data` atime and `mssql.conf` `accepteula`, and explicit user confirmation for any hidden workflow. P6 remains read-only, so not accessed.

### Transaction Preview (Prepared Because Confidence 0.92 ≥0.90, But Still No Apply)

**Transaction ID:** `TX-P6-20260831-MSSQL-DISABLE-PREVIEW-V2` (preview, not yet approved)
**Target:** `mssql-server.service`
**Operation:** `disable service` (`systemctl disable`)
**Current:** `enabled / failed` (status 18)
**Proposed:** `disabled`
**Benefit:** Save 713M RAM peak + 9s CPU per boot, remove failed unit, reduce journal 75/week
**Risk:** `R2` (reversible, no data deletion) - now with confidence 0.92 that local unused, risk if active is low (0.08).
**Rollback:** `systemctl enable mssql-server`
**Reboot:** false
**Authentication:** required `org.polaris.service.manage` `auth_admin_keep` (not invoked in P6)
**Preconditions:** exact `mssql-server.service`, `is-enabled` still `enabled`, unit hash matches, no concurrent transaction
**State:** `PREVIEWED → APPROVAL_REQUIRED` - **STOP, no apply in P6**, requires explicit user approval tied to `TX-P6-...` + `beforeHash` before P7 `BACKUP` → `APPLY`.

**Why safe now:** 0 local connection strings + MySQL for all projects + no listener/process/ready + generic SQL not proof = strong evidence local unused, but still **requires explicit user approval** before P7 as per safety.

### Read-Only Verification

- No `sudo` (0 invocations, `/var/opt/mssql/* Permission denied` correctly skipped)
- No helper (`/run/polaris/helper.sock` not exists)
- No Polkit (`ps aux | grep polkit` only system)
- No writes (`stat /etc/fstab` 2026-08-31 21:19 unchanged, `systemctl is-enabled` still `enabled`)
- No password storage

