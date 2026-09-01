# Polaris CLI — Real Workflow

**Primary interface:** CLI is first-class; Qt GUI will share the same core engine (not duplicate logic).  
**Safety:** `READ → MEASURE → ANALYZE → EXPLAIN → RECOMMEND → PREVIEW → APPROVAL → BACKUP → APPLY → VERIFY → COMPARE → REGRESSION → AUDIT` — if any step cannot be proven safe, fails closed.

## Build

```bash
cmake -S . -B build --fresh -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j$(nproc)
ctest --test-dir build --output-on-failure  # 38/38 100%
```

Binaries: `polaris` (mock), `polaris_real` (real scan), `polaris_p3` (baseline), `polaris_p4` (primary), `polaris_p5` (pilot).

No `sudo` inside Polaris, no `SUDO_ASKPASS`, no password.

## Command Classes

| Class | Commands | Writes | Requires Approval | Can Mutate Host |
|-------|----------|--------|-------------------|-----------------|
| **READ-ONLY** | `polaris_real --json`, `polaris_p3`, `polaris_p4 recommendations`, `polaris_p4 capabilities list`, `polaris_p4 transaction list|show|compare`, `polaris_p4 audit list`, `polaris_p4 profile show`, `polaris_p4 explain`, `polaris_p4 transaction explain`, `polaris_p4 apply --dry-run` | No | No | No |
| **Creates transaction** | `polaris_p4 transaction preview <op>` | Creates `TX-TEST-*` under `/tmp/polaris-test-root/transactions` (fixture), audit `transaction.previewed` | No (creates `PREVIEWED`) | No |
| **Requires explicit approval** | `polaris_p4 transaction approve <id>` | Binds `approvedBeforeHash`/`approvedTarget` (`TransactionValidator`), audit `transaction.approved` | **Yes** — this *is* the approval | No (records `APPROVED`, not `APPLIED`) |
| **Capable of host mutation** | `polaris_p4 profile set` (writes `~/.local/state/polaris/profile.json` `0600`), future `transaction apply` via helper `org.polaris.*` `SO_PEERCRED` `flock` | Yes, but **only** via `APPROVAL→VALIDATION→BACKUP→FINAL→APPLY` and only if helper allowlist includes operation | Yes, explicit `approve` required | **Currently none privileged:** helper allowlist `ping`/`info` only (`IpcProtocol`), `flatpak-unused` `journal-vacuum` stop at `PREVIEWED` on real host (fixture file target) |

## Realistic Example Session

```bash
# 1. Discover — always read-only, no sudo
./build/polaris_real --json | python3 -m json.tool | head -n 60
./build/polaris_p3 --json | python3 -m json.tool
./build/polaris_p4 capabilities list --json | python3 -m json.tool
# → [{"id":"flatpak-unused","risk":"R1"}, {"id":"journal-vacuum","risk":"R1"}]

./build/polaris_p4 recommendations --json | python3 -m json.tool
# → [{"id":"REC-journal-vacuum","benefit":"0.6GB","confidence":0.75}, ...] (if journal >1GB)

# 2. Explain — read-only, why now / what will(not) change / rejection
./build/polaris_p4 profile show --json
./build/polaris_p4 explain flatpak-unused --json | python3 -m json.tool
# WHY NOW: flatpak reclaimable 0MB (this host hasFlatpak false) → NOT APPLICABLE
# WHAT WILL NOT CHANGE: NVIDIA remains claimed, zram remains 8G ...

# Tell Polaris about your workflow (not auth)
./build/polaris_p4 profile set usesKMail yes --json
./build/polaris_p4 explain akonadi-disable --json | grep BLOCKED
# → BLOCKED_BY_USER_WORKFLOW usesKMail=yes → Akonadi will remain enabled...

# 3. Preview — safe, fixture, no real /run/polaris
# Create fixture to prove measured benefit (1.5GB):
mkdir -p /tmp/polaris-test-root/p19
cat > /tmp/polaris-test-root/p19/flatpak.list <<'EOF'
Application Branch Origin InstalledSize
org.freedesktop.Platform 23.08 flathub 900 MB
org.gnome.Platform 50 flathub 600 MB
EOF
cat > /tmp/polaris-test-root/p19/flatpak.unused <<'EOF'
org.freedesktop.Platform 23.08
org.gnome.Platform 50
EOF

./build/polaris_p4 transaction preview flatpak-unused
# → {"transactionId":"TX-TEST-123","capability":"flatpak-unused","state":"PREVIEWED",
#    "expectedBenefit":"1.5 GB disk reclaimed (unused runtimes 2)","reclaimableMB":1500,"confidence":0.85,
#    "preconditions":{"flatpak.reclaimableBytes":"1572864000","flatpak.stateHash":"42ba..."}}
# No writes, test fixture only, audit transaction.previewed

./build/polaris_p4 transaction list
./build/polaris_p4 transaction show TX-TEST-123 --json | python3 -m json.tool | head -n 30

# 4. Approve — explicit, hash-bound, not launch==approval
./build/polaris_p4 transaction approve TX-TEST-123
# → {"transactionId":"TX-TEST-123","approval":"APPROVED","state":"APPROVED"}
# Audit transaction.approved, approvedBeforeHash bound to 42ba...

./build/polaris_p4 transaction explain TX-TEST-123 --verbose
# WHY NOW: Transaction TX-TEST-123 flatpak-unused R1 1.5GB
# WHAT WILL CHANGE: target=/tmp/.../flatpak-unused.state operation=flatpak uninstall --unused
# WHAT WILL NOT CHANGE: NVIDIA 470xx remains claimed ...
# REJECTION CONDITIONS: stale flatpak.stateHash, unavailable evidence, <500MB, already completed ...
# ROLLBACK: flatpak install <runtime>

./build/polaris_p4 audit list | head
# hash-chained, fsync per event, previousHash→eventHash, no password/secret

# 5. Apply — currently fixture-locked, no privileged real-host apply
./build/polaris_p4 apply --dry-run flatpak-unused
# → Dry-run MUST NOT write files, invoke privileged ops, or request password - verified
# Real apply would be: TransactionStore::apply → VALIDATION → BACKUP (SHA-256) → FINAL VALIDATION → APPLY (atomicWrite)
# but helper allowlist ping/info only → journal-vacuum would stay PREVIEWED on real host

# 6. Verify — after hypothetical apply + reboot
./build/polaris_p4 transaction compare TX-P7-NVIDIA-470xx-20260831 --json | python3 -m json.tool
# comparison: boot 54.106→8.515 -84% not regression, memory 4.2→5.6G, storage.free 50G→51G +1GB SUCCESS, verdict SUCCESS

# 7. Check no host mutation during discovery
stat /etc/fstab | grep Modify  # 2026-08-31 21:19:15.195818022 +0330
systemctl is-enabled mssql-server  # disabled
ls /run/polaris/helper.sock  # No such file
ls ~/.local/state/polaris/profile.json  # No such file (unless profile set)
```

**Verify safety:** all above except `profile set`/`approve` are `ReadOnlyGuard` `kReadOnlyMode true` `openReadOnly` only, `test_readonly` verifies `stat /etc/fstab` mtime unchanged. No batch, one change at a time.

See `README.md` for why Polaris exists and `docs/ARCHITECTURE.md` for layers.
