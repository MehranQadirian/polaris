# Polkit - Polaris P4

## Policy Design

Narrowly scoped, operation-specific, principle of least privilege.

### Actions (conceptual, for P4 infrastructure, not yet invoking real optimizations)

```
org.polaris.modify.fstab
  description: Modify filesystem table (stale swap)
  message: Authentication is required to modify filesystem configuration
  defaults: allow_any no, allow_inactive no, allow_active auth_admin_keep

org.polaris.service.manage
  description: Manage systemd service (enable/disable)
  message: Authentication is required to manage system services
  defaults: auth_admin_keep

org.polaris.package.manage
  description: Manage packages via DNF
  defaults: auth_admin

org.polaris.driver.manage
  description: Manage NVIDIA driver
  defaults: auth_admin

org.polaris.kernel.manage
  description: Manage kernel parameters / GRUB
  defaults: auth_admin
```

**Why `auth_admin_keep`:** Session keep 5min (Polkit handles caching) → “authenticate once per optimization session” UX, not per-command, but still explicit per transaction. High-risk driver uses `auth_admin` (no keep) for stronger confirmation.

**Why not `auth_self`:** System modifications affect all users, require admin, not just self.

**Why not `yes`:** Never allow without auth for R2/R3.

## Helper Architecture

```
Polaris client (unprivileged, user) --D-Bus/UDS--> Polaris privileged helper (root, minimal)
                                    Polkit check
                                         |
                                    allowlisted op only
```

- Client never runs as root, never `sudo polaris`, never collects password.
- Helper exposes **only** discrete methods (see `core/safety/FileSafety.h` allowlist, `BackupEngine`, `HelperInterface`): `FileModify`, `SystemdEnable`, `DnfPreview`, etc. - never `run_arbitrary_command`.
- Helper validates: fixed exe paths, structured args, allowlist targets, reject traversal, symlink, NUL, metachars, bounded sizes/time.
- Polkit agent is native KDE `polkit-kde-authentication-agent-1` (already running `3032`), not app text field.

## P4 Status

Policies exist as files `polkit/org.polaris.*.policy` infrastructure, **but no real optimization operation invokes them in P4**. All P4 tests use `TX-TEST` fixtures under `/tmp/polaris-test-root` which require no auth. Real host `/etc/fstab` would require `org.polaris.modify.fstab` in P5+.

## IPC

Prefer D-Bus `org.polaris.Helper` with `peer credential verification` (UDS `SO_PEERCRED`), or UDS `/run/polaris/helper.sock` 0600 with validation. No TCP `0.0.0.0`.

## Trust Boundary

Client (untrusted) | Helper (trusted, minimal) | Polkit (OS). Helper trusts only Polkit grant, not client claim.
