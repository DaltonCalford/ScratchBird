Status: reconstructed_required

# Cluster Shared Identity and Authz Propagation Runtime Model

## Purpose

This document defines the runtime model for cluster-shared user, role, group, and shared-right propagation.

## Canonical Rule

Cluster propagation of identity and authorization objects is runtime-managed but remains subordinate to UUID-backed object identity, lifecycle state, and local effective-authz resolution.

## Covered Propagated Objects

The canonical propagated object classes are:

- user
- role
- group
- shared-right bundle
- membership binding
- privilege grant or revoke bundle

## Runtime Propagation Fields

The runtime shall preserve:

- object UUID
- object class
- propagation generation
- target node set
- local apply state per node
- drift state if the target diverges

## Activation Rule

Propagation does not itself finalize effective authorization. Each target still performs canonical local binding and policy evaluation after the propagated objects are committed locally.

## Drift Rule

If a node has stale, missing, or conflicting propagated identity or authz state, that state is cluster-authz drift and must be published and, where required, block sensitive remote operations.

## Non-Guarantees

This file does not require every cluster to share every identity object. It defines the runtime model where shared identity and authz propagation is enabled.
