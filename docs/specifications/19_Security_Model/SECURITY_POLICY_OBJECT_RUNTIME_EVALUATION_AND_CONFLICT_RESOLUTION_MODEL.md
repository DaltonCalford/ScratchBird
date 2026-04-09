Status: reconstructed_required

# Security Policy Object Runtime Evaluation and Conflict Resolution Model

## Purpose

This document defines how multiple security-policy objects combine at runtime and how conflicting outcomes are resolved.

## Canonical Rule

When multiple security-policy objects apply to the same request, the runtime shall evaluate them in deterministic order and resolve conflicts by explicit precedence rules rather than by incidental implementation order.

## Policy Object Classes

The policy classes covered here include:

- row policies
- column policies
- domain policies
- masking policies
- security-definer or sandbox grants
- shared-right overlays

## Evaluation Rule

The runtime shall evaluate policy objects according to the canonical order already defined for authorization and masking. Within the same stage, it shall preserve deterministic object ordering using explicit precedence and stable identity.

## Conflict Rule

If policies within the same stage disagree:

- explicit deny beats allow unless the stage’s contract says otherwise
- masking cannot turn a deny into an allow
- sandbox scope cannot elevate direct underlying-object rights
- shared-right overlays cannot bypass stricter row, column, or domain policy

## Audit Rule

The runtime shall be able to explain:

- which policy objects were consulted
- what each policy object decided
- which precedence rule resolved the conflict
- why the final result was allow, deny, or mask

## Non-Guarantees

This file does not prescribe one policy language. It prescribes deterministic runtime conflict resolution.
