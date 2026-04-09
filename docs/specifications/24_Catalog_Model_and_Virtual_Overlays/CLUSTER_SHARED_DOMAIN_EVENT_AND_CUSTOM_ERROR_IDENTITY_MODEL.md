Status: reconstructed_required

# Cluster Shared Domain Event and Custom Error Identity Model

## Purpose

This document defines the canonical identity and sharing rules for domains, events, custom error messages, and other object classes whose design requires common cross-node definition.

## Canonical Rule

Any object class that must be common across cluster nodes shall preserve:

- the same canonical definition
- the same UUID identity
- the same semantic interpretation

across every participating node where that object is present.

## Required Shared Object Classes

The canonical shared-definition classes include at minimum:

- domains
- events
- custom error messages

Additional object classes may join this set only when their design explicitly requires cross-node sameness of definition and identity.

## Domain Rule

Domains are first-class shared-definition objects.

Required effects:

- a domain has an immutable system UUID
- the same logical domain uses the same UUID across the cluster
- the same logical domain preserves the same canonical definition across the cluster
- tables, procedures, functions, packages, and other objects that depend on the domain may move between nodes without reinterpreting the domain

## Event Rule

Events are cluster-common signal definitions.

Required effects:

- the same logical event uses the same UUID across the cluster
- event name changes do not redefine event identity
- event semantics remain tied to the shared UUID-backed definition

## Custom Error Message Rule

Custom error messages are cluster-common error-definition objects.

Required effects:

- the same logical custom error message uses the same UUID across the cluster
- the same logical custom error message preserves the same canonical definition across the cluster
- node movement or execution relocation does not remap the error-definition identity

## Publication Rule

Shared-definition objects become authoritative only through committed metadata publication. Node-local drafts or partial updates shall not masquerade as the cluster-common definition.

## Drift Rule

If a node holds a different definition or different UUID for what should be the same shared-definition object, that state is cluster metadata drift and must be classified, reported, and repaired before dependent object movement is considered safe.

## Dependency Rule

Objects depending on shared-definition objects shall bind by UUID-backed identity, not just by user-visible name. This ensures cross-node mobility for:

- tables using custom domains
- procedures or functions using custom domains
- packages using shared events or custom errors
- any admitted object class depending on a cluster-common definition

## Non-Guarantees

This file does not require every metadata object to be cluster-shared. It defines the rule for object classes whose design requires common cross-node identity and definition.
