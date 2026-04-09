# Migration Orchestration Inspection and Heartbeat Boundary

## Scope

This file defines the operational boundary between:

- listener runtime
- controller path
- manager heartbeat lane
- migration inspection and status aggregation

## Canonical control split

The migration orchestration lane must preserve this ownership split:

1. the database and controller path remain authoritative for listener topology
   and policy
2. the listener remains local and bounded
3. the manager is the correct home for server-local heartbeat publication and
   remote migration inspection
4. migration audit and weak-donor status may be surfaced through listener-owned
   status only where the listener is reporting its own local state

Canonical rule:

- the listener must not fabricate end-to-end donor or cutover readiness on its
  own
- combined migration readiness is an aggregated management view

## Inspection boundary

The management lane must remain able to distinguish:

- listener local readiness
- parser pool readiness
- donor capability class
- extraction mode class
- unresolved drift class
- cutover readiness class
- heartbeat freshness

If these are shown through one status surface, each class must still remain
distinct in the payload or row contract.

## Heartbeat rule

Manager heartbeat is the authoritative cluster-facing heartbeat lane for
migration inspection.

Canonical rule:

- migration readiness must eventually be reportable through the manager
  heartbeat and control plane
- the listener is not promoted into the cluster heartbeat bus

## Reconstructed-required behavior

The lost-spec rebuild requires:

- manager-visible migration inspection
- deterministic status aggregation
- explicit weak-donor and cutover readiness classes

If current code does not yet ship the complete end-to-end runtime, that is
implementation drift and not permission to weaken the boundary.
