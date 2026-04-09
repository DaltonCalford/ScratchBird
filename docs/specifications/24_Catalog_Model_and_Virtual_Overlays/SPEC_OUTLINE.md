# Section 24 Specification Outline

## Objective

Define the implementation-ready catalog and virtual-overlay contract so persisted catalog families, overlay registration, startup publication, and schema visibility can be implemented without inferring ownership from adjacent runtime sections.

## Authority split

1. Persistent catalog authority
- `CatalogManager` owns the current persisted row-family surface.

2. Virtual overlay authority
- `VirtualCatalogRouter`, `InformationSchemaHandler`, `SysCatalogHandler`, and engine-specific handlers own current virtual overlay exposure.

3. Startup and install authority
- `Database` startup owns current virtual catalog initialization.
- `CharsetLoader` and `TimezoneLoader` own current proven resource bootstrap inputs.

## Active implementation files

- `CATALOG_TABLE_INVENTORY.md`
- `CATALOG_TABLE_SCHEMA_CORE_OBJECTS.md`
- `CATALOG_TABLE_SCHEMA_ENGINE_SPECIFIC.md`
- `CATALOG_TABLE_SCHEMA_RUNTIME_CONTEXT.md`
- `SYSTEM_OBJECT_VISIBILITY_AND_INSTALLATION.md`
- `SCHEMA_BOOTSTRAP_ORDER_AND_INVARIANTS.md`
- `SCHEMA_CHANGE_AND_DDL_PUBLICATION_STATE_MACHINE.md`
- `DDL_BEHAVIOR_MATRIX_AND_METADATA_LOCK_POLICY.md`
- `CATALOG_INVALIDATION_DEPENDENCY_GRAPH_AND_SELF_CHECK.md`
- `METADATA_AND_STATISTICS_SUBSYSTEM_CONTRACT.md`

## Explicit unsupported or separately owned surfaces

- branch and changeset object model
- exhaustive donor-engine parity claims
- universal resource-bundle lifecycle claims
- deeper runtime semantics owned by security, replication, migration, cluster, or executor sections
