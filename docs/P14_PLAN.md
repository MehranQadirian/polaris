# P14 - Expanded Security & IPC / Helper Architecture - Plan

**Phase:** P14 - Engineering/Security, Not Host Optimization  
**Mode:** READ-ONLY PLANNING + IMPLEMENTATION - no `systemctl`, `dnf`, `akmods`, `dracut`, `sudo`, `reboot`, no Akonadi/NVIDIA/mssql/fstab/zram mutation - only `core/ipc`, `core/safety/lock`, `core/safety/recovery` engineering, `tests/` fixtures `/tmp/polaris-test-root`, `docs/` updates  
**Date:** 2026-09-01 05:00 +0330  
**Source:** Inspect `~/Documents/lin-opt` P13 state (`core/profile/UserProfile.h` 8 fields, `core/safety/transaction/TransactionValidator.h` stale checks, `TransactionStore.h` duplicate/idempotent, `FileSafety.h` allowlist, `AuditLog` fsync, `docs/PROJECT_HANDOFF.md` known decisions Akonadi BLOCKED, NVIDIA COMPLETED, mssql disabled)  
**Dependency:** P13 `UserProfile` (COMPLETED) - P14 hardens IPC boundary needed for future privileged operations before any real-host optimization

---

## 1. What P13 Left & Why P14 Is Next

P13 added `UserProfile` with `UNKNOWN/YES/NO` for `usesKMail` etc. and `ProfileAdvisor` (`BLOCKED/REQUIRES/ALLOWED`), but:
- No authenticated IPC between unprivileged CLI and future minimal helper - any future `Transaction` `APPLY` that needs `org.polaris.*` would currently have no `SO_PEERCRED` peer verification.
- `FileSafety::validatePath` still rejects `..;|&` etc., but no structured `Request` validation for helper IPC (oversized, NUL, traversal, shell metachars, arbitrary exec would not be bounded).
- No socket security (permissions, symlink, stale detection) - `../p4_security` already tests path traversal but not helper socket.
- No `flock` abstraction for `/run/polaris/transaction.lock` - P12 noted “duplicate-id rejection via file existence but no flock … future P14”.
- No `recover` detection for `BACKUP_CREATED/APPLYING` incomplete - P12 noted “FAILED transitions exist but no automatic recover”.

P14 value: establish **minimal, narrowly-scoped, authenticated, fail-closed IPC** so future `P15+` transactions can use a helper safely without moving business logic into privileged code and without bypassing `ReadOnlyGuard`/`StateMachine`/`TransactionValidator`.

---

## 2. Objectives (Engineering, Fail-Closed)

1. **Least privilege** - helper deliberately small, no business logic duplication, no generic `execute(command)`.
2. **Authenticated IPC** - `SO_PEERCRED` where supported, kernel-supplied `uid/pid/gid`, not client-supplied.
3. **Strict request validation** - `protocolVersion`, `requestId`, `operation` allowlist, bounded args, no NUL/shell/traversal/oversized.
4. **Bounded I/O** - `MAX_REQUEST_SIZE` 64KB, `MAX_RESPONSE_SIZE` 64KB, `MAX_ARG_COUNT` 16, `MAX_ARG_SIZE` 4096, `MAX_FIELD_SIZE` 256, timeout 5s, no unbounded allocation.
5. **Deterministic failure** - malformed/truncated/oversized/unknown → fail-closed, no indefinite blocking.
6. **No arbitrary exec** - no `sh -c`, no `exec(argv)`, no password field.
7. **Socket security** - restrictive permissions, no symlink, canonical validation, ownership, stale detection, cleanup, no world-writable.
8. **Locking** - `flock` exclusive, fail-closed if contention, bounded try, `FD_CLOEXEC`, safe cleanup.
9. **Crash/recovery** - detect incomplete `BACKUP_CREATED/APPLYING/APPLIED/VERIFYING`, preserve evidence, fail-closed, require validation/approval (no blind replay).
10. Compatibility with `P12` safety model - `IPC auth ≠ authorization ≠ approval ≠ applied`.

---

## 3. Design Constraints

- Keep `core` no Qt, offline-first, deterministic, no TCP/network dependency.
- Layers: `IpcProtocol` (pure validation) → `IpcAuth` (SO_PEERCRED) → `IpcServer`/`IpcClient` (Unix socket, thin) → `Allowlist` → `Audit` - transaction policy stays in `core/safety` (not duplicated in helper).
- Tests use `/tmp/polaris-test-root` for all sockets (`/tmp/polaris-test-root/p14/helper.sock`), locks (`/tmp/polaris-test-root/p14/transaction.lock`), transactions, backups, audit; real ` /run/polaris/helper.sock` never created.
- P14 must NOT weaken `ReadOnlyGuard`, `FileSafety`, `StateMachine`, `TransactionValidator`, `BackupEngine`, `AuditLog`, `beforeHash` etc.

---

## 4. IPC Architecture

```
Unprivileged CLI / Application (polaris_p4)
        ↓  (1) serialize Request → bounded JSON, ≤64KB
Authenticated IPC client (IpcClient)
        ↓  (2) connect() → Unix socket, poll timeout 5s
Minimal helper IPC server (IpcServer)
        ↓  (3) accept() → getsockopt(SO_PEERCRED) → uid/pid/gid from kernel
Strictly allowlisted operations (IpcProtocol::isAllowedOperation)
        ↓  (4) validate protocol/operation/args (pure)
Existing transaction/safety layer (TransactionValidator, StateMachine)
        ↓  (only if future privileged op ever enabled; in P14 all privileged ops remain disabled/rejected)
AuditLog (fsync per event)
```

- **Transport:** Unix domain socket `AF_UNIX` `SOCK_STREAM`, path `~/.local/state` not used; for P14 tests `IpcServer::testSocketPath()` → `/tmp/polaris-test-root/p14/helper.sock`, real path `IpcServer::socketPath()` → `/run/polaris/helper.sock` (never created in P14, only defined).
- **Framing:** length-prefixed or newline-delimited JSON with explicit `Content-Length`? For minimal P14, use **newline-delimited JSON**: client writes `json + "\n"` (single line, no NUL), server reads up to `MAX_REQUEST_SIZE` bytes until `\n` or EOF, with `poll` timeout 5s. Bounded: if no `\n` within `MAX_REQUEST_SIZE` or timeout → `protocol.error`.
- **Request model:**
```json
{"protocolVersion":1,"requestId":"REQ-001","operation":"ping","args":{}}
{"protocolVersion":1,"requestId":"REQ-002","operation":"info","args":{}}
{"protocolVersion":1,"requestId":"REQ-003","operation":"profile.get","args":{}}
```
  `protocolVersion` int (must be 1), `requestId` string 1..64 bounded, `operation` string 1..64 from allowlist, `args` object ≤16 entries each key ≤64 `value` ≤4096, no NUL, no traversal if key is `path`, no shell metachars in operation/args where path expected.
- **Response model:**
```json
{"protocolVersion":1,"requestId":"REQ-001","status":"ok","payload":{"message":"pong"},"error":""}
{"protocolVersion":1,"requestId":"REQ-002","status":"error","payload":{},"error":"unsupported protocol"}
```
  `status` `ok`/`error`, `payload` object bounded ≤64KB, `error` string bounded ≤1024.
- **Limits:** `MAX_REQUEST_SIZE=64*1024`, `MAX_RESPONSE_SIZE=64*1024`, `MAX_ARG_COUNT=16`, `MAX_ARG_SIZE=4096`, `MAX_FIELD_SIZE=256`, `TIMEOUT_MS=5000`, `PROTOCOL_VERSION=1`.

---

## 5. SO_PEERCRED

Linux `SO_PEERCRED` (`struct ucred {pid_t pid; uid_t uid; gid_t gid;}`) via `getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len)`:

- `IpcAuth::getPeerCred(int fd) → optional<PeerCred>` - on failure (not Linux, `getsockopt` -1, truncated) → `nullopt` → fail-closed `unavailable credentials`.
- `IpcAuth::isAuthorized(const PeerCred& cred, uid_t expectedUid)` - `cred.uid == expectedUid` (same-user authorized), also check `cred.pid>0` where useful, not trusting client-supplied `uid` field (no `uid` in Request).
- Tests: same-user `getuid()` → authorized; wrong `expectedUid = getuid()+1` → rejected; invalid fd `-1` → `nullopt` → rejected; disconnected peer (close before `getPeerCred`) → `nullopt`. All fail-closed, audit `ipc.auth.failed`.
- Platform assumption: **Linux** `SO_PEERCRED` supported on `AF_UNIX`; if not supported (e.g., macOS), `getPeerCred` returns `nullopt` and server rejects - documented, no fallback to trusting client.

---

## 6. Request Validation (Pure, IpcProtocol)

`core/ipc/IpcProtocol.h` pure validation without I/O:

- `static constexpr int PROTOCOL_VERSION=1;` `MAX_REQUEST_SIZE` etc.
- `struct Request {int protocolVersion; std::string requestId; std::string operation; std::map<std::string,std::string> args;};`
- `static ValidationResult validate(const Request& r)` and `static ValidationResult validateRaw(const std::string& rawJson)` - checks:
  - `raw.size() <= MAX_REQUEST_SIZE` else `oversized request`.
  - Contains `protocolVersion` int ==1 else `unsupported protocol`.
  - `requestId` 1..64, no NUL, no `;|&` `` ` `` `$`, UTF-8 printable.
  - `operation` 1..64, in allowlist else `unknown operation`.
  - `args` size ≤ `MAX_ARG_COUNT`, each key ≤ `MAX_FIELD_SIZE`, value ≤ `MAX_ARG_SIZE`, no NUL, if key is `path` or contains `path` then `FileSafety`-style traversal (`..`) and shell metachars rejected, otherwise still reject NUL and shell metachars in all args for safety.
  - No password field (`password` key) → `rejected`.
  - Arbitrary command: if `operation` in `{"exec","execute","run","shell","sudo","command"}` → `rejected` (not in allowlist anyway).
- Returns `ValidationResult{valid, reason, field}` for audit.
- `serialize(const Request&) → string` and `parse(const string&) → Request` - parse validates JSON structure (must contain `"protocolVersion":1, "requestId":"...", "operation":"..."`); malformed (missing braces, missing quotes, truncated) → `malformed frame`.

**Never accept:** `execute(command)` generic API - not defined, not in allowlist.

---

## 7. Operation Allowlist (Minimal, No Privileged Mutation)

**P14 allowlist (safe, read-only or echo):**

- `ping` - args `{}` → `pong` (healthcheck, no privilege)
- `info` - args `{}` → `{"version":"1","operations":["ping","info"]}` (protocol info)
- `profile.get` - optional read-only `profilePath` arg validated as safe path, returns profile JSON (read-only, no mutation) - if needed for future constraint check
- `transaction.status` - optional `transactionId` bounded, returns `state` read-only (no mutation)

**Disabled (future compatibility, rejected in P14):**
- `transaction.apply` - if received, server returns `error: privileged mutation disabled in P14` (allowlist check fails, not even reaching `TransactionStore`). Documented `NO PRIVILEGED MUTATION OPERATION IS ENABLED BY P14.`

Allowlist implemented as `static const std::set<std::string> ALLOWED = {"ping","info"}` plus maybe `profile.get`/`transaction.status` as read-only; for minimal P14 we keep only `ping` + `info` to stay least privilege, but allow read-only queries if genuinely needed for tests. For now: `ALLOWED = {"ping","info"}`.

---

## 8. Socket Security

`IpcServer` creation:

- `socket(AF_UNIX, SOCK_STREAM, 0)` + `FD_CLOEXEC`.
- Parent dir `/tmp/polaris-test-root/p14` → `mkdir -p 0700` (or `0755` but check not world-writable), verify `is_symlink(parent)==false`, `canonical(parent)` matches expected, owner `getuid()`.
- If socket file already exists: check `is_symlink==true` → reject (symlink attack), if `stat` shows it's a socket but stale (no server listening) → `unlink` only if we own and after `connect` test fails, otherwise fail-closed `stale socket`.
- `umask(0077)` before `bind` to ensure socket `0600` not world-writable; after `bind` `chmod 0600`, `listen(8)`.
- `poll` accept with timeout 5s, `accept` with `FD_CLOEXEC`, `getPeerCred` immediately, if not authorized → close, audit `ipc.auth.failed`.
- Bounded connection lifetime: server handles one request per connection, `poll` 5s for request, max one `recv` up to `MAX_REQUEST_SIZE` until `\n`, then validate, then send response ≤`MAX_RESPONSE_SIZE`, then close. No keep-alive.
- Cleanup: `unlink(socketPath)` on `shutdown`/`destructor`, no stale left.

Tests use `/tmp/polaris-test-root/p14/helper.sock`; never create `/run/polaris/helper.sock` (check `ls` before/after).

---

## 9. Locking / Concurrency

`core/safety/lock/TransactionLock.h`:

- Wraps `flock` exclusive non-blocking.
- `TransactionLock(const std::string& path = lockPath())` where `lockPath()` → `~/.local/state/polaris/transaction.lock` real, `testLockPath()` → `/tmp/polaris-test-root/p14/transaction.lock` for tests.
- `bool tryLock()` - `open(O_CREAT|O_RDWR, 0600)` + `flock(fd, LOCK_EX|LOCK_NB)` → if fails `EWOULDBLOCK` → `lock.rejected` audit, fail-closed; if success → `lock.acquire` audit, hold `fd`.
- `bool unlock()` - `flock(LOCK_UN)` + `close` + `FD_CLOEXEC` cleared, audit.
- `~TransactionLock` releases.
- No unsafe bypass: if `tryLock` fails, caller must not proceed to `apply`; `TransactionStore::apply` will check lock via `TransactionLock` before mutation (future P15, but P14 provides abstraction).
- Bounded: non-blocking, no indefinite wait; deterministic `EWOULDBLOCK` vs success.
- Safe cleanup: `unlink` not needed (lock file persists, `flock` is advisory; stale lock not an issue because `flock` releases on close/crash).
- Tests: `lock acquisition` (first succeeds), `lock contention` (second `tryLock` fails while first holds), `lock release` (after first unlock second succeeds), concurrent connections (two `IpcClient` threads both try lock, one succeeds, other `lock.rejected`).

---

## 10. Crash / Recovery Model

`core/safety/recovery/RecoveryDetector.h`:

- Scans `TransactionStore` dir (real or test) for `*.json` files, parses `state` field (or loads via `TransactionStore::get`), classifies:
  - `COMPLETED`, `ROLLED_BACK`, `CANCELLED` → `stable` (no recovery needed)
  - `BACKUP_CREATED`, `APPLYING`, `APPLIED`, `VERIFYING`, `VERIFYING→VERIFIED` incomplete → `incomplete` (needs recovery)
  - `FAILED`, `ROLLING_BACK` → `failed` (needs rollback check)
- `struct RecoveryInfo {std::string id; TxState state; std::string backupPath; bool backupExists; TxState suggested; std::string reason;}` where `suggested` is `FAILED` (fail-closed) not `COMPLETED`.
- **Fail-closed:** detection never auto-applies; `RecoveryDetector::detect(path)` returns vector of `RecoveryInfo` with `suggested=FAILED` and `reason="incomplete transaction detected - requires validation and approval, will not automatically mutate"`. Audit `recovery.detected` with `field=id` `previous=state`.
- Tests: create transaction `TX-TEST-P14-RECOVERY` with `state=BACKUP_CREATED` and backup file, run detector → `incomplete` detected; `APPLYING` also detected; `COMPLETED` not flagged; recovery does not mutate file (hash before/after same).

---

## 11. Audit Extension

Extend `AuditLog` usage (no schema change, just new `operation` strings) with fsync already:

- `ipc.connection.accepted` / `ipc.connection.rejected` / `ipc.auth.failed` / `ipc.request.accepted` / `ipc.request.rejected` / `ipc.protocol.error`
- `lock.acquire` / `lock.rejected` / `lock.release`
- `recovery.detected`

Each audit distinguishes `authenticated` (SO_PEERCRED uid matched) vs `authorized` (operation allowlist) vs `approved` (transaction approval) vs `applied` (host mutation). For P14, no `applied` via IPC (since no privileged op), but audit still logs `applied=false`.

No passwords, no arbitrary request bodies, `error` field only bounded `reason` + `field` + `expected/observed` truncated.

---

## 12. Transaction Integration (No Bypass)

- `IpcServer::handleRequest` after auth and allowlist must still call `TransactionValidator::validateForApply` and `StateMachine::isValidTransition` if future `transaction.apply` ever enabled - but in P14 `transaction.apply` is **disabled**, so `validate` will never be reached via IPC (allowlist rejects before). Documented that IPC auth is not approval.
- Tests: `test_ipc_cannot_bypass_statemachine` - create `COMPLETED` transaction, try IPC `transaction.apply` (rejected at allowlist, not via StateMachine, but even if allowlist bypassed, `TransactionStore::apply` would reject `already_completed`).
- Similar for `validateForApply`: IPC cannot make `beforeHash` stale become valid.

---

## 13. Tests (32 cases, fixtures `/tmp/polaris-test-root`)

**File `tests/unit/test_p14_ipc_protocol.cpp`:**
1 protocol version accepted (1→ok), 2 unsupported (2→error), 3 ping success, 4 malformed frame (missing `}`) rejected, 5 oversized request (>64KB) rejected, 6 truncated (no `\n`, EOF) rejected, 7 unknown operation (`unknownOp`) rejected, 8 arbitrary command (`exec`) rejected, 9 shell command (`operation="ping; rm -rf /"` + args `;|&`) rejected, 10 traversal (`args path="../../etc/passwd"`) rejected, 11 NUL (`operation="ping\0"`) rejected, 12 oversized argument (>4096) rejected

**File `tests/security/test_p14_ipc_auth.cpp`:**
13 same-user authorized (getuid()==expected→ok), 14 wrong UID (expected getuid()+1→rejected), 15 unavailable credentials (fd -1→nullopt→rejected), plus malformed/disconnected peer (closed fd → nullopt)

**File `tests/unit/test_p14_socket_security.cpp`:**
16 socket permission validation (after bind, `stat` mode 0600, not world-writable, parent 0700), 17 symlink rejection (symlink parent or socket path → throw), 18 stale socket behavior (existing socket stale → unlink only if owned, otherwise fail-closed)

**File `tests/unit/test_p14_ipc_server.cpp`:**
19 timeout/deadline (server poll 5s, client no send → timeout), 20 concurrent connections (two clients ping concurrently, both ok, bounded)

**File `tests/unit/test_p14_lock.cpp`:**
21 lock acquisition (tryLock succeeds), 22 lock contention (second tryLock fails while first holds), 23 lock release (after unlock second succeeds)

**File `tests/security/test_p14_ipc_security.cpp`:**
24 audit generated (ipc.request.accepted/rejected appears in audit.log), 25 no password logging (request with `password` field rejected, audit does not contain password value), 26 authenticated ≠ approved (SO_PEERCRED ok but `transaction.apply` still needs `approvedBeforeHash` → IPC cannot bypass approval), 27 IPC cannot bypass StateMachine (COMPLETED→APPLYING rejected via store even if IPC allowed), 28 IPC cannot bypass Validator (stale beforeHash rejected)

**File `tests/unit/test_p14_recovery.cpp`:**
29 incomplete transaction detected (BACKUP_CREATED → incomplete), 30 recovery fails closed (suggested FAILED, not COMPLETED, file hash unchanged, no auto-apply), 31 no real-host mutation (all using `/tmp/polaris-test-root`, stat `/etc/fstab` unchanged), 32 existing P1-P13 tests remain intact (run full ctest)

All tests use `/tmp/polaris-test-root/p14` for sockets/locks/transactions/backups/audit; never use `/run/polaris/helper.sock`.

---

## 14. Security Testing (Plus Existing)

Run `p4_security` (9 checks), `p12_stale` (TOCTOU), `p12_idempotency`, `p12_statemachine`, `p13_profile_*` (4) plus new P14 (6 suites). Explicitly verify still: no `sh -c`, no `exec(argv)`, no password field accepted, no `..` traversal accepted, no symlink bypass (socket and profile), no oversized input accepted, no client-supplied UID trusted, no approval bypass, no StateMachine bypass.

---

## 15. Implementation Files (Minimal)

- **New ipc:** `core/ipc/IpcProtocol.h/.cpp` (Request/Response, limits, serialize/parse, validate, allowlist `ping`/`info`
- **New auth:** `core/ipc/IpcAuth.h/.cpp` (PeerCred, getPeerCred, isAuthorized, platform Linux SO_PEERCRED)
- **New server/client:** `core/ipc/IpcServer.h/.cpp` (bind/listen/accept/auth/validate/allowlist/audit/timeout/cleanup), `core/ipc/IpcClient.h/.cpp` (connect/send/recv with timeout)
- **New lock:** `core/safety/lock/TransactionLock.h/.cpp` (flock, tryLock, unlock, FD_CLOEXEC)
- **New recovery:** `core/safety/recovery/RecoveryDetector.h/.cpp` (scan, classify, suggest FAILED, audit)
- **Extend:** `core/safety/FileSafety.h` already allows `/tmp/polaris-test-root` (no change needed for socket, but add helper sock parent check)
- **Update:** `CMakeLists.txt` add `core/ipc/*.cpp` + `core/safety/lock/*.cpp` + `core/safety/recovery/*.cpp` to `polaris_core`, add 6 P14 test executables
- **No change:** `TransactionValidator`, `StateMachine`, `BackupEngine`, `ReadOnlyGuard` - preserve invariants

---

## 16. Validation (Before COMPLETE)

- Clean `cmake -S . -B /tmp/polaris_p14_build --fresh && cmake --build && ctest` - must be 23/23 (existing 17 + 6 P14) 100%
- Verify `p4_security` + `p12_*` + `p13_*` still pass
- Verify no `sudo`, `systemctl`, `dnf`, `dracut`, `reboot` in P14 tests (grep)
- Verify `ls /run/polaris/helper.sock` not exists before/after, `ls /tmp/polaris-test-root/p14` contains fixtures only
- Verify `stat /etc/fstab` mtime unchanged, `systemctl is-enabled mssql-server` `disabled`
- Verify `grep -r "sh -c" core/` 0, `grep -r "execute(" core/ipc/` 0
- Verify docs: `P14_PLAN.md`, `P14_IMPLEMENTATION_REPORT.md`, `ARCHITECTURE.md` IPC layer, `ROADMAP.md` P14 COMPLETE

---

## 17. Documentation to Update

- `docs/P14_PLAN.md` (this)
- `docs/P14_IMPLEMENTATION_REPORT.md` (post-impl: architecture, protocol, SO_PEERCRED, boundaries, allowlist, socket, lock, recovery, audit, tests, build, host verification, limitations, next P15)
- `docs/ARCHITECTURE.md` - add IPC/Security layer
- `docs/ROADMAP.md` - P14 COMPLETE, next P15
- `docs/PROJECT_STATE.json` - currentPhase P14, next P15, completedPhases + P14, tests 23/23
- `docs/PROJECT_HANDOFF.md` - P14 section, host state still no reboot, candidate Akonadi still REJECTED

---

## 18. Safety Boundaries Summary (What Is NOT Implemented)

- **No privileged mutation** via IPC in P14 - `transaction.apply` would be disabled/rejected even if allowlist contained it; helper remains minimal.
- No `run command` generic API; no `password` field; no `sudo`/`polkit` against real host.
- No real helper socket installed; only fixture path.
- Lock is advisory `flock` (not mandatory); recovery is detection-only, not auto-replay.
- Documented limitation: `SO_PEERCRED` is Linux-specific `AF_UNIX` (fails closed on other platforms).

---

## 19. Not In Scope (P15 onward)

- No Test/CI expansion beyond P14 (P15), no Campaign2 (P17), no Final Benchmark (P18) - those remain future.

