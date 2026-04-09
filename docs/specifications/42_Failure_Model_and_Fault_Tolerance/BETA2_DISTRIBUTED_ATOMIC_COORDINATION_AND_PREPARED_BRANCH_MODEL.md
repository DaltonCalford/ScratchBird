# Beta 2 Distributed Atomic Coordination And Prepared Branch Model

## Purpose

Define native distributed atomic coordination for prepared branches, commit
decision publication, and coordinator failover without making external logs the
source of truth.

## Governing rules

1. Local MGA commit truth remains authoritative for local branches.
2. Distributed coordination records are first-class catalog state.
3. Prepared branches are explicit and bounded in time.
4. Coordinator failover must never create ambiguous commit outcome.

## Canonical metadata

- `sb_dist_txn`
  - `dist_txn_uuid`
  - `coordinator_uuid`
  - `state`
  - `decision_epoch`
- `sb_dist_branch`
  - `branch_uuid`
  - `dist_txn_uuid`
  - `participant_uuid`
  - `branch_state`
  - `prepared_marker`
  - `last_heartbeat_at`

## States

- `OPEN`
- `PREPARING`
- `PREPARED`
- `COMMIT_DECIDED`
- `ABORT_DECIDED`
- `COMPLETED`
- `HEURISTIC_REVIEW`

## Flow

1. Coordinator opens a distributed transaction.
2. Participants perform local work.
3. Coordinator requests prepare.
4. Prepared participants publish durable prepared markers.
5. Coordinator publishes one decision.
6. Participants finalize and acknowledge completion.

## Recovery rules

- a prepared branch without a visible decision enters recovery lookup
- if the coordinator cannot be trusted, operator review may be required
- heuristic completion is quarantined and audited

## Refusal rules

- `DIST_TXN_PARTICIPANT_UNKNOWN`
- `DIST_TXN_PREPARE_TIMEOUT`
- `DIST_TXN_DECISION_AMBIGUOUS`
- `DIST_TXN_HEURISTIC_REVIEW_REQUIRED`

## Cross-section requirements

- section `42` owns branch states and recovery classes
- section `25` owns coordinator placement and heartbeat
- section `19` owns participant trust and attestation when external resources exist
