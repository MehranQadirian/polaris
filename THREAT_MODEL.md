# Threat Model - Polaris

STRIDE: Spoofing (Polkit session spoof → D-Bus peer creds), Tampering (API JSON tamper → schema validation), Repudiation (audit append-only), Information Disclosure (UDS 0600, no telemetry), Denial of Service (job timeout, rate limit), Elevation (helper allowlist).

Test against: command injection, path traversal, privilege escalation, malformed API, replay old tx, malicious paths/pkg args, races.

No telemetry by default, offline-first, opt-in only.
