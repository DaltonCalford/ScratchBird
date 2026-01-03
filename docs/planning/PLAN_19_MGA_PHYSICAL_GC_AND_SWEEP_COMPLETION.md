# PLAN_19_MGA_PHYSICAL_GC_AND_SWEEP_COMPLETION.md

## Status
**Priority:** P0 – Storage Health  
**Phase:** Alpha Exit Gate  
**Owner:** Core Storage  

## Problem Statement
MGA visibility and sweep triggers are implemented, but physical reclamation of record versions and index cleanup
are stubbed. This leads to unbounded version growth and long-term instability.

## Current State
- NEXT/OIT/OAT/OST implemented
- Sweep trigger (OST - OIT) implemented
- reclaimSpace() not implemented

## Tasks
- GC-B1: Implement physical record version reclamation (xmax < OIT)
- GC-B2: Safely unlink record version chains
- GC-B3: Integrate index cleanup for reclaimed records
- GC-B4: Implement cooperative GC during reads
- GC-B5: Implement background GC thread with throttling
- GC-B6: Add GC metrics and diagnostics

## Tests Required
- Massive UPDATE/DELETE churn tests
- Long-running snapshot blocking tests
- Index consistency under sweep
- Crash safety during sweep

## Exit Criteria
- No unbounded version growth
- Indexes never reference reclaimed records
- Long-running snapshots block reclamation safely
