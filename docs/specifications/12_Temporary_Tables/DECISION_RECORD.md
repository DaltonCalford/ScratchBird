# Section 12 Decision Record

## Decision 1: temporary tables are current runtime, not planned syntax

Temporary tables are current engine reality. Their authority is anchored in:
- catalog table type and temp scope enums
- parser `TEMPORARY` and `ON COMMIT` handling
- executor lowering for temp create paths
- connection-context cleanup
- startup purge

## Decision 2: `ON COMMIT` is temp-only

`ON COMMIT` is currently valid only for temporary tables. Parser refusal is part
of the active section `12` contract.

## Decision 3: temp tables obey the transaction model

Temporary table behavior is transaction-scoped under the same always-in-
transaction MGA model as other DDL and DML:
- ScratchBird is always in a transaction
- `COMMIT` and `ROLLBACK` immediately start the next transaction
- autocommit commits only after statement success
- errors leave the current transaction active

Section `12` adds temp-specific cleanup rules on top of that shared lifecycle.

## Decision 4: temp pages are non-durable

Current temp or work pages are explicitly non-durable at page level and are not
part of restart-safe durable state.

## Decision 5: planner spill authority is narrower than runtime workfile authority

Current code proves:
- spill estimation
- spill policy parsing
- spill-disallow refusal
- spill metadata in plan or explain output

Current code does not prove:
- runtime workfile identity
- spill-file lifecycle ownership
- spill restart cleanup
- generalized spill diagnostics

Those unproven surfaces remain explicit unsupported boundaries.
