# sb_server.conf Reference

Complete reference for the main server configuration file.

[Back to Configuration Index](index.md) | [Back to Documentation Index](../index.md)

---

## File Location

| Installation | Path |
|--------------|------|
| DEB/RPM | `/etc/scratchbird/sb_server.conf` |
| Tarball | `/opt/scratchbird/conf/sb_server.conf` |
| Docker | `/etc/scratchbird/sb_server.conf` |
| Windows | `C:\ProgramData\ScratchBird\sb_server.conf` |

---

## [server] Section

Core server settings.

### mode

```ini
mode = multi-database
```

| Value | Description |
|-------|-------------|
| `multi-database` | Multiple databases in data directory (default) |
| `single-database` | Single database file only |

### data_dir

```ini
data_dir = /var/lib/scratchbird
```

Directory containing database files. In multi-database mode, each `.sbdb` file is a separate database.

### database (single-database mode)

```ini
database = /var/lib/scratchbird/main.sbdb
```

Path to database file when using single-database mode.

### auto_create

```ini
auto_create = false
```

Automatically create database files when connecting to non-existent databases.

| Value | Behavior |
|-------|----------|
| `false` | Error on missing database (default) |
| `true` | Create new database file |

### pid_file

```ini
pid_file = /var/run/scratchbird/sb_server.pid
```

Location of PID file for process management.

### shutdown_timeout

```ini
shutdown_timeout = 30
```

Maximum seconds to wait for graceful shutdown before forcing termination.

### worker_threads

```ini
worker_threads = 0
```

Number of worker threads for query processing.

| Value | Behavior |
|-------|----------|
| `0` | Auto-detect based on CPU cores (default) |
| `N` | Fixed number of threads |

### max_connections

```ini
max_connections = 100
```

Maximum concurrent connections across all protocols.

### max_connections_per_user

```ini
max_connections_per_user = 0
```

Maximum connections per user account. `0` = unlimited.

### max_connections_per_database

```ini
max_connections_per_database = 0
```

Maximum connections per database. `0` = unlimited.

### idle_timeout

```ini
idle_timeout = 3600
```

Seconds before idle connections are closed. `0` = never.

### statement_timeout

```ini
statement_timeout = 0
```

Maximum query execution time in milliseconds. `0` = unlimited.

---

## [scheduler] Section

Job scheduler settings.

### enabled

```ini
enabled = true
```

Enable the job scheduler background thread.

### polling_interval_seconds

```ini
polling_interval_seconds = 10
```

Seconds between scheduler polling ticks.

### max_jobs_per_tick

```ini
max_jobs_per_tick = 16
```

Maximum number of due jobs to launch per polling tick.

### cron_fallback_seconds

```ini
cron_fallback_seconds = 60
```

Fallback interval when cron parsing yields no upcoming schedule.

### pre_execute_delay_ms

```ini
pre_execute_delay_ms = 0
```

Optional delay before executing each job (used primarily for testing).

---

## [network] Section

Network and protocol settings.

### bind_address

```ini
bind_address = 0.0.0.0
```

IP address to listen on.

| Value | Behavior |
|-------|----------|
| `0.0.0.0` | All interfaces |
| `127.0.0.1` | Localhost only |
| Specific IP | Single interface |

### Protocol Ports

```ini
native_port = 3092    # ScratchBird native protocol
pg_port = 5432        # PostgreSQL protocol
mysql_port = 3306     # MySQL protocol
fb_port = 3050        # Firebird protocol
```

Set to `0` to disable a protocol.

### Unix Socket

```ini
unix_socket = /var/run/scratchbird/sb.sock
unix_socket_permissions = 0770
unix_socket_group = scratchbird
```

Unix domain socket for local connections. Empty to disable.

### TCP Settings

```ini
tcp_keepalive = 60      # Keepalive interval (seconds)
listen_backlog = 128    # Connection queue size
tcp_nodelay = true      # Disable Nagle's algorithm
```

---

## [ssl] Section

TLS/SSL encryption settings.

### enabled

```ini
enabled = false
```

Enable SSL/TLS for encrypted connections.

### Certificate Files

```ini
cert_file = /etc/scratchbird/ssl/server.crt
key_file = /etc/scratchbird/ssl/server.key
ca_file = /etc/scratchbird/ssl/ca.crt
```

Paths to SSL certificate files (PEM format).

### min_protocol

```ini
min_protocol = TLSv1.2
```

Minimum TLS version.

| Value | Description |
|-------|-------------|
| `TLSv1.2` | TLS 1.2 (recommended minimum) |
| `TLSv1.3` | TLS 1.3 only |

### require_client_cert

```ini
require_client_cert = false
```

Require client certificate authentication.

---

## [memory] Section

Memory allocation settings.

### buffer_pool_size

```ini
buffer_pool_size = 128MB
```

Size of the buffer pool for caching database pages.

**Recommendation:** 25% of available RAM, up to 8GB.

### buffer_pool_page_size

```ini
buffer_pool_page_size = 8192
```

Page size in bytes for the buffer pool.

### buffer_pool_layout

```ini
buffer_pool_layout = single
```

Buffer pool layout selection. Options: `single`, `hot_cold`, `tablespace`.

### buffer_pool_bgwriter_enabled

```ini
buffer_pool_bgwriter_enabled = true
```

Enable the background writer.

### buffer_pool_bgwriter_max_pages

```ini
buffer_pool_bgwriter_max_pages = 1000
```

Maximum pages written per background writer cycle.

### buffer_pool_dirty_ratio_low

```ini
buffer_pool_dirty_ratio_low = 0.10
```

Low dirty ratio threshold.

### buffer_pool_dirty_ratio_high

```ini
buffer_pool_dirty_ratio_high = 0.30
```

High dirty ratio threshold.

### buffer_pool_dirty_ratio_checkpoint

```ini
buffer_pool_dirty_ratio_checkpoint = 0.40
```

Dirty ratio threshold used during checkpoints.

### work_mem

```ini
work_mem = 4MB
```

Memory per operation for sorts, hash joins, etc.

**Recommendation:** Start with 4MB, increase for complex queries.

### maintenance_work_mem

```ini
maintenance_work_mem = 64MB
```

Memory for maintenance operations (sweep/GC, CREATE INDEX).

**Recommendation:** Higher values speed up maintenance.

---

## [pool] Section

Connection pool and cache settings.

### statement_cache

```ini
statement_cache = true
```

Enable prepared statement caching.

### statement_cache_size

```ini
statement_cache_size = 1000
```

Maximum cached statements per connection.

### result_cache

```ini
result_cache = true
```

Enable query result caching.

### result_cache_size

```ini
result_cache_size = 67108864
```

Result cache maximum size (bytes).

### result_cache_ttl

```ini
result_cache_ttl = 300
```

Result cache TTL (seconds).

---

## [authentication] Section

Authentication and password settings.

### methods

```ini
methods = scram-sha-256
```

Authentication methods (comma-separated).

| Method | Description |
|--------|-------------|
| `scram-sha-256` | SCRAM-SHA-256 (recommended) |
| `scram-sha-512` | SCRAM-SHA-512 |
| `md5` | MD5 (legacy, not recommended) |
| `trust` | No authentication (dangerous!) |

### password_hash

```ini
password_hash = argon2id
```

Algorithm for storing passwords.

| Value | Description |
|-------|-------------|
| `argon2id` | Argon2id (recommended) |
| `bcrypt` | bcrypt |

### password_min_length

```ini
password_min_length = 12
```

Minimum password length.

### max_failed_attempts

```ini
max_failed_attempts = 5
```

Failed login attempts before account lockout.

### lockout_duration

```ini
lockout_duration = 300
```

Account lockout duration in seconds.

### allow_superuser_remote

```ini
allow_superuser_remote = false
```

Allow superuser login from non-localhost.

**Security:** Keep `false` in production.

---

## [logging] Section

Logging configuration.

### level

```ini
level = info
```

Minimum log level.

| Level | Description |
|-------|-------------|
| `debug` | Verbose debugging |
| `info` | Informational messages |
| `notice` | Notable events |
| `warning` | Warnings |
| `error` | Errors only |

### destination

```ini
destination = file
```

Log output destination.

| Value | Description |
|-------|-------------|
| `stderr` | Standard error |
| `syslog` | System log |
| `file` | Log file |

### file

```ini
file = /var/log/scratchbird/sb_server.log
```

Log file path (when `destination = file`).

### timestamps

```ini
timestamps = true
```

Include timestamps in log entries.

### log_slow_queries

```ini
log_slow_queries = 1000
```

Log queries exceeding this duration (milliseconds). `0` = disabled.

### log_connections

```ini
log_connections = true
```

Log client connections.

### log_disconnections

```ini
log_disconnections = true
```

Log client disconnections.

---

## [statistics] Section

Statistics and metrics.

### enabled

```ini
enabled = true
```

Enable statistics collection.

### export

```ini
export = none
```

Export format.

| Value | Description |
|-------|-------------|
| `none` | No export (internal only) |
| `prometheus` | Prometheus metrics endpoint |

### prometheus_port

```ini
prometheus_port = 0
```

Port for Prometheus metrics endpoint. `0` = disabled.

---

## [storage] Section

Storage and I/O settings.

### copy_allowed_paths

```ini
copy_allowed_paths =
```

Comma-separated allowlist of directories for server-side `COPY FROM/TO`. Empty means no server-side file access.

### copy_allow_absolute_paths

```ini
copy_allow_absolute_paths = false
```

Allow absolute paths in server-side `COPY` operations.

### copy_allow_relative_paths

```ini
copy_allow_relative_paths = true
```

Allow relative paths in server-side `COPY` operations (resolved under `data_dir`).

### copy_require_superuser

```ini
copy_require_superuser = true
```

Require superuser role for server-side `COPY FROM/TO` file access.

### tablespace_recovery_mode

```ini
tablespace_recovery_mode = strict
```

Controls startup and restore behavior when tablespace files are missing.

| Value | Description |
|-------|-------------|
| `strict` | Fail startup/restore if any tablespace files are missing |
| `allow_missing` | Allow startup/restore to proceed if tablespace files are missing |

---

## Complete Example

```ini
# ScratchBird Server Configuration
# /etc/scratchbird/sb_server.conf

[server]
mode = multi-database
data_dir = /var/lib/scratchbird
auto_create = false
pid_file = /var/run/scratchbird/sb_server.pid
shutdown_timeout = 30
worker_threads = 0
max_connections = 100
idle_timeout = 3600
statement_timeout = 0

[network]
bind_address = 0.0.0.0
native_port = 3092
pg_port = 5432
mysql_port = 3306
fb_port = 3050
unix_socket = /var/run/scratchbird/sb.sock
unix_socket_permissions = 0770
tcp_keepalive = 60
listen_backlog = 128
tcp_nodelay = true

[ssl]
enabled = false
cert_file = /etc/scratchbird/ssl/server.crt
key_file = /etc/scratchbird/ssl/server.key
min_protocol = TLSv1.2

[memory]
buffer_pool_size = 128MB
buffer_pool_page_size = 8192
buffer_pool_layout = single
buffer_pool_bgwriter_enabled = true
buffer_pool_bgwriter_max_pages = 1000
buffer_pool_dirty_ratio_low = 0.10
buffer_pool_dirty_ratio_high = 0.30
buffer_pool_dirty_ratio_checkpoint = 0.40
work_mem = 4MB
maintenance_work_mem = 64MB

[authentication]
methods = scram-sha-256
password_hash = argon2id
password_min_length = 12
max_failed_attempts = 5
lockout_duration = 300
allow_superuser_remote = false

[logging]
level = info
destination = file
file = /var/log/scratchbird/sb_server.log
timestamps = true
log_slow_queries = 1000
log_connections = true
log_disconnections = true

[statistics]
enabled = true
export = none
prometheus_port = 0

[storage]
copy_allowed_paths =
copy_allow_absolute_paths = false
copy_allow_relative_paths = true
copy_require_superuser = true
tablespace_recovery_mode = strict
```

---

## Production Recommendations

### High-Performance Server

```ini
[server]
worker_threads = 0          # Auto-detect
max_connections = 500

[memory]
buffer_pool_size = 4GB        # 25% of 16GB RAM
buffer_pool_page_size = 8192
buffer_pool_layout = single
buffer_pool_bgwriter_enabled = true
buffer_pool_bgwriter_max_pages = 5000
buffer_pool_dirty_ratio_low = 0.10
buffer_pool_dirty_ratio_high = 0.30
buffer_pool_dirty_ratio_checkpoint = 0.40
work_mem = 64MB
maintenance_work_mem = 512MB
```

### Secure Server

```ini
[network]
bind_address = 127.0.0.1    # Localhost only
# Or use firewall for external access

[ssl]
enabled = true
min_protocol = TLSv1.3

[authentication]
methods = scram-sha-256
password_min_length = 16
max_failed_attempts = 3
allow_superuser_remote = false
```

### Development Server

```ini
[server]
auto_create = true

[network]
bind_address = 127.0.0.1

[logging]
level = debug
log_slow_queries = 100

[authentication]
password_min_length = 8
```

---

## Next Steps

- [Configure authentication rules](hba.conf.md)
- [Enable SSL/TLS](ssl-setup.md)
- [Set up monitoring](../admin/monitoring.md)
