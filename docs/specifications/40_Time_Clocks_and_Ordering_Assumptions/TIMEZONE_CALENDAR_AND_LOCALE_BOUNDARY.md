# Timezone Calendar and Locale Boundary

## Purpose

This file defines the current timezone, calendar, and locale boundary.

## Current rules

Timezone-aware behavior is limited to explicit type and client surfaces that represent or format temporal values.

Timezone and locale presentation semantics do not redefine engine-internal transaction ordering, replay ordering, or durability truth.

Calendar and locale formatting behavior may vary by explicit client or tooling surface unless another owning section defines a stronger contract.

Civil-time arithmetic and donor-engine datetime parity are not implied by this section.

## Explicit exclusions

There is no broad cross-language timezone parity claim in current ScratchBird.

There is no universal locale formatting guarantee in current ScratchBird.

There is no complete civil-time arithmetic model defined by this section.
