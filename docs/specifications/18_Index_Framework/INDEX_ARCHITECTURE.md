# Index Architecture

Status: current_authority

## Purpose

All ScratchBird index families are search accelerators layered on top of MGA-visible row truth. No index family is allowed to become the authoritative source of row visibility, transaction truth, or delete truth.

## MGA-first rule

Every index family shall obey these invariants:

- index entries identify candidate row versions or candidate row heads
- final row acceptance is decided by MGA visibility against transaction inventory and version lineage
- dead-entry cleanup is downstream of heap-version reclaim proof
- structural locks or page latches are not substitutes for MGA visibility rules

## Family contract

Every shipped index family shall define at minimum:

- entry format
- key normalization rules
- publication order for insert, update, and delete
- split, merge, and compaction behavior where applicable
- MGA visibility recheck path for candidate hits
- dead-entry lifecycle and cleanup rules
- optimizer-visible metrics packet

If a family cannot satisfy this contract, it shall remain unsupported or quarantined from planner use.

## Search contract

The runtime search contract is:

1. descend the family structure using family-local routing rules
2. return candidate row references
3. re-check MGA visibility using row-version truth
4. accept only candidates visible to the current transaction context
5. optionally record false-positive and visibility-reject metrics for optimizer calibration

## Structural concurrency

Families may use latches, sibling chase, split-tolerant descent, optimistic restart, or other structural coordination mechanisms. These are structural safety tools only. They must not redefine who can see a row version.
