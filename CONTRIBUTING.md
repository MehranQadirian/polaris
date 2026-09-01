# Contributing - Polaris

## Workflow
OBSERVE → HEALTH CHECK → BASELINE → PLAN → VALIDATE → BACKUP → APPROVE → APPLY → VERIFY

Never `CHANGE → HOPE`.

## Safety
- No password storage
- No `run_arbitrary_command`
- Allowlist privileged helper only
- Polkit `auth_admin_keep`
- Dry-run first

## Testing
Use mocks: `FakeSystemProvider` for i5-10210U/MX130 simulation without touching host.
`cmake --build build && ctest` - must pass.

## Docs
Update `ARCHITECTURE.md` + `API.md` for any domain change.
