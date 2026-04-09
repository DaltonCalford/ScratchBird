# Dependencies - 10_GC_and_Sweep

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the authoritative dependency contract for section `10`.

## Upstream dependencies

- section `05` for page legality, compaction bounds, checksum, and repair-state
  constraints
- section `08` for transaction truth, `OIT/OAT/OST`, prepared/limbo handling,
  restart rules, and visibility authority
- section `09` for lock-manager constraints that can affect sweep admission and
  restart-class failure outcomes
- section `24` for catalog-backed sweep cursor history, page-audit findings,
  shadow-capture manifests, and retained-evidence metadata

## Internal runtime dependencies

- `HeapPage` for maturity scan, prune, reclaim, and dead-TID collection
- `SweepManager` for sweep orchestration, policy lanes, staged handoff, and
  progress persistence
- `StorageEngine` for foreground cleanup paths that must reuse the same reclaim
  legality contract
- `CatalogManager` for durable sweep cursor history and retained-evidence state

## Downstream dependents

- section `18` for heap-proof-first index cleanup and backlog publication
- section `20` for support-bundle, diagnostics, and observability surfaces
- section `31` for conformance, performance, and reliability gates

## Dependency rules

1. Section `10` shall not redefine transaction visibility or restart truth
   owned by section `08`.
2. Section `10` shall not absorb page-structure ownership from section `05`.
3. Catalog-backed cursor/evidence state is a real upstream dependency, not an
   optional side channel.
4. Support-bundle and gate consumers are downstream dependencies, not runtime
   reclaim authority.
