# sb_admin

Server administration utility for scheduler and metrics queries.

**Status:** Available (built as `sb_admin`).

[Back to CLI Tools](README.md) | [Back to Home](../Home.md)

---

## Synopsis

```
sb_admin <database> job list [--like <pattern>] [OPTIONS]
sb_admin <database> job runs <job_name> [OPTIONS]
sb_admin <database> metrics [OPTIONS]
```

---

## Description

`sb_admin` connects to the local TCP listener and issues scheduler/metrics queries using admin credentials.

---

## Commands

### Scheduler

| Command | Description |
|---------|-------------|
| `job list` | List jobs (optional LIKE filter) |
| `job runs <job_name>` | Show runs for a named job |

### Metrics

| Command | Description |
|---------|-------------|
| `metrics` | Emit `SHOW METRICS` output |

---

## Connection Options

| Option | Description |
|--------|-------------|
| `-U, --user USER` | Admin username |
| `-P, --password PASS` | Admin password |
| `-p, --port PORT` | TCP port (default: 3092) |
| `--database NAME` | Database name (if not supplied positionally) |
| `-q, --quiet` | Only show errors |

---

## Scheduler Examples

```bash
# List jobs
sb_admin mydb job list -U SYSARCH -P changeme

# Filter jobs by name
sb_admin mydb job list --like "daily%" -U SYSARCH -P changeme

# Show runs for a job
sb_admin mydb job runs daily_sweep -U SYSARCH -P changeme
```

## Metrics Example

```bash
sb_admin mydb metrics -U SYSARCH -P changeme
```

Output:
```
scratchbird_scheduler_job_run_latency_seconds 0.35
scratchbird_scheduler_jobs_pending 2
```
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
