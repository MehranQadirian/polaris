# NVIDIA Preflight Report - MX130 GM108 Maxwell 470xx Migration (READ-ONLY)

**Phase:** NVIDIA 470xx PRE-FLIGHT ANALYSIS ONLY - strictly read-only, no `dnf install/remove/swap`, no `akmods --force`, no `dracut`, no `modprobe`, no `systemctl`, no `/etc`/`/boot`/`/usr`/`/lib/modules` writes, no helper, no Polkit, no reboot  
**Date:** 2026-08-31 23:02 +0330  
**Hardware:** MX130 `10de:174d` GM108 Maxwell (rev a2) `lspci` `lshw UNCLAIMED` `pci.ids 174d GM108M [GeForce MX130]`  
**Current driver:** open `610.57.04` `Dual MIT/GPL` `gsp_tu10x` (requires GSP, Maxwell has none → `NVRM not supported` `probe error -1` 490 occurrences)  
**Candidate:** `akmod-nvidia-470xx 470.256.02-18.fc44` (legacy proprietary, no GSP, supports MX130)  
**Artifacts:** `nvidia_preflight.json` (structured), this `docs/NVIDIA_PREFLIGHT_REPORT.md`

---

## 1. Fedora Version

`NAME="Fedora Linux" VERSION="44 (KDE Plasma Desktop Edition)" RELEASE_TYPE=stable ID=fedora VERSION_ID=44` `PRETTY_NAME Fedora Linux 44` `CPE cpe:/o:fedoraproject:fedora:44` `SUPPORT_END 2027-05-19` - `cat /etc/os-release` `PRETTY_NAME` - **compatible with 470xx** (470.256.02 exists for `fc44`).

## 2. Kernel Version

`Linux fedora 7.1.10-200.fc44.x86_64 #1 SMP PREEMPT_DYNAMIC Sun Aug 23 16:15:11 UTC 2026 x86_64 GNU/Linux` `uname -a` `cat /proc/version` `gcc 16.2.1` - installed `kernel-7.1.5-201`, `7.1.8-200`, `7.1.10-200` (running `7.1.10-200`).

## 3. Architecture

`x86_64` `uname -m` `arch` - **470xx available for x86_64** (repoquery shows `x86_64`).

## 4. Secure Boot State

- `mokutil --sb-state`: `SecureBoot disabled` `Platform is in Setup Mode`
- `bootctl status`: `Secure Boot: disabled (setup)` `Firmware Arch: x64` `TPM2 yes` `Boot Loader: GRUB 2.12`
- `cat /sys/firmware/efi/efivars/SecureBoot-*` `od` shows `6 0 0 0 0` (disabled)
- `ls /boot/efi/EFI/fedora/shim*` `Permission denied` (not needed, disabled)

**Requires MOK:** **No** - disabled, Setup Mode → no MOK enrollment, no Fedora/RPM Fusion signing chain issue, no user interaction for MOK manager, no reboot for MOK. If enabled, would require `MOK enrollment` `auth_admin` `reboot to MOK manager` `exact rollback implications` (documented but not needed here).

## 5. NVIDIA PCI Identity

`lspci -nn -d 10de:` `01:00.0 3D controller [0302]: NVIDIA Corporation GM108M [GeForce MX130] [10de:174d] (rev a2)` `00:02.0 VGA [0300]: Intel CometLake-U GT2 [8086:9b41]` - `sysfs` `vendor 0x10de` `device 0x174d` `class 0x030200` `subsystem_vendor 0x17aa` `subsystem_device 0x3fba` `readlink driver` **no driver symlink (UNCLAIMED)**.

## 6. GPU Model / Architecture

`lshw -C display` `UNCLAIMED` `GM108M [GeForce MX130]` `vendor NVIDIA` `rev a2` `resources memory:b300...` - `pci.ids` `174d GM108M [GeForce MX130]` **Maxwell GM108** (first-gen Maxwell, 384 CUDA, no GSP, 28nm, 2017). `lspci -nn` confirms `10de:174d` GM108.

## 7. Current NVIDIA Packages (16)

`rpm -qa | grep nvidia` sorted + `dnf list installed *nvidia*`:
- `akmod-nvidia-610.57.04-1.fc44.x86_64`
- `kmod-nvidia-7.1.10-200.fc44.x86_64-610.57.04-1.fc44` (+ 7.1.5, 7.1.8)
- `nvidia-gpu-firmware-20260810-1.fc44.noarch`
- `nvidia-modprobe-610.57.04`, `nvidia-persistenced-610.57.04`, `nvidia-settings-610.57.04`
- `xorg-x11-drv-nvidia-610.57.04`, `xorg-x11-drv-nvidia-cuda-610.57.04` + `cuda-libs` i686/x86_64
- `xorg-x11-drv-nvidia-kmodsrc-610.57.04`, `xorg-x11-drv-nvidia-libs-610.57.04` i686/x86_64, `xorg-x11-drv-nvidia-power-610.57.04`

## 8. Installed NVIDIA Modules

`ls -l /lib/modules/7.1.10-200.fc44.x86_64/extra/nvidia/` 5 files `nvidia.ko.xz 10940292` `nvidia-drm.ko.xz 64692` `nvidia-modeset.ko.xz 682688` `nvidia-uvm.ko.xz 664252` `nvidia-peermem.ko.xz 2280` - `modinfo nvidia` `version 610.57.04` `supported external` `license Dual MIT/GPL` `firmware gsp_tu10x.bin gsp_ga10x.bin` `srcversion 8A694B1F...` `vermagic 7.1.10-200 SMP preempt` `sig fedora_1782030771` - **open, requires GSP**.

## 9. Available NVIDIA Modules

`dnf repoquery --available *nvidia*` shows `akmod-nvidia-470xx-470.256.02-18.fc44` (and 16.fc44), `kmod-nvidia-470xx-470.256.02-18.fc44`, `kmod-nvidia-580xx`, `kmod-nvidia-390xx`, `akmod-nvidia-open 610.57.04`, `xorg-x11-drv-nvidia-470xx` etc. - **470xx available**.

## 10. Current Module Ownership

`cat /proc/modules | grep nvidia` 0 lines, `cat /sys/module/nvidia/version` `No such file`, `cat /proc/driver/nvidia/version` `No such file` - **NOT LOADED** (probe failed), `lsmod | grep nvidia` 0, `lsmod | grep nouveau` `nouveau 3977216 0` + `gpu_sched` etc., `lsmod | grep i915` `i915 5611520 71` - ownership external RPM Fusion, tainted.

## 11. nouveau Status

`lsmod nouveau` present `3977216 0`, `cat /etc/modprobe.d/blacklist-nouveau.conf` `blacklist nouveau` `options nouveau modeset=0`, `cat /proc/cmdline` `rd.driver.blacklist=nouveau,nova_core modprobe.blacklist=nouveau,nova_core` - blacklisted but **currently loaded** via `nvidia-fallback.service` `ExecStart=-/sbin/modprobe nouveau` after nvidia probe fails (fallback, not primary).

## 12. i915 Status

`lsmod i915` `5611520 71`, `cat /sys/module/i915/version` `No such file` (in-kernel), provides Intel renderer `Mesa Intel UHD Graphics CML GT2`.

## 13. akmods Status

`systemctl is-active akmods.service` `active`, `ls -l /var/cache/akmods/nvidia/` 4 rpms + 4 logs `kmod-nvidia-7.1.10-200-610.57.04.rpm 10018721` `akmods log 2026-08-12 Successful for 7.1.8` - toolchain works.

## 14. kmod Packages

`rpm -qa | grep kmod-nvidia` 3 `kmod-nvidia-7.1.10/7.1.5/7.1.8-610.57.04`, `dnf list installed kmod-nvidia*` same, `dnf list available kmod-nvidia*` shows `kmod-nvidia-470xx-470.256.02-18.fc44` (would be built via akmods, not prebuilt for every kernel, but akmods will build).

## 15. initramfs NVIDIA References

`ls -lh /boot/initramfs*` `7.1.10-200 156M` `7.1.8 155M` `7.1.5 155M`, `lsinitrd /boot/initramfs-7.1.10-200.fc44.x86_64.img | grep nvidia` **0 lines** (no nvidia in initramfs, expected for open driver with fallback nouveau).

## 16. Kernel Command Line

`cat /proc/cmdline` `BOOT_IMAGE=(hd1,gpt3)/boot/vmlinuz-7.1.10-200.fc44.x86_64 root=UUID=24bd938d-b629-4c12-a681-d19cd1270645 ro rhgb quiet rd.driver.blacklist=nouveau,nova_core modprobe.blacklist=nouveau,nova_core` - also `cat /etc/default/grub` `GRUB_CMDLINE_LINUX="rhgb quiet rd.driver.blacklist=nouveau,nova_core modprobe.blacklist=nouveau,nova_core"` `GRUB_TIMEOUT=5` `BLSCFG true` - 470xx still needs nouveau blacklisted, so keep.

## 17. RPM Fusion Configuration

`cat /etc/yum.repos.d/rpmfusion*` `rpmfusion-free` enabled, `rpmfusion-nonfree` enabled, `rpmfusion-nonfree-nvidia-driver` enabled `metalink`, `dnf repolist | grep rpmfusion` 6 repos: `free`, `free-updates`, `nonfree`, `nonfree-nvidia-driver`, `nonfree-steam`, `nonfree-updates` - **configured**, keys `/etc/pki/rpm-gpg/RPM-GPG-KEY-rpmfusion-*` (2020, linked for 44).

## 18. Availability of 470xx for This Exact Fedora/Kernel

- `dnf repoquery --info akmod-nvidia-470xx` → `470.256.02-16.fc44` `rpmfusion-nonfree` + `470.256.02-18.fc44` `rpmfusion-nonfree-updates` (for Fedora 44, arch x86_64)
- `xorg-x11-drv-nvidia-470xx` → `470.256.02-5.fc44` `rpmfusion-nonfree` **supports MX130** (README lists `174d` GM108M, GeForce 900M series)
- `dnf repoquery --requires akmod-nvidia-470xx` → `/bin/sh`, `kmodtool`, `akmods`, `nvidia-470xx-kmod-common`, `xorg-x11-drv-nvidia-470xx-kmodsrc` - **all available**
- `rpm -q kernel-devel` → `7.1.10-200`, `7.1.8-200`, `7.1.5-201` + `kernel-headers 7.1.3-200` + `kernel-devel-matched 7.1.10` - **can build for current kernel** `7.1.10-200` via akmods.

## 19. Dependency Conflicts

- `dnf repoquery --conflicts akmod-nvidia-470xx` → none (but `repoquery --conflicts` for `xorg-x11-drv-nvidia-470xx` shows `xorg-x11-drv-nvidia`, `340xx`, `390xx` - so **conflicts with 610**)
- `rpm -q --conflicts akmod-nvidia` (610) → `xorg-x11-drv-nvidia-340xx`, `390xx`, `470xx`, `580xx` - **symmetric conflicts** → 610 must be removed before 470xx, `dnf swap` handles.

## 20. Package Conflicts with Current 610 Packages

- Current 610 conflicts with 470xx as above, so **cannot co-install** - must `dnf swap akmod-nvidia akmod-nvidia-470xx` (removes 610, installs 470xx, handles `xorg-x11-drv-nvidia` conflict).
- File conflicts: `dnf repoquery --conflicts` for 610 also shows none, but `xorg` libs conflict via `Conflicts:` tag, not file level.

## 21. PRIME Configuration

- `cat /etc/X11/xorg.conf` **no file**, `ls /etc/X11/xorg.conf.d/` only `00-keyboard.conf`, `cat /usr/share/X11/xorg.conf.d/*nvidia*` **no nvidia prime conf**
- `xrandr --listproviders` `Providers: number : 0` (Wayland, not Xorg, so xrandr not relevant)
- `which nvidia-prime` not found, `ls /usr/bin/*prime*` none - **no nvidia-prime** (Fedora uses DRI PRIME, not nvidia-prime)
- Wayland relevance: Session `wayland` `wayland-0` `DISPLAY :0`, GDM `WaylandEnable` default (no override), prime on Wayland uses `DRI PRIME` `__NV_PRIME_RENDER_OFFLOAD=1` + GBM/EGL, not `xorg.conf`.

## 22. Xorg/Wayland Relevance

Session `wayland` `XDG_SESSION_TYPE=wayland` `WAYLAND_DISPLAY=wayland-0` `DISPLAY=:0` `GDM` no `WaylandEnable` override - **470xx on Wayland uses GBM** (470xx supports Wayland via GBM, but Maxwell on Wayland may have GBM issues, fallback to X11 may be needed - noted as risk).

## 23. Current Renderer

`glxinfo -B` `Extended renderer: Vendor Intel 0x8086 Device Mesa Intel UHD Graphics CML GT2 0x9b41 Version 26.1.8 Accelerated yes Video memory 11687M Unified yes` `OpenGL vendor Intel` `renderer Mesa Intel(R) UHD Graphics (CML GT2)` `direct rendering Yes` - **NVIDIA renderer none (UNCLAIMED)**.

## 24. Current GPU Errors

`journalctl -p 3 -b | grep -i nvidia|nouveau|i915|drm` - **490 NVRM** occurrences `nvidia 0000:01:00.0: probe failed error -1` `NVRM not supported by open nvidia.ko` `GSP` each boot 10×, plus `i915 Atomic update failure on pipe A` `drm error` (not nvidia, but i915). `lsmod` `nvidia` not loaded, `nouveau` + `i915` loaded.

## 25. Rollback Feasibility

**Feasibility:** **High if backup captured, but not guaranteed until backup actually captured in P7.**

- Current kmods `610.57.04` for 3 kernels available in `/var/cache/akmods/nvidia/` (4 rpms `10018721` etc.) + `rpmfusion` still has `610.57.04` in repo (`dnf list available kmod-nvidia 610.57.04` `rpmfusion-nonfree-nvidia-driver` + `updates`), so **rollback packages available**.
- Backup needed: `rpm -qa | grep nvidia` list, `lsmod`/`modinfo` state, `unit hash` `965dacd7...`, `initramfs` 156M (would be rebuilt via `dracut --force`), `kernel` keep `7.1.10`, `PRIME` Intel primary, `Secure Boot` disabled so no MOK.
- Complexity: **Medium** - `dnf swap` handles conflicts, but `kmod` rebuild via `akmods --force` must succeed (requires `kernel-devel 7.1.10`, `kmodtool`, `gcc 16.2.1` - all present, previous akmods log `Successful`), `dracut --force` rebuilds initramfs, **reboot required**.
- **Not claiming guaranteed** until backup actually captured in P7 before apply (as per risk model).

---

## Package Analysis: CURRENT → REMOVE → INSTALL → KEEP

**CURRENT (16):** `akmod-nvidia-610.57.04`, `kmod-nvidia-7.1.10/7.1.5/7.1.8-610.57.04`, `nvidia-gpu-firmware-20260810`, `nvidia-modprobe-610.57.04`, `nvidia-persistenced-610.57.04`, `nvidia-settings-610.57.04`, `xorg-x11-drv-nvidia-610.57.04`, `xorg-x11-drv-nvidia-cuda-610.57.04`, `xorg-x11-drv-nvidia-cuda-libs` i686/x86_64 610, `xorg-x11-drv-nvidia-kmodsrc-610.57.04`, `xorg-x11-drv-nvidia-libs` i686/x86_64 610, `xorg-x11-drv-nvidia-power-610.57.04`.

**TO REMOVE (15):** Same as above except `nvidia-gpu-firmware` (common) and `kmods` will be replaced via `akmods` rebuild (so 15, not 16).

**TO INSTALL (10+1):** `akmod-nvidia-470xx-470.256.02-18.fc44`, `xorg-x11-drv-nvidia-470xx-470.256.02-5.fc44`, `xorg-x11-drv-nvidia-470xx-cuda-470.256.02-5.fc44`, `xorg-x11-drv-nvidia-470xx-cuda-libs` i686/x86_64, `xorg-x11-drv-nvidia-470xx-kmodsrc`, `xorg-x11-drv-nvidia-470xx-libs` i686/x86_64, `xorg-x11-drv-nvidia-470xx-power`, `nvidia-settings-470xx-470.256.02-5.fc44`, `kmod-nvidia-470xx-470.256.02-18.fc44` **built via akmods for 7.1.10-200** (not prebuilt, akmods will build).

**TO KEEP (2):** `nvidia-gpu-firmware` (common, not versioned 610/470xx), `kernel-devel` etc., `akmods` itself.

**Conflicts:** `xorg-x11-drv-nvidia` conflicts with `470xx` (and 340/390/580), so **cannot co-install** - must use `dnf swap`, not just `install`.

---

## Secure Boot

**Enabled:** **No** - `SecureBoot disabled` `Platform is in Setup Mode` `bootctl: disabled (setup)` `efivar SecureBoot 6 0 0 0 0` (disabled). **Requires MOK:** **No**. **Requires Reboot:** **Yes** for driver load, but not for MOK (since disabled). **Fedora/RPM Fusion signing chain:** `shim` `fedora_1782030771` key `2C:24:4E...` PKCS#7 `sha256` - sufficient if Secure Boot were enabled, but not needed now. If enabled, would need `MOK enrollment` `auth_admin` `reboot to MOK manager` `exact rollback implications` (would need to enroll `public_key.der` via `mokutil --import`, reboot to `MOK manager`, user interaction, `rollback` would require `mokutil --delete`).

---

## Kernel / Akmod Compatibility

**Can build for current kernel:** **Yes** - `kernel-devel-7.1.10-200` present, `kernel-headers 7.1.3-200`, `akmods` active, `kmodtool` `/usr/bin/kmodtool` present, `nvidia-470xx-kmod-common` and `kmodsrc` available via `dnf repoquery --requires` (`/bin/sh`, `kmodtool`, `akmods`, `nvidia-470xx-kmod-common`, `xorg-x11-drv-nvidia-470xx-kmodsrc`), `gcc 16.2.1`, `akmods` cache shows previous **Successful** for 610 on same kernel, so toolchain works. **Module ABI:** `vermagic 7.1.10-200 SMP preempt mod_unload` `retpoline Y` - 470xx kmod would have same vermagic after `akmods --force`.

**Do not build:** P4 preflight does **not** run `akmods --force`.

---

## Rollback Design (Not Executed, Infrastructure Only)

**Design, not execution:** Rollback must account for removed packages (15 × 610), installed packages (10 × 470xx), module state (`nvidia` 610 open → 470 proprietary, `nouveau` fallback, `i915`), `initramfs` (156M, `dracut --force` would rebuild, need backup), package versions (610.57.04 vs 470.256.02), Secure Boot (disabled, not needed), kernel selection (keep 7.1.10), PRIME state (Intel primary).

**Exact rollback (would be in P7 before apply, with backup):**
```bash
# Backup current state (P7, before apply)
rpm -qa | grep nvidia > /root/polaris/backups/nvidia-610.list
lsmod | grep nvidia > /root/polaris/backups/lsmod.before
cp /boot/initramfs-7.1.10-200.fc44.x86_64.img /root/polaris/backups/initramfs-610.img
# Apply would be
dnf swap akmod-nvidia akmod-nvidia-470xx
dnf install xorg-x11-drv-nvidia-470xx* nvidia-settings-470xx --allowerasing
akmods --force
dracut --force
reboot
# Rollback (if needed)
dnf swap akmod-nvidia-470xx akmod-nvidia
dnf swap xorg-x11-drv-nvidia-470xx xorg-x11-drv-nvidia
akmods --force
dracut --force
reboot
```
**Not claiming guaranteed** until backup actually captured (as per risk model).

---

## Risk Model

**Classify:** `R3 high` - **do not downgrade below R3 merely because reversible**.

**Explicit risks:**
- **Boot failure:** if 470xx kmod fails to build for 7.1.10, system boots without nvidia but Intel still works, but initramfs may have wrong module → `R3`.
- **Graphical-session failure:** Maxwell on Wayland with 470xx may have GBM issues (470xx Wayland support via GBM, but Maxwell may fallback to X11) → `R3`.
- **Kmod build failure:** requires `kernel-devel` 7.1.10 (present) but `akmods` could fail due to compiler mismatch (gcc 16.2.1) → `R3`.
- **Secure Boot:** disabled so low, but if enabled would be `R3` (MOK enrollment, user interaction, reboot to MOK manager).
- **Dependency conflict:** `xorg-x11-drv-nvidia` conflicts with 470xx, must use `dnf swap`, not just install → `R2`→`R3`.
- **Rollback complexity:** need to keep 610 rpms in cache (`/var/cache/akmods/nvidia/` has 4 rpms) and rebuild via `akmods`+`dracut` → `R3` medium complexity.

---

## Success Criteria (Preflight Succeeds Only If Polaris Can Answer)

1. **Is 470xx compatible with this GPU?** **YES** - GM108M 10de:174d Maxwell is listed in 470xx README (GeForce 900M series, MX130), 470.256.02 supports MX130 via proprietary (no GSP), not 610 open - `pci.ids` `174d GM108M [GeForce MX130]` + `dnf repoquery --info xorg-x11-drv-nvidia-470xx` lists 900M series.
2. **Is it available for this Fedora?** **YES** - `akmod-nvidia-470xx 470.256.02-18.fc44` in `rpmfusion-nonfree-updates` for `fc44` `x86_64`.
3. **Can it build for current kernel?** **YES** - `kernel-devel 7.1.10-200` present, `kmodtool`, `akmods` active, `nvidia-470xx-kmod-common` and `kmodsrc` available, previous `akmods Successful` for 610.
4. **Are dependencies satisfiable?** **YES** - `repoquery --requires` all available (`/bin/sh`, `kmodtool`, `akmods`, `nvidia-470xx-kmod-common`, `kmodsrc`).
5. **Is Secure Boot an obstacle?** **NO** - `SecureBoot disabled` `Setup Mode`, so no MOK, no signing chain issue.
6. **Exactly what packages/modules would change?** **15 to remove, 10 to install, 2 to keep** (see Package Analysis).
7. **Can current state be backed up sufficiently?** **YES, but requires explicit backup in P7** - package list `rpm -qa`, module `lsmod`/`modinfo`, unit hash, `initramfs` 156M, kernel selection, PRIME state - not yet captured in P4 (only test fixtures), so P7 must capture.
8. **Can rollback be performed safely?** **YES, but not guaranteed until backup captured** - `dnf swap` back + `akmods --force` + `dracut --force` + `reboot`, but complexity medium, so `R3`.
9. **What verification tests should run after installation?** 8 checks - `nvidia-smi` should succeed (not `couldn't communicate`), `lspci` claimed (driver `nvidia` not `UNCLAIMED`), `lsmod | grep nvidia` shows `nvidia`, `modinfo nvidia` version `470.256.02` license `Redistributable`, `__NV_PRIME_RENDER_OFFLOAD=1 glxinfo -B` should show `NVIDIA GM108M`, `journalctl -b -p 3 | grep -c NVRM` 0 (not 490), `akmods` `kmod-nvidia-470xx-7.1.10-200` built, `lsinitrd | grep nvidia` shows `nvidia 470xx ko`.

---

## Transaction (Candidate, Not Apply)

**DO NOT create APPLY transaction yet.** If feasibility high, create only `NVIDIA-MIGRATION-CANDIDATE`:

- **ID:** `NVIDIA-MIGRATION-CANDIDATE-470xx`
- **State:** `ANALYZED / NOT APPROVED` (NOT `APPROVED`, `AUTHORIZED`, `BACKUP_CREATED`, `APPLYING`)
- **Proposed operations:** `dnf swap akmod-nvidia akmod-nvidia-470xx` + `dnf install xorg 470xx* nvidia-settings-470xx` + `akmods --force` + `dracut --force` + `reboot`
- **Exact packages:** 15 remove, 10 install, 2 keep
- **Expected module state:** `nvidia 470.256.02` proprietary, no GSP, `lsmod nvidia` claimed true
- **Expected renderer:** `Mesa Intel` primary, `NVIDIA` via `PRIME offload` `__NV_PRIME_RENDER_OFFLOAD=1`
- **Expected PRIME:** Hybrid prime offload on Wayland via GBM, not `xorg.conf`
- **RebootRequired:** `true`
- **Secure Boot:** `requiresMOK false` (disabled), `requiresReboot true` (for driver load)
- **Risk:** `R3 high`
- **Rollback plan:** `dnf swap 470xx -> 610` + `akmods --force` + `dracut --force` (requires backup)
- **Verification plan:** 8 checks as above

---

## Final Verification (Read-Only, No Modifications)

- No `dnf install/remove/swap` executed (only `repoquery` and `rpm -q`)
- No `akmods --force` executed
- No `dracut` executed
- No `modprobe`/`rmmod` executed
- No `systemctl enable/disable` executed (except prior P6 `mssql` disable, not NVIDIA)
- No `/etc`/`/boot`/`/usr`/`/lib/modules`/`initramfs` writes
- No `grubby`/`grub2-mkconfig` executed
- No `reboot` executed
- No helper invoked, no Polkit invoked, no password collected
- All via safe fixed-path commands (`/usr/bin/rpm`, `/usr/bin/dnf repoquery`, `/usr/bin/lspci`, `cat /proc/cmdline`, etc.) no shell interpolation.

---

## Next Steps - Wait for Explicit Approval Before Any NVIDIA Mutation

**STOP after preflight.** Do **NOT** install `470xx`, `reboot`, or touch other optimization. Wait for explicit approval for `NVIDIA-MIGRATION-CANDIDATE-470xx` before any `dnf swap` in future phase. Candidate is **NOT APPROVED** - requires separate explicit approval with `beforeHash` and target.
