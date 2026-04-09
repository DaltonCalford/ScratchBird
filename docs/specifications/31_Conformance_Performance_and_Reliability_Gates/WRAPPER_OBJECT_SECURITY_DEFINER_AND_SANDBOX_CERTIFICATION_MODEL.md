Status: reconstructed_required

# Wrapper Object Security Definer and Sandbox Certification Model

## Purpose

This document defines the certification evidence required for wrapper-object mediated security behavior.

## Required Certification Classes

Certification shall cover:

- view-mediated access with direct underlying-object deny retained
- procedure-mediated access with `EXECUTE` on wrapper only
- function-mediated access with direct underlying-object deny retained
- package-member mediated access with wrapper-scoped permission only
- invoker-context wrapper behavior
- security-definer wrapper behavior
- sandbox-mediated wrapper behavior

## Required Evidence

Each certification case shall preserve:

- caller principal
- wrapper object class and identity
- wrapper security mode
- underlying object set
- direct underlying-object rights state
- final outcome
- audit-chain evidence

## Failure Criteria

Certification fails when:

- wrapper success implies unstated direct underlying-object rights
- the runtime cannot distinguish invoker-context from security-definer behavior
- the audit chain cannot explain why access succeeded or failed

## Non-Guarantees

This file does not require all wrapper classes to exist in every deployment. It defines the certification target where they do.
