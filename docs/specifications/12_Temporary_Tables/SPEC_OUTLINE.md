# Section 12 Specification Outline

## Objective

Define the implementation-ready current ScratchBird contract for temporary
tables and planner-side spill behavior without inventing a broader runtime
workfile subsystem that current code does not prove.

## Scope

Section `12` defines the current ScratchBird authority for:
- temporary tables
- `ON COMMIT` actions on temporary tables
- session and startup cleanup of temporary objects
- non-durable temp page handling
- planner spill estimation and fail-closed spill policy behavior

## Canonical files

- `TEMP_TABLES_NORMATIVE_IMPLEMENTATION.md`
  - temp table identity, scope, cleanup, rollback boundary, and durability
    exclusion
- `TEMP_WORKFILE_AND_OPERATOR_SPILL_CONTRACT.md`
  - spill estimation, spill-disallow refusal, and explain/output metadata
- `DEPENDENCIES.md`
- `TEST_CONTRACT.md`
- `DECISION_RECORD.md`

## Hard boundaries

- temporary tables are a real current runtime feature
- temporary tables obey the global always-in-transaction MGA model from
  sections `08`, `09`, and `35`
- planner spill metadata is current authority
- runtime workfile artifacts are not current section `12` authority

## Non-goals

Section `12` does not define:
- a full spill-file or workfile artifact subsystem
- cross-host temp object behavior
- WAL-style temp durability rules
