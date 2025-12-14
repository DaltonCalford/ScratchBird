# TODO: Logging, Audit, Telemetry Exposure

Goal: Define query/audit logging scope, telemetry export (metrics), and retention controls.

Requirements:
- Query logging: what to log (text/plan/errors), sampling/filters, redaction to avoid sensitive data.
- Audit logging: auth events (existing) plus DDL/DCL; optional DML audit hooks; RBAC-controlled access.
- Telemetry: metrics endpoints/formats (e.g., Prometheus-style), what metrics to expose (align with Beta monitoring todo), and access control.
- Retention/rotation: log file rotation, retention limits, and config knobs.
- Performance impact controls: enable/disable, sampling rates, per-component toggles.

Work Items:
- Spec logging/audit fields and redaction rules.
- Define metrics surface and authentication/authorization for metrics.
- Configuration options for enabling/sampling/retention.
- Tests: logging enabled/disabled, redaction, metrics access control.
