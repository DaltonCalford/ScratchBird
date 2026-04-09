# Beta 2 Failover Session Continuity And Recovery Classification Model

## Purpose

Define the client-visible outcome model for failover, reconnect, statement
replay, and transaction classification in clustered Beta 2 deployments.

## Governing rules

1. ScratchBird must not lie about transaction outcome across failover.
2. Session continuity is best-effort and classification-driven, not silent
   transparency.
3. The canonical outcome classes are durable facts derived from committed MGA
   state and failover epoch state.

## Outcome classes

- `COMMITTED_VISIBLE`
- `REJECTED_NOT_APPLIED`
- `AMBIGUOUS_RECONNECT_REQUIRED`
- `SESSION_REPLAYABLE`
- `SESSION_NOT_REPLAYABLE`
- `CURSOR_STATE_LOST`

## Session token contract

Every clustered session shall carry:

- session uuid
- connection epoch
- last acknowledged request sequence
- transaction uuid if active
- replay eligibility flag
- prepared-statement identity map digest

## Replay eligibility

A disconnected session is `SESSION_REPLAYABLE` only when all are true:

1. no active unclassified write transaction remains
2. parser-visible prepared state is restorable from durable session metadata
3. no donor protocol rule forbids replay for the request class
4. the new node is serving under a later or equal cluster epoch

## Transaction classification workflow

1. Client reconnects and presents session token.
2. New primary validates token epoch and last acknowledged request sequence.
3. Server classifies the last in-flight transaction as:
   - committed and visible
   - rejected or rolled back
   - ambiguous
4. Ambiguous transactions require explicit client retry or inspection.
5. Read-only or idempotent safe requests may be replayed automatically where
   family rules admit it.

## Failover response contract

The server shall return one stable status family on reconnect:

- `FAILOVER_RECONNECTED_REPLAYABLE`
- `FAILOVER_RECONNECTED_MANUAL_RETRY_REQUIRED`
- `FAILOVER_RECONNECTED_TRANSACTION_COMMITTED`
- `FAILOVER_RECONNECTED_TRANSACTION_REJECTED`
- `FAILOVER_RECONNECTED_CURSOR_LOST`

## Refusal rules

- replay attempted for non-replayable request class:
  `FAILOVER_REPLAY_CLASS_REFUSED`
- stale session epoch:
  `FAILOVER_SESSION_EPOCH_STALE`
- unresolved transaction classification:
  `FAILOVER_TRANSACTION_AMBIGUOUS`

## Metrics

- reconnect success rate
- ambiguous transaction count
- replayed statement count
- cursor-loss count
- failover recovery duration to first successful reconnect

## Cross-section requirements

- section 25 owns failover orchestration and epoch publication
- section 29 owns listener and parser reconnect mechanics
- section 42 owns client-visible failure classification semantics
