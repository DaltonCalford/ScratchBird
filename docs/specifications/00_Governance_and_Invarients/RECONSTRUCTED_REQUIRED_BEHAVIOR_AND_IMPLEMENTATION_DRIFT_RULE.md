# Reconstructed Required Behavior and Implementation Drift Rule

## Scope

This file defines how canonical specification authority is assigned during the
rebuild or recovery stage where:

- some behavior is current code-backed authority
- some behavior is recovered from lost specification intent
- some behavior is only partially implemented

## Governing rule

During the rebuild stage, canon must explicitly distinguish:

1. current code-backed authority
2. required reconstructed behavior
3. implementation drift

No implementation-driving section may blur those three classes together.

## Current code-backed authority

Current code-backed authority means:

- the behavior is proven by current ScratchBird source or tests
- the canonical text is allowed to describe it as current implementation truth

## Required reconstructed behavior

Required reconstructed behavior means:

- the original product intent remains authoritative
- the specification must govern implementation even if current code is partial,
  permissive, or missing

Canonical rule:

- required reconstructed behavior is not optional future wishlist
- it is authoritative product behavior that implementation must meet

## Implementation drift

Implementation drift means:

- current code does not yet meet canonical required behavior
- current code is permissive where canon is stricter
- current code omits part of the canonical surface

Implementation drift must be recorded in planning, tickets, gates, or conformance
notes. It must not be hidden by weakening canon.

## Refusal rule

The rebuild must refuse both of the following:

1. weakening canon just because current code is incomplete
2. pretending required reconstructed behavior is already fully shipped

## Product-safety rule

For correctness, security, durability, and authorization surfaces:

- permissive stub behavior in code is always drift
- stricter canonical behavior remains authoritative

## Parser and optimizer rule

This rebuild rule applies directly to:

- parser-local lowering isolation
- index-family optimizer parity
- MGA-first durability and locking
- manager-owned cluster control
- security-definer, RLS, masking, and shared-rights behavior

If current code is weaker than canon on those lanes, the weakness is drift.

## Completion rule

A section reaches implementation-ready rebuild closure only when:

1. current code-backed behavior is captured
2. required reconstructed behavior is captured
3. remaining drift is explicit and bounded
4. another implementation agent can act without guessing which class governs
