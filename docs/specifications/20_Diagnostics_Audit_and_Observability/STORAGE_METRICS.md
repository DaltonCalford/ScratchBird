# Storage Metrics

Status: current_authority

## Purpose

Storage metrics must give operators and the optimizer enough detail to understand page health, reclaim debt, lineage depth, family-local structural efficiency, and index-family planner-readiness.

## Required metric groups

ScratchBird shall expose at minimum the following storage metric groups.

### Page integrity metrics

- page count by family
- header checksum failures by family
- payload checksum failures by family
- repair-marker count by family
- pages in `repair_required` or `containment_required`

### Heap lineage metrics

- tuple versions per page
- average version-chain depth
- max version-chain depth
- reclaimable versions below `OST`
- rolled-back invisible versions pending reclaim
- delete-stub count

### Sweep debt metrics

- sweep pages pending
- sweep passes completed
- versions examined per pass
- versions reclaimed per pass
- pages blocked by verification failure
- pages blocked by derivative-lane failure

### Index health metrics

- pages by index family
- live entries
- dead entries pending cleanup
- split count
- merge or compaction count where the family supports it
- structural validation failures
- candidate-hit false-positive rate after MGA visibility recheck
- visibility-reject rate by family
- exact-recheck reject rate where the family requires exact recheck
- maintenance debt by family

### Planner-family metrics envelope

For every planner-visible index, the system shall expose the current typed planner metrics envelope used by the optimizer. Operator-facing storage diagnostics shall include at minimum:

- `index_id`
- `physical_family`
- `planner_family`
- `queryability_state`
- `metrics_last_refresh_xid`
- `metrics_confidence_class`
- `leaf_pages`
- `height`
- `row_count_est`
- `live_entry_count_est`
- `dead_fraction`
- `bloat_ratio`
- `recheck_ratio_est`
- `correlation`
- `coverage_fraction`
- `maintenance_backlog_ops`
- `publish_lag_xids`
- `reclaim_lag_xids`
- `family_metrics_version`
- `family_metrics_type`
- `family_metrics_payload`

### Planner-family payload classes

Operator outputs shall preserve the same planner-family classification used by the optimizer:

- `ORDERED_EXACT`
- `SUMMARY_CANDIDATE`
- `GENERALIZED_SPATIAL`
- `TEXT_SEARCH`
- `ANN`

### Family-local required metrics

In addition to the common planner envelope, operator outputs shall expose family-local counters where the family supports them.

Ordered family examples:

- duplicate density
- split rate
- right-sibling chase frequency
- range-scan page amplification
- dead-entry debt

Summary family examples:

- ranges summarized
- invalid ranges
- prune ratio
- unsummarized fraction
- summary refresh lag

Spatial family examples:

- overlap ratio
- candidate amplification
- exact geometry recheck rate
- dead-entry debt

Text or inverted family examples:

- token or posting cardinality
- posting-list skew
- pending-merge debt
- exact recheck rate
- stale-hit ratio

ANN family examples:

- candidate expansion count
- rerank fraction
- recall calibration signal
- stale-entry debt
- rebuild or repair debt

### Derivative-lane metrics

- write-after records emitted
- write-after failures
- shadow pages copied
- shadow lag or missing copy count
- temporal records exported
- temporal export failures

### Buffer and writeback policy metrics

Storage diagnostics shall also expose the operator-visible buffer and writeback
policy surfaces needed for commercial-grade cache behavior analysis.

At minimum these shall include:

- hit ratio by policy domain
- hit ratio by page class
- ghost-hit count
- admission reject count by reason
- probation-to-protected promotion count
- protected-to-probation or eviction demotion count
- dirty-generation oldest age
- dirty-generation spread
- writeback queue depth by queue state
- checkpoint debt bytes or pages
- foreground flush latency
- background cleaner pass count
- prefetch debt
- prefetch usefulness rate
- thrash-state transitions
- warmup candidate count
- warmup pages loaded
- warmup usefulness rate after restart
- eviction reason counts by class
- pin wait time
- content lock wait time
- page-table partition contention counters
- per-tablespace cache usage and dirty usage
- background-writer effectiveness ratio
- explicit backpressure or stall-state counters for write admission and flush pressure

These metrics are operator and tuning surfaces only.
They must not be described as recovery truth, WAL distance, or LSN authority.

## Metric freshness and authority

Storage metrics shall carry:

- capture timestamp
- sweep pass identifier when relevant
- family identifier
- collection method (`exact`, `sampled`, `estimated`)
- confidence or sample coverage where not exact

If a metric is not exact, the engine must label it as sampled or estimated. The optimizer must not silently treat sampled metrics as exact.

## Operator and planner contract

The operator diagnostics surface and the optimizer metrics surface must describe the same family metrics state. The operator view may add presentation fields, but it must not invent a second incompatible metrics model.

For buffer and writeback policy metrics, the operator view must remain
ScratchBird-native:

- dirty-generation age is legal
- checkpoint debt is legal
- forced-write fence status is legal
- WAL-distance and LSN-gated flush eligibility are not canonical storage
  metrics for ScratchBird Alpha
