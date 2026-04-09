Status: reconstructed_required

# Security Definer Object Audit Chain and Underlying Object Deny Model

## Purpose

This document defines how security-definer or schema-sandboxed objects preserve auditability while underlying-object direct access remains denied.

## Canonical Rule

Execution through a security-definer or sandboxed object shall produce an audit chain that proves:

- the caller was allowed to invoke the wrapper object
- the wrapper object mediated access
- direct underlying-object rights were not silently granted

## Required Audit Chain

The audit chain shall preserve:

- caller principal
- invoked wrapper object
- wrapper owner or grant chain
- underlying object set consulted
- direct underlying-object deny state for the caller
- final allow or deny result

## Deny Preservation Rule

If the caller lacks direct rights to an underlying object, that deny remains true before, during, and after wrapper invocation. The wrapper only mediates the specific admitted access path.

## Conflict Rule

If wrapper rights and direct-object deny appear to conflict, the audit output shall show that:

- wrapper invocation was allowed
- direct underlying-object access remained denied

## Non-Guarantees

This file does not require every wrapper object to be security-definer. It defines the audit and deny-preservation contract for those that are.
