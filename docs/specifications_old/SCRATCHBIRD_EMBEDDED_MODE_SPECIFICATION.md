# ScratchBird Embedded Mode Specification


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


**Version:** 1.0  
**Status:** Authoritative (V3)
**Last Updated:** February 2026  

---

## 1. Purpose

Define ScratchBird's embedded mode operation - using ScratchBird itself as the storage engine for its own components (registry, security database, configuration) and for application embedding.

**Key Principle:** ScratchBird IS a database engine. It does not need SQLite, JSONB files, or external storage for its own metadata.

---

## 2. Embedded Mode Architecture

### 2.1 Why Use ScratchBird for ScratchBird's Data?

| Aspect | External Storage (Wrong) | ScratchBird Embedded (Correct) |
|--------|-------------------------|-------------------------------|
| **Dependencies** | SQLite, JSON libraries | None (self-hosting) |
| **Code Reuse** | Different storage code | Same MGA, same buffer pool |
| **Consistency** | Multiple code paths | Single storage engine |
| **Features** | Limited (file-based) | Full SQL, indexes, transactions |
| **Backup** | Special handling | Same backup as user databases |
| **Recovery** | Custom logic | Same MGA recovery |

### 2.2 Single-User Embedded Mode

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     SCRATCHBIRD EMBEDDED MODE                               │
└─────────────────────────────────────────────────────────────────────────────┘

Application (User Process)
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                     Application Code                             │   │
│  │  - Business logic                                                │   │
│  │  - SQL queries via ScratchBird API                               │   │
│  │  - Direct function calls (no IPC, no network)                    │   │
│  └─────────────────────────────────┬───────────────────────────────┘   │
│                                    │                                    │
│                                    ▼                                    │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │              ScratchBird Embedded Library                        │   │
│  │  (libscratchbird_core.a / libscratchbird_embedded.so)           │   │
│  │                                                                  │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │   │
│  │  │   Parser    │  │   Query     │  │   SBLR      │             │   │
│  │  │   (V2)      │──▶│   Optimizer │──▶│   Runtime   │             │   │
│  │  │             │  │             │  │             │             │   │
│  │  └─────────────┘  └─────────────┘  └──────┬──────┘             │   │
│  │                                           │                      │   │
│  │                                           ▼                      │   │
│  │  ┌──────────────────────────────────────────────────────────┐  │   │
│  │  │                    MGA Engine                             │  │   │
│  │  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │  │   │
│  │  │  │  Buffer  │  │  Lock    │  │  MGA     │  │  Storage │  │  │   │
│  │  │  │  Pool    │  │  Manager │  │  Core    │  │  Manager │  │  │   │
│  │  │  │          │  │          │  │          │  │          │  │  │   │
│  │  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │  │   │
│  │  └────────────────────────┬─────────────────────────────────┘  │   │
│  │                           │                                     │   │
│  │                           ▼                                     │   │
│  │  ┌──────────────────────────────────────────────────────────┐  │   │
│  │  │              ScratchBird Database File                    │  │   │
│  │  │              (registry.db / security.db / app.db)        │  │   │
│  │  │                                                           │  │   │
│  │  │  - Same format as server databases                        │  │   │
│  │  │  - Single writer (no lock contention)                     │  │   │
│  │  │  - In-memory pages via buffer pool                        │  │   │
│  │  │  - Single user = no lock manager needed                   │  │   │
│  │  │  - GC handled by application thread                       │  │   │
│  │  └──────────────────────────────────────────────────────────┘  │   │
│  │                                                                  │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  NO LISTENERS                                                           │
│  NO PARSER POOL                                                         │
│  NO IPC                                                                 │
│  NO NETWORK                                                             │
│  NO SERVICE THREADS (unless app spawns them)                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘

Key Characteristics:
- Library linked directly into application
- Direct function calls (API), no IPC overhead
- Single writer = no lock manager overhead
- Application responsible for GC (optional thread)
- Same .db file format as server mode
- Can migrate to full server by just opening with server
```

### 2.3 Registry as ScratchBird Database

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     REGISTRY STORAGE - EMBEDDED MODE                        │
└─────────────────────────────────────────────────────────────────────────────┘

WRONG APPROACH (What I incorrectly suggested):
┌─────────────────────────────────────────────────────────────────────────────┐
│  SQLite                     JSONB File                    External DB       │
│  ├─ Needs SQLite lib        ├─ Custom parsing             ├─ Network         │
│  ├─ Different SQL           ├─ No transactions            ├─ Dependencies    │
│  ├─ Separate backup         ├─ Manual consistency         ├─ Complex         │
│  └─ Extra dependency        └─ Limited features           └─ Overkill        │
└─────────────────────────────────────────────────────────────────────────────┘

CORRECT APPROACH (Use ScratchBird itself):
┌─────────────────────────────────────────────────────────────────────────────┐
│  /var/lib/scratchbird/registry.db                                          │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │  System Tables (created by server on first start)                   │   │
│  │                                                                     │   │
│  │  CREATE TABLE registered_databases (                               │   │
│  │      database_id        UUID PRIMARY KEY,                          │   │
│  │      database_name      VARCHAR(128) NOT NULL UNIQUE,              │   │
│  │      database_path      VARCHAR(512) NOT NULL,                     │   │
│  │      owner_user_id      UUID NOT NULL,                             │   │
│  │      created_at         TIMESTAMP DEFAULT CURRENT_TIMESTAMP,       │   │
│  │      security_model     VARCHAR(20) DEFAULT 'local',               │   │
│  │      status             VARCHAR(20) DEFAULT 'active',              │   │
│  │      page_size          INTEGER DEFAULT 8192,                      │   │
│  │      buffer_pool_size   BIGINT,                                    │   │
│  │      max_connections    INTEGER,                                   │   │
│  │      attach_count       INTEGER DEFAULT 0,                         │   │
│  │      description        VARCHAR(512)                               │   │
│  │  );                                                                │   │
│  │                                                                     │   │
│  │  CREATE INDEX idx_db_name ON registered_databases(database_name);  │   │
│  │  CREATE INDEX idx_db_status ON registered_databases(status);       │   │
│  │                                                                     │   │
│  │  CREATE TABLE database_permissions (                               │   │
│  │      database_id    UUID REFERENCES registered_databases,          │   │
│  │      user_id        UUID NOT NULL,                                 │   │
│  │      can_connect    BOOLEAN DEFAULT false,                         │   │
│  │      is_admin       BOOLEAN DEFAULT false,                         │   │
│  │      granted_at     TIMESTAMP,                                     │   │
│  │      PRIMARY KEY (database_id, user_id)                            │   │
│  │  );                                                                │   │
│  │                                                                     │   │
│  │  CREATE TABLE database_statistics (                                │   │
│  │      database_id        UUID PRIMARY KEY,                          │   │
│  │      total_connects     BIGINT DEFAULT 0,                          │   │
│  │      current_connections INTEGER DEFAULT 0,                        │   │
│  │      data_size_bytes    BIGINT DEFAULT 0,                          │   │
│  │      last_connect_time  TIMESTAMP                                  │   │
│  │  );                                                                │   │
│  │                                                                     │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  Benefits:                                                                 │
│  ✓ Self-hosting (ScratchBird stores its own metadata)                     │
│  ✓ Same backup/restore as user databases                                   │
│  ✓ Same recovery (MGA)                                                     │
│  ✓ SQL interface (can query with SELECT)                                   │
│  ✓ Indexes for fast lookups                                                │
│  ✓ Transactions for consistency                                            │
│  ✓ Can use ScratchBird features (compression, encryption, etc.)           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Embedded Mode API

### 3.1 Basic Usage

```cpp
#include <scratchbird/embedded.h>

// Open database in embedded mode
sb_embedded_db* db = sb_embedded_open(
    "/path/to/database.db",     // Database file
    SB_EMBEDDED_CREATE |        // Create if not exists
    SB_EMBEDDED_READWRITE,      // Read-write mode
    nullptr                     // Options (optional)
);

if (!db) {
    // Handle error
    return 1;
}

// Execute SQL directly
sb_result* result = sb_embedded_query(db, 
    "SELECT id, name FROM users WHERE active = true");

// Iterate results
while (sb_result_next_row(result)) {
    int64_t id = sb_result_get_int64(result, 0);
    const char* name = sb_result_get_string(result, 1);
    printf("User %ld: %s\n", id, name);
}

// Cleanup
sb_result_free(result);
sb_embedded_close(db);
```

### 3.2 Registry as Embedded Database

```cpp
class DatabaseRegistry {
private:
    sb_embedded_db* registry_db_ = nullptr;
    
public:
    bool open(const std::string& path) {
        // Open registry as embedded ScratchBird database
        registry_db_ = sb_embedded_open(
            path.c_str(),
            SB_EMBEDDED_CREATE | SB_EMBEDDED_READWRITE,
            nullptr
        );
        
        if (!registry_db_) {
            return false;
        }
        
        // Initialize schema if new
        if (isNewDatabase(registry_db_)) {
            initializeSchema();
        }
        
        return true;
    }
    
    void initializeSchema() {
        // Create tables using standard SQL
        const char* schema_sql = R"(
            CREATE TABLE registered_databases (
                database_id UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
                database_name VARCHAR(128) NOT NULL UNIQUE,
                database_path VARCHAR(512) NOT NULL,
                owner_user_id UUID NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                security_model VARCHAR(20) DEFAULT 'local',
                status VARCHAR(20) DEFAULT 'active',
                page_size INTEGER DEFAULT 8192,
                description VARCHAR(512)
            );
            
            CREATE INDEX idx_db_name ON registered_databases(database_name);
            CREATE INDEX idx_db_status ON registered_databases(status);
            
            CREATE TABLE database_permissions (
                database_id UUID REFERENCES registered_databases,
                user_id UUID NOT NULL,
                can_connect BOOLEAN DEFAULT false,
                is_admin BOOLEAN DEFAULT false,
                PRIMARY KEY (database_id, user_id)
            );
        )";
        
        sb_embedded_execute(registry_db_, schema_sql);
    }
    
    std::vector<DatabaseInfo> listDatabases(const std::string& user_id) {
        std::vector<DatabaseInfo> result;
        
        // Query using standard SQL
        const char* query = R"(
            SELECT d.database_id, d.database_name, d.description,
                   p.can_connect, p.is_admin
            FROM registered_databases d
            LEFT JOIN database_permissions p 
                ON d.database_id = p.database_id AND p.user_id = ?
            WHERE d.status = 'active'
            ORDER BY d.database_name
        )";
        
        sb_result* rs = sb_embedded_query_params(registry_db_, query,
            SB_PARAM_UUID, user_id.c_str()
        );
        
        while (sb_result_next_row(rs)) {
            DatabaseInfo info;
            info.database_id = sb_result_get_uuid(rs, 0);
            info.database_name = sb_result_get_string(rs, 1);
            info.can_connect = sb_result_get_bool(rs, 3);
            result.push_back(info);
        }
        
        sb_result_free(rs);
        return result;
    }
    
    bool createDatabase(const std::string& name, 
                        const std::string& owner_id,
                        std::string* out_db_id) {
        // Insert using standard SQL
        const char* insert = R"(
            INSERT INTO registered_databases 
                (database_id, database_name, owner_user_id)
            VALUES 
                (gen_uuid_v7(), ?, ?)
            RETURNING database_id
        )";
        
        sb_result* rs = sb_embedded_query_params(registry_db_, insert,
            SB_PARAM_STRING, name.c_str(),
            SB_PARAM_UUID, owner_id.c_str()
        );
        
        if (sb_result_next_row(rs)) {
            *out_db_id = sb_result_get_uuid(rs, 0);
            sb_result_free(rs);
            return true;
        }
        
        sb_result_free(rs);
        return false;
    }
};
```

---

## 4. Server Modes

### 4.1 Mode Comparison

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         SERVER MODE MATRIX                                  │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────┬───────────────────┬───────────────────┬───────────────────┐
│     Aspect      │  EMBEDDED         │  IPC SERVER       │  FULL SERVER      │
├─────────────────┼───────────────────┼───────────────────┼───────────────────┤
│                 │                   │                   │                   │
│ Use Case        │ Single app        │ Local processes   │ Network clients   │
│                 │                   │                   │                   │
├─────────────────┼───────────────────┼───────────────────┼───────────────────┤
│                 │                   │                   │                   │
│ Parsers         │ Library calls     │ Library calls     │ Separate processes│
│                 │                   │                   │                   │
├─────────────────┼───────────────────┼───────────────────┼───────────────────┤
│                 │                   │                   │                   │
│ Listeners       │ None              │ None              │ Yes (network)     │
│                 │                   │                   │                   │
├─────────────────┼───────────────────┼───────────────────┼───────────────────┤
│                 │                   │                   │                   │
│ IPC             │ None (direct)     │ Unix sockets      │ Network sockets   │
│                 │                   │                   │                   │
├─────────────────┼───────────────────┼───────────────────┼───────────────────┤
│                 │                   │                   │                   │
│ Lock Manager    │ None (single user)│ Yes               │ Yes               │
│                 │                   │                   │                   │
├─────────────────┼───────────────────┼───────────────────┼───────────────────┤
│                 │                   │                   │                   │
│ GC Thread       │ App-managed       │ Yes               │ Yes               │
│                 │                   │                   │                   │
├─────────────────┼───────────────────┼───────────────────┼───────────────────┤
│                 │                   │                   │                   │
│ Multi-user      │ No                │ Yes               │ Yes               │
│                 │                   │                   │                   │
├─────────────────┼───────────────────┼───────────────────┼───────────────────┤
│                 │                   │                   │                   │
│ Example         │ Mobile app        │ Desktop app       │ Web backend       │
│                 │ IoT device        │ Local services    │ Enterprise DB     │
│                 │                   │                   │                   │
└─────────────────┴───────────────────┴───────────────────┴───────────────────┘
```

### 4.2 IPC Server Mode

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     IPC SERVER MODE                                         │
│        (Shared server for local processes, no network listeners)            │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                         sb_server --mode=ipc                                │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    Core Engine (same as full server)                 │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐            │   │
│  │  │  Buffer  │  │  Lock    │  │  MGA     │  │  Storage │            │   │
│  │  │  Pool    │  │  Manager │  │  Core    │  │  Manager │            │   │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘            │   │
│  └──────────────────────────┬──────────────────────────────────────────┘   │
│                             │                                               │
│                             │ Unix Domain Sockets (no TCP)                  │
│                             │                                               │
│  ┌──────────────────────────┼──────────────────────────────────────────┐   │
│  │                          ▼                                          │   │
│  │         Parser Library (loaded into client processes)               │   │
│  │                                                                     │   │
│  │   ┌────────────┐ ┌────────────┐ ┌────────────┐                     │   │
│  │   │  App 1     │ │  App 2     │ │  App 3     │                     │   │
│  │   │  (Python)  │ │  (Go)      │ │  (C++)     │                     │   │
│  │   │            │ │            │ │            │                     │   │
│  │   │┌──────────┐│ │┌──────────┐│ │┌──────────┐│                     │   │
│  │   ││ libsb    ││ ││ libsb    ││ ││ libsb    ││                     │   │
│  │   ││ parser   ││ ││ parser   ││ ││ parser   ││                     │   │
│  │   │└──────────┘│ │└──────────┘│ │└──────────┘│                     │   │
│  │   └────────────┘ └────────────┘ └────────────┘                     │   │
│  │                                                                     │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  NO TCP LISTENERS (only Unix sockets)                                      │
│  NO NETWORK EXPOSURE                                                       │
│  MULTIPLE LOCAL CLIENTS (via library)                                      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Registry Implementation (Corrected)

### 5.1 Registry as ScratchBird Database

```cpp
namespace scratchbird {
namespace server {

class DatabaseRegistry {
public:
    // Open registry as embedded ScratchBird database
    bool open(const std::string& registry_path, core::ErrorContext* ctx) {
        // Open as embedded database (single user, no lock manager)
        db_ = core::Database::openEmbedded(registry_path, ctx);
        if (!db_) {
            // Try to create new registry
            db_ = core::Database::createEmbedded(registry_path, ctx);
            if (!db_) {
                return false;
            }
            return initializeSchema(ctx);
        }
        return true;
    }
    
private:
    core::Database* db_ = nullptr;  // Embedded ScratchBird database
    
    bool initializeSchema(core::ErrorContext* ctx) {
        // Execute DDL to create registry tables
        const char* ddl = R"(
            CREATE TABLE registered_databases (
                database_id UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
                database_name VARCHAR(128) NOT NULL UNIQUE,
                database_path VARCHAR(512) NOT NULL,
                owner_user_id UUID NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                security_model VARCHAR(20) DEFAULT 'local',
                status VARCHAR(20) DEFAULT 'active',
                page_size INTEGER DEFAULT 8192,
                charset VARCHAR(32) DEFAULT 'UTF8',
                collation VARCHAR(64) DEFAULT 'UTF8_GENERAL_CI',
                buffer_pool_size BIGINT,
                max_connections INTEGER,
                attach_count INTEGER DEFAULT 0,
                description VARCHAR(512),
                tags VARCHAR(1024)
            );
            
            CREATE INDEX idx_db_name ON registered_databases(database_name);
            CREATE INDEX idx_db_status ON registered_databases(status);
            CREATE INDEX idx_db_owner ON registered_databases(owner_user_id);
            
            CREATE TABLE database_permissions (
                database_id UUID REFERENCES registered_databases ON DELETE CASCADE,
                user_id UUID NOT NULL,
                user_name VARCHAR(128),
                can_connect BOOLEAN DEFAULT false,
                can_create_schema BOOLEAN DEFAULT false,
                can_create_table BOOLEAN DEFAULT false,
                can_create_view BOOLEAN DEFAULT false,
                can_create_proc BOOLEAN DEFAULT false,
                can_backup BOOLEAN DEFAULT false,
                can_restore BOOLEAN DEFAULT false,
                is_admin BOOLEAN DEFAULT false,
                granted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                granted_by VARCHAR(128),
                PRIMARY KEY (database_id, user_id)
            );
            
            CREATE INDEX idx_perm_user ON database_permissions(user_id);
            
            CREATE TABLE database_statistics (
                database_id UUID PRIMARY KEY REFERENCES registered_databases ON DELETE CASCADE,
                total_connects BIGINT DEFAULT 0,
                total_disconnects BIGINT DEFAULT 0,
                current_connections INTEGER DEFAULT 0,
                max_connections_reached INTEGER DEFAULT 0,
                queries_executed BIGINT DEFAULT 0,
                queries_failed BIGINT DEFAULT 0,
                avg_query_time_ms DOUBLE DEFAULT 0.0,
                data_size_bytes BIGINT DEFAULT 0,
                index_size_bytes BIGINT DEFAULT 0,
                blob_size_bytes BIGINT DEFAULT 0,
                free_space_bytes BIGINT DEFAULT 0,
                first_connect_time TIMESTAMP,
                last_connect_time TIMESTAMP,
                last_disconnect_time TIMESTAMP,
                stats_updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
            
            CREATE TABLE database_aliases (
                alias_name VARCHAR(128) PRIMARY KEY,
                database_id UUID REFERENCES registered_databases ON DELETE CASCADE,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
            
            CREATE TABLE registry_metadata (
                key VARCHAR(128) PRIMARY KEY,
                value VARCHAR(512),
                description VARCHAR(256)
            );
            
            INSERT INTO registry_metadata (key, value, description) VALUES
            ('schema_version', '1.0', 'Registry schema version'),
            ('created_at', CURRENT_TIMESTAMP, 'Registry creation time'),
            ('server_instance', 'default', 'Server instance name');
        )";
        
        return db_->execute(ddl) == core::Status::OK;
    }
};

} // namespace server
} // namespace scratchbird
```

---

## 6. Summary: Self-Hosting Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                  SCRATCHBIRD SELF-HOSTING PRINCIPLE                         │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│   ScratchBird uses ITSELF for all its metadata storage:                     │
│                                                                             │
│   ┌──────────────────────┐  ┌──────────────────────┐  ┌──────────────────┐ │
│   │  registry.db         │  │  security.db         │  │  User databases  │ │
│   │  (database catalog)  │  │  (auth/users/roles)  │  │  (application    │ │
│   │                      │  │                      │  │   data)          │ │
│   │  - List of databases │  │  - User accounts     │  │                  │ │
│   │  - Database paths    │  │  - Password hashes   │  │  - sales.db      │ │
│   │  - Per-db settings   │  │  - Role permissions  │  │  - hr.db         │ │
│   │                      │  │  - Session tokens    │  │  - production.db │ │
│   └──────────┬───────────┘  └──────────┬───────────┘  └────────┬─────────┘ │
│              │                         │                       │           │
│              └─────────────────────────┴───────────────────────┘           │
│                            │                                               │
│                            ▼                                               │
│              ┌─────────────────────────┐                                   │
│              │   ScratchBird Engine    │                                   │
│              │   (MGA, Buffer Pool,    │                                   │
│              │    Lock Manager, etc.)  │                                   │
│              └─────────────────────────┘                                   │
│                                                                             │
│   NO EXTERNAL DEPENDENCIES:                                                 │
│   ✗ No SQLite                                                               │
│   ✗ No JSONB files                                                          │
│   ✗ No external key-value stores                                            │
│                                                                             │
│   SELF-CONTAINED:                                                           │
│   ✓ ScratchBird stores its own metadata                                     │
│   ✓ Same backup/restore for all databases                                   │
│   ✓ Same recovery mechanism                                                 │
│   ✓ Same features (compression, encryption)                                 │
│   ✓ SQL interface to metadata (SELECT * FROM registered_databases)         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Related Specifications

- [DATABASE_REGISTRY_SPECIFICATION_CORRECTED.md](DATABASE_REGISTRY_SPECIFICATION_CORRECTED.md) - Registry schema (uses ScratchBird, not SQLite/JSONB)
- [ARCHITECTURE_CLARIFICATIONS.md](ARCHITECTURE_CLARIFICATIONS.md) - Critical design decisions
- [SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md](SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md) - Server startup phases

---

## 8. AI Assistant Reminders

**DO NOT assume:**
- "Need external storage for metadata" - ScratchBird IS the storage
- "Use SQLite for registry" - Use ScratchBird embedded
- "JSON files for config" - Use ScratchBird tables
- "Need separate backup system for registry" - Same backup as any database

**ALWAYS remember:**
- ScratchBird is a self-hosting database engine
- It can store its own metadata using itself
- Embedded mode has no listeners, no IPC, direct API calls
- Single-user embedded has no lock manager overhead