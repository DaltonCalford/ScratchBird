# Dependency Lifecycle Audit (Create/Alter/Drop Integrity)

## Context
- Session scope: added exception catalog support, unified lookup, and stored-code dependency recording/cleanup; all tests passing (`ctest --output-on-failure` from `build/`).
- Goal: full life-cycle dependency integrity for all SQL objects: creation must verify referenced objects exist; drops must be blocked when dependents exist (unless explicit cascade design is added later).

## Current Behavior (snapshot)
- **Catalog primitives:** Dependencies table exists; CRUD used by stored-code paths. Exceptions now first-class (`ObjectType::EXCEPTION`), with catalog root persistence and unified lookup.
- **Stored code (function/procedure/package/UDR):**
  - CREATE: QueryCompilerV2 + SemanticAnalyzerV2 collect dependencies (tables/schemas/domains/functions/procedures/UDRs/packages, sequences by name literal) and executor links them when creating stored code.
  - DROP: Stored-code drop clears dependency rows for the dropped object.
  - ALTER: Not implemented (no dependency refresh).
  - Exceptions: catalog support exists, but no dependency links to/from exceptions.
- **DDL tables/views/indexes/sequences/domains/constraints/triggers/etc.:**
  - No uniform dependency recording on CREATE/ALTER (some legacy view/trigger handling existed, but not comprehensive).
  - No dependency verification on CREATE (missing-object block).
  - No drop-time protection: drops of referenced objects are not uniformly blocked; cascade not enforced.
- **Parsers/compilers:**
  - PostgreSQL parser emits opcodes for CREATE FUNCTION/PROCEDURE in new format; other DDL parsers unchanged.
  - Dependency lookup helper added to QueryCompilerV2 to resolve names via unified catalog lookup; not yet used by all paths (e.g., view/trigger definitions).

## Required Work (per object type)
- **Schema:** Record dependencies? (usually roots). Enforce drop-block if child objects exist.
- **Table:** On CREATE verify referenced schemas/types/sequences; record dependency on schema/domain/sequence/default expressions; on DROP block if views/triggers/fks/functions/procs/packages depend.
- **View:** On CREATE collect referenced tables/views/functions/columns/sequences; block if missing; record dependencies; on ALTER (recreate) refresh deps; on DROP block if depended on.
- **Index:** On CREATE verify table/columns exist; record dependency on table and any expression dependencies; refresh on ALTER/REINDEX; block drop if referenced by constraints.
- **Sequence:** On CREATE verify schema; record; on DROP block if referenced (defaults/nextval/currval) or cascade policy.
- **Constraint (PK/UK/CK/FK):** Record dependencies on table, columns, referenced table for FK; block drop of referenced objects; refresh on ALTER TABLE actions.
- **Trigger:** Record dependencies on table, referenced functions/procs; block drop of table/function; refresh on ALTER.
- **Domain/Type:** Record dependencies on underlying types/defaults/collations; block drop if used by columns/domains/functions.
- **Function/Procedure/Package/UDR:** Refresh dependencies on CREATE/ALTER; block drop of referenced objects; block drop of routine when invoked? (not needed). Ensure exceptions referenced are validated.
- **Exception:** Record dependencies when referenced; block drop if referenced by routines/constraints.
- **Privileges/Policies/Comments/etc.:** If modeled as dependencies, ensure drops respect them; otherwise exclude.
- **Emulation objects:** Decide scope (likely skip for now unless used by runtime).

## Implementation Plan (next steps)
1. **Audit existing dependency hooks** in `CatalogManager` and executor: list current calls to `createDependency`, `dropDependenciesForObject`, and any special-case blockers; map to object types.
2. **Add ObjectType coverage** where missing (e.g., exception already added; validate enums used in dependencies table and ensure serialization handles new value).
3. **Create/Alter paths:**
   - For each DDL opcode, collect referenced objects (using SemanticAnalyzerV2/QueryCompilerV2 where possible; add walkers for views/triggers/constraints).
   - Validate existence before executing (return error if missing).
   - Write dependency rows post-creation; refresh on ALTER (drop + reinsert).
4. **Drop paths:**
   - Implement `hasDependents(object_id)` queries using `object_to_dependencies_` map; block drop with clear error.
   - Wire into executor drop handlers for all object types (table/view/index/sequence/domain/constraint/trigger/function/procedure/package/udr/exception).
5. **Tests:**
   - Unit: dependency recording/validation per object type.
   - Integration: drop blocked when dependent exists; create fails on missing reference.
6. **Docs/Tracking:** Update `alpha3_gap_todo.md` and roadmap once each object type is covered.

## Notes
- No cascade semantics implemented; assume block-on-dependents until explicit cascade design is approved.
- Sequence detection currently literal-name based; may need parser-side UUID capture for defaults/nextval usage to tighten validation.
