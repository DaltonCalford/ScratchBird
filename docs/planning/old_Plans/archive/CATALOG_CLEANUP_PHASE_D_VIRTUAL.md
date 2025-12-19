# Catalog Cleanup Phase D: Virtual Catalog Infrastructure

**Created:** November 26, 2025
**Priority:** MEDIUM (Foundation for Phase 2 Wire Protocols)
**Estimated Effort:** 35-50 hours
**Prerequisites:** Phase A, B, C complete
**Status:** ✅ COMPLETE (November 26, 2025)

---

## Overview

This phase implements the virtual catalog infrastructure required for Alpha Phase 2 wire protocol integration. Virtual catalogs allow ScratchBird to present protocol-specific system views while maintaining a single internal catalog.

**Key Design Principle:** Emulated system tables (RDB$*, pg_catalog.*, mysql.*, sys.*) are **created on-demand** when an emulation server/database is configured. They are implemented as **views** that query the internal ScratchBird catalog and transform results to match the emulated format.

Reference:
- `05-Wire-Protocol-Integration-Specification.md`
- `SCHEMA_ARCHITECTURE.md` - Hierarchical schema design

---

## On-Demand Emulation Architecture

Emulated system catalogs are NOT pre-created. They are generated dynamically:

```
User creates emulated server:
  CREATE EMULATED SERVER firebird_local TYPE 'firebird' HOST 'localhost' PORT 3050;

  System creates: /remote/emulated/firebird/localhost/
  (EmulationServerInfo record)

User connects to database:
  CONNECT TO firebird_local DATABASE 'employee';

  System creates:
    - /remote/emulated/firebird/localhost/employee/  (database schema)
    - EmulatedDatabaseInfo record
    - RDB$RELATIONS view → maps to sys.catalog.tables
    - RDB$FIELDS view → maps to sys.catalog.columns
    - RDB$INDICES view → maps to sys.catalog.indexes
    - RDB$PROCEDURES view → maps to sys.catalog.procedures
    - etc...
```

---

## Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│                         Schema Hierarchy                                  │
├──────────────────────────────────────────────────────────────────────────┤
│ / (root)                                                                 │
│ ├── sys/                        # System management (real tables)        │
│ │   ├── catalog/                # Core catalog tables                    │
│ │   ├── security/               # Users, roles, permissions              │
│ │   └── config/                 # Configuration                          │
│ │                                                                        │
│ ├── remote/                                                              │
│ │   └── emulated/               # ON-DEMAND emulation schemas            │
│ │       ├── firebird/                                                    │
│ │       │   └── localhost/      # Server (created on CREATE SERVER)      │
│ │       │       └── employee/   # Database (created on CONNECT)          │
│ │       │           ├── RDB$RELATIONS    ← VIEW to sys.catalog.tables   │
│ │       │           ├── RDB$FIELDS       ← VIEW to sys.catalog.columns  │
│ │       │           └── RDB$INDICES      ← VIEW to sys.catalog.indexes  │
│ │       │                                                                │
│ │       ├── postgresql/                                                  │
│ │       │   └── pgserver/                                                │
│ │       │       └── mydb/                                                │
│ │       │           └── pg_catalog/                                      │
│ │       │               ├── pg_class      ← VIEW                        │
│ │       │               └── pg_attribute  ← VIEW                        │
│ │       │                                                                │
│ │       └── mysql/                                                       │
│ │           └── mysqlserver/                                             │
│ │               └── shop/                                                │
│ │                   └── mysql/                                           │
│ │                       ├── user          ← VIEW                        │
│ │                       └── db            ← VIEW                        │
│ │                                                                        │
│ └── public/                     # Default public schema                  │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## Task List

### D-1: Virtual Catalog Interface Design (4-6 hours)

Design the core virtual catalog abstraction.

**File:** `include/scratchbird/catalog/virtual_catalog.h`

```cpp
#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include <string>
#include <vector>
#include <memory>

namespace scratchbird::catalog {

// Virtual catalog row (key-value pairs)
struct VirtualRow {
    std::vector<std::pair<std::string, TypedValue>> columns;
};

// Virtual catalog result set
struct VirtualResultSet {
    std::vector<std::string> column_names;
    std::vector<uint16_t> column_types;  // DataType enum
    std::vector<VirtualRow> rows;
};

// Protocol type for catalog translation
enum class ProtocolType : uint8_t {
    SCRATCHBIRD = 0,  // Native ScratchBird
    POSTGRESQL = 1,
    MYSQL = 2,
    MSSQL = 3,
    FIREBIRD = 4
};

// Virtual catalog handler interface
class VirtualCatalogHandler {
public:
    virtual ~VirtualCatalogHandler() = default;

    // Check if this handler owns the given schema
    virtual bool ownsSchema(const std::string& schema_name) const = 0;

    // Check if this handler owns the given table
    virtual bool ownsTable(const std::string& schema_name,
                           const std::string& table_name) const = 0;

    // Execute a query against a virtual table
    virtual core::Status queryTable(const std::string& schema_name,
                                    const std::string& table_name,
                                    const std::string& where_clause,
                                    VirtualResultSet& results,
                                    ErrorContext* ctx = nullptr) = 0;

    // Get column definitions for a virtual table
    virtual core::Status getTableColumns(const std::string& schema_name,
                                         const std::string& table_name,
                                         std::vector<core::CatalogManager::ColumnInfo>& columns,
                                         ErrorContext* ctx = nullptr) = 0;

    // List all virtual tables in a schema
    virtual core::Status listTables(const std::string& schema_name,
                                    std::vector<std::string>& table_names,
                                    ErrorContext* ctx = nullptr) = 0;

protected:
    core::CatalogManager* catalog_manager_ = nullptr;
};

// Virtual catalog router
class VirtualCatalogRouter {
public:
    static VirtualCatalogRouter& getInstance();

    // Register a handler for a protocol
    void registerHandler(ProtocolType protocol,
                         std::unique_ptr<VirtualCatalogHandler> handler);

    // Get handler for current session protocol
    VirtualCatalogHandler* getHandler(ProtocolType protocol);

    // Check if schema is virtual
    bool isVirtualSchema(const std::string& schema_name) const;

    // Route query to appropriate handler
    core::Status routeQuery(ProtocolType protocol,
                            const std::string& schema_name,
                            const std::string& table_name,
                            const std::string& where_clause,
                            VirtualResultSet& results,
                            ErrorContext* ctx = nullptr);

    // Initialize with catalog manager reference
    void initialize(core::CatalogManager* catalog);

private:
    VirtualCatalogRouter() = default;
    std::unordered_map<ProtocolType, std::unique_ptr<VirtualCatalogHandler>> handlers_;
    core::CatalogManager* catalog_manager_ = nullptr;
};

} // namespace scratchbird::catalog
```

---

### D-2: information_schema Implementation (8-10 hours)

Implement SQL standard information_schema views.

**File:** `include/scratchbird/catalog/information_schema.h`

**Views to Implement:**
| View | Description | Priority |
|------|-------------|----------|
| SCHEMATA | List of schemas | High |
| TABLES | List of tables | High |
| COLUMNS | Column definitions | High |
| TABLE_CONSTRAINTS | Constraints | High |
| KEY_COLUMN_USAGE | PK/FK columns | High |
| VIEWS | View definitions | Medium |
| ROUTINES | Functions/Procedures | Medium |
| PARAMETERS | Routine parameters | Medium |
| TRIGGERS | Trigger definitions | Medium |
| SEQUENCES | Sequence definitions | Medium |
| DOMAINS | Domain definitions | Low |
| USER_DEFINED_TYPES | UDTs | Low |

**Implementation Pattern:**
```cpp
class InformationSchemaHandler : public VirtualCatalogHandler {
public:
    InformationSchemaHandler(core::CatalogManager* catalog);

    bool ownsSchema(const std::string& schema_name) const override {
        return schema_name == "information_schema" ||
               schema_name == "INFORMATION_SCHEMA";
    }

    bool ownsTable(const std::string& schema_name,
                   const std::string& table_name) const override;

    core::Status queryTable(const std::string& schema_name,
                            const std::string& table_name,
                            const std::string& where_clause,
                            VirtualResultSet& results,
                            ErrorContext* ctx = nullptr) override;

private:
    // Individual table handlers
    core::Status querySchemata(const std::string& where_clause,
                               VirtualResultSet& results);
    core::Status queryTables(const std::string& where_clause,
                             VirtualResultSet& results);
    core::Status queryColumns(const std::string& where_clause,
                              VirtualResultSet& results);
    // ... etc for each view
};
```

**Example: COLUMNS view implementation:**
```cpp
core::Status InformationSchemaHandler::queryColumns(
    const std::string& where_clause,
    VirtualResultSet& results) {

    results.column_names = {
        "TABLE_CATALOG", "TABLE_SCHEMA", "TABLE_NAME", "COLUMN_NAME",
        "ORDINAL_POSITION", "COLUMN_DEFAULT", "IS_NULLABLE", "DATA_TYPE",
        "CHARACTER_MAXIMUM_LENGTH", "NUMERIC_PRECISION", "NUMERIC_SCALE"
    };

    results.column_types = {
        DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR,
        DataType::INT32, DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR,
        DataType::INT32, DataType::INT32, DataType::INT32
    };

    // Get all schemas
    std::vector<SchemaInfo> schemas;
    catalog_manager_->listSchemas(schemas);

    for (const auto& schema : schemas) {
        // Get all tables in schema
        std::vector<TableInfo> tables;
        catalog_manager_->listTables(schema.schema_id, tables);

        for (const auto& table : tables) {
            // Get all columns in table
            std::vector<ColumnInfo> columns;
            catalog_manager_->getColumns(table.table_id, columns);

            for (const auto& col : columns) {
                VirtualRow row;
                row.columns = {
                    {"TABLE_CATALOG", TypedValue::makeString("scratchbird")},
                    {"TABLE_SCHEMA", TypedValue::makeString(schema.schema_name)},
                    {"TABLE_NAME", TypedValue::makeString(table.table_name)},
                    {"COLUMN_NAME", TypedValue::makeString(col.column_name)},
                    {"ORDINAL_POSITION", TypedValue::makeInt32(col.ordinal)},
                    // ... etc
                };
                results.rows.push_back(row);
            }
        }
    }

    return Status::OK;
}
```

---

### D-3: PostgreSQL pg_catalog Handler (6-8 hours)

Implement PostgreSQL-compatible system catalog views.

**File:** `include/scratchbird/catalog/pg_catalog.h`

**Views to Implement:**
| View | Description | Priority |
|------|-------------|----------|
| pg_namespace | Schemas | High |
| pg_class | Tables/indexes/sequences | High |
| pg_attribute | Columns | High |
| pg_type | Data types | High |
| pg_index | Index info | High |
| pg_constraint | Constraints | High |
| pg_proc | Functions/procedures | Medium |
| pg_trigger | Triggers | Medium |
| pg_user | Users | Medium |
| pg_roles | Roles | Medium |
| pg_database | Databases | Low |
| pg_tablespace | Tablespaces | Low |

**Implementation:**
```cpp
class PgCatalogHandler : public VirtualCatalogHandler {
public:
    PgCatalogHandler(core::CatalogManager* catalog);

    bool ownsSchema(const std::string& schema_name) const override {
        return schema_name == "pg_catalog";
    }

    // Map ScratchBird catalog to PostgreSQL format
    core::Status queryPgClass(const std::string& where_clause,
                              VirtualResultSet& results);
    core::Status queryPgAttribute(const std::string& where_clause,
                                  VirtualResultSet& results);
    // ... etc
};
```

---

### D-4: MySQL mysql.* Handler (4-6 hours)

Implement MySQL-compatible system catalog views.

**File:** `include/scratchbird/catalog/mysql_catalog.h`

**Views to Implement:**
| View | Description |
|------|-------------|
| mysql.user | User accounts |
| mysql.db | Database privileges |
| mysql.tables_priv | Table privileges |
| mysql.columns_priv | Column privileges |
| mysql.proc | Stored procedures |
| mysql.event | Events |

---

### D-5: MSSQL sys.* Handler (4-6 hours)

Implement SQL Server-compatible system catalog views.

**File:** `include/scratchbird/catalog/mssql_catalog.h`

**Views to Implement:**
| View | Description |
|------|-------------|
| sys.schemas | Schemas |
| sys.tables | Tables |
| sys.columns | Columns |
| sys.indexes | Indexes |
| sys.types | Data types |
| sys.objects | All objects |
| sys.procedures | Stored procedures |
| sys.sql_modules | Module definitions |

---

### D-6: Query Router Integration (4-6 hours)

Integrate virtual catalog router with query execution.

**Changes to Query Execution:**
1. Before table lookup, check if schema is virtual
2. If virtual, route to appropriate handler
3. Convert VirtualResultSet to standard ResultSet
4. Continue with normal query execution

**Integration Points:**
- `src/core/query_executor.cpp` - Add virtual catalog check
- `src/sblr/executor.cpp` - Handle virtual tables in FROM clause
- `src/core/optimizer/query_planner.cpp` - Virtual table statistics

```cpp
// In query executor
Status QueryExecutor::resolveTable(const std::string& schema_name,
                                   const std::string& table_name,
                                   TableInfo& table_out) {
    // Check if virtual schema
    VirtualCatalogRouter& router = VirtualCatalogRouter::getInstance();
    if (router.isVirtualSchema(schema_name)) {
        // Return virtual table descriptor
        return router.getVirtualTableInfo(session_->protocol,
                                          schema_name, table_name,
                                          table_out);
    }

    // Normal catalog lookup
    return catalog_->getTable(schema_name, table_name, table_out);
}
```

---

### D-7: On-Demand Emulation View Generator (8-10 hours)

Generate protocol-specific system views when emulated database is connected.

**File:** `include/scratchbird/catalog/emulation_view_generator.h`

```cpp
namespace scratchbird::catalog {

// View template for emulated system tables
struct EmulatedViewDefinition {
    std::string view_name;           // e.g., "RDB$RELATIONS"
    std::string source_query;        // SQL that maps to internal catalog
    std::vector<std::string> columns;
};

// Emulation view generator - creates views on-demand
class EmulationViewGenerator {
public:
    EmulationViewGenerator(core::CatalogManager* catalog);

    // Generate all system views for an emulated database
    // Called when user connects to an emulated database
    core::Status generateEmulatedViews(const ID& database_schema_id,
                                       ProtocolType protocol,
                                       ErrorContext* ctx = nullptr);

    // Drop all emulated views (cleanup)
    core::Status dropEmulatedViews(const ID& database_schema_id,
                                   ErrorContext* ctx = nullptr);

private:
    // Protocol-specific view definitions
    std::vector<EmulatedViewDefinition> getFirebirdViews();
    std::vector<EmulatedViewDefinition> getPostgreSQLViews();
    std::vector<EmulatedViewDefinition> getMySQLViews();
    std::vector<EmulatedViewDefinition> getMSSQLViews();

    core::CatalogManager* catalog_;
};

} // namespace scratchbird::catalog
```

**Firebird RDB$ View Mappings:**
```sql
-- RDB$RELATIONS → Tables
CREATE VIEW remote.emulated.firebird.{server}.{db}.RDB$RELATIONS AS
SELECT
    t.table_name AS RDB$RELATION_NAME,
    t.table_id AS RDB$RELATION_ID,
    CASE t.is_system WHEN true THEN 1 ELSE 0 END AS RDB$SYSTEM_FLAG,
    s.schema_name AS RDB$OWNER_NAME,
    t.row_count AS RDB$RELATION_COUNTS
FROM sys.catalog.tables t
JOIN sys.catalog.schemas s ON t.schema_id = s.schema_id
WHERE t.schema_id = {emulated_db_schema_id};

-- RDB$FIELDS → Columns
CREATE VIEW remote.emulated.firebird.{server}.{db}.RDB$FIELDS AS
SELECT
    c.column_name AS RDB$FIELD_NAME,
    t.table_name AS RDB$RELATION_NAME,
    c.ordinal AS RDB$FIELD_POSITION,
    c.data_type AS RDB$FIELD_TYPE,
    c.max_length AS RDB$FIELD_LENGTH,
    c.precision AS RDB$FIELD_PRECISION,
    c.scale AS RDB$FIELD_SCALE,
    CASE c.nullable WHEN true THEN 1 ELSE 0 END AS RDB$NULL_FLAG
FROM sys.catalog.columns c
JOIN sys.catalog.tables t ON c.table_id = t.table_id
WHERE t.schema_id = {emulated_db_schema_id};

-- RDB$INDICES → Indexes
CREATE VIEW remote.emulated.firebird.{server}.{db}.RDB$INDICES AS
SELECT
    i.index_name AS RDB$INDEX_NAME,
    t.table_name AS RDB$RELATION_NAME,
    CASE i.is_unique WHEN true THEN 1 ELSE 0 END AS RDB$UNIQUE_FLAG,
    i.column_count AS RDB$SEGMENT_COUNT
FROM sys.catalog.indexes i
JOIN sys.catalog.tables t ON i.table_id = t.table_id
WHERE t.schema_id = {emulated_db_schema_id};
```

**PostgreSQL pg_catalog View Mappings:**
```sql
-- pg_class → Tables/Indexes/Sequences
CREATE VIEW remote.emulated.postgresql.{server}.{db}.pg_catalog.pg_class AS
SELECT
    t.table_id::int AS oid,
    t.table_name AS relname,
    s.schema_id::int AS relnamespace,
    'r'::char AS relkind,  -- r=table, i=index, S=sequence
    t.owner_id::int AS relowner
FROM sys.catalog.tables t
JOIN sys.catalog.schemas s ON t.schema_id = s.schema_id
WHERE t.schema_id = {emulated_db_schema_id};

-- pg_attribute → Columns
CREATE VIEW remote.emulated.postgresql.{server}.{db}.pg_catalog.pg_attribute AS
SELECT
    c.table_id::int AS attrelid,
    c.column_name AS attname,
    c.ordinal AS attnum,
    c.data_type AS atttypid,
    CASE c.nullable WHEN true THEN false ELSE true END AS attnotnull
FROM sys.catalog.columns c
JOIN sys.catalog.tables t ON c.table_id = t.table_id
WHERE t.schema_id = {emulated_db_schema_id};
```

---

## Virtual Schema Registration

On server startup, register core virtual schema handlers (information_schema):

```cpp
void initializeVirtualCatalogs(core::CatalogManager* catalog) {
    VirtualCatalogRouter& router = VirtualCatalogRouter::getInstance();
    router.initialize(catalog);

    // Register information_schema (shared by all protocols)
    // This is always available at /information_schema
    router.registerHandler(ProtocolType::SCRATCHBIRD,
        std::make_unique<InformationSchemaHandler>(catalog));
}
```

**Emulated protocol catalogs are NOT registered at startup.**
They are generated on-demand when:
1. `CREATE EMULATED SERVER` - creates server schema
2. `CONNECT TO {server} DATABASE {db}` - creates database schema + emulated views

---

## Checklist

### Implementation ✅ COMPLETE
- [x] D-1: Virtual catalog interface design
- [x] D-2: information_schema implementation (12 views)
- [x] D-3: pg_catalog view templates (12 views)
- [x] D-4: mysql.* view templates (6 views)
- [x] D-5: sys.* view templates (8 views)
- [x] D-6: Query router integration
- [x] D-7: On-demand emulation view generator (Firebird RDB$*)

### Testing
- [ ] information_schema queries return correct data (PENDING - needs integration test)
- [ ] CREATE EMULATED SERVER creates correct schema path (PENDING)
- [ ] CONNECT TO {server} DATABASE generates views correctly (PENDING)
- [ ] Emulated views query internal catalog correctly (PENDING)
- [ ] WHERE clause filtering works on emulated views (PENDING)

### Documentation
- [x] Document virtual catalog mapping (in header files)
- [x] Document on-demand view generation (in emulation_view_generator.h)
- [x] Update SCHEMA_ARCHITECTURE.md (already complete)

---

## Effort Summary

| Task | Est. Hours |
|------|-----------|
| D-1: Interface Design | 4-6 |
| D-2: information_schema | 8-10 |
| D-3: pg_catalog templates | 6-8 |
| D-4: mysql.* templates | 4-6 |
| D-5: sys.* templates | 4-6 |
| D-6: Router Integration | 4-6 |
| D-7: On-Demand View Generator | 8-10 |
| **Total** | **38-52 hours** |

---

## Phase 2 Integration Notes

This virtual catalog infrastructure provides the foundation for:
1. **Wire Protocol Integration** - Clients can query system catalogs in their native format
2. **On-Demand Emulation** - System tables only created when emulation is configured
3. **Views to Real Catalog** - Emulated tables are views, not duplicated data
4. **Tool Compatibility** - pgAdmin, MySQL Workbench, SSMS work without modification
5. **Hierarchical Schemas** - Full path support (remote.emulated.firebird.server.db.RDB$*)

---

## Related Documents

- `SCHEMA_ARCHITECTURE.md` - Hierarchical schema design
- `CATALOG_CLEANUP_PHASE_A_CRUD.md` - Emulation CRUD operations
- `CATALOG_CLEANUP_PHASE_B_STRUCTURES.md` - EmulationServerInfo, EmulatedDatabaseInfo

---

## Completion Summary

**Completed:** November 26, 2025

**Files Created:**
| File | Description | Lines |
|------|-------------|-------|
| `include/scratchbird/catalog/virtual_catalog.h` | Virtual catalog interface, router, data structures | ~460 |
| `include/scratchbird/catalog/information_schema.h` | SQL standard information_schema (12 views) | ~980 |
| `include/scratchbird/catalog/pg_catalog.h` | PostgreSQL pg_catalog emulation (12 views) | ~960 |
| `include/scratchbird/catalog/mysql_catalog.h` | MySQL mysql.* emulation (6 tables) | ~470 |
| `include/scratchbird/catalog/mssql_catalog.h` | SQL Server sys.* emulation (8 views) | ~780 |
| `include/scratchbird/catalog/emulation_view_generator.h` | On-demand emulation view generator | ~470 |
| `src/catalog/virtual_catalog.cpp` | Router implementation and initialization | ~170 |

**Total: ~4,290 lines of new code**

**Key Components:**
1. **VirtualCatalogHandler** - Abstract base class for protocol handlers
2. **VirtualCatalogRouter** - Singleton router for query dispatch
3. **InformationSchemaHandler** - SQL standard 12 views (SCHEMATA, TABLES, COLUMNS, etc.)
4. **PgCatalogHandler** - PostgreSQL 12 views (pg_class, pg_attribute, pg_type, etc.)
5. **MySQLCatalogHandler** - MySQL 6 tables (mysql.user, mysql.db, mysql.proc, etc.)
6. **MSSQLCatalogHandler** - SQL Server 8 views (sys.tables, sys.columns, sys.objects, etc.)
7. **EmulationViewGenerator** - On-demand Firebird RDB$* view generation

**Note:** Integration tests pending - requires wire protocol implementation (Alpha Phase 2).

---

**Document Version:** 1.2
**Last Updated:** November 26, 2025 (PHASE COMPLETE)
