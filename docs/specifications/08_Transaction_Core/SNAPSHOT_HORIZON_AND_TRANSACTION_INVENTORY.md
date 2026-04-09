# Snapshot Horizon and Transaction Inventory

Status: current_authority

## 1. Inventory authority

The canonical transaction inventory model is:

- `TIP` is authoritative durable inventory
- `CLOG` is synchronized secondary state
- `ProcArray` is live attachment inventory only
- prepared inventory is added explicitly to snapshot active membership

No snapshot or restart path may treat live backend inventory as durable truth.

## 2. Horizon markers

The engine tracks Firebird-style horizon markers:

- `OIT` - oldest interesting transaction
- `OAT` - oldest active transaction
- `OST` - oldest snapshot transaction

These markers are updated after:

- transaction-start snapshot publication
- transaction end
- startup inventory reconciliation

They are not WAL-derived markers.

## 3. Snapshot object model

A snapshot object owns:

- `snapshot_serial`
- `snapshot_txid_low`
- `snapshot_txid_high`
- `snapshot_commit_seqno_high`
- captured active-member set
- owner kind in `{TRANSACTION_RETAINED, STATEMENT, FORENSIC_REPLAY}`
- owner backend slot

## 4. Snapshot capture algorithm

The canonical capture procedure is:

1. capture `snapshot_txid_high` from current `next_xid`
2. capture `snapshot_commit_seqno_high` from latest durable committed sequence
3. scan live attachment inventory for candidate active `xid` values below the
   high watermark
4. filter those candidates through published `TIP` state
5. add durable prepared `xid` values
6. sort and deduplicate the active set
7. set `snapshot_txid_low` to the lowest active member or to the high watermark
   when no active members exist
8. allocate a `snapshot_serial`
9. publish backend-visible pin state when snapshot lifetime requires pinning

## 5. Horizon update algorithm

### OAT

`OAT` is the lowest durable or live active transaction visible after filtering
through current inventory truth.

### OST

`OST` is the oldest retained snapshot transaction that still pins snapshot
horizons.

### OIT

`OIT` advances only when older transactions are no longer interesting to MGA
visibility, restart, or cleanup logic.

The engine must never advance these markers from parser-facing heuristics or
from lock-state guesswork.

## 6. Snapshot pinning

Snapshot readers may pin visibility horizons through the transaction manager.

Rules:

1. pins are associated with backend slot and `snapshot_serial`
2. retained transaction snapshots pin until transaction end
3. statement snapshots pin only for statement lifetime
4. forensic replay snapshots pin for replay-session lifetime
5. backend-visible `xmin` or equivalent horizon state must reflect active pins

## 7. Retained versus statement snapshots

The engine distinguishes:

- retained transaction-start snapshot
- statement snapshot for read-consistency scope
- forensic replay snapshot

These snapshot owners have different lifetimes and must not be conflated.

## 8. Snapshot-at-anchor rule

If the engine supports explicit snapshot inheritance or attach-at-snapshot
behavior, the requested anchor must refer to a currently active valid snapshot.

Rules:

1. unknown snapshot anchor fails closed
2. stale or inactive snapshot anchor fails closed
3. adopted snapshot anchor becomes the retained snapshot for the new
   transaction boundary

## 9. Negative requirements

The following are not canonical:

1. treating live backend inventory as durable restart truth
2. capturing active membership without `TIP` filtering
3. inventing snapshot membership from lock state instead of transaction
   inventory

## 10. Implementation contract

Any implementation against this file must prove:

1. snapshot capture uses both live inventory and durable inventory
2. prepared transactions participate in snapshot active membership
3. `OIT`, `OAT`, and `OST` remain explicit inventory markers
4. snapshot pins are backend-visible horizon state
