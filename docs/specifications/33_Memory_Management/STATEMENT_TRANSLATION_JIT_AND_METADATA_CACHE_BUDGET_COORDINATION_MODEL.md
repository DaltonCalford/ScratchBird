Status: current_authority_beta1

# Statement Translation JIT and Metadata Cache Budget Coordination Model

## Purpose

This document defines how ScratchBird coordinates budgets across statement
cache, translation cache, metadata cache, and JIT metadata cache beneath the
Section 33 budget tree.

## Canonical rule

These caches compete for the same process memory budget and shall be coordinated
through explicit class budgets, floors, and shrink ordering. No cache class is
allowed to grow without regard to the others.

## Cache classes

The coordinated classes are:

- prepared statement cache
- translation cache
- metadata and statistics cache
- JIT metadata cache

## Budget model

Each class shall have:

- reserved baseline budget
- soft ceiling
- hard ceiling
- eviction or refusal action
- accounting owner

## Pressure coordination order

When aggregate pressure rises, the runtime shall coordinate in this order:

1. trim translation-cache entries
2. trim prepared-statement cache entries
3. trim cold metadata cache entries where correctness allows
4. refuse new JIT compilation and trim cold JIT metadata
5. escalate to broader subsystem pressure handling

## Identity rule

Eviction or trimming shall preserve cache identity correctness. A stale
statement, translation, metadata, or JIT artifact shall never survive under a
mismatched epoch merely to satisfy budget retention.

## Budget credit rule

Where one cache class grows due to workload mix, the runtime may rebalance
budgets dynamically, but it shall record:

- donor class
- recipient class
- reason for rebalance
- duration or persistence of the rebalance

## Diagnostics requirements

The runtime shall expose:

- bytes per class
- hit and miss counts per class
- invalidation counts per class
- pressure-triggered trims per class
- rebalance events

## Non-guarantees

This file does not require one fixed percentage split across cache classes. It
requires coordinated accounting, pressure ordering, and deterministic
identity-safe trimming.
