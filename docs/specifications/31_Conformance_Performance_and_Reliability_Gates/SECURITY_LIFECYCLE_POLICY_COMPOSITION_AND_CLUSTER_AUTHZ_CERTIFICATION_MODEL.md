Status: reconstructed_required

# Security Lifecycle Policy Composition and Cluster Authz Certification Model

## Purpose

This document defines the certification evidence required for security-object lifecycle, policy composition, and cluster-authz propagation.

## Required Certification Classes

Certification shall cover:

- user, role, and group lifecycle transitions
- home-schema binding on principal activation
- row, column, and domain policy composition
- shared-right bundle activation
- cluster propagation of identity or authz objects
- cluster-authz drift detection and refusal of sensitive remote mutation where required

## Required Evidence

Each certification case shall preserve:

- security-object UUID
- lifecycle state before and after change
- home-schema context where applicable
- policy composition stages consulted
- cluster propagation generation where applicable
- resulting allow, deny, drift, or refusal state

## Failure Criteria

Certification fails when:

- lifecycle state changes are not auditable
- home-schema binding changes effective context without explicit evidence
- policy composition order cannot be reconstructed
- propagated authz drift is undetectable or ignored for sensitive operations

## Non-Guarantees

This file does not require all cluster-authz features to be enabled in every deployment. It defines the certification target for the recovered model.
