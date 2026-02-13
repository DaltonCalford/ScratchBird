# Findings: TRIGGER_CONTEXT_VARIABLES.md

Spec file: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/triggers/TRIGGER_CONTEXT_VARIABLES.md`

## Authoritativeness
- The file is labeled "Non-Authoritative Reference" and is not listed in `docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.

## Summary
There is no parser, AST, or executor support for the trigger context variable system described in this spec (e.g., `GET TRIGGER_EVENT`, `TRIGGER_USER`, `CHANGED_COLUMNS`, `GET COLUMN_VALUE`, `TRIGGER_SHARED_DATA`, etc.). The executor has minimal trigger contexts for OLD/NEW row access and statement transition tables, but no SQL-level variable syntax is implemented. As written, this spec is not implemented.

## Implemented (Partial / Foundational)
- Trigger execution exists with row-level OLD/NEW access and statement-level OLD/NEW transition tables in the executor.
  - Evidence: `src/sblr/executor.cpp` TriggerContext and StatementTriggerContext definitions.
- Trigger creation parsing exists (CREATE TRIGGER, events, timing, granularity) but does not include any trigger context variable syntax.
  - Evidence: `src/parser/parser_v3.cpp` `parseCreateTrigger()`.

## Gaps / Discrepancies
1. **Trigger context variables not parsed**
- No parser support for `GET TRIGGER_EVENT`, `TRIGGER_EVENT`, `TRIGGER_USER`, `TRIGGER_TIMESTAMP`, `GET CHANGED_COLUMNS`, `IS COLUMN CHANGED()`, etc.
  - Evidence: no references in parser files.

2. **No AST/SBLR representation for context variables**
- There are no AST nodes or SBLR opcodes for trigger context variables or trigger-introspection functions.

3. **No executor binding for SQL-level trigger variables**
- Executor only supports OLD/NEW row access through TriggerContext methods, not via SQL variables or functions.

4. **Advanced features missing**
- Transition table variables (`OLD_TABLE` / `NEW_TABLE`) are not exposed via SQL.
- Shared data coordination (`TRIGGER_SHARED_DATA`, `TRIGGER_SKIP_REMAINING`, `TRIGGER_CANCEL`, `TRIGGER_OPERATION`) not implemented.
- Database-level/DDL triggers and corresponding variables are not exposed at SQL level.

## Notes
- The lexer only recognizes trigger keywords for DDL, not trigger context variable identifiers.
- The trigger execution framework exists but would require a dedicated variable/function binding layer to expose these semantics.

## Suggested Next Steps
- If these variables are required, define the authoritative spec and add parser/AST/SBLR opcodes or built-in functions to expose trigger context.
- Otherwise, keep this document as non-authoritative guidance and remove it from conformance checks.
