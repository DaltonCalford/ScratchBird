# Metadata and Statistics Subsystem Contract

## Canonical ownership model

The authoritative metadata surfaces are:
- durable system catalog rows for object identity and object attributes
- canonical object-definition rows written through `setObjectDefinition(...)`
- dependency rows written through `replaceDependencies(...)` or `clearDependenciesFor(...)`
- schema-epoch rows appended through `appendSchemaEpochCatalogEntry(...)`
- schema-change plan, event, backfill-progress, and cutover-guard rows used by
  the bounded Beta 1 online-schema-change model
- security-policy epoch rows for security publication anchors
- publication catalog rows for publication-specific durable metadata
- runtime transaction rows for in-flight and terminal transaction evidence

Statistics are derived aids. They are not canonical identity or publication truth.

## Transaction and visibility model

Metadata follows the same model as all other engine state:
- ScratchBird is always inside a transaction
- `DDL` and `DML` are both transaction-scoped
- only committed metadata is globally published
- uncommitted metadata is visible only inside the owning transaction context
- `COMMIT` and `ROLLBACK` both immediately begin the next transaction
- autocommit means a successful statement is followed by commit; statement error leaves the current transaction active and does not advance committed metadata anchors

Metadata publication and parser cache synchronization are MGA-governed visibility operations. They are not WAL-, redo-, or LSN-governed operations.

## Security metadata publication model

Security metadata publication has two committed epoch classes:
- schema publication epoch
- security publication epoch

Current code-backed security epoch anchors include:
- global security policy epoch
- per-table policy epoch

Rules:
1. schema-changing `DDL` publishes through committed schema epochs
2. security-changing `DDL` and security catalog mutation publish through committed security epochs
3. permission caches, session security state, and security-aware metadata mirrors must validate against security epoch anchors, not schema epoch alone
4. a cache answer is invalid if its schema epoch matches but its security epoch is stale

## Parser-assist catalog helper contract

The parser-assist layer owns four read-only helper families.

Canonical exported helper names:
- `sb_catalog_resolve_name_to_uuid`
- `sb_catalog_resolve_uuid_to_path_name`
- `sb_catalog_snapshot_begin`
- `sb_catalog_delta_since_anchor`

The first two are point-resolution helpers. The latter two are committed-baseline bulk cache helpers.

## Security-aware payload rule

Metadata consumers that use helper payloads or cache rows for authorization-sensitive behavior must carry enough anchor state to distinguish:
- schema publication anchor
- security publication anchor
- current transaction-local overlay source when applicable

## Helper invocation rule for security-sensitive consumers

Parsers, planners, and metadata mirrors that rely on grants, policy visibility, or security-sensitive discoverability must:
1. honor schema-epoch cache invalidation
2. honor security-epoch cache invalidation
3. refuse to reuse cached security answers when either anchor is stale or unproven

## Stable helper result codes

All four helpers use the same stable result vocabulary.
- `SB_CATALOG_OK`
- `SB_CATALOG_OBJECT_NOT_FOUND`
- `SB_CATALOG_UUID_NOT_FOUND`
- `SB_CATALOG_PATH_AMBIGUOUS`
- `SB_CATALOG_KIND_FILTER_INVALID`
- `SB_CATALOG_ANCHOR_UNKNOWN_RESET_REQUIRED`
- `SB_CATALOG_ANCHOR_STALE_RESET_REQUIRED`
- `SB_CATALOG_VISIBILITY_UNPROVEN`
- `SB_CATALOG_INTERNAL_CONTRACT_ERROR`
