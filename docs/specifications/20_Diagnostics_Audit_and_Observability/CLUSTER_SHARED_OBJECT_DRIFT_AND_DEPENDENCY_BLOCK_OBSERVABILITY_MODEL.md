Status: reconstructed_required

# Cluster Shared Object Drift and Dependency Block Observability Model

## Purpose

This document defines the observability contract for drift and mobility blockers involving cluster-shared definition objects.

## Canonical Rule

Cluster-shared definition drift is a first-class operational state. The system shall publish it explicitly rather than surfacing it only as opaque migration or activation failures.

## Required Observability Fields

The observability surface shall expose:

- shared-definition object class
- shared-definition UUID
- drift class
- affected node set
- dependent object count
- active mobility blocker count
- oldest unresolved blocker age

## Blocker Rule

When a dependent object cannot move or activate because of shared-definition mismatch, observability shall expose:

- dependent object identity
- target node identity
- blocking shared-definition UUID
- blocker class

## Non-Guarantees

This file does not require one metrics backend. It requires the drift and blocker fields to be observable.
