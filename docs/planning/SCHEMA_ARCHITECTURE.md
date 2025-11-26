# ScratchBird Schema Architecture

**Created:** November 26, 2025
**Status:** REFERENCE DOCUMENT
**Purpose:** Define the hierarchical schema namespace architecture

---

## Overview

ScratchBird schemas are **hierarchical namespaces** similar to a file directory tree. Schemas can be nested to unlimited depth, and objects within schemas are referenced using dot-notation paths.

---

## Schema Hierarchy

```
/ (root)
├── sys/                              # System management
│   ├── security/                     # Security tables (users, roles, permissions)
│   ├── catalog/                      # Core catalog tables
│   ├── statistics/                   # Statistics and analytics
│   └── config/                       # Configuration settings
│
├── users/                            # User home directories
│   ├── alice/                        # User alice's home schema
│   │   ├── projects/
│   │   └── temp/
│   └── bob/                          # User bob's home schema
│
├── remote/                           # Remote server connections
│   ├── scratchbird/                  # Remote ScratchBird servers
│   │   ├── server1/                  # Mounted remote ScratchBird
│   │   └── server2/
│   │
│   └── emulated/                     # Emulated foreign database servers
│       ├── firebird/
│       │   ├── localhost/            # Local Firebird server
│       │   │   ├── employee/         # employee.fdb database
│       │   │   │   └── RDB$*/        # Firebird system tables (views)
│       │   │   └── inventory/        # inventory.fdb database
│       │   └── remoteserver/         # Remote Firebird server
│       │       └── production/
│       │
│       ├── postgresql/
│       │   └── pgserver1/
│       │       └── mydb/
│       │           └── pg_catalog/   # PostgreSQL system catalog (views)
│       │
│       ├── mysql/
│       │   └── mysqlserver/
│       │       └── shop/
│       │           └── mysql/        # MySQL system tables (views)
│       │
│       └── mssql/
│           └── sqlserver1/
│               └── sales/
│                   └── sys/          # SQL Server system views
│
├── public/                           # Default public schema
│
└── [user-created schemas]/           # Application schemas
    └── ...
```

---

## Path Notation

Objects are referenced using dot-notation:

```sql
-- Full path to a table
SELECT * FROM remote.emulated.firebird.localhost.employee.EMPLOYEE;

-- Full path to Firebird system table (emulated)
SELECT * FROM remote.emulated.firebird.localhost.employee.RDB$RELATIONS;

-- User's home directory
SELECT * FROM users.alice.projects.my_table;

-- System security table
SELECT * FROM sys.security.users;
```

---

## Key Concepts

### 1. Unlimited Nesting Depth
Schemas can be nested to any depth. The `parent_schema_id` in `SchemaInfo` creates the hierarchy.

### 2. Synonyms (Cross-Schema Pointers)
Synonyms allow creating aliases that point to objects in other schemas:

```sql
-- Create synonym in current schema pointing to remote table
CREATE SYNONYM employees FOR remote.emulated.firebird.localhost.employee.EMPLOYEE;

-- Now can query using short name
SELECT * FROM employees;
```

### 3. On-Demand Emulation Setup
Emulated server schemas are **NOT** pre-created. They are created dynamically when:
1. User configures an emulated server connection
2. User connects to a specific database on that server
3. System tables (RDB$*, pg_catalog.*, etc.) are created as **views** to real catalog

### 4. System Tables as Views
Emulated system tables (e.g., `RDB$RELATIONS`, `pg_catalog.pg_class`) are implemented as **views** that query the internal ScratchBird catalog and transform results to match the emulated format.

---

## Schema Types

| Schema Type | Location | Created By | Purpose |
|-------------|----------|------------|---------|
| System | `/sys/*` | Bootstrap | System management |
| User Home | `/users/{username}/*` | User creation | User workspace |
| Remote Native | `/remote/scratchbird/*` | Manual mount | Remote ScratchBird servers |
| Remote Emulated | `/remote/emulated/{engine}/{server}/{db}/*` | Server config | Foreign database emulation |
| Public | `/public` | Bootstrap | Default schema |
| Application | `/{name}/*` | CREATE SCHEMA | User applications |

---

## Emulation Schema Creation Flow

When a user configures a Firebird server emulation:

```
1. User: CREATE EMULATED SERVER firebird_local
         TYPE 'firebird'
         HOST 'localhost'
         PORT 3050;

   System creates:
   - /remote/emulated/firebird/                    (if not exists)
   - /remote/emulated/firebird/localhost/          (server schema)
   - EmulationServerInfo record

2. User: CONNECT TO firebird_local DATABASE 'employee';

   System creates:
   - /remote/emulated/firebird/localhost/employee/ (database schema)
   - EmulatedDatabaseInfo record
   - RDB$RELATIONS view (maps to sys.catalog.tables)
   - RDB$FIELDS view (maps to sys.catalog.columns)
   - RDB$INDICES view (maps to sys.catalog.indexes)
   - ... other RDB$ views
```

---

## Implementation Requirements

### SchemaInfo Structure Updates
```cpp
struct SchemaInfo {
    ID schema_id;
    ID parent_schema_id;          // Parent schema (zero for root)
    std::string schema_name;      // Short name (not full path)
    std::string full_path;        // Cached full path (e.g., "remote.emulated.firebird")
    SchemaType schema_type;       // SYSTEM, USER_HOME, REMOTE, EMULATED, PUBLIC, APPLICATION
    ID owner_id;
    // ... existing fields
};

enum class SchemaType : uint8_t {
    SYSTEM = 0,        // /sys/*
    USER_HOME = 1,     // /users/{username}/*
    REMOTE_NATIVE = 2, // /remote/scratchbird/*
    REMOTE_EMULATED = 3, // /remote/emulated/*
    PUBLIC = 4,        // /public
    APPLICATION = 5    // User-created
};
```

### Synonym Structure
```cpp
struct SynonymInfo {
    ID synonym_id;
    ID schema_id;                 // Schema containing the synonym
    std::string synonym_name;     // Local name
    std::string target_path;      // Full path to target object
    ObjectType target_type;       // TABLE, VIEW, SEQUENCE, PROCEDURE, etc.
    ID owner_id;
    uint64_t created_time;
};
```

### Emulation View Generation
When an emulated database is connected:
1. Create database schema under server schema
2. Generate protocol-specific system views:
   - Firebird: `RDB$RELATIONS`, `RDB$FIELDS`, `RDB$INDICES`, etc.
   - PostgreSQL: `pg_catalog.pg_class`, `pg_catalog.pg_attribute`, etc.
   - MySQL: `mysql.user`, `information_schema.*`, etc.
   - MSSQL: `sys.tables`, `sys.columns`, `INFORMATION_SCHEMA.*`, etc.

3. Views query internal catalog and transform to protocol format

---

## Path Resolution Algorithm

```cpp
Status resolveSchemaPath(const std::string& path, SchemaInfo& schema_out) {
    std::vector<std::string> parts = splitPath(path);  // Split on '.'

    ID current_schema_id = ROOT_SCHEMA_ID;

    for (const auto& part : parts) {
        // Find child schema with this name under current parent
        SchemaInfo child;
        Status s = getChildSchema(current_schema_id, part, child);
        if (!s.ok()) {
            return s;  // Schema not found
        }
        current_schema_id = child.schema_id;
    }

    return getSchema(current_schema_id, schema_out);
}
```

---

## Reserved Schema Names

| Name | Purpose |
|------|---------|
| `sys` | System management schemas |
| `users` | User home directories |
| `remote` | Remote server mounts |
| `public` | Default public schema |
| `information_schema` | SQL standard system views |
| `pg_catalog` | PostgreSQL compatibility (alias) |

---

## Related Documents

- `SCHEMA_NAVIGATION_AND_SEARCH_PATH.md` - Navigation commands, search path, system table locations
- `CATALOG_CLEANUP_PHASE_A_CRUD.md` - Add synonym CRUD
- `CATALOG_CLEANUP_PHASE_B_STRUCTURES.md` - Add SynonymInfo structure
- `CATALOG_CLEANUP_PHASE_D_VIRTUAL.md` - Update for on-demand view generation

---

**Document Version:** 1.1
**Last Updated:** November 26, 2025
