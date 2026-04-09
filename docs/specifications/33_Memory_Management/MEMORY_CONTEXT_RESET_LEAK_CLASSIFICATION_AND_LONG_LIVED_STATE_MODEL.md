Status: reconstructed_required

# Memory Context Reset Leak Classification and Long Lived State Model

## Purpose

This document defines how ScratchBird classifies memory that survives context reset and how long-lived state is separated from leak candidates.

## Canonical Rule

Memory surviving a reset or teardown boundary shall be classified explicitly as either:

- intentionally long-lived state
- retained cache state
- leak candidate

Unclassified retention is non-conforming.

## Reset Boundaries

The canonical reset boundaries include:

- scratch arena reset
- statement teardown
- operator teardown
- transaction retirement
- session teardown

## Long-Lived State Rule

Memory may survive a shorter-lived boundary only when it is explicitly owned by a longer-lived context and reclassified accordingly. Survival alone does not imply a leak.

## Leak Classification Rule

A leak candidate exists when memory:

- remains owned by a retired context
- has no valid owner after reset
- cannot be justified by cache or long-lived-state policy

## Diagnostics Rule

The memory subsystem shall classify retained bytes at a reset boundary into:

- reowned long-lived state
- cache-retained state
- deferred cleanup state
- leak-candidate state

## Non-Guarantees

This file does not require one leak detector implementation. It requires explicit classification of post-reset survivors.
