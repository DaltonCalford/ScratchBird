# Database Registry Specification (Corrected)


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

## CRITICAL CORRECTION

**Previous Incorrect Approaches:**
- ❌ SQLite for registry (external dependency, different storage code)
- ❌ JSONB file format (custom parsing, no transactions, no indexes)

**Correct Approach:**
- ✅ **Use ScratchBird itself** - Self-hosting database engine
- Store registry in a ScratchBird database using embedded mode
- Same storage engine, same backup, same recovery as user databases

---

## 1. Purpose

Define the database registry - the system catalog that tracks all databases known to a ScratchBird server instance, including their locations, security models, and current status.

---

## 2. Architecture

### 2.1 Registry as ScratchBird Database

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     REGISTRY - SELF-HOSTING APPROACH                        │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│   /var/lib/scratchbird/registry.db                                         │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐  │
│   │            ScratchBird Embedded Database (Single User)              │  │
│   │                                                                     │  │
│   │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐             │  │
│   │  │  System      │  │  System      │  │  System      │             │  │
│   │  │  Tables      │  │  Indexes     │  │  Procedures  │             │  │
│   │  │              │  │              │  │              │             │  │
│   │  │  registered_ │  │  idx_db_name │  │  register_   │             │  │
│   │  │  databases   │  │  idx_db_owner│  │  database()  │             │  │
│   │  │              │  │              │  │              │             │  │
│   │  │  database_   │  │  idx_perm_   │  │  verify_     │             │  │
│   │  │  permissions │  │  user        │  │  ownership() │             │  │
│   │  │              │  │              │  │              │             │  │
│   │  │  database_   │  │              │  │              │             │  │
│   │  │  statistics  │  │              │  │              │             │  │
│   │  │              │  │              │  │              │             │  │
│   │  │  database_   │  │              │  │              │             │  │
│   │  │  aliases     │  │              │  │              │             │  │
│   │  └──────────────┘  └──────────────┘  └──────────────┘             │  │
│   │                                                                     │  │
│   │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐             │  │
│   │  │  MGA Core    │  │  Buffer Pool │  │  Storage     │             │  │
│   │  │  (versions)  │  │  (cache)     │  │  Manager     │             │  │
│   │  └──────────────┘  └──────────────┘  └──────────────┘             │  │
│   │                                                                     │  │
│   └─────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
│   NO LOCK MANAGER (single user - the server process)                       │
│   GC HANDLED BY SERVER (or optional background thread)                     │
│   SAME FORMAT AS USER DATABASES                                            │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Why ScratchBird for Registry?

| Requirement | SQLite/JSONB (Wrong) | ScratchBird (Correct) |
|-------------|---------------------|----------------------|
| Dependencies | External library | Self-contained |
| Transactions | Limited/None | Full ACID (MGA) |
| Indexes | Manual/File-only | B-tree indexes |
| SQL Interface | Different SQL/None | Native SBLR/SQL |
| Backup | Custom handling | Same `sb_backup` |
| Recovery | Custom logic | Same MGA recovery |
| Encryption | Extra layer | Native support |
| Compression | External | Native support |
| Future features | Manual updates | Automatic |

---

## 3. Registry Schema

### 3.1 System Tables

```sql
-- Main database catalog
CREATE TABLE registered_databases (
    -- Identification
    database_id         UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    database_name       VARCHAR(128) NOT NULL UNIQUE,
    
    -- Storage
    database_path       VARCHAR(512) NOT NULL,
    page_size           INTEGER DEFAULT 8192 CHECK (page_size IN (4096, 8192, 16384, 32768)),
    
    -- Security
    owner_user_id       UUID NOT NULL,
    security_model      VARCHAR(20) DEFAULT 'local' 
                        CHECK (security_model IN ('local', 'ldap', 'kerberos', 'oauth')),
    
    -- Status
    status              VARCHAR(20) DEFAULT 'active'
                        CHECK (status IN ('active', 'inactive', 'recovering', 'backup', 'maintenance')),
    
    -- Configuration
    charset             VARCHAR(32) DEFAULT 'UTF8',
    collation           VARCHAR(64) DEFAULT 'UTF8_GENERAL_CI',
    buffer_pool_size    BIGINT,           -- Override server default
    max_connections     INTEGER,          -- Override server default
    
    -- Metadata
    description         VARCHAR(512),
    tags                VARCHAR(1024),
    
    -- Statistics
    attach_count        INTEGER DEFAULT 0,
    created_at          TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at          TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Indexes for common lookups
CREATE INDEX idx_db_name ON registered_databases(database_name);
CREATE INDEX idx_db_status ON registered_databases(status);
CREATE INDEX idx_db_owner ON registered_databases(owner_user_id);

-- Permission grants
CREATE TABLE database_permissions (
    database_id         UUID REFERENCES registered_databases ON DELETE CASCADE,
    user_id             UUID NOT NULL,
    user_name           VARCHAR(128),
    
    -- Permission flags
    can_connect         BOOLEAN DEFAULT false,
    can_create_schema   BOOLEAN DEFAULT false,
    can_create_table    BOOLEAN DEFAULT false,
    can_create_view     BOOLEAN DEFAULT false,
    can_create_proc     BOOLEAN DEFAULT false,
    can_backup          BOOLEAN DEFAULT false,
    can_restore         BOOLEAN DEFAULT false,
    is_admin            BOOLEAN DEFAULT false,
    
    -- Audit
    granted_at          TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    granted_by          VARCHAR(128),
    
    PRIMARY KEY (database_id, user_id)
);

CREATE INDEX idx_perm_user ON database_permissions(user_id);
CREATE INDEX idx_perm_db ON database_permissions(database_id);

-- Runtime statistics
CREATE TABLE database_statistics (
    database_id             UUID PRIMARY KEY REFERENCES registered_databases ON DELETE CASCADE,
    
    -- Connection stats
    total_connects          BIGINT DEFAULT 0,
    total_disconnects       BIGINT DEFAULT 0,
    current_connections     INTEGER DEFAULT 0,
    max_connections_reached INTEGER DEFAULT 0,
    
    -- Query stats
    queries_executed        BIGINT DEFAULT 0,
    queries_failed          BIGINT DEFAULT 0,
    avg_query_time_ms       DOUBLE DEFAULT 0.0,
    
    -- Size stats
    data_size_bytes         BIGINT DEFAULT 0,
    index_size_bytes        BIGINT DEFAULT 0,
    blob_size_bytes         BIGINT DEFAULT 0,
    free_space_bytes        BIGINT DEFAULT 0,
    
    -- Timestamps
    first_connect_time      TIMESTAMP,
    last_connect_time       TIMESTAMP,
    last_disconnect_time    TIMESTAMP,
    stats_updated_at        TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Aliases for databases
CREATE TABLE database_aliases (
    alias_name          VARCHAR(128) PRIMARY KEY,
    database_id         UUID REFERENCES registered_databases ON DELETE CASCADE,
    created_at          TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_by          VARCHAR(128)
);

-- Registry metadata
CREATE TABLE registry_metadata (
    key                 VARCHAR(128) PRIMARY KEY,
    value               VARCHAR(512),
    description         VARCHAR(256)
);

-- Insert initial metadata
INSERT INTO registry_metadata (key, value, description) VALUES
('schema_version', '1.0', 'Registry schema version'),
('created_at', CURRENT_TIMESTAMP, 'Registry creation timestamp'),
('server_instance', 'default', 'Server instance identifier');
```

---

## 4. C++ Implementation

### 4.1 DatabaseRegistry Class

```cpp
#pragma once

#include "scratchbird/core/database.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuid.h"
#include <string>
#include <vector>
#include <optional>

namespace scratchbird {
namespace server {

struct DatabaseInfo {
    core::UUID database_id;
    std::string database_name;
    std::string database_path;
    core::UUID owner_user_id;
    std::string security_model;
    std::string status;
    int page_size;
    std::optional<int64_t> buffer_pool_size;
    std::optional<int> max_connections;
    std::string description;
    int attach_count;
    core::Timestamp created_at;
    core::Timestamp updated_at;
};

struct PermissionInfo {
    core::UUID database_id;
    core::UUID user_id;
    std::string user_name;
    bool can_connect;
    bool can_create_schema;
    bool can_create_table;
    bool can_create_view;
    bool can_create_proc;
    bool can_backup;
    bool can_restore;
    bool is_admin;
};

struct StatisticsInfo {
    core::UUID database_id;
    int64_t total_connects;
    int64_t current_connections;
    int64_t queries_executed;
    int64_t data_size_bytes;
    int64_t index_size_bytes;
    core::Timestamp last_connect_time;
};

class DatabaseRegistry {
public:
    DatabaseRegistry();
    ~DatabaseRegistry();

    // Lifecycle
    core::Status open(const std::string& registry_path, core::ErrorContext* ctx);
    void close();
    bool isOpen() const;

    // Database CRUD
    core::Status registerDatabase(const DatabaseInfo& info, 
                                   core::UUID* out_id, 
                                   core::ErrorContext* ctx);
    core::Status unregisterDatabase(const core::UUID& db_id, 
                                     core::ErrorContext* ctx);
    core::Status updateDatabase(const core::UUID& db_id,
                                 const DatabaseInfo& updates,
                                 core::ErrorContext* ctx);
    
    // Queries
    std::optional<DatabaseInfo> getDatabaseById(const core::UUID& db_id);
    std::optional<DatabaseInfo> getDatabaseByName(const std::string& name);
    std::optional<DatabaseInfo> getDatabaseByAlias(const std::string& alias);
    std::vector<DatabaseInfo> listDatabases(const std::string& user_id);
    std::vector<DatabaseInfo> listDatabasesByStatus(const std::string& status);
    
    // Permissions
    core::Status grantPermission(const PermissionInfo& perm, core::ErrorContext* ctx);
    core::Status revokePermission(const core::UUID& db_id, 
                                   const core::UUID& user_id,
                                   core::ErrorContext* ctx);
    std::vector<PermissionInfo> getUserPermissions(const core::UUID& user_id);
    bool checkPermission(const core::UUID& db_id, 
                         const core::UUID& user_id,
                         const std::string& permission);
    
    // Aliases
    core::Status createAlias(const std::string& alias, 
                              const core::UUID& db_id,
                              const std::string& created_by,
                              core::ErrorContext* ctx);
    core::Status dropAlias(const std::string& alias, core::ErrorContext* ctx);
    
    // Statistics
    core::Status updateConnectStats(const core::UUID& db_id, bool connected);
    core::Status updateQueryStats(const core::UUID& db_id, 
                                   bool success,
                                   double elapsed_ms);
    core::Status updateSizeStats(const core::UUID& db_id);
    StatisticsInfo getStatistics(const core::UUID& db_id);
    
    // Utility
    core::Status setDatabaseStatus(const core::UUID& db_id, 
                                    const std::string& status,
                                    core::ErrorContext* ctx);
    std::string resolvePath(const std::string& path_or_alias);
    
private:
    core::Database* db_ = nullptr;  // Embedded ScratchBird database
    std::string registry_path_;
    bool initialized_ = false;
    
    bool initializeSchema(core::ErrorContext* ctx);
    core::Status executeDDL(const char* ddl, core::ErrorContext* ctx);
};

} // namespace server
} // namespace scratchbird
```

### 4.2 Implementation

```cpp
#include "scratchbird/server/database_registry.h"
#include "scratchbird/core/embedded.h"

namespace scratchbird {
namespace server {

DatabaseRegistry::DatabaseRegistry() = default;

DatabaseRegistry::~DatabaseRegistry() {
    close();
}

core::Status DatabaseRegistry::open(const std::string& registry_path, 
                                     core::ErrorContext* ctx) {
    registry_path_ = registry_path;
    
    // Try to open existing registry
    db_ = core::Database::openEmbedded(registry_path, ctx);
    
    if (!db_) {
        // Create new registry database
        db_ = core::Database::createEmbedded(registry_path, ctx);
        if (!db_) {
            return core::Status::ERROR;
        }
        
        // Initialize schema
        if (!initializeSchema(ctx)) {
            db_->close();
            db_ = nullptr;
            return core::Status::ERROR;
        }
    }
    
    initialized_ = true;
    return core::Status::OK;
}

void DatabaseRegistry::close() {
    if (db_) {
        db_->close();
        db_ = nullptr;
    }
    initialized_ = false;
}

bool DatabaseRegistry::initializeSchema(core::ErrorContext* ctx) {
    const char* schema_ddl = R"(
        CREATE TABLE registered_databases (
            database_id UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
            database_name VARCHAR(128) NOT NULL UNIQUE,
            database_path VARCHAR(512) NOT NULL,
            owner_user_id UUID NOT NULL,
            security_model VARCHAR(20) DEFAULT 'local',
            status VARCHAR(20) DEFAULT 'active',
            page_size INTEGER DEFAULT 8192,
            charset VARCHAR(32) DEFAULT 'UTF8',
            collation VARCHAR(64) DEFAULT 'UTF8_GENERAL_CI',
            buffer_pool_size BIGINT,
            max_connections INTEGER,
            attach_count INTEGER DEFAULT 0,
            description VARCHAR(512),
            tags VARCHAR(1024),
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
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
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            created_by VARCHAR(128)
        );
        
        CREATE TABLE registry_metadata (
            key VARCHAR(128) PRIMARY KEY,
            value VARCHAR(512),
            description VARCHAR(256)
        );
        
        INSERT INTO registry_metadata (key, value, description) VALUES
        ('schema_version', '1.0', 'Registry schema version'),
        ('created_at', CURRENT_TIMESTAMP, 'Registry creation timestamp'),
        ('server_instance', 'default', 'Server instance identifier');
    )";
    
    return db_->execute(schema_ddl) == core::Status::OK;
}

core::Status DatabaseRegistry::registerDatabase(const DatabaseInfo& info,
                                                 core::UUID* out_id,
                                                 core::ErrorContext* ctx) {
    const char* insert_sql = R"(
        INSERT INTO registered_databases 
            (database_name, database_path, owner_user_id, security_model,
             page_size, charset, collation, buffer_pool_size, max_connections,
             description, tags)
        VALUES 
            (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        RETURNING database_id
    )";
    
    auto stmt = db_->prepare(insert_sql);
    if (!stmt) {
        return core::Status::ERROR;
    }
    
    stmt->bindString(1, info.database_name);
    stmt->bindString(2, info.database_path);
    stmt->bindUUID(3, info.owner_user_id);
    stmt->bindString(4, info.security_model);
    stmt->bindInt(5, info.page_size);
    stmt->bindString(6, info.charset);
    stmt->bindString(7, info.collation);
    stmt->bindOptionalInt64(8, info.buffer_pool_size);
    stmt->bindOptionalInt(9, info.max_connections);
    stmt->bindString(10, info.description);
    stmt->bindString(11, info.tags);
    
    auto result = stmt->executeQuery();
    if (result->next()) {
        *out_id = result->getUUID(0);
        return core::Status::OK;
    }
    
    return core::Status::ERROR;
}

std::optional<DatabaseInfo> DatabaseRegistry::getDatabaseByName(
    const std::string& name) {
    
    const char* query = R"(
        SELECT database_id, database_name, database_path, owner_user_id,
               security_model, status, page_size, charset, collation,
               buffer_pool_size, max_connections, description, 
               attach_count, created_at, updated_at
        FROM registered_databases
        WHERE database_name = ? AND status != 'deleted'
    )";
    
    auto stmt = db_->prepare(query);
    stmt->bindString(1, name);
    
    auto result = stmt->executeQuery();
    if (result->next()) {
        DatabaseInfo info;
        info.database_id = result->getUUID(0);
        info.database_name = result->getString(1);
        info.database_path = result->getString(2);
        info.owner_user_id = result->getUUID(3);
        info.security_model = result->getString(4);
        info.status = result->getString(5);
        info.page_size = result->getInt(6);
        info.charset = result->getString(7);
        info.collation = result->getString(8);
        info.buffer_pool_size = result->getOptionalInt64(9);
        info.max_connections = result->getOptionalInt(10);
        info.description = result->getString(11);
        info.attach_count = result->getInt(12);
        info.created_at = result->getTimestamp(13);
        info.updated_at = result->getTimestamp(14);
        return info;
    }
    
    return std::nullopt;
}

std::vector<DatabaseInfo> DatabaseRegistry::listDatabases(const std::string& user_id) {
    std::vector<DatabaseInfo> databases;
    
    const char* query = R"(
        SELECT d.database_id, d.database_name, d.database_path, 
               d.owner_user_id, d.security_model, d.status,
               d.page_size, d.charset, d.collation, d.buffer_pool_size,
               d.max_connections, d.description, d.attach_count,
               d.created_at, d.updated_at
        FROM registered_databases d
        LEFT JOIN database_permissions p 
            ON d.database_id = p.database_id AND p.user_id = ?
        WHERE d.status = 'active'
          AND (d.owner_user_id = ? OR p.can_connect = true OR p.is_admin = true)
        ORDER BY d.database_name
    )";
    
    auto stmt = db_->prepare(query);
    stmt->bindUUID(1, user_id);
    stmt->bindUUID(2, user_id);
    
    auto result = stmt->executeQuery();
    while (result->next()) {
        DatabaseInfo info;
        info.database_id = result->getUUID(0);
        info.database_name = result->getString(1);
        info.database_path = result->getString(2);
        info.owner_user_id = result->getUUID(3);
        info.security_model = result->getString(4);
        info.status = result->getString(5);
        info.page_size = result->getInt(6);
        info.charset = result->getString(7);
        info.collation = result->getString(8);
        info.buffer_pool_size = result->getOptionalInt64(9);
        info.max_connections = result->getOptionalInt(10);
        info.description = result->getString(11);
        info.attach_count = result->getInt(12);
        info.created_at = result->getTimestamp(13);
        info.updated_at = result->getTimestamp(14);
        databases.push_back(info);
    }
    
    return databases;
}

core::Status DatabaseRegistry::updateConnectStats(const core::UUID& db_id, 
                                                   bool connected) {
    const char* update = connected ? R"(
        UPDATE database_statistics
        SET total_connects = total_connects + 1,
            current_connections = current_connections + 1,
            first_connect_time = COALESCE(first_connect_time, CURRENT_TIMESTAMP),
            last_connect_time = CURRENT_TIMESTAMP,
            stats_updated_at = CURRENT_TIMESTAMP
        WHERE database_id = ?
    )" : R"(
        UPDATE database_statistics
        SET total_disconnects = total_disconnects + 1,
            current_connections = current_connections - 1,
            last_disconnect_time = CURRENT_TIMESTAMP,
            stats_updated_at = CURRENT_TIMESTAMP
        WHERE database_id = ?
    )";
    
    auto stmt = db_->prepare(update);
    stmt->bindUUID(1, db_id);
    stmt->execute();
    
    return core::Status::OK;
}

} // namespace server
} // namespace scratchbird
```

---

## 5. Registry Location

### 5.1 Default Paths

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     REGISTRY LOCATION                                       │
└─────────────────────────────────────────────────────────────────────────────┘

Platform        │  Default Path
────────────────┼─────────────────────────────────────────────────────────────
Linux           │  /var/lib/scratchbird/registry.db
                │  (or $SCRATCHBIRD_HOME/registry.db if set)
────────────────┼─────────────────────────────────────────────────────────────
macOS           │  /usr/local/var/scratchbird/registry.db
                │  (or $SCRATCHBIRD_HOME/registry.db if set)
────────────────┼─────────────────────────────────────────────────────────────
Windows         │  C:\ProgramData\ScratchBird\registry.db
                │  (or %SCRATCHBIRD_HOME%\registry.db if set)
────────────────┼─────────────────────────────────────────────────────────────
Embedded        │  Configurable by application
                │  (./registry.db or in-memory :memory:)
```

### 5.2 Configuration

```ini
; scratchbird.conf - Registry configuration
[registry]
; Path to registry database
path = /var/lib/scratchbird/registry.db

; Create if not exists (default: true)
auto_create = true

; Sync mode for durability (default: full)
; full = fsync on every transaction
; normal = fsync periodically
; off = no fsync (risk of corruption)
sync_mode = full

; Backup settings
[registry.backup]
; Enable automatic backup
enabled = true

; Backup interval (hours)
interval = 24

; Backup retention (days)
retention = 30

; Backup directory
backup_dir = /var/lib/scratchbird/backups
```

---

## 6. Startup Flow

### 6.1 Server Startup - Registry Phase

```
┌─────────────────────────────────────────────────────────────────────────────┐
│         PHASE 3: STARTUP DATABASES (Including Registry)                     │
└─────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────┐
    │ 3.1: Open Registry Database                                         │
    │                                                                     │
    │  ┌─────────────────────────────────────────────────────────────┐   │
    │  │  DatabaseRegistry::open("/var/lib/scratchbird/registry.db") │   │
    │  │                                                             │   │
    │  │  ┌───────────────────────────────────────────────────────┐ │   │
    │  │  │  core::Database::openEmbedded()                       │ │   │
    │  │  │  ├─ Open database file                                │ │   │
    │  │  │  ├─ Read header (validate format)                     │ │   │
    │  │  │  ├─ Initialize buffer pool                            │ │   │
    │  │  │  ├─ NO LOCK MANAGER (single user)                     │ │   │
    │  │  │  └─ NO GC THREAD (app-managed)                        │ │   │
    │  │  └───────────────────────────────────────────────────────┘ │   │
    │  │                                                             │   │
    │  │  if (new database) {                                        │   │
    │  │      initializeSchema()                                     │   │
    │  │  }                                                          │   │
    │  └─────────────────────────────────────────────────────────────┘   │
    │                                                                     │
    └─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
    ┌─────────────────────────────────────────────────────────────────────┐
    │ 3.2: Load System Databases from Registry                            │
    │                                                                     │
    │  ┌─────────────────────────────────────────────────────────────┐   │
    │  │  SELECT database_id, database_name, database_path, status  │   │
    │  │  FROM registered_databases                                 │   │
    │  │  WHERE status = 'active'                                   │   │
    │  └─────────────────────────────────────────────────────────────┘   │
    │                              │                                      │
    │                              ▼                                      │
    │  For each database:                                                  │
    │  ┌─────────────────────────────────────────────────────────────┐   │
    │  │  Database::open(database_path)                              │   │
    │  │  ├─ Open user database file                                  │   │
    │  │  ├─ Initialize buffer pool                                   │   │
    │  │  └─ (Lock manager started later in Phase 4)                  │   │
    │  │                                                              │   │
    │  │  startup_databases_[db_id] = database                        │   │
    │  └─────────────────────────────────────────────────────────────┘   │
    │                                                                     │
    └─────────────────────────────────────────────────────────────────────┘
```

---

## 7. AI Assistant Reminders

### 7.1 Correct vs Incorrect

| Wrong Approach | Correct Approach |
|---------------|------------------|
| "Use SQLite for registry" | Use ScratchBird itself (embedded mode) |
| "Use JSONB file for config" | Use ScratchBird tables |
| "Need separate backup system" | Same `sb_backup` works for registry |
| "External metadata storage" | Self-hosting: ScratchBird stores its own data |
| "Different storage for system" | Same storage engine for everything |

### 7.2 Key Principles

1. **ScratchBird is Self-Hosting**
   - It uses its own engine for all storage needs
   - No external dependencies for metadata

2. **Embedded Mode for System Databases**
   - Registry, security DB use embedded mode
   - Single user (the server process)
   - No lock manager overhead

3. **Same Format, Same Tools**
   - Registry is a `.db` file like any other
   - Same backup, restore, recovery
   - Same SQL interface

4. **No SQLite, No JSONB, No External Storage**
   - Never suggest SQLite for ScratchBird's data
   - Never suggest JSON files for configuration
   - ScratchBird is the storage engine

---

## 8. Related Specifications

- [SCRATCHBIRD_EMBEDDED_MODE_SPECIFICATION.md](SCRATCHBIRD_EMBEDDED_MODE_SPECIFICATION.md) - Embedded mode details
- [ARCHITECTURE_CLARIFICATIONS.md](ARCHITECTURE_CLARIFICATIONS.md) - Common AI misconceptions
- [SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md](SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md) - Startup phases