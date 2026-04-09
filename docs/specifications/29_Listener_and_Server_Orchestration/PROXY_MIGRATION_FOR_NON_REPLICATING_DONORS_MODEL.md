Status: reconstructed_required

# Proxy Migration for Non-Replicating Donors Model

## Purpose

This document defines the reconstructed required model for using the ScratchBird listener, optional manager, and migration proxy surfaces to move data from donor systems that do not expose native replication.

## Canonical Topology

The canonical topology is:

1. optional manager as the guarded front-door
2. listener runtime for the donor dialect
3. donor-dialect parser performing dialect-local SQL to SBLR lowering when needed
4. migration proxy or extraction controller
5. target-side ScratchBird engine applying committed MGA state

## Separation of Responsibilities

The manager owns:

- admission
- remote control
- heartbeat publication
- deployment policy

The listener owns:

- protocol termination for the donor dialect
- validated handoff to parser workers
- listener-local status

The parser owns:

- donor-dialect parsing
- donor-dialect-local lowering to SBLR
- no dependency on any other parser

The migration controller owns:

- snapshot orchestration
- delta replay ordering
- cutover fence tracking
- divergence assessment

## Proxy Observation Modes

The proxy lane shall support these reconstructed modes:

- passive read-only snapshot extraction
- observed DML capture through proxied sessions
- administrative fence and final-sync mode
- donor quiesce then target promotion

## Non-Replicating Donor Rule

Where the donor lacks natural replication, the listener or proxy path shall not pretend to offer a true replication feed. It shall instead publish one of the weak-donor extraction modes from section 39 and surface the uncertainty class end to end.

## Control Path Rule

Administrative control for proxy migration shall flow through:

- admin SQL or native management surfaces
- engine-owned management records
- manager or controller dispatch
- listener-local runtime control over the bounded IPC seam

The listener shall not become an autonomous migration authority.

## Required Runtime Status

The listener or proxy migration lane shall expose:

- donor identity
- extraction mode
- current batch or replay position
- backlog depth
- uncertainty class
- cutover-ready state
- divergence-scan state

## Fail-Closed Rules

Automatic cutover shall be refused when:

- uncertainty class is `UNBOUNDED`
- divergence scan is required and not complete
- replay ordering cannot be proven
- the manager policy requires quiesce and donor activity is still observed

## Security Boundary

Migration inspection and migration mutation are distinct privileges. A principal allowed to inspect status is not automatically allowed to initiate cutover, clear quarantine, or override uncertainty-class fences.

## Non-Guarantees

This file is reconstructed required canon. It does not claim every controller and proxy orchestration surface is already fully shipped in the current codebase.
