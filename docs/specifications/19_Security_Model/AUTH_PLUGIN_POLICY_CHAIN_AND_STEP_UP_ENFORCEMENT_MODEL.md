Status: reconstructed_required_with_current_substrate

# Auth Plugin Policy Chain and Step-Up Enforcement Model

## Purpose

This document defines how ScratchBird applies authentication-plugin policy, step-up requirements, and MFA or re-authentication gates before publishing high-risk capabilities.

## Canonical Rule

Authentication success is not enough to grant every capability. The engine shall enforce policy-driven step-up or stronger authentication before publishing sensitive mutation capabilities.

## Policy Chain

The canonical policy chain is:

1. transport identity intake
2. plugin eligibility check
3. plugin authentication
4. local principal binding
5. ordinary capability publication
6. step-up requirement evaluation for requested high-risk actions
7. step-up satisfaction or refusal
8. elevated capability publication if admitted

## High-Risk Capability Classes

Step-up evaluation shall be required for at least:

- remote-management mutation
- listener topology mutation
- plugin or authentication policy mutation
- shared-right or shared-user mutation
- cluster secret unseal or rotation actions
- shadow promotion or failback actions that materially alter availability posture

## Satisfied Step-Up States

The engine shall classify elevated capability state as:

- `NOT_REQUIRED`
- `REQUIRED_PENDING`
- `SATISFIED`
- `REFUSED`
- `EXPIRED`

## Expiry Rule

Satisfied step-up state shall expire according to policy and shall not silently persist indefinitely across unrelated sessions or capability boundaries.

## Audit Requirements

The engine shall audit:

- policy requiring step-up
- method used to satisfy it
- principal receiving elevated capability
- expiry or revocation of the elevated state
- refusal reason

## Non-Guarantees

This file does not require one MFA technology. It requires a policy chain that can refuse sensitive actions until stronger authentication is satisfied.
