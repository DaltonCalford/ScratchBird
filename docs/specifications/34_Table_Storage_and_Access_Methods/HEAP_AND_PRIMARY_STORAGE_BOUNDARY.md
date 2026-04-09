# Heap and Primary Storage Boundary

Status: current_authority

## Purpose

Define the current heap-backed primary storage contract, including stable tuple identity, MGA back-version locality, scan behavior, and storage-layer interaction with online migration and TOAST.

## Current Primary Storage Truth

Current primary storage is heap-backed and uses:
- `TupleHeader`
- `ItemPointer`
- stable `TID`
- `GPID`
- MGA back-version links
- stable `row_uuid`

## Stable Tuple Identity

Current code already treats tuple identity as:
- `TID = GPID + slot`
- stable logical row identity through `row_uuid`
- current visible head resolved through stable TID and visibility rules

This means storage access must separate:
- physical placement
- stable tuple identity
- logical row identity

## MGA Back-Version Placement

Current storage runtime already includes locality-aware back-version placement rules.

The runtime chooses placement using:
- same extent preference
- same locality bucket preference
- distance scoring

Back-version placement is therefore:
- not arbitrary
- not fully global
- intentionally locality preserving

## Heap Scan Runtime

Current heap scan behavior includes:
- sequential page traversal
- optional visibility filtering
- session filtering for temp surfaces
- read-ahead growth from sequential detection
- separate primary-tablespace and non-primary-tablespace read-ahead handling

Scan behavior must remain subordinate to MGA visibility truth rather than pretending row locks or predicate locks define visibility.

## TOAST and Oversized Value Interaction

Current primary storage already integrates TOAST preparation during mutation.

That includes:
- deciding whether a tuple should externalize payload
- storing TOAST pointers in the heap tuple image
- preserving logical mutation semantics while changing physical storage layout

## Online Migration and Dual-Source Visibility Boundary

Current code-backed storage runtime already includes dual-source tuple resolution for online migration through stable `TID` lookup.

That means:
- the storage layer can resolve which tablespace or source to read during an online migration window
- this is still subordinate to committed migration and visibility truth
- storage lookup does not invent routing authority

## Primary Storage Rules

1. Heap storage is the default primary-storage truth unless another current section explicitly proves otherwise.
2. Visibility semantics at scan time must defer to transaction and lock truth rather than restating them loosely.
3. Stable `TID` and `row_uuid` semantics must not be collapsed into page-local folklore.
4. Back-version locality is part of current storage behavior and must be preserved.
5. Online migration read resolution must remain generation and visibility subordinate, not listener authoritative.

## Explicit Non-Guarantees

- no multiple first-class row-store engines
- no broad scan-hint contract
- no claim of fully mature forwarding-row semantics across all storage cases
