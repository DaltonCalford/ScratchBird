Status: reconstructed_required

# Shared User Rights Cluster Propagation and Authz Drift Model

## Purpose

This document defines how shared users, roles, groups, and rights propagate across cluster-managed targets and how authorization drift is detected.

## Canonical Rule

Shared user and rights state propagated through the cluster layer shall remain attributable to its source grants and auditable across targets. Remote propagation does not create implicit local grants outside the admitted propagation record.

## Propagated Objects

The canonical propagated objects are:

- shared users
- shared roles
- shared groups
- membership bindings
- grant bundles
- revocation bundles

## Propagation Rule

Propagation shall preserve:

- source identity
- target set
- grant or revoke intent
- version or generation marker
- local apply state per target

## Local Authorization Rule

Each target shall still resolve effective authorization locally using the canonical user, role, group, and sandbox resolution order. Cluster propagation provides inputs to that resolution; it does not bypass it.

## Drift Rule

Authorization drift exists when:

- a target has stale propagated rights data
- a target applied only part of a grant or revoke bundle
- target-local effective rights differ from cluster-intended shared rights

## Repair Rule

Repair shall preserve:

- which grant sources are trusted
- whether the cluster view or the local view is authoritative for repair
- whether the target must be quarantined from further propagation until corrected

## Security Rule

Changes to shared users or shared rights are security-sensitive remote-management operations. They require mutation privilege distinct from inspection privilege.

## Non-Guarantees

This file does not require that every current deployment already uses cluster-shared identities. It defines the canonical model for the feature where admitted.
