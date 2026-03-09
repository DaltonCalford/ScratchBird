# System Functions

[Categories README](./README.md)

## Synopsis

Functions for accessing database system information and session state.

## Session Information

| Function | Description | Example |
|----------|-------------|---------|
| `current_user` | Current user name | `current_user` → `'admin'` |
| `session_user` | Session user name | `session_user` |
| `current_database()` | Current database | `current_database()` → `'mydb'` |
| `current_schema()` | Current schema | `current_schema()` → `'public'` |
| `current_schemas(boolean)` | Search path schemas | `current_schemas(true)` |
| `pg_backend_pid()` | Process ID | Process ID of backend |
| `version()` | Server version | Full version string |
| `inet_client_addr()` | Client IP address | Remote client address |
| `inet_client_port()` | Client port | Remote client port |
| `inet_server_addr()` | Server IP address | Local server address |
| `inet_server_port()` | Server port | Local server port |

## Configuration

| Function | Description | Example |
|----------|-------------|---------|
| `current_setting(name)` | Get setting value | `current_setting('max_connections')` |
| `set_config(name, value, is_local)` | Set configuration | `set_config('app.debug', 'true', true)` |

## Object Information

| Function | Description | Example |
|----------|-------------|---------|
| `pg_typeof(expr)` | Type of expression | `pg_typeof(123)` → `'integer'` |
| `collation_for(expr)` | Collation | `collation_for('text')` |
| `to_regclass(text)` | OID of table | `to_regclass('users')` |
| `to_regtype(text)` | OID of type | `to_regtype('integer')` |

## Environment

| Function | Description |
|----------|-------------|
| `clock_timestamp()` | Current timestamp (changes during query) |
| `statement_timestamp()` | Statement start time |
| `transaction_timestamp()` | Transaction start time |
| `now()` | Current timestamp |

## Examples

```sql
-- Current user info
SELECT current_user, current_database(), current_schema();

-- Server version
SELECT version();

-- Get configuration
SELECT current_setting('max_connections');

-- Set session variable
SELECT set_config('app.tenant_id', '123', false);

-- Get client connection info
SELECT inet_client_addr(), inet_client_port();

-- Check type
SELECT pg_typeof(123), pg_typeof('hello'), pg_typeof(now());

-- All settings
SELECT name, setting FROM pg_settings WHERE name LIKE 'max_%';

-- Active connections
SELECT count(*) FROM pg_stat_activity;

-- Database size
SELECT pg_size_pretty(pg_database_size(current_database()));

-- Table size
SELECT pg_size_pretty(pg_total_relation_size('users'));
```

## System Views

```sql
-- Current activity
SELECT * FROM pg_stat_activity;

-- Database statistics
SELECT * FROM pg_stat_database;

-- Table statistics
SELECT * FROM pg_stat_user_tables;

-- Index statistics
SELECT * FROM pg_stat_user_indexes;

-- Locks
SELECT * FROM pg_locks;

-- Settings
SELECT * FROM pg_settings;
```
