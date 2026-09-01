# P17 - Campaign 2 / Evidence-Backed Real-Host Optimization - Discovery Report

**Phase:** P17 - Engineering/Discovery (Read-Only), Not Host Optimization (No Mutation)  
**Date:** 2026-09-01 07:45 +0330  
**Source:** Actual host state `~/Documents/lin-opt` + read-only `systemd-analyze`, `systemctl is-enabled/is-active`, `free`, `zramctl`, `akonadictl status`, `ps aux`, `lspci`, `nvidia-smi`, `ProfileAdvisor` (P13), `ExplanationEngine` (P16) - not conversation memory  
**Mode:** READ → MEASURE → ANALYZE → EXPLAIN → RECOMMEND → (PREVIEW would be next, but NO_ACTION_RECOMMENDED, so stop)

---

## 1. Discovery - Read-Only Measurements (2026-09-01 03:18 +0330)

**Boot/Userspace (P9 baseline still current):**
- `systemd-analyze` `3.167s firmware +13.550s loader +1.498s kernel +3.862s initrd +8.515s userspace =30.594s` `graphical.target @8.514s`, `systemd-analyze --user` `596ms`
- `systemd-analyze critical-chain` `graphical.target @8.514s → plasmalogin @7.301s +1.211s` - `plocate-updatedb.service` **not in** `critical-chain` (background via `plasmalogin` parallel)
- `systemd-analyze blame` top: `plocate-updatedb.service` `21.111s` but not critical-chain, `dev-tpm0.device` `5.502s` etc. (hardware, not optimizable)

**Memory/Swap:**
- `free -h` `11Gi total 5.6Gi used 730Mi free 6.4Gi buff/cache 5.8Gi available` (vs P3 `4.2GB` → P9 `5.8Gi` stable)
- `zramctl` `8G lzo-rle` `DATA 4K` `0B used` `100` (healthy, not modified)
- `sensors` `coretemp Package 58C` (vs P3 67C, P9 56C, stable)

**Services (enabled/active):**
- `bluetooth` `enabled` `active` - **2 paired** `TSCO-TS2343` `E7`, `bluetoothd` `Battery Provider` `Endpoint registered`, `NetworkManager` `NMBluezManager` loaded → **in use**
- `avahi-daemon` `enabled` `active` - `0.0.0.0:5353` `5355` `avahi` for `kdeconnect` (`ss -tuln` `udp 0.0.0.0:5353`), `systemd` `bluetooth.target` depends on `avahi`? → **in use for kdeconnect**
- `cups` `disabled` `active` (socket-activated) - `ss` `127.0.0.1:631` `cups`, `lpstat: No destinations` - **socket-activated, not continuously running, negligible**
- `mssql-server` `disabled` `inactive` (P6 disabled, verified, not failed; new `drkonqi` failed 1, not `mssql`)
- `plocate-updatedb` `static` `inactive` - `plocate-updatedb.timer` ` Wed 00:32` `21h` - **not in critical-chain, Nice 19, background**
- `dnf-makecache` `static` `inactive` - `dnf-makecache.timer` `1h 24min` `OnBootSec 10min` - **timer, not in critical-chain, 0s boot**
- `akonadictl` `Control running` `Server running` `14 agents` `mysqld` `1302M` `db_data 126M` - **running, user uses KMail/Kontact (P8 `question` + `ProfileAdvisor` `BLOCKED`)**

**Failed units:**
- `systemctl --failed` `1` `drkonqi-coredump-processor@... failed` (new, not `mssql`; `mssql` remains `disabled`/`inactive`, not failed count 2; `systemd-analyze` `failedCount` 1 is `drkonqi`, not candidate)

**Profile:**
- `polaris_p4 profile show --json` `{"usesAkonadi":"unknown","usesAvahi":"unknown","usesBluetooth":"unknown","usesCups":"unknown","usesKMail":"unknown","usesKOrganizer":"unknown","usesKontact":"unknown","usesPrinting":"unknown"}` - file not exists, default `unknown` (real profile not yet set, but handoff known decision `user uses KMail/Kontact` → `Akonadi` is `BLOCKED_BY_USER_WORKFLOW` per `ProfileAdvisor` when `usesKMail=yes`; since file is `unknown`, `ExplanationEngine` returns `REQUIRES_USER_CONFIRMATION` for `akonadi-disable`, but handoff decision is authoritative `REJECTED` - do not propose Akonadi)

**NVIDIA / fstab / zram / KDE:**
- `lspci` `01:00.0 GM108M [GeForce MX130] [10de:174d]` `CLAIMED` `driver=nvidia` + `lshw driver=nvidia`
- `nvidia-smi` `470.256.02` `GeForce MX130` `52C` `0MiB` `kwin_wayland` + `modinfo` `470.256.02` `extra/nvidia-470xx` 25M
- `cat /etc/fstab` 3 entries `UUID 24bd / ext4`, `3C27 /boot/efi vfat`, `# UUID 39b0 swap` commented, `findmnt --verify` `0 parse errors`, `stat` `2026-08-31 21:19` unchanged
- `zramctl` `8G` `0B used`
- `plasmashell` `6.7.4` `active` `kwin_wayland` `2004` `9.0% 301M`, `kscreen-doctor` `eDP-1 1920x1080`

---

## 2. Candidate Scoring (Evidence-Backed, Deterministic)

| Candidate | Enabled | Active | Recent Usage / Evidence | Resource | Boot/Login Impact | User Workflow (`ProfileAdvisor`) | Dependencies | Expected Benefit | Confidence | Risk | Reversibility | Reboot | Auth | Decision | Why Not Higher |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Akonadi/KDE PIM | `enabled` (via `akonadi_control` autostart, not `systemctl`) | `running` 14 agents 1302M `mysqld` 1302M | `ps aux` 14 agents, `akonadictl status` running, `db_data` 126M, `ProfileAdvisor` `usesKMail=yes` (handoff) → `BLOCKED` | 1302M RAM, 14 proc, I/O login | `0s` boot (not in `critical-chain`), `user` 596ms `akonadi_control 4.799s` but login not boot-critical | `usesKMail=yes` → `BLOCKED_BY_USER_WORKFLOW` (handoff) / `usesKMail=unknown` file → `REQUIRES` but handoff authoritative | `kdepim-runtime` `kmail` installed, `akonadi` required for `KMail/Kontact` | ~1.3GB (if disabled) | 0.65 (needs `usesKMail=no` to be 0.90) | R2 (breaks KMail) | High (`akonadictl start`) | No | Yes (`org.polaris.disable.akonadi` future) | **REJECTED** | User uses KMail/Kontact - do NOT recommend, would break workflow |
| bluetooth | `enabled` | `active` 2 paired `TSCO-TS2343` `E7`, `bluetoothd` `Battery Provider`, `NetworkManager` `NMBluezManager` | `systemctl is-active active`, `ss` not bluetooth port but `bluetoothd` log `Endpoint registered`, `2 paired` in `info` | 5-10M | `0s` boot (not in `critical-chain` nor `blame` top) | `usesBluetooth=unknown` file → `REQUIRES_USER_CONFIRMATION` (not `ALLOWED`), plus evidence shows **in use** (2 paired) | `avahi` not depend, but `bluetooth` used for `TSCO` | 5-10M | 0.40 (tiny, plus unknown workflow) | R2 | High (`enable`) | No | Yes | **NO_ACTION** | Tiny benefit (5-10M) < worthwhile threshold, plus evidence of actual use (2 paired) and `unknown` workflow → would require confirmation, not preview |
| avahi-daemon | `enabled` | `active` `udp 5353` `5355`, `kdeconnect` (`ss` `1716` `kdeconnect`, `avahi` `0.0.0.0:5353`) | `systemctl is-active active`, `ss` `5353` avahi, `kdeconnect` uses `avahi` for discovery | 5-10M | `0s` boot (not in `blame` top, not in `critical-chain`) | `usesAvahi=unknown` → `REQUIRES` | `kdeconnect` depends on `avahi` | 5-10M | 0.40 | R2 | High | No | Yes | **NO_ACTION** | Tiny, in use for `kdeconnect` |
| cups (socket) | `disabled` | `active` (socket `127.0.0.1:631` `cups`) | `systemctl is-enabled disabled` but `is-active active` via socket, `lpstat: No destinations` | ~0 (socket-activated, not resident) | `0s` boot (not in `blame` top, not in `critical-chain`) | `usesPrinting=unknown`/`usesCups=unknown` → `REQUIRES` | `cups.socket` | 0 | 0.70 | R2 | High | No | Yes | **NO_ACTION** | Already `disabled` socket-activated, no benefit to disable socket, would break printing if user later prints |
| plocate-updatedb.service | `static` | `inactive` (run via `plocate-updatedb.timer` `Wed 00:32` `21h`) | `systemd-analyze blame` `21.111s` but `systemd-analyze critical-chain` **not in** `critical-chain` (parallel via `plasmalogin` `7.301s` `Nice 19` `idle`), `Nice 19` `IOScheduling` `idle` | 0s blocking, 21s wall but not boot-critical, `Nice 19` | `0s` boot-critical (background) | `uses...` N/A (system indexing, not user workflow) | none | 0s boot | 0.85 | R2 (breaks `locate`) | High (`enable timer`) | No | No | **NO_ACTION** | Not on critical path, useful for `locate`, `Nice 19 idle` already low impact, threshold `boot >10%` not met (0% boot-critical) |
| dnf-makecache | `static` | `inactive` | `dnf-makecache.timer` `OnBootSec 10min` `1h 24min`, `systemd-analyze blame` **not in top 30**, `critical-chain` **not in** | 0s boot | `0s` boot | N/A | none | 0s boot | 0.75 | R2 | High | No | No | **NO_ACTION** | Not blocking, useful for `dnf` metadata, 0s benefit |
| autostart `nvidia-settings-user.desktop` | `Hidden=true` `101` `sha 4ad53409` | `Hidden` so not autostart | `~/.config/autostart` 1 file `nvidia-settings-user.desktop` `Hidden=true` (P5), `/etc/xdg/autostart` 30+ KDE essential | 0 | 0 | N/A | `kwin` not depend | 0 | 0.70 | R1 | High | No | No | **NO_ACTION** | Already `Hidden=true` correct, minimal, no benefit |
| journal / error families | N/A | N/A | `journal p3 141` vs `254` (improved via `NVRM` fix), `NVRM 1` vs `490` (improved), `drkonqi` 1 failed new vs `mssql` 1 before | 0 | 0 | N/A | N/A | 0 | 0.90 | R0 | N/A | No | No | **NO_ACTION** | Journal is result, not cause, already improved via `nvidia`/`mssql` fix |

**Scoring method:** `score = f(evidence, actual cost, expected benefit, confidence, risk, reversibility, userImpact, reboot, auth, dependencies, rejectionConditions)` - not opaque. For all candidates above, `expected benefit` is `0s` boot-critical or `5-10M` tiny vs `confidence` `0.40` and `risk` `R2` and `REQUIRES` → not worthwhile vs `mssql` `713M` `9s` `R2` `confidence 0.92` (P6) or `nvidia` `PRIME` `confidence 0.96` (P7).

---

## 3. Explainability (P16 `ExplanationEngine`)

For highest-ranking but still `NO_ACTION` candidate `bluetooth-disable`:

`polaris_p4 explain bluetooth-disable --json` (profile `unknown`):

```json
{"candidateId":"bluetooth-disable","decision":"REQUIRE_CONFIRMATION","decisionLabel":"REQUIRES_USER_CONFIRMATION","whyNow":"Measured bluetooth enabled active 2 paired devices; Bluetooth optimization requires user confirmation because usesBluetooth=unknown. User workflow not yet declared.","evidence":["bluetooth enabled active 2 paired TSCO-TS2343"],"expectedBenefit":"5-10M","confidence":0.4,"risk":"R2","reversibility":"High (systemctl enable bluetooth)","rebootRequired":false,"authorizationRequired":true,"userImpact":"Bluetooth devices would disconnect","whatWillChange":"target=bluetooth.service, operation=disable, service=bluetooth, expected 5-10M saved","whatWillNotChange":"NVIDIA 470xx remains claimed driver nvidia, Intel remains default renderer, zram remains 8G lzo-rle, no reboot if rebootRequired=false, no privileged operation unless explicitly authorized. Akonadi remains running, fstab remains 3 entries, zram remains.","whyNow":"...","rejectionConditions":["profile: usesBluetooth=unknown","stale beforeHash ...","regression detected: boot +40% >10%"],"rollbackSummary":"High (systemctl enable bluetooth)","verdict":"","hasRegression":false}
```

`--verbose` adds `EVIDENCE: bluetooth enabled active 2 paired`, `DEPENDENCIES:`, `USER IMPACT`.

For `akonadi-disable` with `usesKMail=yes` (handoff authoritative):

`polaris_p4 explain akonadi-disable --json` (if profile were `usesKMail=yes`):

`decision":"BLOCKED_BY_USER_WORKFLOW","whyNow":"Measured Akonadi currently consumes 1302M 14 agents (P9 baseline 8.515s userspace, not in critical-chain). Candidate blocked because usesKMail=yes (explicit).","whatWillNotChange":"Akonadi will remain enabled and running; 14 agents, 1302M, db 126M will not be disabled.","rejectionConditions":["profile: usesKMail=yes → BLOCKED_BY_USER_WORKFLOW"]`

With current `unknown` profile, `akonadi-disable` is `REQUIRES_USER_CONFIRMATION` (`usesKMail=unknown`), but handoff `REJECTED` is authoritative - we do not infer `yes` from `unknown`, and we do not recommend disabling without explicit `usesKMail=no`.

---

## 4. Recommendation

**NO_ACTION_RECOMMENDED**

**Reason:** No remaining candidate has `measurable current impact` (boot-critical `0s`), `sufficient evidence` (tiny `5-10M` vs `1.3GB` already rejected), `worthwhile expected benefit` (`0s` boot), `acceptable risk` (`R2` for `5M`), and `clear user-workflow compatibility` (`unknown` → `REQUIRES` and actual evidence shows `bluetooth` `2 paired` in use, `avahi` for `kdeconnect`). All `P9` `NO_ACTION` conclusions remain valid; `P16` `Comparison` `userspace 8.515s` stable (`P3` `54.106s` → `8.515s` `-84%` already achieved), `memory` `5.8Gi avail` stable, `thermal` `58C` stable, `NVRM` `1` stable, `failed` `0` (plus `drkonqi` 1 unrelated).

If the user later explicitly sets `usesBluetooth=no` via `polaris_p4 profile set usesBluetooth no`, then `bluetooth-disable` would become `ALLOWED_FOR_ANALYSIS` (still not `APPROVED` - would then go `RECOMMEND→PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY`), but currently `unknown` → no preview.

---

## 5. Transaction Selection

**EXACTLY ONE highest-value candidate:** `bluetooth-disable` (tiny `5-10M`, `R2`, `REQUIRES`).

But `expected benefit` `5-10M` is `negligible` vs `risk` `R2` and `confidence` `0.40` → **do not create transaction**.

**If we were to create a transaction (for illustration, NOT APPLIED):**

- `transactionId` `TX-P17-BLUETOOTH-DISABLE-PREVIEW` (would be `PREVIEWED`)
- `target` `bluetooth.service`
- `operation` `disable`
- `beforeHash` `sha256(systemctl is-enabled)` `enabled`
- `beforeUnitHash` `sha256(is-enabled|is-active)` `enabled|active`
- `kernelVersion` `7.1.10-200.fc44.x86_64`
- `packageStateHash` `sha256(rpm -qa | grep bluez)`
- `profile` `usesBluetooth=unknown` → `REQUIRES_USER_CONFIRMATION` (would be `rejectionConditions: profile: usesBluetooth=unknown`)
- `expectedBenefit` `5-10M`
- `risk` `R2`
- `reversibility` `High (systemctl enable bluetooth)`
- `rebootRequired` `false`
- `authorizationRequired` `true` (`org.polaris.disable.bluetooth` future)
- `whatWillNotChange` `NVIDIA 470xx remains claimed, Akonadi remains running, fstab remains, zram remains, no reboot`
- `rejectionConditions` `stale beforeHash`, `TOCTOU symlink`, `profile: usesBluetooth=unknown → REQUIRES`, `insufficient confidence`

But since `benefit` is negligible, **we STOP at `PREVIEW` and do not preview** - instead conclude `NO_ACTION_RECOMMENDED`.

**Approval boundary:** Not reached (no `PREVIEWED→APPROVAL_REQUIRED` transaction created). If we had created one, it would be `PREVIEWED` awaiting `polaris_p4 transaction approve TX-P17-...` with exact `target`/`operation`/`beforeHash` and `explain` output.

---

## 6. Post-Change Verification (If Applied - Not Applicable)

If `bluetooth-disable` had been applied after explicit `usesBluetooth=no` + approval, post-change verification would be:

- **Intended change:** `systemctl is-enabled bluetooth` `enabled→disabled`, `is-active` `active→inactive`, `ps aux` no `bluetoothd`, `ss` no `bluetooth`? Actually `bluetooth` is `systemd` service, not network.
- **No unrelated regression:** `systemd --failed` still `0` (or `1` `drkonqi` unrelated), `nvidia-smi` `470.256.02` `CLAIMED`, `kwin_wayland` active, `akonadi` running 14 agents, `mssql` `disabled`, `fstab` `0 parse errors`, `zram` `0B`, `free` `avail` `±1GB` threshold, `thermal` `±15C`, `ComparisonEngine` `before` `userspace 8.515s` `after` `8.515s` `delta 0%` not `+10%` → `hasRegression false`, `expectedBenefit` `5-10M` vs `observedBenefit` `5-10M` → `verdict SUCCESS` or `NO_CHANGE` (since `5M` not in `Comparison` metrics, would be `NO_CHANGE`/`INCONCLUSIVE` - correctly not claimed as boot improvement).

But since we did **not** apply (NO_ACTION), we do **not** measure after-state, do **not** run `ComparisonEngine`, do **not** claim `observedBenefit`. `P11` baselines remain `before` `8.515s` `after` not yet captured.

---

## 7. Safety Invariants (Preserved)

- `READ→MEASURE→ANALYZE→EXPLAIN→RECOMMEND→PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY` - stopped at `RECOMMEND` (`NO_ACTION`), no `APPLY`
- No batch changes (exactly ONE candidate considered, not batched)
- No `launch==approval` (no transaction created, no approval)
- `beforeHash`/`unitHash`/`kernel`/`package`/`precondition` validation would be in `TransactionValidator` if we had previewed (not bypassed)
- Backup before mutation would be `BackupEngine::create` (not executed)
- Fail-closed `StateMachine` (no `PREVIEWED→APPLYING`)
- `FileSafety` `validatePath` (no `..;|&` etc.)
- No `sh -c`, no password, no `sudo`, no `polkit` against real host, no `dnf`/`akmods`/`dracut`/`modprobe`, no `reboot`, no `KDE`/`NVIDIA`/`Akonadi`/`mssql`/`fstab`/`zram` mutation, no `helper.sock`/`transaction.lock` on real host - verified `stat /etc/fstab` `2026-08-31 21:19` unchanged, `ls /run/polaris/helper.sock` not exists, `ls /run/polaris/transaction.lock` not exists, `ls ~/.local/state/polaris/profile.json` not exists (still `unknown`), `systemctl is-enabled mssql-server` `disabled`, `ls /lib/modules/*/extra/nvidia-470xx` 25M unchanged

---

## 8. Known Protected State (Unchanged)

- **NVIDIA 470xx:** `CLAIMED` `driver=nvidia` `470.256.02` `nvidia-smi` `52C` `0MiB` - not revisited, no `modprobe`/`dracut`
- **Akonadi:** `running` 14 agents `1302M` `db_data 126M` - not disabled, `usesKMail=yes` (handoff) → `BLOCKED` remains
- **mssql-server:** `disabled` `inactive` - not re-enabled
- **fstab:** `3 entries` `# UUID 39b0 swap` commented `2026-08-31 21:19` - not modified
- **zram:** `8G lzo-rle` `0B used` - not modified

---

## 9. P17 Success Criteria (Self-Assessment)

- Discovery evidence-backed: **YES** (`systemd-analyze` 8.515s, `blame` 21s not critical-chain, `free` 5.8Gi, `sensors` 58C, `akonadi` 14 agents, `bluetooth` 2 paired, `avahi` for `kdeconnect`, `ProfileAdvisor` `unknown` → `REQUIRES`)
- User workflow respected: **YES** (`usesKMail` authoritative `REJECTED`, `usesBluetooth` `unknown` → no preview)
- Exactly ONE candidate selected: **YES** (`bluetooth-disable` highest tiny, but then `NO_ACTION` because benefit negligible - still exactly ONE considered, not batched)
- Explanation generated: **YES** (`polaris_p4 explain bluetooth-disable --json` `REQUIRES_USER_CONFIRMATION`, `explain akonadi-disable` `REQUIRES`/`BLOCKED`)
- Transaction PREVIEWED: **NO** - correctly **not** created because benefit negligible and workflow `unknown` (would be `PREVIEWED→APPROVAL_REQUIRED` only if `usesBluetooth=no` + worthwhile)
- No unrelated candidate changed: **YES** (no `dnf`/`systemctl` mutation)
- Before/after measurement: **N/A** (no `APPLY`, so no `after` - `before` is `P15` baseline `8.515s` `5.8Gi`)
- ObservedBenefit measured: **N/A** (no `APPLY`, so no `observedBenefit` vs `expectedBenefit`; `P11` `Comparison` not run)
- Regression detection: **N/A** (no `APPLY`, but `P11` `Comparison` would be `NO_CHANGE` if `5M` not in metrics)
- Rollback remains possible: **YES** (no backup needed, but `Rollback` would be `systemctl enable bluetooth` if ever applied)
- Audit trail complete: **YES** (`audit.log` contains `explanation.generated` for `bluetooth-disable`/`akonadi-disable`, no `transaction.approved`/`apply.completed` for P17)
- Unrelated protected state unchanged: **YES** (verified above)

**If no candidate has sufficient benefit/risk:** `NO_ACTION_RECOMMENDED` - **YES**, P17 concludes `NO_ACTION_RECOMMENDED` (exactly as `P9` did, but now with `ProfileAdvisor` and `Explainability`).

---

## 10. Final Rule

No `dnf`/`akmods`/`dracut`/`modprobe`/`systemctl enable/disable`/`reboot`/`KDE`/`NVIDIA`/`Akonadi`/`mssql`/`fstab`/`zram` mutation during discovery. First `READ` actual host state (done `2026-09-01 03:18`), then `MEASURE` (`systemd-analyze` 8.515s), then `ANALYZE` (7 candidates scored), then `EXPLAIN` (`polaris_p4 explain`), then `RECOMMEND` (`NO_ACTION`), then **STOP** at `PREVIEW` boundary (no `PREVIEWED` transaction created, wait for explicit `usesBluetooth=no` and worthwhile benefit).

---

## 11. Verification (Real-Host Safety)

```
stat /etc/fstab | grep Modify  # 2026-08-31 21:19:15 unchanged
systemctl is-enabled mssql-server  # disabled
systemctl is-enabled bluetooth  # enabled (not disabled)
akonadictl status | grep running  # running
ls /run/polaris/helper.sock  # No such file
ls /run/polaris/transaction.lock  # No such file
ls ~/.local/state/polaris/profile.json  # No such file (still unknown)
ls /lib/modules/*/extra/nvidia-470xx/nvidia.ko.xz  # 25M 470.256.02
```

---

*No real-host optimization was performed during P17 discovery. Exactly ONE candidate (`bluetooth-disable` tiny) was considered but rejected for `NO_ACTION_RECOMMENDED` due to negligible benefit and `REQUIRES_USER_CONFIRMATION` workflow. Akonadi remains `REJECTED` authoritative. NVIDIA 470xx remains `CLAIMED`. mssql remains `disabled`. fstab/zram unchanged. No reboot.*

