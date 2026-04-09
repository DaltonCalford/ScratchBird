# Section 12 Dependencies

## Upstream ownership

Section `12` depends on:
- section `08`
  - transaction lifecycle and autocommit semantics
- section `09`
  - locking and conflict consequences
- section `24`
  - catalog identity and schema publication surfaces
- section `35`
  - durability and recovery boundary
- section `36`
  - planner spill decision inputs

## Direct code dependencies

Current section `12` authority is grounded in:
- parser temp-table and `ON COMMIT` handling
- catalog manager temp metadata and startup purge
- connection-context temp cleanup
- heap-page and on-disk temp page flags
- buffer-pool non-durable page handling
- planner spill policy and spill estimate surfaces
- executor explain or output spill metadata

## Explicit non-ownership

Section `12` does not own:
- full runtime workfile subsystem design
- distributed temp object behavior
- native wire or protocol concerns
