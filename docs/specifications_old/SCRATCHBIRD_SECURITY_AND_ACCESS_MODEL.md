# ScratchBird Security and Access Model


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

## 1. Overview

ScratchBird uses a **decentralized, database-centric security model** where each database has its own owner and security configuration. The server can operate in three modes:

1. **Standalone Mode** - No shared security, each database is independent
2. **Shared Security Mode** - Central security database controls access
3. **Cluster Mode** - Shared security distributed across server group

**Key Principle:** Database ownership is per-database, not server-wide. The database creator becomes its administrator.

---

## 2. Server Security Modes

### 2.1 Mode Comparison

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    SECURITY MODE COMPARISON                                 │
└─────────────────────────────────────────────────────────────────────────────┘

┌──────────────────┬────────────────────┬────────────────────┬────────────────┐
│     Aspect       │   STANDALONE       │  SHARED SECURITY   │    CLUSTER     │
├──────────────────┼────────────────────┼────────────────────┼────────────────┤
│                  │                    │                    │                │
│ Security DB      │ None (per-DB)      │ security.db        │ Distributed    │
│                  │                    │                    │ security.db    │
├──────────────────┼────────────────────┼────────────────────┼────────────────┤
│                  │                    │                    │                │
│ User Sees        │ Only their DBs     │ Depends on config  │ Depends on     │
│ Database List    │ (or empty if none) │ (may be empty)     │ cluster config │
│                  │                    │                    │                │
├──────────────────┼────────────────────┼────────────────────┼────────────────┤
│                  │                    │                    │                │
│ Create Database  │ Always allowed     │ Configurable       │ Configurable   │
│ Permission       │ (user owns it)     │ (per-user setting) │ (per-user)     │
│                  │                    │                    │                │
├──────────────────┼────────────────────┼────────────────────┼────────────────┤
│                  │                    │                    │                │
│ Database Owner   │ Creator            │ Creator (but       │ Creator (with  │
│                  │ (full control)     │ server may limit)  │ cluster-wide   │
│                  │                    │                    │ visibility)    │
├──────────────────┼────────────────────┼────────────────────┼────────────────┤
│                  │                    │                    │                │
│ User Management  │ Per-database       │ Centralized        │ Cluster-wide   │
│                  │                    │                    │                │
├──────────────────┼────────────────────┼────────────────────┼────────────────┤
│                  │                    │                    │                │
│ Typical Use      │ Development        │ Enterprise         │ High-avail     │
│ Case             │ Single-user        │ Multi-user         │ Multi-server   │
│                  │                    │                    │                │
└──────────────────┴────────────────────┴────────────────────┴────────────────┘
```

---

## 3. Standalone Mode (Default)

### 3.1 New Server, First Connection

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     NEW SERVER - FIRST USER CONNECTS                        │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│   Server State                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  registry.db: EMPTY (no databases registered)                       │   │
│   │  Open databases: NONE                                               │   │
│   │  Security model: STANDALONE                                         │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                              │                                              │
│                              │ Connection from user "alice"               │
│                              ▼                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  1. Authenticate (OS auth, password file, or none for first)        │   │
│   │                                                                     │   │
│   │  2. List Databases Query:                                           │   │
│   │     SELECT database_name FROM registered_databases                  │   │
│   │     WHERE owner_user_id = 'alice-uuid'                              │   │
│   │                                                                     │   │
│   │     RESULT: EMPTY SET                                               │   │
│   │                                                                     │   │
│   │  3. User sees: "No databases available"                             │   │
│   │     (Or empty list, depending on client)                            │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 User Creates First Database

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     USER CREATES DATABASE                                   │
└─────────────────────────────────────────────────────────────────────────────┘

User "alice" executes: CREATE DATABASE 'sales'

Server:
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│  1. Create database file: /var/lib/scratchbird/databases/sales.db          │
│                                                                             │
│  2. Initialize database schema:                                             │
│     ┌─────────────────────────────────────────────────────────────────┐    │
│     │  System tables:                                                  │    │
│     │  ├─ sb_users (alice is first entry, auto-admin)                 │    │
│     │  ├─ sb_roles                                                     │    │
│     │  ├─ sb_permissions                                               │    │
│     │  └─ sb_audit_log                                                 │    │
│     └─────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│  3. Register in registry:                                                   │
│     INSERT INTO registered_databases (                                      │
│         database_name, database_path, owner_user_id, security_model        │
│     ) VALUES (                                                              │
│         'sales',                                                            │
│         '/var/lib/scratchbird/databases/sales.db',                          │
│         'alice-uuid',                                                       │
│         'local'    -- Database uses its own security                        │
│     );                                                                      │
│                                                                             │
│  4. Grant alice full permissions:                                           │
│     ┌─────────────────────────────────────────────────────────────────┐    │
│     │  In sales.db:                                                    │    │
│     │  INSERT INTO sb_users (user_id, user_name, is_admin)           │    │
│     │  VALUES ('alice-uuid', 'alice', true);                          │    │
│     │                                                                  │    │
│     │  alice is now DATABASE ADMINISTRATOR                             │    │
│     └─────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│  5. Return success to user                                                  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

Result:
- Database "sales" exists
- alice is the owner
- alice has FULL CONTROL over this database
- alice can create users, roles, set permissions as she wishes
- Server has no control over sales.db security (it's local to the DB)
```

### 3.3 Database Administrator Powers

```sql
-- As database owner "alice" connected to "sales" database:

-- Create users for this database only
CREATE USER 'bob' PASSWORD 'secret';
CREATE USER 'carol' PASSWORD 'hidden';

-- Create roles
CREATE ROLE 'sales_team';
CREATE ROLE 'managers';

-- Grant permissions
GRANT CONNECT ON DATABASE TO bob, carol;
GRANT SELECT, INSERT, UPDATE ON orders TO sales_team;
GRANT ALL ON orders TO managers;
GRANT CREATE TABLE TO managers;

-- Grant roles to users
GRANT sales_team TO bob;
GRANT managers TO carol;

-- View audit log (database-specific)
SELECT * FROM sb_audit_log WHERE database_name = 'sales';

-- Backup (as owner)
BACKUP DATABASE '/backups/sales_2024.bak';
```

**Important:** These users/roles exist ONLY in the `sales` database. They are not server-wide.

---

## 4. Shared Security Mode

### 4.1 Configuration

```ini
; scratchbird.conf
[security]
; Mode: standalone, shared, or cluster
mode = shared

; Path to shared security database
security_database = /var/lib/scratchbird/security.db

; Access control
[security.access]
; Can users see database list? (default: false)
show_database_list = false

; Can users create databases? (default: false)
can_create_database = false

; Who can create databases?
database_creators = admin, dba_team
```

### 4.2 User Connection Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                 SHARED SECURITY - USER CONNECTION                           │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│   Server: security.db exists and is configured                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  security.db tables:                                                │   │
│   │  ├─ sb_users (server-wide users)                                    │   │
│   │  ├─ sb_groups                                                       │   │
│   │  ├─ server_permissions (who can do what)                            │   │
│   │  └─ database_access (which users can access which DBs)              │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                              │                                              │
│                              │ User "bob" connects                          │
│                              ▼                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  1. Authenticate against security.db                                │   │
│   │     SELECT user_id FROM sb_users WHERE user_name = 'bob'            │   │
│   │     AND password_hash = hash('provided_password')                   │   │
│   │                                                                     │   │
│   │  2. Check server-level permissions:                                 │   │
│   │     SELECT can_create_database, can_see_all_databases               │   │
│   │     FROM server_permissions WHERE user_id = 'bob-uuid'              │   │
│   │                                                                     │   │
│   │     Result: can_create_database = false                             │   │
│   │             can_see_all_databases = false                           │   │
│   │                                                                     │   │
│   │  3. List databases (depends on config):                             │   │
│   │     IF show_database_list = false:                                  │   │
│   │        Show: "No databases available" or empty list                 │   │
│   │     ELSE:                                                           │   │
│   │        SELECT d.database_name FROM registered_databases d           │   │
│   │        JOIN database_access a ON d.database_id = a.database_id      │   │
│   │        WHERE a.user_id = 'bob-uuid' AND a.can_connect = true        │   │
│   │                                                                     │   │
│   │  4. bob wants to create database:                                   │   │
│   │     CHECK: Is bob in 'database_creators' group?                     │   │
│   │     RESULT: NO → ERROR: "Insufficient privileges"                   │   │
│   │                                                                     │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.3 Database Access with Shared Security

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              CONNECTING TO DATABASE (Shared Security)                       │
└─────────────────────────────────────────────────────────────────────────────┘

User "bob" tries to connect to database "production"

1. Server checks security.db:
   SELECT can_connect FROM database_access
   WHERE database_id = 'production-uuid' AND user_id = 'bob-uuid'

   Results:
   a) No row found → "Access denied to database 'production'"
   
   b) Row exists, can_connect = false → "Access denied"
   
   c) Row exists, can_connect = true → Proceed to database-level auth

2. If allowed, connect to production.db:
   - Check production.sb_users for bob
   - If exists: use database-level permissions
   - If not exists: "User not authorized in this database"

3. Bob is now connected with permissions from production.db
```

---

## 5. Cluster Mode

### 5.1 Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     CLUSTER SECURITY MODE                                   │
└─────────────────────────────────────────────────────────────────────────────┘

┌──────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│   Server A       │◄───►│   Server B       │◄───►│   Server C       │
│   (Primary)      │     │   (Secondary)    │     │   (Secondary)    │
└────────┬─────────┘     └────────┬─────────┘     └────────┬─────────┘
         │                        │                        │
         │    Replicated          │                        │
         └────────────────────────┴────────────────────────┘
                              │
                              ▼
                    ┌──────────────────┐
                    │  Distributed     │
                    │  Security DB     │
                    │  (replicated     │
                    │   across all)    │
                    └──────────────────┘

Behavior:
- Same as Shared Security Mode
- Security database is synchronized across cluster
- User authentication works on any server
- Database list shows cluster-wide accessible databases
- Database creation permission is cluster-wide setting
```

### 5.2 Cluster Configuration

```ini
[security]
mode = cluster

[cluster]
node_id = server-a
cluster_name = production_cluster
security_replication = synchronous

[security.access]
show_database_list = true
can_create_database = false
database_creators = cluster_admin, dba_team
```

---

## 6. Parser Isolation and Connection Termination

### 6.1 Parser Per-Connection Model

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     PARSER ISOLATION MODEL                                  │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                               Server                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │  Core Engine (shared)                                               │    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐            │    │
│  │  │  Buffer  │  │  Lock    │  │  MGA     │  │  Storage │            │    │
│  │  │  Pool    │  │  Manager │  │  Core    │  │  Manager │            │    │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘            │    │
│  └────────────────────────────────┬────────────────────────────────────┘    │
│                                   │                                         │
│         ┌─────────────────────────┼─────────────────────────┐               │
│         │                         │                         │               │
│         ▼                         ▼                         ▼               │
│  ┌──────────────┐          ┌──────────────┐          ┌──────────────┐      │
│  │  Connection 1│          │  Connection 2│          │  Connection 3│      │
│  │  (User: bob) │          │  (User: alice│          │  (User: evil)│      │
│  │              │          │              │          │              │      │
│  │┌────────────┐│          │┌────────────┐│          │┌────────────┐│      │
│  ││  Parser    ││          ││  Parser    ││          ││  Parser    ││      │
│  ││  Process   ││          ││  Process   ││          ││  Process   ││      │
│  ││  (PID:     ││          ││  (PID:     ││          ││  (PID:     ││      │
│  ││   12345)   ││          ││   12346)   ││          ││   12347)   ││      │
│  │└────────────┘│          │└────────────┘│          │└────────────┘│      │
│  └──────────────┘          └──────────────┘          └──────────────┘      │
│                                                                             │
│  Key Benefits:                                                              │
│  - Each parser is SEPARATE PROCESS                                          │
│  - Memory isolation between connections                                     │
│  - Crash in one parser doesn't affect others                                │
│  - Easy to terminate misbehaving connections                                │
│  - Resource limits enforced by OS                                           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 6.2 Connection Termination

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     KILLING A CONNECTION                                    │
└─────────────────────────────────────────────────────────────────────────────┘

Scenario: User "evil" is running infinite loop query, needs to be stopped

┌─────────────────────────────────────────────────────────────────────────────┐
│  Administrator Action:                                                      │
│  SELECT * FROM sb_connections WHERE user_name = 'evil';                    │
│  -- Shows: connection_id, pid, database, query_start, current_query        │
│                                                                             │
│  KILL CONNECTION 'conn-uuid';                                              │
│  -- or                                                                     │
│  KILL QUERY 'query-uuid';                                                  │
│                                                                             │
│  Server Action:                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │  1. Find parser process:                                            │   │
│  │     pid = getParserPid(connection_id)                               │   │
│  │                                                                     │   │
│  │  2. Signal termination:                                             │   │
│  │     kill(pid, SIGTERM)  -- Graceful shutdown                        │   │
│  │     OR                                                              │   │
│  │     kill(pid, SIGKILL)  -- Immediate (if SIGTERM fails)             │   │
│  │                                                                     │   │
│  │  3. Wait for process exit (with timeout)                            │   │
│  │                                                                     │   │
│  │  4. Clean up:                                                       │   │
│  │     - Release any locks held by this connection                     │   │
│  │     - Rollback any active transaction                               │   │
│  │     - Free parser slot in pool                                      │   │
│  │     - Close socket                                                  │   │
│  │                                                                     │   │
│  │  5. Parser process exits, OS reclaims all memory                    │   │
│  │                                                                     │   │
│  │  6. Any problems/inconsistencies in parser are GONE                 │   │
│  │     (memory leaks, corruption, etc. isolated to dead process)       │   │
│  │                                                                     │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  Result: Clean termination, no server impact                                │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 6.3 Automatic Cleanup on Disconnect

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     NORMAL DISCONNECT CLEANUP                               │
└─────────────────────────────────────────────────────────────────────────────┘

User disconnects (or network drops, or client crashes):

┌─────────────────────────────────────────────────────────────────────────────┐
│  Listener detects socket close/error                                        │
│                              │                                              │
│                              ▼                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │  1. Mark connection as closing in ConnectionManager                 │   │
│  │                                                                     │   │
│  │  2. Signal parser process to exit:                                  │   │
│  │     write(pipe, "SHUTDOWN")                                         │   │
│  │                                                                     │   │
│  │  3. Parser receives signal:                                         │   │
│  │     - Rollback any uncommitted transaction                          │   │
│  │     - Release any resources                                         │   │
│  │     - Exit cleanly                                                  │   │
│  │                                                                     │   │
│  │  4. Wait for parser process to exit (non-blocking)                  │   │
│  │                                                                     │   │
│  │  5. Reclaim parser slot:                                            │   │
│  │     parser_pool_.releaseWorker(parser_id)                           │   │
│  │                                                                     │   │
│  │  6. Update statistics:                                              │   │
│  │     registry_.updateConnectStats(db_id, false)                      │   │
│  │                                                                     │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  Even if parser crashes during cleanup:                                     │
│  - Server detects process death (SIGCHLD)                                   │
│  - Cleans up resources anyway                                               │
│  - No memory leaks (OS reclaims process memory)                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Security Configuration Examples

### 7.1 Standalone (Development)

```ini
; Minimal configuration, maximum flexibility
[security]
mode = standalone

; No other security settings needed
; Each user creates and owns their databases
```

### 7.2 Shared Security (Enterprise)

```ini
; Centralized control
[security]
mode = shared
security_database = /secure/security.db

; Strict access control
[security.access]
show_database_list = false
can_create_database = false
database_creators = dba_team

; Pre-defined users can connect to specific databases
[security.databases]
allow_adhoc_connections = false
```

### 7.3 Cluster (High Availability)

```ini
; Distributed security
[security]
mode = cluster

[cluster]
node_id = node1
cluster_name = prod_cluster
join_token = ${CLUSTER_JOIN_TOKEN}

; Cluster-wide permissions
[security.access]
show_database_list = true
can_create_database = true
database_creators = %admin_group
```

---

## 8. Summary Table

| Scenario | Standalone | Shared | Cluster |
|----------|------------|--------|---------|
| First user sees | Empty list | Depends on config | Depends on config |
| Create DB | Yes (owns it) | Maybe (configured) | Maybe (configured) |
| User management | Per-database | Central | Distributed |
| DB visibility | Owner only | Configurable | Configurable |
| Security admin | DB owner | Server admin | Cluster admin |

---

## 9. Related Specifications

- [DATABASE_REGISTRY_SPECIFICATION_CORRECTED.md](DATABASE_REGISTRY_SPECIFICATION_CORRECTED.md) - Registry as ScratchBird database
- [SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md](SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md) - Server startup with security init
- [SCRATCHBIRD_EMBEDDED_MODE_SPECIFICATION.md](SCRATCHBIRD_EMBEDDED_MODE_SPECIFICATION.md) - Embedded mode security