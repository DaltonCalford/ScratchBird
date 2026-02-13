# Operations & Monitoring Specifications

Status: Authoritative (V3)
Last Updated: 2026-02-08

This directory contains operational monitoring and observability specifications
for ScratchBird. Only files listed in
`docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are normative.

## Specifications in this Directory

- `PROMETHEUS_METRICS_REFERENCE.md` - Required Prometheus metrics
- `LISTENER_POOL_METRICS.md` - Listener and parser pool metrics
- `MONITORING_SQL_VIEWS.md` - SQL-visible monitoring views (sessions/locks/statements/perf)
- `MONITORING_DIALECT_MAPPINGS.md` - Column-level mapping to pg_stat_*/MON$/performance_schema
- `OID_MAPPING_STRATEGY.md` - PostgreSQL OID mapping strategy

## Related Specifications

- `docs/specifications/parser/v3/network/` (listener/pool and protocols)
- `docs/specifications/parser/v3/transaction/` (MGA/locks/visibility)
- `docs/specifications/parser/v3/storage/` (I/O and page counters)
