# Temporary Tables Normative Implementation

## Current authority

Temporary tables are current implementation authority in ScratchBird.

Current code-backed authority includes:
- `TableType::TEMPORARY`
- temp metadata and data scope enums
- `TempOnCommitAction`
- temp create-path lowering
- session temp lookup filtering
- commit-path temp cleanup
- session-end temp cleanup
- startup temp purge
- temp page non-durability

## Temp identity and scope model

Current temp-table identity is catalog-backed and uses:
- `TableType::TEMPORARY`
- temp metadata scope
- temp data scope
- session ownership metadata

Current code also carries session-local temp schema identity.

## `ON COMMIT` handling

Current `ON COMMIT` handling is:
- allowed only on temporary tables
- parser rejects `ON COMMIT` on non-temp tables
- parser accepts:
  - `DELETE ROWS`
  - `PRESERVE ROWS`
  - `DROP`

Current proven runtime authority:
- `DELETE ROWS`
  - implemented
- `PRESERVE ROWS`
  - implemented
- `DROP`
  - accepted syntax and current lowering surface exist
  - section `12` does not claim any broader temp-object teardown subsystem
    beyond the documented commit, session-end, and startup cleanup paths

## Cleanup lifecycle

### Commit boundary

Connection-context commit handling applies temp-table `ON COMMIT` actions.

This is layered on top of the shared transaction invariant:
- `COMMIT` ends the current transaction and immediately starts the next one

### Session end

Current runtime has session-end cleanup entry points for temporary tables and
temporary views.

### Startup

Current catalog startup path purges temporary tables left behind from prior
runtime state.

## Durability exclusion

Temp pages are explicitly marked through `PAGE_FLAG_TEMPORARY_WORK`.

Current temp/work pages are treated as:
- non-durable
- excluded from durable checkpoint tracking
- excluded from restart-safe page generations

Current rule:
- temp-work pages are intentionally outside durable MGA restart state

## Rollback boundary

Temporary-table DDL and DML remain subject to the shared transaction and
savepoint rules owned by sections `08`, `09`, and `35`.

Section `12` adds no separate temp-specific rollback mechanism beyond:
- the shared MGA transaction lifecycle
- the explicit `ON COMMIT` actions documented above

Behavior outside those documented rules must not be inferred.

## Hard boundaries

- temporary tables are real and current
- global transaction rules still govern temp DDL and temp DML
- no WAL, redo-log, or log-authoritative temp recovery semantics are allowed
- no claim of durable restart-preserved temp data is allowed
