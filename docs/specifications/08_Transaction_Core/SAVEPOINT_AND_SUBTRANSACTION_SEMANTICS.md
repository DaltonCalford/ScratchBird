# Savepoint and Subtransaction Semantics

Status: current_authority

## 1. Governing rule

ScratchBird savepoints are logical MGA backout boundaries inside the current
transaction.
They are not independent top-level transactions and they are not WAL replay
points.

## 2. Savepoint frame model

Each savepoint frame owns at minimum:

- frame identifier
- optional user-visible name
- parent frame identifier
- creation command identifier
- rollback change inventory
- temporary-object creation inventory
- staged metadata or DDL side-effect inventory
- lock and resource acquisition markers required by backout

There is always an implicit transaction-level root frame even when the user has
not created a named savepoint.

## 3. User savepoint operations

Canonical user operations are:

- `SAVEPOINT <name>`
- `ROLLBACK TO SAVEPOINT <name>`
- `RELEASE SAVEPOINT <name>`

### Required behavior

1. savepoints exist only inside the current transaction
2. names resolve from the most recent matching active frame
3. `ROLLBACK TO SAVEPOINT` retains the named savepoint
4. younger savepoints are discarded on rollback to the named savepoint
5. `RELEASE SAVEPOINT` removes the named frame and merges its rollback state
   into its parent

## 4. Automatic statement savepoints

The engine uses implicit statement savepoints to preserve statement atomicity.

### Required behavior

1. statement execution may open an implicit statement savepoint
2. statement failure may roll back to that implicit frame
3. successful statement completion releases the implicit statement frame
4. statement restart uses this mechanism to preserve outer transaction
   continuity

This follows the Firebird-style system savepoint model rather than a separate
statement transaction model.

## 5. Savepoint creation algorithm

1. verify current transaction boundary is active
2. allocate a new savepoint frame identifier
3. bind the frame to the current command identifier and parent frame
4. initialize empty rollback, temp-object, and metadata side-effect inventories
5. push the frame onto the savepoint stack

## 6. Rollback-to-savepoint algorithm

1. verify current transaction boundary is active
2. resolve the target savepoint from the most recent matching frame
3. collect newest-first rollback changes after the target frame boundary
4. run MGA backout over those collected changes
5. release row and object effects acquired after the target boundary according
   to backout ownership rules
6. drop temporary objects created after the target boundary
7. discard younger savepoint frames
8. retain the named target frame
9. clear the target frame's mutation inventory
10. restore command-id and statement state from the target frame boundary

## 7. Release-savepoint algorithm

1. resolve the named frame
2. merge rollback inventory into the parent frame
3. merge temporary-object creation history into the parent frame
4. merge metadata-side-effect tracking into the parent frame
5. delete the released frame

Release never publishes schema or transaction state by itself.

## 8. Lock and resource rules

Savepoint rollback releases effects acquired after the target boundary according
to the current lock and backout model.
It does not end the outer transaction.

Rules:

1. resources wholly owned by younger savepoint work are released on rollback to
   savepoint
2. resources owned by older frames or by the outer transaction remain
3. rollback to savepoint does not clear the transaction's durable `ACTIVE` state
4. waiting transactions may proceed only for resources actually released by the
   backout path

## 9. Temporary-object rules

Temporary objects created inside savepoint scope are tracked by savepoint frame.

Rules:

1. temp objects created after the target boundary are dropped on rollback to
   savepoint
2. temp objects created before the target boundary remain
3. `RELEASE SAVEPOINT` merges temp-object creation history into the parent frame

## 10. DDL interaction

Transactional `DDL` participates in savepoint and transaction boundaries.

Rules:

1. metadata changes staged after a savepoint are part of that savepoint's
   backout inventory
2. rollback to savepoint discards unpublished metadata work after the target
   boundary
3. savepoint operations do not authorize out-of-band schema publication
4. commit remains the only ordinary publication boundary

## 11. Stable incident vocabulary

- `TX_SAVEPOINT_NOT_FOUND`
- `TX_SAVEPOINT_STACK_CORRUPT`
- `TX_SAVEPOINT_BACKOUT_FAILED`
- `TX_SAVEPOINT_RELEASE_FAILED`
- `TX_STATEMENT_SAVEPOINT_REQUIRED`

## 12. Negative requirements

The following are not canonical:

1. treating savepoints as separate top-level transactions
2. treating rollback to savepoint as ending the transaction
3. bypassing MGA backout with WAL or redo semantics
4. erasing the named savepoint during `ROLLBACK TO SAVEPOINT`

## 13. Implementation contract

Any implementation against this file must prove:

1. savepoints exist only inside the current transaction
2. rollback to savepoint retains the named frame and discards only younger
   frames
3. release merges rollback state into the parent frame
4. automatic statement savepoints provide statement atomicity
5. savepoint rollback uses MGA backout, not log replay
