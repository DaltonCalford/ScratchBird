Status: reconstructed_required_with_current_substrate

# Cluster Secret Shard Distribution Rotation and Unseal Model

## Purpose

This document defines the reconstructed required model for cluster-held partial secret shards, quorum-based unseal, rotation, and auditability.

## Canonical Rule

Cluster encryption and related high-value secrets shall never be stored as one recoverable cleartext blob on a single ordinary node. Cluster policy shall distribute partial secret material across multiple trust domains and require quorum to reconstruct or authorize unseal.

## Object Model

The canonical model consists of:

- cluster identity
- secret family
- secret version
- shard set
- quorum policy
- escrow or recovery policy
- unseal session
- rotation event

## Shard Rule

Each shard is only a partial secret fragment plus policy metadata. A shard alone is non-authoritative and non-usable.

## Quorum Policy

Each secret family shall define a policy with:

- total shard count
- minimum quorum
- allowed holder classes
- expiry or forced-rotation conditions
- audit requirements

## Holder Classes

Allowed shard holders may include:

- server-local manager custody
- cluster control-plane custody
- administrative recovery escrow
- hardware-backed secure storage where available

The exact holder mix is policy-defined. No single ordinary runtime process may hold a full reconstructable secret outside an active unseal boundary.

## Unseal Lifecycle

The canonical unseal lifecycle is:

1. request unseal for a named secret family and version
2. validate caller privilege and policy
3. gather quorum evidence from shard holders
4. reconstruct or authorize in-memory unseal material
5. publish a bounded unseal session
6. use the session only for the admitted cryptographic operations
7. retire the unseal session and zero or release transient material

## Rotation Rules

Rotation shall:

- create a new secret version
- create a new shard set under the new version
- preserve the ability to read prior data according to retention policy
- record rotation provenance and approvers
- retire old versions only after policy conditions are satisfied

## Distribution Rules

Shard distribution shall prefer fault-domain separation. Multiple shards from the same version shall not be co-located in a way that defeats quorum assumptions unless the policy explicitly records and accepts that degraded posture.

## Audit Requirements

The engine and cluster plane shall audit:

- shard issuance
- shard relocation
- quorum request
- unseal granted
- unseal refused
- rotation initiated
- rotation committed
- shard holder degraded or unavailable

## Failure Rules

Loss of derivative audit or archive lanes does not destroy cluster secret truth. Loss of quorum availability may block new unseal sessions, but it shall not rewrite prior committed data or silently degrade encryption posture.

## Non-Guarantees

This file does not prescribe one cryptographic shard algorithm. The required invariants are partial distribution, quorum-based unseal, rotation traceability, and fail-closed refusal when quorum policy is not met.
