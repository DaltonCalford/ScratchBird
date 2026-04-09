# Optimizer Pass Pipeline

Status: current_authority

## Current pass pipeline

The current pass pipeline is code-backed but narrower than historical section prose.

## Current proof lanes

- semantic analysis support for v3 optimizer preparation
- rewrite-before-search markers carried in runtime-plan payload contracts
- access-path candidate enumeration and family lowering
- selectivity and statistics-driven costing
- join-ordering search and legality pruning
- runtime-plan finalization and diagnostics export

## Current authority rule

This file authorizes only the pass ordering and pass families proven by the current select-planning path. It does not authorize a broader exhaustive pass registry outside that code-backed path.

## Non-guarantees

- pass ordering outside the code-backed select-planning path is not claimed here
- broad normalization, rewrite-evidence, and cross-statement pass inventories are not claimed here unless proven in current audited code
