# ScratchBird Server Architecture and Connection Lifecycle Specification

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Version:** 1.0  
**Status:** Authoritative (V3)
**Last Updated:** February 2026  

## 1. Purpose

This specification defines the complete end-to-end flow of ScratchBird server operation, from initial installation through client connection, authentication, and session establishment. It ties together installation, configuration, database registry, network listeners, authentication, and session management into a cohesive architecture.

## 2. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           ScratchBird Server Architecture                        │
└─────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────┐
│                              INSTALLATION PHASE                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐ │
│  │ Create       │  │ Create       │  │ Generate     │  │ Create Database      │ │
│  │ Directory    │  │ Config File  │  │ Certificates │  │ Registry             │ │
│  │ Structure    │  │ (/etc/...)   │  │ (if needed)  │  │ (if not exists)      │ │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └──────────┬───────────┘ │
│         └─────────────────┴─────────────────┴─────────────────────┘              │
└─────────────────────────────────────────────────────────────────────────────────┘
                                         │
                                         ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              CONFIGURATION LAYER                                 │
│  ┌─────────────────────────────────────────────────────────────────────────────┐│
│  │  sb_server.conf (Main Server Config)                                         ││
│  │  ├── [server] - Data directories, default paths                              ││
│  │  ├── [network] - Listener ports, bind addresses                              ││
│  │  ├── [ssl] - TLS certificates, protocols                                     ││
│  │  ├── [authentication] - Auth methods, security DB                            ││
│  │  ├── [memory] - Buffer pools, cache sizes                                    ││
│  │  ├── [resources] - Language/collation/timezone paths                         ││
│  │  └── [registry] - Database registry path                                     ││
│  └─────────────────────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────────────────┘
                                         │
                                         ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                            RUNTIME ARCHITECTURE                                  │
│                                                                                  │
│   ┌─────────────────────────────────────────────────────────────────────────┐   │
│   │                     sb_server (Main Process)                             │   │
│   │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌────────────────┐ │   │
│   │  │ Native      │  │ PostgreSQL  │  │ MySQL       │  │ Firebird       │ │   │
│   │  │ Listener    │  │ Listener    │  │ Listener    │  │ Listener       │ │   │
│   │  │ :3092       │  │ :5432       │  │ :3306       │  │ :3050          │ │   │
│   │  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └───────┬────────┘ │   │
│   │         └─────────────────┴─────────────────┴─────────────────┘         │   │
│   │                              │                                          │   │
│   │                    ┌─────────┴─────────┐                                │   │
│   │                    │  Control Plane    │                                │   │
│   │                    │  (Parser Pools)   │                                │   │
│   │                    └─────────┬─────────┘                                │   │
│   │                              │                                          │   │
│   │  ┌───────────────────────────┼─────────────────────────────────────┐   │   │
│   │  │      DATABASE INSTANCES   │                                     │   │   │
│   │  │  ┌─────────┐ ┌─────────┐ │ ┌─────────┐ ┌─────────┐             │   │   │
│   │  │  │ DB_A    │ │ DB_B    │ │ │ DB_C    │ │ DB_D    │             │   │   │
│   │  │  │ (sales) │ │ (hr)    │ │ │ (prod)  │ │ (test)  │             │   │   │
│   │  │  └─────────┘ └─────────┘ │ └─────────┘ └─────────┘             │   │   │
│   │  │                          │                                      │   │   │
│   │  │  Database Registry: /var/lib/scratchbird/registry.sb            │   │   │
│   │  └────────────────────────────────────────────────────────────────┘   │   │
│   └─────────────────────────────────────────────────────────────────────────┘   │
│                                                                                  │
│   Resources: /usr/share/scratchbird/resources/                                   │
│   ├── languages/          (collation, charset definitions)                       │
│   ├── timezones/          (timezone database)                                    │
│   └── udr/                (user-defined routine libraries)                       │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

## 3. Directory Structure

### 3.1 *nix Systems (FHS Compliant)

```
/etc/scratchbird/                    # Configuration
    sb_server.conf                   # Main server configuration
    sb_hba.conf                      # Host-based authentication rules
    listeners.conf                   # Listener-specific overrides
    ssl/                             # TLS certificates (if local)
        server.crt
        server.key
        ca.crt

/var/lib/scratchbird/                # Data and runtime
    registry.sb                      # Database registry (see §4)
    databases/                       # Default database storage
        <database_id>/
            database.db              # Main database file
            database.log             # Write-ahead log
            temporary/               # Temporary tablespace
    run/                             # PID files, sockets
        scratchbird.pid
        listeners/
            native.sock
            pg.sock
    cache/                           # Query cache, plan cache

/var/log/scratchbird/                # Log files
    server.log
    listener_native.log
    listener_pg.log
    audit/
        YYYY/MM/DD/audit.log

/usr/share/scratchbird/              # Read-only resources
    resources/
        languages/                   # Collation and charset data
            en_US/
            de_DE/
            ...
        timezones/                   # IANA timezone database
            zoneinfo/
        charsets/                    # Character encoding mappings
        collations/                  # ICU collation data
    udr/                             # Default UDR libraries
    sql/                             # System SQL scripts
        create_security_database.sql
        upgrade_v1_to_v2.sql

/usr/lib/scratchbird/                # Shared libraries
    libscratchbird_core.so
    libscratchbird_embedded.so
    plugins/
        udr/
        storage/

/usr/bin/                            # Executables
    sb_server
    sb_isql
    sb_admin
    sb_backup
    sb_restore
    sb_security
    sb_setup                        # Configuration wizard
    sb_listener_*                   # Protocol listeners
    sb_parser_*                     # Protocol parsers
```

### 3.2 Windows Systems

```
%ProgramFiles%\ScratchBird\
    bin\                            # Executables
    lib\                            # DLLs
    config\                        # Configuration
        sb_server.conf
        sb_hba.conf
    resources\                     # Resources (languages, timezones)
    ssl\                           # TLS certificates

%ProgramData%\ScratchBird\
    data\                          # Database files
        registry.sb
        <database_id>\
    logs\                          # Log files
    temp\                          # Temporary files
    cache\                         # Caches
```

## 4. Database Registry

### 4.1 Purpose

The database registry maintains a catalog of all databases known to the server instance. Each port listener may have its own registry (enabling MSSQL-style "named instances"). When a client connects, they receive a list of databases they can access from the registry.

### 4.2 Registry Location

Default: `/var/lib/scratchbird/registry.sb` (or `%ProgramData%\ScratchBird\data\registry.sb` on Windows)

Configurable via `server.registry_path` in sb_server.conf.

### 4.3 Registry Schema

The registry is a SQLite database with the following structure:

```sql
-- Main database catalog
CREATE TABLE registered_databases (
    database_id         TEXT PRIMARY KEY,       -- UUID v7
    database_name       TEXT NOT NULL,          -- Human-readable name
    database_path       TEXT NOT NULL,          -- Filesystem path to .db file
    owner_user_id       TEXT NOT NULL,          -- UUID of owning user
    created_at          TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at          TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    -- Security model
    security_model      TEXT DEFAULT 'local',   -- local | shared | external
    security_db_path    TEXT,                   -- Path to security DB if not local
    
    -- Default settings (override server defaults)
    default_page_size   INTEGER,                -- 4096, 8192, 16384
    default_charset     TEXT,                   -- UTF8, ISO8859_1, etc.
    default_collation   TEXT,                   -- UTF8_GENERAL_CI, etc.
    
    -- Resource overrides
    buffer_pool_size    INTEGER,                -- Override server default
    max_connections     INTEGER,                -- Per-database limit
    
    -- Status
    status              TEXT DEFAULT 'active',  -- active | disabled | recovering
    last_attach_time    TIMESTAMP,
    attach_count        INTEGER DEFAULT 0,
    
    -- Audit
    created_by          TEXT,
    comments            TEXT
);

-- Database aliases (alternate names for databases)
CREATE TABLE database_aliases (
    alias_name          TEXT PRIMARY KEY,
    database_id         TEXT NOT NULL REFERENCES registered_databases(database_id),
    created_at          TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Database-specific user permissions (supplements security DB)
CREATE TABLE database_permissions (
    database_id         TEXT NOT NULL REFERENCES registered_databases(database_id),
    user_id             TEXT NOT NULL,
    permission_type     TEXT NOT NULL,          -- CONNECT | ADMIN | BACKUP
    granted_at          TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    granted_by          TEXT,
    PRIMARY KEY (database_id, user_id, permission_type)
);

-- Database storage locations (tablespaces)
CREATE TABLE database_tablespaces (
    tablespace_id       TEXT PRIMARY KEY,
    database_id         TEXT NOT NULL REFERENCES registered_databases(database_id),
    tablespace_name     TEXT NOT NULL,
    file_path           TEXT NOT NULL,
    file_size           INTEGER,                -- Current size in bytes
    max_size            INTEGER,                -- Max size (0 = unlimited)
    autoextend          BOOLEAN DEFAULT true,
    increment_size      INTEGER                 -- Auto-extend increment
);

-- Registry metadata
CREATE TABLE registry_metadata (
    key                 TEXT PRIMARY KEY,
    value               TEXT,
    description         TEXT
);

-- Insert registry version
INSERT INTO registry_metadata (key, value, description) VALUES
('schema_version', '1.0', 'Registry schema version'),
('created_at', datetime('now'), 'Registry creation timestamp'),
('server_instance', 'default', 'Server instance name');
```

### 4.4 Registry Operations

#### CREATE DATABASE

When a client executes `CREATE DATABASE`:

1. Generate new database UUID v7
2. Create database directory: `{data_dir}/{database_id}/`
3. Initialize database file with system catalog
4. Insert entry into `registered_databases`
5. Grant creator full admin rights
6. Fire database creation triggers

#### DROP DATABASE

1. Verify user has DROP permission
2. Mark database as 'disabled' in registry
3. Disconnect all attached users
4. Move database files to archive or delete
5. Remove registry entry (or mark as 'dropped' for audit)

#### Database Listing

When a client requests database list (via SBWP or protocol-specific command):

```sql
SELECT 
    d.database_name,
    d.database_id,
    d.comments,
    CASE WHEN dp.permission_type = 'CONNECT' THEN 1 ELSE 0 END as can_connect
FROM registered_databases d
LEFT JOIN database_permissions dp 
    ON d.database_id = dp.database_id 
    AND dp.user_id = :current_user_id
WHERE d.status = 'active'
  AND (d.security_model = 'local' OR dp.permission_type IS NOT NULL)
ORDER BY d.database_name;
```

### 4.5 Per-Port Registry Isolation

Different ports can serve different database sets (like MSSQL named instances):

```ini
[network]
; Port 3092 - Default instance, all databases
native_port = 3092
native_registry = /var/lib/scratchbird/registry.sb

; Port 13092 - Development instance, dev databases only  
native_dev_port = 13092
native_dev_registry = /var/lib/scratchbird/registry_dev.sb
```

## 5. Installation Process

### 5.1 Package Installation (Linux)

```bash
# Install server package
sudo apt install scratchbird-server    # Debian/Ubuntu
sudo dnf install scratchbird-server    # RHEL/Fedora

# Post-install script (run automatically)
# 1. Create directories
mkdir -p /var/lib/scratchbird/{databases,run,cache}
mkdir -p /var/log/scratchbird/audit
mkdir -p /etc/scratchbird/ssl
chown -R scratchbird:scratchbird /var/lib/scratchbird /var/log/scratchbird
chmod 750 /var/lib/scratchbird /var/log/scratchbird

# 2. Generate TLS certificate if not provided
if [ ! -f /etc/scratchbird/ssl/server.crt ]; then
    openssl req -new -x509 -days 365 -nodes \
        -out /etc/scratchbird/ssl/server.crt \
        -keyout /etc/scratchbird/ssl/server.key \
        -subj "/CN=$(hostname -f)"
    chown scratchbird:scratchbird /etc/scratchbird/ssl/server.*
    chmod 600 /etc/scratchbird/ssl/server.key
fi

# 3. Initialize database registry if not exists
if [ ! -f /var/lib/scratchbird/registry.sb ]; then
    sb_server --init-registry --registry-path /var/lib/scratchbird/registry.sb
fi

# 4. Create default security database if not exists
if [ ! -f /var/lib/scratchbird/security.db ]; then
    sb_server --init-security-db
fi

# 5. Enable and start service
systemctl enable scratchbird
systemctl start scratchbird
```

### 5.2 Configuration Wizard (sb_setup)

Interactive first-run configuration:

```bash
sb_setup --interactive

# Flow:
# 1. Detect system resources (CPU, RAM, disk)
# 2. Select installation profile:
#    - Development (minimal resources, all logging)
#    - Production (optimized for throughput)
#    - Embedded (minimal footprint)
# 3. Configure listeners:
#    - Which protocols to enable
#    - Port numbers
#    - TLS settings
# 4. Configure authentication:
#    - Local security database
#    - LDAP/Active Directory
#    - Kerberos
# 5. Set data directory locations
# 6. Configure memory settings
# 7. Apply configuration and restart
```

## 6. Connection Lifecycle

### 6.1 Overview

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                          CONNECTION LIFECYCLE                                    │
└─────────────────────────────────────────────────────────────────────────────────┘

Client                          Listener               Parser              Engine
  │                                │                     │                   │
  │── TLS Handshake (if enabled) ─▶│                     │                   │
  │◀──────── TLS Complete ─────────│                     │                   │
  │                                │                     │                   │
  │── SBWP: Startup Message ──────▶│                     │                   │
  │                                │── Socket Handoff ──▶│                   │
  │                                │                     │                   │
  │◀──── Auth Method Request ──────│◀────── Start ───────│                   │
  │                                │                     │                   │
  │────── Auth Credentials ───────▶│                     │                   │
  │                                │                     │── Auth Request ──▶│
  │                                │                     │◀── Auth Result ───│
  │◀──── Auth Success/Failure ─────│◀────────────────────│                   │
  │                                │                     │                   │
  │── Database List Request ──────▶│                     │                   │
  │                                │                     │── Query Registry──▶│
  │◀──── Database List ────────────│◀────────────────────│◀── Registry Data ──│
  │                                │                     │                   │
  │──── Attach Database ──────────▶│                     │                   │
  │                                │                     │── Attach Request ─▶│
  │                                │                     │◀── Attach Success ─│
  │                                │                     │                   │
  │◀──── Session Established ──────│◀────────────────────│◀── Session Start ──│
  │                                │                     │                   │
  │──── SQL Query ────────────────▶│                     │                   │
  │                                │                     │── Execute SBLR ───▶│
  │◀──── Query Results ────────────│◀────────────────────│◀── Results ────────│
  │                                │                     │                   │
  │──── Disconnect/Terminate ─────▶│                     │── Detach ────────▶│
  │                                │                     │◀── Detach Ack ─────│
  │                                │                     │── Exit             │
```

### 6.2 Phase 1: Network Connection

1. **Client initiates TCP connection** to listener port
2. **Listener accepts** and applies network policy:
   - Check connection limits
   - Verify source IP against allow/deny lists
   - Queue if necessary
3. **TLS Handshake** (if TLS enabled on port):
   - TLS 1.2 or 1.3 only
   - Server presents certificate
   - Optional client certificate verification
4. **Socket handoff** to parser worker from pool

### 6.3 Phase 2: Protocol Handshake

For SBWP (ScratchBird Wire Protocol):

```
Client                                          Server
  │                                               │
  │── Startup Message ───────────────────────────▶│
  │   - Protocol version (4 bytes)                │
  │   - Client capabilities (8 bytes)             │
  │   - Connection parameters (user, database,    │
  │     application_name, client_encoding, etc.)  │
  │                                               │
  │◀── Authentication Request ────────────────────│
  │   - Auth method (SCRAM-SHA-256, GSSAPI, etc.) │
  │   - Nonce/challenge (if applicable)           │
  │                                               │
  │── Authentication Response ───────────────────▶│
  │   - Credentials per auth method               │
  │                                               │
  │◀── Authentication Result ─────────────────────│
  │   - Success: session ID, server version       │
  │   - Failure: error code, message              │
```

### 6.4 Phase 3: Database Selection

After successful authentication but before database attachment:

**Option A: Client specifies database in startup**
- If database name provided in Startup Message → Direct attach
- If database doesn't exist → Error: "Database not found"
- If no permission → Error: "Permission denied"

**Option B: Client requests database list**
```
Client                                          Server
  │                                               │
  │── ListDatabases Request ─────────────────────▶│
  │                                               │
  │◀── Database List Response ────────────────────│
  │   - Database name                             │
  │   - Database ID                               │
  │   - Description                               │
  │   - Connect permission flag                   │
  │                                               │
  │── AttachDatabase Request ────────────────────▶│
  │   - Database name or ID                       │
  │   - Attach options (read-only, etc.)          │
  │                                               │
  │◀── Attach Result ─────────────────────────────│
  │   - Success: database handle, schema info     │
  │   - Failure: error code                       │
```

### 6.5 Phase 4: Session Initialization

Once attached to a database:

1. **Load user profile**:
   - Default schema (from user account or database default)
   - User roles and group memberships
   - Session parameters (lock timeout, statement timeout)

2. **Fire connection triggers**:
   ```sql
   -- Database-level trigger
   CREATE TRIGGER ON CONNECT
   EXECUTE PROCEDURE log_connection();
   
   -- User-level trigger could set session variables
   SET SESSION TIMEZONE = (SELECT timezone FROM user_preferences WHERE user_id = CURRENT_USER);
   ```

3. **Begin implicit transaction** (if auto-commit enabled, transaction per statement)

4. **Send ready for query**:
   ```
   Client ◀── ReadyForQuery ─── Server
            - Transaction status (idle/in_transaction/error)
            - Session parameters
   ```

### 6.6 Phase 5: Query Execution

```
Client                                          Server (Parser + Engine)
  │                                               │
  │── Query Message ─────────────────────────────▶│
  │   - SQL text or prepared statement handle     │
  │   - Parameters (if prepared statement)        │
  │                                               │
  │     [Parser: Parse SQL → SBLR]                │
  │     [Engine: Validate, Plan, Execute]         │
  │                                               │
  │◀── Query Results ─────────────────────────────│
  │   - RowDescription (column names, types)      │
  │   - DataRow (actual data)                     │
  │   - CommandComplete (affected rows, etc.)     │
  │                                               │
  │◀── ReadyForQuery ─────────────────────────────│
```

### 6.7 Phase 6: Disconnection

```
Client                                          Server
  │                                               │
  │── Terminate Message ─────────────────────────▶│
  │   OR connection closed                        │
  │                                               │
  │     [Rollback uncommitted transaction]        │
  │     [Fire ON DISCONNECT triggers]             │
  │     [Release session resources]               │
  │     [Detach from database]                    │
  │     [Log disconnection]                       │
  │                                               │
  │◀── Connection Closed ◀────────────────────────│
```

## 7. Authentication Flow

### 7.1 Authentication Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Authentication Process                               │
└─────────────────────────────────────────────────────────────────────────────┘

                        ┌─────────────────────────────────────┐
                        │         Authentication Types         │
                        ├─────────────────────────────────────┤
                        │ 1. Local (security database)        │
                        │ 2. Database-local (per-db users)    │
                        │ 3. Shared Security Database         │
                        │ 4. External (LDAP/AD/Kerberos)      │
                        │ 5. Certificate (TLS client cert)    │
                        │ 6. Multi-Factor (MFA)               │
                        └──────────────┬──────────────────────┘
                                       │
                                       ▼
┌─────────────┐     ┌──────────────────────────────────────┐     ┌───────────┐
│   Client    │────▶│   Authentication Negotiation         │────▶│  Engine   │
│             │     │                                      │     │  Security │
└─────────────┘     │  1. Client sends auth method prefs   │     │  Manager  │
                    │  2. Server selects method            │     │           │
                    │  3. Challenge/Response exchange      │     └───────────┘
                    │  4. Identity verification            │           │
                    │  5. Session token generation         │           ▼
                    └──────────────────────────────────────┘    ┌───────────┐
                                                                  │ User/Role │
                                                                  │ Resolution│
                                                                  └───────────┘
```

### 7.2 Authentication Methods

#### 7.2.1 SCRAM-SHA-256 (Default)

```
Client                                      Server
  │                                           │
  │── ClientFirst (username, nonce) ────────▶│
  │                                           │── Lookup user
  │◀── ServerFirst (salt, iteration, nonce) ─│◀── Get stored key
  │                                           │
  │── ClientProof (computed HMAC) ──────────▶│── Verify proof
  │                                           │
  │◀── ServerSignature ──────────────────────│◀── Auth success
```

#### 7.2.2 Kerberos/GSSAPI

```
Client                                      Server
  │                                           │
  │── GSSAPI Init (service ticket) ─────────▶│── Validate with KDC
  │                                           │
  │◀── GSSAPI Continue/Complete ─────────────│◀── Mutual auth
  │                                           │
  │── Encrypted user identity ──────────────▶│── Map to SB user
```

#### 7.2.3 TLS Client Certificate

```
Client                                      Server
  │                                           │
  ├── TLS Handshake with client cert ──────▶│── Extract CN/SAN
  │                                           │── Map cert to user
  │◀── TLS Complete + Auth Success ──────────│
```

### 7.3 User Resolution

After identity verification, resolve to ScratchBird user:

```sql
-- Map external identity to SB user
SELECT user_id, user_name, default_schema, roles
FROM security_users
WHERE 
    -- For local auth
    (auth_method = 'local' AND user_name = :username)
    -- For Kerberos
    OR (auth_method = 'kerberos' AND krb_principal = :principal)
    -- For LDAP
    OR (auth_method = 'ldap' AND ldap_dn = :dn)
    -- For certificate
    OR (auth_method = 'cert' AND cert_fingerprint = :fingerprint);
```

### 7.4 Role and Permission Resolution

```sql
-- Get direct roles
SELECT role_name FROM user_roles WHERE user_id = :user_id;

-- Get inherited roles (recursive)
WITH RECURSIVE role_tree AS (
    SELECT role_id, role_name FROM roles WHERE role_id IN (
        SELECT role_id FROM user_roles WHERE user_id = :user_id
    )
    UNION ALL
    SELECT r.role_id, r.role_name 
    FROM roles r
    JOIN role_hierarchy h ON r.role_id = h.child_role_id
    JOIN role_tree rt ON h.parent_role_id = rt.role_id
)
SELECT * FROM role_tree;

-- Get effective permissions
SELECT permission, object_type, object_name, grant_option
FROM effective_permissions(:user_id, :database_id);
```

## 8. Session Management

### 8.1 Session State

Each connection has:

| Attribute | Description |
|-----------|-------------|
| session_id | UUID v7 unique identifier |
| user_id | Authenticated user |
| database_id | Currently attached database |
| default_schema | Schema for unqualified names |
| current_transaction | Active transaction ID (if any) |
| isolation_level | READ COMMITTED, REPEATABLE READ, SERIALIZABLE |
| auto_commit | true/false |
| statement_timeout | milliseconds |
| lock_timeout | milliseconds |
| timezone | session timezone |
| client_encoding | character encoding |
| application_name | client application identifier |

### 8.2 Session Variables

```sql
-- Set session variable
SET SESSION TIMEZONE = 'America/New_York';
SET SESSION lock_timeout = 30000;

-- Get session variable
SHOW TIMEZONE;
SELECT current_setting('lock_timeout');
```

### 8.3 Session Triggers

```sql
-- Database-level connection trigger
CREATE TRIGGER trg_on_connect
ON DATABASE CONNECT
EXECUTE PROCEDURE init_user_session();

-- Disconnection trigger
CREATE TRIGGER trg_on_disconnect
ON DATABASE DISCONNECT
EXECUTE PROCEDURE log_session_stats();
```

## 9. Multi-Instance Support

Multiple ScratchBird server instances can run on the same host:

```ini
; Instance 1: /etc/scratchbird/sb_server.conf
[server]
instance_name = production
data_directory = /var/lib/scratchbird/prod
registry_path = /var/lib/scratchbird/prod/registry.sb
pid_file = /var/run/scratchbird/prod.pid

[network]
native_port = 3092
pg_port = 5432

; Instance 2: /etc/scratchbird/sb_server_dev.conf
[server]
instance_name = development
data_directory = /var/lib/scratchbird/dev
registry_path = /var/lib/scratchbird/dev/registry.sb
pid_file = /var/run/scratchbird/dev.pid

[network]
native_port = 13092
pg_port = 15432
```

Start instances:
```bash
sb_server --config /etc/scratchbird/sb_server.conf
sb_server --config /etc/scratchbird/sb_server_dev.conf
```

## 10. Related Specifications

- [Installation and Build](../deployment/INSTALLATION_AND_BUILD_SPECIFICATION.md)
- [Installer and Config Generator](../deployment/INSTALLER_FEATURES_AND_CONFIG_GENERATOR.md)
- [Systemd Service](../deployment/SYSTEMD_SERVICE_SPECIFICATION.md)
- [Network Listener and Parser Pool](../network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md)
- [Security Architecture](../Security Design Specification/01_SECURITY_ARCHITECTURE.md)
- [Identity and Authentication](../Security Design Specification/02_IDENTITY_AUTHENTICATION.md)
- [Authorization Model](../Security Design Specification/03_AUTHORIZATION_MODEL.md)
- [TLS/SSL Configuration](../Security Design Specification/AUTH_CERTIFICATE_TLS.md)
- [IPC Security](../Security Design Specification/05_IPC_SECURITY.md)
- [System Catalog](../catalog/SYSTEM_CATALOG_STRUCTURE.md)
- [Y-Valve Architecture](../core/Y_VALVE_ARCHITECTURE.md)

## 11. Implementation Status

| Component | Status | Notes |
|-----------|--------|-------|
| Directory structure | 🟢 Implemented | FHS-compliant layout |
| TLS certificate generation | 🟡 Partial | Manual creation supported |
| Database registry | 🔴 Not implemented | SQLite registry needed |
| Per-port registry isolation | 🔴 Not implemented | Future feature |
| sb_setup wizard | 🔴 Not implemented | Configuration generator needed |
| SBWP protocol | 🟡 Partial | Basic messages implemented |
| Database list command | 🔴 Not implemented | Registry query needed |
| Session triggers | 🔴 Not implemented | Future feature |
| Multi-instance support | 🟡 Partial | Config files work, registry isolation doesn't |
