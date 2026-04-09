# Allocator Free Space and Writeback Authority Model

Status: reconstructed_required_with_current_substrate

## Purpose

Define one single-owner authority split for allocation, free-space truth, writeback staging, disk-full fencing, and operator-visible incident state.

## Owner split

| Owner | Authority |
| --- | --- |
| `PageManager` | allocation, FSM truth, tablespace growth, free-page publication |
| `BufferPool` | frame residency, dirty tracking, writeback queues, eviction staging |
| `Database` | canonical config load, write-admission fencing, engine-level incident state |
| `GarbageCollector` | reclaim requests and cleanup eligibility inputs |
| `Checkpoint/Durability` | forced-write publication boundary and checkpoint drain ordering |

## Hard rules

1. Allocation truth is not owned by buffer replacement policy.
2. Writeback completion is not publication truth until durability rules say so.
3. Disk-full or writeback failure must fence admission fail closed through one explicit engine-visible incident state.
4. Prefetch, ghost history, and replacement hints are subordinate to correctness and publication.

## Operator surfaces

The canonical operator-visible surfaces shall expose:
- allocation pressure
- free-space exhaustion
- dirty backlog
- writeback failure class
- disk-full fence state
- restart recovery of pending dirty-state incidents
