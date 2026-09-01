# P13 - User Workflow / Profile Engine - Implementation Report

**Phase:** P13 - Engineering, Not Host Optimization  
**Date:** 2026-09-01 04:30 +0330  
**Source:** Repository `~/Documents/lin-opt` verified via `cmake`, `ctest`, `ls`, `cat`, `stat`, `systemctl`, `audit.log` - not conversation memory  
**Status:** **COMPLETE** - explicit, auditable, deterministic profile engine with recommendation constraints, no inference, no host mutation, no real profile created by tests

---

## 1. Implementation Summary

Implemented a small, explicit, offline-first profile engine that lets Polaris remember user-declared workflow constraints so future recommendations avoid breaking workflows the user depends on. Design preserves `P12` hardened lifecycle: profile is **input to future recommendations**, never a trigger for host mutation and never bypasses `RECOMMEND→PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY`.

Lifecycle:

```
User declares → ProfileService::updateField(field, yes/no/unknown) explicit, no inference
                → ProfileStore::save atomic (tmp+fsync+chmod 0600+rename) at ~/.local/state/polaris/profile.json
                → AuditLog fsync "profile.updated" field/previous/new
                → Future Recommendation: ProfileAdvisor::canConsiderAkonadi/Bluetooth/... returns
                   BLOCKED_BY_USER_WORKFLOW (yes) / REQUIRES_USER_CONFIRMATION (unknown) / ALLOWED_FOR_ANALYSIS (no, still needs approval)
                → Even ALLOWED still requires full transaction approval per P12 (StateMachine, beforeHash, backup)
```

All operations deterministic, side-effect-free except atomic profile write, no shell, no password, no sudo, no reboot, no service mutation.

---

## 2. Files Changed / Added

**Modified:**
- `core/safety/FileSafety.h:18` - extend `isAllowedPath` and `validatePath` to allow `~/.local/state/polaris/profile.json` and `~/.local/state/polaris/` (P13 safe user file), keep `/tmp/polaris-test-root` allowlist; preserves existing P4/P5 checks
- `cli/p4_cli.cpp:1` - add `core/profile` includes, `cmd_profile_show` / `cmd_profile_set`, dispatch `profile show|set`, help updated, `--json` support, auditable, not authorization
- `docs/ARCHITECTURE.md:1` - add P13 layer description
- `docs/ROADMAP.md:25` - P13 COMPLETE line, ranked list updated
- `CMakeLists.txt:8` - add `core/profile/UserProfile.cpp`, `ProfileStore.cpp`, `ProfileService.cpp`, `ProfileAdvisor.cpp` to `polaris_core`; add 4 P13 test executables

**Added:**
- `core/profile/UserProfile.h:1` (210L) - `TriState` enum `UNKNOWN/YES/NO`, `toString`/`fromString`, `UserProfile` struct 8 fields (`usesKMail`, `usesKontact`, `usesKOrganizer`, `usesBluetooth`, `usesPrinting`, `usesAvahi`, `usesCups`, `usesAkonadi`) + `extra` map, `isDefaultUnknown`, `==`, `knownFields()` sorted, `getField`/`setField` explicit throws on unknown field, `toJson` deterministic sorted keys, `fromJson` strict throws on malformed/invalid
- `core/profile/UserProfile.cpp` (stub)
- `core/profile/ProfileStore.h:1` (110L) - `profilePath()` (`HOME/.local/state/polaris/profile.json`), `testProfilePath()`, `load` (missing→unknown no create, symlink→throw, malformed→throw + audit `profile.load.malformed`), `save` (validate path traversal/metachars/allowlist, symlink check, `create_directories`, atomic `tmp+fsync+chmod 0600+rename`, parent canonical check, 0600), `exists`/`remove`
- `core/profile/ProfileStore.cpp` (stub)
- `core/profile/ProfileService.h:1` (80L) - `ProfileUpdateResult` (`success`, `field`, `previousValue`/`newValue`, `timestamp`, `auditOperation`, `reason`), `ProfileService::updateField` (validates known field via `knownFields`, validates value `yes/no/unknown`, checks `isIdempotent` → `profile.update.idempotent` audit skips write, else `setField` + `save` + `profile.updated` audit with `field`/`previous`/`new`, no inference, `updateFieldInStore`, `isIdempotent`)
- `core/profile/ProfileService.cpp` (stub)
- `core/profile/ProfileAdvisor.h:1` (40L) - `Decision` enum, `AdvisorResult` (`decision`, `reason`, `causingField/value`, `explicitFact`, `whatWillNotChange`, `confirmationRequired`, `candidate`)
- `core/profile/ProfileAdvisor.cpp:1` (200L) - `canConsiderAkonadi` (YES any of `usesKMail/Kontact/KOrganizer/Akonadi` → `BLOCKED_BY_USER_WORKFLOW` with field-specific reason, `whatWillNotChange` “Akonadi will remain enabled…”, `confirmationRequired` explicit no needed; UNKNOWN any → `REQUIRES_USER_CONFIRMATION`; all NO → `ALLOWED_FOR_ANALYSIS` with “does NOT authorize mutation; still requires RECOMMEND→PREVIEW→APPROVAL…”); `canConsiderBluetooth`, `canConsiderPrinting` (YES `usesPrinting` or `usesCups` → BLOCKED), `canConsiderAvahi`/`Cups`, generic `canConsider`
- `tests/unit/test_p13_profile_model.cpp` (80L) - 6 cats: default unknown, explicit yes/no/unknown, deterministic serialization, round-trip, missing profile, malformed
- `tests/unit/test_p13_profile_store.cpp` (90L) - 6 cats: atomic 0600 deterministic, symlink rejection, traversal/metachars, malformed, deterministic file, real profile not touched
- `tests/security/test_p13_profile_service.cpp` (90L) - 6 cats: explicit updates, no inference (`KMail→Akonadi` not inferred), audit generation, idempotency (mtime unchanged), unknown field rejected, explicit vs unknown
- `tests/unit/test_p13_profile_advisor.cpp` (120L) - 12 cats: KMail/Kontact/Akonadi/KOrganizer yes→BLOCKED, unknown→REQUIRES, explicit no→ALLOWED, Bluetooth yes→BLOCKED/unknown→REQUIRES/no→ALLOWED, Printing/Avahi similarly, profile never becomes approval (ALLOWED still needs transaction approval), no host mutation, explainability strings
- `docs/P13_PLAN.md` (15K)
- `docs/P13_IMPLEMENTATION_REPORT.md` (this)

No modification to: `Real*Provider`, `BaselineEngine`, `ComparisonEngine`, `Transaction`/`StateMachine`/`BackupEngine`/`ReadOnlyGuard` (preserve P12 invariants), `akonadi`, `mssql`, `nvidia`, `fstab`, `zram`.

---

## 3. Profile Model

**Schema (deterministic JSON, sorted keys):**

```json
{"usesAkonadi":"unknown","usesAvahi":"unknown","usesBluetooth":"unknown","usesCups":"unknown","usesKMail":"unknown","usesKOrganizer":"unknown","usesKontact":"unknown","usesPrinting":"unknown"}
```

**Semantics:**
- `unknown` = not yet declared (default for all 8 fields, also for `extra`); missing keys in JSON decode to `unknown` (but `fromJson` strict: malformed JSON throws, invalid value `maybe` throws).
- `yes` = user explicitly declared they use workflow (e.g., `usesKMail=yes`).
- `no` = user explicitly declared they do NOT use workflow (e.g., `usesBluetooth=no`).

**Extensible:** `extra` `map<string,TriState>` sorted; `knownFields()` returns 8 sorted; `setField` throws on unknown field (future extension via code change, not silent).

**Example known decisions (from P8/P9):**
- Real host: `usesKMail=yes`, `usesKontact=yes` (user uses KMail) → `Akonadi` is `BLOCKED_BY_USER_WORKFLOW` (must not be disabled unless user explicitly changes to `no`).

**Backward compatibility:** No prior `profile.json` → `UserProfile::isDefaultUnknown()==true`; old transactions unaffected.

---

## 4. Persistence

**Real path:** `~/.local/state/polaris/profile.json` (`getenv("HOME")` + `/.local/state/polaris/profile.json`, fallback `/tmp/polaris-state/profile.json`).

**Test path:** `/tmp/polaris-test-root/profile.json` or `dir/profile.json` (injected via `ProfileStore::save(profile, path)`).

**Requirements satisfied:**
- JSON format, deterministic (`toJson` sorted).
- Atomic write: `tmp.<pid>` + `fwrite` + `fflush` + `fsync` + `chmod 0600` + `isRegularFile(tmp)` check + parent `canonical` check + `rename` + `chmod 0600` final. No partial/corrupt on interrupt (temp not renamed until fsync).
- Permissions 0600 (`chmod` after tmp and after rename).
- No secrets/passwords (only `yes/no/unknown`).
- No arbitrary paths: `validateProfilePath` rejects `..`, `;|&` `` ` `` `$`, `NUL`, `>4096`, not in allowlist (`/tmp/polaris-test-root/` or `HOME/.local/state/polaris/`). Symlink rejected before read/write (`is_symlink`).
- Do not follow unsafe symlinks: `canonical` parent validation.
- If file does not exist, `load` returns unknown profile and **does not** create file (verified `exists(path)==false` after load).
- Malformed JSON: `load` throws `invalid_argument` and audits `profile.load.malformed` (so caller can handle, tests assert throw).
- Uses `FileSafety` mechanisms: allowlist extended, `isRegularFile`, `isSymlink`, `canonical` reused.

**Tests:** atomic 0600, symlink rejected (real→link), traversal `..` and `; rm` rejected, malformed throws, deterministic file hash for same content, real profile not touched by test fixtures.

---

## 5. Explicit Update API

`ProfileService::updateField(UserProfile& profile, const std::string& field, TriState value, const std::string& path)`:

1. Validate `field` in `knownFields()` else throw `invalid_argument` and audit `profile.update.rejected.unknown_field`.
2. Validate `value` via `fromString` (already TriState).
3. `previous = profile.getField(field)`.
4. If `previous == value` → idempotent: audit `profile.update.idempotent` with `field` `previous` `new` `applied=false`, **skip** `save` (mtime unchanged, file hash unchanged) → deterministic.
5. Else `profile.setField(field, value)` explicit (no inference of related fields), `ProfileStore::save(profile, path)` atomic, audit `profile.updated` with `field` `previous` `new` `applied=true`.

**No inference:** `updateField(usesKMail, YES)` does **not** set `usesAkonadi` or `usesKontact` (verified `test_no_inference`).

**String overload:** `updateField(profile, field, "yes"/"no"/"unknown", path)` parses via `fromString`.

**Convenience:** `updateFieldInStore(field, valueStr, path)` loads (or default if missing/malformed) then updates.

**Tests:** explicit `yes`/`no`/`unknown` via `TriState` and string, load/save round-trip, audit `profile.updated` contains `field`/`previous`/`new`, idempotent skips write (stat mtime same), unknown field rejected, `explicit vs unknown` distinguish.

---

## 6. Auditability

Uses existing `AuditLog::append` (fsync per event, hash chain `previousHash`).

**Operations:**
- `profile.updated` - `error="field=usesKMail previous=unknown new=yes applied=true"`
- `profile.update.idempotent` - `field=usesKMail previous=yes new=yes applied=false`
- `profile.load.malformed` - `malformed profile at <path>: <what>`
- `profile.update.rejected.unknown_field` - `Unknown profile field: <field>`

Each preserves `timestamp` (`nowISO`), `transactionId="PROFILE"`, `user="test"`, `previousHash` chain, no secrets, no passwords, no arbitrary user content beyond `field` and `yes/no/unknown`.

**Tests:** `test_audit_event_generation` checks `audit.log` contains `profile.updated` with `previous=unknown` `new=yes`; `test_real_profile_not_touched` ensures no stray audit for `PROFILE` when using test path.

---

## 7. Recommendation Integration

`ProfileAdvisor` (pure, side-effect-free, deterministic):

**Akonadi** (critical):
- `usesKMail==YES` || `usesKontact==YES` || `usesKOrganizer==YES` || `usesAkonadi==YES` → `BLOCKED_BY_USER_WORKFLOW`, `reason` includes `uses<Changed>=yes. Akonadi must remain available for ...`, `explicitFact=true`, `whatWillNotChange="Akonadi will remain enabled and running; 14 agents, 1302M..."`, `confirmationRequired="User must explicitly set usesKMail=no, usesKontact=no, usesKOrganizer=no and usesAkonadi=no to consider (current: usesKMail=yes)."`.
- Any `UNKNOWN` and none `YES` → `REQUIRES_USER_CONFIRMATION`, `explicitFact=false`, `reason` `usesKMail=unknown... User workflow not yet declared.`, `confirmationRequired` `... currently usesKMail=unknown`.
- All `NO` → `ALLOWED_FOR_ANALYSIS`, `reason` `... may be considered for analysis because ... This does NOT authorize mutation; still requires RECOMMEND→PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY.`, `whatWillNotChange` `... until a future transaction is previewed... profile alone does not mutate.`

**Bluetooth:** `usesBluetooth==YES` → `BLOCKED`; `UNKNOWN` → `REQUIRES`; `NO` → `ALLOWED` (with `does NOT authorize mutation`).

**Printing:** `usesPrinting==YES` or `usesCups==YES` → `BLOCKED`; `UNKNOWN` any → `REQUIRES`; both `NO` → `ALLOWED`.

**Avahi/Cups:** similar.

**Safety:** `ALLOWED_FOR_ANALYSIS` explicitly states profile is **constraint, NOT approval** - even `usesBluetooth=no` still requires full P12 transaction flow (tested `test_profile_never_becomes_approval`: create `bluetooth-disable` transaction without approval, `validateForApply` still fails even though advisor says `ALLOWED`).

**Explainability:** every `AdvisorResult` provides `reason`, `causingField/value`, `explicitFact`, `whatWillNotChange`, `confirmationRequired` - tested `test_explainability` asserts non-empty.

---

## 8. CLI

Extended `cli/p4_cli.cpp` without new binary:

```
polaris_p4 profile show [--json]
  → ProfileStore::load(profilePath) (missing→unknown, no file creation); prints JSON sorted; with --json same JSON; without --json also prints "# Profile: <path> (exists/not exists)" and Akonadi advisor example

polaris_p4 profile set <field> <yes|no|unknown> [--json]
  → ProfileService::updateField with real path; validates field/value; saves atomically; audits; prints {"field":"usesKMail","previousValue":"unknown","newValue":"yes","status":"updated"|"idempotent"}; error to stderr {"error":"..."} for unknown field
  → Help: "Fields: usesKMail, usesKontact, usesKOrganizer, usesBluetooth, usesPrinting, usesAvahi, usesCups, usesAkonadi"
```

Follow existing conventions: `cmd == "profile"` dispatch, `--json` flag, `stderr` for errors, not equal to authorization for host mutation (only profile file write).

**Verification:** `/tmp/polaris_p13_build/polaris_p4 profile show` prints `{"usesAkonadi":"unknown",...}` and `Akonadi: REQUIRES_USER_CONFIRMATION ...` without creating `~/.local/state/polaris/profile.json` (verified `ls` before/after `not exists`).

---

## 9. Tests and Exact Results

**New tests (P13, fixtures `/tmp/polaris-test-root` only, never touch real profile):**

- `test_p13_profile_model` - 6 cats: default unknown, explicit yes/no/unknown, deterministic serialization (same profile→same JSON, keys sorted, two insertion orders→same JSON), load/save round-trip (save then load equal), missing profile (load→unknown without create), malformed (fromJson throws, ProfileStore::load throws, invalid value `maybe` throws)
- `test_p13_profile_store` - 6 cats: atomic persistence (0600, regular not symlink, deterministic second save same hash), symlink rejection (load/save throw), traversal/metachar rejection, malformed handling (load throws + audit), deterministic file serialization (same values different insertion→same file), real profile not touched (stat before/after mtime same or not exists remains)
- `test_p13_profile_service` - 6 cats: explicit updates (yes/no/unknown via TriState and string, persisted), no inference (`usesKMail→usesAkonadi` stays unknown), audit generation (`profile.updated` with previous/new in audit.log), idempotency (same value→`profile.update.idempotent`, mtime unchanged), unknown field rejected, explicit vs unknown semantics
- `test_p13_profile_advisor` - 12 cats: KMail yes→BLOCKED, Kontact yes→BLOCKED, Akonadi yes→BLOCKED, KOrganizer yes→BLOCKED, unknown→REQUIRES (Akonadi and Bluetooth), explicit no all→ALLOWED (Akonadi and Bluetooth), Bluetooth yes→BLOCKED, Printing yes→BLOCKED / Cups yes→BLOCKED, Avahi yes→BLOCKED, profile never becomes approval (ALLOWED still needs transaction approval), no host mutation (real profile stat unchanged), explainability (reason/whatWillNotChange/confirmationRequired non-empty)

**Total:** `ctest 17/17 0.13s 100%`

```
1/17 unit                      Passed 0.00s
2/17 real_providers            Passed 0.04s
3/17 parsers                   Passed 0.00s
4/17 readonly                  Passed 0.00s
5/17 p4_security               Passed 0.01s (9 checks)
6/17 comparison                Passed 0.00s (12 cats)
7/17 post_change               Passed 0.00s
8/17 regression                Passed 0.00s
9/17 observed_benefit          Passed 0.00s
10/17 p12_stale                Passed 0.01s
11/17 p12_idempotency          Passed 0.01s
12/17 p12_statemachine         Passed 0.00s
13/17 p12_transaction_model    Passed 0.01s
14/17 p13_profile_model        Passed 0.01s
15/17 p13_profile_store        Passed 0.01s
16/17 p13_profile_service      Passed 0.01s
17/17 p13_profile_advisor      Passed 0.00s
```

Existing P12 security/idempotency/state-machine tests still pass (13/13 → 17/17 after P13). No test weakened.

---

## 10. Build Result

```
cmake -S . -B /tmp/polaris_p13_build --fresh → Configuring done, Generating done
cmake --build /tmp/polaris_p13_build → 100% Built polaris, polaris_real, polaris_tests, test_real_providers, test_parsers, test_readonly, polaris_p3, polaris_p4, polaris_p5, test_comparison, test_post_change, test_regression, test_observed_benefit, test_p12_stale, test_p12_idempotency, test_p12_statemachine, test_p12_transaction_model, test_p13_profile_model, test_p13_profile_store, test_p13_profile_service, test_p13_profile_advisor
ctest → 17/17 100% 0.13s
```

No `-Werror` warnings beyond deprecated `SHA256_*` (already `Wno-error=deprecated-declarations`).

---

## 11. Security Verification

- No `sudo`, `systemctl`, `dnf`, `dracut`, `modprobe`, `sh -c` in P13 code/tests (grep 0 except read-only `systemctl is-enabled` in P6 history).
- `FileSafety::validatePath` still rejects `..`, `;|&` `` ` `` `$`, `NUL`, `>4096`, symlink, non-allowlist - extended only for profile, not weakened.
- `StateMachine` still fail-closed, `TransactionValidator` still checks approval, `ReadOnlyGuard` intact, `BackupEngine` no overwrite, `AuditLog` fsync preserved.
- `ProfileStore::save` checks symlink and canonical before rename; `chmod 0600` ensures owner-only.
- No password collection, no secrets in profile (only `yes/no/unknown`), audit does not log passwords.

---

## 12. Host-Modification Verification

P13 is engineering phase - **no real-host mutation**:

- `stat /etc/fstab` `2026-08-31 21:19` unchanged
- `systemctl is-enabled mssql-server` `disabled` (P6) unchanged
- `systemctl is-enabled bluetooth` `enabled` unchanged (profile `usesBluetooth=no` would not disable)
- `akonadictl status` `running` (not disabled, profile `usesKMail=yes` blocks)
- `ls /run/polaris/helper.sock` not exists
- `ls ~/.local/state/polaris/profile.json` **not exists** before and after `ctest` (tests used `/tmp/polaris-test-root` fixtures only); `polaris_p4 profile show` without `set` does not create file (verified `not exists` before/after).
- Only `policies/profile set` would create real profile, but not invoked during P13 tests/validation; no `dnf`/`systemctl`/`reboot`/`modprobe` executed (grep `systemctl` in P13 tests 0, only profile fixtures).

---

## 13. Known Limitations / Not Implemented

- **Profile scope:** Only 8 workflow fields now; future fields (e.g., `usesDocker`, `usesVPN`) require code change to `knownFields()` + advisor (extensible via `extra` map but not yet exposed via CLI/advisor).
- **No UI interview:** P13 provides CLI `profile show/set` and service API, but no interactive `question` UI to suggest `usesKMail?` based on `kmail` installed - future P16 HCI may add prompt.
- **No automatic recommendation update:** `RecommendationEngine` 7 recs not yet automatically re-ranked based on profile; `ProfileAdvisor` is standalone constraint layer, not yet integrated into `RecommendationEngine::rank` (future P13→P17).
- **No profile versioning/migration:** `profile.json` has no `version` field; current `UserProfile` will treat old file with missing keys as `unknown` (forward compatible), but no explicit migration logic.
- **No encryption:** Profile at `~/.local/state/polaris/profile.json` is plaintext 0600, not encrypted - acceptable because no secrets, only workflow booleans; future could add `xdg` portal if needed.
- **Concurrent profile writes:** No `flock` on profile file; concurrent `profile set` from two processes could interleave (last wins, atomic per write but not transactional) - acceptable for single-user interview, future P14 may add `flock`.

All limitations are documented and do not weaken safety - profile remains constraint, not approval.

---

## 14. Next Phase

**P14 - Expanded Security & IPC/Helper Architecture** (minimal helper, `SO_PEERCRED`, `audit fsync` already, `flock`, `crash recovery`). P13 unblocked `isStale` + profile for P14 helper trust boundary. Do NOT implement P14 now - STOP after P13 engineering.

---

## 15. Verification Commands

```
rm -rf /tmp/polaris_p13_build && cmake -S ~/Documents/lin-opt -B /tmp/polaris_p13_build --fresh && cmake --build /tmp/polaris_p13_build && ctest --test-dir /tmp/polaris_p13_build --output-on-failure
stat /etc/fstab | grep Modify  # 2026-08-31 21:19 unchanged
systemctl is-enabled mssql-server  # disabled
ls ~/.local/state/polaris/profile.json  # not exists (unless user explicitly set) - tests use /tmp/polaris-test-root
ls -R /tmp/polaris-test-root | head
cat /tmp/polaris-test-root/audit.log | grep profile.updated | tail
polaris_p4 profile show --json
```

---

*No real-host optimization was performed during P13. No reboot occurred. No unrelated project area was modified beyond profile engine.*

