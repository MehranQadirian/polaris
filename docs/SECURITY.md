
## P4 Update (2026-08-31)

- Transaction state machine `StateMachine.h` enforces explicit `validateTransition()` - `PROPOSED->APPLYING` rejected, fail closed, tested.
- FileSafety allowlist `/tmp/polaris-test-root` only for P4 mutations, `validatePath` rejects `..`, `;|&`, `NUL`, `>4096`, symlink, non-allowlist - tested 9 security tests PASS.
- BackupEngine `BackupEngine.cpp` versioned, SHA-256, no overwrite, fail closed if backup fails.
- AuditLog `AuditLog.cpp` hash chaining `previousHash` + `eventHash` SHA256, append-only, not logging secrets.
- No helper installed in P4 - infrastructure only, policies exist but not invoking real ops.
