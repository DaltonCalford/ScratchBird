# sys.metrics.runtime Results

Validated by `SqlObservabilityViewBuilderTest.BuildsRuntimeAndHealthRowsWithDeterministicOrdering`.

Observed:
- runtime rows include canonical metric metadata (`metric_name`, `metric_type`, `value`, `labels_json`, `updated_at`)
- deterministic ordering is enforced across repeated builds
- label JSON contains expected key/value bindings (e.g., `db`, `result`)
