# Beta 2 Cluster Remote Fragment Execution Exchange And Admission Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 runtime contract for launching remote query fragments,
managing exchange channels, applying admission and spill rules, and publishing
deterministic failure outcomes.

## Governing rules

1. One coordinator node owns query admission and fragment graph publication.
2. Remote fragments consume one bounded service envelope; they may not bypass
   local scheduler or memory governance.
3. Exchange buffers are derivative transport state, not durability truth.
4. Fragment retries are legal only where the fragment contract is explicitly
   idempotent.

## Runtime objects

- `fragment_ticket`
  - query uuid, plan uuid, fragment uuid, attempt id, tenant id, service class
- `exchange_channel`
  - channel uuid, producer fragment uuid, consumer fragment uuid, motion class,
    ordering contract, spill policy
- `remote_worker_slot`
  - node id, slot class, admitted bytes, admitted cpu weight, active fragment
    count
- `fragment_attempt_state`
  - `CREATED`, `ADMITTED`, `RUNNING`, `SPILLING`, `STITCH_WAIT`, `SUCCEEDED`,
    `REFUSED`, `FAILED`, `CANCELLED`

## Admission path

1. Coordinator validates fragment graph and node capability matches.
2. Coordinator acquires service-class and tenant budget.
3. Coordinator allocates remote worker slots.
4. Coordinator opens exchange channels.
5. Coordinator launches remote fragment attempts.

No fragment may run before both worker-slot admission and exchange-channel
publication succeed.

## Exchange behavior

- `GATHER`
  - producers push batches to one coordinator consumer
- `GATHER_MERGE`
  - producers publish ordered batches with sort-key metadata
- `BROADCAST`
  - one producer duplicates batches to all consumer fragments
- `REPARTITION_HASH`
  - producer hashes on declared distribution keys
- `REPARTITION_RANGE`
  - producer routes by declared range map

Every exchange channel must publish:

- batch rows
- batch bytes
- spill bytes
- end-of-stream marker
- failure marker with one stable refusal or failure code

## Spill and backpressure

1. If channel memory exceeds its envelope, the producer first applies
   cooperative backpressure.
2. If backpressure is insufficient and spill is legal, batches are written to
   spill segments under the owning query uuid.
3. If spill is not legal or spill budget is exhausted, the fragment is refused.

## Retry rules

- scan-only fragments may be retried when their snapshot and source placement
  remain valid
- repartitioned partial aggregates may retry only before their consumer begins
  final aggregation
- coordinator stitch fragments are never retried implicitly

## Failure outcomes

- `DIST_QUERY_REMOTE_NODE_UNAVAILABLE`
- `DIST_QUERY_EXCHANGE_SPILL_REFUSED`
- `DIST_QUERY_FRAGMENT_RETRY_EXHAUSTED`
- `DIST_QUERY_FRAGMENT_SNAPSHOT_STALE`
- `DIST_QUERY_FRAGMENT_CAPABILITY_MISMATCH`

## Metrics

- fragment admission wait time
- remote slot occupancy by service class
- exchange bytes and spill bytes
- fragment retry count
- coordinator cancel fanout latency

## Cross-section requirements

- section `25` owns fragment launch, exchange channels, slot admission, and
  retry rules
- section `33` owns memory budgets used by remote fragments and exchange spill
- section `36` owns the fragment graph and motion semantics
