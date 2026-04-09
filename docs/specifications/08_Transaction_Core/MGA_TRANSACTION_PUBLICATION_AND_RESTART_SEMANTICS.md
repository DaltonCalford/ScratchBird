# MGA Transaction Publication and Restart Semantics

Status: current_authority

## 1. Publication authority

ScratchBird transaction publication is MGA-state publication.
It is not WAL publication.

### Durable and live authorities

- `TIP` is the authoritative durable transaction inventory
- `CLOG` is synchronized secondary state for compact lookup and repair
- page-0 or database-header transaction fields publish `next_xid` and related
  progress markers
- `ProcArray` is live attachment inventory only and is never restart truth

## 2. Durable state vocabulary

- `ACTIVE`
- `COMMITTED`
- `ROLLED_BACK`
- `PREPARED`

`ACTIVE` is durable begin truth.
`COMMITTED` and `ROLLED_BACK` are durable terminal truth.
`PREPARED` is durable limbo truth.
`ABORTED` is reserved for non-transaction/session/operation cancellation paths
that do not publish terminal transaction inventory state.

## 3. MGA versus WAL comparison

### MGA publication

1. durable transaction inventory is updated
2. durable page-state truth remains inside the database image
3. visibility is computed from inventory plus version lineage
4. restart reconciles durable inventory and durable page state

### WAL publication

1. authoritative log record is written
2. data pages may lag behind
3. restart correctness is rebuilt from log replay

ScratchBird Alpha follows the MGA publication column only.

## 4. Begin publication order

A transaction becomes live-attachment-visible only after durable begin
publication completes.

### Required order

1. allocate `xid`
2. write `TIP = ACTIVE`
3. write `CLOG = IN_PROGRESS`
4. publish header `next_xid`
5. perform required begin fence
6. publish `ProcArray` ownership

### Hard rule

The engine must never publish a transaction in live attachment inventory before
the durable begin state exists.

## 5. Terminal publication order

A transaction becomes durably terminal only after durable terminal inventory
publication plus the required fence completes.

### Commit order

1. publish `TIP = COMMITTED`
2. publish `CLOG = COMMITTED`
3. perform required commit fence
4. clear `ProcArray` ownership

### Rollback order

1. publish `TIP = ROLLED_BACK`
2. publish `CLOG = ROLLED_BACK`
3. perform required rollback fence
4. clear `ProcArray` ownership

### Prepare order

1. persist prepared durable evidence
2. publish `TIP = PREPARED`
3. publish `CLOG = PREPARED`
4. perform required prepared-state fence
5. clear current attachment ownership

## 6. Durability modes

The transaction layer may expose multiple durability policies, but the state
machine remains TIP and CLOG authoritative in all of them.

### Mode contract

| Mode | Fence requirement | Canonical meaning |
| --- | --- | --- |
| `STRICT` | required before acknowledgement | fully durable terminal acknowledgement |
| `GROUP_COMMIT` | batch fence allowed | batched durable acknowledgement without changing TIP and CLOG truth |
| `DEVELOPMENT_UNSAFE` | weak or deferred fence | development-only weakening of timing, not a different state machine |

None of these modes authorizes WAL semantics.

## 7. Snapshot publication inputs

Snapshot capture uses:

- durable `TIP` inventory
- synchronized `CLOG` lookup where needed
- current live `ProcArray` membership
- durable prepared-transaction inventory
- current `next_xid`
- latest committed sequence or equivalent commit fence marker

## 8. Snapshot publication algorithm

1. capture `snapshot_txid_high` from `next_xid`
2. capture `snapshot_commit_seqno_high` from the latest committed durable
   sequence
3. read candidate active members from `ProcArray`
4. discard candidates not below the snapshot high watermark
5. filter candidate members through published durable state
6. add durable prepared members
7. sort and deduplicate active membership
8. compute low watermark from lowest active member or high watermark when none
   exist
9. publish the retained or statement snapshot object
10. update backend-visible pin state when the snapshot lifetime requires it

## 9. Worked publication example

### 9.1 Begin example

1. allocate `xid = 100`
2. publish `TIP[100] = ACTIVE`
3. publish `CLOG[100] = IN_PROGRESS`
4. publish `next_xid = 101`
5. force the begin fence
6. only then place `100` in live attachment inventory

If crash occurs before step 5 completes, restart must not treat the transaction
as safely attachment-visible begin truth.

### 9.2 Commit example

1. row versions for transaction `100` already exist in pages
2. publish `TIP[100] = COMMITTED`
3. publish `CLOG[100] = COMMITTED`
4. force the terminal fence
5. clear live attachment ownership

If crash occurs before step 4 completes, restart reconciles durable evidence.
It does not replay a WAL stream to invent commit truth.

## 10. Restart-decision vocabulary

Restart decisions must be expressed in canonical MGA terms:

- `TX_RESTART_TUPLE_WRITE_CONFLICT`
- `TX_RESTART_LOCK_TIMEOUT`
- `TX_RESTART_DEADLOCK_DETECTED`
- `TX_RESTART_MISSING_STATEMENT_SNAPSHOT`
- `TX_RESTART_INACTIVE_STATEMENT_SCOPE`
- `TX_RESTART_FORENSIC_REPLAY_ACTIVE`
- `TX_RESTART_UNSUPPORTED_CONFLICT_STATUS`

Each restart reason must map to exactly one outcome:

- `RETRY_ALLOWED`
- `RETRY_REQUIRED`
- `FAIL_CLOSED`

Restart classification is based on transaction and lock state.
It is not SQL-dialect heuristic behavior.

## 11. Startup reconciliation algorithm

Startup recovery is state reconciliation, not WAL replay.

### Required reconciliation steps

1. load durable transaction header and inventory roots
2. read durable `TIP` inventory
3. read synchronized `CLOG` state
4. read durable prepared-transaction evidence
5. compare `TIP`, `CLOG`, prepared records, and on-page state markers
6. normalize incomplete `ACTIVE` states to safe terminal outcomes
7. preserve only those prepared states that have durable prepared evidence
8. remove stale prepared records
9. rewrite `CLOG` to match `TIP` truth where repair is required
10. republish horizon markers and restart-visible transaction inventory

## 12. Repair and refusal rules

The engine must fail closed when:

- `ProcArray` is the only available truth for a restart decision
- durable state cannot distinguish `COMMITTED`, `ROLLED_BACK`, and `PREPARED`
- prepared inventory cannot be reconciled safely
- snapshot active-set completeness cannot be proven

The engine must never:

- treat `ProcArray` as durable truth after restart
- treat unknown `CLOG` state as committed by default
- describe statement restart as redo, undo-log replay, or WAL replay

## 13. Implementation contract

Any implementation against this file must prove:

1. begin publication is durable-before-visible
2. terminal publication is terminal-before-procarray-clear
3. snapshot capture uses durable inventory plus live inventory
4. restart outcomes use the canonical restart vocabulary
5. startup repair is inventory reconciliation, not WAL replay
