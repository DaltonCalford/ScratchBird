Status: reconstructed_required

# Shared Definition DDL Publication and Row Identity Continuity Certification Model

## Purpose

This document defines the certification evidence required for cluster-shared definition DDL publication and row-identity continuity across recovery workflows.

## Required Certification Classes

Certification shall cover:

- committed publication of a shared-definition DDL change
- rollback of an uncommitted shared-definition DDL change
- drift detection after divergent node state
- repair restoring cluster-common UUID and definition
- export or restore preserving logical row UUID continuity
- export or restore preserving row UUID alias-column binding

## Required Evidence

Each certification case shall preserve:

- shared-definition UUID or logical row UUID
- object or row identity class
- committed generation before and after
- rollback or repair outcome where applicable
- continuity outcome for export or restore where applicable

## Failure Criteria

Certification fails when:

- a rolled-back shared-definition generation appears as committed cluster truth
- repair changes shared-definition UUID instead of restoring the committed identity
- export or restore creates a second logical row UUID for the same logical row
- row UUID alias binding is lost or duplicated

## Non-Guarantees

This file does not require every deployment to enable all workflows simultaneously. It defines the certification target for the recovered identity and publication model.
