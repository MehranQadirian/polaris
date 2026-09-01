
## P4 Threats Mitigated (Tested)

- Path traversal, symlink, command injection, NUL, oversized, invalid transition, replay, backup overwrite, fake operation, audit tamper - all `test_p4_security` PASS fail closed.
- Trust boundary: Client (untrusted) | Helper (minimal, allowlist) | Polkit (OS). Client claim not trusted.

