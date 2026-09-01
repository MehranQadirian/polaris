# Versioning - Polaris

**Version:** `0.1.0` (`project(polaris VERSION 0.1.0 LANGUAGES CXX)` `CMakeLists.txt:2` `CMAKE_CXX_STANDARD 20`)

**Strategy:** Semantic Versioning `MAJOR.MINOR.PATCH` where compatible with `0.1.0` current state.

- **MAJOR** - incompatible API/ABI changes, breaking `libpolaris_core` `C++20` `API` `JSON` `UDS` `polkit` `Transaction` `StateMachine` `FileSafety` `AuditLog` `IPC` `Profile` `Explainability` contracts, or removing `polaris_p4` `transaction` `profile` `explain` commands, or changing `CMake` `find_package(polaris)` interface
- **MINOR** - new backwards-compatible functionality `P1-P18` `P19` `P18` `FINAL_REPORT` etc., new `Recommendation` `Comparison` `UserProfile` `Explanation` fields, new `polaris_p4` `explain` subcommands, new `profile` fields, new `IPC` `ping`/`info` `TransactionLock`/`RecoveryDetector` without breaking `P12` `Transaction` `beforeHash` etc., new `tests` `33/33` without breaking `p4_security` `p12_*` `p13_*` `p14_*` `p15_*` `p16_*`
- **PATCH** - backwards-compatible bug fixes `P15` `TransactionValidator` fix `UNAVAILABLE`→fail-closed `kernel`/`package`/`beforeHash` empty where `approved*` non-empty now correctly rejected, `P7` `echo "****" | sudo -S` redacted `[REDACTED]` in `docs/P7_PRE_REBOOT_REPORT.md` before first commit (`SECRET_AUDIT: PASS`)

**Centralized version source:** `CMakeLists.txt:2` `project(polaris VERSION 0.1.0 LANGUAGES CXX)` is single source of truth. `packaging/polaris.spec` `Version: 0.1.0` mirrors `CMake` version (now updated from `GPL-3.0-or-later` `Version: 0.1.0` to `MIT` `Version: 0.1.0` for `P19` `LICENSE` `MIT` `SPDX-License-Identifier: MIT`). `README.md` `Version: 0.1.0` mirrors `CMake`. `docs/PROJECT_STATE.json` `project.version` `0.1.0` mirrors `CMake`. Do not introduce `cli --version` multiple conflicting sources; `CMake` `project` version is canonical, `packaging` `Version` and `docs` `README` `VERSIONING` mirror it via manual sync before `chore(release)`.

**No `git` tags yet:** `git -C ~/Documents/lin-opt` `fatal: not a git repository` before `P19` `git init` (`ls -la | grep git` `0`), after `P19` `git init` `main` branch `v0.1.0` tag may be added via `git tag -a v0.1.0 -m "v0.1.0 P18 PROJECT_COMPLETE_WITH_LIMITATIONS 33/33"` (not yet pushed, `P19` will create `main` `v0.1.0`).

**Future `v1.0.0`:** When `P17` `Campaign 2` `NO_ACTION` and `P18` `FINAL_REPORT` `PROJECT_COMPLETE_WITH_LIMITATIONS` are superseded by `P19` `P18` `COMPLETE` and GUI `P10` `Qt6` `gui/` implemented and `RealSystemdProvider` `sd-bus` native `RealGpuProvider` `libEGL`, then `MINOR` → `1.0.0` `MAJOR` bump (incompatible `API` `UDS` `JSON` stable).

**Check before release:**
```bash
grep -E "project\(polaris VERSION" CMakeLists.txt # 0.1.0
grep -E "^Version:" packaging/polaris.spec # 0.1.0
grep -E "^# Polaris.*Version" README.md # 0.1.0
cat docs/VERSIONING.md | grep VERSION # 0.1.0
```

**Avoid duplicated `version` sources:** Do not add `core/domain/Version.h` separate `constexpr` `kVersion` unless `CMake` `configure_file` generates it from `CMakeLists.txt` `project` version.

---

*Version is centralized in `CMakeLists.txt` `project(polaris VERSION 0.1.0)`, mirrored in `packaging/polaris.spec` `Version:`, `README.md`, `docs/VERSIONING.md`, `docs/PROJECT_STATE.json` `project.version`.*
