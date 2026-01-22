# Driver Metadata Query Contract (Native)

Version: 1.0
Status: Draft
Last Updated: January 2026

## Purpose

Provide a stable, server-supported metadata query contract for native
ScratchBird drivers. These queries are the baseline for JDBC/ODBC metadata
and for language drivers that expose schema introspection.

## References

- `docs/specifications/catalog/README.md`
- `docs/specifications/catalog/SCHEMA_PATH_RESOLUTION.md`
- `docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`

## Required sys.* Views

The server must expose the following sys.* views with stable column sets.
These views may map to internal catalog tables as needed.

1. sys.schemas
   - schema_id, schema_name, owner_id, default_tablespace_id, is_valid

2. sys.tables
   - table_id, schema_id, table_name, table_type, owner_id, is_valid

3. sys.columns
   - column_id, table_id, column_name, data_type_id, ordinal_position,
     is_nullable, default_value, is_valid

4. sys.indexes
   - index_id, table_id, index_name, index_type, is_unique, is_valid

5. sys.constraints
   - constraint_id, table_id, constraint_name, constraint_type, is_valid

6. sys.procedures
   - procedure_id, schema_id, procedure_name, routine_type, is_valid

7. sys.functions
   - function_id, schema_id, function_name, is_valid

## Schema/Search Path Behavior

- Drivers must be able to set a default schema or search path.
- The server must honor SET SCHEMA or SET SEARCH_PATH for native sessions.

## JDBC/ODBC Mapping

The server must allow drivers to map sys.* views into standard metadata
schemas (DatabaseMetaData, INFORMATION_SCHEMA) without requiring emulation
ports or protocol translation.

