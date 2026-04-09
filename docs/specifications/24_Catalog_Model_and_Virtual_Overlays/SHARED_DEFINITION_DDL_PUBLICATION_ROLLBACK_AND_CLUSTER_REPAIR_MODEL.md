Status: reconstructed_required

# Shared Definition DDL Publication Rollback and Cluster Repair Model

## Purpose

This document defines how DDL affecting cluster-shared definition objects is published, rolled back, and repaired across the cluster.

## Canonical Rule

Domains, events, custom error messages, and other cluster-shared definition objects follow ordinary MGA transaction rules for creation, alteration, and retirement. Cluster-common publication occurs only on commit.

## Covered Shared Definitions

The canonical shared-definition DDL classes are:

- domain create or alter or drop
- event create or alter or drop
- custom error definition create or alter or drop

## Publication Rule

For shared-definition DDL:

1. parse and lower to UUID-bound shared-definition intent
2. stage uncommitted metadata changes in the current transaction
3. publish the new shared-definition generation only on commit
4. record the resulting cluster-common generation and definition digest

## Rollback Rule

If the current transaction rolls back:

- uncommitted shared-definition changes are retired
- the prior committed shared-definition generation remains authoritative
- no node may observe the rolled-back generation as the active cluster-common truth

## Cluster Repair Rule

If a node diverges from the committed cluster-common generation:

- the divergence shall be classified explicitly
- the shared-definition object may be quarantined for mobility or activation purposes
- repair shall re-establish the committed UUID and definition, not invent a replacement identity

## Non-Guarantees

This file does not require all repair actions to be automatic. It requires commit-bound publication, rollback correctness, and explicit repair semantics.
