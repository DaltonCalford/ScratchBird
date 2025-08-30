### Implementation plan (tailored)

This plan defines the REQ-* taxonomy, structure, parallel tracks, and AI vs developer ownership to build comprehensive documentation with traceability.

Sections:

- REQ taxonomy
- Directory structure
- Automation
- Parallel tracks and ownership
- Step-by-step execution

## REQ taxonomy

See `traceability/spec/requirements.md` for full list and sources.

Prefixes:

- REQ-CORE-HEAP-*, REQ-CORE-SPACE-*, REQ-TXN-MGA-*, REQ-CATALOG-BOOT-*, REQ-EXEC-ENGINE-*, REQ-OPT-STAT-*, REQ-INTEGRITY-TRIG-*, REQ-PSQL-RUNTIME-*, REQ-INDEX-FAMILIES-*, REQ-FDW-SPI-DBLINK-*, REQ-SERVER-YVALVE-*, REQ-BACKUP-PITR-*, REQ-REPL-LOGICAL-*, REQ-TABLESPACES-*, REQ-ADMIN-MAINT-*, REQ-SEC-RLS-*, REQ-JSON-SPATIAL-COLL-*, REQ-PARTITION-MV-*, REQ-TOOLING-UX-*, REQ-QA-PERF-*, REQ-PACKAGING-DOCS-*, REQ-IMPL-SCAN-*.

## Structure

Key hubs:

- `project/compliance/coverage-dashboard.md`, `project/compliance/gaps-and-recommendations.md`
- `traceability/spec/requirements.md`, `traceability/mappings/spec_map.yaml`
- `api/index.md`, `api/modules/*.md`
- `subprojects/*/index.md` + `subprojects/*/compliance.md`

## Automation

- ctags-based `code_anchors.json`
- Generator to inject “Implementation References” into pages
- Spec extractor to materialize `requirements.md` from `ProjectPlan/*`
- Coverage report generation to compliance dashboards

## Parallel tracks and ownership

- Storage, Executor, Index families, Language reference, Config/Errors: AI drafts → Dev review
- Transactions, Integrity/Triggers, Optimizer constants, Server/Auth/TLS, WAL/Recovery, PSQL semantics: Dev primary → AI assists

## Steps

1) Scaffold docs and templates
2) Generate requirements index and initialize mappings
3) Extract code anchors and API stubs
4) Seed core docs with Spec Trace and Implementation References
5) Author area docs in parallel; run link/anchor checks
6) Developer review on complex areas; update gaps/recs
7) Add CI for drift checks and link validity
