# Specialized Access Method Boundary

Status: current_authority

## Purpose

Define the current boundary for specialized access families such as columnstore, ANN, text, spatial, bitmap, and other non-primary-storage families.

## Current Rule

Specialized access families are real runtime families, but they are not promoted to primary storage truth by analogy alone.

## Current Specialized Family Classes

Current specialized families include:
- vector and ANN families
- text and inverted families
- spatial families
- summary and bitmap families
- columnstore analytical family

## Columnstore Boundary

Columnstore is a current specialized analytical storage family with its own durable metadata and segment-page structures.

It is:
- real current code-backed behavior
- a specialized access family
- not the default primary row-store truth

## Planner and Runtime Reachability Rule

Planner reachability for specialized methods is only as broad as current section `18` and `36` proof.

That means:
- specialized families may be candidate producers
- heap-visible truth remains authoritative for MGA acceptance
- section `34` must not widen specialized families into a generic optimizer abstraction beyond current proof

## Maintenance and Rebuild Rule

Maintenance semantics remain family specific.

Shared section `34` rule:
- specialized-family cleanup, rebuild, and verification must stay subordinate to concrete family proof and heap-truth interaction

## Operational Support Rule

Operator guarantees remain narrow unless directly surfaced elsewhere.

Section `34` therefore must:
- acknowledge real specialized families
- refuse to overstate parity
- refuse to promote them into general table-storage truth

## Explicit Non-Guarantees

- no universal ANN, text, or spatial parity claim
- no mature planner abstraction over every specialized family
- no blanket operator support guarantee for experimental methods
