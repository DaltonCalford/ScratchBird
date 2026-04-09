Status: reconstructed_required

# Shared Definition Dependency and Mobility Blocker Row Family Model

## Purpose

This document defines the catalog-backed row families used to track dependencies on cluster-shared definition objects and to record mobility blockers when those dependencies are misaligned.

## Canonical Rule

Node mobility and remote activation depend on explicit metadata proving that dependent objects reference cluster-shared definitions by UUID and that any drift or mismatch is recorded as a blocker.

## Canonical Row Families

The canonical row families are:

- shared-definition dependency rows
- dependency verification rows
- mobility blocker rows
- drift classification rows

## Shared-Definition Dependency Rows

Each dependency row shall preserve:

- dependent object UUID
- dependent object class
- shared-definition UUID
- shared-definition class
- dependency role
- active generation

## Mobility Blocker Rows

Each mobility blocker row shall preserve:

- dependent object UUID
- target node identity
- blocking shared-definition UUID
- blocker class
- blocker creation time
- current blocker state

## Blocker Classes

The canonical blocker classes are:

- missing shared-definition object
- UUID mismatch
- definition mismatch
- unresolved cluster drift

## Publication Rule

Mobility blockers become authoritative only through committed metadata publication. Node-local transient warnings do not replace committed blocker state when the system has already classified a dependency as unsafe.

## Non-Guarantees

This file does not require every cluster to move objects between nodes. It defines the metadata substrate required where mobility is admitted.
