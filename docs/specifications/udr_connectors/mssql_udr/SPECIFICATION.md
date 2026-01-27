# MSSQL UDR Specification

Status: Draft (Beta). This specification defines the MSSQL UDR that connects
to SQL Server using the native TDS wire protocol (no external client library).

## Scope
- Supported SQL Server versions: 2016 - 2022, Azure SQL.
- Uses TDS 7.x protocol (LOGIN7, SQL Batch, RPC).
- Supports pass-through DDL/DML/PSQL via sys.remote_exec/remote_query.
- Supports schema discovery + mounting for foreign tables and migration.

## References
- ../UDR_CONNECTOR_BASELINE.md
- ../../Alpha Phase 2/11-Remote-Database-UDR-Specification.md
- ../../Alpha Phase 2/11d-MSSQL-Client-Implementation.md

## UDR Module
- Library: libscratchbird_mssql_udr.so
- FDW handler: mssql_udr_handler
- FDW validator: mssql_udr_validator
- Capabilities: network

## Connection and Authentication
- TLS: required when available; verify server cert if verify enabled
- Auth methods: SQL auth, Windows/Kerberos, Azure AD (where supported)
- Session defaults: SET NOCOUNT ON, read committed snapshot support

## Server Options

| Option | Default | Description |
| --- | --- | --- |
| host | required | Server host (allowlisted) |
| port | 1433 | Server port |
| database | required | Database name |
| encrypt | true | TLS on/off |
| connect_timeout | 5000 | ms |
| query_timeout | 30000 | ms |
| allow_ddl | false | Allow CREATE/ALTER/DROP |
| allow_dml | true | Allow INSERT/UPDATE/DELETE |
| allow_psql | false | Allow EXEC/CALL |
| allow_passthrough | false | Allow sys.remote_exec/query |

## Metadata Discovery (Required)
Use system catalogs for schema analysis:
- sys.databases, sys.schemas
- sys.tables, sys.views, sys.columns
- sys.indexes, sys.index_columns
- sys.key_constraints, sys.foreign_keys
- sys.objects (routines, triggers)

### Introspection Examples (SQL Server)
```sql
-- Schemas
SELECT schema_id, name
FROM sys.schemas;

-- Tables / views
SELECT t.object_id, s.name AS schema_name, t.name, t.type
FROM sys.objects t
JOIN sys.schemas s ON s.schema_id = t.schema_id
WHERE t.type IN ('U','V');

-- Columns
SELECT c.object_id, c.column_id, c.name, ty.name AS type_name,
       c.max_length, c.precision, c.scale, c.is_nullable, c.is_identity
FROM sys.columns c
JOIN sys.types ty ON ty.user_type_id = c.user_type_id
WHERE c.object_id = OBJECT_ID(?);

-- Primary/unique constraints
SELECT kc.name, kc.type, kc.parent_object_id, ic.index_id, ic.key_ordinal, c.name AS column_name
FROM sys.key_constraints kc
JOIN sys.index_columns ic ON ic.object_id = kc.parent_object_id AND ic.index_id = kc.unique_index_id
JOIN sys.columns c ON c.object_id = ic.object_id AND c.column_id = ic.column_id
WHERE kc.parent_object_id = OBJECT_ID(?);

-- Foreign keys
SELECT fk.name, fk.parent_object_id, fk.referenced_object_id, fkc.parent_column_id, fkc.referenced_column_id
FROM sys.foreign_keys fk
JOIN sys.foreign_key_columns fkc ON fkc.constraint_object_id = fk.object_id
WHERE fk.parent_object_id = OBJECT_ID(?);

-- Indexes
SELECT i.name, i.is_unique, i.type_desc, ic.key_ordinal, c.name AS column_name
FROM sys.indexes i
JOIN sys.index_columns ic ON ic.object_id = i.object_id AND ic.index_id = i.index_id
JOIN sys.columns c ON c.object_id = ic.object_id AND c.column_id = ic.column_id
WHERE i.object_id = OBJECT_ID(?) AND i.is_hypothetical = 0;

-- Routines (procedures + functions)
SELECT o.object_id, s.name AS schema_name, o.name, o.type
FROM sys.objects o
JOIN sys.schemas s ON s.schema_id = o.schema_id
WHERE o.type IN ('P','FN','IF','TF');

-- Triggers
SELECT t.object_id, t.name, t.parent_id
FROM sys.triggers t
WHERE t.parent_class_desc = 'OBJECT_OR_COLUMN';
```

If a category is unavailable (e.g., limited permissions), report it as
`not_supported` and continue.

## Schema Mounting
- Database maps to catalog; schema maps to schema (default `dbo`).
- Imported schemas are mounted under `legacy_<server>` (or `mount_root`
  override) per the Remote Database UDR spec.

## Pass-through and Migration
- Pass-through: sys.remote_exec, sys.remote_query, sys.remote_call
- Migration: optional and explicit; uses the migration state machine defined in
  UDR_CONNECTOR_BASELINE.md and the Remote Database UDR spec.

## Testing Checklist
- TLS + LOGIN7 authentication flow.
- Schema discovery for tables/views/indexes.
- Pass-through denied when allow_passthrough=false.
