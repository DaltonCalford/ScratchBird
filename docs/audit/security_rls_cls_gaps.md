# Security: Row/Column-Level Controls Audit & Gaps

## Current
- Catalog has tables for policies and column permissions (`policies_table_page_`, column_permissions) and object permissions; RLS is notionally present but needs verification of runtime enforcement.
- Column-level GRANT/REVOKE exists; no built-in “mask/obfuscate” per-row/column policy observed.

## Requested/Gap
- Fine-grained row-level security: enforce per-row filters per role/user (verify existing implementation and tests).
- Column-level masking/obfuscation by row: column is present in result set but value is masked for rows that do not satisfy policy (e.g., employee sees direct reports but not managers).
- Need policy definition model: predicate-based masking or function-based redaction.
- Dependency on session/user/role context and potentially hierarchical org relationships.

## Prior Art
- PostgreSQL: RLS with USING/WITH CHECK; no native column masking (some extensions use views).
- SQL Server: RLS (predicate) and Dynamic Data Masking (column masking).
- Oracle: VPD (row predicates) + data redaction.

## Feasibility Notes
- Row filters: feasible if existing RLS hooks are wired in planner/executor; need to confirm and extend.
- Column masking: requires planner/executor rewrite to substitute expressions for protected columns on rows failing predicate; must integrate with type system and avoid leaks via functions/sorts/indexes/logs.
- Performance: predicate and masking expression must be pushed down efficiently; consider caching policies per session.

## Work Items
- [ ] Audit existing RLS enforcement path (policy catalogs, executor integration, tests).  
- [ ] Design column masking policy syntax and catalog (per column, with predicate and mask expression).  
- [ ] Implement planner/executor rewrite for masked columns with row predicates; ensure no side-channel leaks (logs, error messages).  
- [ ] Extend GRANT/REVOKE to include mask-aware privileges if needed, or keep separate policy.  
- [ ] Tests: row filters, masked columns, mixed visibility; confirm emulated dialects unaffected.  
- [ ] Docs: document policy syntax, limitations, and performance considerations.  
