Status: reconstructed_required

# Logical Row UUID Alias and Cluster Tracking Certification Model

## Purpose

This document defines the certification evidence required for logical row UUID generation, alias binding, and cluster tracking.

## Required Certification Classes

Certification shall cover:

- row creation with generated system row UUID
- update creating new lineage while preserving logical row UUID
- table with no user-visible row UUID alias
- table with user-visible UUID row-identity alias column
- cluster movement or placement tracking preserving the logical row UUID

## Required Evidence

Each certification case shall preserve:

- logical row UUID
- row alias-column state
- version-lineage transition
- cluster placement identity where applicable
- final outcome

## Failure Criteria

Certification fails when:

- an update creates a second logical row UUID for the same logical row
- alias-column binding produces a second competing UUID identity
- cluster tracking loses the logical row UUID across placement change

## Non-Guarantees

This file does not require every deployment to expose row UUIDs directly to end users. It defines the certification target for the recovered identity model.
