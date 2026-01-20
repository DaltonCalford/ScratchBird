# sb_admin CLI Specification

## 1. Overview

`sb_admin` is the administrative command-line tool for ScratchBird database management, providing:
- Server and cluster management
- Database administration
- Diagnostics and health checking
- Backup and restore operations
- Monitoring integration (Nagios, Prometheus)

**Binary:** `sb_admin`
**Version:** 1.0.0

---

## 2. Global Options

```
sb_admin [global-options] <command> [command-options]

Global Options:
  -h, --host <host>       Server hostname (default: localhost)
  -p, --port <port>       Server port (default: 3092)
  -U, --user <user>       Username (default: $SBUSER or current user)
  -W, --password          Prompt for password
  -d, --database <db>     Database name (default: $SBDATABASE)
  -c, --config <file>     Configuration file path
  -v, --verbose           Verbose output
  -q, --quiet             Suppress non-error output
  --json                  Output in JSON format
  --no-color              Disable colored output
  --timeout <seconds>     Command timeout (default: 30)
  --version               Show version and exit
  --help                  Show help and exit
```

---

## 3. Command Reference

### 3.1 Server Commands

```
sb_admin server <subcommand>

Subcommands:
  status          Show server status
  start           Start server (requires root/systemd)
  stop            Stop server gracefully
  restart         Restart server
  reload          Reload configuration (SIGHUP)
  info            Show server information
  config          Show/set configuration
  connections     List active connections
  kill <pid>      Terminate a connection
  terminate <pid> Force-terminate a connection
```

#### Examples

```bash
# Check server status
sb_admin server status
# Output:
# Server: running (pid 12345)
# Uptime: 7 days, 3 hours
# Connections: 45/200 (22.5%)
# Memory: 2.1 GB / 4 GB
# Databases: 3

# Show detailed server info
sb_admin server info --json

# List connections
sb_admin server connections
# Output:
# PID    | User     | Database | State   | Duration | Query
# 12346  | admin    | mydb     | idle    | 00:05:23 |
# 12347  | app_user | mydb     | active  | 00:00:01 | SELECT * FROM...

# Kill idle connections
sb_admin server kill --idle-time 3600

# Reload configuration
sb_admin server reload
```

### 3.2 Database Commands

```
sb_admin database <subcommand>

Subcommands:
  list            List all databases
  create <name>   Create new database
  drop <name>     Drop database
  info <name>     Show database information
  size <name>     Show database size
  sweep <name>    Run sweep/GC (VACUUM alias)
  analyze <name>  Update statistics
  check <name>    Check database integrity
```

#### Examples

```bash
# List databases
sb_admin database list
# Output:
# Name       | Owner  | Size     | Connections | Created
# mydb       | admin  | 1.2 GB   | 12          | 2024-01-15
# analytics  | dba    | 45.6 GB  | 3           | 2024-02-20

# Create database
sb_admin database create newdb --owner admin --encoding UTF8

# Check integrity
sb_admin database check mydb --full
# Output:
# Checking mydb...
# Tables: 45/45 OK
# Indexes: 123/123 OK
# Constraints: 67/67 OK
# Result: PASSED
```

### 3.3 Cluster Commands

```
sb_admin cluster <subcommand>

Subcommands:
  status          Show cluster status
  init            Initialize new cluster
  join <node>     Join existing cluster
  leave           Leave cluster gracefully
  nodes           List cluster nodes
  promote         Promote standby to primary
  demote          Demote primary to standby
  failover        Initiate failover
  rebalance       Rebalance shards
  sync-status     Show replication status
```

#### Examples

```bash
# Show cluster status
sb_admin cluster status
# Output:
# Cluster: production (3 nodes)
# State: healthy
# Primary: node1.example.com:3092
#
# Node              | Role     | State   | Lag    | Shards
# node1.example.com | primary  | online  | -      | 4
# node2.example.com | standby  | online  | 0.2s   | 4
# node3.example.com | standby  | online  | 0.5s   | 4

# Join cluster
sb_admin cluster join node1.example.com:3092 --role standby

# Initiate failover
sb_admin cluster failover --target node2.example.com

# Check replication lag
sb_admin cluster sync-status
```

### 3.4 User Commands

```
sb_admin user <subcommand>

Subcommands:
  list            List users
  create <name>   Create user
  drop <name>     Drop user
  alter <name>    Modify user
  password <name> Change password
  roles <name>    Show user roles
  grant           Grant privileges
  revoke          Revoke privileges
```

#### Examples

```bash
# Create user
sb_admin user create appuser --password --role readonly

# Change password
sb_admin user password appuser

# Grant role
sb_admin user grant appuser --role admin --database mydb
```

### 3.5 Backup Commands

```
sb_admin backup <subcommand>

Subcommands:
  create          Create backup
  list            List backups
  info <id>       Show backup details
  restore         Restore from backup
  verify <id>     Verify backup integrity
  delete <id>     Delete backup
  schedule        Manage backup schedules
  export          Export to external format
```

#### Options for `backup create`

```
sb_admin backup create [options]

Options:
  --database <name>       Database to backup (default: all)
  --type <type>           full, incremental, differential
  --compress              Enable compression (default: true)
  --compress-level <1-9>  Compression level (default: 6)
  --parallel <n>          Parallel workers (default: CPU count)
  --target <path>         Backup destination
  --label <label>         Backup label/description
  --checkpoint            Force checkpoint before backup
  --exclude-tables <list> Tables to exclude
  --manifest              Generate manifest file
```

#### Examples

```bash
# Full backup
sb_admin backup create --database mydb --target /backups/

# Incremental backup
sb_admin backup create --type incremental --target /backups/

# List backups
sb_admin backup list
# Output:
# ID       | Type        | Database | Size    | Date                | Status
# bk-001   | full        | mydb     | 1.2 GB  | 2024-03-01 02:00:00 | valid
# bk-002   | incremental | mydb     | 45 MB   | 2024-03-02 02:00:00 | valid

# Restore
sb_admin backup restore bk-001 --target /data/restored/

# Verify backup
sb_admin backup verify bk-001
```

### 3.6 Restore Commands

```
sb_admin restore <subcommand>

Subcommands:
  full            Full restore from backup
  pitr            Point-in-time recovery
  table           Restore specific tables
  status          Show restore progress
  cancel          Cancel running restore
```

#### Examples

```bash
# Point-in-time recovery
sb_admin restore pitr --backup bk-001 --target-time "2024-03-01 15:30:00"

# Restore specific tables
sb_admin restore table users,orders --backup bk-001 --database mydb
```

---

## 4. Diagnostics Commands

### 4.1 Health Check

```
sb_admin health [options]

Options:
  --full              Full health check
  --quick             Quick health check (default)
  --component <name>  Check specific component
  --json              JSON output for automation
```

#### Output

```bash
sb_admin health --full
# Output:
# Component          | Status | Details
# Server             | OK     | Running, pid 12345
# Connections        | OK     | 45/200 (22.5%)
# Memory             | OK     | 2.1 GB / 4 GB (52%)
# Disk               | WARN   | 85% used
# Replication        | OK     | Lag < 1s
# Locks              | OK     | No deadlocks
# Vacuum             | OK     | Last: 2 hours ago
# Checkpoints        | OK     | Regular, no backlog
# Write-after log    | N/A    | Optional (post-gold)
#
# Overall: WARNING (1 issue)
```

### 4.2 Performance Diagnostics

```
sb_admin diag <subcommand>

Subcommands:
  slow-queries      Show slow queries
  locks             Show lock information
  bloat             Check table/index bloat
  cache             Show cache statistics
  io                Show I/O statistics
  wait-events       Show wait event statistics
  activity          Show current activity
  explain <query>   Analyze query plan
```

#### Examples

```bash
# Show slow queries
sb_admin diag slow-queries --threshold 1000 --limit 10
# Output:
# Duration | Calls | Query
# 5.2s     | 15    | SELECT * FROM orders WHERE...
# 3.1s     | 8     | UPDATE inventory SET...

# Check for bloat
sb_admin diag bloat --threshold 20
# Output:
# Table/Index      | Size    | Bloat % | Wasted
# orders           | 1.2 GB  | 35%     | 420 MB
# idx_orders_date  | 200 MB  | 25%     | 50 MB

# Show locks
sb_admin diag locks
# Output:
# PID   | Mode          | Granted | Waiting | Object
# 12346 | AccessShare   | Yes     | -       | users
# 12347 | RowExclusive  | Yes     | -       | orders
# 12348 | AccessShare   | No      | 12347   | orders
```

### 4.3 Log Analysis

```
sb_admin logs <subcommand>

Subcommands:
  tail            Tail server logs
  search          Search logs
  errors          Show recent errors
  stats           Log statistics
```

#### Examples

```bash
# Tail logs
sb_admin logs tail --follow --filter ERROR

# Search logs
sb_admin logs search "connection refused" --since "1 hour ago"

# Error summary
sb_admin logs errors --last 24h
```

---

## 5. Monitoring Integration

### 5.1 Nagios/NRPE Commands

```
sb_admin check <check-name> [options]

Check Names:
  connection        Check server connectivity
  replication       Check replication status
  connections       Check connection count
  disk              Check disk usage
  memory            Check memory usage
  locks             Check for lock issues
  sweep             Check sweep/GC status
  backup            Check backup status
  queries           Check long-running queries
  custom <query>    Custom SQL check
```

#### Nagios Exit Codes

| Code | Status | Description |
|------|--------|-------------|
| 0 | OK | Check passed |
| 1 | WARNING | Warning threshold exceeded |
| 2 | CRITICAL | Critical threshold exceeded |
| 3 | UNKNOWN | Check failed to execute |

#### Examples

```bash
# Check connections (Nagios format)
sb_admin check connections --warning 80 --critical 95
# Output:
# OK - Connections: 45/200 (22.5%) | connections=45;160;190;0;200

# Check replication lag
sb_admin check replication --warning 5 --critical 30
# Output:
# OK - Replication lag: 0.5s | lag_seconds=0.5;5;30;0;

# Check disk usage
sb_admin check disk --warning 80 --critical 90
# Output:
# WARNING - Disk usage: 85% | disk_percent=85;80;90;0;100

# Custom SQL check
sb_admin check custom "SELECT COUNT(*) FROM failed_jobs WHERE created_at > NOW() - INTERVAL '1 hour'" \
  --warning 10 --critical 100 --label "Failed Jobs"
```

### 5.2 Prometheus Metrics

```
sb_admin metrics [options]

Options:
  --format <fmt>    prometheus, json, text
  --port <port>     Start metrics server on port
  --once            Output metrics once and exit
```

#### Example Output

```bash
sb_admin metrics --format prometheus --once
# Output:
# # HELP scratchbird_connections_total Total connections
# # TYPE scratchbird_connections_total gauge
# scratchbird_connections_total{state="active"} 12
# scratchbird_connections_total{state="idle"} 33
#
# # HELP scratchbird_queries_total Total queries executed
# # TYPE scratchbird_queries_total counter
# scratchbird_queries_total{type="select"} 1234567
# scratchbird_queries_total{type="insert"} 456789
#
# # HELP scratchbird_replication_lag_seconds Replication lag
# # TYPE scratchbird_replication_lag_seconds gauge
# scratchbird_replication_lag_seconds{node="standby1"} 0.5
```

### 5.3 SNMP Support

```
sb_admin snmp <subcommand>

Subcommands:
  agent           Start SNMP sub-agent
  walk            Walk MIB tree
  get <oid>       Get specific OID
```

---

## 6. Maintenance Commands

### 6.1 Sweep/GC (VACUUM alias)

```
sb_admin sweep [options]   # VACUUM alias supported

Options:
  --database <name>   Target database
  --table <name>      Target table
  --full              Not supported (no VACUUM FULL rewrite)
  --analyze           Update statistics after sweep/GC
  --parallel <n>      Parallel workers
  --verbose           Verbose output
```

### 6.2 Sweep Status (planned)

```
sb_admin sweep status [options]

Options:
  --database <name>   Target database (default: current)
  --json              Emit JSON
  --verbose           Include OIT/OAT/OST markers
```

**Notes**:
- Read-only; does not perform a sweep.
- Returns MON_SWEEP and MON_DATABASE markers for "sweep pressure" analysis.

### 6.3 Reindex

```
sb_admin reindex [options]

Options:
  --database <name>   Target database
  --table <name>      Target table
  --index <name>      Specific index
  --concurrently      Non-blocking reindex
```

### 6.4 Maintenance Mode

```
sb_admin maintenance <subcommand>

Subcommands:
  enable          Enable maintenance mode
  disable         Disable maintenance mode
  status          Show maintenance status
```

---

## 7. Security Commands

```
sb_admin security <subcommand>

Subcommands:
  audit           Audit log management
  ssl             SSL certificate management
  keys            Encryption key management
  firewall        Connection firewall rules
```

#### Examples

```bash
# Show SSL certificate status
sb_admin security ssl status
# Output:
# Certificate: /etc/scratchbird/server.crt
# Issuer: Let's Encrypt
# Valid: 2024-01-01 to 2024-04-01
# Days remaining: 45
# Status: OK

# Rotate encryption keys
sb_admin security keys rotate --type data-encryption

# List firewall rules
sb_admin security firewall list
```

---

## 8. Configuration File

```ini
# ~/.sb_admin.conf or /etc/scratchbird/sb_admin.conf

[connection]
host = localhost
port = 3092
user = admin
database = postgres
ssl_mode = prefer

[output]
format = text
color = auto
verbose = false

[backup]
default_target = /var/backups/scratchbird
compress = true
compress_level = 6

[monitoring]
nagios_warning = 80
nagios_critical = 95
```

---

## 9. Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | General error |
| 2 | Connection error |
| 3 | Authentication error |
| 4 | Permission denied |
| 5 | Resource not found |
| 6 | Operation timeout |
| 7 | Invalid arguments |

---

## 10. Environment Variables

| Variable | Description |
|----------|-------------|
| `SBHOST` | Default host |
| `SBPORT` | Default port |
| `SBUSER` | Default username |
| `SBPASSWORD` | Password (not recommended) |
| `SBDATABASE` | Default database |
| `SBSSLMODE` | SSL mode |
| `SBCONFIG` | Config file path |
