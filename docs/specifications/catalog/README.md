# System Catalog Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains system catalog specifications for ScratchBird's metadata management and schema resolution.

## Overview

The system catalog stores all database metadata including tables, columns, indexes, constraints, users, roles, and permissions. ScratchBird's catalog design supports multi-dialect operation, schema path resolution, and component-based architecture.

## Specifications in this Directory

- **[SYSTEM_CATALOG_STRUCTURE.md](SYSTEM_CATALOG_STRUCTURE.md)** - Complete system catalog table structure
- **[SCHEMA_PATH_RESOLUTION.md](SCHEMA_PATH_RESOLUTION.md)** - Schema search path and name resolution
- **[SCHEMA_PATH_SECURITY_DEFAULTS.md](SCHEMA_PATH_SECURITY_DEFAULTS.md)** - Schema path security defaults
- **[COMPONENT_MODEL_AND_RESPONSIBILITIES.md](COMPONENT_MODEL_AND_RESPONSIBILITIES.md)** (155 lines) - Component architecture and responsibilities
- **[CATALOG_CORRECTION_PLAN.md](CATALOG_CORRECTION_PLAN.md)** - Catalog structure correction and migration plan

## Key Concepts

### System Catalog Tables

Core catalog tables (all in `sys` schema):

#### Object Definitions
- **sys.databases** - Database definitions
- **sys.schemas** - Schema definitions
- **sys.tables** - Table definitions
- **sys.columns** - Column definitions
- **sys.indexes** - Index definitions
- **sys.constraints** - Constraint definitions
- **sys.sequences** - Sequence generators
- **sys.views** - View definitions
- **sys.triggers** - Trigger definitions
- **sys.procedures** - Stored procedure definitions
- **sys.functions** - Function definitions
- **sys.packages** - Package definitions

#### Security & Access Control
- **sys.users** - User accounts
- **sys.roles** - Role definitions
- **sys.role_members** - Role membership
- **sys.permissions** - Object permissions
- **sys.row_level_security** - RLS policies

System view definitions and SQL behavior for users/roles are specified in
`ddl/DDL_ROLES_AND_GROUPS.md`.

#### Advanced Features
- **sys.domains** - User-defined domains
- **sys.types** - User-defined types
- **sys.tablespaces** - Tablespace definitions
- **sys.foreign_servers** - Foreign data wrapper servers
- **sys.foreign_tables** - Foreign table definitions

### Schema Path Resolution

See [SCHEMA_PATH_RESOLUTION.md](SCHEMA_PATH_RESOLUTION.md) for:

- **Search path** - PostgreSQL-compatible schema search path
- **Name resolution** - Unqualified name lookup algorithm
- **Dialect-specific behavior** - Different resolution rules per dialect

### Component Model

See [COMPONENT_MODEL_AND_RESPONSIBILITIES.md](COMPONENT_MODEL_AND_RESPONSIBILITIES.md) for:

- **CatalogManager** - Central catalog coordinator
- **SchemaResolver** - Name resolution and lookup
- **PermissionManager** - Access control enforcement
- **MetadataCache** - Catalog caching layer

## Catalog Bootstrap

The system catalog is self-describing:

1. **Bootstrap phase** - Hardcoded catalog tables created
2. **System schema** - `sys` schema populated with metadata
3. **Standard schemas** - `public`, `information_schema` created
4. **Emulation schemas** - `pg_catalog`, `mysql`, etc. for dialect emulation

## Multi-Dialect Support

The catalog supports multiple SQL dialects:

- **PostgreSQL** - `pg_catalog` system views
- **MySQL** - `information_schema` and `mysql` schema
- **Firebird** - `RDB$` system tables
- **MSSQL** - `sys` catalog views (post-gold)

All dialect-specific views map to the core `sys` catalog tables.

## Related Specifications

- [Parser](../parser/) - Schema-qualified name parsing
- [DDL Operations](../ddl/) - Catalog modification statements
- [Security](../Security%20Design%20Specification/) - Permission checking
- [Types System](../types/) - Type definitions and domains

## Critical Reading

Before working on catalog implementation:

1. **MUST READ:** [../../../MGA_RULES.md](../../../MGA_RULES.md) - MGA architecture rules
2. **MUST READ:** [../../../IMPLEMENTATION_STANDARDS.md](../../../IMPLEMENTATION_STANDARDS.md) - Implementation standards
3. **READ IN ORDER:**
   - [SYSTEM_CATALOG_STRUCTURE.md](SYSTEM_CATALOG_STRUCTURE.md) - Catalog structure
   - [SCHEMA_PATH_RESOLUTION.md](SCHEMA_PATH_RESOLUTION.md) - Name resolution
   - [COMPONENT_MODEL_AND_RESPONSIBILITIES.md](COMPONENT_MODEL_AND_RESPONSIBILITIES.md) - Architecture

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
