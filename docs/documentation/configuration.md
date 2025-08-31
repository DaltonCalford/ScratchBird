### Configuration

**What it is**

ScratchBird configuration controls server behavior, performance tuning, resource limits, security settings, and operational parameters. Configuration can be set through configuration files, environment variables, command-line arguments, or runtime SET commands. The configuration system supports hierarchical defaults with override capabilities at multiple levels.

**Why it matters**

- **Performance**: Tune memory, I/O, and query execution parameters
- **Security**: Configure authentication, encryption, and access controls
- **Reliability**: Set backup, recovery, and high availability options
- **Resource Management**: Control connection limits and resource usage
- **Monitoring**: Configure logging, metrics, and audit trails

**How to use it**

Edit the main configuration file (`scratchbird.conf`), set environment variables for containerized deployments, or use SET commands for session-specific settings. Changes to most settings require a server restart, while some can be modified at runtime.

## Configuration File

### Location and Format

```bash
# Default locations (checked in order)
/etc/scratchbird/scratchbird.conf
/usr/local/etc/scratchbird/scratchbird.conf
$HOME/.scratchbird/scratchbird.conf
./scratchbird.conf

# Format: KEY=value
# Comments start with #
# Environment variable substitution: ${VAR_NAME}
```

### Basic Configuration

```ini
# scratchbird.conf

# Network Settings
BIND_ADDRESS=0.0.0.0
PORT=5439
MAX_CONNECTIONS=100

# Logging
LOG_LEVEL=info  # trace, debug, info, warn, error
LOG_FILE=/var/log/scratchbird/server.log
LOG_ROTATION=daily
LOG_RETENTION_DAYS=30

# Data Storage
DATA_DIR=/var/lib/scratchbird/data
TEMP_DIR=/var/lib/scratchbird/temp

# Security
REQUIRE_SSL=false
SSL_CERT_FILE=/etc/scratchbird/server.crt
SSL_KEY_FILE=/etc/scratchbird/server.key
```

## Core Settings

### Server Settings

```ini
# Network Configuration
BIND_ADDRESS=0.0.0.0          # Listen address
PORT=5439                      # Listen port
UNIX_SOCKET=/tmp/.s.PGSQL.5439  # Unix domain socket
MAX_CONNECTIONS=100            # Maximum concurrent connections
SUPERUSER_RESERVED_CONNECTIONS=3  # Reserved for superusers

# Worker Processes
WORKER_THREADS=auto           # Number of worker threads (auto = CPU cores)
IO_THREADS=4                  # I/O thread pool size
BACKGROUND_WORKERS=2          # Background maintenance workers

# Timeouts
CONNECTION_TIMEOUT=60         # Connection establishment timeout (seconds)
STATEMENT_TIMEOUT=0           # Query execution timeout (0 = unlimited)
IDLE_SESSION_TIMEOUT=0        # Idle session timeout (0 = unlimited)
LOCK_TIMEOUT=0               # Lock acquisition timeout (0 = unlimited)
```

### Memory Settings

```ini
# Memory Management
SHARED_BUFFERS=256MB          # Shared memory for caching
WORK_MEM=4MB                  # Memory per sort/hash operation
MAINTENANCE_WORK_MEM=64MB     # Memory for maintenance operations
TEMP_BUFFERS=8MB              # Temporary buffer cache per session

# Cache Sizes
EFFECTIVE_CACHE_SIZE=4GB      # OS cache size estimate for planner
QUERY_CACHE_SIZE=32MB         # Query result cache
METADATA_CACHE_SIZE=16MB      # Schema metadata cache

# Memory Limits
MAX_MEMORY_PER_QUERY=1GB      # Maximum memory per query
MAX_MEMORY_PER_SESSION=2GB    # Maximum memory per session
```

### Storage Settings

```ini
# Data Directories
DATA_DIR=/var/lib/scratchbird/data
TEMP_DIR=/var/lib/scratchbird/temp
BACKUP_DIR=/var/lib/scratchbird/backup

# File Management
PAGE_SIZE=4096                # Database page size (bytes)
SEGMENT_SIZE=1GB              # Maximum file segment size
WAL_SEGMENT_SIZE=16MB         # Write-ahead log segment size

# Tablespaces
DEFAULT_TABLESPACE=pg_default
TEMP_TABLESPACES=pg_temp

# Compression
ENABLE_COMPRESSION=true       # Enable data compression
COMPRESSION_LEVEL=6          # Compression level (1-9)
```

## Performance Tuning

### Query Planner

```ini
# Planner Cost Constants
SEQ_PAGE_COST=1.0            # Sequential page read cost
RANDOM_PAGE_COST=4.0         # Random page read cost
CPU_TUPLE_COST=0.01          # CPU cost per tuple
CPU_INDEX_TUPLE_COST=0.005   # CPU cost per index tuple
CPU_OPERATOR_COST=0.0025     # CPU cost per operator

# Planner Method Configuration
ENABLE_SEQSCAN=on            # Enable sequential scans
ENABLE_INDEXSCAN=on          # Enable index scans
ENABLE_INDEXONLYSCAN=on      # Enable index-only scans
ENABLE_BITMAPSCAN=on         # Enable bitmap scans
ENABLE_TIDSCAN=on            # Enable TID scans
ENABLE_SORT=on               # Enable explicit sorts
ENABLE_HASHAGG=on            # Enable hash aggregation
ENABLE_HASHJOIN=on           # Enable hash joins
ENABLE_MERGEJOIN=on          # Enable merge joins
ENABLE_NESTLOOP=on           # Enable nested loop joins

# Planner Behavior
DEFAULT_STATISTICS_TARGET=100  # Default statistics sample size
JOIN_COLLAPSE_LIMIT=8          # Maximum joins to flatten
FROM_COLLAPSE_LIMIT=8          # Maximum FROM items to flatten
```

### Parallel Execution

```ini
# Parallel Query
MAX_PARALLEL_WORKERS=8         # Maximum parallel workers system-wide
MAX_PARALLEL_WORKERS_PER_GATHER=4  # Per-query parallel workers
PARALLEL_SETUP_COST=1000      # Cost to start parallel workers
PARALLEL_TUPLE_COST=0.1        # Cost per tuple in parallel mode
MIN_PARALLEL_TABLE_SCAN_SIZE=8MB  # Minimum size for parallel scan
MIN_PARALLEL_INDEX_SCAN_SIZE=512KB  # Minimum size for parallel index scan
```

### Write Performance

```ini
# Write-Ahead Logging
WAL_LEVEL=replica              # minimal, replica, or logical
WAL_BUFFERS=16MB              # WAL buffer size
WAL_WRITER_DELAY=200ms        # WAL writer sleep time
WAL_WRITER_FLUSH_AFTER=1MB   # Force flush after this much WAL
CHECKPOINT_TIMEOUT=5min       # Maximum time between checkpoints
CHECKPOINT_COMPLETION_TARGET=0.5  # Checkpoint spread factor
MAX_WAL_SIZE=1GB              # Maximum WAL size before checkpoint
MIN_WAL_SIZE=80MB             # Minimum WAL size to retain

# Commit Behavior
SYNCHRONOUS_COMMIT=on         # Wait for WAL flush on commit
COMMIT_DELAY=0                # Delay before commit (microseconds)
COMMIT_SIBLINGS=5             # Concurrent commits to wait for
```

## Security Configuration

### Authentication

```ini
# Authentication Methods
AUTH_METHOD=md5               # trust, md5, scram-sha-256, cert
PASSWORD_ENCRYPTION=scram-sha-256  # Password storage encryption
SSL_MODE=prefer               # disable, allow, prefer, require, verify-ca, verify-full

# SSL/TLS Configuration
SSL_CERT_FILE=/etc/scratchbird/server.crt
SSL_KEY_FILE=/etc/scratchbird/server.key
SSL_CA_FILE=/etc/scratchbird/ca.crt
SSL_CRL_FILE=/etc/scratchbird/crl.pem
SSL_CIPHERS=HIGH:MEDIUM:+3DES:!aNULL
SSL_PREFER_SERVER_CIPHERS=on
SSL_MIN_PROTOCOL_VERSION=TLSv1.2
```

### Access Control

```ini
# Connection Security
LISTEN_ADDRESSES=localhost    # Comma-separated list
UNIX_SOCKET_PERMISSIONS=0770  # Unix socket file permissions
UNIX_SOCKET_GROUP=scratchbird # Unix socket owning group

# Host-Based Authentication (hba.conf equivalent)
HBA_FILE=/etc/scratchbird/hba.conf

# Row-Level Security
ROW_SECURITY=on               # Enable row-level security
```

## Logging and Monitoring

### Log Configuration

```ini
# Log Destinations
LOG_DESTINATION=stderr        # stderr, syslog, eventlog, csvlog
LOGGING_COLLECTOR=on          # Enable logging collector
LOG_DIRECTORY=/var/log/scratchbird
LOG_FILENAME=scratchbird-%Y-%m-%d.log
LOG_FILE_MODE=0600           # Log file permissions
LOG_ROTATION_AGE=1d          # Rotate logs daily
LOG_ROTATION_SIZE=100MB      # Rotate when size exceeds
LOG_TRUNCATE_ON_ROTATION=off

# What to Log
LOG_CONNECTIONS=on           # Log successful connections
LOG_DISCONNECTIONS=on        # Log session terminations
LOG_DURATION=off             # Log statement durations
LOG_ERROR_VERBOSITY=default  # terse, default, verbose
LOG_HOSTNAME=off             # Include hostname in logs
LOG_LINE_PREFIX='%t [%p]: [%l-1] user=%u,db=%d,app=%a '
LOG_LOCK_WAITS=off          # Log long lock waits
LOG_STATEMENT=none           # none, ddl, mod, all
LOG_TEMP_FILES=0            # Log temp files >= size
LOG_TIMEZONE=UTC

# Log Levels
CLIENT_MIN_MESSAGES=notice   # Messages sent to client
LOG_MIN_MESSAGES=warning     # Messages written to log
LOG_MIN_ERROR_STATEMENT=error  # Log statements causing errors
LOG_MIN_DURATION_STATEMENT=-1  # Log slow statements (ms)
```

### Statistics

```ini
# Statistics Collection
TRACK_ACTIVITIES=on          # Track command execution
TRACK_ACTIVITY_QUERY_SIZE=1024  # Query string size in pg_stat_activity
TRACK_COUNTS=on              # Track table/index access counts
TRACK_IO_TIMING=off          # Track I/O timing (performance impact)
TRACK_FUNCTIONS=none         # none, pl, all
UPDATE_PROCESS_TITLE=on      # Update process title with activity

# Statistics Sampling
STATS_TEMP_DIRECTORY=/var/run/scratchbird
LOG_STATEMENT_STATS=off      # Log statement statistics
LOG_PARSER_STATS=off         # Log parser statistics
LOG_PLANNER_STATS=off        # Log planner statistics
LOG_EXECUTOR_STATS=off       # Log executor statistics
```

## Runtime Configuration

### SET Commands

```sql
-- Session-level settings
SET work_mem = '8MB';
SET statement_timeout = '30s';
SET search_path = 'myschema, public';
SET time_zone = 'America/New_York';

-- Transaction-level settings
SET LOCAL synchronous_commit = off;
SET LOCAL work_mem = '16MB';

-- Show current setting
SHOW work_mem;
SHOW ALL;  -- Show all settings

-- Reset to default
RESET work_mem;
RESET ALL;
```

### Configuration Functions

```sql
-- Get configuration value
SELECT current_setting('work_mem');
SELECT current_setting('custom.variable', true);  -- Missing OK

-- Set configuration value
SELECT set_config('work_mem', '16MB', false);  -- Session
SELECT set_config('work_mem', '16MB', true);   -- Transaction

-- Reload configuration file
SELECT pg_reload_conf();

-- Check configuration file
SELECT * FROM pg_file_settings WHERE error IS NOT NULL;
```

## Environment Variables

```bash
# Override configuration file settings
export SCRATCHBIRD_PORT=5440
export SCRATCHBIRD_DATA_DIR=/data/scratchbird
export SCRATCHBIRD_LOG_LEVEL=debug

# Connection environment variables
export PGHOST=localhost
export PGPORT=5439
export PGDATABASE=mydb
export PGUSER=myuser
export PGPASSWORD=mypass

# Start server with environment overrides
scratchbird-server
```

## Configuration Best Practices

### Development Environment

```ini
# Development settings (scratchbird-dev.conf)
LOG_LEVEL=debug
LOG_STATEMENT=all
LOG_DURATION=on
LOG_MIN_DURATION_STATEMENT=0
SHARED_BUFFERS=128MB
MAX_CONNECTIONS=20
```

### Production Environment

```ini
# Production settings (scratchbird-prod.conf)
LOG_LEVEL=warning
LOG_STATEMENT=ddl
LOG_MIN_DURATION_STATEMENT=1000  # Log queries > 1 second
SHARED_BUFFERS=4GB
MAX_CONNECTIONS=200
EFFECTIVE_CACHE_SIZE=12GB
CHECKPOINT_SEGMENTS=32
CHECKPOINT_COMPLETION_TARGET=0.9
```

### High-Performance Configuration

```ini
# Performance-optimized settings
SHARED_BUFFERS=8GB            # 25% of RAM
EFFECTIVE_CACHE_SIZE=24GB     # 75% of RAM
WORK_MEM=32MB
MAINTENANCE_WORK_MEM=2GB
RANDOM_PAGE_COST=1.1          # For SSD storage
WAL_BUFFERS=64MB
CHECKPOINT_SEGMENTS=64
CHECKPOINT_COMPLETION_TARGET=0.9
MAX_PARALLEL_WORKERS=16
MAX_PARALLEL_WORKERS_PER_GATHER=8
```

## Monitoring Configuration Changes

```sql
-- View current configuration
SELECT name, setting, unit, category, short_desc
FROM pg_settings
WHERE source != 'default'
ORDER BY category, name;

-- Track configuration changes
CREATE TABLE config_history (
    changed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    setting_name TEXT,
    old_value TEXT,
    new_value TEXT,
    changed_by TEXT DEFAULT CURRENT_USER
);

-- Audit configuration changes
CREATE TRIGGER config_audit
AFTER UPDATE ON pg_settings
FOR EACH ROW
EXECUTE FUNCTION log_config_change();
```

## Implementation Details

**Configuration Parser** (`src/engine/config.cpp`):
- Parses configuration files
- Handles environment variable substitution
- Validates setting values

**Configuration Manager** (`include/scratchbird/engine/config.h`):
- Stores configuration values
- Manages setting precedence
- Handles runtime updates

**Default Configuration** (`packaging/config/scratchbird.conf`):
- Template configuration file
- Documented settings
- Example values

**Code Anchors**:
- Config parser: `src/engine/config.cpp`
- Config header: `include/scratchbird/engine/config.h`
- Default config: `packaging/config/scratchbird.conf`
- Systemd service: `packaging/systemd/scratchbird.service`

## See also

- [Installation](./installation.md) - Initial configuration setup
- [CLI Tools](./cli-tools.md) - Configuration management tools
- [Session & Transaction](./session-and-transaction.md) - Runtime SET commands
- [Performance](./explain-analyze.md) - Performance tuning
- [Security](./ddl-roles-users-grants.md) - Security configuration