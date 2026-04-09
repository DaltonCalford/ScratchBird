# Clock Skew Timeout and Lease Boundary

## Purpose

This file defines the current timeout and skew boundary.

## Current rules

Timeout behavior is local runtime policy, not distributed coordination truth.

A timeout may drive retry, backoff, refusal, or local bounded waiting behavior in the owning runtime or protocol surface.

Clock skew is not part of current transaction correctness, replay correctness, or ownership correctness.

No lease, leader-lease, quorum-timeout, or clock-skew compensation model is implied unless an owning section explicitly defines it.

## Explicit exclusions

There is no lease-based ownership model in current ScratchBird.

There is no distributed timeout correctness guarantee in current ScratchBird.

There is no clock-skew compensation framework in current ScratchBird.
