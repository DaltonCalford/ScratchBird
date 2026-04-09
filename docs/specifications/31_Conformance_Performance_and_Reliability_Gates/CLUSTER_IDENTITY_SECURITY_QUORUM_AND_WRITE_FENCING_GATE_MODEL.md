# Cluster Identity, Security Quorum, and Write Fencing Gate Model

## Scope

This file defines the certification model for:

- persisted cluster identity survival
- cluster-config epoch persistence
- key-shard custody correctness
- security-quorum enforcement
- leader-term and routing-epoch write fencing

## Current code-backed authority

Current code-backed recovery proves:

1. page-zero cluster identity survives restart
2. key-shard custody rows already exist with uniqueness constraints
3. stale leader term is rejected
4. wrong leader identity is rejected
5. stale routing epoch is rejected
6. security-quorum runtime already distinguishes deny and bounded-cache posture

## Gate objective

The gate must prove that cluster identity, security quorum, and write fencing
remain fail-closed and restart-stable.

## Required evidence families

The gate must retain, at minimum:

- run identity
- pre-restart identity snapshot
- post-restart identity snapshot
- key-shard custody summary
- quorum-decision summary
- write-fence refusal summary by class
- routing-epoch refusal summary

## Pass criteria

The gate passes only if:

1. cluster identity persists across restart without unintended mutation
2. standalone zero-identity posture remains explicitly distinguishable from
   cluster-bound posture
3. duplicate or inconsistent shard custody is rejected
4. stale leader-term writes are rejected
5. stale routing-epoch writes are rejected
6. required security-quorum posture is enforced without silent fail-open

## Fail-closed rules

The gate fails if:

1. cluster identity becomes ambiguous after restart
2. stale writer authority is accepted
3. security-quorum deny posture is bypassed without an explicit canonical
   bounded-cache rule
4. required evidence families are missing

## Reconstructed-required behavior

The rebuild requires later promotion of:

- stronger executed gate artifacts for quorum-sensitive cluster lanes
- tighter correlation between key-shard custody evidence and security-quorum
  outputs
- stronger operator-facing summaries that align with sections `20`, `24`,
  `25`, and `42`
