# Encryption Key Shard and Cluster Identity Catalog Model

## Scope

This file defines the persisted cluster identity and encryption-key-shard substrate already present in the current code base.

This file is authoritative for:

- persisted database-local cluster identity fields
- persisted encryption key shard catalog rows
- the catalog-side custody model for split or partitioned cluster secrets

## Database header cluster identity

The current on-disk database header already persists:

- `database_uuid`
- `cluster_id`
- `node_id`
- `cluster_config_epoch`

### Current code-backed behavior

The unit tests prove:

- a newly created standalone database defaults to all-zero `cluster_id`
- a newly created standalone database defaults to all-zero `node_id`
- a newly created standalone database defaults to `cluster_config_epoch = 0`
- `set_cluster_identity(cluster_id, node_id, cluster_config_epoch)` persists across restart

### Canonical meaning

Database-local cluster identity is not merely catalog decoration. It is durable page-zero identity and therefore part of restart, shadow, backup, and cluster-admission truth.

`cluster_config_epoch` is authoritative persistent identity-generation state for cluster membership and routing configuration.

## Encryption key shard catalog

### Purpose

The encryption-key-shard family is the persisted custody ledger for partitioned encryption material held across holders.

This is the canonical persisted basis for the reconstructed cluster shared-secret model where encryption or decryption authority is distributed in partial snippets or shards.

### Current code-backed row fields

The current catalog manager persists:

- `shard_id`
- `key_id`
- `shard_index`
- `shard_total`
- `shard_material_encrypted_id`
- `holder_identity`
- `created_time`
- optional `last_collected_time`

### Admission rules

The current code enforces:

- `shard_id` is required
- `key_id` is required
- `shard_material_encrypted_id` is required
- `shard_total` must be greater than zero
- `shard_index` must be strictly less than `shard_total`
- `holder_identity` must be non-empty and fit storage constraints
- there can be at most one valid row for a given `(key_id, shard_index)`

### Retrieval and listing

The current code exposes:

- get by `shard_id`
- list by `key_id`
- soft-delete semantics through validity flagging

### Canonical meaning

This row family is the persisted shard-custody ledger. It does not by itself prove that current code already performs full threshold reconstruction or cluster-wide automated secret assembly. It proves the persisted custody substrate exists and must remain the canonical home for that reconstruction.

## Current code-backed versus reconstructed-required behavior

### Current code-backed

The current code proves:

- durable page-zero cluster identity
- durable cluster config epoch
- durable key-shard custody rows
- shard uniqueness and bounded indexing
- holder identity persistence

### Required reconstructed behavior

The rebuilt cluster security model must use this persisted substrate for:

- shared encryption or decryption password partitioning
- holder inventory
- collection evidence
- quorum or threshold reconstruction workflow

### Drift rule

No later spec may move cluster shared-secret custody into transient process memory or untracked external text files as the primary source of truth. External secret managers may participate, but the canonical cluster custody model must remain representable against this persisted row family and page-zero identity.

## MGA boundary

These identity and shard rows are ordinary MGA-governed database state:

- transaction-scoped
- restart-visible by durable page and row state
- not recovered by replay of an external WAL
