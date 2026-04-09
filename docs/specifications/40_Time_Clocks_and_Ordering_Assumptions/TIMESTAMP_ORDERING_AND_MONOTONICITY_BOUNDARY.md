# Timestamp Ordering and Monotonicity Boundary

## Purpose

This file defines what ScratchBird does and does not mean by timestamp order.

## Current rules

Transaction publication order is not defined by wall-clock timestamps.

Transaction correctness remains governed by MGA publication, visibility, transaction-map state, and committed publication order, not by timestamp comparison.

User-visible timestamps are presentation values and data values. They do not by themselves prove strict monotonic runtime order.

Local counters or local timestamps may be monotonic within a narrow implementation surface, but no system-wide monotonic order guarantee is implied unless an owning section defines that surface explicitly.

## Explicit exclusions

There is no global total-order timestamp subsystem.

There is no strict monotonic timestamp guarantee across hosts, processes, restarts, or artifacts.

There is no timestamp-based conflict-resolution model.
