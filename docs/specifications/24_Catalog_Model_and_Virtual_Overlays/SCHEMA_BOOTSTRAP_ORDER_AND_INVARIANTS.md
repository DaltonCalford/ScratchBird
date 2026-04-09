# Schema Bootstrap Order and Invariants

Status: current_authority

## Current startup anchors

Current source proves:
- database startup reaches virtual catalog initialization
- persisted catalog families back the current control-plane rows
- charset and timezone loaders provide the current resource bootstrap inputs

## Current invariants

- startup must initialize persisted catalog state before overlay exposure depends on it
- startup must complete virtual catalog registration before those overlays are queryable
- resource loaders must populate current charset and timezone lookup inputs before dependent lookups rely on them

## Non-claims

This file does not claim:
- a fully audited deterministic schema bootstrap order across every documented family
- a universal bootstrap checksum contract
- a proven exhaustive installation-order algorithm
