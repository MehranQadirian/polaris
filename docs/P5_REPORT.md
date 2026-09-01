# P5 Report - Controlled Real-Host Pilot (Single R1 User Autostart)

**Phase:** P5 - ONE real, low-risk, evidence-backed, reversible optimization via `PREVIEW → APPROVAL → BACKUP → APPLY → VERIFY → BEFORE/AFTER → AUDIT`  
**Mode:** REAL-HOST PILOT, but **NO_OP** - target already in desired state, no host modification per explicit user rejection  
**Date:** 2026-08-31 22:31 +0330  
**Host:** fedora 7.1.10-200.fc44.x86_64 Fedora 44 KDE Plasma 6.7.4 Wayland  
**Transaction:** `TX-P5-20260831-001` `nvidia-autostart-hidden-true` `R1` `~/.config/autostart/nvidia-settings-user.desktop`  
**Result:** `ALREADY_APPLIED / NO_OP` - file already `Hidden=true` (101 bytes, sha `4ad53409`), no backup/apply per user rejection, verified via read-only health + test-copy rollback, audit complete, no other host modifications.

---

## 1. Transaction ID

`TX-P5-20260831-001` - real transaction, persisted `~/.local/state/polaris/transactions/TX-P5-20260831-001.json` with `beforeHash 4ad53409...`, `state PREVIEWED` initially, now `ALREADY_APPLIED`.

## 2. Exact Operation

Disable user-level autostart entry `~/.config/autostart/nvidia-settings-user.desktop` by ensuring `Hidden=true` (XDG autostart `Hidden=true` hides system entry `/etc/xdg/autostart/nvidia-settings-user.desktop` `Exec=nvidia-settings -l`). Method `FileSafety::atomicWrite` (temp+fsync+rename, symlink protected) - **not executed on real file** per NO_OP, but tested on `/tmp/polaris-test-p5/test.desktop`.

## 3. Before State

- **Path:** `/home/mehrangh/.config/autostart/nvidia-settings-user.desktop`
- **Exists:** true, regular file, not symlink, canonical matches exactly `realpath` equals target
- **Owner:** `mehrangh:mehrangh` `0644` `Uid 1000`
- **Size:** 101 bytes, within safe limits <4096
- **Content:**
  ```
  [Desktop Entry]
  Type=Application
  Exec=nvidia-settings -l
  Hidden=true
  X-GNOME-Autostart-enabled=false
  ```
- **SHA-256:** `4ad5340929f7d89be19a513f34aae694f0d5f2590622e51868e93bdb04b25991`
- **Contains:** `nvidia-settings` yes, `Hidden=true` already
- **Timestamp:** `Modify 2026-08-31 21:13:08` (from P2 Level1)

## 4. After State

- **Content:** Same as before (idempotent, no diff)
- **SHA-256:** Same `4ad53409...`
- **Diff:** `(no diff - already Hidden=true, idempotent)` - preview showed no diff, user correctly rejected modification.
- **Status:** `ALREADY_APPLIED` - file already in desired state, no host modification.

## 5. Evidence

- **Current verification:** `RealGpuProvider` `sysfs driver` missing `claimed false`, `moduleLoaded false`, `journal NVRM not supported by open nvidia.ko` 26-99/boot, `glRenderer Mesa Intel only` - NVIDIA driver not functional (GM108 Maxwell, 610 open requires GSP, probe `error -1`).
- **Prior manual:** `systemd --user app-nvidia-settings` `2.562s CPU` `journal 2026-08-31 21:04:08 ERROR driver not loaded` - measured 2.56s login overhead.
- **Current file:** `nvidia-settings` autostart would launch while driver unavailable → error.
- **Confidence:** High for relevance, but **no diff** → transaction is NO_OP.

## 6. Risk

`R1` low and reversible (user file, no system files, no kernel/systemd/dnf). Verified via `FileSafety` allowlist includes `~/.config/autostart/nvidia-settings-user.desktop`, `validatePath` rejects `..`, `;|&`, `NUL`, `>4096`, symlink, non-allowlist.

## 7. Approval

**Explicit approval tied to exact transaction:** User was shown preview with `TARGET`, `CURRENT STATE` (101 bytes, sha `4ad53409`), `PROPOSED STATE` (`Hidden=true`, no diff), `WHY`, `EVIDENCE`, `BENEFIT`, `RISK R1`, `ROLLBACK`, `REBOOT false`, `AUTH not required`. User **rejected** with reason: “target is already in desired state (Hidden=true) and proposed change has no diff. Do not perform backup, atomic write, or any host modification. Instead, mark as ALREADY_APPLIED / NO_OP.” Approval state recorded as `REJECTED_NO_OP` with `beforeHash` check - if file had changed after preview, approval would be invalidated (hash mismatch check in `polaris_p5 approve`).

## 8. Backup

**Not created for real host** per user rejection (no diff, no modification). Planned backup would have been `~/.local/state/polaris/backups/TX-P5-20260831-001/nvidia-settings-user.desktop.bak` with original 101 bytes, sha `4ad53409`, perms `0644`, owner `mehrangh`. Verified **test copy** backup: `/tmp/polaris-test-p5/test.desktop` `Hidden=false` → `test.desktop.bak` sha `5649d618...` size 101, `isRegularFile` true, no overwrite, `sha256` verified - proves backup integrity mechanism works without touching real file.

## 9. Verification

**Immediate verification (read-only, no modification):**
- Reread file → `Hidden=true` present true
- `is_regular_file` true, not symlink true, canonical matches true
- `stat` owner `mehrangh` true, perms `0644` true
- `sha256` `4ad53409` matches beforeHash, no unrelated content change (same 101 bytes)
- Result: **PASSED** - file valid, already in desired state, no modification needed.

**Post-change health check (read-only diagnostics, no reboot):**
- Polaris healthy: `polaris_real --human` OK
- KDE/Plasma healthy: `systemctl --user is-active plasma-plasmashell` active, `kwin_wayland` 302M, no new crash
- `systemctl --failed` still 1 `mssql-server.service` (not changed in P5)
- `findmnt --verify` success (fstab unchanged in P5, still `612` with commented swap from P2)
- `systemd-analyze` 3.275/11.427/1.484/3.931/54.106 unchanged (user autostart not boot)
- Network `nmcli` active, ping 1.1.1.1 ok, thermal 67°C no throttling, filesystem 62% used

## 10. Performance Measurement

**Distinguish login vs boot:**
- **System boot time:** `systemd-analyze` `userspace 54.106s` - **unchanged by this user autostart** (user session, not systemd boot). Do not claim reduced boot time.
- **Login/session overhead:**
  - **Before:** `nvidia-settings -l` 2.56s CPU per login (prior manual, `systemd --user` 2.562s, `journal ERROR driver not loaded`).
  - **After (projected next login):** `0s` (Hidden=true prevents launch, no `nvidia-smi` probe via autostart).
  - **Measured now:** Cannot measure login overhead without new login session - current session still from before P2 (uptime 11h). P5 correctly states: **will be verified after next login** via `journalctl --user -b -n 50 | grep nvidia-settings` should show 0 ERROR, and `systemd --user` should not show `app-nvidia-settings` 2.56s.
  - **Current measurement:** `ps aux | grep nvidia-settings` none (already hidden), so overhead already 0 for this session.

## 11. Journal Comparison

- **Before (previous session, 2026-08-31 21:04:08):** `journalctl --user` `nvidia-settings ERROR driver not loaded` + `systemd app-nvidia-settings` 2.562s.
- **After (current boot, before P5):** `journalctl -b -p 3` still has `nvidia probe 26-99` (driver still broken, not related to autostart) but `journalctl --user -b` should have **no new** `nvidia-settings ERROR` after P2's `Hidden=true` (since autostart hidden). P5 verifies current `journalctl --user -b -n 50 | grep nvidia-settings` 0 lines (checked 22:31, 0). **Do not compare unrelated historical journal** (e.g., 109 historical vs 26 current boot) - scope distinguished: `current boot (-b)` vs `next session`.
- **Success criteria (next session):** After next login, `journalctl --user --since today | grep "nvidia-settings.*ERROR"` should be 0, and `systemd --user status app-nvidia*` should show `Hidden=true` prevents activation. Not yet observed until next login - P5 reports projected, not claimed success.

## 12. Rollback Readiness

**Real file:** No rollback needed (no change), but plan valid: `BackupEngine::restore` would restore original 101 bytes from `~/.local/state/polaris/backups/...` via `atomicWrite` with `canonical` check. **Test copy verification:** Created `/tmp/polaris-test-p5/test.desktop` `Hidden=false` → backup `test.desktop.bak` sha `5649d618` → apply `Hidden=true` → verify `Hidden=true` → rollback `cp test.desktop.bak test.desktop` → verify `Hidden=false` restores - **PASS**. Proves rollback logic works without touching real file, as instructed.

## 13. Security Verification

- **FileSafety:** `validatePath` allowlist passed, `isRegularFile` true, `isSymlink` false, `canonical` matches, `owned by current user` true, size <4096 true, contains expected entry true, `NUL` `;|&` `..` `>4096` rejected - all checks PASS.
- **No sudo:** `ps aux | grep sudo` none during P5, `polaris_p5` uses `FileSafety::atomicWrite` on user-owned file, no `sudo`, no `execv` with sudo.
- **No Polkit:** `polkit` not invoked (user file, `auth not required`), `ps aux | grep polkit` only system `polkitd` and KDE agent, no Polaris auth request.
- **No helper:** Not installed, not used.
- **No password:** `grep -r password` 0 hits except docs “never”, no storage.
- **Atomic write:** Not executed on real file per NO_OP, but test copy used `FileSafety::atomicWrite` temp+fsync+rename, symlink protected - tested.
- **No shell:** No `sh -c`, no `sed -i` blindly, no redirection, fixed paths, structured args, bounded time.

## 14. Test Results

| Test | Result |
|------|--------|
| `unit` | Pass 0.00s |
| `real_providers` | Pass 0.03s |
| `parsers` | Pass 0.00s |
| `readonly` | Pass 0.00s |
| `p4_security` 9 checks | Pass 0.01s - traversal, symlink, metachars, invalid transition, replay, backup no overwrite, oversized, fake op, audit chain |
| `test_p5` precondition | Pass - 9 checks all OK (Fedora, exists, regular, not symlink, canonical, owned, size 101, contains nvidia-settings, Hidden) |
| `test_p5` preview | Pass - transaction `TX-P5-20260831-001` PREVIEWED, beforeHash `4ad53409`, diff no diff idempotent |
| `test_p5` approval | REJECTED_NO_OP - correct per user, hash still matches, no concurrent modification |
| `test_p5` backup | NOT_CREATED per NO_OP (real), but test copy backup verified |
| `test_p5` apply | NOT executed per NO_OP, test copy atomicWrite verified |
| `test_p5` verification | Pass - Hidden=true present, regular, owned, perms 0644, sha matches |
| `test_p5` rollback test copy | Pass - Hidden=false → Hidden=true → Hidden=false restores |
| `ctest` all | 100% 5/5 0.05s |
| No other host modifications | Verified `stat /etc/fstab` mtime 2026-08-31 21:19 unchanged, `systemctl --failed` still 1, `NetworkManager-wait-online` still disabled, `ps aux | grep -E "dnf|fstab|mssql|akonadi"` no new modifies |

## 15. Any Limitations

- **Idempotent NO_OP:** Since file already `Hidden=true` from P2 Level1, P5 did not demonstrate a real before→after diff on host. Rollback readiness proven only via test copy, not real file. Next real optimization (e.g., fstab, mssql) will demonstrate true before/after.
- **Login measurement:** Cannot measure login overhead until next login session; projected 0s, not yet observed.
- **Journal next session:** Requires next login to verify `nvidia-settings ERROR` disappears; current boot still has driver probe errors (unrelated to autostart, driver still broken).
- **Single-operation pilot:** No batch optimization, no other files touched - as required.

---

## Audit

Recorded `~/.local/state/polaris/transactions/TX-P5-20260831-001.json` and `audit.log`:

- `transaction.previewed` 22:31 `mehrangh` PENDING
- `approval.rejected` REJECTED_NO_OP (already Hidden=true, no diff) - **not** `transaction.approved`
- `verification.passed` ALREADY_APPLIED

No `backup.created`, `transaction.applied` for real host per NO_OP - correct per user instruction. Test copy audit shows `backup.created` and `verification.passed` for fixture.

Never logged passwords/secrets.

---

## CLI

```
polaris_p5 preview          # precondition + preview, no writes
polaris_p5 approve TX-...   # explicit approval tied to id/target/hash
polaris_p5 apply            # would backup+apply if not NO_OP (blocked in this case)
```

All via `polaris_p5` (P5 pilot), not `sudo`, not Polkit.

---

## Next Steps - Wait for Explicit Approval

P5 success criterion **met**: *Polaris safely executed (or correctly handled as NO_OP) one real, evidence-backed, reversible, audited, verified transaction on the host without unnecessary modification.*

**Do not** automatically continue to:
- fstab optimization
- MSSQL disable
- Akonadi disable
- NVIDIA 470xx driver
- DNF/compositor/wait-online/swap

Those require separate transactions and separate approvals.

**Awaiting explicit approval for next transaction (P6).**
