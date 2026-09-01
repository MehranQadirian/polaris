# P8 Report - Strictly Read-Only Optimization Discovery Pass (Post-P7)

**Phase:** P8 - READ-ONLY optimization discovery, re-run baseline, compare P3/P7, rank candidates, no apply  
**Date:** 2026-09-01 00:50 +0330  
**Mode:** READ-ONLY - no `/etc`/`/sys`/`/proc` writes, no `systemctl`, no `dnf`, no `akmods`, no `dracut`, no `sudo`, no Polkit, no helper, no reboot - only `systemd-analyze`, `systemctl is-enabled` (read), `free`, `sensors`, `journalctl`, `akonadictl status`, `cat` configs  
**Artifacts:** `p8_analysis.json` (7 candidates, ranking, highest-value), this `docs/P8_REPORT.md`, `p3_analysis.json`, `p2_scan.json`, `nvidia_preflight.json`

---

## Re-Run Baseline (Read-Only) vs P3/P7

**Systemd (P3 54.106s userspace → P8 8.515s):** `Startup finished in 3.167s (firmware) + 13.550s (loader) + 1.498s (kernel) + 3.862s (initrd) + 8.515s (userspace) = 30.594s` `graphical.target @8.514s` vs P3 `3.275/11.427/1.484/3.931/54.106` - **delta -45.591s userspace (-84%)**, **failed 1 → 0** (mssql disabled, nvidia now 470xx not failed). Blame top now `plocate-updatedb.service 21.111s` but **critical-chain** via `plasmalogin 7.301s` `plymouth 6.793s` `systemd-user-sessions 5.850s` → **plocate is background parallel, not blocker** (vs P3 `packagekit 45s` `wait-online 5.3s` `dnf-makecache 2m28s` which was also background). **Huge improvement confirmed.**

**Memory:** `free -h` `11Gi total 5.3Gi used 852Mi free 1.1Gi shared 6.5Gi buff/cache 6.1Gi avail` vs P3 `4.2GB avail 1.6GB swap used`, now `zramctl` `8G lzo-rle DATA 4K COMPR 80B` `swapon 8G 0B used` (vs 1.6GB used), `pressure` `some avg10 0.00` `full 0` - **+1.9GB avail, -1.6GB swap, pressure 0 still healthy** (P3 pressure also 0, but swap was 1.6GB under 11h uptime, now 0 after reboot).

**Journal:** `journalctl -b -p 3` `141` lines (vs P3 254-305) `grep -c NVRM` **1** (vs 490) `not supported by open` **0** (vs many) - **cleaned** due to 470xx. Remaining families: `VirtualBox VBoxCreateUSBNode 6`, `hid-generic 1`, `ACPI ETMD 2`, `kvm_amd 3`, `bluetooth Failed to set mode 1`, `anydesk coredump 1` (Process 1377) - **not bottlenecks**.

**Thermal:** `sensors` `coretemp Package 50-56C` `nvme 34.9°C` `pch 45C` vs P3 `67C` - **cooler** after reboot (load lower).

**Storage:** `lsblk` `nvme0n1 465G NVMe` `sda 931G HDD` same, `df -h / 69% 102G free` same, `findmnt --verify` `0 parse errors` (from P2 fstab fix), `fstrim.timer` `enabled active` - **healthy, no change**.

**GPU:** `nvidia 470.256.02` claimed `lspci` `driver nvidia` `lshw driver=nvidia`, `lsmod nvidia 40767488`, `modinfo 470.256.02`, `nvidia-smi` `470.256.02` `49C` `kwin_wayland 0MiB`, `glxinfo` `Mesa Intel` default, `PRIME` `__NV_PRIME...=nvidia` → `NVIDIA GeForce MX130` - **verified in P7 post-reboot, remains**.

---

## Candidates Investigated (Read-Only)

All candidates were inspected via read-only `akonadictl status`, `ps aux`, `cat /etc/fstab`, `findmnt --verify`, `systemctl status/is-enabled`, `systemd-analyze blame/critical-chain`, `journalctl`, `ls -l ~/.config/autostart`, `ls -l /etc/xdg/autostart`, `sensors`, `free`, etc. - **no writes**.

### 1. Akonadi/KDE PIM

- **Target:** `Akonadi` `akonadi_control` + 14 agents + `mysqld --defaults-file=/home/mehrangh/.local/share/akonadi/mysql.conf` `8592514912` VIRT
- **Current:** `akonadictl status` `Control running` `Server running`, `ps aux` 14 agents + mysqld `1302.61 MB` RSS sum (was 1302M), `ls -l ~/.local/share/akonadi/db_data` 126M, `Akonadi.error` 0, `socket-fedora-default` symlinked, **not in** `systemctl --failed` (user service), **not in** `systemd-analyze blame` top 20 (parallel user, not system boot).
- **Baseline:** 1302M RSS, 14 processes, not blocking boot, user login overhead only.
- **Expected benefit:** **~1.3GB RAM** reclaimed, 14 processes terminated, reduced I/O at login (30s akonadi startup), **0s boot time** (user service, parallel, not system boot).
- **Confidence:** 0.65 (needs user confirmation that KMail/Kontact/KOrganizer not used - `kdepim-runtime`, `kmail`, `kontact` installed 26.08.0 per prior `rpm -q`, but not necessarily used; `db_data 126M` suggests some use, but may be stale).
- **Risk:** `R2` moderate, reversible ( `akonadictl start` + remove `StartServer=false` ), but **breaks KMail** if actively used.
- **Reversibility:** High (user service, no reboot, no auth).
- **Affects:** `runtime` (login), not `boot` (parallel), **rebootRequired false**, **authorizationRequired false** (user-owned).
- **Why worth:** If PIM not used, it is the **single largest reclaimable user memory** after NVIDIA/mssql fixes (NVIDIA already fixed, mssql disabled). Measurable benefit high.
- **Why not worth:** If KMail actively used (126M db suggests possible use), disabling breaks workflow; confidence only 0.65 without user confirmation.
- **Rollback:** `akonadictl start` + `rm ~/.config/akonadi/akonadiserverrc` `StartServer=false` or `systemctl --user unmask akonadi`
- **Preview ID:** `TX-P8-AKONADI-DISABLE-PREVIEW` (would be `PREVIEWED → APPROVAL_REQUIRED`)

### 2. fstab / Storage Configuration

- **Target:** `/etc/fstab` + `nvme0n1`/`sda` + `fstrim.timer`
- **Current:** `/etc/fstab` 3 entries (`UUID 24bd... / ext4 defaults`, `UUID 3C27... /boot/efi vfat`, `# UUID 39b0... swap` commented from P2), `findmnt --verify` `0 parse errors`, `fstrim.timer` `enabled active` `5 days left`, `df -h /` `69%` 102G free, `lsblk` `nvme0n1 465G` `sda 931G HDD`.
- **Baseline:** 3 entries, no errors, trim enabled, free 102G, not full.
- **Benefit:** **0** - already optimal (P2 fixed stale swap, trim enabled).
- **Confidence:** 0.95
- **Risk:** `R3` high if changed (filesystem mount, could break boot).
- **Reversibility:** Medium (restore fstab backup).
- **Affects:** `boot` (mount), `rebootRequired false` (but mount change needs daemon-reload), `authorizationRequired true` (root).
- **Why not worth:** No evidence of bottleneck, fstab healthy, no benefit, high risk.
- **Preview ID:** none (no preview, no benefit)

### 3. Remaining Boot / Login Delays

- **Target:** `plocate-updatedb.service 21.111s`, `tpm 5.5s`, `sr0 5.1s`, `sda 5.1s` + `critical-chain` 8.514s via `plasmalogin`
- **Current:** `userspace 8.515s` (vs 54.106s before, -84%), `critical-chain` via `plasmalogin 7.301s` + `plymouth 6.793s`, **not** via `plocate` (plocate is background parallel, not blocker), `tpm`/`sr0`/`sda` device probes parallel 5s.
- **Baseline:** `plocate 21s` but not blocking, `tpm` 5.5s parallel.
- **Benefit:** **~0s boot** (plocate not blocking, tpm is hardware probe), only background I/O/CPU saved if disabled.
- **Confidence:** 0.85
- **Risk:** `R2` (disabling `plocate` breaks `locate` DB updates).
- **Reversibility:** High
- **Affects:** `background` (not boot)
- **RebootRequired:** false
- **AuthorizationRequired:** true
- **Why not worth:** No measurable boot benefit (critical-chain not via plocate), only background I/O, not justified.
- **Preview ID:** none (not worth preview)

### 4. dnf-makecache

- **Target:** `dnf-makecache.service` / `dnf-makecache.timer`
- **Current:** `inactive (dead)`, `timer enabled active 35min left`, `OnBootSec 10min OnUnitInactiveSec 3h`, `ConditionACPower true`, **not in** `critical-chain` (checked `systemd-analyze critical-chain | grep dnf` 0 lines), **not in** current `blame` top 30 (previously 2m28s was old boot before P7, now not top).
- **Baseline:** Not in current blame top (previously 2m28s was old boot, transient after `dnf clean`).
- **Benefit:** **0s boot** (not blocking, background `Nice 19` `IOSchedulingPriority 7`), only background makecache I/O.
- **Confidence:** 0.75
- **Risk:** `R2` (disabling disables automatic metadata refresh, `dnf` will be slower).
- **Reversibility:** High
- **Affects:** `background`
- **RebootRequired:** false
- **AuthorizationRequired:** true
- **Why not worth:** Not blocking, useful for `dnf`, would degrade `dnf` experience.
- **Preview ID:** none

### 5. Services Other Than Already-Disabled mssql-server

- **Target:** 60+ enabled services (`bluetooth` `active running`, `avahi-daemon` `active`, `cups` `disabled` but `active` via socket, `abrt`, `auditd`, `chronyd`, `firewalld`, etc.)
- **Current:** `systemctl --failed` **0** (was 1 mssql, now 0), `list-unit-files --state=enabled` 60+ (bluetooth, avahi, cups, etc.), all not in `critical-chain` (chain via `plasmalogin`, not these).
- **Baseline:** No failed services, no boot blocker among enabled services.
- **Benefit:** Small: `bluetooth` ~1 task, `avahi` 2 tasks, `cups` socket-activated - maybe 5-10M RAM, not measurable boot.
- **Confidence:** 0.40 (no evidence user doesn't use BT/printing/mDNS - cannot infer unused).
- **Risk:** `R2` (disabling `bluetooth` breaks BT, `avahi` breaks mDNS, `cups` breaks printing).
- **Reversibility:** High
- **Affects:** `runtime`
- **RebootRequired:** false
- **AuthorizationRequired:** true
- **Why not worth:** No evidence user doesn't use BT/printing, benefit tiny, risk moderate.
- **Preview ID:** none (needs user interview)

### 6. Unnecessary Autostart Entries

- **Target:** `~/.config/autostart/nvidia-settings-user.desktop` `Hidden=true` (already handled, P5 NO_OP), `/etc/xdg/autostart` 30+ entries (`anydesk_global_tray`, `baloo_file`, `at-spi-dbus-bus`, `geoclue`, `gmenudbusmenuproxy`, `gnome-keyring`, `kaccess`, `kdeconnect`, `discover notifier`, `kalendarac`, `plasmashell`, `xwaylandvideobridge`, etc.), `nvidia-settings-470xx-user.desktop` 275B enabled (for new 470xx driver, correctly enabled).
- **Current:** `~/.config/autostart` 1 file `Hidden=true`, `/etc/xdg/autostart` 30+ mostly KDE/GNOME essential, `nvidia-settings-470xx` enabled (correct for 470xx).
- **Baseline:** Autostart not in `systemd-analyze` (user session, not system boot), `cat nvidia-settings-470xx-user.desktop` not hidden (correct).
- **Benefit:** **0** - `nvidia-settings-470xx` should run for 470xx, others are essential (baloo, kdeconnect, etc.).
- **Confidence:** 0.70
- **Risk:** `R1` (user autostart, reversible) but risk breaking KDE if disabled.
- **Reversibility:** High
- **Affects:** `login` (user session)
- **RebootRequired:** false
- **AuthorizationRequired:** false
- **Why not worth:** No unnecessary autostart detected, current is minimal and correct.
- **Preview ID:** none

### 7. Journal / Error Families

- **Target:** `p3 141` (was 254-305), `NVRM 1` (was 490), `anydesk coredump` 1, `VirtualBox` 6, `hid-generic` 1, `ACPI ETMD` 2, `kvm_amd` 3, `bluetooth` 1.
- **Current:** `journalctl -b -p 3` `141` lines (vs 254), `NVRM 1` loading 470.256.02 (vs 490 `not supported`), `anydesk` coredump still present, but **not bottlenecks**.
- **Baseline:** `p3 141`, `NVRM 1` (loading, not error).
- **Benefit:** **0** - journal is result, not cause; fixing underlying (nvidia, mssql) already cleaned journal (490→1, 1 failed→0).
- **Confidence:** 0.90
- **Risk:** `R0` (no direct fix)
- **Reversibility:** N/A
- **Affects:** `logging`
- **RebootRequired:** false
- **AuthorizationRequired:** false
- **Why not worth:** Remaining families are hardware/BIOS (ACPI) or app (anydesk) not system bottlenecks.
- **Preview ID:** none

---

## Ranking (1 = Highest Value)

| Rank | Candidate | Measurable Benefit | Confidence | Risk | Reversibility | Affects | Reboot | Auth | Why Rank |
|------|-----------|-------------------|------------|------|---------------|---------|--------|------|----------|
| 1 | **Akonadi** | **~1.3GB RAM** +14 processes, **high** | 0.65 (needs user confirmation) | R2 | High | runtime (login) | false | false | **Highest benefit** (largest reclaimable RAM after NVIDIA/mssql), low auth, no reboot, but needs confirmation |
| 2 | Services (bluetooth etc.) | 5-10M RAM, **tiny** | 0.40 | R2 | High | runtime | false | true | Small benefit, low confidence, rank 2 but not worth without interview |
| 3 | Boot (plocate 21s) | **0s boot** (not blocking) | 0.85 | R2 | High | background | false | true | Not worth as boot optimization, rank 3 |
| 4 | dnf-makecache | **0s boot** (background) | 0.75 | R2 | High | background | false | true | Not worth, rank 4 |
| 5 | Journal | **0** | 0.90 | R0 | N/A | logging | false | false | Rank 5 |
| 6 | Autostart | **0** | 0.70 | R1 | High | login | false | false | Already optimal, rank 6 |
| 7 | fstab | **0** | 0.95 | R3 | Medium | boot (mount) | false | true | Already optimal, high risk if changed, rank 7 lowest |

---

## Per-Candidate Preview (Exact Transaction Preview ID if it reaches PREVIEWED)

Only **CAND-AKONADI** reaches `PREVIEWED → APPROVAL_REQUIRED` - all others are `none (no preview, no benefit)` and **do not** generate a transaction.

### CAND-AKONADI Preview

- **Preview ID:** `TX-P8-AKONADI-DISABLE-PREVIEW`
- **Exact Target:** `Akonadi/KDE PIM` (`akonadi_control` PID 2941 + 14 agents + `mysqld` `/home/mehrangh/.local/share/akonadi/mysql.conf` `8592514912` VIRT)
- **Current State:** `running, 14 agents, 1302M RSS, db_data 126M, socket-fedora-default -> /run/user/1000/akonadi, Akonadi.error 0`
- **Proposed State:** `disabled for user session` (`akonadictl stop` + config `~/.config/akonadi/akonadiserverrc` `StartServer=false` + `systemctl --user mask akonadi` or `XDG autostart Hidden`)
- **Evidence:** `akonadictl status` `Control running` `Server running`, `ps aux` 14 agents `1302.61 MB`, `ls -l ~/.local/share/akonadi/db_data` 126M, `not in` `systemctl --failed` (user service), `not in` `systemd-analyze blame` top 20 (parallel)
- **Measured Baseline:** 1302M RSS, 14 processes, not blocking boot, user service
- **Expected Benefit:** **~1.3GB RAM** reclaimed, 14 processes terminated, reduced I/O at login (30s akonadi startup), **0s boot time** (user service, parallel, not system boot)
- **Confidence:** **0.65** (needs user confirmation that KMail/Kontact/KOrganizer not used - `kdepim-runtime`, `kmail`, `kontact` installed 26.08.0 per prior `rpm -q`, but not necessarily used; `db_data 126M` suggests some use, but may be stale)
- **Risk:** `R2` moderate, **reversible** (`akonadictl start` + remove `StartServer=false` + `unmask`), but **breaks KMail/Kontact** if actively used
- **Rollback:** `akonadictl start; sed -i '/StartServer=false/d' ~/.config/akonadi/akonadiserverrc; systemctl --user unmask akonadi`
- **RebootRequired:** `false` (user session, `akonadictl stop` immediate, no reboot)
- **AuthorizationRequired:** `false` (user-owned `~/.config/akonadi`, `akonadictl` is user command, no Polkit, no `sudo`)
- **Why Worth:** If KMail/Kontact not used, this is the **single largest reclaimable user memory** after NVIDIA/mssql fixes, high benefit, low auth, no reboot
- **Why Not Worth:** If KMail actively used (126M db suggests possible use), disabling **breaks PIM workflow**; confidence only 0.65 without user confirmation
- **State:** `PREVIEWED → APPROVAL_REQUIRED` - **requires explicit user confirmation that PIM not used** - do not apply without approval

### All Other Candidates

- **No preview** - benefit 0, not worth, high risk or no evidence, marked `none (no preview, no benefit)` in `p8_analysis.json:1` - correctly **do not** reach `PREVIEWED`.

---

## Highest-Value Next Transaction (ONE)

**Candidate:** `CAND-AKONADI`  
**Preview ID:** `TX-P8-AKONADI-DISABLE-PREVIEW`  
**Target:** `Akonadi/KDE PIM`  
**Current:** `running, 14 agents, 1302M` **Proposed:** `disabled`  
**Benefit:** **~1.3GB RAM** (largest after 470xx/mssql)  
**Confidence:** 0.65 (needs user confirmation)  
**Risk:** `R2` reversible, no reboot, no auth  
**Reboot:** false **Auth:** false  
**State:** `PREVIEWED → APPROVAL_REQUIRED` - **explicitly ask for approval for this ONE transaction only**

**Why Ranked 1:** Among remaining, only Akonadi has **high measurable benefit** (1.3GB) with **high reversibility** and **no reboot/auth**, vs `fstab` 0 benefit + R3, `boot` 0s boot, `dnf` 0s, `services` tiny 5-10M + low confidence, `autostart` 0, `journal` 0. So Akonadi is objectively highest-value, but **requires user confirmation** that PIM not used - hence 0.65 confidence, not 0.90, and must stop at `APPROVAL_REQUIRED`.

---

## Read-Only Verification (P8)

- No `/etc` writes (`stat /etc/fstab` 612 21:19 unchanged)
- No `/sys` writes, no `/proc` writes
- No `systemctl` modifications (only `is-enabled` read, `status` read)
- No `dnf` `akmods` `dracut` `modprobe` `sudo` `Polkit` `helper` `reboot`
- All via read-only `systemd-analyze`, `systemctl is-enabled` (read), `free`, `sensors`, `journalctl`, `akonadictl status`, `cat` configs
- `ps aux | grep sudo` none, `ls /run/polaris/helper.sock` not exists

---

## Next Steps - Explicitly Ask for Approval for ONE Transaction Only

**STOP after this read-only discovery pass.**

**Explicitly ask for approval for this ONE transaction only:**

> **Do you approve `TX-P8-AKONADI-DISABLE-PREVIEW` to disable Akonadi/KDE PIM (1302M, 14 agents) - assuming you do NOT use KMail/Kontact/KOrganizer?**  
> This is **R2** moderate, reversible (`akonadictl start`), **no reboot**, **no auth**, benefit **~1.3GB RAM** if PIM unused, but **will break KMail** if you do use it.

**If you approve:** P9 will do `PRECONDITION CHECK` (verify `akonadictl status` still running, `StartServer` not already false, no concurrent transaction) → `BACKUP` (capture `akonadiserverrc`, `ps aux` before) → `APPLY` (`akonadictl stop` + `StartServer=false`) → `VERIFY` (`akonadictl status` stopped, `ps aux` 0 akonadi, `free` +1.3GB) → `AUDIT`.

**If you do NOT use KMail, approve. If you DO use KMail, reject - Polaris will keep Akonadi running (correct, no optimization).**

**Do not touch NVIDIA 470xx again unless verification shows regression** - verified post-reboot `nvidia-smi` 470.256.02 `claimed` `PRIME` works, `journal NVRM 1` vs 490, `systemctl --failed` 0, Intel remains default, KDE Wayland works.

**Artifacts:** This `docs/P8_REPORT.md`, `p8_analysis.json` (7 candidates, ranking, highest-value), `p3_analysis.json` `p2_scan.json` `nvidia_preflight.json` for comparison.

**Roadmap/Status Update:** P8 discovery complete, one candidate `TX-P8-AKONADI-DISABLE-PREVIEW` at `APPROVAL_REQUIRED`, awaiting explicit approval before any P9 apply.

**Do not modify host beyond read-only inspection in P8 - as instructed, host untouched beyond reads.**

---

## User Decision (2026-09-01 00:50+)

**TX-P8-AKONADI-DISABLE-PREVIEW - REJECTED by user** - Reason: User uses KMail/Kontact (or chooses to keep PIM). Polaris correctly required explicit confirmation for R2 with confidence 0.65, and user exercised control to keep the service. **No modification was made** - Akonadi remains running (14 agents, 1302M), as verified via `akonadictl status` still `running`.

**Result:** Highest-value candidate not applied, as per safety model. No other candidate meets high benefit/high confidence/low risk threshold: next candidates (services 5-10M tiny benefit 0.40, boot 0s, dnf 0s, journal 0, autostart 0, fstab 0) are correctly ranked lower and not previewed as transactions. P8 correctly **stopped at APPROVAL_REQUIRED** and respected user control.

**Roadmap Status:** P8 discovery complete, no P8 apply. Awaiting explicit approval for any alternative transaction if user later identifies a different high-value candidate (e.g., after confirming BT not used, could revisit CAND-SERVICES with higher confidence). Do not batch.

