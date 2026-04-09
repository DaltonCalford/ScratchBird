# Section 16: Context Variables

Status: current_authority

## Current shipped authority

The current implementation proves three distinct surfaces:
- ConnectionContext session and identity state
- executor-visible SHOW state
- executor-local row and trigger context

This section owns the boundary between those surfaces. It does not claim a universal typed context-variable registry.

## Current implementation baseline

Current proved connection-context state includes:
- current user identity
- session user identity
- superuser state
- current schema
- search path
- dialect tag
- generic session-variable set, get, clear, clear-all, and list behavior

Current proved SHOW exposure includes bounded runtime names such as:
- search_path
- current_schema and schema aliases
- dialect_tag
- sql_dialect
- charset
- statement_timeout
- autocommit
- time_zone
- transaction_isolation
- server_version
- operator.strict_mode
- generic session-variable fallback when the name is present in session state

Current proved row-context boundary includes:
- row and trigger context exist inside executor paths
- missing row context fails closed
- public ROW.NEW or ROW.OLD variable syntax is not currently claimed as section-wide authority

## Explicit unsupported or unclaimed surfaces

This section does not currently claim:
- one typed engine-wide SYSTEM, SESSION, TRANSACTION, STATEMENT, and ROW variable registry
- one namespace-id table
- one catalog-backed alias registry
- one public row-variable namespace contract
- one typed transaction-variable inventory beyond bounded SHOW exposure

## Direct audit lookup anchors

- `src/core/connection_context.cpp` search key `ConnectionContext::setSessionVariable(`
- `src/core/connection_context.cpp` search key `ConnectionContext::getSessionVariable(`
- `include/scratchbird/core/connection_context.h` search key `set_statement_timeout(`

## File Index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [CONTEXT_VARIABLES_NORMATIVE_IMPLEMENTATION.md](CONTEXT_VARIABLES_NORMATIVE_IMPLEMENTATION.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->

## Maintenance
- Update file list with `../skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`.
