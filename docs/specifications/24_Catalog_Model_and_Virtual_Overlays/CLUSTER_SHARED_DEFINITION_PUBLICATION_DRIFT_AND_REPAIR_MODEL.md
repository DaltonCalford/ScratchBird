Status: reconstructed_required

# Cluster Shared Definition Publication Drift and Repair Model

## Purpose

This document defines how cluster-shared definitions are published, how drift is classified, and how repair is recorded.

## Canonical Rule

Cluster-shared definitions become authoritative only through committed publication of the shared canonical definition. Divergent node-local copies are drift states that require explicit repair classification.

## Publication Rule

For domains, events, custom error messages, and other cluster-shared definitions:

- committed publication establishes the active shared definition
- the shared UUID and canonical definition become the cluster-common truth
- node-local tentative changes do not override the shared definition until committed

## Drift Classes

The canonical drift classes are:

- missing on node
- UUID mismatch
- definition mismatch
- stale generation

## Repair Rule

Repair shall preserve:

- shared-definition UUID
- prior drift class
- source of repair truth
- repaired node set
- resulting generation

## Non-Guarantees

This file does not require automatic repair for every drift class. It requires explicit publication, drift classification, and repair recording.
