Status: reconstructed_required

# Row UUID and Shared Definition Cluster Inspection Operator Model

## Purpose

This document defines the operator-facing inspection model for row UUID identity, row UUID aliasing, and shared-definition dependency state across the cluster.

## Canonical Rule

Operators shall be able to inspect:

- row UUID identity behavior
- row UUID alias-column binding
- shared-definition dependency state
- remote-execution or mobility blockers

without inferring these only from failed operations.

## Required Inspection Fields

The inspection surface shall preserve:

- object identity
- logical row UUID or alias-binding state where relevant
- shared-definition UUID dependencies
- drift or blocker state
- target node identity where remote execution or movement is in scope

## Non-Guarantees

This file does not require one specific tool syntax. It defines the operator-visible information contract.
