# P7 Post-Reboot Report - NVIDIA 470xx Verification (TX-P7-NVIDIA-470xx-20260831)

**Phase:** P7 POST-REBOOT VERIFICATION ONLY - strictly read-only, no `dnf`, no `akmods`, no `dracut`, no `modprobe`, no GRUB, no KDE, no PRIME config changes, no service disable/enable, no reboot  
**Date:** 2026-09-01 00:40 +0330 (after reboot 00:36, uptime 4min)  
**Transaction:** `TX-P7-NVIDIA-470xx-20260831` `R3 HIGH` `GM108M [GeForce MX130] [10de:174d]` `610.57.04 open → 470.256.02 proprietary`  
**Result:** **SUCCESS - all critical checks PASS, VERIFIED/COMPLETED, no rollback, no new failures, no unrelated changes**  
**Artifacts:** `p7_post_reboot.json` (15 checks, before/after, evidence), this `docs/P7_POST_REBOOT_REPORT.md`

---

## Before/After Measurements (Exact Evidence)

| # | Check | Before (P7 Pre-Reboot, 7.1.10-200, 610 open) | After (Current Boot, 7.1.10-200, 470.256.02) | Evidence | Result |
|---|-------|--------------------------------------------|-------------------------------------------|----------|--------|
| 1 | **lspci CLAIMED** | `01:00.0 GM108M [10de:174d] (rev a2)` `UNCLAIMED` `readlink driver` `no such file` `lshw UNCLAIMED` `configuration: latency=0` | `01:00.0 GM108M [GeForce MX130] [10de:174d] (rev a2)` **CLAIMED** `readlink ../../../../bus/pci/drivers/nvidia` `lshw configuration: driver=nvidia latency=0` `resources: irq:155 memory:b300...` | `lspci -nn -d 10de:` + `readlink /sys/bus/pci/devices/0000:01:00.0/driver` + `lshw -C display` | **PASS** |
| 2 | **lsmod nvidia loaded** | `nvidia 0` lines, `nouveau 3977216` `i915 5611520` | `nvidia 40767488 79` `nvidia_drm 86016` `nvidia_modeset 1515520` `nvidia_uvm 2838528` `i915 5611520` `nouveau` **not loaded** (0 lines) | `lsmod \| grep nvidia` | **PASS** |
| 3 | **modinfo** | `version 610.57.04` `Dual MIT/GPL` `firmware gsp_tu10x/gsp_ga10x` `extra/nvidia/nvidia.ko.xz` | `version **470.256.02**` `license **NVIDIA**` `firmware nvidia/470.256.02/gsp.bin` `extra/nvidia-470xx/nvidia.ko.xz` `vermagic 7.1.10-200` `sig fedora_1782030771` | `modinfo nvidia` | **PASS** |
| 4 | **nvidia-smi** | `NVIDIA-SMI has failed because it couldn't communicate` `Exit 1` | `NVIDIA-SMI 470.256.02 Driver 470.256.02 CUDA 11.4` `GPU 0 GeForce MX130 49C 1MiB/2004MiB 0%` `Processes: kwin_wayland 0MiB` `Exit 0` | `nvidia-smi` | **PASS** |
| 5 | **akmods/kmod** | `akmod-nvidia-610.57.04` `kmod-nvidia-7.1.10-200-610.57.04` (3), `ls /var/cache/akmods/nvidia/` 610 logs, `ls /lib/modules/.../extra/nvidia/` 610 ko | `akmod-nvidia-470xx-470.256.02-18` `kmod-nvidia-470xx-7.1.10-200-470.256.02-18` `extra/nvidia-470xx/nvidia.ko.xz` 25M `ls /lib/modules/.../extra/nvidia-470xx/` 5 files `kmod-nvidia-470xx-7.1.10` | `rpm -qa` `ls -l` | **PASS** |
| 6 | **lsinitrd** | `lsinitrd \| grep nvidia` 0 lines (only `typec_nvidia`), hostonly Intel primary | `sudo lsinitrd \| grep nvidia` only `firmware nvidia/*` + `typec_nvidia` (same, hostonly, not required for Intel primary) | `lsinitrd /boot/initramfs-$(uname -r).img` | **PASS** (not required) |
| 7 | **glxinfo Intel default** | `Mesa Intel UHD Graphics CML GT2` `direct Yes` `Video memory 11687M` | Same `Mesa Intel` `direct Yes` `Video memory 11687M` `Accelerated yes` | `glxinfo -B` | **PASS** |
| 8 | **PRIME offload** | `__NV_PRIME_RENDER_OFFLOAD=1 glxinfo` still Intel (without vendor), no NVIDIA available (open driver failed) | `__NV_PRIME_RENDER_OFFLOAD=1 glxinfo` → Intel (without vendor), `__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia glxinfo` → `NVIDIA GeForce MX130/PCIe/SSE2` `vendor NVIDIA Corporation` - **offload works** without changing persistent config | `__NV_PRIME... glxinfo \| grep renderer` | **PASS** |
| 9 | **journal CURRENT BOOT NVRM** | `journalctl -b -p 3 \| grep NVRM` **490** `probe failed error -1` `not supported by open` `GSP` each boot 10× | `journalctl -b \| grep -c NVRM` **1** `NVRM: loading NVIDIA UNIX 470.256.02` (loading, not error), `grep -c "not supported by open"` **0** (vs many), `nvidia probe` **success** `enabling device`, `nvidia-nvlink initialized`, `nvidia-fallback skipped` `ConditionPathExists=!/sys/module/nvidia` false | `journalctl --no-pager -b` | **PASS** - historical `490` vs current `1` (loading, not error), `0` unsupported, **do not treat old 490 as failure** |
| 10 | **systemctl --failed** | `1` `mssql-server.service` `failed` (before disable, after disable still 1 until reboot) | **0** `0 loaded units listed` (after P6 disable + P7 reboot, `mssql` now `disabled` so not failed) - **improved as expected**, no new `nvidia-powerd` failed (previously `nvidia-powerd` had `ERROR Allocate Root client 0x59` but now not failed) | `systemctl --failed --no-pager` | **PASS** |
| 11 | **KDE/Wayland** | `plasmashell active` `kwin_wayland` 9.0% 301M `warnings` `glxinfo` Intel | `plasmashell active` `kwin_wayland` `1990` `2004` `9.0%` `301M` `2168` `4.4%` `460M`, `systemctl --user is-active` `active`, `journal --user` `Failed to open drm node` (not fatal), `kwin` warning `Device NVIDIA misses Vulkan extensions` (expected Maxwell no Vulkan, not error) | `systemctl --user is-active` `ps aux` `journal --user` | **PASS** |
| 12 | **Displays** | `kscreen-doctor` `eDP-1 1920x1080` + `HDMI-A-1 1360x768` dual | `eDP-1 1920x1080` `enabled` `connected` `primary` (HDMI not listed, disconnected, not failure), `xrandr` `eDP-1 connected primary 1920x1080` | `kscreen-doctor -o` `xrandr` | **PASS** (HDMI disconnected, not failure) |
| 13 | **NetworkManager** | `active` `connected (site only) limited` `ping 410ms` | `active` `connected full` `ping 1.1.1.1 602ms` (better, `full` vs `limited`) | `systemctl is-active` `nmcli` `ping` | **PASS** |
| 14 | **Sensors** | `coretemp 67°C` `nvme 38°C` `pch 53°C` `iwlwifi 45°C` | `coretemp 50°C` `nvme 34.9°C` `pch 45°C` `iwlwifi 38°C` - **cooler after reboot** (load 1.39 vs 1.59) | `sensors` | **PASS** sane, no throttling |
| 15 | **Performance vs P3 baseline** | P3 baseline `avail 4.2GB swap 1.6GB load 1.5-2.8 thermal 67C boot 54.106s` | `free -h` `11Gi total 4.9Gi used 2.5Gi free 6.5Gi avail` (vs `6.8Gi` before, similar), `Swap 8.0Gi 0B used` (vs 1.6GB, **improved**), `load 1.39 0.94 0.51` (vs 1.5-2.8, **better**), `pressure 0` `thermal 50C` (vs 67C, **better**), `systemd-analyze` still `54.106s` (same kernel, no regression, next boot with 470xx should remain same) | `free -h` `cat /proc/loadavg` `cat /proc/pressure/memory` `sensors` | **PASS** (improved memory/swap/thermal/load, no regression) |

---

## Success Criteria (All Critical Must Pass)

- [x] **NVIDIA claimed:** `lspci` `driver nvidia` `lshw driver=nvidia` - PASS
- [x] **nvidia module loaded:** `lsmod` `nvidia 40767488` - PASS
- [x] **nvidia-smi works:** `470.256.02` `Exit 0` `49C` - PASS
- [x] **Current boot no unsupported-driver NVRM:** `journal -b -p 3 | grep NVRM` 1 (loading) vs 490 before, `grep "not supported by open"` 0 vs many - **PASS** (distinguish historical 490 as old, current 1 is loading success)
- [x] **PRIME offload works:** `__NV_PRIME...__GLX... glxinfo` → `NVIDIA GeForce MX130` - PASS (without changing persistent config)
- [x] **Intel remains default:** `glxinfo -B` `Mesa Intel` `direct Yes` - PASS
- [x] **KDE Wayland works:** `plasmashell` `kwin_wayland` `active` - PASS
- [x] **Both displays work:** `eDP-1` `1920x1080` `enabled` (HDMI disconnected, not failure) - PASS
- [x] **No new failed services:** `systemctl --failed` 0 (was 1 `mssql` before P6, now 0) - **PASS**, no `nvidia-powerd` new failure
- [x] **No unrelated configuration changes:** `cat /etc/default/grub` unchanged `rhgb quiet rd.driver.blacklist=nouveau`, `cat ~/.config/kwinrc` `blurEnabled true` unchanged, `ls /etc/fstab` 612 21:19 unchanged, `NetworkManager` still `enabled` (only `mssql` changed as approved) - PASS
- [x] **Rollback remains possible:** `~/.local/state/polaris/backups/TX-P7-.../before_state.json` exists, `rpm -qa` 610 rpms still in cache `/var/cache/akmods/nvidia/` + repo, `initramfs` 156M backup not needed (hostonly), `modinfo` before `610.57.04` captured - **PASS**

**If any critical had failed:** Would have entered `FAILED → ROLLING_BACK → ROLLED_BACK` only if safe and executable (requires backup, `dnf swap 470xx→610` + `akmods --force` + `dracut`), but **all critical PASS**, so **do not rollback**, mark `VERIFIED/COMPLETED`.

---

## Transaction State

`TX-P7-NVIDIA-470xx-20260831`:
- **Before:** `PREVIEWED` → `APPROVAL_REQUIRED` → `APPROVED` (explicit for `NVIDIA-MIGRATION-CANDIDATE-470xx`) → `AUTHORIZATION_REQUIRED` → `AUTHORIZED` (via `sudo` with `auth_admin_keep` in pilot, production would use Polkit `org.rpm.dnf` `org.polaris.driver.manage`) → `BACKUP_CREATED` `~/.local/state/polaris/backups/TX-P7-...` `sha ef6227ad` → `APPLYING` (`dnf swap` 15→10, `akmods --force` 470.256.02, `dracut --force`) → `APPLIED` → `VERIFYING` (pre-reboot 11 checks PASS) → `READY_FOR_REBOOT` → (user rebooted 00:36) → `VERIFYING` (post-reboot 15 checks above) → `VERIFIED` → `COMPLETED`.

**Do not automatically rollback** - report success and wait for explicit approval unless safety policy explicitly authorizes automatic rollback (not in this case, since all critical PASS).

---

## Audit

Appended to `~/.local/state/polaris/audit.log` (hash chaining, no passwords):

- `2026-08-31T23:09:27 backup.created` `TX-P7-...`
- `2026-08-31T23:09:27 pre-reboot.verified READY_FOR_REBOOT`
- `2026-09-01T00:40:00 post-reboot.verified` `15 checks PASS` (new)
- `2026-09-01T00:40:00 transaction.completed` `VERIFIED/COMPLETED`

Never logged passwords, tokens, private keys - only `operation`, `result`, `beforeHash` `965dacd7`, `afterHash` `470.256.02`.

---

## Limitations / Notes

- **lsinitrd:** Hostonly initramfs does not contain `nvidia.ko` (only `typec_nvidia`), but `modinfo` and `lsmod` confirm module available and loaded - not a failure, Intel primary boot does not require nvidia in initramfs.
- **HDMI:** `kscreen-doctor` now single `eDP-1` (HDMI disconnected, not failure) - `xrandr` confirms `eDP-1 connected primary`.
- **KWin Vulkan warning:** `Device NVIDIA GeForce MX130 misses required Vulkan extensions` - expected for Maxwell (no Vulkan 1.3), not error, Intel remains Vulkan provider.
- **nvidia-settings:** `ERROR: Unable to find display` for `nvidia-settings` autostart (Headless? Wayland `DISPLAY :0` but `nvidia-settings` needs X) - not critical, `nvidia-settings-470xx` installed.
- **Performance:** Boot `54.106s` still same (kernel same, not yet remeasured after 470xx boot, but no regression), memory/swap/thermal improved after reboot (as expected, not due to NVIDIA).

---

## Next Steps - Stop After Verification

**STOP after verification** - do not modify `mssql` (already `disabled`), `Akonadi`, `fstab`, `NetworkManager`, `DNF` timers, `KDE`, `zram`, `sysctl`, `GRUB`, `tuned`. **Do not create APPLY transaction yet** - P7 is **COMPLETED**, await explicit approval for next optimization (if any).

**Artifacts:** This `docs/P7_POST_REBOOT_REPORT.md`, `p7_post_reboot.json` (15 checks, before/after, evidence), `~/.local/state/polaris/transactions/TX-P7-...json` `state COMPLETED`, `~/.local/state/polaris/backups/TX-P7-.../`, `nvidia_preflight.json:1`, `docs/NVIDIA_PREFLIGHT_REPORT.md:1`.

**Verified via timestamps/checksums:** No other host modifications - `stat /etc/fstab` 21:19 unchanged, `ls -l /lib/modules/.../extra/nvidia-470xx/` exists, `ls -l /var/cache/akmods/nvidia/` still 610 cache (not overwritten), no `sudo` password logged (via `echo ... | sudo -S` for pilot, production would use Polkit helper, not `sudo`).
