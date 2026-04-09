# Schema Visibility And Translation Matrix

## Purpose
Define parser-facing schema visibility, name resolution, parser catalog caching, and request translation behavior for native and emulated parsers.

## Scope
- visibility rules per parser target
- name-to-UUID and UUID-to-name translation
- full committed catalog snapshot and committed catalog delta helpers
- `CREATE DATABASE` semantic transformations
- service-channel and catalog-surface visibility constraints

## Boundary invariants
1. Parsers parse and transform requests; engine executes `SBLR` and UUID operations only.
2. Parsers must resolve identifiers to UUID before `SBLR` emission.
3. Engine responses are mapped back to parser dialect formats by parser adapters.
4. Emulated parsers are strict subsets of native capabilities.
5. ScratchBird is always in a transaction; parser cache synchronization therefore occurs at transaction boundaries, not at arbitrary statement boundaries.

## Name resolution contract

### Ingress resolution (`name -> uuid`)
- Input tuple: `(schema_path_or_search_path, object_type, identifier, language_id)`
- Lookup source: canonical name registry and object catalog
- Output: object UUID and normalized type token

### Egress resolution (`uuid -> display name`)
- Input tuple: `(object_uuid, language_id, parser_target)`
- Output:
  - preferred translated name for session language when available
  - fallback to default object name otherwise

### Stability rule
- Dependency edges always store UUIDs.
- Rename operations update the name registry only; dependent objects remain valid.

## Canonical parser-assist catalog helper contract

Canonical internal helper names:
- `sb_catalog_resolve_name_to_uuid`
- `sb_catalog_resolve_uuid_to_path_name`
- `sb_catalog_snapshot_begin`
- `sb_catalog_delta_since_anchor`

Rules:
1. The first two helpers are point-resolution helpers and are authoritative for current transaction-local truth.
2. The last two helpers are bulk cache helpers and are required so parsers can reduce repetitive name-lookup traffic.
3. `sb_catalog_snapshot_begin` returns the full committed ScratchBird object catalog to the parser process, including canonical full path and associated UUID for each active object row.
4. `sb_catalog_delta_since_anchor` returns the committed delta needed to move a parser cache from one committed transaction baseline to the next.
5. `sb_catalog_delta_since_anchor` must return `reset_required = true` rather than partial fiction when incremental advancement is not provable.

## Bulk helper usage model

1. On parser bootstrap or cache miss, call `sb_catalog_snapshot_begin`.
2. Cache the returned `anchor_schema_epoch_uuid` as the parser committed baseline.
3. On each new transaction baseline, call `sb_catalog_delta_since_anchor` unless a successful autocommit response already carried the same canonical delta.
4. Inside a still-open transaction, if the parser has executed `DDL` that can change name binding or rendering, use point helpers rather than trusting the previous bulk cache image.
5. Bulk helper output is an optimization aid; it never authorizes parsers to bypass point-resolution truth when transaction-local changes exist.
6. If `reset_required = true`, drop the bulk cache and reacquire it through `sb_catalog_snapshot_begin` before the next bind.

## Parser visibility matrix

| Parser Target | Visible Root Branches | Default Working Schema | Hidden Branches |
| --- | --- | --- | --- |
| native | `root.sys`, `root.users`, `root.remote`, `root.local`, `root.nosql` | `root.users.public` | only policy or ACL hidden surfaces |
| firebird | `root.remote.emulation.firebird.<server>.databases.<db>` plus required overlays | dialect profile default | all non-firebird emulation branches and raw `root.sys.*` internals |
| postgresql | `root.remote.emulation.postgresql.<server>.databases.<db>` plus `pg_catalog` overlays | dialect profile default search path | all non-postgresql emulation branches and raw `root.sys.*` internals |
| mysql | `root.remote.emulation.mysql.<server>.databases.<db>` plus `information_schema` overlays | dialect profile default database schema | all non-mysql emulation branches and raw `root.sys.*` internals |

## Unqualified name resolution
1. Parser obtains the resolved ordered search path from session profile.
2. Parser resolves each unqualified name by scanning schema entries in order.
3. First match wins.
4. Fully qualified paths bypass search-path scanning.
5. Child objects such as indexes, triggers, and constraints must resolve in parent-object scope.
6. The bulk cache used for scan order may be seeded from `sb_catalog_snapshot_begin` and refreshed from `sb_catalog_delta_since_anchor`, but current transaction-local `DDL` must still be honored via point lookup.

## Mandatory request metadata to engine
Each parser request to engine must include:
1. `parser_target`
2. `parser_profile_id`
3. `parser_profile_version`
4. `session_language_id`
5. `resolved_object_map`
6. `operator_map`
7. `original_request_payload`
8. `normalized_request_payload`
9. `schema_epoch_uuid`

## Mandatory response metadata from engine
Engine must return enough metadata for parser rendering:
1. result-column object UUIDs where applicable
2. type UUID or type-token metadata
3. server diagnostic ids
4. row-level hidden system metadata when requested by parser feature profile
5. optional committed parser delta payload for successful autocommit statements

## Negative requirements
1. Parser must not bypass UUID resolution and inject name strings as object identity in `SBLR`.
2. Parser must not expose unsupported features as silent no-op.
3. Parser must not surface hidden schema branches via error-message leakage.
4. Parser must not advance bulk-cache anchors after failed statements.
