# P13 - User Workflow / Profile Engine - Plan

**Phase:** P13 - Engineering, Not Host Optimization  
**Mode:** READ-ONLY PLANNING + IMPLEMENTATION - no `dnf`, `systemctl`, `akmods`, `dracut`, `sudo`, `reboot`, no Akonadi/NVIDIA/mssql/fstab/zram mutation - only `core/profile` engineering, `tests/` fixtures `/tmp/polaris-test-root`, `docs/` updates  
**Date:** 2026-09-01 04:00 +0330  
**Source:** Inspect `~/Documents/lin-opt` P12 state (`core/safety/transaction/TransactionValidator.h`, `TransactionStore.h`, `core/safety/FileSafety.h`, `AuditLog`, `docs/PROJECT_HANDOFF.md` known decisions Akonadi REJECTED, NVIDIA COMPLETED)  
**Dependency:** P12 `TransactionValidator/Store` (COMPLETED, stale/idempotency) - P13 adds profile constraints on top without touching approval flow

---

## 1. What P12 Left & Why P13 Is Next

P12 hardened `PREVIEW→APPROVAL→VALIDATION→BACKUP→FINAL→APPLY` and `COMPLETED` idempotency, but recommendations still lack user workflow context:

- `RecommendationEngine` 7 recs + `p8/p9_analysis.json` rank candidates (`akonadi` 1302M R2, `bluetooth` 5M) without knowing `usesKMail=yes` → would have suggested akonadi disable with confidence 0.65 instead of blocked.
- No persisted `profile.json` - every session re-asks `usesKMail?` via `question` tool; not auditable, not reversible.
- Future `P14` helper and `P17` Campaign2 need `isStale` + profile to avoid proposing `akonadi` or `bluetooth` to users who actually use them.

P13 value: make future recommendations **workflow-aware** while keeping `Profile ≠ Approval` (even `usesBluetooth=no` still requires `PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY`).

---

## 2. Objectives (Engineering, Not Optimization)

1. **Explicit profile model** with `unknown/yes/no` (not missing≡false) for `usesKMail`, `usesKontact`, `usesKOrganizer`, `usesBluetooth`, `usesPrinting`, `usesAvahi`, `usesCups`, `usesAkonadi` + extensible map.
2. **Persistence** `~/.local/state/polaris/profile.json` - deterministic JSON, atomic write (temp+fsync+rename), 0600, no symlink, canonical validation via `FileSafety`.
3. **Explicit update API** `profile set <field> <yes|no|unknown>` - no inference (`KMail→Akonadi` not inferred, `bluetooth` hardware ≠ `usesBluetooth`).
4. **Auditability** - each `profile.updated` event records `field`, `previousValue`, `newValue`, `timestamp`, `operation/result`, no secrets.
5. **Recommendation integration** - safe `ProfileAdvisor` answers `canConsiderAkonadi?`, `canConsiderBluetooth?` etc. with `BLOCKED_BY_USER_WORKFLOW` for `yes`, `REQUIRES_USER_CONFIRMATION` for `unknown`, `ALLOWED_FOR_ANALYSIS` for explicit `no` (still needs approval).
6. **Explainability** - every decision returns `reason`, `causingField/value`, `explicitFact`, `whatWillNotChange`, `confirmationRequired`.

---

## 3. Design Constraints

- Keep `core` no Qt, offline-first, deterministic, no network.
- Layers: `Domain (UserProfile/TriState)` → `Store (ProfileStore)` → `Service (ProfileService)` → `Advisor (ProfileAdvisor)` → `CLI (profile show/set)` - not coupled to KDE/systemd/DNF.
- Do not infer; do not tie profile to `Transaction` approval flow; profile is **constraint**, not approval.
- Preserve existing invariants: `ReadOnlyGuard`, `FileSafety`, `StateMachine`, `BackupEngine`, `AuditLog`, `beforeHash` etc. unchanged.
- Backward compatibility: old transactions load, old tests pass.

---

## 4. Domain Model

`core/profile/UserProfile.h`:

```cpp
enum class TriState { UNKNOWN, YES, NO };
std::string toString(TriState) // "unknown"/"yes"/"no"
TriState fromString(const std::string&) // throws on invalid

struct UserProfile {
  TriState usesKMail = TriState::UNKNOWN;
  TriState usesKontact = TriState::UNKNOWN;
  TriState usesKOrganizer = TriState::UNKNOWN;
  TriState usesBluetooth = TriState::UNKNOWN;
  TriState usesPrinting = TriState::UNKNOWN;
  TriState usesAvahi = TriState::UNKNOWN;
  TriState usesCups = TriState::UNKNOWN;
  TriState usesAkonadi = TriState::UNKNOWN;
  std::map<std::string, TriState> extra; // future keys sorted

  bool isDefaultUnknown() const;
  bool operator==(const UserProfile&) const;
  std::string toJson() const; // deterministic sorted keys, compact
  static UserProfile fromJson(const std::string&); // throws on malformed? For store, catch and return UNKNOWN
  static std::vector<std::string> knownFields(); // list of 8 fields
  TriState getField(const std::string& field) const; // throws if unknown field
  void setField(const std::string& field, TriState value); // throws if unknown field, but extra map allows future? For P13, restrict to known list, future extensible via extra
};
```

- Unknown default, not `false`. All fields start `UNKNOWN`.
- `toJson` deterministic: keys alphabetical, values `"unknown"/"yes"/"no"`, extra sorted.
- `fromJson` parses minimal JSON `{"usesKMail":"yes", ...}` ignoring unknown keys? For malformed, throw `invalid_argument` - `ProfileStore::load` will catch and treat as empty + audit `profile.load.malformed`.

---

## 5. Persistence (ProfileStore)

`core/profile/ProfileStore.h/.cpp`:

- `static std::string profilePath()` → `~/.local/state/polaris/profile.json` (`$HOME` or `/tmp` fallback)
- `static std::string testProfilePath()` → `/tmp/polaris-test-root/profile.json` (for tests; also helper `testProfilePath(dir)`)
- `static UserProfile load(const std::string& path = profilePath())` - if not exists → `UserProfile{}` unknown (do NOT create file on read); if malformed → return unknown + audit `profile.load.malformed` (no throw to caller? But tests need to assert malformed handling - return `std::optional` or throw? Choose to return `UserProfile` but also log; `load` will throw `runtime_error` on malformed for explicit test, and test catches. For safety, `load` throws on malformed, caller handles.)
- `static void save(const UserProfile& profile, const std::string& path = profilePath())` - validate path: `FileSafety::validatePath` extended to allow `profilePath`, check `isSymlink` false, `canonical` parent matches, then atomic write: `ProfileStore::atomicWriteJson(path, json)` using `FileSafety::atomicWrite` pattern (temp+fsync+rename, 0600). Ensure deterministic JSON, no partial/corrupt (temp file, fsync, rename). Permissions 0600 (owner read/write only). No secrets, no passwords.
- Extend `FileSafety::isAllowedPath` and `validatePath` to allow `~/.local/state/polaris/profile.json` and `/tmp/polaris-test-root/**` (already allowed prefix covers test path). Real path check: `if(path.rfind(homeState,0)==0) allow` where `homeState = HOME/.local/state/polaris/`.

Atomicity test: simulate interrupt by writing then verifying file exists only after rename (already tested via `BackupEngine` backup no overwrite, but profile adds explicit atomic).

---

## 6. Explicit Update API (ProfileService)

`core/profile/ProfileService.h/.cpp`:

```cpp
struct ProfileUpdateResult {
  bool success;
  std::string field;
  TriState previousValue;
  TriState newValue;
  std::string timestamp;
  std::string auditOperation; // "profile.updated"
  std::string reason;
};

class ProfileService {
  static ProfileUpdateResult updateField(UserProfile& profile, const std::string& field, TriState value, const std::string& path = ProfileStore::profilePath());
  static ProfileUpdateResult updateField(UserProfile& profile, const std::string& field, const std::string& valueStr); // parse yes/no/unknown
  static bool isIdempotent(const UserProfile& before, const std::string& field, TriState value); // same value → idempotent
};
```

- Explicit: caller must pass field name and value; no inference.
- Validate field is known (one of 8) - if unknown, throw `invalid_argument` → CLI prints error, audit `profile.update.rejected.unknown_field`.
- Check idempotency: if `profile.getField(field) == value` → `success=true`, `auditOperation="profile.update.idempotent"` with `previousValue==newValue`, no file rewrite needed? But we can still rewrite to be deterministic (or skip). Choose to skip write (deterministic, no mtime change) and audit idempotent.
- Otherwise set `profile.setField(field, value)`, then `ProfileStore::save`, then `AuditLog::append` with `operation="profile.updated"`, `error="field=usesKMail previous=unknown new=yes applied=true"`.
- Do not infer `usesKMail=yes` → `usesAkonadi=yes` - keep separate.
- Audit always: `field`, `previousValue`, `newValue`, `timestamp` (`nowISO`), `operation/result`, no secrets.

---

## 7. Auditability

Use existing `AuditLog::append` (now fsync per event, hash chain). For profile:

- `operation="profile.updated"` with `error="field=usesKMail previous=unknown new=yes applied=true"`
- `operation="profile.update.idempotent"` for same value
- `operation="profile.load.malformed"` for malformed JSON (when `load` catches)
- `operation="profile.update.rejected.unknown_field"` for invalid field

Each event preserves `previousHash` chain, `timestamp`, `user`, no passwords, no arbitrary user content beyond field name and `yes/no/unknown`.

---

## 8. Recommendation Integration (ProfileAdvisor)

`core/profile/ProfileAdvisor.h/.cpp`:

```cpp
enum class Decision { BLOCKED_BY_USER_WORKFLOW, REQUIRES_USER_CONFIRMATION, ALLOWED_FOR_ANALYSIS };
struct AdvisorResult {
  Decision decision;
  std::string reason; // human
  std::string causingField; // e.g., "usesKMail"
  std::string causingValue; // "yes"
  bool explicitFact; // true if YES/NO explicit, false if UNKNOWN
  std::string whatWillNotChange; // e.g., "Akonadi will remain enabled and running"
  std::string confirmationRequired; // e.g., "User must explicitly set usesKMail=no to consider"
};

class ProfileAdvisor {
  static AdvisorResult canConsiderAkonadi(const UserProfile&);
  static AdvisorResult canConsiderBluetooth(const UserProfile&);
  static AdvisorResult canConsiderPrinting(const UserProfile&);
  static AdvisorResult canConsiderAvahi(const UserProfile&);
  // Generic: canConsider(const UserProfile&, const std::string& candidateId)
};
```

- **Akonadi rule** (critical): if `usesKMail==YES` || `usesKontact==YES` || `usesAkonadi==YES` → `BLOCKED_BY_USER_WORKFLOW`, reason `"Akonadi optimization is blocked because <field>=yes. Akonadi must remain available for <KMail/Kontact/Akonadi> workflow."`, `explicitFact=true`, `whatWillNotChange="Akonadi will remain enabled; 14 agents, 1302M, db 126M will not be disabled"`, `confirmationRequired="User must explicitly set usesKMail=no and usesKontact=no and usesAkonadi=no to consider (current: ...)"`.
- If any of those is `UNKNOWN` and none is `YES` → `REQUIRES_USER_CONFIRMATION`, reason `"Akonadi optimization requires user confirmation because <field>=unknown. User workflow not yet declared."`, `explicitFact=false`, `whatWillNotChange` same.
- If all are `NO` → `ALLOWED_FOR_ANALYSIS`, reason `"Akonadi optimization may be considered for analysis because user explicitly declared usesKMail=no, usesKontact=no, usesAkonadi=no. This does NOT authorize mutation; still requires RECOMMEND→PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY."`, `explicitFact=true`.
- Similar for `Bluetooth`: `usesBluetooth==YES` → `BLOCKED` (`usesBluetooth=yes` fact), `UNKNOWN` → `REQUIRES`, `NO` → `ALLOWED`.
- `Printing`: `usesPrinting==YES` or `usesCups==YES` → `BLOCKED`; `UNKNOWN` → `REQUIRES`; `NO` → `ALLOWED`.
- `Avahi`: `usesAvahi==YES` → `BLOCKED`; etc.
- **Safety:** Even `ALLOWED_FOR_ANALYSIS` does **not** bypass `StateMachine`, `beforeHash`, `backup`; tests assert `ProfileAdvisor` result never equals transaction approval; `TransactionValidator` still requires approval.

Explainability strings are deterministic, tested.

---

## 9. CLI

Extend `cli/p4_cli.cpp` (existing, already handles `transaction preview/list/show/approve`, `audit list`, `apply --dry-run`) with:

```
polaris_p4 profile show [--json]
  → load profile (real path) without auto-creating file if missing; print JSON {"usesKMail":"unknown", ...} or pretty

polaris_p4 profile set <field> <yes|no|unknown> [--json]
  → validate field exists, value is yes/no/unknown, load profile (or empty if missing), call ProfileService::updateField, save atomically, audit, print {"field":"usesKMail","previousValue":"unknown","newValue":"yes","status":"updated"} or {"status":"idempotent"}
```

- Follow existing CLI conventions: subcommand `profile` with `show`/`set`, `--json` flag, error to `stderr`, exit 0/1.
- Do NOT make `profile set` equivalent to authorization for host mutation - just profile file write, no `systemctl`/`dnf`.
- For tests, CLI will use test path? For real CLI, use `profilePath()`; tests will directly use `ProfileService`/`ProfileStore` with `/tmp/polaris-test-root` path, not via CLI subprocess modifying real profile.

No new binary; keep `polaris`, `polaris_real`, `polaris_p3`, `polaris_p4`, `polaris_p5`.

---

## 10. Tests (14+ cases, fixtures `/tmp/polaris-test-root`)

**File `tests/unit/test_p13_profile_model.cpp`:**
- default unknown state (all fields UNKNOWN, `isDefaultUnknown` true)
- explicit yes/no/unknown set/get
- deterministic serialization (same profile → same JSON, keys sorted, round-trip)
- load/save round-trip (save then load equal)
- missing profile (load non-existent → UNKNOWN, do not create file)

**File `tests/unit/test_p13_profile_store.cpp`:**
- atomic persistence (save writes temp then rename, file exists only after complete)
- unsafe/symlink path rejection (`profile.json` symlink → throw, `..` traversal)
- malformed profile (write invalid JSON, load throws or returns UNKNOWN + audit)
- deterministic serialization (two profiles same values → same file hash)

**File `tests/security/test_p13_profile_service.cpp`:**
- audit event generation (updateField creates `profile.updated` with previous/new)
- idempotency (set same value → `idempotent` audit, file mtime unchanged? Check hash same)
- explicit yes/no/unknown (set each, load back equals)

**File `tests/unit/test_p13_profile_advisor.cpp`:**
- profile-aware recommendation: KMail yes → Akonadi BLOCKED
- Kontact yes → BLOCKED
- Akonadi yes → BLOCKED
- all unknown → REQUIRES_USER_CONFIRMATION
- explicit no all → ALLOWED_FOR_ANALYSIS
- Bluetooth yes → BLOCKED, unknown → REQUIRES, no → ALLOWED
- Printing yes → BLOCKED, unknown → REQUIRES, no → ALLOWED
- Avahi yes → BLOCKED
- profile never becomes transaction approval (Advisor ALLOWED does not imply `TransactionStore::apply` without approval - test that `validateForApply` still fails if not approved, even with profile ALLOWED)
- explainability strings contain causingField, explicitFact, whatWillNotChange, confirmationRequired

All tests use `/tmp/polaris-test-root` fixtures, never modify `~/.local/state/polaris/profile.json` (verify via `stat` before/after that real profile not created if didn't exist, or mtime unchanged).

---

## 11. Implementation Files (Minimal)

- **New domain:** `core/profile/UserProfile.h/.cpp` (TriState, UserProfile, JSON)
- **New store:** `core/profile/ProfileStore.h/.cpp` (path, load, save, atomic, validate, permissions 0600)
- **New service:** `core/profile/ProfileService.h/.cpp` (updateField, audit, idempotency)
- **New advisor:** `core/profile/ProfileAdvisor.h/.cpp` (Decision, AdvisorResult, canConsider*)

- **Extend:** `core/safety/FileSafety.h` - allow `~/.local/state/polaris/profile.json` and `/tmp/polaris-test-root` already covers test; add `isAllowedPath` for profile
- **Extend:** `cli/p4_cli.cpp` - add `cmd_profile_show`, `cmd_profile_set`, dispatch `profile` subcommand
- **Update:** `CMakeLists.txt` - add `core/profile/*.cpp` to `polaris_core`, add 4 test executables
- **No change:** `Transaction`, `StateMachine`, `BackupEngine`, `ReadOnlyGuard`, real providers - preserve invariants

---

## 12. Validation (Before COMPLETE)

- Clean `cmake -S . -B /tmp/polaris_p13_build --fresh && cmake --build && ctest` - must be 17/17 (existing 13 + 4 P13)
- Verify `p4_security` 9/9, `p12_*` 13/13 still pass
- Verify no `sudo`, `systemctl`, `dnf`, `dracut`, `reboot`, `modprobe` in P13 tests (grep)
- Verify `stat ~/.local/state/polaris/profile.json` not created by tests (if not existed before, still not exists after `ctest`; if existed before, mtime unchanged) - check via fixture-only writes
- Verify docs: `P13_PLAN.md`, `P13_IMPLEMENTATION_REPORT.md`, `ROADMAP.md` P13 COMPLETE, `ARCHITECTURE.md` layer added, `TRANSACTION_MODEL.md` note profile does not bypass approval

---

## 13. Documentation to Update

- `docs/P13_PLAN.md` (this)
- `docs/P13_IMPLEMENTATION_REPORT.md` (post-impl: files, schema, CLI, advisor, audit, tests, build, host verification, limitations, next P14)
- `docs/ARCHITECTURE.md` - add Profile Engine layer
- `docs/ROADMAP.md` - P13 COMPLETE, next P14
- `docs/PROJECT_STATE.json` - currentPhase P13, next P14, completedPhases + P13, tests 17/17
- `docs/PROJECT_HANDOFF.md` - P13 section, host state still no reboot, candidate Akonadi still REJECTED now via profile `usesKMail=yes`

---

## 14. Safety Boundaries Summary

- Profile is **input to future recommendations**, not a trigger.
- Even `usesAkonadi=no` → still requires `RECOMMEND→PREVIEW→APPROVAL→BACKUP→APPLY→VERIFY` and explicit transaction approval per P12.
- No inference; no `KMail→Akonadi` auto; no `bluetooth` hardware → `usesBluetooth`.
- All profile writes atomic, 0600, auditable, reversible (`set` back to `unknown`/`yes`).
- Tests never touch real profile; CLI real path only when user explicitly runs `polaris_p4 profile set`.

---

## 15. Not In Scope (P14 onward)

- No helper `SO_PEERCRED`, no `flock`, no `recover`, no GUI, no optimization campaign - those remain P14+.
- Do not implement automatic optimization based on `usesKMail=no` - just make `Akonadi` *considerable* for future analysis.

