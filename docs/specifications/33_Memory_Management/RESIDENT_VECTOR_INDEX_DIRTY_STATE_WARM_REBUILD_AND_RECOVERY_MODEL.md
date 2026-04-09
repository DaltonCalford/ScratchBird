Status: reconstructed_required

# Resident Vector Index Dirty State Warm Rebuild and Recovery Model

## Purpose

This document defines how resident vector indexes behave across dirty-state accumulation, restart, warm rebuild, and recovery.

## Canonical Rule

Resident vector index state is authoritative only for live in-memory service while admitted. After process loss or restart, durable state and canonical rebuild sources remain authoritative for recovery.

## Dirty-State Classes

The canonical dirty-state classes are:

- clean resident
- dirty resident with durable flush pending
- dirty resident with flush in progress
- degraded resident awaiting rebuild

## Restart Rule

After restart:

- the old in-memory resident image is gone
- the engine shall use durable index state or canonical rebuild sources
- warm rebuild may reconstruct the resident image before or on first use

## Warm Rebuild Rule

Warm rebuild shall preserve:

- resident family identity
- source durable image or rebuild source
- rebuild generation
- resulting admission state
- degraded or refusal reason if rebuild fails

## Recovery Rule

Recovery never trusts lost in-memory resident state over durable truth. If dirty resident state had not completed its ordered flush before failure, restart shall fall back to the last completed durable boundary and rebuild resident state from there.

## Non-Guarantees

This file does not claim zero-cost restart for resident vector indexes. It defines the correctness boundary and warm-rebuild requirement.
