### CLI Tools

**What it is**

ScratchBird provides command-line tools for database administration, maintenance, monitoring, and troubleshooting. These tools operate directly on database files and can be used for integrity checking, space analysis, backup/restore, performance tuning, and operational tasks without connecting through the SQL interface.

**Why it matters**

- **Maintenance**: Perform offline maintenance and repairs
- **Diagnostics**: Analyze problems when the server is down
- **Monitoring**: Track space usage and performance metrics
- **Recovery**: Restore corrupted databases
- **Automation**: Script administrative tasks

**How to use it**

Run tools from the command line with appropriate privileges. Most tools require read access to database files, while some maintenance tools need write access. Use these tools in maintenance windows or for emergency recovery when the database server cannot start.

## dbcheck - Database Integrity Checker

### Overview

The `dbcheck` utility performs comprehensive integrity validation of database files, detecting corruption, verifying page structures, and validating cross-references.

### Usage

```bash
dbcheck <database_path> [options]

Options:
  --quick           Perform quick corruption check only
  --verbose         Enable detailed progress output
  --no-checksums    Skip page checksum verification
  --no-tuples       Skip tuple-level validation
  --max-issues N    Limit output to N issues (default: 1000)
```

### Examples

```bash
# Basic integrity check
dbcheck /var/lib/scratchbird/data/mydb.db

# Quick corruption scan
dbcheck /data/production.db --quick

# Detailed validation with verbose output
dbcheck /data/production.db --verbose --max-issues 500

# Check without validating checksums (faster)
dbcheck /data/large_db.db --no-checksums
```

### Exit Codes

- `0` - No corruption detected
- `1` - Warnings found (investigation recommended)
- `2` - Corruption detected (repair needed)
- `3` - Critical corruption (database may be unusable)
- `4` - Validation failed (I/O error or tool error)

### Output Interpretation

```bash
$ dbcheck /data/mydb.db --verbose
ScratchBird Database Integrity Check
Database: /data/mydb.db
Mode: Full Validation

Checking segment 0...
  Pages: 65536 total, 45231 used
  ✓ Page headers valid
  ✓ Checksums verified
  ✓ Tuple structures intact
  ✓ Free space maps consistent

Checking segment 1...
  Pages: 32768 total, 28901 used
  ⚠ Warning: High fragmentation detected (>30%)
  ✓ Page structures valid

Summary:
  Total pages checked: 98304
  Corrupted pages: 0
  Warnings: 1
  Recommendation: Run VACUUM to reduce fragmentation
```

### Common Issues and Solutions

```bash
# Issue: Checksum failures
$ dbcheck /data/mydb.db
ERROR: Page checksum failure at page 12345
Solution: Restore from backup or use recovery tools

# Issue: Tuple corruption
$ dbcheck /data/mydb.db
ERROR: Invalid tuple header at page 5678, offset 42
Solution: Export valid data, recreate table

# Issue: High fragmentation
$ dbcheck /data/mydb.db
WARNING: Fragmentation level 45% in segment 2
Solution: Run VACUUM FULL or reorganize tables
```

## dbspace - Space Usage Monitor

### Overview

The `dbspace` utility analyzes database space utilization, providing insights into storage efficiency, fragmentation, and growth patterns.

### Usage

```bash
dbspace <database_path>

# No options - provides comprehensive space analysis
```

### Examples

```bash
# Analyze space usage
dbspace /var/lib/scratchbird/data/production.db

# Check specific database
dbspace ./test_database.db

# Monitor after maintenance
dbspace /data/mydb.db > space_report.txt
```

### Output Format

```bash
$ dbspace /data/mydb.db
ScratchBird Database Space Monitor
===================================

Database: /data/mydb.db
Total Size: 4.2 GB
Segments: 3

Segment Analysis:
┌─────────┬──────────┬──────────┬───────────┬──────────────┐
│ Segment │ Size     │ Used     │ Free      │ Fragmentation│
├─────────┼──────────┼──────────┼───────────┼──────────────┤
│ seg0    │ 1.0 GB   │ 950 MB   │ 74 MB     │ 12%          │
│ seg1    │ 1.0 GB   │ 890 MB   │ 134 MB    │ 18%          │
│ seg2    │ 2.2 GB   │ 2.1 GB   │ 100 MB    │ 25%          │
└─────────┴──────────┴──────────┴───────────┴──────────────┘

Space Pressure: NORMAL
Largest Free Extent: 45 MB (seg1)
Recommendation: No immediate action required

Table Space Usage (Top 10):
1. orders         - 1.2 GB (28.6%)
2. order_items    - 890 MB (20.7%)
3. customers      - 456 MB (10.6%)
4. products       - 234 MB (5.4%)
5. audit_log      - 198 MB (4.6%)
```

### Space Pressure Levels

- **NORMAL**: Adequate free space (>20%)
- **MODERATE**: Monitor space usage (10-20% free)
- **HIGH**: Plan for expansion (5-10% free)
- **CRITICAL**: Immediate action required (<5% free)

### Exit Codes

- `0` - Normal space pressure
- `1` - High space pressure warning
- `2` - Critical space pressure
- `3` - Error analyzing database

## scratchbird-server - Database Server

### Overview

The main database server process that handles client connections and query execution.

### Usage

```bash
scratchbird-server [options]

Options:
  -c, --config FILE    Configuration file path
  -D, --data-dir DIR   Data directory path
  -p, --port PORT      Listen port (default: 5439)
  -h, --host HOST      Bind address (default: localhost)
  -d, --daemon         Run as daemon
  -l, --log-file FILE  Log file path
  -v, --verbose        Verbose output
  --version            Show version information
  --help               Show help message
```

### Examples

```bash
# Start with default configuration
scratchbird-server

# Start with custom config file
scratchbird-server -c /etc/scratchbird/production.conf

# Start on different port
scratchbird-server -p 5440

# Start as daemon with logging
scratchbird-server -d -l /var/log/scratchbird/server.log

# Start with specific data directory
scratchbird-server -D /data/scratchbird
```

### Daemon Management

```bash
# Systemd service management
systemctl start scratchbird
systemctl stop scratchbird
systemctl restart scratchbird
systemctl status scratchbird

# Enable auto-start
systemctl enable scratchbird

# View logs
journalctl -u scratchbird -f
```

## scratchbird-cli - Interactive SQL Client

### Overview

Interactive command-line client for executing SQL queries and managing databases.

### Usage

```bash
scratchbird-cli [options] [database]

Options:
  -h, --host HOST      Server host (default: localhost)
  -p, --port PORT      Server port (default: 5439)
  -U, --user USER      Username
  -d, --database DB    Database name
  -f, --file FILE      Execute commands from file
  -c, --command SQL    Execute single command
  -o, --output FILE    Output file
  -q, --quiet          Quiet mode
  -v, --verbose        Verbose mode
  --no-readline        Disable readline
  --version            Show version
```

### Interactive Commands

```bash
# Connect to database
scratchbird-cli -h localhost -p 5439 -U admin -d mydb

# Interactive prompt
mydb> SELECT * FROM users LIMIT 5;
mydb> \d              -- List tables
mydb> \d users        -- Describe table
mydb> \l              -- List databases
mydb> \c other_db     -- Connect to another database
mydb> \i script.sql   -- Execute script file
mydb> \o output.txt   -- Send output to file
mydb> \q              -- Quit
```

### Batch Mode

```bash
# Execute single command
scratchbird-cli -c "SELECT COUNT(*) FROM orders" mydb

# Execute script file
scratchbird-cli -f migration.sql mydb

# Pipe commands
echo "SELECT * FROM products" | scratchbird-cli mydb

# Export data
scratchbird-cli -c "SELECT * FROM customers" mydb > customers.csv
```

## scratchbird-backup - Backup Utility

### Overview

Creates consistent backups of databases while they're online or offline.

### Usage

```bash
scratchbird-backup [options] database

Options:
  -o, --output FILE    Backup file path
  -c, --compress       Enable compression
  -j, --jobs N         Parallel jobs (default: 1)
  --exclude-table TAB  Exclude specific table
  --schema-only        Backup schema only
  --data-only          Backup data only
  -v, --verbose        Verbose output
```

### Examples

```bash
# Full backup
scratchbird-backup -o /backup/mydb_full.backup mydb

# Compressed backup
scratchbird-backup -c -o /backup/mydb.backup.gz mydb

# Parallel backup
scratchbird-backup -j 4 -o /backup/large_db.backup large_db

# Schema only
scratchbird-backup --schema-only -o schema.sql mydb

# Exclude large tables
scratchbird-backup --exclude-table audit_log -o mydb_no_audit.backup mydb
```

## scratchbird-restore - Restore Utility

### Overview

Restores databases from backup files created by scratchbird-backup.

### Usage

```bash
scratchbird-restore [options] backup_file

Options:
  -d, --database DB    Target database name
  -c, --clean          Drop existing objects first
  -C, --create         Create database
  -j, --jobs N         Parallel jobs
  -t, --table TABLE    Restore specific table only
  -n, --schema SCHEMA  Restore specific schema only
  -v, --verbose        Verbose output
```

### Examples

```bash
# Restore full backup
scratchbird-restore -d mydb_restored /backup/mydb_full.backup

# Create new database and restore
scratchbird-restore -C -d new_db /backup/mydb.backup

# Restore specific table
scratchbird-restore -d mydb -t orders /backup/mydb_full.backup

# Parallel restore
scratchbird-restore -j 4 -d large_db /backup/large_db.backup
```

## scratchbird-vacuum - Maintenance Utility

### Overview

Performs vacuum operations to reclaim space and update statistics.

### Usage

```bash
scratchbird-vacuum [options] [database]

Options:
  -f, --full           VACUUM FULL (locks tables)
  -z, --analyze        Update statistics
  -t, --table TABLE    Vacuum specific table
  -j, --jobs N         Parallel workers
  -v, --verbose        Verbose output
  --min-xid-age N      Minimum transaction age
  --min-mxid-age N     Minimum multixact age
```

### Examples

```bash
# Basic vacuum
scratchbird-vacuum mydb

# Full vacuum with analyze
scratchbird-vacuum -f -z mydb

# Vacuum specific table
scratchbird-vacuum -t orders mydb

# Parallel vacuum
scratchbird-vacuum -j 4 large_db
```

## Utility Scripts

### Health Check Script

```bash
#!/bin/bash
# scratchbird-health-check.sh

DB_PATH="/var/lib/scratchbird/data/production.db"
LOG_FILE="/var/log/scratchbird/health.log"

echo "=== ScratchBird Health Check ===" | tee -a $LOG_FILE
date | tee -a $LOG_FILE

# Check integrity
echo "Checking integrity..." | tee -a $LOG_FILE
if dbcheck $DB_PATH --quick; then
    echo "✓ Integrity check passed" | tee -a $LOG_FILE
else
    echo "✗ Integrity issues detected!" | tee -a $LOG_FILE
    exit 1
fi

# Check space
echo "Checking space..." | tee -a $LOG_FILE
dbspace $DB_PATH | tee -a $LOG_FILE

# Check server status
if systemctl is-active scratchbird > /dev/null; then
    echo "✓ Server is running" | tee -a $LOG_FILE
else
    echo "✗ Server is not running!" | tee -a $LOG_FILE
    exit 1
fi

echo "Health check complete" | tee -a $LOG_FILE
```

### Automated Backup Script

```bash
#!/bin/bash
# scratchbird-auto-backup.sh

BACKUP_DIR="/backup/scratchbird"
DB_NAME="production"
DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_FILE="$BACKUP_DIR/${DB_NAME}_${DATE}.backup.gz"
RETENTION_DAYS=7

# Create backup
echo "Starting backup of $DB_NAME..."
scratchbird-backup -c -o $BACKUP_FILE $DB_NAME

if [ $? -eq 0 ]; then
    echo "Backup completed: $BACKUP_FILE"
    
    # Remove old backups
    find $BACKUP_DIR -name "${DB_NAME}_*.backup.gz" -mtime +$RETENTION_DAYS -delete
    echo "Cleaned up backups older than $RETENTION_DAYS days"
else
    echo "Backup failed!"
    exit 1
fi
```

## Best Practices

### Regular Maintenance

```bash
# Daily tasks
dbcheck /data/mydb.db --quick          # Quick integrity check
scratchbird-vacuum -z mydb             # Update statistics

# Weekly tasks
dbcheck /data/mydb.db                  # Full integrity check
scratchbird-vacuum mydb                # Standard vacuum
dbspace /data/mydb.db > space_report.txt

# Monthly tasks
scratchbird-vacuum -f mydb             # Full vacuum (maintenance window)
scratchbird-backup -c -o monthly.backup mydb
```

### Monitoring Setup

```bash
# Cron jobs for monitoring
# /etc/cron.d/scratchbird-monitor

# Hourly space check
0 * * * * scratchbird dbspace /data/mydb.db > /dev/null 2>&1 || alert-critical

# Daily integrity check
0 2 * * * scratchbird dbcheck /data/mydb.db --quick || alert-warning

# Weekly full check
0 3 * * 0 scratchbird dbcheck /data/mydb.db --verbose > /var/log/weekly-check.log
```

## Implementation Details

**dbcheck** (`src/dbcheck.cpp`):
- Page-level validation
- Checksum verification
- Tuple structure validation
- Cross-reference checking

**dbspace** (`src/dbspace.cpp`):
- Segment analysis
- Fragmentation detection
- Space pressure calculation
- Usage recommendations

**Server** (`src/server.cpp`):
- Connection handling
- Query processing
- Configuration management

**Code Anchors**:
- dbcheck utility: `src/dbcheck.cpp`
- dbspace utility: `src/dbspace.cpp`
- Segment monitor: `include/scratchbird/engine/segment_monitor.h`
- File management: `include/scratchbird/engine/file.h`

## See also

- [Configuration](./configuration.md) - Server configuration
- [Installation](./installation.md) - Tool installation
- [Session & Transaction](./session-and-transaction.md) - SQL commands
- [EXPLAIN ANALYZE](./explain-analyze.md) - Query analysis
- [Backup & Recovery](./missing-and-future.md) - Advanced recovery