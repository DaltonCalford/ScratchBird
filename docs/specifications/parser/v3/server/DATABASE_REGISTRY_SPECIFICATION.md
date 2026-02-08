# Database Registry Specification

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Version:** 1.0  
**Status:** Authoritative (V3)
**Last Updated:** February 2026  

## 1. Purpose

Define the database registry architecture - a centralized catalog that tracks all databases managed by a ScratchBird server instance. The registry enables:

- Multiple databases per server instance
- Database discovery (list available databases)
- Per-database configuration overrides
- Database-level security policies
- Named instances (MSSQL-style multi-tenancy)

## 2. Concepts

### 2.1 Registry vs Database

| Aspect | Registry | Database |
|--------|----------|----------|
| **Purpose** | Catalog of databases | Actual user data storage |
| **Format** | SQLite database | ScratchBird proprietary format |
| **Location** | Server instance path | Configurable per database |
| **Contents** | Metadata, permissions, paths | Tables, indexes, data |
| **Access** | Server only | Server + attached clients |

### 2.2 Security Models

**Local Security (default):**
- Users defined in database-local security tables
- Each database has independent user accounts
- Suitable for development, isolated applications

**Shared Security Database:**
- Central security database (`security.db`)
- Multiple databases share user accounts
- Single sign-on across databases
- Suitable for enterprise deployments

**External Authentication:**
- LDAP/Active Directory integration
- Kerberos authentication
- No local user storage (or minimal mapping)

## 3. Registry Schema

### 3.1 Core Tables

```sql
-- ============================================================
-- DATABASE REGISTRY SCHEMA v1.0
-- ============================================================

-- Main database catalog
CREATE TABLE registered_databases (
    -- Identity
    database_id         TEXT PRIMARY KEY,           -- UUID v7
    database_name       TEXT NOT NULL UNIQUE,       -- Human-readable name
    database_alias      TEXT,                       -- Short alias (optional)
    
    -- Storage
    database_path       TEXT NOT NULL,              -- Path to .db file
    database_log_path   TEXT,                       -- Path to WAL/log
    
    -- Ownership
    owner_user_id       TEXT NOT NULL,              -- UUID of creator/owner
    owner_user_name     TEXT,                       -- Denormalized for display
    
    -- Timestamps
    created_at          TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at          TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_attach_time    TIMESTAMP,                  -- Last connection
    
    -- Security model
    security_model      TEXT DEFAULT 'local',       -- local | shared | external
    security_db_path    TEXT,                       -- Path to security DB
    
    -- Default settings (override server defaults)
    default_page_size   INTEGER DEFAULT 8192,       -- 4096 | 8192 | 16384
    default_charset     TEXT DEFAULT 'UTF8',        -- Character set
    default_collation   TEXT DEFAULT 'UTF8_GENERAL_CI',
    
    -- Resource limits (NULL = use server default)
    buffer_pool_size    INTEGER,                    -- Bytes
    max_connections     INTEGER,                    -- Per-database limit
    max_statement_time  INTEGER,                    -- Milliseconds
    
    -- Status
    status              TEXT DEFAULT 'active',      -- active | disabled | 
                                                    -- maintenance | recovering
    attach_count        INTEGER DEFAULT 0,          -- Total connections
    
    -- Comments
    description         TEXT,                       -- User description
    tags                TEXT,                       -- JSON array of tags
    
    -- Audit
    created_by          TEXT,                       -- Original creator
    created_from_host   TEXT,                       -- Host where created
    
    -- Constraints
    CHECK (security_model IN ('local', 'shared', 'external')),
    CHECK (status IN ('active', 'disabled', 'maintenance', 'recovering')),
    CHECK (default_page_size IN (4096, 8192, 16384))
);

-- Create indexes
CREATE INDEX idx_db_name ON registered_databases(database_name);
CREATE INDEX idx_db_status ON registered_databases(status);
CREATE INDEX idx_db_security ON registered_databases(security_model);
CREATE INDEX idx_db_owner ON registered_databases(owner_user_id);

-- ============================================================
-- ALIASES
-- ============================================================

CREATE TABLE database_aliases (
    alias_name          TEXT PRIMARY KEY,
    database_id         TEXT NOT NULL REFERENCES registered_databases(database_id)
                        ON DELETE CASCADE,
    created_at          TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_by          TEXT
);

CREATE INDEX idx_alias_db ON database_aliases(database_id);

-- ============================================================
-- PERMISSIONS
-- ============================================================

-- Database-level permissions (supplements security DB)
CREATE TABLE database_permissions (
    database_id         TEXT NOT NULL REFERENCES registered_databases(database_id)
                        ON DELETE CASCADE,
    user_id             TEXT NOT NULL,              -- From security DB
    user_name           TEXT,                       -- Denormalized
    
    -- Permission types
    can_connect         BOOLEAN DEFAULT false,      -- Basic connect
    can_create_schema   BOOLEAN DEFAULT false,      -- CREATE SCHEMA
    can_create_table    BOOLEAN DEFAULT false,      -- CREATE TABLE
    can_create_view     BOOLEAN DEFAULT false,      -- CREATE VIEW
    can_create_proc     BOOLEAN DEFAULT false,      -- CREATE PROCEDURE
    can_backup          BOOLEAN DEFAULT false,      -- BACKUP DATABASE
    can_restore         BOOLEAN DEFAULT false,      -- RESTORE DATABASE
    is_admin            BOOLEAN DEFAULT false,      -- Full admin rights
    
    -- Grant info
    granted_at          TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    granted_by          TEXT,
    expires_at          TIMESTAMP,                  -- Optional expiration
    
    PRIMARY KEY (database_id, user_id)
);

CREATE INDEX idx_perm_user ON database_permissions(user_id);
CREATE INDEX idx_perm_connect ON database_permissions(database_id, can_connect) 
    WHERE can_connect = true;

-- Role-based permissions (alternative to direct grants)
CREATE TABLE database_role_permissions (
    database_id         TEXT NOT NULL REFERENCES registered_databases(database_id)
                        ON DELETE CASCADE,
    role_name           TEXT NOT NULL,
    
    can_connect         BOOLEAN DEFAULT false,
    can_create_schema   BOOLEAN DEFAULT false,
    can_create_table    BOOLEAN DEFAULT false,
    can_create_view     BOOLEAN DEFAULT false,
    can_create_proc     BOOLEAN DEFAULT false,
    can_backup          BOOLEAN DEFAULT false,
    can_restore         BOOLEAN DEFAULT false,
    is_admin            BOOLEAN DEFAULT false,
    
    granted_at          TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    granted_by          TEXT,
    
    PRIMARY KEY (database_id, role_name)
);

-- ============================================================
-- TABLESPACES
-- ============================================================

CREATE TABLE database_tablespaces (
    tablespace_id       TEXT PRIMARY KEY,           -- UUID
    database_id         TEXT NOT NULL REFERENCES registered_databases(database_id)
                        ON DELETE CASCADE,
    
    tablespace_name     TEXT NOT NULL,              -- Logical name
    tablespace_type     TEXT DEFAULT 'data',        -- data | index | temp | blob
    
    -- File info
    file_path           TEXT NOT NULL,              -- Absolute path
    file_size           INTEGER DEFAULT 0,          -- Current size (bytes)
    max_size            INTEGER DEFAULT 0,          -- 0 = unlimited
    autoextend          BOOLEAN DEFAULT true,
    increment_size      INTEGER DEFAULT 10485760,   -- 10MB default increment
    
    -- Status
    is_online           BOOLEAN DEFAULT true,
    is_readonly         BOOLEAN DEFAULT false,
    
    created_at          TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    UNIQUE (database_id, tablespace_name)
);

CREATE INDEX idx_ts_database ON database_tablespaces(database_id);

-- ============================================================
-- STATISTICS
-- ============================================================

CREATE TABLE database_statistics (
    database_id         TEXT PRIMARY KEY REFERENCES registered_databases(database_id)
                        ON DELETE CASCADE,
    
    -- Connection stats
    total_connects      INTEGER DEFAULT 0,
    total_disconnects   INTEGER DEFAULT 0,
    current_connections INTEGER DEFAULT 0,
    max_connections_reached INTEGER DEFAULT 0,
    
    -- Query stats
    queries_executed    INTEGER DEFAULT 0,
    queries_failed      INTEGER DEFAULT 0,
    avg_query_time_ms   REAL DEFAULT 0,
    
    -- Storage stats
    data_size_bytes     INTEGER DEFAULT 0,
    index_size_bytes    INTEGER DEFAULT 0,
    blob_size_bytes     INTEGER DEFAULT 0,
    free_space_bytes    INTEGER DEFAULT 0,
    
    -- Time tracking
    first_connect_time  TIMESTAMP,
    last_connect_time   TIMESTAMP,
    last_disconnect_time TIMESTAMP,
    total_uptime_seconds INTEGER DEFAULT 0,
    
    -- Updated automatically
    stats_updated_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- ============================================================
-- BACKUP HISTORY
-- ============================================================

CREATE TABLE database_backups (
    backup_id           TEXT PRIMARY KEY,           -- UUID
    database_id         TEXT NOT NULL REFERENCES registered_databases(database_id)
                        ON DELETE CASCADE,
    
    backup_type         TEXT NOT NULL,              -- full | incremental | diff
    backup_path         TEXT NOT NULL,              -- Path to backup file
    backup_size_bytes   INTEGER,
    
    started_at          TIMESTAMP NOT NULL,
    completed_at        TIMESTAMP,
    duration_seconds    INTEGER,
    
    status              TEXT DEFAULT 'running',     -- running | completed | failed
    error_message       TEXT,
    
    started_by          TEXT,                       -- User who initiated
    
    -- For incremental/differential
    base_backup_id      TEXT REFERENCES database_backups(backup_id)
);

CREATE INDEX idx_backup_db ON database_backups(database_id);
CREATE INDEX idx_backup_time ON database_backups(completed_at);

-- ============================================================
-- REGISTRY METADATA
-- ============================================================

CREATE TABLE registry_metadata (
    key                 TEXT PRIMARY KEY,
    value               TEXT,
    description         TEXT
);

-- Initial data
INSERT INTO registry_metadata (key, value, description) VALUES
('schema_version', '1.0', 'Registry schema version'),
('registry_created_at', datetime('now'), 'Registry creation timestamp'),
('server_instance', 'default', 'Server instance name'),
('registry_format', 'sqlite', 'Storage format');

-- ============================================================
-- VIEWS
-- ============================================================

-- User-friendly database list with permission check
CREATE VIEW user_database_list AS
SELECT 
    d.database_id,
    d.database_name,
    d.database_alias,
    d.description,
    d.status,
    d.created_at,
    d.last_attach_time,
    d.attach_count,
    CASE WHEN dp.can_connect = 1 THEN 1 ELSE 0 END as can_connect,
    CASE WHEN dp.is_admin = 1 THEN 1 ELSE 0 END as is_admin
FROM registered_databases d
LEFT JOIN database_permissions dp 
    ON d.database_id = dp.database_id
WHERE d.status = 'active';

-- Database summary with sizes
CREATE VIEW database_summary AS
SELECT 
    d.database_id,
    d.database_name,
    d.status,
    d.security_model,
    COALESCE(s.data_size_bytes, 0) + COALESCE(s.index_size_bytes, 0) as total_size,
    s.current_connections,
    s.queries_executed,
    d.created_at
FROM registered_databases d
LEFT JOIN database_statistics s ON d.database_id = s.database_id;
```

## 4. Registry Operations

### 4.1 CREATE DATABASE

```sql
-- Procedure: create_database
-- Parameters:
--   - database_name: Name for the new database
--   - owner_id: UUID of creating user
--   - options: JSON with optional parameters

PROCEDURE create_database(
    IN database_name TEXT,
    IN owner_id TEXT,
    IN options JSON,
    OUT database_id TEXT,
    OUT database_path TEXT
)
```

**Steps:**

1. **Validate name**: Check uniqueness, valid characters
2. **Generate UUID**: UUID v7 for database_id
3. **Create directory**: `{data_dir}/{database_id}/`
4. **Initialize database file**:
   - Create `.db` file with system catalog
   - Create `.log` file (WAL)
5. **Insert registry entry**:
   ```sql
   INSERT INTO registered_databases (
       database_id, database_name, database_path,
       owner_user_id, security_model, ...
   ) VALUES (?, ?, ?, ?, 'local', ...);
   ```
6. **Grant owner permissions**:
   ```sql
   INSERT INTO database_permissions (
       database_id, user_id, can_connect, is_admin, ...
   ) VALUES (?, ?, true, true, ...);
   ```
7. **Initialize statistics**:
   ```sql
   INSERT INTO database_statistics (database_id) VALUES (?);
   ```
8. **Fire triggers**: Execute ON DATABASE CREATE triggers

### 4.2 DROP DATABASE

```sql
PROCEDURE drop_database(
    IN database_name TEXT,
    IN drop_by_user_id TEXT,
    IN options JSON  -- {force: boolean, backup_first: boolean}
)
```

**Steps:**

1. **Verify permissions**: User must have is_admin or DROP ANY DATABASE
2. **Check active connections**: Fail if connected (unless force=true)
3. **Backup** (if requested)
4. **Mark disabled**:
   ```sql
   UPDATE registered_databases 
   SET status = 'disabled', updated_at = datetime('now')
   WHERE database_name = ?;
   ```
5. **Disconnect all clients** (force mode)
6. **Fire triggers**: ON DATABASE DROP
7. **Archive or delete files**:
   - Option A: Move to `{data_dir}/.trash/{database_id}/`
   - Option B: Permanent deletion
8. **Remove registry entries** (or keep for audit)

### 4.3 ATTACH DATABASE (Connection)

```sql
PROCEDURE attach_database(
    IN database_name TEXT,
    IN user_id TEXT,
    IN attach_options JSON,  -- {read_only: boolean, schema: string}
    OUT session_info JSON
)
```

**Steps:**

1. **Lookup database**:
   ```sql
   SELECT * FROM registered_databases 
   WHERE database_name = ? AND status = 'active';
   ```
2. **Verify permissions**:
   ```sql
   SELECT can_connect, is_admin FROM database_permissions
   WHERE database_id = ? AND user_id = ?;
   ```
3. **Check connection limit**:
   ```sql
   SELECT current_connections, max_connections 
   FROM database_statistics WHERE database_id = ?;
   ```
4. **Update statistics**:
   ```sql
   UPDATE database_statistics SET
       current_connections = current_connections + 1,
       total_connects = total_connects + 1,
       last_connect_time = datetime('now')
   WHERE database_id = ?;
   ```
5. **Update database record**:
   ```sql
   UPDATE registered_databases SET
       last_attach_time = datetime('now'),
       attach_count = attach_count + 1
   WHERE database_id = ?;
   ```
6. **Return session info**: database_id, paths, default schema, user permissions

### 4.4 DETACH DATABASE (Disconnection)

```sql
PROCEDURE detach_database(
    IN database_id TEXT,
    IN session_stats JSON  -- Optional query stats from session
)
```

**Steps:**

1. **Update statistics**:
   ```sql
   UPDATE database_statistics SET
       current_connections = current_connections - 1,
       total_disconnects = total_disconnects + 1,
       last_disconnect_time = datetime('now')
   WHERE database_id = ?;
   ```
2. **Update aggregate stats** (if session_stats provided)
3. **Fire triggers**: ON DATABASE DISCONNECT

### 4.5 LIST DATABASES

```sql
-- For authenticated user
SELECT 
    database_id,
    database_name,
    database_alias,
    description,
    can_connect,
    is_admin
FROM user_database_list
WHERE can_connect = 1
ORDER BY database_name;
```

## 5. API Interface

### 5.1 C++ Registry Manager Class

```cpp
namespace scratchbird {
namespace server {

class DatabaseRegistry {
public:
    struct DatabaseInfo {
        std::string database_id;
        std::string database_name;
        std::string database_path;
        std::string security_model;
        std::string status;
        // ... other fields
    };
    
    struct CreateOptions {
        std::optional<std::string> page_size;
        std::optional<std::string> charset;
        std::optional<std::string> security_model;
        std::optional<std::string> description;
    };
    
    struct PermissionInfo {
        bool can_connect = false;
        bool can_create_schema = false;
        bool can_create_table = false;
        bool is_admin = false;
    };

    // Lifecycle
    static std::unique_ptr<DatabaseRegistry> open(
        const std::string& registry_path,
        core::ErrorContext* ctx = nullptr);
    
    bool initializeNewRegistry(core::ErrorContext* ctx = nullptr);
    
    // Database operations
    core::Status createDatabase(
        const std::string& database_name,
        const std::string& owner_user_id,
        const CreateOptions& options,
        std::string* out_database_id,
        core::ErrorContext* ctx = nullptr);
    
    core::Status dropDatabase(
        const std::string& database_name,
        const std::string& drop_by_user_id,
        bool force = false,
        core::ErrorContext* ctx = nullptr);
    
    core::Status attachDatabase(
        const std::string& database_name,
        const std::string& user_id,
        DatabaseInfo* out_info,
        PermissionInfo* out_perms,
        core::ErrorContext* ctx = nullptr);
    
    core::Status detachDatabase(
        const std::string& database_id,
        core::ErrorContext* ctx = nullptr);
    
    // Queries
    std::vector<DatabaseInfo> listDatabases(
        const std::string& for_user_id,
        bool include_all = false);
    
    std::optional<DatabaseInfo> getDatabaseByName(
        const std::string& database_name);
    
    std::optional<DatabaseInfo> getDatabaseById(
        const std::string& database_id);
    
    // Permissions
    core::Status grantPermission(
        const std::string& database_id,
        const std::string& user_id,
        const PermissionInfo& perms,
        const std::string& granted_by_user_id);
    
    core::Status revokePermission(
        const std::string& database_id,
        const std::string& user_id,
        const std::string& revoked_by_user_id);
    
    // Statistics
    core::Status updateStatistics(
        const std::string& database_id,
        const SessionStatistics& stats);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace server
} // namespace scratchbird
```

### 5.2 IPC Messages

```cpp
// Message: CreateDatabaseRequest
struct CreateDatabaseRequest {
    static constexpr uint32_t MESSAGE_TYPE = 0xD001;
    
    char database_name[128];
    char owner_user_id[37];     // UUID string
    uint32_t options_length;
    char options_json[2048];    // JSON-encoded CreateOptions
};

// Message: CreateDatabaseResponse
struct CreateDatabaseResponse {
    static constexpr uint32_t MESSAGE_TYPE = 0xD002;
    
    uint32_t status;            // 0 = success
    char database_id[37];
    char database_path[512];
    char error_message[256];
};

// Message: ListDatabasesRequest
struct ListDatabasesRequest {
    static constexpr uint32_t MESSAGE_TYPE = 0xD003;
    
    char requesting_user_id[37];
    uint32_t flags;             // 1 = include_all (admin only)
};

// Message: DatabaseListEntry
struct DatabaseListEntry {
    char database_id[37];
    char database_name[128];
    char description[256];
    uint32_t can_connect;       // 0 or 1
    uint32_t is_admin;          // 0 or 1
};

// Message: ListDatabasesResponse
struct ListDatabasesResponse {
    static constexpr uint32_t MESSAGE_TYPE = 0xD004;
    
    uint32_t status;
    uint32_t database_count;
    // Followed by database_count * DatabaseListEntry
};

// Message: AttachDatabaseRequest
struct AttachDatabaseRequest {
    static constexpr uint32_t MESSAGE_TYPE = 0xD005;
    
    char database_name[128];    // or database_id
    char user_id[37];
    uint32_t flags;             // bit 0 = read_only
};

// Message: AttachDatabaseResponse
struct AttachDatabaseResponse {
    static constexpr uint32_t MESSAGE_TYPE = 0xD006;
    
    uint32_t status;
    char database_id[37];
    char database_path[512];
    char default_schema[128];
    uint32_t permissions;       // Bitmask of PermissionInfo
    char error_message[256];
};
```

## 6. Integration Points

### 6.1 Server Startup

```cpp
int Server::initialize() {
    // 1. Load configuration
    config_ = loadConfig(config_path_);
    
    // 2. Open or create registry
    registry_ = DatabaseRegistry::open(config_.registry_path, &ctx);
    if (!registry_) {
        // Try to create new registry
        registry_ = DatabaseRegistry::open(config_.registry_path, &ctx);
        if (!registry_->initializeNewRegistry(&ctx)) {
            LOG_FATAL("Failed to initialize database registry: {}", ctx.message);
            return 1;
        }
    }
    
    // 3. Verify each configured database exists in registry
    for (const auto& db_name : config_.default_databases) {
        auto db_info = registry_->getDatabaseByName(db_name);
        if (!db_info) {
            LOG_WARN("Configured database '{}' not found in registry", db_name);
        }
    }
    
    // 4. Start listeners
    // ...
}
```

### 6.2 Parser Integration

When parser receives database list request:

```cpp
void Parser::handleListDatabases() {
    // Get authenticated user
    auto user_id = session_->getAuthenticatedUserId();
    
    // Query registry
    auto databases = registry_->listDatabases(user_id, false);
    
    // Send response
    ListDatabasesResponse response;
    response.database_count = databases.size();
    send(response);
    
    for (const auto& db : databases) {
        DatabaseListEntry entry;
        strncpy(entry.database_name, db.database_name.c_str(), 128);
        // ... fill other fields
        send(entry);
    }
}
```

### 6.3 SQL Integration

```sql
-- SQL-level access to registry (admin only)

-- List databases (like MSSQL: SELECT * FROM sys.databases)
SELECT * FROM scratchbird_sys.databases;

-- Create database
CREATE DATABASE sales 
    OWNER = 'admin'
    DESCRIPTION = 'Sales department database'
    PAGE_SIZE = 16384;

-- Drop database
DROP DATABASE sales;

-- Grant access
GRANT CONNECT ON DATABASE sales TO 'john_doe';
GRANT ADMIN ON DATABASE sales TO 'jane_smith';

-- Revoke access
REVOKE CONNECT ON DATABASE sales FROM 'john_doe';
```

## 7. Migration from Single-Database Mode

For backward compatibility with existing single-database deployments:

```cpp
void migrateSingleDatabase(const std::string& db_file_path) {
    // 1. Check if database file exists
    if (!fileExists(db_file_path)) {
        return;
    }
    
    // 2. Check if already registered
    auto existing = registry_->getDatabaseByName("default");
    if (existing) {
        return;  // Already migrated
    }
    
    // 3. Register existing database
    CreateOptions opts;
    opts.description = "Migrated default database";
    
    std::string database_id;
    registry_->createDatabase(
        "default",          // database_name
        "SYSDBA",           // owner
        opts,
        &database_id,
        nullptr
    );
    
    // 4. Update registry with actual path
    registry_->updateDatabasePath(database_id, db_file_path);
    
    LOG_INFO("Migrated database to registry: {}", database_id);
}
```

## 8. Related Specifications

- [Server Architecture](SERVER_ARCHITECTURE_AND_CONNECTION_LIFECYCLE.md)
- [System Catalog](../catalog/SYSTEM_CATALOG_STRUCTURE.md)
- [Security Architecture](../Security Design Specification/01_SECURITY_ARCHITECTURE.md)
- [Authorization Model](../Security Design Specification/03_AUTHORIZATION_MODEL.md)
- [IPC Contract](../network/ENGINE_PARSER_IPC_CONTRACT.md)

## 9. Implementation Notes

### 9.1 Concurrency

- Registry uses SQLite with WAL mode for concurrent readers
- Write operations (CREATE/DROP DATABASE) use exclusive locking
- Read operations (LIST/ATTACH) can proceed concurrently

### 9.2 Backup Considerations

- Registry should be backed up along with databases
- Registry backup includes metadata but not actual data
- Point-in-time recovery requires consistent backup of registry + databases

### 9.3 Replication

- Registry is instance-local (not replicated)
- Each replica in a cluster has its own registry
- Database IDs are globally unique (UUID) to prevent conflicts
