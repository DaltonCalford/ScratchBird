# Findings: triggers/README.md

Spec file: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/triggers/README.md`

## Authoritativeness
- The file is labeled "Non-Authoritative Reference" and is not listed in `docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.

## Summary
This README summarizes trigger features and context variables, but those context variables are not implemented in code. The core trigger system (BEFORE/AFTER, row vs statement) exists, but the SQL-level context variables described here are missing.

## Implemented (Partial)
- Trigger creation and execution for BEFORE/AFTER INSERT/UPDATE/DELETE with row or statement granularity exist.
  - Evidence: `src/parser/parser_v3.cpp` `parseCreateTrigger()`; trigger execution in `src/sblr/executor.cpp`.
- OLD/NEW row access exists internally in executor trigger context (not as SQL variables).
  - Evidence: `src/sblr/executor.cpp` TriggerContext.

## Gaps / Discrepancies
- SQL-level context variables listed in README (e.g., `CURRENT_USER`, `TG_NAME`, `TG_WHEN`, `TG_LEVEL`, `TG_OP`, `TG_TABLE_NAME`, `TG_TABLE_SCHEMA`) are not parsed or bound in the executor as trigger variables or functions.
- The README references `TRIGGER_CONTEXT_VARIABLES.md`, which is also non-authoritative and unimplemented.

## Notes
- This file is informational; conformance should be driven by authoritative specs.

## Suggested Next Steps
- If trigger context variables are required, define an authoritative spec and implement parser/AST/SBLR or built-in functions.
- Otherwise, keep as documentation-only and avoid using for conformance checks.
