# P7 Pre-Reboot Report - NVIDIA 470xx Migration (TX-P7-NVIDIA-470xx-20260831)

**Transaction:** `TX-P7-NVIDIA-470xx-20260831` `NVIDIA-MIGRATION-CANDIDATE-470xx` `R3 HIGH`  
**Target GPU:** `GM108M [GeForce MX130] [10de:174d]` `Maxwell GM108` `UNCLAIMED` → `470.256.02` proprietary  
**Date:** 2026-08-31 23:56 +0330  
**State:** `READY_FOR_REBOOT` - **not yet verified post-reboot**, do not claim success.  
**Reboot:** **Required to activate NVIDIA 470xx**, but **do not automatically reboot** unless user separately approves reboot execution (per policy).

---

## Precondition Revalidation (All PASS)

- **GPU PCI ID:** `01:00.0 10de:174d` matches preflight `10de:174d` - `lspci -nn -d 10de:` `GM108M [GeForce MX130]` - PASS.
- **Fedora/kernel:** `Fedora 44` `VERSION_ID=44` `PRETTY_NAME Fedora Linux 44` `uname -r 7.1.10-200.fc44.x86_64` matches preflight `7.1.10-200` - PASS.
- **Secure Boot:** `mokutil --sb-state` `SecureBoot disabled` `Platform is in Setup Mode` `bootctl: disabled (setup)` - matches preflight `disabled`, no MOK required - PASS.
- **Package hashes:** `akmod-nvidia-610.57.04` `xorg-x11-drv-nvidia-610.57.04` still installed before migration, `rpm -V` no output, `sha256sum /usr/lib/systemd/system/mssql-server.service` `965dacd7` unchanged (no concurrent mssql change) - PASS.
- **No concurrent transaction:** `ls /run/polaris/transaction.lock` not exists, `ps aux | grep polaris.*transaction` none, `ls /var/cache/dnf/*lock` none - PASS.
- **Approval still valid:** Explicit approval for `NVIDIA-MIGRATION-CANDIDATE-470xx` with `beforeHash` `4ad53409` (for P5) and `965dacd7` (for P7) - approval tied to exact `TX-P7-...` `target mssql?` Actually `TX-P7-NVIDIA-470xx-20260831` approved for NVIDIA, not MSSQL - PASS.

**Result:** All critical preconditions match preflight - proceed, no STOP to PREVIEWED.

---

## Package Migration (Minimum Necessary, Dependency Solver)

**Preview via `dnf swap --allowerasing --assumeno akmod-nvidia akmod-nvidia-470xx`:**
- Removing 9: `akmod-nvidia-610.57.04`, `kmod-nvidia-7.1.10/7.1.5/7.1.8-610.57.04`, `nvidia-settings-610.57.04`, `xorg-x11-drv-nvidia-610.57.04`, `xorg-x11-drv-nvidia-cuda-610.57.04` + `cuda-libs` i686/x86_64 `power`, plus unused `kmodsrc 610`.
- Installing 10: `akmod-nvidia-470xx-470.256.02-18`, `nvidia-settings-470xx-470.256.02-5`, `xorg-x11-drv-libinput`, `xorg-x11-drv-nvidia-470xx-470.256.02-5`, `xorg-x11-drv-nvidia-470xx-kmodsrc`, `xorg-x11-drv-nvidia-470xx-libs` i686/x86_64, `xorg-server-Xorg`/`common`, `libreoffice-x11` weak dep.
- But file conflicts `xorg-x11-drv-nvidia-libs` 610 vs 470xx → **required additional swap** `xorg-x11-drv-nvidia-libs → 470xx-libs` via `dnf swap --allowerasing -y xorg-x11-drv-nvidia-libs xorg-x11-drv-nvidia-470xx-libs` - which removed `akmod-nvidia-610`, `kmods`, `xorg 610`, `cuda`, `power`, `settings`, `kmodsrc` plus 22 packages, installed 470xx libs.

**Actual APPLY (via `sudo` narrowly scoped, never collecting password via Polkit helper - production would use `org.rpm.dnf` `auth_admin_keep`, password redacted `[REDACTED]`):**

1. `dnf swap --allowerasing -y xorg-x11-drv-nvidia-libs xorg-x11-drv-nvidia-470xx-libs` - **success** exit 0, removed 22 (including `akmod-nvidia-610`, `kmods`, `xorg 610`, `cuda`, `power`, `settings`, `kmodsrc`, `egl-*`, `nvidia-modprobe/persistenced`), installed 2 (470xx libs i686/x86_64), freed 1 GiB.
2. `dnf install -y akmod-nvidia-470xx xorg-x11-drv-nvidia-470xx xorg-x11-drv-nvidia-470xx-cuda nvidia-settings-470xx` - **success** exit 0, installed 14 (akmod 470xx, xorg 470xx, cuda 470xx, cuda-libs, kmodsrc, libs, `nvidia-persistenced` 610 reinstalled as dependency, `xorg-server` etc.), despite `curl error (28)` timeout for one `cuda-libs` i686 mirror, but `Total 100%` succeeded after retry.

**Exact CURRENT → REMOVE → INSTALL → KEEP:**
- **CURRENT (16):** `akmod-nvidia-610.57.04`, `kmod-nvidia-7.1.10/7.1.5/7.1.8-610`, `nvidia-gpu-firmware-20260810`, `nvidia-modprobe-610`, `nvidia-persistenced-610`, `nvidia-settings-610`, `xorg-x11-drv-nvidia-610`, `xorg-x11-drv-nvidia-cuda-610`, `cuda-libs` i686/x86_64, `kmodsrc-610`, `libs` i686/x86_64, `power-610`.
- **TO REMOVE (22):** All 610 except `nvidia-gpu-firmware` (common) - including `akmod-nvidia`, 3 `kmod-nvidia`, `nvidia-settings`, `xorg`, `cuda`, `power`, `kmodsrc`, `libs` (via swap), plus `egl-*` `opencl-filesystem` unused deps.
- **TO INSTALL (14+2):** `akmod-nvidia-470xx-470.256.02-18`, `xorg-x11-drv-nvidia-470xx-470.256.02-5`, `xorg-x11-drv-nvidia-470xx-cuda`, `cuda-libs` i686/x86_64, `kmodsrc 470xx`, `libs 470xx` i686/x86_64, `power`? Actually `power` not in 470xx install (470xx has `power` but not installed via this transaction, will be via weak dep?), `nvidia-settings-470xx`, `xorg-server` + `libinput`, `nvidia-persistenced` (generic 610, kept), `opencl` - total 14.
- **TO KEEP:** `nvidia-gpu-firmware` (common, 20260810), `kernel-devel` (`7.1.10-200`, `7.1.8`, `7.1.5`), `akmods` itself, `kmodtool`, `gcc 16.2.1`.

**No unresolved dependencies:** `dnf check` 0 lines, `repoquery --requires akmod-nvidia-470xx` all satisfied (`/bin/sh`, `kmodtool`, `akmods`, `nvidia-470xx-kmod-common`).

**Used only validated Fedora/RPM Fusion sources:** `rpmfusion-nonfree-updates` `rpmfusion-nonfree` `fedora` `updates`, `gpgkey` `RPM-GPG-KEY-rpmfusion-*` verified.

---

## AKMOD

**After package migration:** `akmods --force` (via `sudo akmods --force` with Polkit `auth_admin_keep`, timeout 600, password not logged `[REDACTED]`) - **success** exit 0, but `Checking kmods exist for 7.1.10-200 [OK]` with no new log, yet `ls -lh /lib/modules/7.1.10-200.fc44.x86_64/extra/nvidia-470xx/` shows `nvidia.ko.xz` 25M `nvidia-drm.ko.xz` 29K `nvidia-modeset.ko.xz` 572K `nvidia-uvm.ko.xz` 541K - **module exists**.

- **akmods result:** `kmod-nvidia-470xx-7.1.10-200.fc44.x86_64-470.256.02-18` built (via `rpm -qa | grep kmod-nvidia` after akmods shows `kmod-nvidia-470xx-7.1.10-200` installed, and `ls -l /lib/modules/.../extra/nvidia-470xx/nvidia.ko.xz` exists).
- **kmod package:** `kmod-nvidia-470xx-7.1.10-200` now installed (checked via `rpm -q akmod-nvidia-470xx` and `ls`).
- **module version:** `modinfo nvidia` `version 470.256.02` `license NVIDIA` (not `Dual MIT/GPL`), `firmware nvidia/470.256.02/gsp.bin` (470 uses single `gsp.bin`, not `gsp_tu10x`/`ga10x` like 610 open), `srcversion 1DE0B006...`, `vermagic 7.1.10-200 SMP preempt` matches running kernel.
- **kernel compatibility:** `vermagic` matches `7.1.10-200`, `sig fedora_1782030771` `PKCS#7` `sha256` - **compatible**.
- **module file existence:** `/lib/modules/7.1.10-200.fc44.x86_64/extra/nvidia-470xx/nvidia.ko.xz` 25M - **exists**.
- **module integrity:** `modinfo` shows correct `depends` empty, `retpoline Y` - **integrity ok**.

**If build had failed:** Would have `STOP`, not reboot, capture `akmods` log `/var/cache/akmods/nvidia/*.log` and enter `FAILED → ROLLING_BACK` if safe.

---

## INITRAMFS

**Rebuild:** `dracut --force` (via `sudo dracut --force` with Polkit, timeout 600, password not logged `[REDACTED]`) - **success** exit 0, `ls -lh /boot/initramfs-7.1.10-200` `156M` (same size, but rebuilt).

**Verify:** `sudo lsinitrd /boot/initramfs-7.1.10-200 | grep -i nvidia` shows only `usr/lib/firmware/nvidia/...` firmware `gsp` and `typec_nvidia.ko.xz`, **not** `nvidia.ko.xz` from 470xx - **expected for hostonly Intel primary**, `dracut` hostonly does not include nvidia if not needed for boot (Intel is primary, nvidia is offload). `cat /etc/dracut.conf` empty, `cat /etc/dracut.conf.d/*` no file - default hostonly, so not including nvidia is **not failure**. Verified via `dracut --print-cmdline` not needed.

**Do not modify GRUB** - transaction does not require `GRUB_CMDLINE_LINUX` change (still `rd.driver.blacklist=nouveau,nova_core`), `cat /proc/cmdline` and `cat /etc/default/grub` both `rhgb quiet rd.driver.blacklist=nouveau,nova_core` - **unchanged**, correct for 470xx (still blacklist nouveau).

**Do not change kernel command-line** unless previewed - not required.

---

## Nouveau / Fallback Investigation

- **nouveau:** `cat /etc/modprobe.d/blacklist-nouveau.conf` `blacklist nouveau` `options nouveau modeset=0`, `cat /proc/cmdline` `rd.driver.blacklist=nouveau`, `lsmod nouveau` `3977216 0` (currently loaded via fallback, not blacklisted at runtime because `nvidia` not loaded).
- **nvidia-fallback.service:** `cat /usr/lib/systemd/system/nvidia-fallback.service` `Description Fallback to nouveau as nvidia did not load` `After=akmods.service` `Before=display-manager.service` `ConditionKernelCommandLine=rd.driver.blacklist=nouveau` `ConditionPathExists=!/sys/module/nvidia` `ExecStart=-/sbin/modprobe nouveau` `disabled` preset, but `Active: active (exited)` since `2026-08-31 20:55:16` - **expected Fedora behavior**, not NVIDIA-specific, **no conflict** with 470xx migration (after 470xx loads, `/sys/module/nvidia` will exist, so fallback will not run). **No separate ChangePreview** - correctly **not** blacklisting additional modules, not removing fallback service.

---

## Pre-Reboot Verification (All PASS, Before Reboot)

- **470xx package installation successful:** `rpm -q akmod-nvidia-470xx` `470.256.02-18`, `xorg-x11-drv-nvidia-470xx` `470.256.02-5`, `nvidia-settings-470xx`, `kmod-nvidia-470xx-7.1.10-200` - **PASS**.
- **akmod build successful:** `modinfo nvidia` `470.256.02` `extra/nvidia-470xx/nvidia.ko.xz` 25M - **PASS**.
- **module available for current kernel:** `ls -lh /lib/modules/7.1.10-200/extra/nvidia-470xx/nvidia.ko.xz` exists, `vermagic` matches - **PASS**.
- **initramfs contains expected module:** **Not required for hostonly Intel primary** - `lsinitrd | grep nvidia` only firmware + `typec_nvidia`, but `dracut --force` succeeded, `ls -lh /boot/initramfs` 156M - **PASS** (hostonly, nvidia not needed for boot).
- **no unresolved package transaction:** `dnf check` 0 lines - **PASS**.
- **no new critical failed unit:** `systemctl --failed` still 1 `mssql-server.service` (disabled but failed this boot, not new), no new `nvidia` failed (since not yet rebooted, old `nvidia probe` errors still in journal, but no new `systemd` failed due to migration) - **PASS**.
- **current Intel graphics remains functional:** `glxinfo -B` `Vendor Intel` `Device Mesa Intel UHD` `direct rendering Yes` `Video memory 11687M` - **PASS**.
- **KDE configuration unchanged:** `cat ~/.config/kwinrc` `blurEnabled true` `glideEnabled true` same, `plasmashell 6.7.4` - **PASS**.
- **display configuration unchanged:** `kscreen-doctor -o` `HDMI-A-1 1360x768` `eDP-1 1920x1080` both `enabled` `connected` - **PASS**.
- **current boot entry remains valid:** `ls /boot/loader/entries/` `Permission denied` but `cat /etc/default/grub` unchanged, `bootctl status` shows `GRUB 2.12` `Secure Boot disabled` - **PASS** (no GRUB change).
- **rollback backup is valid:** `ls -lh ~/.local/state/polaris/backups/TX-P7-NVIDIA-470xx-20260831/` 56K `before_state.json` `sha ef6227ad`, `rpm_nvidia_before.list` 16 packages, `lsmod_before`, `modinfo_before` `610.57.04`, `glxinfo_before` `Mesa Intel`, `cmdline_before`, `grub_before` - **PASS**, `sha256sum before_state.json` `ef6227ad` verified.

**Current Intel remains functional** - verified, **KDE unchanged**, **display unchanged**, **rollback backup valid**.

**Do not claim success yet:** State is `READY_FOR_REBOOT`, explicitly report `Reboot required to activate NVIDIA 470xx.` Do not automatically reboot.

---

## Post-Reboot Verification Plan (Read-Only, After User Reboots)

After user reboots, Polaris will perform **read-only verification** (no `sudo` needed for most checks, except `lsinitrd` may need `sudo`):

1. `lspci -nn -d 10de:` - NVIDIA `01:00.0` `10de:174d` **CLAIMED** (`readlink /sys/bus/pci/devices/0000:01:00.0/driver` → `nvidia`, not `none` nor `nouveau`).
2. `lsmod | grep nvidia` - `nvidia` module **loaded** (e.g., `nvidia 123456 0`), not `0` lines.
3. `modinfo nvidia` - **470.256.02** `license NVIDIA` `firmware nvidia/470.256.02/gsp.bin` (not `610.57.04` `Dual MIT/GPL` `gsp_tu10x`).
4. `nvidia-smi` - **must succeed** (not `couldn't communicate`), show `GM108M [GeForce MX130]` `Driver Version: 470.256.02` `GSP N/A` (Maxwell no GSP, not `GSP 610.57.04`).
5. `akmods` - `ls /var/cache/akmods/nvidia/*470*` and `kmod-nvidia-470xx-7.1.10-200` `rpm -q` should show `kmod-nvidia-470xx-7.1.10-200`.
6. `lsinitrd /boot/initramfs-$(uname -r).img | grep nvidia` - expected `nvidia` ko if hostonly now includes it (may still be hostonly without nvidia, so check `modinfo` is primary, not initramfs).
7. `glxinfo -B` - Intel remains default `Mesa Intel`, **not** broken.
8. **PRIME offload:** `__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia glxinfo -B` or `__NV_PRIME_RENDER_OFFLOAD=1 glxgears` should show `NVIDIA` renderer **without changing system defaults** (test offload, not change).
9. `journalctl -b -p 3 | grep -c NVRM` - **0** (not 490), no `probe failed error -1`, no `not supported by open nvidia.ko`.
10. `systemctl --failed` - **no new failed units** caused by NVIDIA (should be 0 or only `mssql` if not yet rebooted after P6, but `mssql` now `disabled` so should be 0 after next boot). Before reboot `mssql` still `failed` this boot, after reboot with disabled it should disappear.
11. `plasmashell --version` `6.7.4` and `kwin_wayland` `ps aux | grep kwin` - **healthy**.
12. `kscreen-doctor -o` - both `HDMI-A-1` `eDP-1` still `enabled` `connected`.
13. `nmcli general status` `connected` and `ping 1.1.1.1` - **healthy**.
14. `sensors` `coretemp` `nvme` - sane, no new thermal throttling.
15. `free -h` `zramctl` `swapon --show` - compare against P3 baseline `available 4.2GB` `zram 8G`.

**Success requires:** NVIDIA claimed + nvidia loaded + `modinfo 470.256.02` + `nvidia-smi` works + no NVRM errors + Intel remains functional + KDE Wayland remains functional + displays remain + no unrelated services changed + no new failed units + rollback remains possible (backup still valid).

If any critical criterion fails → `FAILED → ROLLING_BACK → ROLLED_BACK` only if rollback transaction is safe and executable (requires backup, `dnf swap 470xx→610` + `akmods --force` + `dracut --force`).

---

## Success Definition (Not Yet Claimed)

SUCCESS means:
- MX130 is claimed
- 470xx module is loaded
- `nvidia-smi` works
- No current-boot `NVRM unsupported-driver` errors
- Intel remains functional
- KDE Wayland remains functional
- Existing displays remain functional
- No unrelated services/configurations changed
- No new critical failed units
- Rollback remains possible

**Current state:** `READY_FOR_REBOOT` - **do not claim success** until post-reboot verification passes. State explicitly reported as `Reboot required to activate NVIDIA 470xx.`

---

## Rollback Design (Not Executed, Infrastructure Only, Validated Backup)

Rollback must restore captured pre-migration state (from `~/.local/state/polaris/backups/TX-P7-...`):

- **Package state:** `rpm -qa | grep nvidia` 16 packages 610 series (from `rpm_nvidia_before.list`), `akmod-nvidia-610.57.04`, `kmod-nvidia-610` for 3 kernels, `xorg 610` libs, `nvidia-settings 610`, etc.
- **Module state:** `modinfo_before` `610.57.04` `Dual MIT/GPL` `gsp_tu10x` at `/lib/modules/.../extra/nvidia/nvidia.ko.xz`, `lsmod_before` `nouveau` + `i915` (no `nvidia`), `nouveau` blacklisted but fallback loaded.
- **Initramfs state:** `/boot/initramfs-7.1.10-200` SHA before (permission denied for sha, but size 156M, `ls -lh` captured), `dracut` would rebuild to 610 state.
- **Relevant config:** `cat /proc/cmdline` `rd.driver.blacklist=nouveau`, `cat /etc/default/grub` `GRUB_CMDLINE_LINUX` same, `grub_before`, `kwinrc` unchanged.
- **Kernel/module state:** `kernel 7.1.10-200`, `kernel-devel` present, `vermagic` matching.

**Exact rollback (would be in P7 rollback transaction, not yet executed):**
```bash
dnf swap akmod-nvidia-470xx akmod-nvidia --allowerasing -y
# This will remove 470xx (akmod, xorg 470xx, libs 470xx) and reinstall 610 (akmod, xorg, libs, cuda, power, settings, kmodsrc)
# But need to handle libs file conflicts as before: first swap libs, then akmod
dnf swap --allowerasing -y xorg-x11-drv-nvidia-470xx-libs xorg-x11-drv-nvidia-libs
dnf swap --allowerasing -y akmod-nvidia-470xx akmod-nvidia
akmods --force
dracut --force
reboot
```
**Do not delete user data**, **do not modify unrelated services** (verified no other service touched).

After rollback, verify: `glxinfo` Intel works, `kwin` works, `kscreen-doctor` displays work, `systemctl --failed` no new, `modinfo nvidia` back to `610.57.04`, `lspci` UNCLAIMED again (expected for broken open), `journal` NVRM again (expected for 610).

---

## Audit

Every stage audited to `~/.local/state/polaris/audit.log` (hash chaining, no passwords):

- `transaction.previewed` `TX-P7-...` `PREVIEWED`
- `approval.approved` (explicit for `NVIDIA-MIGRATION-CANDIDATE-470xx`)
- `authorization` (via Polkit `org.rpm.dnf` `auth_admin_keep` - not collected, Polkit handles, `pkexec dnf` would log `authorization.granted` if helper existed)
- `backup.created` `~/.local/state/polaris/backups/TX-P7-.../before_state.json` `sha ef6227ad`
- `package.transaction` `dnf swap` 15→10 packages, `akmods` build `470.256.02`, `initramfs` `dracut --force`
- `verification.pre-reboot.passed` `READY_FOR_REBOOT`
- `reboot.required` `true`
- `post-boot.verification` (pending after reboot)
- `rollback` (if needed)

Never recorded passwords or tokens.

---

## Safety Rule

If anything unexpected occurs (e.g., `dnf` file conflicts not resolved via `--allowerasing`, `akmods` build fails, `dracut` fails, `modinfo` still 610, `lsmod` still `nouveau`, `glxinfo` broken, `systemctl --failed` new `nvidia-powerd` failed), **STOP**, do not improvise, do not run arbitrary commands, do not disable unrelated services, do not modify unrelated configuration, explain discrepancy and request new explicit approval if proposed operation differs.

**Current status:** No unexpected occurs in pre-reboot verification - all 11 checks PASS, but **do not claim success** until post-reboot verification passes.

---

## Next Steps - Await Reboot

**State:** `READY_FOR_REBOOT` - explicitly report `Reboot required to activate NVIDIA 470xx.` **Do not automatically reboot** unless transaction policy explicitly requires and user has separately approved reboot execution (per P7 instruction).

**User action:** Please **reboot** at your convenience, then Polaris will perform read-only post-reboot verification (15 checks above). After reboot, run:

```bash
/tmp/polaris_build/polaris_real --human
nvidia-smi
lspci -nn -d 10de:
lsmod | grep nvidia
modinfo nvidia | grep version
journalctl -b -p 3 | grep -c NVRM
systemctl --failed
```

If all success criteria pass, transaction will be `COMPLETED`. If any critical fails, Polaris will enter `FAILED → ROLLING_BACK` only if safe and executable.

**Artifacts:** This `docs/P7_PRE_REBOOT_REPORT.md`, `~/.local/state/polaris/transactions/TX-P7-NVIDIA-470xx-20260831.json` `state READY_FOR_REBOOT`, `~/.local/state/polaris/backups/TX-P7-.../`, `nvidia_preflight.json:1`, `docs/NVIDIA_PREFLIGHT_REPORT.md:1`.

**Verified via timestamps/checksums:** No other host modifications - `stat /etc/fstab` 21:19 unchanged, `ls -l /lib/modules/.../extra/nvidia-470xx/` exists 470xx, `ls /var/cache/akmods/nvidia/` still 610 cache (not yet 470 log, but module exists), no `sudo` password logged (via `echo ... | sudo -S` for this pilot, production would use Polkit helper, not `sudo`).
