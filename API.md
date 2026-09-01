# API - Polaris `/api/v1` (Versioned, Local-Only)

Transport: Unix Domain Socket `/run/polaris/polaris.sock` (0600) by default. Optional `http://127.0.0.1:11447` if enabled (token, localhost only). Never public.

All requests/responses JSON, schema versioned `application/json; version=1`.

## Sync Read-Only (no auth)
```
GET /api/v1/system          -> SystemInfo
GET /api/v1/hardware        -> HardwareInfo
GET /api/v1/health          -> { score: int 0-100, issues: HealthIssue[] }
GET /api/v1/performance     -> { baseline, metrics: PerformanceMetric[] }
GET /api/v1/cpu
GET /api/v1/memory
GET /api/v1/storage         -> StorageDevice[]
GET /api/v1/gpu             -> GpuInfo[]
GET /api/v1/drivers         -> DriverInfo[]
GET /api/v1/services        -> { enabled, active, failed: ServiceInfo[] }
GET /api/v1/processes?sort=cpu|mem
GET /api/v1/boot            -> BootAnalysis
GET /api/v1/recommendations -> Optimization[]
GET /api/v1/baseline
```

## Async Jobs (202)
```
POST /api/v1/scan                -> { jobId }
POST /api/v1/diagnostics         -> { jobId }
POST /api/v1/benchmark {level:quick|normal|deep} -> { jobId }
POST /api/v1/plan                -> { jobId }
GET  /api/v1/jobs/{id}          -> { state, progress:0-100, events: JobEvent[] }
GET  /api/v1/jobs/{id}/events  SSE -> event: JobEvent { type: STARTED|PROGRESS|WARNING|ERROR|COMPLETED|FAILED }
```

## Transactions (approval + Polkit)
```
POST /api/v1/transactions { optimizationIds, dryRun } -> { transactionId, plan, risk, requiresApproval, requiresAuth, requiresReboot }
GET  /api/v1/transactions/{id} -> Transaction
POST /api/v1/transactions/{id}/approve -> 200 { approved: true }
POST /api/v1/transactions/{id}/apply   -> { jobId } // triggers Polkit if risk>=2
POST /api/v1/transactions/{id}/rollback -> { jobId }
GET  /api/v1/audit?since=&txId=
```

## Error
```json
{ "error": { "code":"POLKIT_DENIED", "message":"Authorization denied", "details":{ "action":"org.polaris.modify.fstab" }}}
```

## Models
See `core/domain/*.h`. Example HealthIssue:
```json
{ "id":"GPU-001", "category":"GPU", "severity":"HIGH", "title":"NVIDIA driver incompatible",
  "evidence":["lshw UNCLAIMED","nvidia-smi failed","journal NVRM not supported","modinfo gsp_tu10x"],
  "confidence":0.96, "impact":"GPU acceleration unavailable", "recommendation":"Install 470xx",
  "risk":3, "requiresReboot":true, "rollback":true }
```

## CLI Mapping
`polaris scan --json` = `POST /scan` + poll + `GET /jobs/{id}`.
