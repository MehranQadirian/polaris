# Audit - Polaris P4

## Model

Append-only, tamper-evident, structured.

```
AuditEvent {
  timestamp, transactionId, operation, user (UID, not password),
  approval, authorizationResult, backupPath, changes, verification,
  rollback, error, previousHash, eventHash (SHA256(previousHash+timestamp+...))
}
```

Stored: `~/.local/state/polaris/audit.log` (or `/tmp/polaris-test-root/audit.log` for TX-TEST) - each line JSON, `eventHash = SHA256(timestamp+transactionId+operation+user+approval+previousHash)`, `previousHash` is prior `eventHash` → hash chain. Accidental/tampering detection via chain break.

## Operations Audited

`transaction.created`, `previewed`, `approved`, `authorization.requested/granted/denied`, `backup.created`, `started`, `completed`, `verification.passed/failed`, `rollback.started/completed`, plus `rejected`, `invalid_transition`, `validation_failed`.

## Security Events

Do not treat `authorization denied` as crash - audit as `authorization.denied` with error, no retry loop.

## What Not Logged

Never passwords, secrets, tokens, private keys - only hashes, operation, result.

## CLI

```
polaris audit list [--transaction <id>]
polaris audit show <eventHash>
polaris transaction show <id> --audit
```

For P4, `polaris_p4 audit list` shows test events.

## Implementation

`core/safety/audit/AuditLog.h` `hashEvent()` via `openssl/sha.h` SHA256, `append()` creates directory, reads last `eventHash` as `previousHash`, writes JSON line with `previousHash`+`eventHash`. `list()` parses.

## Trust

Client cannot bypass audit - helper appends directly, not client. Even if GUI compromised, helper still audits.
