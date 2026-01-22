# ScratchBird Service Mode and systemd Integration Specification

**Version:** 1.0
**Date:** December 10, 2025
**Status:** Design Complete - Ready for Implementation
**Author:** dcalford with Claude Code

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [Server Binary (sb_server)](#3-server-binary)
4. [Configuration File](#4-configuration-file)
5. [Database Modes](#5-database-modes)
6. [systemd Integration](#6-systemd-integration)
7. [Signal Handling](#7-signal-handling)
8. [Lifecycle Management](#8-lifecycle-management)
9. [Logging](#9-logging)
10. [Monitoring and Health Checks](#10-monitoring-and-health-checks)
11. [Security Hardening](#11-security-hardening)
12. [Cluster Service Mode](#12-cluster-service-mode)
13. [Package Installation](#13-package-installation)
14. [Troubleshooting](#14-troubleshooting)
15. [Configuration Reference](#15-configuration-reference)

---

## 1. Overview

ScratchBird operates in two primary modes:

1. **Embedded Mode** - Library linked directly into application (like SQLite)
2. **Service Mode** - Standalone server process managed by systemd

This specification covers **Service Mode** - running ScratchBird as a persistent daemon that survives client disconnections and system reboots.

### Design Goals

1. **Production Ready** - Suitable for enterprise deployment
2. **systemd Native** - Full integration with modern Linux init system
3. **Zero Downtime** - Configuration reload without restart
4. **Observable** - Comprehensive logging and metrics
5. **Secure** - Defense in depth, principle of least privilege

### Key Features

| Feature | Description |
|---------|-------------|
| Hot Configuration Reload | Change settings without restart (SIGHUP) |
| Graceful Shutdown | Complete active queries before exit |
| Multi-Database Support | One server manages multiple databases |
| Connection Persistence | Server stays running when clients disconnect |
| Automatic Recovery | Restart on failure with backoff |
| Resource Limits | Memory, connections, file descriptors |
| Security Isolation | Sandboxing, capability dropping |

---

## 2. Architecture

### 2.1 Process Model

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     ScratchBird Service Architecture                     │
└─────────────────────────────────────────────────────────────────────────┘

                              systemd
                                 │
                                 │ ExecStart, ExecReload, ExecStop
                                 ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          sb_server (main process)                        │
│                                                                          │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │  Main Thread                                                        │ │
│  │  - Signal handling                                                  │ │
│  │  - Configuration management                                         │ │
│  │  - Health monitoring                                                │ │
│  │  - systemd notification (sd_notify)                                 │ │
│  └────────────────────────────────────────────────────────────────────┘ │
│                                                                          │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │  Listener Threads (per protocol)                                    │ │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌────────────┐ │ │
│  │  │ Native :3092 │ │ PG :5432     │ │ MySQL :3306  │ │ TDS*:1433 │ │ │
│  │  └──────────────┘ └──────────────┘ └──────────────┘ └────────────┘ │ │
│  │  ┌──────────────┐ ┌──────────────┐                                 │ │
│  │  │ FB :3050     │ │ Unix Socket  │                                 │ │
│  │  └──────────────┘ └──────────────┘                                 │ │
│  └────────────────────────────────────────────────────────────────────┘ │
│                                                                          │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │  Worker Thread Pool                                                 │ │
│  │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐           │ │
│  │  │Worker 1│ │Worker 2│ │Worker 3│ │Worker 4│ │  ...   │           │ │
│  │  └────────┘ └────────┘ └────────┘ └────────┘ └────────┘           │ │
│  └────────────────────────────────────────────────────────────────────┘ │
│                                                                          │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │  Background Services                                                │ │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐               │ │
│  │  │ Checkpoint   │ │ Garbage      │ │ Stats        │               │ │
│  │  │ Writer       │ │ Collector    │ │ Collector    │               │ │
│  │  └──────────────┘ └──────────────┘ └──────────────┘               │ │
│  └────────────────────────────────────────────────────────────────────┘ │
│                                                                          │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │  Database Instances                                                 │ │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐               │ │
│  │  │ Database A   │ │ Database B   │ │ Database C   │               │ │
│  │  │ /data/db_a   │ │ /data/db_b   │ │ /data/db_c   │               │ │
│  │  └──────────────┘ └──────────────┘ └──────────────┘               │ │
│  └────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
```

**Note:** TDS/MSSQL listener is reserved for post-gold; current versions expose Native/PG/MySQL/Firebird only.

### 2.2 Thread Model

| Thread Type | Count | Purpose |
|-------------|-------|---------|
| Main | 1 | Signal handling, coordination |
| Listener | 1 per protocol | Accept connections |
| Worker | Configurable (default: CPU cores × 2) | Query execution |
| Checkpoint | 1 per database | MGA checkpointing (no write-after log (WAL, optional post-gold)) |
| Garbage Collector | 1 per database | MGA version cleanup |
| Stats Collector | 1 | Aggregate statistics |
| Watchdog | 1 | Health monitoring |

---

## 3. Server Binary

### 3.1 Command-Line Interface

```
sb_server - ScratchBird Database Server

USAGE:
    sb_server [OPTIONS]

OPTIONS:
    -c, --config <FILE>         Configuration file path
                                Default: /etc/scratchbird/sb_server.conf

    -D, --data-dir <DIR>        Data directory (multi-database mode)
                                Default: /var/lib/scratchbird

    -d, --database <FILE>       Single database file path (single-database mode)
                                Mutually exclusive with --data-dir

    --create                    Create database(s) if they don't exist

    -h, --host <ADDR>           Bind address for all protocols
                                Default: 0.0.0.0

    -p, --port <PORT>           ScratchBird native protocol port
                                Default: 3092

    --pg-port <PORT>            PostgreSQL protocol port
                                Default: 5432 (0 to disable)

    --mysql-port <PORT>         MySQL protocol port
                                Default: 3306 (0 to disable)

    --tds-port <PORT>           TDS/MSSQL protocol port (post-gold, reserved)
                                Default: 1433 (0 to disable)

    --fb-port <PORT>            Firebird protocol port
                                Default: 3050 (0 to disable)

    -k, --unix-socket <PATH>    Unix domain socket path
                                Default: /var/run/scratchbird/sb.sock

    -N, --max-connections <N>   Maximum concurrent connections
                                Default: 100

    -B, --shared-buffers <SIZE> Shared buffer pool size
                                Default: 128MB

    -F, --foreground            Run in foreground (don't daemonize)

    -s, --single-user           Single-user mode (maintenance)

    -V, --version               Print version and exit

    -?, --help                  Show this help message

ENVIRONMENT VARIABLES:
    SCRATCHBIRD_CONFIG          Configuration file path
    SCRATCHBIRD_DATA_DIR        Data directory
    SCRATCHBIRD_LOG_LEVEL       Log level (debug, info, warning, error)

EXAMPLES:
    # Start with default configuration
    sb_server

    # Start in foreground with custom config
    sb_server -F -c /path/to/custom.conf

    # Single database mode
    sb_server -d /path/to/mydb.sbdb --create

    # Multi-database mode with custom ports
    sb_server -D /var/lib/scratchbird --pg-port 5434 --mysql-port 0

    # Maintenance mode (single user, no network)
    sb_server -d /path/to/mydb.sbdb -s

EXIT CODES:
    0   Success
    1   General error
    2   Configuration error
    3   Permission denied
    4   Database error
    5   Network error
    6   Already running (PID file exists)
```

### 3.2 Version Information

```bash
$ sb_server --version
sb_server (ScratchBird) 1.0.0
Protocol versions:
  - ScratchBird Native: 1.0
  - PostgreSQL: 3.0
  - MySQL: 10
  - TDS: 7.4 (post-gold)
  - Firebird: 13
Build: Release
Compiler: GCC 13.2.0
OpenSSL: 3.1.4
zstd: 1.5.5
```

---

## 4. Configuration File

### 4.1 File Location

Configuration files are searched in order:

1. Command-line `--config` option
2. `$SCRATCHBIRD_CONFIG` environment variable
3. `./sb_server.conf` (current directory)
4. `~/.config/scratchbird/sb_server.conf`
5. `/etc/scratchbird/sb_server.conf`

### 4.2 File Format

INI-style format with sections:

```ini
# ScratchBird Server Configuration
# /etc/scratchbird/sb_server.conf
#
# Lines starting with # or ; are comments
# Values can be quoted: key = "value with spaces"
# Environment variables: key = ${ENV_VAR}
# Include other files: @include /path/to/other.conf

#==============================================================================
# SERVER SECTION
#==============================================================================
[server]
# Operation mode: single-database | multi-database
mode = multi-database

# Data directory for multi-database mode
# Each .sbdb file in this directory is a separate database
data_dir = /var/lib/scratchbird

# Single database path (for mode = single-database)
# database = /var/lib/scratchbird/main.sbdb

# Create databases if they don't exist
auto_create = false

# PID file location
pid_file = /var/run/scratchbird/sb_server.pid

# Maximum time to wait for graceful shutdown (seconds)
shutdown_timeout = 30

# Worker thread count (0 = auto-detect based on CPU cores)
worker_threads = 0

# Maximum concurrent connections across all protocols
max_connections = 100

# Per-user connection limit (0 = unlimited)
max_connections_per_user = 0

# Per-database connection limit (0 = unlimited)
max_connections_per_database = 0

# Idle connection timeout (seconds, 0 = never)
idle_timeout = 3600

# Statement timeout (milliseconds, 0 = unlimited)
statement_timeout = 0

# Lock wait timeout (milliseconds)
lock_timeout = 30000

#==============================================================================
# NETWORK SECTION
#==============================================================================
[network]
# Bind address (0.0.0.0 for all interfaces, 127.0.0.1 for localhost only)
bind_address = 0.0.0.0

# ScratchBird native protocol port (0 to disable)
native_port = 3092

# PostgreSQL protocol port (0 to disable)
pg_port = 5432

# MySQL protocol port (0 to disable)
mysql_port = 3306

# TDS/MSSQL protocol port (reserved; post-gold, 0 to disable)
tds_port = 1433

# Firebird protocol port (0 to disable)
fb_port = 3050

# Unix domain socket path (empty to disable)
unix_socket = /var/run/scratchbird/sb.sock

# Unix socket permissions (octal)
unix_socket_permissions = 0770

# Unix socket group
unix_socket_group = scratchbird

# TCP keepalive interval (seconds)
tcp_keepalive = 60

# TCP user timeout (milliseconds)
tcp_user_timeout = 30000

# Listen backlog
listen_backlog = 128

# Enable TCP_NODELAY (disable Nagle's algorithm)
tcp_nodelay = true

#==============================================================================
# SSL/TLS SECTION
#==============================================================================
[ssl]
# Enable SSL/TLS (required for production)
enabled = true

# Server certificate file (PEM format)
cert_file = /etc/scratchbird/ssl/server.crt

# Server private key file (PEM format)
key_file = /etc/scratchbird/ssl/server.key

# Certificate authority file for client verification
ca_file = /etc/scratchbird/ssl/ca.crt

# Minimum TLS version: TLSv1.2 | TLSv1.3
min_protocol = TLSv1.2

# Preferred TLS version (for negotiation)
preferred_protocol = TLSv1.3

# Cipher suites (OpenSSL format)
# For TLS 1.3: TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256
# For TLS 1.2: ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256
ciphers = TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:ECDHE-RSA-AES256-GCM-SHA384

# Require client certificate
require_client_cert = false

# Certificate revocation list
crl_file =

# Verify client certificate depth
verify_depth = 3

# Enable OCSP stapling
ocsp_stapling = true

# DH parameters file (for DHE ciphers)
dh_params_file = /etc/scratchbird/ssl/dh4096.pem

# ECDH curve: prime256v1 | secp384r1 | secp521r1 | X25519
ecdh_curve = X25519

#==============================================================================
# CONNECTION POOL SECTION
#==============================================================================
[pool]
# Minimum connections to maintain per database
min_connections = 5

# Maximum connections per database
max_connections = 50

# Idle connection timeout (seconds)
idle_timeout = 300

# Maximum connection lifetime (seconds)
max_lifetime = 3600

# Connection validation interval (seconds)
validation_interval = 60

# Validation query (empty = use driver's isValid)
validation_query = SELECT 1

# Time to wait for connection from pool (milliseconds)
acquire_timeout = 30000

# Enable prepared statement caching
statement_cache = true

# Maximum cached statements per connection
statement_cache_size = 1000

# Enable query result caching
result_cache = true

# Result cache maximum size (bytes)
result_cache_size = 67108864  # 64MB

# Result cache TTL (seconds)
result_cache_ttl = 300

#==============================================================================
# MEMORY SECTION
#==============================================================================
[memory]
# Buffer pool size (canonical)
# Format: 128MB, 1GB, etc.
buffer_pool_size = 128MB

# Buffer pool page size (bytes)
buffer_pool_page_size = 8192

# Background writer controls
buffer_pool_bgwriter_enabled = true
buffer_pool_bgwriter_max_pages = 1000
buffer_pool_dirty_ratio_low = 0.10
buffer_pool_dirty_ratio_high = 0.30
buffer_pool_dirty_ratio_checkpoint = 0.40

# Work memory per operation (sorts, hash joins)
work_mem = 4MB

# Maintenance work memory (sweep/GC, CREATE INDEX)
maintenance_work_mem = 64MB

# Maximum memory for result sets
max_result_size = 100MB

# Memory for temporary files (0 = unlimited)
temp_file_limit = 0

# Huge pages: off | try | on
huge_pages = try

#==============================================================================
# STORAGE SECTION
#==============================================================================
[storage]
# Optional write-after log (post-gold). MGA does not use write-after log (WAL) for recovery; these are no-ops until implemented.
# Write-after log (WAL, optional post-gold) directory (default: same as data directory)
wal_dir =

# Checkpoint interval (seconds)
checkpoint_interval = 300

# Checkpoint completion target (0.0 - 1.0)
checkpoint_completion_target = 0.9

# Maximum write-after log (WAL, optional post-gold) size before checkpoint (bytes)
max_wal_size = 1073741824  # 1GB

# Minimum write-after log (WAL, optional post-gold) size to retain
min_wal_size = 80MB

# fsync after each commit: on | off (DANGEROUS if off)
fsync = on

# Synchronous commit: on | off | local | remote_write | remote_apply
synchronous_commit = on

# Full page writes after checkpoint
full_page_writes = on

# Write-after log (WAL, optional post-gold) compression
wal_compression = zstd

# Data file sync method: fsync | fdatasync | open_sync | open_datasync
wal_sync_method = fdatasync

# COPY file access policy (server-side COPY FROM/TO)
# Comma-separated allowlist of directories. Empty = no server-side file access.
copy_allowed_paths =

# Allow COPY ... FROM/TO absolute paths (default false)
copy_allow_absolute_paths = false

# Allow COPY ... FROM/TO relative paths (resolved under data_dir)
copy_allow_relative_paths = true

# Require superuser for server-side COPY file access
copy_require_superuser = true

#==============================================================================
# AUTHENTICATION SECTION
#==============================================================================
[authentication]
# Authentication methods (comma-separated, tried in order)
# Options: trust, password, md5, scram-sha-256, certificate, ldap, gssapi, sspi
methods = scram-sha-256,certificate

# Password hashing algorithm: bcrypt | argon2id
password_hash = argon2id

# Argon2 parameters
argon2_memory = 65536      # KB
argon2_iterations = 3
argon2_parallelism = 4

# bcrypt cost factor (4-31)
bcrypt_cost = 12

# Minimum password length
password_min_length = 12

# Require password complexity (uppercase, lowercase, digit, special)
password_require_complexity = true

# Maximum failed login attempts before lockout
max_failed_attempts = 5

# Account lockout duration (seconds)
lockout_duration = 300

# Session timeout (seconds, 0 = never)
session_timeout = 0

# Allow superuser remote login
allow_superuser_remote = false

#==============================================================================
# LDAP SECTION
#==============================================================================
[ldap]
# Enable LDAP authentication
enabled = false

# LDAP server URI (ldap:// or ldaps://)
server = ldaps://ldap.example.com:636

# Bind DN for LDAP queries
bind_dn = cn=scratchbird,ou=services,dc=example,dc=com

# Bind password (or use bind_password_file)
bind_password =

# File containing bind password
bind_password_file = /etc/scratchbird/ldap.passwd

# User search base
user_base = ou=users,dc=example,dc=com

# User search filter ({username} is replaced)
user_filter = (uid={username})

# Group search base
group_base = ou=groups,dc=example,dc=com

# Group membership attribute
group_member_attr = memberUid

# TLS certificate verification: none | optional | required
tls_verify = required

# LDAP timeout (seconds)
timeout = 10

# Cache LDAP results (seconds)
cache_ttl = 300

#==============================================================================
# KERBEROS SECTION
#==============================================================================
[kerberos]
# Enable Kerberos/GSSAPI authentication
enabled = false

# Kerberos keytab file
keytab = /etc/scratchbird/krb5.keytab

# Service principal name
service_name = scratchbird

# Kerberos realm
realm = EXAMPLE.COM

# Allow credential delegation
allow_delegation = false

#==============================================================================
# AUDIT SECTION
#==============================================================================
[audit]
# Enable audit logging
enabled = true

# Audit log file
log_file = /var/log/scratchbird/audit.log

# Audit log format: text | json | syslog
log_format = json

# Events to audit (comma-separated)
# Options: connect, disconnect, authenticate, authorize, query, ddl, dml, error
events = connect,disconnect,authenticate,authorize,ddl,error

# Include query text in audit log
log_query_text = false

# Truncate query text at (characters, 0 = no truncation)
query_text_max_length = 1000

# Include parameter values (security risk!)
log_parameters = false

# Hash chain for tamper detection
hash_chain = true

# Rotate audit log: daily | weekly | size
rotate = daily

# Keep rotated logs for (days)
rotate_keep_days = 90

# Compress rotated logs
rotate_compress = true

#==============================================================================
# LOGGING SECTION
#==============================================================================
[logging]
# Log level: debug | info | notice | warning | error
level = info

# Log destination: stderr | syslog | file
destination = file

# Log file path (for destination = file)
file = /var/log/scratchbird/sb_server.log

# Log file format: text | json
format = text

# Include timestamps
timestamps = true

# Timestamp format (strftime)
timestamp_format = %Y-%m-%d %H:%M:%S.%f %z

# Include PID
include_pid = true

# Include thread ID
include_thread_id = true

# Log rotation: daily | weekly | size
rotate = daily

# Maximum log file size (for rotate = size)
rotate_size = 100MB

# Keep rotated logs for (days)
rotate_keep_days = 30

# Compress rotated logs
rotate_compress = true

# Log slow queries (milliseconds, 0 = disabled)
log_slow_queries = 1000

# Log query plans for slow queries
log_slow_query_plans = true

# Log all DDL statements
log_ddl = true

# Log all connections
log_connections = true

# Log all disconnections
log_disconnections = true

# Log lock waits longer than (milliseconds)
log_lock_waits = 1000

# Log temporary file usage larger than (bytes)
log_temp_files = 10485760  # 10MB

# Log checkpoints
log_checkpoints = true

#==============================================================================
# STATISTICS SECTION
#==============================================================================
[statistics]
# Enable statistics collection
enabled = true

# Statistics collection interval (seconds)
interval = 60

# Track per-query statistics
track_queries = true

# Track I/O timing
track_io_timing = true

# Track function calls
track_functions = all  # none | pl | all

# Statistics retention (hours)
retention = 168  # 7 days

# Export statistics: none | prometheus | statsd | both
export = prometheus

# Prometheus metrics endpoint port
prometheus_port = 9090

# StatsD server
statsd_host = localhost
statsd_port = 8125
statsd_prefix = scratchbird

#==============================================================================
# MAINTENANCE SECTION
#==============================================================================
[maintenance]
# TODO: Rename autovacuum_* to gc_* after current work completes; keep autovacuum_* as
#       compatibility aliases. No code changes in this phase.
# Auto sweep/GC enabled (PostgreSQL autovacuum alias)
autovacuum = true

# Auto sweep/GC check interval (seconds)
autovacuum_interval = 60

# GC threshold (dead versions)
autovacuum_threshold = 50

# GC scale factor
autovacuum_scale_factor = 0.2

# Analyze threshold
autoanalyze_threshold = 50

# Analyze scale factor
autoanalyze_scale_factor = 0.1

# Maximum auto sweep/GC workers
autovacuum_workers = 3

# Auto sweep/GC memory limit
autovacuum_work_mem = 64MB

# MGA garbage collection interval (seconds)
gc_interval = 300

# Oldest transaction to retain (seconds)
gc_retain_time = 3600

#==============================================================================
# REPLICATION SECTION (Beta feature)
#==============================================================================
[replication]
# Enable replication
enabled = false

# Role: primary | standby
role = primary

# Replication slots
max_replication_slots = 10

# Write-after log (WAL, optional post-gold) sender processes
max_wal_senders = 10

# Standby connection string (for standby role)
primary_conninfo =

# Replication delay alert threshold (seconds)
alert_delay_threshold = 300

#==============================================================================
# CLUSTER SECTION
#==============================================================================
[cluster]
# Enable cluster mode
enabled = false

# Cluster name
name =

# Cluster CA certificate
ca_cert = /etc/scratchbird/cluster/ca.crt

# This node's certificate
node_cert = /etc/scratchbird/cluster/node.crt

# This node's private key
node_key = /etc/scratchbird/cluster/node.key

# Node ID (UUID, auto-generated if empty)
node_id =

# Cluster discovery method: static | dns | etcd | consul
discovery = static

# Static peer list (for discovery = static)
peers =

# DNS domain (for discovery = dns)
dns_domain =

# etcd endpoints (for discovery = etcd)
etcd_endpoints =

# Consul address (for discovery = consul)
consul_address =

# Federation enabled
federation = true

# Maximum federated query hops
max_federation_hops = 3

# Federation query timeout (seconds)
federation_timeout = 60

#==============================================================================
# EXTENSIONS SECTION
#==============================================================================
[extensions]
# UDR plugin directory
udr_directory = /usr/lib/scratchbird/udr

# Preload UDR plugins (comma-separated)
preload =

# Shared library path for plugins
library_path = /usr/lib/scratchbird/lib

# Allow untrusted plugins
allow_untrusted = false
```

### 4.3 Configuration Includes

```ini
# Main configuration can include other files
@include /etc/scratchbird/conf.d/*.conf

# Or specific files
@include /etc/scratchbird/ssl.conf
@include /etc/scratchbird/auth.conf
```

### 4.4 Environment Variable Substitution

```ini
[server]
# Use environment variables
data_dir = ${SCRATCHBIRD_DATA_DIR:-/var/lib/scratchbird}

[ldap]
# Sensitive values from environment
bind_password = ${LDAP_BIND_PASSWORD}
```

---

## 5. Database Modes

### 5.1 Single-Database Mode

One server instance manages one database file:

```ini
[server]
mode = single-database
database = /var/lib/scratchbird/production.sbdb
```

**Use Cases:**
- Development and testing
- Small deployments
- Embedded-style usage with network access
- Maximum isolation

**Behavior:**
- All connections share single database
- `CREATE DATABASE` fails (not supported in this mode)
- `USE database` fails
- Simpler resource management

### 5.2 Multi-Database Mode

One server instance manages multiple database files:

```ini
[server]
mode = multi-database
data_dir = /var/lib/scratchbird
auto_create = false
```

**Directory Structure:**
```
/var/lib/scratchbird/
├── main.sbdb              # Default database
├── production.sbdb        # Production database
├── analytics.sbdb         # Analytics database
├── test.sbdb              # Test database
└── system/                # System metadata
    ├── users.sbdb         # Shared user database
    └── config.sbdb        # Cluster configuration
```

**Use Cases:**
- Production servers
- Multi-tenant applications
- Logical separation of data

**Behavior:**
- Clients specify database in connection string
- `CREATE DATABASE name` creates new .sbdb file (ScratchBird only; emulated dialects create metadata only)
- `DROP DATABASE name` removes file (with safeguards)
- Per-database resource limits
- Cross-database queries (with federation)

### 5.3 Database Selection

**Connection String:**
```
# Native protocol
scratchbird://user:pass@host:3092/database_name

# PostgreSQL protocol
postgresql://user:pass@host:5432/database_name

# MySQL protocol
mysql://user:pass@host:3306/database_name
```

**Default Database:**
- If no database specified: use `main` database
- Can be configured: `default_database = main`

---

## 6. systemd Integration

### 6.1 Service Unit File

```ini
# /etc/systemd/system/scratchbird.service
[Unit]
Description=ScratchBird Database Server
Documentation=https://scratchbird.dev/docs
After=network-online.target
Wants=network-online.target
# Optional dependencies
After=time-sync.target
After=nss-lookup.target

[Service]
Type=notify
User=scratchbird
Group=scratchbird

# Paths
Environment=SCRATCHBIRD_CONFIG=/etc/scratchbird/sb_server.conf
WorkingDirectory=/var/lib/scratchbird

# Main process
ExecStart=/usr/bin/sb_server --config ${SCRATCHBIRD_CONFIG}

# Reload configuration
ExecReload=/bin/kill -HUP $MAINPID

# Graceful shutdown
ExecStop=/bin/kill -TERM $MAINPID
TimeoutStopSec=30

# Restart policy
Restart=on-failure
RestartSec=5
RestartPreventExitStatus=SIGTERM

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096
LimitCORE=infinity
LimitMEMLOCK=infinity

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ProtectKernelTunables=true
ProtectKernelModules=true
ProtectKernelLogs=true
ProtectControlGroups=true
ProtectProc=invisible
ProcSubset=pid
RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6
RestrictNamespaces=true
RestrictRealtime=true
RestrictSUIDSGID=true
LockPersonality=true
MemoryDenyWriteExecute=true
SystemCallArchitectures=native
SystemCallFilter=@system-service
SystemCallFilter=~@privileged @resources

# Allow binding to privileged ports (< 1024) if needed
AmbientCapabilities=CAP_NET_BIND_SERVICE
CapabilityBoundingSet=CAP_NET_BIND_SERVICE

# Writable paths
ReadWritePaths=/var/lib/scratchbird
ReadWritePaths=/var/log/scratchbird
ReadWritePaths=/var/run/scratchbird

# Private /tmp
PrivateTmp=true

# systemd notification
NotifyAccess=main
WatchdogSec=60

[Install]
WantedBy=multi-user.target
```

### 6.2 Socket Activation (Optional)

For on-demand startup:

```ini
# /etc/systemd/system/scratchbird.socket
[Unit]
Description=ScratchBird Database Server Socket

[Socket]
ListenStream=3092
ListenStream=5432
ListenStream=3306
ListenStream=/var/run/scratchbird/sb.sock
Accept=false
NoDelay=true

[Install]
WantedBy=sockets.target
```

### 6.3 Drop-in Overrides

```ini
# /etc/systemd/system/scratchbird.service.d/override.conf
[Service]
# Override memory limit
MemoryMax=8G

# Add environment variable
Environment=SCRATCHBIRD_LOG_LEVEL=debug

# Change user
User=postgres
```

### 6.4 systemd Notification Protocol

The server implements `sd_notify()` for lifecycle events:

```c
// Startup complete
sd_notify(0, "READY=1");

// Status updates
sd_notify(0, "STATUS=Accepting connections on port 3092");
sd_notify(0, "STATUS=Processing 42 queries");

// Reloading configuration
sd_notify(0, "RELOADING=1");
// ... reload ...
sd_notify(0, "READY=1");

// Stopping
sd_notify(0, "STOPPING=1");

// Watchdog ping (every WatchdogSec/2)
sd_notify(0, "WATCHDOG=1");

// Extend timeout during slow operations
sd_notify(0, "EXTEND_TIMEOUT_USEC=60000000");  // 60 seconds
```

### 6.5 Service Management Commands

```bash
# Start service
sudo systemctl start scratchbird

# Stop service (graceful)
sudo systemctl stop scratchbird

# Restart service
sudo systemctl restart scratchbird

# Reload configuration (no restart)
sudo systemctl reload scratchbird

# Check status
sudo systemctl status scratchbird

# View logs
sudo journalctl -u scratchbird -f

# Enable at boot
sudo systemctl enable scratchbird

# Disable at boot
sudo systemctl disable scratchbird
```

---

## 7. Signal Handling

### 7.1 Signal Table

| Signal | Action | Description |
|--------|--------|-------------|
| `SIGTERM` | Graceful shutdown | Stop accepting connections, finish queries, exit |
| `SIGINT` | Graceful shutdown | Same as SIGTERM |
| `SIGQUIT` | Immediate shutdown | Rollback active transactions, exit immediately |
| `SIGHUP` | Reload configuration | Re-read config file, apply changes |
| `SIGUSR1` | Rotate log files | Close and reopen log files |
| `SIGUSR2` | Dump statistics | Write current statistics to log |
| `SIGCONT` | Resume | Resume after SIGSTOP |

### 7.2 Graceful Shutdown Sequence

```
1. Receive SIGTERM
2. Stop accepting new connections
3. Set "shutting down" flag
4. Wait for active queries to complete (up to shutdown_timeout)
5. Send "connection closing" to idle clients
6. Checkpoint all databases
7. Close all connections
8. Release all resources
9. Exit with code 0
```

### 7.3 Configuration Reload (SIGHUP)

**Reloadable Settings:**
- Connection limits
- Timeouts
- Pool settings
- Logging levels
- Authentication settings (except methods)
- SSL certificates (requires new connections)
- IP whitelists

**Non-Reloadable Settings (require restart):**
- Ports and bind addresses
- Data directory
- Worker thread count
- Shared buffer size
- Authentication methods

```bash
# Reload configuration
sudo systemctl reload scratchbird
# Or
sudo kill -HUP $(cat /var/run/scratchbird/sb_server.pid)
```

### 7.4 Log Rotation (SIGUSR1)

```bash
# Rotate logs
sudo kill -USR1 $(cat /var/run/scratchbird/sb_server.pid)

# Or with logrotate
cat > /etc/logrotate.d/scratchbird << 'EOF'
/var/log/scratchbird/*.log {
    daily
    rotate 30
    compress
    delaycompress
    notifempty
    create 640 scratchbird scratchbird
    sharedscripts
    postrotate
        /bin/kill -USR1 $(cat /var/run/scratchbird/sb_server.pid 2>/dev/null) 2>/dev/null || true
    endscript
}
EOF
```

---

## 8. Lifecycle Management

### 8.1 Startup Sequence

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Startup Sequence                                 │
└─────────────────────────────────────────────────────────────────────────┘

1. Parse command-line arguments
2. Load configuration file
3. Initialize logging
4. Check/create PID file
5. Drop privileges (if started as root)
6. Initialize SSL context
7. Initialize shared memory
8. Open database files
   ├── Verify database integrity
   ├── Skip write-after log (WAL) replay (MGA); optional write-after log replay if configured
   └── Initialize buffer pool
9. Start background services
   ├── Checkpoint writer
   ├── Garbage collector
   └── Stats collector
10. Start listener threads
    ├── Native protocol (3092)
    ├── PostgreSQL (5432)
    ├── MySQL (3306)
    ├── TDS (1433, post-gold)
    ├── Firebird (3050)
    └── Unix socket
11. Start worker thread pool
12. Notify systemd: READY=1
13. Enter main event loop
```

### 8.2 Shutdown Sequence

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        Shutdown Sequence                                 │
└─────────────────────────────────────────────────────────────────────────┘

1. Receive shutdown signal
2. Notify systemd: STOPPING=1
3. Stop accepting new connections
4. Wait for queries to complete (with timeout)
   ├── Log queries that are taking too long
   └── Cancel queries exceeding timeout
5. Disconnect all clients
6. Stop worker threads
7. Final checkpoint for each database
8. Close database files
9. Stop background services
10. Release shared memory
11. Remove PID file
12. Exit with appropriate code
```

### 8.3 Recovery Startup

If the server crashed:

```
1. Detect unclean shutdown (PID file exists, process not running)
2. Remove stale PID file
3. For each database:
   ├── Open database file
   ├── Detect incomplete checkpoint
   ├── Skip write-after log (WAL) replay (MGA); optional write-after log replay if configured
   ├── Verify data integrity
   └── Complete recovery
4. Continue normal startup
```

---

## 9. Logging

### 9.1 Log Levels

| Level | Description | Example |
|-------|-------------|---------|
| `debug` | Detailed debugging | Query plans, lock details |
| `info` | Informational | Startup, shutdown, connections |
| `notice` | Normal but significant | Configuration changes |
| `warning` | Warning conditions | Approaching limits |
| `error` | Error conditions | Query failures |

### 9.2 Log Format (Text)

```
2025-12-10 14:30:45.123 UTC [12345] [worker-3] INFO: Connection received from 192.168.1.100:54321
2025-12-10 14:30:45.456 UTC [12345] [worker-3] INFO: User 'alice' authenticated successfully
2025-12-10 14:30:46.789 UTC [12345] [worker-3] DEBUG: Executing query: SELECT * FROM users WHERE id = $1
2025-12-10 14:30:46.791 UTC [12345] [worker-3] DEBUG: Query completed in 2.1ms, returned 1 row
```

### 9.3 Log Format (JSON)

```json
{
  "timestamp": "2025-12-10T14:30:45.123Z",
  "level": "INFO",
  "pid": 12345,
  "thread": "worker-3",
  "message": "Connection received",
  "client_ip": "192.168.1.100",
  "client_port": 54321,
  "database": "production",
  "user": null
}
```

### 9.4 Structured Log Fields

| Field | Description |
|-------|-------------|
| `timestamp` | ISO 8601 timestamp |
| `level` | Log level |
| `pid` | Process ID |
| `thread` | Thread name |
| `session_id` | Client session ID |
| `database` | Database name |
| `user` | Username |
| `client_ip` | Client IP address |
| `query_id` | Query identifier |
| `duration_ms` | Operation duration |
| `rows` | Rows affected/returned |
| `error_code` | SQLSTATE error code |

### 9.5 Log Destinations

**File:**
```ini
[logging]
destination = file
file = /var/log/scratchbird/sb_server.log
```

**Syslog:**
```ini
[logging]
destination = syslog
syslog_facility = local0
syslog_ident = scratchbird
```

**journald (stderr):**
```ini
[logging]
destination = stderr
# journald captures stderr automatically
```

---

## 10. Monitoring and Health Checks

### 10.1 Health Check Endpoint

The server provides a health check on a dedicated port:

```ini
[statistics]
prometheus_port = 9090
```

**Endpoints:**

| Endpoint | Description |
|----------|-------------|
| `GET /health` | Basic health check (200 OK or 503) |
| `GET /ready` | Readiness check (accepting connections) |
| `GET /live` | Liveness check (process alive) |
| `GET /metrics` | Prometheus metrics |

### 10.2 Health Check Response

```json
GET /health

{
  "status": "healthy",
  "version": "1.0.0",
  "uptime_seconds": 86400,
  "databases": {
    "main": "healthy",
    "production": "healthy"
  },
  "connections": {
    "active": 42,
    "idle": 58,
    "max": 100
  },
  "memory": {
    "buffer_pool_used": "64MB",
    "buffer_pool_total": "128MB"
  }
}
```

### 10.3 Prometheus Metrics

```prometheus
# HELP scratchbird_up Server is up
# TYPE scratchbird_up gauge
scratchbird_up 1

# HELP scratchbird_connections_active Active connections
# TYPE scratchbird_connections_active gauge
scratchbird_connections_active{database="main"} 42

# HELP scratchbird_connections_total Total connections
# TYPE scratchbird_connections_total counter
scratchbird_connections_total{database="main"} 12345

# HELP scratchbird_queries_total Total queries executed
# TYPE scratchbird_queries_total counter
scratchbird_queries_total{database="main",type="select"} 100000
scratchbird_queries_total{database="main",type="insert"} 5000

# HELP scratchbird_query_duration_seconds Query duration histogram
# TYPE scratchbird_query_duration_seconds histogram
scratchbird_query_duration_seconds_bucket{le="0.001"} 50000
scratchbird_query_duration_seconds_bucket{le="0.01"} 80000
scratchbird_query_duration_seconds_bucket{le="0.1"} 95000
scratchbird_query_duration_seconds_bucket{le="1"} 99000
scratchbird_query_duration_seconds_bucket{le="+Inf"} 100000

# HELP scratchbird_buffer_pool_hit_ratio Buffer pool hit ratio
# TYPE scratchbird_buffer_pool_hit_ratio gauge
scratchbird_buffer_pool_hit_ratio 0.987

# HELP scratchbird_checkpoint_duration_seconds Checkpoint duration
# TYPE scratchbird_checkpoint_duration_seconds histogram

# HELP scratchbird_replication_lag_seconds Replication lag
# TYPE scratchbird_replication_lag_seconds gauge

# HELP scratchbird_locks_waiting Queries waiting for locks
# TYPE scratchbird_locks_waiting gauge
```

### 10.4 SHOW Commands

```sql
-- Server status
SHOW SERVER STATUS;

-- Connection status
SHOW CONNECTIONS;
SHOW CONNECTION 12345;

-- Pool status
SHOW POOL STATUS;

-- Database status
SHOW DATABASES;
SHOW DATABASE main;

-- Configuration
SHOW CONFIG;
SHOW CONFIG network;

-- Statistics
SHOW STATISTICS;
SHOW STATISTICS queries;

-- Locks
SHOW LOCKS;
SHOW LOCKS WAITING;

-- Activity
SHOW ACTIVITY;
SHOW ACTIVITY WHERE state = 'active';
```

---

## 11. Security Hardening

### 11.1 User and Group

```bash
# Create dedicated user
sudo useradd --system --home-dir /var/lib/scratchbird \
    --shell /usr/sbin/nologin scratchbird

# Set ownership
sudo chown -R scratchbird:scratchbird /var/lib/scratchbird
sudo chown -R scratchbird:scratchbird /var/log/scratchbird
sudo chown -R scratchbird:scratchbird /var/run/scratchbird
```

### 11.2 File Permissions

```bash
# Configuration files
chmod 640 /etc/scratchbird/sb_server.conf
chmod 600 /etc/scratchbird/ssl/*.key
chmod 644 /etc/scratchbird/ssl/*.crt

# Data directory
chmod 700 /var/lib/scratchbird

# Log directory
chmod 750 /var/log/scratchbird

# Run directory
chmod 755 /var/run/scratchbird
```

### 11.3 systemd Security Options

The service unit includes comprehensive hardening:

```ini
# Process isolation
NoNewPrivileges=true
PrivateTmp=true
PrivateDevices=true

# Filesystem protection
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/scratchbird /var/log/scratchbird /var/run/scratchbird

# Kernel protection
ProtectKernelTunables=true
ProtectKernelModules=true
ProtectKernelLogs=true
ProtectControlGroups=true

# Network restrictions
RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6

# System call filtering
SystemCallArchitectures=native
SystemCallFilter=@system-service
SystemCallFilter=~@privileged @resources

# Memory protection
MemoryDenyWriteExecute=true

# Other restrictions
RestrictNamespaces=true
RestrictRealtime=true
RestrictSUIDSGID=true
LockPersonality=true
```

### 11.4 Capability Dropping

If started as root, the server drops to the configured user after:
1. Binding to privileged ports (< 1024)
2. Opening database files
3. Creating PID file

```c
// Drop privileges
if (getuid() == 0) {
    // Keep only needed capabilities
    cap_t caps = cap_init();
    cap_set_flag(caps, CAP_EFFECTIVE, 1, (cap_value_t[]){CAP_NET_BIND_SERVICE}, CAP_SET);
    cap_set_proc(caps);
    cap_free(caps);

    // Change to service user
    setgid(scratchbird_gid);
    setuid(scratchbird_uid);
}
```

### 11.5 Network Security

```ini
[network]
# Bind to specific interface only
bind_address = 192.168.1.10

# Or localhost only
bind_address = 127.0.0.1

[ssl]
# Force TLS
enabled = true
min_protocol = TLSv1.2

# Strong ciphers only
ciphers = TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256

[authentication]
# Disable remote superuser
allow_superuser_remote = false
```

---

## 12. Cluster Service Mode

### 12.1 Multi-Node Configuration

Each node in a cluster runs sb_server with cluster configuration:

```ini
# Node 1: /etc/scratchbird/sb_server.conf
[cluster]
enabled = true
name = production-cluster
node_id = 550e8400-e29b-41d4-a716-446655440001
ca_cert = /etc/scratchbird/cluster/ca.crt
node_cert = /etc/scratchbird/cluster/node1.crt
node_key = /etc/scratchbird/cluster/node1.key
discovery = static
peers = node2.example.com:3092,node3.example.com:3092
federation = true
```

### 12.2 Cluster Service Template

For multiple nodes on same machine (development):

```ini
# /etc/systemd/system/scratchbird@.service
[Unit]
Description=ScratchBird Database Server (Node %i)
After=network-online.target

[Service]
Type=notify
User=scratchbird
ExecStart=/usr/bin/sb_server --config /etc/scratchbird/node%i.conf
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

```bash
# Start nodes
sudo systemctl start scratchbird@1
sudo systemctl start scratchbird@2
sudo systemctl start scratchbird@3
```

### 12.3 Cluster Health Checks

```bash
# Check cluster status
sb_admin cluster status

# Output:
Cluster: production-cluster
Status: healthy
Nodes:
  node1 (550e8400-...-440001): primary, online, 0ms lag
  node2 (550e8400-...-440002): standby, online, 12ms lag
  node3 (550e8400-...-440003): standby, online, 8ms lag
Federation: enabled
Databases: 3 (main, production, analytics)
```

---

## 13. Package Installation

### 13.1 Directory Layout

```
/usr/bin/
├── sb_server           # Main server binary
├── sb_isql             # Interactive SQL client
├── sb_admin            # Administration utility
├── sb_backup           # Backup utility
├── sb_restore          # Restore utility
└── sb_verify           # Database verification

/etc/scratchbird/
├── sb_server.conf      # Main configuration
├── conf.d/             # Drop-in configuration
├── ssl/                # SSL certificates
│   ├── server.crt
│   ├── server.key
│   ├── ca.crt
│   └── dh4096.pem
└── cluster/            # Cluster certificates
    ├── ca.crt
    ├── node.crt
    └── node.key

/var/lib/scratchbird/   # Data directory
├── main.sbdb           # Default database
└── wal/                # Optional write-after log files (post-gold)

/var/log/scratchbird/   # Log directory
├── sb_server.log       # Main server log
└── audit.log           # Audit log

/var/run/scratchbird/   # Runtime directory
├── sb_server.pid       # PID file
└── sb.sock             # Unix socket

/usr/lib/scratchbird/   # Libraries and plugins
├── lib/                # Shared libraries
└── udr/                # UDR plugins

/usr/share/scratchbird/ # Shared data
├── doc/                # Documentation
└── examples/           # Example configurations
```

### 13.2 Post-Install Script

```bash
#!/bin/bash
# /usr/lib/scratchbird/postinstall.sh

# Create user if doesn't exist
if ! id scratchbird &>/dev/null; then
    useradd --system --home-dir /var/lib/scratchbird \
        --shell /usr/sbin/nologin scratchbird
fi

# Create directories
mkdir -p /var/lib/scratchbird
mkdir -p /var/log/scratchbird
mkdir -p /var/run/scratchbird
mkdir -p /etc/scratchbird/conf.d
mkdir -p /etc/scratchbird/ssl
mkdir -p /etc/scratchbird/cluster

# Set ownership
chown -R scratchbird:scratchbird /var/lib/scratchbird
chown -R scratchbird:scratchbird /var/log/scratchbird
chown -R scratchbird:scratchbird /var/run/scratchbird

# Set permissions
chmod 700 /var/lib/scratchbird
chmod 750 /var/log/scratchbird
chmod 755 /var/run/scratchbird
chmod 640 /etc/scratchbird/*.conf
chmod 750 /etc/scratchbird/ssl
chmod 750 /etc/scratchbird/cluster

# Generate self-signed certificate if none exists
if [ ! -f /etc/scratchbird/ssl/server.crt ]; then
    openssl req -new -x509 -days 365 -nodes \
        -out /etc/scratchbird/ssl/server.crt \
        -keyout /etc/scratchbird/ssl/server.key \
        -subj "/CN=scratchbird/O=ScratchBird"
    chmod 600 /etc/scratchbird/ssl/server.key
fi

# Generate DH parameters if none exist
if [ ! -f /etc/scratchbird/ssl/dh4096.pem ]; then
    openssl dhparam -out /etc/scratchbird/ssl/dh4096.pem 4096
fi

# Reload systemd
systemctl daemon-reload

echo "ScratchBird installed successfully."
echo "Start with: sudo systemctl start scratchbird"
```

### 13.3 Package Dependencies

**Runtime Dependencies:**
- libc6 (>= 2.31)
- libssl3 (>= 3.0)
- libzstd1 (>= 1.5)
- systemd (>= 250)

**Optional Dependencies:**
- libldap-2.5 (LDAP authentication)
- libkrb5-3 (Kerberos authentication)
- libsasl2-2 (SASL authentication)

---

## 14. Troubleshooting

### 14.1 Common Issues

**Server won't start:**
```bash
# Check if already running
pgrep -f sb_server
cat /var/run/scratchbird/sb_server.pid

# Check port availability
ss -tlnp | grep -E '3092|5432|3306'

# Check permissions
ls -la /var/lib/scratchbird
ls -la /var/run/scratchbird

# Check configuration
sb_server --config /etc/scratchbird/sb_server.conf --check
```

**Connection refused:**
```bash
# Check service status
systemctl status scratchbird

# Check listening ports
ss -tlnp | grep sb_server

# Check firewall
sudo iptables -L -n | grep -E '3092|5432'

# Test local connection
sb_isql -h localhost -p 3092
```

**High memory usage:**
```bash
# Check buffer pool
sb_admin show buffers

# Check connection count
sb_admin show connections | wc -l

# Check for memory leaks (debug build)
valgrind --leak-check=full sb_server -F
```

**Slow queries:**
```bash
# Enable slow query log
sb_admin set log_slow_queries 100

# Check active queries
sb_admin show activity

# Check locks
sb_admin show locks waiting
```

### 14.2 Debug Mode

```bash
# Start in foreground with debug logging
sb_server -F -c /etc/scratchbird/sb_server.conf \
    --log-level debug

# Or set environment variable
SCRATCHBIRD_LOG_LEVEL=debug sb_server -F
```

### 14.3 Diagnostic Commands

```bash
# Server information
sb_admin info

# Configuration dump
sb_admin show config

# Connection details
sb_admin show connections --verbose

# Query activity
sb_admin show activity --queries

# Lock information
sb_admin show locks --blocking

# Buffer pool statistics
sb_admin show buffers --histogram

# Write-after log status (optional post-gold)
sb_admin show wal  # optional post-gold

# Replication status (if enabled)
sb_admin show replication
```

---

## 15. Configuration Reference

### 15.1 Quick Reference Table

| Section | Key | Default | Reloadable | Description |
|---------|-----|---------|------------|-------------|
| server | mode | multi-database | No | Database mode |
| server | data_dir | /var/lib/scratchbird | No | Data directory |
| server | max_connections | 100 | Yes | Max connections |
| server | worker_threads | auto | No | Worker count |
| server | shutdown_timeout | 30 | Yes | Shutdown timeout |
| network | bind_address | 0.0.0.0 | No | Bind address |
| network | native_port | 3092 | No | Native port |
| network | pg_port | 5432 | No | PostgreSQL port |
| network | tcp_keepalive | 60 | Yes | Keepalive interval |
| ssl | enabled | true | No | Enable SSL |
| ssl | min_protocol | TLSv1.2 | Yes* | Min TLS version |
| ssl | cert_file | - | Yes* | Certificate file |
| pool | min_connections | 5 | Yes | Min pool connections |
| pool | max_connections | 50 | Yes | Max pool connections |
| pool | idle_timeout | 300 | Yes | Idle timeout |
| memory | buffer_pool_size | 128MB | No | Buffer pool size |
| memory | buffer_pool_page_size | 8192 | No | Buffer pool page size (bytes) |
| memory | buffer_pool_bgwriter_enabled | true | Yes | Enable background writer |
| memory | buffer_pool_bgwriter_max_pages | 1000 | Yes | Max pages per bgwriter cycle |
| memory | buffer_pool_dirty_ratio_low | 0.10 | Yes | Low dirty ratio threshold |
| memory | buffer_pool_dirty_ratio_high | 0.30 | Yes | High dirty ratio threshold |
| memory | buffer_pool_dirty_ratio_checkpoint | 0.40 | Yes | Checkpoint dirty ratio threshold |
| memory | work_mem | 4MB | Yes | Work memory |
| logging | level | info | Yes | Log level |
| logging | destination | file | No | Log destination |
| audit | enabled | true | Yes | Enable audit |
| statistics | enabled | true | Yes | Enable stats |
| drivers | native_host | localhost | Yes | Default native driver host |
| drivers | native_port | 3092 | Yes | Default native driver port |
| drivers | sslmode | prefer | Yes | Default driver SSL mode |
| drivers | connect_timeout_ms | 5000 | Yes | Driver connect timeout |
| drivers | default_database | - | Yes | Default database name for drivers |
| drivers | default_role | - | Yes | Default role for drivers |
| drivers | allow_cleartext | false | Yes | Allow cleartext auth for drivers |
| drivers | application_name | scratchbird_driver | Yes | Driver application_name default |

*Requires new connections to take effect

### 15.2 Size Value Format

```ini
# Bytes
buffer_pool_size = 134217728

# Kilobytes
buffer_pool_size = 131072KB
buffer_pool_size = 131072K

# Megabytes
buffer_pool_size = 128MB
buffer_pool_size = 128M

# Gigabytes
buffer_pool_size = 1GB
buffer_pool_size = 1G
```

### 15.3 Time Value Format

```ini
# Milliseconds
statement_timeout = 30000
statement_timeout = 30000ms

# Seconds
idle_timeout = 300
idle_timeout = 300s
idle_timeout = 5m

# Minutes
session_timeout = 60m
session_timeout = 1h

# Hours
max_lifetime = 1h
```

### 15.4 Environment Variable Mapping

| Config Key | Environment Variable |
|------------|---------------------|
| server.data_dir | SCRATCHBIRD_DATA_DIR |
| logging.level | SCRATCHBIRD_LOG_LEVEL |
| server.max_connections | SCRATCHBIRD_MAX_CONNECTIONS |
| network.native_port | SCRATCHBIRD_PORT |
| drivers.native_host | SCRATCHBIRD_DRIVER_HOST |
| drivers.native_port | SCRATCHBIRD_DRIVER_PORT |
| drivers.sslmode | SCRATCHBIRD_DRIVER_SSLMODE |
| drivers.connect_timeout_ms | SCRATCHBIRD_DRIVER_CONNECT_TIMEOUT_MS |
| drivers.default_database | SCRATCHBIRD_DRIVER_DATABASE |
| drivers.application_name | SCRATCHBIRD_DRIVER_APPLICATION_NAME |
| drivers.ssl_cert | SCRATCHBIRD_DRIVER_SSL_CERT |
| drivers.ssl_key | SCRATCHBIRD_DRIVER_SSL_KEY |
| drivers.ssl_root_cert | SCRATCHBIRD_DRIVER_SSL_ROOT_CERT |

---

## Appendix A: Example Configurations

### A.1 Development Configuration

```ini
# /etc/scratchbird/sb_server.conf (Development)
[server]
mode = single-database
database = ./dev.sbdb
auto_create = true
max_connections = 10

[network]
bind_address = 127.0.0.1
native_port = 3092
pg_port = 5432
mysql_port = 0
tds_port = 0
fb_port = 0

[drivers]
native_host = localhost
native_port = 3092
sslmode = disable
default_database = dev

[ssl]
enabled = false

[logging]
level = debug
destination = stderr

[authentication]
methods = trust
```

### A.2 Production Configuration

```ini
# /etc/scratchbird/sb_server.conf (Production)
[server]
mode = multi-database
data_dir = /var/lib/scratchbird
max_connections = 200
worker_threads = 16
shutdown_timeout = 60

[network]
bind_address = 0.0.0.0
native_port = 3092
pg_port = 5432
mysql_port = 3306
tds_port = 0
fb_port = 0
tcp_keepalive = 30

[ssl]
enabled = true
cert_file = /etc/scratchbird/ssl/server.crt
key_file = /etc/scratchbird/ssl/server.key
ca_file = /etc/scratchbird/ssl/ca.crt
min_protocol = TLSv1.3
require_client_cert = false

[pool]
min_connections = 10
max_connections = 100
idle_timeout = 600
statement_cache = true
statement_cache_size = 2000

[memory]
buffer_pool_size = 4GB
buffer_pool_page_size = 8192
buffer_pool_bgwriter_enabled = true
buffer_pool_bgwriter_max_pages = 5000
buffer_pool_dirty_ratio_low = 0.10
buffer_pool_dirty_ratio_high = 0.30
buffer_pool_dirty_ratio_checkpoint = 0.40
work_mem = 64MB
maintenance_work_mem = 512MB

[authentication]
methods = scram-sha-256
password_hash = argon2id
max_failed_attempts = 5
lockout_duration = 600

[logging]
level = info
destination = file
file = /var/log/scratchbird/sb_server.log
format = json
log_slow_queries = 500

[audit]
enabled = true
log_file = /var/log/scratchbird/audit.log
log_format = json
events = connect,authenticate,authorize,ddl,error

[statistics]
enabled = true
export = prometheus
prometheus_port = 9090
```

### A.3 High-Availability Configuration

```ini
# /etc/scratchbird/sb_server.conf (HA Primary)
[server]
mode = multi-database
data_dir = /var/lib/scratchbird
max_connections = 500

[cluster]
enabled = true
name = production-ha
node_id = 550e8400-e29b-41d4-a716-446655440001
ca_cert = /etc/scratchbird/cluster/ca.crt
node_cert = /etc/scratchbird/cluster/primary.crt
node_key = /etc/scratchbird/cluster/primary.key
discovery = static
peers = standby1.example.com:3092,standby2.example.com:3092
federation = true

[replication]
enabled = true
role = primary
max_replication_slots = 10
max_wal_senders = 10
synchronous_commit = remote_apply

[storage]
fsync = on
synchronous_commit = on
full_page_writes = on
wal_compression = zstd
```

---

## Appendix B: Migration from Embedded to Service Mode

### B.1 Steps

1. Stop application using embedded database
2. Install sb_server package
3. Move database file to data directory
4. Configure sb_server.conf
5. Start service
6. Update application connection string
7. Start application

### B.2 Connection String Changes

**Before (embedded):**
```cpp
Database db("/path/to/mydb.sbdb");
```

**After (service):**
```cpp
Connection conn("scratchbird://user:pass@localhost:3092/mydb");
```

---

**Document End**
