# Startup Recovery Flow

Status: current_authority

## 1. Governing rule

Startup recovery is state reconciliation.
It is not WAL replay.

The startup flow normalizes transaction inventory, page debt, and bootstrap state until the database can safely admit new work.

## 2. Core startup phases

The canonical startup flow is:

1. open and validate primary bootstrap state
2. load TIP inventory
3. load synchronized secondary state such as CLOG
4. reconcile incomplete transaction state
5. restore durable prepared ownership where valid
6. normalize inventory markers
7. rebuild or validate FSM and checkpoint-debt queues
8. classify any corruption or quarantine conditions
9. only then admit new work

## 3. Transaction reconciliation rules

During startup reconciliation:

1. `ACTIVE` transactions found after restart are incomplete and must be made terminal
2. `PREPARED` transactions remain prepared only when durable prepared evidence exists
3. stale prepared records without valid durable backing are removed
4. CLOG is synchronized to reconciled TIP truth
5. startup summary records how many transactions were normalized

## 4. Startup quarantine

If startup corruption policy requires quarantine, the connection layer may force default read-only behavior.
This is a fail-closed admission rule.
It does not create WAL recovery mode.

## 5. Recovery output vocabulary

Startup reconciliation publishes at least these outcome classes:

1. clean shutdown marker present or absent
2. startup repair required or not required
3. active-to-rolled-back conversions
4. active-to-prepared conversions
5. stale prepared records removed
6. prepared TIP without catalog evidence count
7. synchronized CLOG state count

## 6. Admission boundary

New work may be admitted only after:

1. fixed bootstrap pages validate
2. transaction inventory is reconciled
3. checkpoint and dirty-state debt required for admission is classified
4. quarantine or read-only restrictions are applied when needed

## 7. Negative requirements

The following are not canonical:

1. redo-log replay phases
2. LSN-based restart ownership
3. treating dirty ProcArray state as restart truth
4. admitting new work before transaction inventory normalization

## 8. Implementation contract

Any implementation against this file must prove:

1. startup recovery is inventory reconciliation
2. incomplete active transactions are normalized to terminal state
3. prepared-state retention requires durable evidence
4. admission happens only after reconciliation completes
