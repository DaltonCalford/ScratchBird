# sb_admin

Server administration utility.

[Back to Tools Index](index.md) | [Back to Documentation Index](../index.md)

---

## Synopsis

```
sb_admin <command> [OPTIONS] [ARGS]
```

---

## Description

`sb_admin` provides administrative commands for managing the ScratchBird server, including server control, session management, and monitoring.

---

## Commands

### Server Control

| Command | Description |
|---------|-------------|
| `shutdown` | Graceful server shutdown |
| `restart` | Restart server |
| `reload` | Reload configuration |
| `status` | Show server status |

### Session Management

| Command | Description |
|---------|-------------|
| `show connections` | List active connections |
| `show sessions` | Show session details |
| `kill <pid>` | Terminate session |
| `kill-all` | Terminate all sessions |

### Monitoring

| Command | Description |
|---------|-------------|
| `show stats` | Server statistics |
| `show pool` | Connection pool status |
| `show locks` | Active locks |
| `show queries` | Running queries |

---

## Connection Options

| Option | Description |
|--------|-------------|
| `-H, --host HOST` | Server hostname |
| `-P, --port PORT` | Server port |
| `-U, --user USER` | Admin username |
| `-p, --password` | Prompt for password |

---

## Server Control

### Shutdown

```bash
# Graceful shutdown (wait for queries)
sb_admin shutdown

# Immediate shutdown
sb_admin shutdown --immediate

# With timeout
sb_admin shutdown --timeout 60
```

### Restart

```bash
# Graceful restart
sb_admin restart

# Immediate restart
sb_admin restart --immediate
```

### Reload Configuration

```bash
# Reload config without restart
sb_admin reload
```

Output:
```
Configuration reloaded successfully.
Changes applied:
  - max_connections: 100 -> 200
  - log_level: info -> debug
```

### Server Status

```bash
sb_admin status
```

Output:
```
ScratchBird Server Status
  Version: 0.9.0
  Uptime: 5 days, 3:24:15
  PID: 12345
  Mode: multi-database

Connections:
  Active: 45
  Idle: 12
  Maximum: 200

Memory:
  Shared buffers: 4 GB (78% used)
  Work memory: 256 MB

Databases:
  mydb: 3.2 GB, 25 connections
  analytics: 15 GB, 8 connections
  test: 45 MB, 2 connections
```

---

## Session Management

### Show Connections

```bash
sb_admin show connections
```

Output:
```
PID    User      Database   Client IP        State    Duration
-----  --------  ---------  ---------------  -------  ---------
1001   admin     mydb       192.168.1.100    active   00:05:23
1002   appuser   mydb       192.168.1.101    idle     00:12:45
1003   analyst   analytics  10.0.0.50        active   01:23:45
```

### Show Sessions (Detailed)

```bash
sb_admin show sessions
```

Output:
```
Session 1001:
  User: admin
  Database: mydb
  Client: 192.168.1.100:54321
  Connected: 2024-01-15 10:30:00
  State: active
  Query: SELECT * FROM orders WHERE...
  Query start: 2024-01-15 10:35:23
  Transactions: 145

Session 1002:
  User: appuser
  ...
```

### Kill Session

```bash
# Kill single session
sb_admin kill 1001

# Kill with message
sb_admin kill 1001 --message "Maintenance required"

# Force kill (SIGKILL)
sb_admin kill 1001 --force
```

### Kill All Sessions

```bash
# Kill all (except admin)
sb_admin kill-all

# Kill all for specific database
sb_admin kill-all --database mydb

# Kill all for specific user
sb_admin kill-all --user appuser
```

---

## Monitoring Commands

### Server Statistics

```bash
sb_admin show stats
```

Output:
```
Server Statistics (since startup):
  Connections total: 15,234
  Connections active: 45
  Queries executed: 1,234,567
  Transactions committed: 234,567
  Transactions rolled back: 1,234

Cache Statistics:
  Buffer hits: 98.5%
  Index hits: 99.2%

I/O Statistics:
  Blocks read: 12,345
  Blocks written: 5,678
```

### Connection Pool

```bash
sb_admin show pool
```

Output:
```
Connection Pool Status:
  Pool size: 100
  Active: 45
  Idle: 55
  Waiting: 0

Per-database:
  mydb: 25 active, 15 idle
  analytics: 8 active, 12 idle
```

### Active Locks

```bash
sb_admin show locks
```

Output:
```
Active Locks:
  PID   Database  Table      Lock Type  Granted  Waiting
  ----  --------  ---------  ---------  -------  -------
  1001  mydb      orders     RowExcl    Yes      -
  1003  mydb      orders     RowShare   Yes      -
  1005  mydb      orders     RowExcl    No       1001

Blocked queries: 1
```

### Running Queries

```bash
sb_admin show queries
```

Output:
```
Running Queries:
  PID   User     Duration  Query
  ----  -------  --------  ----------------------------------------
  1001  admin    00:05:23  SELECT * FROM orders WHERE customer_id...
  1003  analyst  01:23:45  SELECT COUNT(*) FROM fact_sales GROUP...

Long-running (> 1 min): 1
```

---

## Maintenance Commands

### Vacuum Database

```bash
sb_admin vacuum mydb

# Full vacuum (locks table)
sb_admin vacuum mydb --full

# Analyze after vacuum
sb_admin vacuum mydb --analyze
```

### Analyze Statistics

```bash
sb_admin analyze mydb

# Specific table
sb_admin analyze mydb --table orders
```

### Checkpoint

```bash
sb_admin checkpoint
```

---

## Automation

### Health Check Script

```bash
#!/bin/bash
# Health check for monitoring system

STATUS=$(sb_admin status --format json)
CONNECTIONS=$(echo $STATUS | jq '.connections.active')
MAX=$(echo $STATUS | jq '.connections.maximum')

if [ $CONNECTIONS -gt $((MAX * 90 / 100)) ]; then
    echo "WARNING: Connection usage at ${CONNECTIONS}/${MAX}"
    exit 1
fi

echo "OK: ${CONNECTIONS}/${MAX} connections"
exit 0
```

### Kill Long Queries

```bash
#!/bin/bash
# Kill queries running > 1 hour

sb_admin show queries --format csv | \
    awk -F, '$3 > 3600 {print $1}' | \
    while read pid; do
        sb_admin kill $pid --message "Query exceeded time limit"
    done
```

---

## Output Formats

```bash
# Human-readable (default)
sb_admin show stats

# JSON
sb_admin show stats --format json

# CSV
sb_admin show connections --format csv
```

---

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | General error |
| 2 | Connection error |
| 3 | Authentication error |
| 4 | Permission denied |
| 5 | Session not found |

---

## Required Permissions

| Command | Required Role |
|---------|---------------|
| `show *` | Any authenticated user |
| `kill` | Superuser or session owner |
| `kill-all` | Superuser |
| `shutdown` | Superuser |
| `restart` | Superuser |
| `reload` | Superuser |

---

## Troubleshooting

### "Permission denied"

```bash
# Must be superuser for admin commands
sb_admin -U admin shutdown
```

### "Session not found"

```bash
# Session may have ended, refresh list
sb_admin show connections
```

### "Cannot kill session"

```bash
# Use force for stuck sessions
sb_admin kill 1001 --force
```

---

## See Also

- [Monitoring](../admin/monitoring.md)
- [Troubleshooting](../admin/troubleshooting.md)
- [sb_server](sb-server.md)
