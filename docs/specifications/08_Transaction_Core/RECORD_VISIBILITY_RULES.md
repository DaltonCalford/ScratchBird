# Record Visibility Rules

Status: current_authority

## 1. Governing rule

Record visibility is MGA visibility.
It is derived from creator state, deleter state, reader transaction context,
isolation mode, and retained or statement snapshot state where required.

It is not derived from WAL position, log sequence, or parser dialect.

## 2. Version inputs

The canonical visibility inputs are:

- `create_xid`
- `delete_xid`
- reader `xid`
- reader visibility mode
- retained transaction snapshot when required
- statement snapshot when required
- durable transaction-state lookup for creator and deleter

## 3. Visibility modes

The transaction layer evaluates visibility under these modes:

- `READ_CURRENT_TRANSACTION`
- `READ_CURRENT_VERSION`
- `SNAPSHOT`

### Mode mapping

| Transaction setting | Visibility mode | Snapshot owner |
| --- | --- | --- |
| `READ_COMMITTED` | `READ_CURRENT_TRANSACTION` | none unless statement read consistency is active |
| `READ_COMMITTED_READ_CONSISTENCY` | statement-snapshot evaluation | statement snapshot |
| `SNAPSHOT` | `SNAPSHOT` | retained transaction snapshot |
| `SNAPSHOT_TABLE_STABILITY` | `SNAPSHOT` | retained transaction snapshot |
| forensic replay | replay-bounded snapshot | replay snapshot |

## 4. Transaction-state evaluation algorithm

Given an `xid` and reader context:

1. reject invalid `xid`
2. if `xid` belongs to the reader transaction, return `VISIBLE_TO_SELF`
3. if `xid` is frozen or prehistorical committed inventory, return
   `VISIBLE_COMMITTED`
4. if evaluating under non-snapshot current-reader mode and `xid` is in the
   reader's future relative to current high watermark, return `INVISIBLE_FUTURE`
5. if evaluating under snapshot mode and `xid` is above snapshot high
   watermark, return `INVISIBLE_FUTURE`
6. resolve durable state from MGA inventory
7. if snapshot mode and `xid` is a member of the captured active set, return
   `INVISIBLE_ACTIVE_SNAPSHOT_MEMBER`
8. if durable state is `COMMITTED`, apply snapshot commit-fence rule and return
   `VISIBLE_COMMITTED` when allowed
9. if durable state is `ACTIVE`, return `INVISIBLE_ACTIVE`
10. if durable state is `ROLLED_BACK`, return `INVISIBLE_ROLLED_BACK`
11. if durable state is `PREPARED`, return `INVISIBLE_PREPARED` for ordinary
    readers

## 5. Record visibility algorithm

For a candidate version:

1. evaluate creator state
2. if creator state is not visible and a back version exists, return
   `FOLLOW_BACK_VERSION`
3. if creator state is not visible and no back version exists, return
   `TERMINAL_NOT_VISIBLE`
4. if `delete_xid = 0`, return `RETURN_VISIBLE`
5. evaluate deleter state
6. if deleter state is not visible, return `RETURN_VISIBLE`
7. if deleter state is visible, return `TERMINAL_NOT_VISIBLE`

## 6. Version-chain traversal vocabulary

- `RETURN_VISIBLE`
- `FOLLOW_BACK_VERSION`
- `TERMINAL_NOT_VISIBLE`
- `CORRUPT_VERSION`

### Traversal rules

1. invalid creator metadata may produce `CORRUPT_VERSION`
2. invisible creator with a back version yields `FOLLOW_BACK_VERSION`
3. visible creator plus invisible deleter yields `RETURN_VISIBLE`
4. visible creator plus visible deleter yields `TERMINAL_NOT_VISIBLE`
5. a version marked corrupt must not be silently treated as invisible normal
   data

## 7. Snapshot-specific rules

### Retained transaction snapshot

- active-set membership is fixed at retained snapshot capture
- creator or deleter in that captured active set is invisible
- transactions above snapshot high watermark are invisible

### Statement snapshot

- statement snapshot is created at statement scope only when the transaction
  mode requires it
- missing required statement snapshot is a restart-class failure
- statement snapshot lifetime ends at statement completion or statement restart

### Prepared transaction rule

Prepared versions remain invisible to ordinary readers unless a special
prepared-transaction administrative path explicitly owns different semantics.
That path is not ordinary SQL visibility.

## 8. Delete and back-version rule

`delete_xid` is evaluated independently from `create_xid`.

Consequences:

1. committed creator plus no visible delete yields a visible version
2. invisible creator requires version-chain back traversal where a back version
   exists
3. visible creator plus visible delete makes the current version non-visible
4. visible delete does not make older visible back versions disappear; traversal
   may continue where the storage family supports it

## 9. Statement read consistency and restart

Statement read consistency is statement-snapshot scoped.

Rules:

1. statement snapshot is requested from the transaction manager at statement
   scope
2. statement restart requires an active statement scope and valid statement
   snapshot when the mode requires one
3. missing statement snapshot is `TX_RESTART_MISSING_STATEMENT_SNAPSHOT`
4. statement restart preserves outer transaction continuity through savepoint
   backout, not log replay

## 10. Negative requirements

The following are not canonical:

1. visibility from WAL or log-sequence comparison
2. treating prepared versions as committed-visible to ordinary readers
3. inventing parser-specific visibility rules
4. replacing snapshot membership with ad hoc lock-state checks

## 11. Implementation contract

Any implementation against this file must prove:

1. creator and deleter visibility are evaluated independently
2. snapshot rules use captured active membership and committed fence rules
3. prepared transactions remain invisible to ordinary readers
4. traversal uses the canonical action vocabulary
5. visibility uses MGA inventory state only
