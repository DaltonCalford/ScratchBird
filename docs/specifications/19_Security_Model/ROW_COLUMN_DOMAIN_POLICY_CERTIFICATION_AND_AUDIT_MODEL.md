Status: reconstructed_required

# Row Column Domain Policy Certification and Audit Model

## Purpose

This document defines the certification and audit evidence required for row-level, column-level, domain-level, and masking policy behavior.

## Canonical Rule

Security policy enforcement is not complete until it is auditable and certifiable. The engine shall be able to prove which policy stage admitted, transformed, or refused access.

## Required Certification Classes

Certification shall cover:

- row deny
- row allow
- column visibility deny
- column mutation deny
- domain policy refusal
- masking transform
- security-definer or sandboxed execution with underlying-object denial preserved

## Required Evidence

Each certification record shall preserve:

- principal identity
- effective authorization inputs
- row-policy result
- column-policy result
- domain-policy result
- masking result
- final outcome

## Audit Rule

Operational audit shall preserve enough information to explain:

- why a row disappeared
- why a column was hidden
- why a value was masked
- why a domain write was refused
- whether a sandboxed object mediated access

## Failure Criteria

Certification fails when:

- policy order cannot be reconstructed
- a masking transform occurs after a deny should have blocked access
- sandbox mediation cannot be distinguished from direct-object rights

## Non-Guarantees

This file does not define the policy language itself. It defines the proof and audit obligations for the policy engine.
