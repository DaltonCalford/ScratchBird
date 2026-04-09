Status: reconstructed_required

# DDL Lowering and Remote Execution UUID Identity Certification Model

## Purpose

This document defines the certification evidence required for row UUID alias lowering, shared-definition dependency lowering, and remote execution preconditions.

## Required Certification Classes

Certification shall cover:

- DDL lowering of a row UUID alias column
- DDL lowering of a domain dependency to shared-definition UUID
- routine or package dependency on shared event or custom error UUID
- remote execution admitted with aligned shared-definition dependencies
- remote execution refused with drift or blocker state

## Required Evidence

Each certification case shall preserve:

- emitted canonical UUID dependencies
- row UUID alias binding state if present
- target-node dependency verification result
- final execution or activation outcome

## Failure Criteria

Certification fails when:

- DDL lowering loses UUID dependency identity
- row UUID aliasing produces ambiguous row identity
- remote execution succeeds despite unresolved shared-definition mismatch

## Non-Guarantees

This file does not require every runtime to enable remote execution today. It defines the certification target where that capability exists.
