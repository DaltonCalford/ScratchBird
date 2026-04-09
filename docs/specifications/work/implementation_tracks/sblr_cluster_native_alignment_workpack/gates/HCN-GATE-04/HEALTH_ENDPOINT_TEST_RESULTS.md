# Health Endpoint Test Results

Validated by:
- `HealthReadinessContractTest.HealthAndReadinessReflectStateTransitions`
- `HealthReadinessContractTest.ExportsComponentRowsForSqlHealthView`
- `HealthReadinessContractTest.MetricsEndpointRoutesHealthAndReadinessPaths`

Observed:
- `/healthz` returns `OK` when liveness is satisfied.
- `/readyz` returns `NOT_READY` until all readiness dependencies are satisfied.
- endpoint router dispatches `/healthz` and `/readyz` to health/readiness payloads.
