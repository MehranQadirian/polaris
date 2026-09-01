
## P4 Extensions (Infrastructure, No Real Mutations Yet)

```
GET  /api/v1/transactions
GET  /api/v1/transactions/{id}
POST /api/v1/transactions/preview { operationId, target } -> ChangePreview
POST /api/v1/transactions/{id}/approve
POST /api/v1/transactions/{id}/execute  # blocked in P4 for real host, only TX-TEST
POST /api/v1/transactions/{id}/rollback
GET  /api/v1/transactions/{id}/audit
GET  /api/v1/transactions/{id}/diff
```

All require explicit approval + Polkit for R2/R3 in future P5+.

## P11 Extensions (Post-Change Measurement)

```
GET  /api/v1/transactions/{id}  # now includes beforeBaseline, afterBaseline, comparison, expectedBenefit, observedBenefit, regression, verdict
POST /api/v1/transactions/{id}/compare  # explicit compare if needed
GET  /api/v1/performance/baseline
GET  /api/v1/performance/baseline/{id}
POST /api/v1/performance/benchmark
GET  /api/v1/performance/bottlenecks
GET  /api/v1/recommendations
```

Prefer structured JSON from `domain::Comparison` rather than ad-hoc strings. No new standalone binary.
