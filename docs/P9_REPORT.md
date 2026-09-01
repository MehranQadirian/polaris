# P9 Report - Fine-Grained Read-Only Optimization Discovery (Post-P7, Post-P8 Rejection)

**Phase:** P9 - READ-ONLY optimization discovery, fresh baseline, compare P3/P7/P8, rank candidates, no apply  
**Date:** 2026-09-01 00:55 +0330  
**Mode:** READ-ONLY - no `/etc`/`/sys`/`/proc` writes, no `systemctl`, no `dnf`, no `akmods`, no `dracut`, no `sudo`, no Polkit, no helper, no reboot - only `systemd-analyze`, `systemctl is-enabled` (read), `free`, `sensors`, `journalctl`, `akonadictl status`, `cat` configs  
**Artifacts:** `p9_analysis.json` (6 candidates, ranking, NO_ACTION_RECOMMENDED), this `docs/P9_REPORT.md`

---

## Fresh Baseline vs P3/P7/P8

**Systemd (P3 54.106s → P8 8.515s → P9 8.515s):** `Startup finished in 3.167s (firmware) + 13.550s (loader) + 1.498s (kernel) + 3.862s (initrd) + 8.515s (userspace) = 30.594s` `graphical.target @8.514s` vs P3 `3.275/11.427/1.484/3.931/54.106 = 74.223s` - **delta -45.591s userspace -84%**, **failed 1 → 0** (mssql disabled, nvidia 470xx now, P8 still 1 mssql before disable, now 0), `blame` top `plocate-updatedb.service 21.111s` but **critical-chain** via `plasmalogin 7.301s` `plymouth 6.793s` **not via plocate** (so plocate is background parallel, not blocker). **Huge improvement confirmed and stable.**

**User systemd (P9 fresh):** `Startup finished in 596ms (userspace)` `default.target @596ms` top `akonadi_control 4.799s` (rejected, still running), `plasma-startupsound 2.475s`, `plasma-kcminit 1.873s` - **user login 596ms total**, akonadi dominates but rejected.

**Memory:** `free -h` `11Gi 5.3Gi used 852Mi free 1.1Gi shared 6.5Gi buff 6.1Gi avail` (vs P3 4.2GB avail, now 6.1Gi **+1.9GB**), `zramctl` `8G lzo-rle DATA 4K COMPR 80B` `Swap 8.0Gi 0B used` (vs 1.6GB used, **-1.6GB**), `pressure cpu some avg10 1.00` (vs 0, transient due to updatedb), `memory pressure` `some 0.00` **healthy**, no swap pressure.

**Journal:** `journalctl -b -p 3` `141` lines (vs P3 254, P8 141), `grep -c NVRM` **1** (loading 470.256.02, vs 490 `not supported`), `anydesk coredump` still present (user `anydesk` dumped core, not system bottleneck), `VirtualBox` 6, `hid-generic` 1, `ACPI` 2, `kvm_amd` 3 - **cleaned** as side effect of nvidia/mssql fixes.

**Thermal:** `sensors` `coretemp 56C` (vs P3 67C, **-11C**), `nvme 35.9C` (vs 38C), `pch 45C` (vs 53C) - **cooler** after reboot, no throttling.

**Storage:** `lsblk` `nvme0n1 465G` `sda 931G` same, `df -h /` `69%` 102G free same, `findmnt --verify` `0 parse errors` (P2 fstab fix), `fstrim.timer` `enabled active` - **healthy, no change**.

**GPU:** `nvidia 470.256.02` claimed `lspci` `driver nvidia`, `lsmod nvidia 40767488`, `nvidia-smi` `470.256.02` `49C`, `glxinfo` `Mesa Intel` default, `PRIME` `__NV_PRIME...=nvidia` → `NVIDIA GeForce MX130` - **verified in P7 post-reboot, remains**.

---

## Candidates Investigated (Read-Only)

All via read-only `akonadictl status`, `ps aux`, `cat /etc/fstab`, `findmnt --verify`, `systemctl is-enabled` (read), `systemd-analyze blame/critical-chain`, `journalctl`, `ls -l ~/.config/autostart`, `sensors`, `free`, etc. - **no writes**.

### 1. Login / Autostart Overhead

- **Target:** `plasma-startupsound 2.475s`, `plasma-kcminit 1.873s`, plus `akonadi 4.799s` (already rejected) - **autostart files:** `ls -l /etc/xdg/autostart` 38 entries, `ls -l ~/.config/autostart` 2 files (`nvidia-settings-user.desktop` `Hidden=true` from P5 + `nvidia-settings-470xx-user.desktop` 275B `Hidden=false` correctly enabled for 470xx).
- **Measured:** `systemd-analyze --user` 596ms total, top `akonadi 4.799s` (80% of user boot but rejected), next non-akonadi is `startupsound 2.475s` (sound, not bottleneck). `cat /etc/xdg/autostart/nvidia-settings-470xx-user.desktop` `Hidden=false` correct (470xx should run).
- **Benefit:** If akonadi not disabled, next is `startupsound 2.475s` → disabling would save 2.475s login, but breaks startup sound (cosmetic) and is KDE essential.
- **Confidence:** 0.50, **Risk R1**, reversible, login not system boot, no reboot, no auth.
- **Why not worth:** Akonadi rejected, remaining autostarts are essential (`baloo`, `kdeconnect`, `plasmashell`, `xwaylandvideobridge`, etc.) or tiny (startupsound is sound, not slow). **No measurable benefit without disabling akonadi or essential KDE.**

### 2. Background Services with Actual Usage Evidence

- **bluetooth.service:** `enabled` `active running` `bluetoothd 885` 1 task, **2 devices paired** `TSCO-TS2343 69:CA:3C:DA:FA:1E` and `E7 41:42:35:2C:FC:92` via `bluetoothctl devices` - **evidence shows active use**, not unused. Benefit `~1 task` `5M` **tiny**, 0s boot (not in critical-chain), risk `R2` breaks paired BT. **Not worth - do not infer unused without evidence, here evidence shows used.**
- **avahi-daemon:** `enabled` `active` 2 tasks, `avahi-browse -at` shows `_kdeconnect._udp` on `wlp0s20f3` and `lo` - **used for kdeconnect** (which is active `kdeconnectd` 3332). Benefit tiny, risk breaks mDNS/kdeconnect.
- **cups.service:** `disabled` preset `disabled` but `active running` via `cups.path`/`cups.socket` (socket-activated), `lpstat: No destinations added` (no printers configured) - **already socket-activated (optimal)**, not persistent overhead, `blame` not in top 30. Benefit 0 (already optimal), risk breaks printing if later added.
- **Other enabled 60+** (`abrt`, `auditd`, `chronyd`, `firewalld`, etc.) - all not in `critical-chain` (chain via `plasmalogin`), no failed, no evidence of not using.
- **Overall:** No candidate with `0 failed`, no boot blocker, benefit **5-10M tiny**, confidence 0.40 (no evidence user doesn't use BT/printing), **not worth without interview**.

### 3. plocate / Indexing Runtime Impact

- **Target:** `plocate-updatedb.service` and `timer`
- **Current:** `inactive dead` since `00:45:45` (11min ago), `TriggeredBy plocate-updatedb.timer`, last run `21.111s wall 12.107s CPU 1.4G peak`, `db 86M` at `/var/lib/plocate/plocate.db`, `cat service` `IOSchedulingClass=idle` `Nice=19` `PrivateTmp` etc., `ls -l` 86M.
- **Measured:** `systemd-analyze blame` `21.111s` but **not in** `critical-chain` (chain via `plasmalogin 7.301s`, not `plocate`), `critical-chain grep plocate` 0 lines.
- **Benefit:** **0s boot** (not blocking), only 12s CPU and 1.4G peak saved if disabled, but breaks `locate` DB updates.
- **Confidence:** 0.80, **Risk R2**, reversible, background (not boot), no reboot, auth true.
- **Why not worth:** Not on critical path, useful for `locate`, already `Nice 19 idle`, not a bottleneck.

### 4. dnf-makecache Runtime / Resource Impact

- **Target:** `dnf-makecache.service` / `dnf-makecache.timer`
- **Current:** `inactive dead`, `timer enabled active 27min left`, `OnBootSec 10min OnUnitInactiveSec 3h`, `ConditionACPower true`, **not in** `critical-chain` (`grep dnf` 0 lines), **not in** current `blame` top 30 (previously 2m28s was old boot before P7, now not top).
- **Measured:** Not in current `blame` top 30, not blocking, `Nice 19` `IOSchedulingPriority 7`.
- **Benefit:** **0s boot** (not blocking), only background metadata refresh.
- **Confidence:** 0.75, **Risk R2** (disabling makes `dnf` slower).
- **Why not worth:** Not blocking, useful, 0s boot benefit.

### 5. Other Evidence-Backed Low-Risk Bottlenecks

- **Journal:** `p3 141` (vs 254), `NVRM 1` (vs 490), `anydesk coredump` 1, `VirtualBox` 6, `hid-generic` 1, `ACPI` 2, `kvm_amd` 3 - remaining families are **hardware/BIOS** (`ACPI ETMD` bug) or **app** (`anydesk` coredump, not system), not system bottlenecks. No direct optimization (journal is log).
- **Memory:** `pressure some avg10 0.00`, `available 6.1GB`, `Swap 0B used`, `zram 8G 0B` - healthy, **do not modify zram** (explicitly forbidden).
- **Storage:** `fstab` 3 entries, `findmnt --verify` 0 errors, `fstrim.timer` enabled, `df` 69% - **healthy, no change**.

---

## Ranking (1 = Highest Value, Evidence-Driven)

| Rank | Candidate | Measurable Benefit | Confidence | Risk | Reversibility | Affects | Reboot | Auth | Why Rank |
|------|-----------|-------------------|------------|------|---------------|---------|--------|------|----------|
| 1 | Akonadi (rejected) | **~1.3GB RAM** +14 processes, **high** but rejected | 0.65 (needs confirmation) | R2 | High | runtime login | false | false | **Highest benefit** but explicitly rejected by user (uses KMail) - not applicable |
| 2 | Services (bluetooth etc.) | **5-10M RAM**, **tiny** | 0.40 (no evidence unused) | R2 | High | runtime | false | true | Small benefit, low confidence, rank 2 but **not worth** without interview |
| 3 | Boot plocate 21s | **0s boot** (not blocking) | 0.85 | R2 | High | background | false | true | **Not worth** as boot optimization, rank 3 |
| 4 | dnf-makecache | **0s boot** | 0.75 | R2 | High | background | false | true | Not worth, rank 4 |
| 5 | Journal | **0** | 0.90 | R0 | N/A | logging | false | false | Already improved via nvidia/mssql, rank 5 |
| 6 | Autostart | **0** (already optimal, nvidia fix done) | 0.70 | R1 | High | login | false | false | Rank 6 |
| 7 | fstab | **0** (already optimal) | 0.95 | R3 | Medium | boot mount | false | true | Rank 7 lowest, high risk |

**Key principle:** Do not optimize merely to reduce `blame` numbers, do not disable service merely because enabled, do not infer unused without evidence - all candidates correctly evaluated as **background, not boot-critical**, and **benefit is theoretical maximum, not realistic** (e.g., plocate 21s wall but 0s boot).

---

## Per-Candidate Preview (Exact Transaction Preview ID if PREVIEWED)

Only **Akonadi** would have reached `PREVIEWED → APPROVAL_REQUIRED` (`TX-P8-AKONADI-DISABLE-PREVIEW`), but it was **explicitly rejected** in P8 (user uses KMail) and is **explicitly prohibited from revisiting** per P9 rules (`Do NOT modify Akonadi/KDE PIM`). All other candidates **do not** reach `PREVIEWED` because **expected benefit is negligible** (0s boot or tiny 5-10M) and **do not create a transaction when expected benefit is negligible** - correctly **no preview**.

- **CAND-AKONADI:** Would be `TX-P8-AKONADI-DISABLE-PREVIEW` `target Akonadi` `current running 14 agents 1302M` `proposed disabled` `evidence akonadictl status running 1302M` `benefit ~1.3GB` `confidence 0.65` `risk R2` `rollback akonadictl start` `rebootRequired false` `authorizationRequired false` `why worth` **largest reclaimable after NVIDIA/mssql** `why not worth` **breaks KMail** - but **rejected, do not re-preview** per P9 `Do NOT modify Akonadi`.

- **All others:** `previewId: none (no preview, no benefit)` - correctly **do not** create transaction, per `do not create a transaction when expected benefit is negligible`, `do not optimize merely to reduce blame`, `do not disable merely because enabled`, `do not infer unused without evidence`.

---

## Highest-Value Next Transaction (ONE, At Most)

**Candidate:** **none** - **NO_ACTION_RECOMMENDED**

**Reason:** After `PREVIEWED → APPROVAL_REQUIRED` for Akonadi was **rejected** (user uses KMail), no remaining candidate has a **worthwhile benefit/risk ratio**:
- Next best (`bluetooth` 5M, `plocate` 0s boot, `dnf` 0s, `autostart` 0, `fstab` 0) are **all 0s boot or tiny 5-10M**, with **low confidence 0.30-0.40** (no evidence user doesn't use BT/printing) or **high risk R2/R3** for 0 benefit.
- Per P9 rule: `do not create a transaction when expected benefit is negligible` - all remaining are negligible (0s boot or tiny RAM) vs **1.3GB Akonadi which was rejected**, so **no transaction** reaches `PREVIEWED`.

**State:** `NO_ACTION_RECOMMENDED` - **explicitly documented, no unapproved host modification**.

**Why not worth doing any of the remaining:** All remaining bottlenecks are **background, not boot-critical** (`critical-chain` via `plasmalogin`, not `plocate`/`dnf`), **boot vs login distinguished** (user `596ms` vs system `8.515s`), **realistic benefit 0s boot** (theoretical maximum 21s plocate is not realized because parallel), **confidence low** (no evidence BT not used, but `bluetoothctl devices` shows 2 paired, so actually used), **risk R2** would break workflows for 0 benefit.

---

## Fresh Baseline Comparison (P3 vs P7 vs P8 vs P9)

| Metric | P3 Baseline (21:09, before P7) | P7 Post-NVIDIA (00:40) | P8 (00:50, post-P7) | P9 Fresh (00:55, current) | Delta P3→P9 | Verification |
|--------|-------------------------------|------------------------|---------------------|---------------------------|-------------|--------------|
| `userspace` | 54.106s | 54.106s (pre-reboot) → 8.515s (post-reboot) | 8.515s | **8.515s** | **-45.591s -84%** | `systemd-analyze` |
| `failed` | 1 `mssql` (2 before P2) | 0 (after P6 disable + P7) | 0 | **0** | **-1** | `systemctl --failed` |
| `NVRM` | 490 | 1 (loading 470.256.02) | 1 | **1** (loading, not error) | **-489** | `journalctl -b -p 3` |
| `avail` | 4.2GB | 6.5Gi (P7) | 6.1Gi | **6.1Gi** | **+1.9GB** | `free -h` |
| `swap used` | 1.6GB | 0B (P7) | 0B | **0B** | **-1.6GB** | `swapon --show` |
| `thermal` | 67C | 50C | 50C | **56C** (vs 67C, still -11C) | **-11C** | `sensors` |
| `p3` | 254 | 141 | 141 | **141** | **-113** | `journalctl -p 3` |

**No regression** - all metrics stable or improved after P7/P8, no new bottlenecks introduced.

---

## Read-Only Verification (P9)

- No `/etc` writes (`stat /etc/fstab` 612 21:19 unchanged since P2)
- No `/sys` writes, no `/proc` writes
- No `systemctl` modifications (only `is-enabled` read, `status` read, `analyze` read)
- No `dnf` `akmods` `dracut` `modprobe` `sudo` `Polkit` `helper` `reboot`
- All via read-only `systemd-analyze`, `systemctl is-enabled` (read), `free`, `sensors`, `journalctl`, `akonadictl status`, `cat` configs
- `ps aux | grep sudo` none, `ls /run/polaris/helper.sock` not exists

---

## Next Steps - Explicitly Identified ONE Highest-Value Next Transaction (But Not Created Due to NO_ACTION)

**Highest-value candidate was Akonadi (1.3GB) but explicitly rejected** - per P9 `Do NOT modify Akonadi/KDE PIM` (user uses KMail) and P8 rejection, so **no transaction** is created.

**All remaining candidates are NO_ACTION_RECOMMENDED** - as per `If no remaining candidate has a worthwhile benefit/risk ratio: NO_ACTION_RECOMMENDED`.

**State:** `NO_ACTION_RECOMMENDED` - **explicitly documented, no unapproved host modification, no PREVIEWED transaction created** (would be `TX-P8-AKONADI-DISABLE-PREVIEW` but rejected, so not recreated).

**Why not worth doing any remaining:** All remaining have **negligible expected benefit** (0s boot or 5-10M) **not theoretical maximum** (21s plocate is wall time, not boot time, realistic benefit 0s), **low confidence** without evidence, **R2 risk** for no benefit, **reversibility high but not needed**.

**Artifacts:** This `docs/P9_REPORT.md`, `p9_analysis.json` (6 candidates, ranking, NO_ACTION_RECOMMENDED), `p3_analysis.json` `nvidia_preflight.json` for comparison.

**Roadmap/Status Update:** P9 discovery complete, no P9 preview created due to `NO_ACTION_RECOMMENDED`, awaiting explicit user direction if they later identify a different workflow (e.g., confirm BT not used, then CAND-SERVICES could be revisited with higher confidence).

**Do not modify host beyond read-only inspection in P9 - as instructed, host untouched beyond reads.**

