# GC Sweep Algorithm

Status: current_authority

## Purpose

ScratchBird sweep is a Firebird-style MGA maintenance pass. It is not a WAL vacuum, log replay, or log-driven truth-rebuild mechanism. The database pages, transaction inventory, version chains, committed transaction states, and prepared transaction evidence remain authoritative. Sweep exists to validate durable state, compact reclaimable history, preserve required derivative evidence, and reduce version-chain debt without changing committed truth.

## Governing invariants

- ScratchBird is always in a transaction context.
- MGA state is authoritative. WAL-like streams are derivative only.
- Forced writes and ordered writes are correctness requirements.
- Sweep may reclaim history only after transaction-state and visibility proof.
- Sweep must not make an index authoritative for visibility; row-version visibility stays authoritative.
- Sweep must not advance reclamation markers ahead of required verification and evidence capture.

## Sweep inputs

The sweep controller shall capture the following before any destructive action:

- oldest interesting transaction `OIT`
- oldest active transaction `OAT`
- oldest retained snapshot horizon `OST`
- current next transaction identifier `NTX`
- current committed schema epoch
- current forced-write posture
- page-verification posture and checksum policy
- derivative export policies for write-after, shadow, and temporal archive lanes

`OST` is the oldest retained snapshot transaction/horizon that can legally
require older versions to remain materialized.

## Sweep phases

Sweep shall execute the following ordered phases.

### Phase 0: admission and horizon freeze

1. Refuse sweep if the filespace is not in a recovery-clean or recovery-permitted state.
2. Capture `OIT`, `OAT`, `OST`, `NTX`, and the committed schema epoch.
3. Create a dedicated sweep worker transaction context using the current default transaction settings plus the internal sweep role.
4. Mark the sweep context as non-authoritative for user visibility and authoritative only for validation and reclaim decisions.
5. Freeze the captured horizons for the duration of the pass. Newer transactions may continue to run, but reclaim decisions must use the frozen horizons captured at admission.

### Phase 1: mandatory verification before prune

For each page family scheduled for sweep:

1. Validate page header fields.
2. Validate header checksum and payload/data checksum where configured for the page family.
3. Validate page type, page generation, and repair markers.
4. Validate transaction inventory pages before using them as reclaim authority.
5. Validate heap tuple chains and backversion links before pruning any tuple chain.
6. Validate index structural invariants before index cleanup is allowed.

If verification fails, the page or page family shall move to `repair_required` or `containment_required`. Sweep must not perform destructive reclamation on that target until containment or repair policy allows it.

### Phase 2: version-chain classification

For each heap version chain, classify every materialized version as one of:

- `retain_visible_current`
- `retain_visible_historical`
- `retain_prepared_or_uncertain`
- `retain_repair_required`
- `prune_rolled_back_invisible`
- `prune_committed_obsolete`
- `prune_delete_stub`

A version may be placed into a prune class only if all of the following are true:

- the creating transaction has a terminal state
- any deleting or superseding transaction has a terminal state if present
- no active or prepared snapshot at or below `OST` can still require the version
- the chain linking is valid and verified
- derivative evidence requirements for the version have been satisfied or are explicitly configured as best-effort

### Phase 3: derivative evidence capture

Before removing any reclaimable version or unlinking any dead index entry, sweep shall run derivative evidence lanes in the following order:

1. local page/version audit event emission
2. write-after log emission when enabled
3. shadow-page capture when enabled
4. temporal archive emission when enabled

These lanes are derivative evidence only. They do not become correctness authority and must not redefine transaction truth. If local MGA truth and a derivative sink disagree, local MGA truth wins and the sink is marked degraded.

### Phase 4: heap reclaim

After a version chain is verified and derivative evidence requirements are satisfied:

1. unlink obsolete backversions from newest to oldest while preserving a visible head for every still-legal snapshot
2. reclaim rolled-back invisible versions
3. reclaim committed obsolete versions below `OST`
4. convert reclaimed tuple slots to free-space inventory updates using ordered page-local writes
5. publish updated free-space metadata only after the heap page write is durable

### Phase 5: index cleanup eligibility

Sweep shall not perform index cleanup until the corresponding heap reclaim decision is durable.

For each index family:

1. identify candidate dead entries whose referenced heap versions are no longer visible to any legal snapshot
2. re-check heap lineage and transaction inventory proof
3. remove or compact dead entries according to the family-local cleanup algorithm
4. preserve structural safety under page split, merge, or sibling-chase rules
5. update family metrics and cleanup debt counters

Index cleanup is a derived consequence of heap-version reclamation. It is not the source of visibility truth.

### Phase 6: sweep completion and marker advance

At end of pass:

1. write sweep completion metrics and counters
2. advance sweep watermarks only if every required page family completed without unresolved destructive ambiguity
3. advance `OIT` or equivalent oldest-interesting marker only after the system proves all lower retired states are either reclaimed or intentionally retained
4. persist completion state using forced-write / ordered-write rules

## Forced-write and ordered-write rules

Sweep shall obey the same forced-write correctness posture as normal transaction publication.

- transaction inventory state must reach durable storage before sweep relies on it for reclaim
- repaired or compacted page images must be durably written before dependent metadata is advanced
- free-space publication must not outrun page-image durability
- shadow files must receive the same forced-write discipline as the primary file when shadowing is enabled

## Statement of what sweep is not

Sweep is not:

- WAL replay
- redo-log application
- undo-log application
- background correctness reconstruction from derivative logs
- permission to reclaim versions still required by active or prepared snapshots

## Required outputs

Every sweep pass shall emit at minimum:

- versions examined
- versions reclaimed
- versions retained by active snapshot
- versions retained by prepared transaction
- pages verified
- pages failed verification
- pages repaired or quarantined
- index entries examined
- index entries removed
- write-after records emitted
- shadow pages copied
- temporal records exported
- sweep debt before and after pass
