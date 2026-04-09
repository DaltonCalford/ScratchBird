# Transaction Lifecycle

Status: current_authority

## 1. Governing model

ScratchBird is always in a transaction.
There is no ordinary attachment state in which SQL executes outside a
transaction.

The attachment owns exactly one current transaction boundary except during:
- explicit shutdown or termination
- the narrow handoff window after `PREPARE TRANSACTION`
- startup or recovery refusal before normal execution begins
- explicit dormant-detach retention of an active transaction context

This lifecycle is MGA-native. It is not WAL-owned, parser-owned, or
auto-commit-idle owned.

## 2. MGA versus WAL boundary

ScratchBird transaction behavior must be understood in MGA terms:

1. the database stores multiple transaction-stamped record versions
2. visibility is computed from durable transaction state plus version lineage
3. `COMMIT` and `ROLLBACK` publish terminal transaction state
4. dormant or prepared state preserves or records transactional truth without
   turning a log into the authoritative recovery source
5. garbage collection later removes versions that are no longer needed

The following WAL-style mental model is non-canonical:

1. update the row in place
2. describe the change in an authoritative redo log
3. rebuild correctness later from log replay

ScratchBird does not use that model for Alpha transaction truth.

## 3. Canonical transaction states

### Attachment-owned boundary state

- `TX_BOUNDARY_ACTIVE`
- `TX_BOUNDARY_DORMANT_RETAINED`
- `TX_BOUNDARY_PREPARED_HANDOFF`
- `TX_BOUNDARY_SHUTDOWN`
- `TX_BOUNDARY_TERMINATED`

### Durable transaction state

- `TX_DURABLE_ACTIVE`
- `TX_DURABLE_COMMITTED`
- `TX_DURABLE_ROLLED_BACK`
- `TX_DURABLE_PREPARED`
- `TX_DURABLE_DORMANT`

`TX_BOUNDARY_ACTIVE` is the normal attachment state.
`TX_DURABLE_PREPARED` and `TX_DURABLE_DORMANT` are not ordinary parser-owned
execution states.
`TX_DURABLE_ABORTED` is reserved for non-transaction abort/cancel paths and is
not the durable terminal state of a completed transaction rollback.

## 4. Boundary rules

These rules are mandatory:

1. attachment initialization opens the initial transaction boundary
2. `COMMIT` ends the current transaction and immediately opens the next one
3. `ROLLBACK` ends the current transaction and immediately opens the next one
4. `PREPARE TRANSACTION` moves the current transaction to durable prepared state
   and immediately opens the next transaction for the attachment
5. explicit dormant detach preserves or records the current transaction for
   later explicit reattach instead of auto-opening a replacement immediately
6. `shutdownTransaction()` is the explicit exception path that ends the current
   transaction without opening a replacement
7. governance termination is the only other normal path that prevents
   replacement begin

No idle-between-statements transaction model is canonical.

## 5. Transaction settings model

`START TRANSACTION` and `SET TRANSACTION` do not enter transactional mode.
They change transaction attributes for an attachment that is already inside a
transaction.

Required staged settings:
- isolation level
- read-only flag
- read-committed mode
- lock-wait policy
- lock-timeout value
- snapshot inheritance or explicit snapshot-anchor policy when supported

Settings application rules:
1. the attachment always has a current transaction when these statements run
2. the statement validates the requested setting set
3. if `commit_outstanding = true`, the current transaction is committed and the
   replacement transaction is opened with the new settings immediately
4. if `commit_outstanding = false`, the new settings are staged in connection
   context and become active at the next `COMMIT`, `ROLLBACK`, or
   `PREPARE TRANSACTION` boundary
5. validation failure leaves current transaction settings unchanged
6. staged settings are attachment-owned state, not parser-owned state

## 6. Begin algorithm

Required begin procedure:
1. allocate a new `xid`
2. allocate or derive a new transaction UUID and lineage record
3. materialize effective settings from:
   - current attachment defaults
   - staged next-transaction settings
   - runtime safety overrides such as startup read-only quarantine
4. write durable `TIP` state as `ACTIVE`
5. write synchronized `CLOG` state as `IN_PROGRESS`
6. publish transaction header progress such as `next_xid`
7. perform the required durability fence for begin publication
8. publish live attachment membership in `ProcArray`
9. capture retained transaction snapshot when the isolation mode requires it
10. update horizon markers such as `OIT`, `OAT`, and `OST` after snapshot
    publication completes
11. clear staged next-transaction settings that were just consumed

Begin must fail closed if:
- a new `xid` cannot be allocated
- durable begin publication cannot be established
- startup or governance policy prohibits the requested write mode
- an explicit snapshot anchor cannot be validated

## 7. Commit algorithm

Required commit procedure:
1. verify current transaction boundary is active
2. verify no unresolved statement-restart or savepoint backout conflict remains
3. flush transactional `DDL` staging into commit-owned catalog publication work
4. flush transaction-end policy such as temp-object or governance actions
5. publish terminal `TIP` state as `COMMITTED`
6. publish terminal `CLOG` state as `COMMITTED`
7. perform the required durability fence for the current durability mode
8. clear `ProcArray` live ownership only after terminal durability is
   established
9. persist transaction history, schema epoch, and lineage evidence where enabled
10. clear statement state, statement snapshot, and savepoint stack
11. apply any staged next-transaction settings
12. immediately open the replacement transaction unless shutdown or termination
    explicitly prevents it

Commit success is reported only after the required durability fence completes.

## 8. Rollback algorithm

Required rollback procedure:
1. verify current transaction boundary is active
2. invoke the MGA backout path or transaction-level rollback path for the full
   transaction scope
3. discard transactional `DDL` staging and unpublished metadata changes
4. drop or detach temporary objects whose lifetime ends with rollback
5. publish terminal `TIP` state as `ROLLED_BACK`
6. publish terminal `CLOG` state as `ROLLED_BACK`
7. perform the required rollback durability fence for the current durability
   mode
8. clear `ProcArray` live ownership only after terminal durability is
   established
9. clear statement state, statement snapshot, and savepoint stack
10. apply any staged next-transaction settings
11. immediately open the replacement transaction unless shutdown or termination
    explicitly prevents it

Rollback ends the current transaction and starts the next one.
It does not leave the attachment outside a transaction.

## 9. Prepared transaction algorithm

Required prepare procedure:
1. verify current transaction boundary is active
2. verify transaction is eligible for prepare
3. persist prepared-transaction durable evidence
4. publish terminal `TIP` state as `PREPARED`
5. publish synchronized `CLOG` state as `PREPARED`
6. establish the required durability fence for prepared state
7. clear current attachment ownership of the prepared transaction
8. allocate a fresh backend slot or attachment-local transaction boundary
9. open the next transaction immediately for the attachment

Prepared state is durable limbo state.
It is resolved only by `COMMIT PREPARED` or `ROLLBACK PREPARED`.

## 10. Dormant detach and reattach algorithm

Current code-backed authority supports explicit dormant detach and explicit
dormant reattach.

### Dormant detach

Required dormant-detach procedure:
1. validate that a live connection context exists
2. capture current transaction identity and attachment identity
3. capture current user, session user, role, schema, isolation, access mode,
   wait mode, autocommit mode, session settings, and statement diagnostics
4. assign `dormant_since` and lease-expiry timestamps
5. create a single-use reattach authkey scoped to reattach
6. persist a dormant-transaction catalog row
7. retain the in-memory `ConnectionContext` in the dormant registry when the
   process remains alive
8. return:
   - `dormant_id`
   - single-use reattach authkey

Required invariant:
- retaining the dormant connection preserves locks and `ProcArray` visibility
  for the live dormant transaction

### Dormant reattach

Required dormant-reattach procedure:
1. client reconnects explicitly with dormant reattach enabled
2. client supplies:
   - `dormant_id`
   - single-use reattach authkey
3. authentication completes first
4. server verifies the dormant row belongs to the authenticated principal
5. server verifies the reattach authkey
6. if the original in-memory dormant context still exists:
   - rebind that live `ConnectionContext`
   - preserve the transaction and its MGA visibility state
7. if the process restarted and replacement reattach is allowed:
   - reopen a replacement transaction from persisted dormant session state
   - do not resurrect or replay the original transaction
8. if restart-time reattach is denied:
   - fail closed
   - leave dormant evidence operator-visible for inspection or cleanup

Current restart policy names are:
- `ALLOW_REPLACEMENT`
- `DENY_AFTER_RESTART`

Current cleanup policy names are:
- `KEEP`
- `EXPIRE_ONLY`
- `ROLLBACK_EXPIRED`
- `ROLLBACK_EXPIRED_AND_PURGE`

### MGA boundary for dormant reattach

Dormant reattach is MGA state retention or state-guided replacement.
It is not WAL replay.

The original transaction is never reconstructed from an authoritative log.

## 11. Autocommit algorithm

Autocommit is an attachment policy layered on top of the always-in-transaction
model.

Autocommit is effective only when:
- `autocommit_mode = true`
- `autocommit_suspended = false`

Statement completion under autocommit:
1. statement begins inside the current transaction
2. statement may create an implicit statement savepoint
3. if the statement fails:
   - the post-statement commit does not occur
   - the transaction remains active
   - the caller may correct work, continue, roll back to savepoint, or issue
     `ROLLBACK`
4. if the statement succeeds:
   - statement-local savepoint is released
   - `COMMIT` executes automatically
   - a replacement transaction opens immediately

Autocommit never means "no transaction".

## 12. DDL and DML rule

`DDL` and `DML` obey the same transaction lifecycle:
- same begin boundary
- same savepoint model
- same rollback behavior
- same MGA visibility and publication rules
- same commit and replacement-boundary rules

Consequences:
1. `DDL` is not implicitly out-of-band
2. schema publication is commit-bound
3. rollback of `DDL` is ordinary transaction rollback behavior
4. failed `DDL` under autocommit does not commit and does not leave the
   attachment outside a transaction

## 13. Multi-handle common-transaction boundary

The current ScratchBird implementation authority is:
- one active attachment-owned transaction boundary
- prepared transactions
- explicit dormant detach and explicit dormant reattach
- restart-time replacement reattach when policy allows

General concurrent multi-attachment execution against one live transaction is
not promoted here as current generic authority.

If a broader common-transaction or multi-handle transaction model is promoted
later, it must preserve:
- one authoritative MGA transaction lifecycle
- one authoritative commit or rollback outcome
- one authoritative visibility inventory
- no WAL-owned resurrection path

## 14. Observability requirements

Dormant and restart-reattach state must remain operator-visible through the MGA
observability surfaces for:
- dormant policy
- dormant transactions
- restart-time stale or replacement state

These views are evidence lanes for MGA state management, not an alternate
transaction owner.
durable version chain already stored in the database.

## 13. Exception and refusal cases

The engine must fail closed when:

- commit is requested for a non-active boundary
- rollback is requested for a non-active boundary
- prepare is requested for a non-active boundary
- replacement begin fails after a normal boundary transition
- forensic replay policy prohibits requested transaction mutation
- startup quarantine requires read-only and caller requests read-write
- staged settings contain an invalid combination such as conflicting wait policy
  and timeout semantics

## 14. Stable incident vocabulary

- `TX_INVALID_BOUNDARY_STATE`
- `TX_BEGIN_PUBLICATION_FAILED`
- `TX_TERMINAL_PUBLICATION_FAILED`
- `TX_REPLACEMENT_BEGIN_FAILED`
- `TX_PREPARE_NOT_ALLOWED`
- `TX_MODE_CHANGE_REFUSED`
- `TX_STARTUP_READONLY_REFUSED`
- `TX_FORENSIC_REPLAY_MUTATION_REFUSED`

## 15. Negative requirements

The following are explicitly non-canonical:

1. an ordinary idle attachment with no current transaction
2. `START TRANSACTION` as the act of entering transactional mode
3. implicit `DDL` autocommit outside the normal boundary rules
4. WAL-owned transaction truth

## 16. Implementation contract

Any implementation against this file must prove:

1. attachments begin with a transaction boundary
2. `COMMIT`, `ROLLBACK`, and `PREPARE TRANSACTION` reopen a replacement
   boundary immediately
3. autocommit performs post-success commit only
4. MGA row-version behavior follows the worked examples above
