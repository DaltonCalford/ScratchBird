Status: reconstructed_required

# Security Definer Procedure Function and Package Execution Context Model

## Purpose

This document defines the canonical execution context for procedures, functions, and packages that run under security-definer or sandbox-mediated rules.

## Canonical Rule

Procedures, functions, and packages may execute under a mediated security context distinct from the caller’s direct object privileges. That mediated context is explicit, bounded, and auditable.

## Execution Context Inputs

The mediated execution context shall preserve:

- caller principal identity
- wrapper object identity
- wrapper owner or defining principal
- wrapper security mode
- underlying object set admitted by the wrapper
- caller direct-rights state for those underlying objects

## Security Modes

The canonical wrapper security modes are:

- invoker-context
- security-definer
- schema-sandbox-mediated

## Security-Definer Rule

In security-definer mode:

- the caller must still have `EXECUTE` on the wrapper object
- the wrapper may access underlying objects through the wrapper’s admitted rights chain
- the caller does not gain direct rights to those underlying objects

## Sandbox Rule

In schema-sandbox-mediated mode:

- wrapper-defined access may be broader than the caller’s direct object rights
- access remains limited to the wrapper’s admitted contract
- the wrapper may not silently elevate unrelated object access beyond that contract

## Transaction Rule

Wrapper execution remains in the caller’s current transaction context. Security mediation does not create a separate transaction, separate visibility horizon, or separate commit authority.

## Non-Guarantees

This file does not require every procedure, function, or package to be security-definer. It defines the canonical mediated-context model where that mode is admitted.
