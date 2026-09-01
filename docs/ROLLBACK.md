# Rollback - Polaris P4

## Architecture

Every mutation declares rollback **before** it can execute. Transaction refuses to run if `rollbackAvailable==false` and `risk>=R2` for reversible operation.

## Backup

Versioned, transaction-associated, not overwriting:

```
~/.local/state/polaris/backups/<transactionId>/<filename>.bak   # real
/tmp/polaris-test-root/backups/<TX-TEST>/...                  # P4 test
```

Each `Backup` includes: transactionId, originalPath, backupPath, timestamp, SHA-256, size, permissions (0644), owner, group. Created via `BackupEngine::create()` with `FileSafety::validatePath()` allowlist, `is_regular_file` check, `copy` + `sha256File()`. If backup exists → throw, refuse to overwrite. If backup fails → **do not apply change** (fail closed).

## Rollback

CLI:
```
polaris transaction list
polaris transaction show <id>
polaris transaction rollback <id>
```

Rollback is itself: authenticated (Polkit same action), validated (current state still matches beforeState? If changed since preview → STOP, require new preview), audited, verified.

Steps: `ROLLING_BACK` → read backup → `FileSafety::atomicWrite` (temp + fsync + rename) → `verify` via same read-only providers → `ROLLED_BACK` or `FAILED`.

Never blindly restore without checking current state (TOCTOU protection via `canonical` and `stat` before/after).

## P4 Test Fixtures

All P4 rollback tests operate exclusively on `/tmp/polaris-test-root/etc/fstab` dummy, never real `/etc/fstab`. Test `test_p4_security` verifies `BackupEngine` no overwrite, symlink attack block.

## Crash Recovery

State persisted `~/.local/state/polaris/transactions/<id>.json`. After restart `polaris transaction recover` detects `BACKUP_CREATED`, `APPLYING`, `VERIFYING` incomplete → requires re-validation, never auto-continue, fail closed if inconsistent.

## Reboot Handling

`rebootRequired=true` shown, no auto reboot. User reboots separately.
