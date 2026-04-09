Status: reconstructed_required

# Row Level Column Level Domain Policy Composition and Runtime Binding Model

## Purpose

This document defines how row-level, column-level, and domain-level policies compose and bind at runtime after principal resolution.

## Canonical Rule

Policy composition is explicit and runtime-bound. The security model does not evaluate row, column, and domain policy as unrelated ad hoc checks; it evaluates them as ordered stages bound to the current principal, wrapper context if any, and operation class.

## Runtime Binding Inputs

The canonical binding inputs are:

- bound principal identity
- active roles and groups
- shared-right bundles
- wrapper or sandbox mediation state
- object identity
- requested privilege class

## Composition Rule

Policy composition shall preserve the canonical order:

1. row-level policy
2. column-level policy
3. domain-level policy
4. masking transformation where still applicable

Each stage consumes the earlier stage’s admitted scope and may narrow it further.

## Binding Rule

At runtime, the composed policy bundle shall bind to:

- one operation class
- one principal context
- one object or object-set context

It shall not float independently of the request that caused it.

## Conflict Rule

If policy objects within the same stage conflict, the deterministic precedence model applies. If later stages would otherwise expose data denied earlier, the earlier denial remains authoritative.

## Non-Guarantees

This file does not define a single policy syntax. It defines the runtime composition and binding model that any admitted policy syntax must obey.
