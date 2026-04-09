Status: reconstructed_required_with_current_substrate

# Buffer Cache Working Set and Pressure Escalation Model

## Purpose

This document defines how ScratchBird shall prioritize, protect, and shed in-memory state under pressure. It covers page cache, execution caches, JIT artifacts, and memory-resident index families.

## Canonical Working-Set Classes

ScratchBird shall account for at least the following memory working-set classes:

- durable page and buffer cache
- temporary workfile buffers
- statement cache
- SQL-to-SBLR or SBLR-to-v3 translation cache
- JIT artifact cache
- resident vector and ANN index state
- metadata and statistics cache

## Pressure Classes

The runtime shall classify pressure as:

- `NORMAL`
- `ELEVATED`
- `HIGH`
- `CRITICAL`
- `FAIL_CLOSED`

## Escalation Order

When pressure rises, the engine shall shed or refuse memory in this order:

1. clear statement scratch and short-lived arenas
2. reduce or invalidate translation-cache entries
3. reduce or invalidate statement-cache entries
4. refuse new JIT compilation and optionally retire cold JIT artifacts
5. increase spill behavior for eligible operators
6. refuse new resident-index admission
7. refuse new memory-heavy work when local safety would otherwise be compromised

## Protected Working Sets

The following classes are protected and shall not be evicted by ordinary cache pressure before the earlier steps have been exhausted:

- buffer cache pages needed for durability or checkpoint progress
- currently executing operator state
- active transaction metadata
- admitted resident vector index state currently serving queries

## Resident Vector Index Rule

Vector and related ANN index classes that are marked resident shall:

- load from durable storage on first admitted use
- remain in memory after load
- flush durable changes to disk according to the owning index family contract
- continue serving reads from the in-memory state

Ordinary LRU-style eviction is non-conforming for admitted resident vector indexes. Retirement is permitted only for:

- explicit administrative unload
- process shutdown
- fatal pressure policy that records an operator-visible degraded state
- corruption or compatibility refusal

## Dirty-State Rule

Pressure handling shall never discard dirty durable state without the owning durability path completing its ordered flush and publication requirements.

## Cache Identity Rule

Each cache class shall have an explicit identity and invalidation policy. Pressure shedding may invalidate entries, but it shall not reuse an entry under the wrong identity or epoch.

## Metrics Requirements

The runtime shall publish, per working-set class:

- admitted bytes
- resident bytes
- eviction or invalidation count
- spill count where applicable
- refusal count
- pressure state transitions

## Optimizer Interaction

Pressure state shall be visible to the optimizer and admission logic when it changes plan eligibility for:

- resident vector indexes
- JIT acceleration
- memory-heavy operator shapes

## Non-Guarantees

This file does not require every index family to be permanently resident. It requires only that index families explicitly classified as resident obey the admission and retention rules above.
