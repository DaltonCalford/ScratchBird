# Catalog Cleanup Phase D: Virtual Catalog Infrastructure

**Created:** November 26, 2025
**Priority:** MEDIUM (Foundation for Phase 2 Wire Protocols)
**Estimated Effort:** 30-40 hours
**Prerequisites:** Phase A, B, C complete
**Status:** NOT STARTED

---

## Overview

This phase implements the virtual catalog infrastructure required for Alpha Phase 2 wire protocol integration. Virtual catalogs allow ScratchBird to present protocol-specific system views (pg_catalog, information_schema, mysql.*, sys.*) while maintaining a single internal catalog.

Reference: `05-Wire-Protocol-Integration-Specification.md`

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     Wire Protocol Layer                         │
├──────────────┬──────────────┬──────────────┬───────────────────┤
│  PostgreSQL  │    MySQL     │    MSSQL     │     Firebird      │
│  (port 5432) │  (port 3306) │  (port 1433) │    (port 3050)    │
└──────┬───────┴──────┬───────┴──────┬───────┴────────┬──────────┘
       │              │              │                │
       ▼              ▼              ▼                ▼
┌──────────────────────────────────────────────────────────────────┐
│                   Virtual Catalog Router                         │
│  - Intercepts system catalog queries                            │
│  - Routes to protocol-specific handlers                          │
│  - Translates results to protocol format                         │
└──────────────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────────────┐
│                    Protocol Catalog Handlers                     │
├──────────────┬──────────────┬──────────────┬───────────────────┤
│ pg_catalog   │ mysql.*      │ sys.*        │ RDB$*             │
│ handler      │ handler      │ handler      │ handler           │
└──────┬───────┴──────┬───────┴──────┬───────┴────────┬──────────┘
       │              │              │                │
       └──────────────┴──────────────┴────────────────┘
                              │
                              ▼
┌──────────────────────────────────────────────────────────────────┐
│                   Internal ScratchBird Catalog                   │
│                      (CatalogManager)                            │
└──────────────────────────────────────────────────────────────────┘
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

## Virtual Schema Registration

On server startup, register virtual schemas:

```cpp
void initializeVirtualCatalogs(core::CatalogManager* catalog) {
    VirtualCatalogRouter& router = VirtualCatalogRouter::getInstance();
    router.initialize(catalog);

    // Register information_schema (all protocols)
    router.registerHandler(ProtocolType::POSTGRESQL,
        std::make_unique<InformationSchemaHandler>(catalog));
    router.registerHandler(ProtocolType::MYSQL,
        std::make_unique<InformationSchemaHandler>(catalog));
    router.registerHandler(ProtocolType::MSSQL,
        std::make_unique<InformationSchemaHandler>(catalog));

    // Register protocol-specific catalogs
    router.registerHandler(ProtocolType::POSTGRESQL,
        std::make_unique<PgCatalogHandler>(catalog));
    router.registerHandler(ProtocolType::MYSQL,
        std::make_unique<MySqlCatalogHandler>(catalog));
    router.registerHandler(ProtocolType::MSSQL,
        std::make_unique<MssqlCatalogHandler>(catalog));
}
```

---

## Checklist

### Implementation
- [ ] D-1: Virtual catalog interface design
- [ ] D-2: information_schema implementation (12 views)
- [ ] D-3: pg_catalog handler (12 views)
- [ ] D-4: mysql.* handler (6 views)
- [ ] D-5: sys.* handler (8 views)
- [ ] D-6: Query router integration

### Testing
- [ ] information_schema queries return correct data
- [ ] pg_catalog queries work with PostgreSQL clients
- [ ] mysql.* queries work with MySQL clients
- [ ] sys.* queries work with MSSQL clients
- [ ] WHERE clause filtering works

### Documentation
- [ ] Document virtual catalog mapping
- [ ] Update wire protocol documentation

---

## Effort Summary

| Task | Est. Hours |
|------|-----------|
| D-1: Interface Design | 4-6 |
| D-2: information_schema | 8-10 |
| D-3: pg_catalog | 6-8 |
| D-4: mysql.* | 4-6 |
| D-5: sys.* | 4-6 |
| D-6: Router Integration | 4-6 |
| **Total** | **30-42 hours** |

---

## Phase 2 Integration Notes

This virtual catalog infrastructure provides the foundation for:
1. **Wire Protocol Integration** - Clients can query system catalogs in their native format
2. **Tool Compatibility** - pgAdmin, MySQL Workbench, SSMS work without modification
3. **Migration Support** - Schema introspection tools work correctly
4. **Cross-Protocol Access** - Unified access to metadata regardless of protocol

---

**Document Version:** 1.0
**Last Updated:** November 26, 2025
