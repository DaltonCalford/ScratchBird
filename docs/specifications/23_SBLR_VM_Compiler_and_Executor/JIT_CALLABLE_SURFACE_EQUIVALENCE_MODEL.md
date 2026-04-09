Status: current_authority

# JIT Callable Surface Equivalence Model

## Purpose

This file records the currently proven callable surface coverage for JIT
VM/native equivalence.

## Current callable-surface coverage

The current JIT differential tests already cover equivalence across the
following callable or callable-adjacent surfaces:

1. scalar function-style expression execution
2. table-backed aggregate query execution
3. package-member style execution surface
4. trigger-adjacent table activity surface

## Current code-backed equivalence cases

The covered cases currently include:

1. function-style `ABS(-42)`
2. procedure-like aggregate path using table creation, insert, and `SELECT SUM(v)`
3. package-member style arithmetic path
4. trigger-adjacent table population plus `COUNT(*)`

For these cases, the current tests require VM/native agreement on:

1. success state
2. result-set presence
3. row presence or row count where applicable
4. rendered result value

## Interpretation boundary

These tests prove that current JIT scope is broader than pure scalar arithmetic.

They do not yet justify claiming universal native coverage for every routine
surface or side-effecting construct.

## Fail-closed rule

Any callable surface not covered by:

1. scope eligibility
2. supported opcode families
3. trust and compatibility rules
4. differential equivalence coverage

shall remain on the VM path or explicit compile rejection path.
