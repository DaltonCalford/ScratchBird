Status: reconstructed_required

# Shared Definition Publication Rollback and Repair Observability Model

## Purpose

This document defines the observability contract for publication, rollback, drift, and repair of cluster-shared definition objects.

## Canonical Rule

Operators shall be able to observe:

- the active committed shared-definition generation
- pending uncommitted changes where admitted by the session or admin surface
- rollback outcomes for abandoned changes
- drift and repair state after publication divergence

## Required Observability Fields

The observability surface shall preserve:

- shared-definition UUID
- shared-definition class
- active committed generation
- current drift class if any
- last repair generation if any
- last rollback of a pending change if recorded

## Repair Rule

When repair occurs, observability shall expose:

- repair source of truth
- nodes affected
- resulting generation
- whether blockers were cleared

## Non-Guarantees

This file does not require every rollback to be broadcast as a general event stream. It requires that publication, rollback, drift, and repair outcomes be inspectable.
