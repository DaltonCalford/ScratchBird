# Cluster Shared Secret Sharding and Unlock Quorum Model

## Status

reconstructed_required_with_current_substrate

## Purpose

This document defines how cluster-shared encryption and authentication secrets are represented, sharded, unlocked, and governed across the cluster.

## Core Rule

Cluster-shared secret material must not be modeled as one monolithic plaintext secret replicated everywhere.

Instead, ScratchBird canonically uses:

- encryption profiles
- encryption keys
- encrypted key shards
- unlock policy
- minimum shard count
- security quorum behavior

This is the required cluster-secret model.

## Encryption Profile Catalog

Current encryption profiles define at least:

- profile id and name
- cipher
- KDF algorithm
- KDF parameter reference
- key rotation policy
- minimum shards required
- unlock timeout
- active flag

This means shard/quorum behavior is policy-backed, not ad hoc.

## Encryption Key and Key-Shard Catalog

Current encryption-key rows define:

- key id
- profile id
- key kind
- key status
- encrypted key-material id
- key-material hash
- key version
- lifecycle timestamps

Current encryption-key-shard rows define:

- shard id
- key id
- shard index
- shard total
- encrypted shard-material id
- holder identity
- creation and collection timestamps

This is explicit proof that key material is already modeled in shard form.

## Bootstrap Unlock Catalog

Current encryption bootstrap rows define:

- database id
- profile id
- active key id
- minimum shards required
- unlock timeout
- unlock policy
- last unlock time
- last unlock result
- policy version

The bootstrap surface is therefore designed for controlled unlock and rotation, not static one-time initialization only.

## Canonical Secret-Sharing Requirement

The required cluster behavior is:

- cluster encryption or decryption secrets may be split across holders
- no single holder is required to possess the whole logical secret
- unlock succeeds only when the policy-defined minimum shard threshold is met
- shard material remains encrypted at rest

This is the authoritative design direction even where some runtime collection lanes may still be partial.

## Holder Identity Model

Each stored shard has a holder identity.

Canonical holder identities may represent:

- nodes
- operators
- services
- escrow authorities
- other approved cluster principals

The specification does not require one specific identity namespace, but it does require that every shard be attributable to a specific holder identity.

## Unlock Policy Rule

`unlock_policy` is a first-class bootstrap field and must control how shard collection and unlock are allowed.

At minimum, unlock policy governs:

- whether local-only unlock is permitted
- whether remote quorum participation is required
- timeout behavior
- refusal behavior when quorum is missing

The field must not exist only as decorative metadata.

## Security Quorum Runtime

The current `SecurityQuorum` runtime provides:

- required count
- total count
- failure mode
- optional status provider

Current failure modes are:

- `FAIL_OPEN`
- `FAIL_CLOSED`
- `REQUIRE_REMOTE`

Current decisions are:

- `ALLOW_CACHE`
- `BYPASS_CACHE`
- `DENY`

## Canonical Meaning of Quorum Decisions

The required semantic meaning is:

- `ALLOW_CACHE`: quorum conditions are sufficiently met for normal cached or local continuation
- `BYPASS_CACHE`: strict quorum is not met, but the configured failure mode permits a narrower, non-cached or degraded path
- `DENY`: the operation must fail closed

This is especially important for cluster-shared key and secret access.

## Shared Authentication and Cluster Secret Linkage

Cluster security metadata already includes fields such as:

- security tier
- key rotation policy
- auth-secret UUID linkage on selected alert/cluster surfaces

The canonical rule is that shared user and cluster rights behavior may reference shared secret material, but:

- authentication proof
- authorization proof
- secret unlock proof

remain distinct layers.

## Rotation Rule

Key rotation policy is part of the encryption profile and must be treated as authoritative lifecycle policy.

Shard-based key rotation must preserve:

- old/new key lifecycle distinction
- active key identity
- minimum shard threshold for the new active key
- refusal to activate a key that cannot satisfy security tier and quorum requirements

## Partial-Implementation Boundary

Current code proves:

- sharded key catalog structures
- bootstrap unlock policy structures
- security-quorum runtime with fail-open/fail-closed/require-remote behavior

Current code read in this pass does not fully prove the complete live cluster-wide shard collection and unlock orchestration bus.

Therefore this document is reconstructed required behavior with substantial
current substrate and partial runtime implementation drift.
