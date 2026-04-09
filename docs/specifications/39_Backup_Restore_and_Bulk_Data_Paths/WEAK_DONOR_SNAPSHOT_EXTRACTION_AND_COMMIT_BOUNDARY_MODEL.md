Status: reconstructed_required_with_current_substrate

# Weak Donor Snapshot Extraction and Commit Boundary Model

## Purpose

This document defines how ScratchBird shall extract data from donor systems that do not provide native replication, deterministic commit logs, or durable transaction forcing. It exists so proxy or migration behavior does not invent consistency semantics during cutover.

## Canonical Rule

For weak donors, ScratchBird shall treat extracted data as observed source state, not as authoritative transactional truth. Migration correctness comes from explicit extraction boundaries, verification, replay ordering, and cutover fences, not from assumed donor atomicity.

## Weak Donor Definition

A weak donor is any source system that lacks one or more of the following:

- authoritative transaction visibility feed
- durable ordered change stream
- forced-write or equivalent durable commit guarantee
- reliable commit identifier that can be fenced at cutover

## Extraction Modes

ScratchBird shall classify weak-donor extraction into one of these modes:

- `FULL_SNAPSHOT_ONLY`
- `SNAPSHOT_PLUS_BEST_EFFORT_DELTA`
- `PROXY_OBSERVED_CHANGE_CAPTURE`
- `APP_QUIESCE_AND_FINAL_SNAPSHOT`

The chosen mode shall be published in migration metadata and operator status.

## Canonical Snapshot Boundary

Every weak-donor extraction cycle shall record a boundary tuple containing:

- donor identity
- extraction mode
- extraction start time
- extraction end time
- donor-side consistency indicator if any
- object set covered
- observed high-water marker if available
- known uncertainty flags

## Full Snapshot Rules

When only a full snapshot is available:

1. enumerate the target object set
2. record the extraction boundary tuple
3. extract objects in a deterministic order
4. compute source-side and ingest-side row or chunk counts where possible
5. attach uncertainty markers for objects extracted without stable donor isolation
6. publish the resulting snapshot as a migration batch, not as a donor transaction stream

## Best-Effort Delta Rules

If a donor exposes partial or approximate change evidence, ScratchBird may append deltas only when:

- the delta source is tied to a recorded snapshot anchor
- each delta event can be ordered within the migration session
- ambiguity is surfaced explicitly

Best-effort deltas are derivative migration inputs. They do not become transactional truth unless validated against the target-side cutover rules.

## Proxy-Observed Change Capture

When a donor lacks natural replication, ScratchBird may use a proxy or migration surface to observe statements or row changes. In that case:

- the proxy-observed stream is only as strong as the capture coverage
- uncovered donor writes remain a correctness risk
- cutover shall require either application quiescence, explicit fence, or verified divergence scan

## Cutover Fence Rules

A weak donor may be promoted only when all required fences are satisfied:

- snapshot extraction batch is complete
- replayable delta backlog is drained or explicitly waived
- divergence scan status is published
- operator accepts any residual uncertainty class if zero-loss cutover cannot be proven

## Uncertainty Classes

The migration lane shall classify residual risk as:

- `NONE`
- `LOW_OBSERVED_GAP_RISK`
- `PARTIAL_DELTA_COVERAGE`
- `UNFENCED_DONOR_WRITES`
- `UNBOUNDED`

`UNBOUNDED` forbids automatic cutover.

## MGA Target Rule

All ingested data becomes ordinary ScratchBird MGA-managed state once committed on the target side. The weak-donor status does not weaken target-side MGA correctness after commit.

## Audit Requirements

The migration record shall preserve:

- extraction mode
- snapshot boundary tuple
- delta source identity if used
- replay ordering or batch sequence
- uncertainty class
- cutover decision
- divergence findings

## Non-Guarantees

This file does not claim that weak donors can always provide zero-gap live migration. It defines how to represent and fence the uncertainty without inventing nonexistent donor guarantees.
