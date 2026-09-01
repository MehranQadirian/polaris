# Security - Polaris

## Principles
SAFETY over SPEED, NATIVE OS AUTH over PASSWORD STORAGE, REVERSIBILITY over IRREVERSIBILITY. Never store passwords, never disable SELinux/firewalld, never run whole app as root.

## Authentication
Polkit + D-Bus. `auth_admin_keep` → session 5min (OS-managed). No app password field, no storage, no env var, no CLI --password, no logging secrets. UX: [Authorize] → native Polkit agent → session reused for further approved ops in same tx.

## Privileged Helper
`polaris-privileged-helper` (D-Bus activated, root, allowlist). Exposes only discrete methods: `BackupFile`, `RestoreBackup`, `SystemdEnable/DisableUnit` (regex + allowlist), `DnfPreview/Apply`, `WriteFstabEntry` (validated), `ApplySysctl` (allowlist), `CreateModprobeConfig`, `RebuildInitramfs`. Never `run_arbitrary_command`. Args canonicalized, fixed paths, timeout, audit.

## Hardening (planned)
`NoNewPrivileges=yes`, `PrivateTmp=yes`, `ProtectSystem=strict`, `ProtectHome=read-only`, `CapabilityBoundingSet` minimal, `SystemCallFilter=@system-service`, `RestrictAddressFamilies=AF_UNIX`, `SystemCallArchitectures=native`.

## API Security
UDS 0600, localhost HTTP optional token, strict JSON schema, no network expose by default.

## Audit
Append-only SQLite `audit` table: ts, txId, op, risk, authState, result, exit, rollback. Never passwords.

## Threats
See `THREAT_MODEL.md` - injection, traversal, privilege escalation, malformed JSON, replay mitigated by allowlist, validation, timeout, nonce.

## Persistence
SQLite `/var/lib/polaris/polaris.db` for scans, baselines, tx, backups metadata. No passwords/tokens/keys.
