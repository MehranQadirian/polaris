# P14 - Expanded Security & IPC / Helper Architecture - Implementation Report

**Phase:** P14 - Engineering/Security, Not Host Optimization  
**Date:** 2026-09-01 05:30 +0330  
**Source:** Repository `~/Documents/lin-opt` verified via `cmake`, `ctest`, `ls`, `cat`, `stat`, `systemctl`, `audit.log` - not conversation memory  
**Status:** **COMPLETE** - minimal, narrowly-scoped, authenticated, fail-closed IPC with no privileged mutation enabled, plus flock lock and fail-closed recovery detection, all on fixtures `/tmp/polaris-test-root`, no real-host mutation

---

## 1. Implementation Summary

Established a minimal security boundary between unprivileged Polaris CLI/Application and any future privileged helper. Helper is deliberately small: it enforces **authentication** (`SO_PEERCRED` kernel creds) and **strict request validation** (bounded, no shell/traversal/NUL), but **does not duplicate business logic** (`TransactionValidator`/`StateMachine` remain in `core/safety`) and **does not expose privileged mutation** in P14. Flow is:

```
CLI → IpcClient → serialize Request (≤64KB) + "\n"
      → connect() Unix socket (poll 5s) → IpcServer accept()
      → getsockopt(SO_PEERCRED) → uid/pid/gid from kernel (not client field)
      → isAuthorized(uid==getuid()) → fail-closed if wrong/unavailable/spoofed
      → validateRaw() (size, NUL, protocolVersion, operation allowlist, args count/size, shell/traversal, password) → fail-closed
      → allowlist (ping/info only) → handle → serialize Response + "\n" → client recv (poll 5s)
      → AuditLog fsync (previousHash chain) with authenticated vs authorized vs approved vs applied distinction
```

Separate `TransactionLock` (`flock` exclusive non-blocking) and `RecoveryDetector` (scan `BACKUP_CREATED/APPLYING` → suggest `FAILED`, never auto-apply) address prior limitations without touching `/run/polaris` on real host.

---

## 2. Files Changed / Added

**Modified:**
- `core/safety/FileSafety.h:18` - already extended for `~/.local/state/polaris/profile.json` (P13), no change needed for IPC socket (socket security uses `FileSafety::isSymlink`/`canonical` directly)
- `CMakeLists.txt:8` - add `core/ipc/IpcProtocol.cpp`, `IpcAuth.cpp`, `IpcServer.cpp`, `IpcClient.cpp`, `core/safety/lock/TransactionLock.cpp`, `core/safety/recovery/RecoveryDetector.cpp` to `polaris_core`; add 7 P14 test executables
- `docs/ARCHITECTURE.md:1` - add IPC/Security layer
- `docs/ROADMAP.md:27` - P14 COMPLETE line
- `core/ipc/IpcServer.cpp:47` - fix `FileSafety::isSymlink` include, validate socket path shell metachars, audit TX-TEST prefix for test log

**Added:**
- `core/ipc/IpcProtocol.h:1` (85L) - `Request`/`Response`/`ValidationResult`, `PROTOCOL_VERSION=1`, `MAX_REQUEST_SIZE=64KB`, `MAX_RESPONSE_SIZE=64KB`, `MAX_ARG_COUNT=16`, `MAX_ARG_SIZE=4096`, `MAX_FIELD_SIZE=256`, `TIMEOUT_MS=5000`, `allowedOperations()={"ping","info"}`, `validate()`/`validateRaw()` (NUL, protocol, requestId/operation size/shell/traversal/allowlist, args count/size/shell/traversal/password/control-char, fail-closed), `serialize`/`parse` (minimal deterministic JSON, throws on malformed/truncated), `serializeResponse`/`parseResponse`, helpers `containsNul`/`containsTraversal`/`containsShellMetachars`
- `core/ipc/IpcProtocol.cpp:1` (346L) - pure validation, deterministic serialize/parse, `validateRaw` size/NUL/malformed handling, oversized/truncated/unknown/arbitrary/shell/traversal/NUL/oversized-arg checks
- `core/ipc/IpcAuth.h:1` (35L) - `PeerCred` (`pid,uid,gid`), `getPeerCred(int fd)` via `getsockopt(SO_PEERCRED)` Linux, `isAuthorized(cred, expectedUid)` (`uid==expected && pid>0`), `containsSpoofedCred(args)` (keys `uid/pid/gid/peer_uid` → spoof), `currentUid()`
- `core/ipc/IpcAuth.cpp:1` (40L) - `getPeerCred` returns `nullopt` on `fd<0` or `getsockopt` failure (unavailable → fail-closed), Linux `SO_PEERCRED` only, documented
- `core/ipc/IpcServer.h:1` (45L) - `defaultSocketPath()` `/run/polaris/helper.sock` (defined but never created in P14), `testSocketPath()` `/tmp/polaris-test-root/p14/helper.sock`, `validateSocketPath` (NUL, traversal, shell metachars, `>200`, allowlist `/tmp/polaris-test-root/` or `/run/polaris/`, symlink), `checkParentSecurity` (exists, not symlink, not world-writable `S_IWOTH`, owned by `getuid()`), `isStaleSocket` (connect `ECONNREFUSED` → stale), `start()` (validate, `mkdir 0700`, stale unlink if owned, `socket` `FD_CLOEXEC`, `umask 0077` `bind` `chmod 0600` `listen(8)`), `stop()` `close`+`unlink` only if not symlink, `handleRequest(raw, peerCred)` (auth → unavailable→error, `isAuthorized`→error, `containsSpoofedCred`→error, `validateRaw`→error, allowlist `ping`/`info` → `ok` else `error`), `handleNextConnection(timeoutMs)` (poll accept, getPeerCred, poll recv with timeout, handle, send bounded response + `\n`, close)
- `core/ipc/IpcServer.cpp:1` (339L) - socket security, auth, validation, allowlist, audit `ipc.*` with `TX-TEST-IPC-` prefix for test log, timeout 5s, no unbounded allocation
- `core/ipc/IpcClient.h:1` (25L) - `testSocketPath`, `send(Request)` serialize+sendRaw, `sendRaw(string)` (check size, `socket` `FD_CLOEXEC`, non-blocking `connect` + `poll` `SO_ERROR`, `send`, `poll` `recv`, size checks, parseResponse)
- `core/ipc/IpcClient.cpp:1` (70L) - poll timeout, `::send`/`::recv` to avoid shadowing, `MAX_REQUEST_SIZE` check
- `core/safety/lock/TransactionLock.h:1` (35L) - `defaultLockPath()` `/run/polaris/transaction.lock` (never used in tests), `testLockPath()` `/tmp/polaris-test-root/p14/transaction.lock`, `tryLock()` (`open O_CREAT|O_RDWR 0600` `FD_CLOEXEC` `flock LOCK_EX|LOCK_NB` → `lock.rejected` on `EWOULDBLOCK`, `chmod 0600`, `lock.acquire`), `unlock()` `flock LOCK_UN`+`close`, `isLocked`, audit `lock.acquire`/`rejected`/`release`, parent symlink/world-writable/ownership checks
- `core/safety/lock/TransactionLock.cpp:1` (70L) - `flock` exclusive, fail-closed, `FD_CLOEXEC`, safe cleanup, not world-writable
- `core/safety/recovery/RecoveryDetector.h:1` (35L) - `RecoveryInfo` (`id`, `state`, `backupPath`, `backupExists`, `suggested=FAILED`, `reason`), `detect(storePath)` scans `*.json` for `state` in `BACKUP_CREATED/APPLYING/APPLIED/VERIFYING/AUTHORIZED` → incomplete, `suggested FAILED` (never `COMPLETED`), `isIncomplete`, `shouldFailClosed` always true, `defaultStorePath`/`testStorePath`, audit `recovery.detected`
- `core/safety/recovery/RecoveryDetector.cpp:1` (80L) - parse `id`/`state` via simple search, check `BackupEngine::testBackupRoot`/`backupRoot` for `backupExists`, suggest `FAILED` with `requires validation and approval, will not automatically mutate`, audit
- `tests/unit/test_p14_ipc_protocol.cpp` (90L) - 12 cats: protocol accepted, unsupported rejected, ping, malformed, oversized, truncated, unknown op, arbitrary exec, shell, traversal, NUL, oversized arg
- `tests/security/test_p14_ipc_auth.cpp` (110L) - 5 cats + integration: same-user authorized, wrong UID rejected, unavailable (`fd -1`, closed), spoofed uid field rejected, disconnected peer (nullopt), socketpair integration smoke
- `tests/unit/test_p14_socket_security.cpp` (100L) - 5 cats: permission 0600 not world-writable + cleanup, symlink rejection, traversal/shell in path rejected, stale socket detection (`ECONNREFUSED`), parent symlink rejection
- `tests/unit/test_p14_ipc_server.cpp` (100L) - 4 cats: ping via client/server, info via client/server, timeout (client no send → server poll timeout within 200ms), concurrent (4 parallel pings)
- `tests/unit/test_p14_lock.cpp` (100L) - 5 cats: acquisition, contention (second fails while first holds), release, symlink rejection, concurrent (2 threads, at least 1 success, no deadlock)
- `tests/security/test_p14_ipc_security.cpp` (314L) - 12 cats: audit generated (ipc.request.accepted/rejected in test audit.log), no password logging (secret123 not in audit), authenticated≠approved (ping ok but transaction still PENDING), cannot bypass StateMachine (COMPLETED→APPLYING rejected at allowlist + store `already_completed`), cannot bypass validator (stale beforeHash rejected), no sh -c, no arbitrary exec, no password collection, no traversal, no symlink bypass (atomicWrite symlink), no oversized, no privilege assumption (spoof uid rejected)
- `tests/unit/test_p14_recovery.cpp` (100L) - 4 cats: incomplete BACKUP_CREATED/APPLYING detected, COMPLETED not flagged, recovery fails closed (suggested FAILED, file hash unchanged, backup preserved, no auto-apply), no real-host mutation (stat /etc/fstab unchanged), smoke existing tests intact
- `docs/P14_PLAN.md` (19K)
- `docs/P14_IMPLEMENTATION_REPORT.md` (this)

No modification to: `Real*Provider`, `BaselineEngine`, `ComparisonEngine`, `Transaction`/`StateMachine`/`TransactionValidator` (preserve P12), `UserProfile`/`ProfileStore` (preserve P13, `usesKMail=yes` still blocks Akonadi), `akonadi`, `mssql`, `nvidia`, `fstab`, `zram`.

---

## 3. IPC Protocol

**Version:** `PROTOCOL_VERSION=1` (int, must be 1; `validateRaw` rejects 2 with `unsupported protocol`).

**Request JSON (deterministic, compact, newline-terminated for framing):**
```json
{"protocolVersion":1,"requestId":"REQ-001","operation":"ping","args":{}}
{"protocolVersion":1,"requestId":"REQ-002","operation":"info","args":{}}
```
`requestId` 1..64, no NUL/shell/traversal, `operation` 1..64 from allowlist, `args` object ≤16 entries, each key ≤64, value ≤4096, no NUL/shell/traversal/control-char, no `password` key, `args` total ≤64KB.

**Response JSON:**
```json
{"protocolVersion":1,"requestId":"REQ-001","status":"ok","payload":{"message":"pong"},"error":""}
{"protocolVersion":1,"requestId":"REQ-002","status":"error","payload":{},"error":"unknown operation"}
```
`status` `ok`/`error`, `payload` bounded ≤64KB, `error` ≤1024.

**Limits (explicit, bounded, enforced before allocation):**
- `MAX_REQUEST_SIZE=65536`, `MAX_RESPONSE_SIZE=65536`
- `MAX_ARG_COUNT=16`, `MAX_ARG_SIZE=4096`, `MAX_FIELD_SIZE=256`
- `TIMEOUT_MS=5000` (poll for connect, accept, recv, send)
- No unbounded allocation based on peer input: `validateRaw` checks `raw.size()` first, then parse only if ≤MAX, `recv` buffer fixed 8192, `args` count checked.

**Framing:** Client `serialize(req) + "\n"` → server `recv` up to 8191 bytes → if `raw.size()>MAX` → `oversized` error; if no complete `{"protocolVersion"...}` with braces → `malformed frame` error; truncated (missing `}` or missing fields) → `malformed` → fail-closed `ipc.protocol.error`.

---

## 4. SO_PEERCRED Behavior

**Linux `SO_PEERCRED`:** `getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len)` returns `struct ucred {pid_t pid; uid_t uid; gid_t gid;}` from kernel, not client.

**Implementation `IpcAuth::getPeerCred`:**
- `fd<0` → `nullopt`
- `getsockopt` fails or `len != sizeof(ucred)` → `nullopt` (unavailable → fail-closed)
- On non-Linux `#ifndef SO_PEERCRED` → `nullopt` (fail-closed, documented)

**`isAuthorized`:**
- `cred.uid == expectedUid` (`getuid()`) && `cred.pid>0` → `true` else `false`
- Does **not** trust `args["uid"]` - `containsSpoofedCred` checks `args` keys `uid/pid/gid/peer_uid` → if present → `spoofed credentials rejected` (even if kernel cred is valid)

**Tests:**
- `socketpair` same-user `getuid()` → `isAuthorized` true
- `expectedUid = getuid()+1` → false (wrong UID rejected)
- `fd=-1` / closed fd → `nullopt` (unavailable → `ipc.auth.failed` `unavailable credentials`)
- Spoofed field `{"uid":"0"}` → `containsSpoofedCred` true → server returns `error: spoofed credentials rejected`
- Disconnected peer (`nullopt` cred) → `handleRequest` returns `error: unavailable credentials` (tested via `handleRequest(raw, nullopt)`)

**Platform assumption:** Linux `AF_UNIX` `SO_PEERCRED` supported on Fedora 44 (verified `getsockopt` success in tests); if unavailable, server fails closed (no fallback to trusting client).

---

## 5. Security Boundaries

- **No shell:** `IpcProtocol::containsShellMetachars` rejects `;|&` `` ` `` `$` in `operation`/`requestId`/`args` values; `grep -r "sh -c" core/ipc` 0, no `popen`/`system`/`execvp` with client input.
- **No arbitrary exec:** No `execute(command)` API; `allowedOperations` only `ping`/`info`; `validate` rejects `exec`/`execute`/`run`/`shell`/`sudo`/`command` as `unknown operation`; tests `test_arbitrary_command_rejected` pass.
- **No password handling:** `validate` rejects any `args` key containing `password` (case-insensitive) or exact `password`/`passwd`/`secret`; `IpcServer` never logs `args` values beyond bounded `reason` (audit `error` contains `reason` not `secret123` → test `no password logging` asserts `secret123` not in `audit.log`).
- **No traversal:** `containsTraversal` (`..`) rejected in `operation`/`args` and `validateSocketPath`; `FileSafety` still protects real paths.
- **No symlink bypass:** `validateSocketPath` checks `isSymlink(path)` before bind; `checkParentSecurity` checks parent not symlink; `IpcServer::start` checks after `mkdir`; `TransactionLock` checks `is_symlink(lockPath)` and parent; tests `test_socket_symlink_rejection`/`test_parent_symlink_rejection` pass.
- **No oversized:** `validateRaw` `raw.size()>64KB` → `oversized`, `validate` `args` count>16 or value>4096 → rejected; `IpcClient::send` also checks `raw.size()>MAX` before send.
- **No privilege assumption:** `IpcAuth` never reads `args["uid"]`; only kernel `ucred` trusted; spoof attempt → `spoofed credentials rejected`.
- **No approval bypass:** `IpcProtocol` allowlist does not contain `transaction.apply`; even if it did, `TransactionStore::apply` would still require `approvedBeforeHash` and `StateMachine` → tested `test_ipc_cannot_bypass_*`.
- **No StateMachine bypass:** `IpcServer` does not call `TransactionStore` directly for privileged ops (none enabled); tests verify `COMPLETED` transaction via IPC still rejected.

---

## 6. Operation Allowlist

**P14 allowlist (minimal, safe, least privilege):**

- `ping` → `{"message":"pong"}` (healthcheck, no privilege, no args)
- `info` → `{"version":"1","operations":"ping,info"}` (protocol info)

**NO PRIVILEGED MUTATION OPERATION IS ENABLED BY P14.** Documented in code `IpcProtocol::allowedOperations` only `ping`/`info`; `IpcServer::handleRequest` switch only those two, else `unknown operation` error. Future `transaction.apply` would be disabled (`if(operation=="transaction.apply") return error: privileged mutation disabled in P14` - but currently not even in allowlist, so first check fails). Tests `test_unknown_operation_rejected` and `test_ipc_cannot_bypass_*` verify.

---

## 7. Socket Security

- **Path:** Real `IpcServer::defaultSocketPath()` `/run/polaris/helper.sock` defined but **never created** in P14 (tests use `testSocketPath()` `/tmp/polaris-test-root/p14/helper.sock`). Verified `ls /run/polaris/helper.sock` not exists before/after `ctest`.
- **Permissions:** Parent dir `0700` (`mkdir` + `chmod`), socket `0600` (`umask 0077` before `bind` + `chmod 0600` after), not world-writable (`S_IWOTH` check in `checkParentSecurity` and after bind `stat` check in test).
- **Parent handling:** `validateAndPrepare` checks `parent` exists → not symlink, not world-writable, owned by `getuid()`; if not exists → `create_directories` `0700` then re-check not symlink.
- **No symlink following:** `validateSocketPath` `isSymlink(path)` → reject; `checkParentSecurity` `isSymlink(parent)` → reject; `start` also checks after `mkdir`.
- **Canonical validation:** `FileSafety::isSymlink` + `stat` ownership used (not full `canonical` for socket, but parent `stat` ownership covers).
- **Stale detection:** `isStaleSocket` `stat` `S_ISSOCK` → try `connect` → `ECONNREFUSED` → stale → `unlink` only if owned; else `socket already exists and not stale` → fail-closed. Tested `test_stale_socket_behavior`.
- **Bounded lifetime:** `handleNextConnection` `poll` 5s for accept, `poll` 5s for recv, one request per connection, then `close`; no keep-alive.
- **Cleanup:** `stop()` `close(listenFd_)` + `unlink(socketPath)` only if not symlink, audit `server stopped`.

---

## 8. Locking

`core/safety/lock/TransactionLock`:

- **Exclusive:** `open(O_CREAT|O_RDWR, 0600)` + `flock(LOCK_EX|LOCK_NB)` → if `EWOULDBLOCK` → `lock.rejected` fail-closed.
- **Fail closed if contention:** Second `tryLock` while first holds → `false` (tested `test_lock_contention`).
- **No unsafe bypass:** If parent symlink/world-writable or lock path symlink → `lock.rejected`; no `LOCK_SH` bypass.
- **Deterministic:** Non-blocking, no wait, `EWOULDBLOCK` vs success deterministic.
- **Bounded:** Immediate return, no indefinite block.
- **Safe cleanup:** `unlock()` `flock(LOCK_UN)` + `close` + `FD_CLOEXEC` (set on `open`), destructor unlocks; lock file persists (advisory, not stale), `unlink` not needed.
- **No inheritance:** `FD_CLOEXEC` set.

**Fixture path:** `TransactionLock::testLockPath()` `/tmp/polaris-test-root/p14/transaction.lock` for tests; real `defaultLockPath()` `/run/polaris/transaction.lock` never touched (verified `ls /run/polaris/transaction.lock` not exists).

**Tests:** `test_lock_acquisition` (first succeeds), `test_lock_contention` (second fails, after first unlock second succeeds), `test_lock_release` (destructor releases), `test_lock_symlink_rejection`, `test_concurrent_lock` (2 threads contention at least 1 success, no deadlock).

---

## 9. Crash / Recovery

`core/safety/recovery/RecoveryDetector`:

- Scans `storePath` (`/tmp/polaris-test-root/transactions` test, real `~/.local/state/polaris/transactions` default) for `*.json`, extracts `id`/`state` via simple search (not full JSON parse, fail-closed if missing).
- `isIncomplete(s)` → `true` for `BACKUP_CREATED`, `APPLYING`, `APPLIED`, `VERIFYING`, `AUTHORIZED` (states that could be left after crash during `BACKUP_CREATED→APPLYING`).
- `COMPLETED`, `ROLLED_BACK`, `CANCELLED`, `FAILED`, `PREVIEWED`, `APPROVED`, `PROPOSED` → not incomplete.

**Fail-closed:** `detect()` returns `vector<RecoveryInfo>` with `suggested=FAILED` (never `COMPLETED`), `reason="incomplete transaction detected in state X - requires validation and approval, will not automatically mutate"`, `backupExists` checked via `BackupEngine::testBackupRoot`/`backupRoot` existence, audit `recovery.detected`. Never auto-replays `APPLYING` (even if backup exists).

**Tests:** `test_incomplete_detected` (BACKUP_CREATED and APPLYING flagged, COMPLETED not), `test_recovery_fails_closed` (suggested FAILED, file hash unchanged, backup preserved, no auto-apply), `test_no_real_host_mutation` (stat `/etc/fstab` unchanged before/after detect), `test_existing_tests_intact_smoke` (store still works after detector).

**Limitation:** Detection only, not automatic `polaris transaction recover` execution (which would require re-validation and approval). Documented.

---

## 10. Audit

Extended `AuditLog` usage (existing `fsync` per event, `previousHash` chain preserved). New `operation` values:

- `ipc.connection.accepted` (server started, peer authorized)
- `ipc.connection.rejected` (bind/listen/accept failure, symlink, stale, cleanup)
- `ipc.auth.failed` (unavailable credentials, wrong UID `peer 1001 expected 1000`, spoofed cred in args)
- `ipc.request.accepted` (`ping`/`info`)
- `ipc.request.rejected` (unknown operation, oversized, etc.)
- `ipc.protocol.error` (malformed, truncated, NUL, oversized raw, timeout, poll error)
- `lock.acquire` / `lock.rejected` / `lock.release`
- `recovery.detected` (incomplete `id` in state `BACKUP_CREATED`)

**Safe logging:** `error` field contains bounded `reason` (≤1024, no raw body), `field`, `expected/observed` truncated, never `password`/`secret` value, never arbitrary request body, never sensitive env. Tests `test_audit_generated` checks `audit.log` contains `ipc.request.accepted`/`rejected`, `test_no_password_logging` asserts `secret123` not in audit.

**Distinction:** Audit records `authenticated` (SO_PEERCRED `uid` matched) vs `authorized` (operation allowlist `ping`/`info`) vs `approved` (transaction `approvedBeforeHash` - still required, not implied by IPC) vs `applied` (host mutation via `FileSafety::atomicWrite` - not performed via IPC in P14, so `applied=false` for all IPC ops).

---

## 11. Transaction Integration (No Bypass)

- `IpcServer::handleRequest` does **not** call `TransactionValidator`/`StateMachine`/`TransactionStore` directly for privileged ops (none enabled). It only handles allowlisted `ping`/`info`.
- Even if future privileged op were allowed, it must still pass `TransactionStore::apply` which enforces `validateForApply` (beforeHash/unitHash/kernel/package/precondition/TOCTOU), `StateMachine` (`BACKUP_CREATED→APPLYING`), exact `approved*` binding, `BackupEngine`, `finalValidation`.
- Tests `test_authenticated_not_approved` (ping ok via IPC but transaction still `PENDING` → `validateForApply` fails), `test_ipc_cannot_bypass_statemachine` (COMPLETED `transaction.apply` via IPC rejected at allowlist, direct `TransactionStore::apply` also rejects `already_completed`), `test_ipc_cannot_bypass_validator` (stale beforeHash via store still fails even though ping via IPC succeeds).

---

## 12. Tests and Exact Results

**New tests (P14, fixtures `/tmp/polaris-test-root/p14` only):**

- `test_p14_ipc_protocol` - 12 cats: protocol accepted, unsupported rejected, ping, malformed, oversized, truncated, unknown op, arbitrary exec, shell, traversal, NUL, oversized arg
- `test_p14_ipc_auth` - 5 cats: same-user authorized, wrong UID rejected, unavailable (`fd -1`, closed), spoofed uid field rejected, disconnected peer (nullopt)
- `test_p14_socket_security` - 5 cats: permission 0600 not world-writable, symlink rejection, traversal/shell in path rejected, stale socket detection (`ECONNREFUSED`), parent symlink rejection
- `test_p14_ipc_server` - 4 cats: ping via client/server, info via client/server, timeout (client no send → server poll 200ms timeout), concurrent (4 parallel pings)
- `test_p14_lock` - 5 cats: acquisition, contention (second fails, after unlock second succeeds), release on destruction, symlink rejection, concurrent (2 threads)
- `test_p14_ipc_security` - 12 cats: audit generated, no password logging, authenticated≠approved, cannot bypass StateMachine, cannot bypass validator, no sh -c, no arbitrary exec, no password collection, no traversal, no symlink bypass, no oversized, no privilege assumption (spoof uid)
- `test_p14_recovery` - 4 cats: incomplete BACKUP_CREATED/APPLYING detected, COMPLETED not flagged, recovery fails closed (suggested FAILED, file hash unchanged, backup preserved), no real-host mutation, smoke existing store

**Total:** `ctest 24/24 0.57s 100%`

```
1/24 unit                      Passed 0.00s
2/24 real_providers            Passed 0.04s
3/24 parsers                   Passed 0.00s
4/24 readonly                  Passed 0.00s
5/24 p4_security               Passed 0.01s
6/24 comparison                Passed 0.00s
7/24 post_change               Passed 0.00s
8/24 regression                Passed 0.00s
9/24 observed_benefit          Passed 0.00s
10/24 p12_stale                Passed 0.01s
11/24 p12_idempotency          Passed 0.01s
12/24 p12_statemachine         Passed 0.00s
13/24 p12_transaction_model    Passed 0.01s
14/24 p13_profile_model        Passed 0.01s
15/24 p13_profile_store        Passed 0.00s
16/24 p13_profile_service      Passed 0.01s
17/24 p13_profile_advisor      Passed 0.00s
18/24 p14_ipc_protocol         Passed 0.00s
19/24 p14_ipc_auth             Passed 0.01s
20/24 p14_socket_security      Passed 0.00s
21/24 p14_ipc_server           Passed 0.36s
22/24 p14_lock                 Passed 0.07s
23/24 p14_ipc_security         Passed 0.01s
24/24 p14_recovery             Passed 0.01s
```

Existing `p4_security` 9/9, `p12_*` 4 suites, `p13_*` 4 suites still pass (17/17 → 24/24).

---

## 13. Build Result

```
cmake -S . -B /tmp/polaris_p14_build --fresh → Configuring done, Generating done
cmake --build /tmp/polaris_p14_build → 100% Built polaris, polaris_real, polaris_tests, test_real_providers, test_parsers, test_readonly, polaris_p3, polaris_p4, polaris_p5, test_comparison, test_post_change, test_regression, test_observed_benefit, test_p12_stale, test_p12_idempotency, test_p12_statemachine, test_p12_transaction_model, test_p13_profile_model, test_p13_profile_store, test_p13_profile_service, test_p13_profile_advisor, test_p14_ipc_protocol, test_p14_ipc_auth, test_p14_socket_security, test_p14_ipc_server, test_p14_lock, test_p14_ipc_security, test_p14_recovery
ctest → 24/24 100% 0.57s
```

No `-Werror` warnings beyond deprecated `SHA256_*` (already `Wno-error=deprecated-declarations`). No `sh -c` in `core/ipc`.

---

## 14. Real-Host Modification Verification

P14 engineering, no host optimization - verified:

- `stat /etc/fstab` `Modify: 2026-08-31 21:19:15` unchanged before/after `ctest`
- `ls /run/polaris/helper.sock` `No such file` before and after (real socket never created; tests use `/tmp/polaris-test-root/p14/helper.sock` which is cleaned after each test)
- `ls /run/polaris/transaction.lock` `No such file` (real lock never created; tests use `/tmp/polaris-test-root/p14/transaction.lock`)
- `ls /etc` `ls /usr` mtime unchanged (no `/etc`/`/usr` writes)
- `systemctl is-enabled mssql-server` `disabled` (P6) unchanged; `systemctl is-enabled bluetooth` `enabled` unchanged
- `akonadictl status` `running` (not disabled)
- `ls /lib/modules/*/extra/nvidia-470xx/nvidia.ko.xz` 25M unchanged
- `stat ~/.local/state/polaris/profile.json` `No such file` before/after (tests use `/tmp/polaris-test-root` fixtures only)
- `grep -r "dnf " tests/unit/test_p14*` 0, `grep -r "systemctl " tests/` only read-only `is-enabled` in P6 history, no `dracut`/`modprobe`/`reboot`/`sudo`/`polkit` in P14 tests
- `find /tmp/polaris-test-root -type f | wc -l` shows only fixtures (`p14/*`, `transactions`, `audit.log`, `profile.json` test copies)

---

## 15. Known Limitations / What Is NOT Implemented

- **No privileged mutation via IPC:** `transaction.apply` and any future `dnf`/`systemctl` mutation remain disabled/rejected in P14 (allowlist only `ping`/`info`). Documented `NO PRIVILEGED MUTATION OPERATION IS ENABLED BY P14.`
- **No generic exec:** No `execute(command)` API, no `sh -c`, no arbitrary `argv`/`executable` path.
- **No password field:** `password` key always rejected, never logged.
- **No real helper service installed:** `IpcServer` is tested via in-process `socketpair`/`Unix socket` fixtures, not via `systemd` service, not via `polkit` against real host.
- **Lock is advisory `flock`:** Not mandatory, not persisted across reboot, `flock` releases on close/crash (correct), but not tested with `systemd` `Requires` or `PID` file.
- **Recovery is detection-only:** `RecoveryDetector` suggests `FAILED`, preserves evidence, never auto-mutates; no `polaris transaction recover` command that re-validates and requires approval (future P14+).
- **SO_PEERCRED Linux-specific:** On non-Linux `SO_PEERCRED` unavailable → `getPeerCred` returns `nullopt` → fail-closed `unavailable credentials`; no fallback to trust client-supplied UID.
- **No network/TLS:** `AF_UNIX` only, no TCP, no `D-Bus` `SO_PEERCRED` already used where supported, not `sd-bus` `org.freedesktop.systemd1`.
- **No Qt/network deps:** `core` still no Qt, offline-first deterministic.

All limitations are fail-closed and documented.

---

## 16. Next Phase

**P15 - Test/CI/Fixture Expansion** (15+ tests: `transaction state`, `rollback`, `crash/recovery`, `stale-preview`, `concurrent`, `idempotency`, `regression`, `fixture-based real-provider` with isolated fixtures). P14 unblocks helper trust boundary needed for P15's `crash/recovery` and `concurrent` tests. Do NOT implement P15 now - STOP after P14 engineering.

---

## 17. Verification Commands

```
rm -rf /tmp/polaris_p14_build && cmake -S ~/Documents/lin-opt -B /tmp/polaris_p14_build --fresh && cmake --build /tmp/polaris_p14_build && ctest --test-dir /tmp/polaris_p14_build --output-on-failure
stat /etc/fstab | grep Modify  # 2026-08-31 21:19 unchanged
ls /run/polaris/helper.sock  # No such file
ls /run/polaris/transaction.lock  # No such file
systemctl is-enabled mssql-server  # disabled
ls -R /tmp/polaris-test-root/p14 | head
grep -r "sh -c" core/ipc  # 0
grep -r "password" core/ipc  # only validation rejection, not collection
```

---

*No real-host optimization was performed during P14. No reboot occurred. No unrelated project area was modified beyond IPC/lock/recovery.*

