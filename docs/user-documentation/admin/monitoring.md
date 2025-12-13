# Monitoring

Monitor ScratchBird health and performance.

[Back to Admin Index](index.md) | [Back to Documentation Index](../index.md)

---

## Monitoring Methods

| Method | Use Case |
|--------|----------|
| **Built-in views** | Ad-hoc queries |
| **Prometheus** | Time-series metrics |
| **Logging** | Debug and audit |
| **System tools** | OS-level monitoring |

---

## Built-in Statistics Views

### Connection Statistics

```sql
-- Active connections
SELECT
    datname,
    usename,
    client_addr,
    state,
    query_start,
    NOW() - query_start AS duration,
    query
FROM pg_stat_activity
WHERE state != 'idle';

-- Connection count by database
SELECT datname, COUNT(*) as connections
FROM pg_stat_activity
GROUP BY datname;

-- Connection count by user
SELECT usename, COUNT(*) as connections
FROM pg_stat_activity
GROUP BY usename;
```

### Database Statistics

```sql
-- Database size and activity
SELECT
    datname,
    pg_size_pretty(pg_database_size(datname)) AS size,
    numbackends AS connections,
    xact_commit AS commits,
    xact_rollback AS rollbacks,
    blks_hit AS cache_hits,
    blks_read AS disk_reads,
    ROUND(blks_hit::numeric / NULLIF(blks_hit + blks_read, 0) * 100, 2) AS cache_ratio
FROM pg_stat_database
WHERE datname NOT LIKE 'template%';
```

### Table Statistics

```sql
-- Table sizes and row counts
SELECT
    schemaname,
    relname AS table_name,
    pg_size_pretty(pg_total_relation_size(schemaname || '.' || relname)) AS total_size,
    n_live_tup AS rows,
    n_dead_tup AS dead_rows,
    last_vacuum,
    last_autovacuum
FROM pg_stat_user_tables
ORDER BY pg_total_relation_size(schemaname || '.' || relname) DESC
LIMIT 20;
```

### Index Statistics

```sql
-- Index usage
SELECT
    schemaname,
    relname AS table_name,
    indexrelname AS index_name,
    idx_scan AS scans,
    idx_tup_read AS tuples_read,
    idx_tup_fetch AS tuples_fetched,
    pg_size_pretty(pg_relation_size(indexrelid)) AS size
FROM pg_stat_user_indexes
ORDER BY idx_scan DESC;

-- Unused indexes
SELECT
    schemaname || '.' || relname AS table,
    indexrelname AS index,
    pg_size_pretty(pg_relation_size(indexrelid)) AS size
FROM pg_stat_user_indexes
WHERE idx_scan = 0
ORDER BY pg_relation_size(indexrelid) DESC;
```

---

## Prometheus Integration

### Enable Prometheus Export

In `sb_server.conf`:

```ini
[statistics]
enabled = true
export = prometheus
prometheus_port = 9090
```

### Available Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `scratchbird_connections_total` | Gauge | Total connections |
| `scratchbird_connections_active` | Gauge | Active connections |
| `scratchbird_queries_total` | Counter | Total queries |
| `scratchbird_query_duration_seconds` | Histogram | Query latency |
| `scratchbird_transactions_committed` | Counter | Committed transactions |
| `scratchbird_transactions_rolled_back` | Counter | Rolled back transactions |
| `scratchbird_cache_hit_ratio` | Gauge | Buffer cache hit ratio |
| `scratchbird_disk_reads_total` | Counter | Disk reads |
| `scratchbird_memory_used_bytes` | Gauge | Memory usage |

### Prometheus Configuration

```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'scratchbird'
    static_configs:
      - targets: ['localhost:9090']
    scrape_interval: 15s
```

### Grafana Dashboard

Import dashboard or create panels:

```json
{
  "panels": [
    {
      "title": "Connections",
      "targets": [
        {"expr": "scratchbird_connections_total"}
      ]
    },
    {
      "title": "Query Rate",
      "targets": [
        {"expr": "rate(scratchbird_queries_total[5m])"}
      ]
    },
    {
      "title": "Cache Hit Ratio",
      "targets": [
        {"expr": "scratchbird_cache_hit_ratio"}
      ]
    }
  ]
}
```

---

## Logging

### Log Levels

| Level | Description |
|-------|-------------|
| `debug` | Verbose debugging |
| `info` | Normal operations |
| `notice` | Notable events |
| `warning` | Potential issues |
| `error` | Errors only |

Configure in `sb_server.conf`:

```ini
[logging]
level = info
destination = file
file = /var/log/scratchbird/sb_server.log
timestamps = true
```

### Slow Query Logging

```ini
[logging]
log_slow_queries = 1000  # milliseconds
```

Review slow queries:

```bash
grep "SLOW QUERY" /var/log/scratchbird/sb_server.log
```

### Connection Logging

```ini
[logging]
log_connections = true
log_disconnections = true
```

### View Logs

```bash
# Real-time log
tail -f /var/log/scratchbird/sb_server.log

# Systemd journal
journalctl -u scratchbird -f

# Filter errors
journalctl -u scratchbird -p err
```

---

## System Monitoring

### CPU Usage

```bash
# Process CPU
top -p $(pgrep sb_server)

# CPU per thread
ps -eLf | grep sb_server
```

### Memory Usage

```bash
# Process memory
ps aux | grep sb_server

# Detailed memory
cat /proc/$(pgrep sb_server)/status | grep -E "Vm|Rss"
```

### Disk I/O

```bash
# I/O statistics
iostat -x 1

# Process I/O
iotop -p $(pgrep sb_server)
```

### Network

```bash
# Connection count
ss -tnp | grep sb_server | wc -l

# Connection states
ss -tn state established | grep :5432
```

---

## Health Checks

### Simple Health Check

```bash
#!/bin/bash
if sb_isql -H localhost -P 3092 -c "SELECT 1" > /dev/null 2>&1; then
    echo "OK"
    exit 0
else
    echo "FAIL"
    exit 1
fi
```

### Detailed Health Check

```bash
#!/bin/bash
# Check connection
if ! sb_isql -H localhost -c "SELECT 1" > /dev/null 2>&1; then
    echo "CRITICAL: Cannot connect"
    exit 2
fi

# Check connection count
CONNS=$(sb_isql -H localhost -tA -c "SELECT COUNT(*) FROM pg_stat_activity")
MAX_CONNS=$(sb_isql -H localhost -tA -c "SHOW max_connections")
RATIO=$((CONNS * 100 / MAX_CONNS))

if [ $RATIO -gt 90 ]; then
    echo "WARNING: Connection usage ${RATIO}%"
    exit 1
fi

# Check disk space
USAGE=$(df /var/lib/scratchbird | tail -1 | awk '{print $5}' | tr -d '%')
if [ $USAGE -gt 90 ]; then
    echo "WARNING: Disk usage ${USAGE}%"
    exit 1
fi

echo "OK: ${CONNS} connections, ${USAGE}% disk"
exit 0
```

### Docker Health Check

```dockerfile
HEALTHCHECK --interval=30s --timeout=10s --retries=3 \
    CMD sb_isql -H localhost -c "SELECT 1" || exit 1
```

---

## Alerting

### Connection Alerts

```sql
-- High connection count
SELECT CASE
    WHEN COUNT(*) > (SELECT setting::int * 0.9 FROM pg_settings WHERE name = 'max_connections')
    THEN 'ALERT: High connection usage'
    ELSE 'OK'
END
FROM pg_stat_activity;
```

### Long-Running Queries

```sql
-- Queries running > 5 minutes
SELECT
    pid,
    usename,
    NOW() - query_start AS duration,
    query
FROM pg_stat_activity
WHERE state = 'active'
  AND NOW() - query_start > INTERVAL '5 minutes';
```

### Lock Monitoring

```sql
-- Blocked queries
SELECT
    blocked.pid AS blocked_pid,
    blocking.pid AS blocking_pid,
    blocked.query AS blocked_query
FROM pg_stat_activity blocked
JOIN pg_stat_activity blocking ON blocking.pid = ANY(pg_blocking_pids(blocked.pid))
WHERE blocked.pid != blocking.pid;
```

---

## Performance Dashboards

### Real-Time Queries

```sql
-- Live query view
SELECT
    pid,
    usename,
    state,
    EXTRACT(EPOCH FROM (NOW() - query_start))::int AS seconds,
    LEFT(query, 80) AS query
FROM pg_stat_activity
WHERE state != 'idle'
ORDER BY query_start;
```

### Cache Performance

```sql
-- Buffer cache hit ratio over time
SELECT
    datname,
    blks_hit,
    blks_read,
    ROUND(
        blks_hit::numeric / NULLIF(blks_hit + blks_read, 0) * 100, 2
    ) AS hit_ratio
FROM pg_stat_database
WHERE datname NOT LIKE 'template%';
```

### Transaction Rate

```sql
-- Transactions per second
SELECT
    datname,
    xact_commit + xact_rollback AS total_xact,
    xact_commit AS commits,
    xact_rollback AS rollbacks
FROM pg_stat_database;
```

---

## Monitoring Best Practices

1. **Set baselines** - Know normal behavior
2. **Alert on anomalies** - Not just thresholds
3. **Monitor trends** - Week-over-week comparison
4. **Log retention** - Keep logs for analysis
5. **Regular review** - Check dashboards daily
6. **Automate responses** - Kill long queries automatically

---

## Next Steps

- [Performance tuning](performance-tuning.md)
- [Troubleshooting](troubleshooting.md)
- [Configure logging](../configuration/sb_server.conf.md)
