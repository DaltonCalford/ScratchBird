# Parser Freeze Checklist (Phase 12)

This checklist summarizes acceptance scope and gates for declaring the SQL parser frozen for implementation.

## Scope completeness
- Core DDL: table, index (incl. methods/partial), sequence, domain, view, collation, charset, exception
- PSQL: EXECUTE BLOCK, routines (proc/func), triggers, packages
- DML: insert/update/delete/merge, returning/for update, with expressions/operators
- SELECT: CTE, joins, group/having, window, set-ops, plan, order/fetch, for update
- Session/Txn: create/alter/drop database, connect/disconnect, set names/role/dialect, set transaction/commit/rollback/savepoint
- Lifecycle surfaces: comment on, alter ... rename to, grant/revoke (incl. new objects)
- Extensions: database link (table@link), foreign data (server/mapping/table/import), tablespaces, backup/restore
- Admin: trace profile, audit policy; schedules/jobs; background tasks; replication (pub/sub)
- Cluster: cluster/node/service/auth provider
- Modern: row-level security (policy), materialized view
- Quality-of-life: smart terminators, leading doc comments

## Diagnostics and recovery
- Diagnostics style consistent with specs/engine/diagnostics.yaml
- Warning spans attached where sensible (Ast.warning_spans)
- Clause-level recovery guards present for DDL/PSQL; never crash at End-of-input

## Specs and tests
- Each surface has a spec entry in specs/ with grammar, acceptance, diagnostics
- MANIFEST includes all specs
- Unit tests exist for each family; full suite green under RelWithDebInfo and sanitizers (CI)
- Fuzz smoke passes; no parser assertions

## Frozen deliverables
- Headers/APIs stable for parser entry points and AST payloads
- Normalizers/formatters stable where provided (e.g., select/plan pretty-print)
- No further grammar additions without RFC in specs

## Post-freeze handoff
- Open items marked under future_engine_tasks per spec
- Implementation phases: resolver/type-check, engine DDL, optimizer integration, providers, admin runtimes
