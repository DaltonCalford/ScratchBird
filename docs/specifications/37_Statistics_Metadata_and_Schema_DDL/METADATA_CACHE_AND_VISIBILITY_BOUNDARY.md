# Metadata Cache and Visibility Boundary

## Canonical truth

The canonical metadata truth is:
- committed durable catalog rows
- committed canonical object-definition rows
- committed dependency rows
- committed schema-epoch rows
- committed security-policy epoch rows

Caches, overlays, plan artifacts, parser-assist mirrors, and statistics are derived views only.

## Visibility rules

### Same transaction
The owning transaction may observe its own uncommitted metadata and `DDL` through its transaction-local overlay.

### Other transactions
Other transactions must observe only committed metadata anchored to their transaction-visible committed schema epoch and committed security epoch.

### After commit or rollback
Because ScratchBird is always in a transaction:
- after `COMMIT`, the next transaction starts from the newly committed schema and security anchors
- after `ROLLBACK`, the next transaction starts from the prior committed anchors

## Cache rules

A metadata cache may exist for performance, but it must obey these rules:
- cache entries are keyed by committed schema epoch, committed security epoch when security-sensitive, and object identity
- use-time validation is mandatory before bind or execution when either anchor may have changed
- stale entries must be retired, not patched in place with guessed updates
- cache lag is tolerated only until next validation point; stale cache is never authoritative

## Permission cache rule

Current code-backed permission caching already keys answers against:
- global security policy epoch
- table policy epoch

Canon rule:
- permission and discoverability caches must validate against both anchors where relevant
- a matching schema epoch does not authorize reuse of stale security answers

## Parser-assist helper boundary

The parser helper surface has two classes of data:
- committed bulk state from `sb_catalog_snapshot_begin` and `sb_catalog_delta_since_anchor`
- exact current-transaction point resolution from `sb_catalog_resolve_name_to_uuid` and `sb_catalog_resolve_uuid_to_path_name`

Required separation:
- bulk helpers expose committed state only
- point helpers may surface current-transaction overlay for the owning session
- a parser must not assume that a committed bulk cache already contains its own uncommitted local `DDL`
