# Section 40 Test Contract

## Status

- Specification status: current_authority_with_direct_contract_proof
- Last code-audit date: 2026-03-30

## Current status

Section `40` now has maintained direct proof for the catalog-backed clock
policy, clock source, node clock state, and clock-violation-event contract
surface.

The proof is intentionally catalog-contract oriented rather than elapsed-time
or wall-clock oriented, which matches the section rule that local clocks may
annotate or schedule but do not define MGA or replay truth.

## Purpose

This file defines the proof expectations for section `40`.

## Current direct proof surface

- `tests/unit/test_catalog_cluster_clock_extension_contract.cpp` search
  `ClockCatalogContracts` for invalid policy-threshold rejection, duplicate
  clock-policy or clock-source refusal, node-clock-state uniqueness, invalid
  violation-event rejection, clock-source list round-trip, and cleanup
  sequencing.
- `src/core/catalog_manager.cpp` search
  `CatalogManager::upsertClockSourceCatalogEntry` for the persisted mutation
  path that owns declared clock-source writes.
- `src/core/catalog_manager.cpp` search `struct ClockSourceRecord` for the
  durable record shape consumed by the section `40` catalog contract.

## Required proof shape

Claims in this section must be backed by maintained tests, gate surfaces, or adjacent authoritative sections that own the underlying correctness mechanism.

If a claim depends on MGA publication, transaction visibility, schema epoch publication, restore ordering, or replay-binding order, the proof must come from the owning adjacent sections rather than from standalone wall-clock assertions.

## Required behaviors that must remain covered

- invalid clock-policy threshold configurations fail closed
- duplicate clock-policy names and duplicate per-policy clock-source priority
  ranks are rejected
- duplicate node-clock-state rows for the same node and policy are rejected
- invalid healthy or no-action violation events are rejected when they do not
  represent an actual violation
- declared clock sources remain listable and deletable through the catalog
  contract surface

## Explicit negative requirements

This section shall not be treated as proving a distributed clock subsystem.

This section shall not be treated as proving a lease subsystem.

This section shall not be treated as proving total chronological correlation across all artifacts and logs.
