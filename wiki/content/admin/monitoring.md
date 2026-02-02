# Monitoring

**Last Updated:** 2026-01-30

---

## Overview

Effective monitoring ensures your ScratchBird database remains healthy, performant, and available. This guide covers built-in metrics, external monitoring integration, alerting, and dashboard configuration.

**Topics covered:**
- Built-in monitoring views
- Prometheus metrics export
- Grafana dashboards
- Log management
- Alerting strategies

---

## Part 1: Built-in Monitoring

### System Catalog Views

ScratchBird provides several views for monitoring database health:

```sql
-- Active connections
SELECT * FROM pg_stat_activity;

-- Database statistics
SELECT * FROM pg_stat_database;

-- Table statistics
SELECT * FROM pg_stat_user_tables;

-- Index statistics
SELECT * FROM pg_stat_user_indexes;

-- Background writer statistics
SELECT * FROM pg_stat_bgwriter;
```

### SHOW METRICS

The server can emit core metrics directly:

```sql
SHOW METRICS;
```

Or via the CLI:

```bash
sb_admin mydb metrics -U SYSARCH -P changeme
```

### Key Metrics

**Connection Metrics:**
```sql
-- Current connections by state
SELECT
    state,
    COUNT(*) AS connections,
    ROUND(COUNT(*) * 100.0 / (SELECT setting::int FROM pg_settings WHERE name = 'max_connections'), 2) AS pct_of_max
FROM pg_stat_activity
WHERE backend_type = 'client backend'
GROUP BY state
ORDER BY connections DESC;

-- Connections by database
SELECT
    datname AS database,
    COUNT(*) AS connections
FROM pg_stat_activity
WHERE backend_type = 'client backend'
GROUP BY datname
ORDER BY connections DESC;

-- Connections by user
SELECT
    usename AS username,
    COUNT(*) AS connections
FROM pg_stat_activity
WHERE backend_type = 'client backend'
GROUP BY usename
ORDER BY connections DESC;
```

**Performance Metrics:**
```sql
-- Cache hit ratio (should be > 99%)
SELECT
    datname AS database,
    ROUND(
        blks_hit * 100.0 / NULLIF(blks_hit + blks_read, 0),
        2
    ) AS cache_hit_ratio
FROM pg_stat_database
WHERE datname NOT IN ('template0', 'template1');

-- Transaction rates
SELECT
    datname AS database,
    xact_commit AS commits,
    xact_rollback AS rollbacks,
    ROUND(xact_rollback * 100.0 / NULLIF(xact_commit + xact_rollback, 0), 2) AS rollback_pct
FROM pg_stat_database
WHERE datname NOT IN ('template0', 'template1');

-- Tuple operations
SELECT
    datname AS database,
    tup_returned AS rows_returned,
    tup_fetched AS rows_fetched,
    tup_inserted AS rows_inserted,
    tup_updated AS rows_updated,
    tup_deleted AS rows_deleted
FROM pg_stat_database
WHERE datname NOT IN ('template0', 'template1');
```

**Table Health:**
```sql
-- Tables needing vacuum
SELECT
    schemaname || '.' || relname AS table_name,
    n_dead_tup AS dead_tuples,
    n_live_tup AS live_tuples,
    ROUND(n_dead_tup * 100.0 / NULLIF(n_live_tup + n_dead_tup, 0), 2) AS dead_pct,
    last_vacuum,
    last_autovacuum
FROM pg_stat_user_tables
WHERE n_dead_tup > 1000
ORDER BY n_dead_tup DESC
LIMIT 20;

-- Table sizes
SELECT
    schemaname || '.' || relname AS table_name,
    pg_size_pretty(pg_total_relation_size(relid)) AS total_size,
    pg_size_pretty(pg_relation_size(relid)) AS table_size,
    pg_size_pretty(pg_indexes_size(relid)) AS index_size
FROM pg_stat_user_tables
ORDER BY pg_total_relation_size(relid) DESC
LIMIT 20;

-- Sequential vs index scans
SELECT
    schemaname || '.' || relname AS table_name,
    seq_scan,
    seq_tup_read,
    idx_scan,
    idx_tup_fetch,
    CASE
        WHEN seq_scan > 0 THEN ROUND(idx_scan * 100.0 / (seq_scan + idx_scan), 2)
        ELSE 100
    END AS index_usage_pct
FROM pg_stat_user_tables
WHERE seq_scan + idx_scan > 0
ORDER BY seq_scan DESC
LIMIT 20;
```

**Index Health:**
```sql
-- Unused indexes
SELECT
    schemaname || '.' || relname AS table_name,
    indexrelname AS index_name,
    pg_size_pretty(pg_relation_size(indexrelid)) AS index_size,
    idx_scan AS scans
FROM pg_stat_user_indexes
WHERE idx_scan = 0
AND indexrelname NOT LIKE '%_pkey'
ORDER BY pg_relation_size(indexrelid) DESC;

-- Index efficiency
SELECT
    schemaname || '.' || relname AS table_name,
    indexrelname AS index_name,
    idx_scan AS scans,
    idx_tup_read AS tuples_read,
    idx_tup_fetch AS tuples_fetched
FROM pg_stat_user_indexes
WHERE idx_scan > 0
ORDER BY idx_scan DESC
LIMIT 20;
```

### Real-time Monitoring Queries

**Long-running queries:**
```sql
SELECT
    pid,
    usename,
    datname,
    state,
    query_start,
    NOW() - query_start AS duration,
    LEFT(query, 100) AS query_preview
FROM pg_stat_activity
WHERE state != 'idle'
AND query_start < NOW() - INTERVAL '30 seconds'
ORDER BY query_start;
```

**Blocking queries:**
```sql
SELECT
    blocked.pid AS blocked_pid,
    blocked.usename AS blocked_user,
    blocking.pid AS blocking_pid,
    blocking.usename AS blocking_user,
    blocked.query AS blocked_query,
    blocking.query AS blocking_query
FROM pg_stat_activity blocked
JOIN pg_stat_activity blocking ON blocking.pid = ANY(pg_blocking_pids(blocked.pid))
WHERE blocked.cardinality(pg_blocking_pids(blocked.pid)) > 0;
```

**Lock contention:**
```sql
SELECT
    l.relation::regclass AS table_name,
    l.mode AS lock_mode,
    l.granted,
    a.usename,
    a.query,
    a.query_start
FROM pg_locks l
JOIN pg_stat_activity a ON l.pid = a.pid
WHERE l.relation IS NOT NULL
ORDER BY l.relation, l.granted;
```

---

## Part 2: Prometheus Integration

### Enable Metrics Export

Edit `sb_server.conf`:
```ini
[statistics]
enabled = true
export = prometheus
prometheus_port = 9090
prometheus_path = /metrics
```

Restart server:
```bash
sudo systemctl restart scratchbird
```

### Available Metrics

```bash
# View all metrics
curl http://localhost:9090/metrics

# Key metrics:
# scratchbird_connections_active       - Current active connections
# scratchbird_connections_idle         - Current idle connections
# scratchbird_connections_total        - Total connections since startup
# scratchbird_queries_total            - Total queries executed
# scratchbird_queries_duration_seconds - Query duration histogram
# scratchbird_transactions_committed   - Committed transactions
# scratchbird_transactions_rolled_back - Rolled back transactions
# scratchbird_buffer_pool_hit_ratio    - Buffer pool cache hit ratio
# scratchbird_disk_read_bytes          - Bytes read from disk
# scratchbird_disk_write_bytes         - Bytes written to disk
# scratchbird_rows_returned            - Rows returned by queries
# scratchbird_rows_inserted            - Rows inserted
# scratchbird_rows_updated             - Rows updated
# scratchbird_rows_deleted             - Rows deleted
# scratchbird_table_size_bytes         - Table sizes
# scratchbird_index_size_bytes         - Index sizes
# scratchbird_dead_tuples              - Dead tuples count
# scratchbird_replication_lag_seconds  - Replication lag (if applicable)
```

### Prometheus Configuration

Add to `prometheus.yml`:
```yaml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

scrape_configs:
  - job_name: 'scratchbird'
    static_configs:
      - targets: ['scratchbird-server:9090']
    relabel_configs:
      - source_labels: [__address__]
        target_label: instance
        regex: '([^:]+):.*'
        replacement: '${1}'

  # Multiple instances
  - job_name: 'scratchbird-cluster'
    static_configs:
      - targets:
          - 'db1.example.com:9090'
          - 'db2.example.com:9090'
          - 'db3.example.com:9090'
```

### Recording Rules

Create `scratchbird_rules.yml`:
```yaml
groups:
  - name: scratchbird_recording
    interval: 30s
    rules:
      # Connection utilization
      - record: scratchbird:connection_utilization
        expr: |
          scratchbird_connections_active /
          scratchbird_connections_max * 100

      # Query rate (queries per second)
      - record: scratchbird:queries_per_second
        expr: |
          rate(scratchbird_queries_total[5m])

      # Transaction rate
      - record: scratchbird:transactions_per_second
        expr: |
          rate(scratchbird_transactions_committed[5m]) +
          rate(scratchbird_transactions_rolled_back[5m])

      # Rollback ratio
      - record: scratchbird:rollback_ratio
        expr: |
          rate(scratchbird_transactions_rolled_back[5m]) /
          (rate(scratchbird_transactions_committed[5m]) +
           rate(scratchbird_transactions_rolled_back[5m])) * 100

      # Cache hit ratio (5m average)
      - record: scratchbird:cache_hit_ratio_5m
        expr: |
          avg_over_time(scratchbird_buffer_pool_hit_ratio[5m])

      # Query latency percentiles
      - record: scratchbird:query_latency_p50
        expr: |
          histogram_quantile(0.50, rate(scratchbird_queries_duration_seconds_bucket[5m]))

      - record: scratchbird:query_latency_p95
        expr: |
          histogram_quantile(0.95, rate(scratchbird_queries_duration_seconds_bucket[5m]))

      - record: scratchbird:query_latency_p99
        expr: |
          histogram_quantile(0.99, rate(scratchbird_queries_duration_seconds_bucket[5m]))
```

---

## Part 3: Alerting

### Prometheus Alerting Rules

Create `scratchbird_alerts.yml`:
```yaml
groups:
  - name: scratchbird_alerts
    rules:
      # Connection alerts
      - alert: ScratchBirdHighConnectionUsage
        expr: scratchbird:connection_utilization > 80
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High connection usage on {{ $labels.instance }}"
          description: "Connection usage is {{ $value }}% (threshold: 80%)"

      - alert: ScratchBirdConnectionsExhausted
        expr: scratchbird:connection_utilization > 95
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Connections nearly exhausted on {{ $labels.instance }}"
          description: "Connection usage is {{ $value }}% (threshold: 95%)"

      # Performance alerts
      - alert: ScratchBirdLowCacheHitRatio
        expr: scratchbird:cache_hit_ratio_5m < 95
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "Low cache hit ratio on {{ $labels.instance }}"
          description: "Cache hit ratio is {{ $value }}% (threshold: 95%)"

      - alert: ScratchBirdHighQueryLatency
        expr: scratchbird:query_latency_p95 > 1.0
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High query latency on {{ $labels.instance }}"
          description: "95th percentile latency is {{ $value }}s (threshold: 1s)"

      - alert: ScratchBirdHighRollbackRatio
        expr: scratchbird:rollback_ratio > 5
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High rollback ratio on {{ $labels.instance }}"
          description: "Rollback ratio is {{ $value }}% (threshold: 5%)"

      # Availability alerts
      - alert: ScratchBirdDown
        expr: up{job="scratchbird"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "ScratchBird instance down: {{ $labels.instance }}"
          description: "ScratchBird has been unreachable for more than 1 minute"

      # Replication alerts
      - alert: ScratchBirdReplicationLag
        expr: scratchbird_replication_lag_seconds > 60
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Replication lag detected on {{ $labels.instance }}"
          description: "Replication lag is {{ $value }}s (threshold: 60s)"

      - alert: ScratchBirdReplicationLagCritical
        expr: scratchbird_replication_lag_seconds > 300
        for: 2m
        labels:
          severity: critical
        annotations:
          summary: "Critical replication lag on {{ $labels.instance }}"
          description: "Replication lag is {{ $value }}s (threshold: 300s)"

      # Storage alerts
      - alert: ScratchBirdDiskSpaceLow
        expr: |
          (node_filesystem_avail_bytes{mountpoint="/var/lib/scratchbird"} /
           node_filesystem_size_bytes{mountpoint="/var/lib/scratchbird"}) * 100 < 20
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Low disk space for ScratchBird on {{ $labels.instance }}"
          description: "Only {{ $value }}% disk space remaining"

      # Dead tuples alert
      - alert: ScratchBirdHighDeadTuples
        expr: scratchbird_dead_tuples > 1000000
        for: 30m
        labels:
          severity: warning
        annotations:
          summary: "High dead tuple count on {{ $labels.instance }}"
          description: "{{ $value }} dead tuples detected. VACUUM may be needed."
```

### Alertmanager Configuration

Create `alertmanager.yml`:
```yaml
global:
  smtp_smarthost: 'smtp.example.com:587'
  smtp_from: 'alerts@example.com'
  smtp_auth_username: 'alerts@example.com'
  smtp_auth_password: 'password'

route:
  group_by: ['alertname', 'instance']
  group_wait: 30s
  group_interval: 5m
  repeat_interval: 4h
  receiver: 'default'
  routes:
    - match:
        severity: critical
      receiver: 'critical'
      continue: true
    - match:
        severity: warning
      receiver: 'warning'

receivers:
  - name: 'default'
    email_configs:
      - to: 'dba@example.com'

  - name: 'critical'
    email_configs:
      - to: 'oncall@example.com'
    slack_configs:
      - api_url: 'https://hooks.slack.com/services/XXX'
        channel: '#alerts-critical'
        title: '{{ .GroupLabels.alertname }}'
        text: '{{ range .Alerts }}{{ .Annotations.description }}{{ end }}'
    pagerduty_configs:
      - service_key: 'your-pagerduty-key'

  - name: 'warning'
    email_configs:
      - to: 'dba@example.com'
    slack_configs:
      - api_url: 'https://hooks.slack.com/services/XXX'
        channel: '#alerts-warning'
```

---

## Part 4: Grafana Dashboards

### Installing Grafana

```bash
# Add repository
sudo apt-get install -y apt-transport-https software-properties-common
wget -q -O - https://packages.grafana.com/gpg.key | sudo apt-key add -
echo "deb https://packages.grafana.com/oss/deb stable main" | sudo tee /etc/apt/sources.list.d/grafana.list

# Install
sudo apt-get update
sudo apt-get install grafana

# Start
sudo systemctl enable --now grafana-server
```

### Dashboard JSON

Create `scratchbird-dashboard.json`:
```json
{
  "dashboard": {
    "title": "ScratchBird Overview",
    "uid": "scratchbird-main",
    "tags": ["scratchbird", "database"],
    "timezone": "browser",
    "refresh": "30s",
    "panels": [
      {
        "title": "Active Connections",
        "type": "gauge",
        "gridPos": {"h": 8, "w": 6, "x": 0, "y": 0},
        "targets": [
          {
            "expr": "scratchbird_connections_active",
            "legendFormat": "{{ instance }}"
          }
        ],
        "fieldConfig": {
          "defaults": {
            "thresholds": {
              "steps": [
                {"color": "green", "value": null},
                {"color": "yellow", "value": 80},
                {"color": "red", "value": 95}
              ]
            },
            "max": 100,
            "unit": "percent"
          }
        }
      },
      {
        "title": "Queries per Second",
        "type": "timeseries",
        "gridPos": {"h": 8, "w": 12, "x": 6, "y": 0},
        "targets": [
          {
            "expr": "rate(scratchbird_queries_total[5m])",
            "legendFormat": "{{ instance }}"
          }
        ]
      },
      {
        "title": "Cache Hit Ratio",
        "type": "stat",
        "gridPos": {"h": 8, "w": 6, "x": 18, "y": 0},
        "targets": [
          {
            "expr": "scratchbird_buffer_pool_hit_ratio",
            "legendFormat": "{{ instance }}"
          }
        ],
        "fieldConfig": {
          "defaults": {
            "thresholds": {
              "steps": [
                {"color": "red", "value": null},
                {"color": "yellow", "value": 95},
                {"color": "green", "value": 99}
              ]
            },
            "unit": "percent"
          }
        }
      },
      {
        "title": "Query Latency (p95)",
        "type": "timeseries",
        "gridPos": {"h": 8, "w": 12, "x": 0, "y": 8},
        "targets": [
          {
            "expr": "histogram_quantile(0.95, rate(scratchbird_queries_duration_seconds_bucket[5m]))",
            "legendFormat": "p95 - {{ instance }}"
          },
          {
            "expr": "histogram_quantile(0.50, rate(scratchbird_queries_duration_seconds_bucket[5m]))",
            "legendFormat": "p50 - {{ instance }}"
          }
        ],
        "fieldConfig": {
          "defaults": {
            "unit": "s"
          }
        }
      },
      {
        "title": "Transactions",
        "type": "timeseries",
        "gridPos": {"h": 8, "w": 12, "x": 12, "y": 8},
        "targets": [
          {
            "expr": "rate(scratchbird_transactions_committed[5m])",
            "legendFormat": "Committed - {{ instance }}"
          },
          {
            "expr": "rate(scratchbird_transactions_rolled_back[5m])",
            "legendFormat": "Rolled Back - {{ instance }}"
          }
        ]
      },
      {
        "title": "Row Operations",
        "type": "timeseries",
        "gridPos": {"h": 8, "w": 24, "x": 0, "y": 16},
        "targets": [
          {
            "expr": "rate(scratchbird_rows_returned[5m])",
            "legendFormat": "Returned"
          },
          {
            "expr": "rate(scratchbird_rows_inserted[5m])",
            "legendFormat": "Inserted"
          },
          {
            "expr": "rate(scratchbird_rows_updated[5m])",
            "legendFormat": "Updated"
          },
          {
            "expr": "rate(scratchbird_rows_deleted[5m])",
            "legendFormat": "Deleted"
          }
        ]
      }
    ]
  }
}
```

### Import Dashboard

```bash
# Via API
curl -X POST \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_API_KEY" \
  -d @scratchbird-dashboard.json \
  http://localhost:3000/api/dashboards/db
```

---

## Part 5: Log Management

### Log Configuration

Edit `sb_server.conf`:
```ini
[logging]
level = info
destination = stderr
format = json

# Log slow queries
log_slow_queries = true
slow_query_threshold = 1000  # milliseconds

# Log connections
log_connections = true
log_disconnections = true

# Statement logging
log_statement = ddl  # none, ddl, mod, all
```

### Log Formats

**Standard format:**
```
2026-01-19 10:30:45.123 INFO [main] Server started on port 3092
2026-01-19 10:30:46.456 INFO [conn-1] Connection from 192.168.1.100
2026-01-19 10:30:47.789 WARNING [conn-1] Slow query (1523ms): SELECT * FROM large_table
```

**JSON format (recommended for log aggregation):**
```json
{"timestamp":"2026-01-19T10:30:45.123Z","level":"INFO","component":"main","message":"Server started on port 3092"}
{"timestamp":"2026-01-19T10:30:46.456Z","level":"INFO","component":"conn-1","message":"Connection from 192.168.1.100","client_ip":"192.168.1.100"}
{"timestamp":"2026-01-19T10:30:47.789Z","level":"WARNING","component":"conn-1","message":"Slow query","duration_ms":1523,"query":"SELECT * FROM large_table"}
```

### Log Rotation

Create `/etc/logrotate.d/scratchbird`:
```
/var/log/scratchbird/*.log {
    daily
    rotate 14
    compress
    delaycompress
    missingok
    notifempty
    create 0640 scratchbird scratchbird
    postrotate
        systemctl reload scratchbird > /dev/null 2>&1 || true
    endscript
}
```

### Centralized Logging

**Filebeat configuration:**
```yaml
filebeat.inputs:
  - type: log
    enabled: true
    paths:
      - /var/log/scratchbird/*.log
    json.keys_under_root: true
    json.add_error_key: true
    fields:
      service: scratchbird
      environment: production

output.elasticsearch:
  hosts: ["elasticsearch:9200"]
  index: "scratchbird-%{+yyyy.MM.dd}"
```

**Fluent Bit configuration:**
```ini
[INPUT]
    Name              tail
    Path              /var/log/scratchbird/*.log
    Parser            json
    Tag               scratchbird.*
    Refresh_Interval  5

[FILTER]
    Name              modify
    Match             scratchbird.*
    Add               service scratchbird
    Add               environment production

[OUTPUT]
    Name              es
    Match             scratchbird.*
    Host              elasticsearch
    Port              9200
    Index             scratchbird
    Type              _doc
```

---

## Part 6: Health Checks

### HTTP Health Endpoint

ScratchBird provides a health check endpoint:

```bash
# Basic health check
curl http://localhost:9090/health
# Response: {"status":"healthy","version":"1.0.0"}

# Detailed health check
curl http://localhost:9090/health?detailed=true
# Response:
# {
#   "status": "healthy",
#   "version": "1.0.0",
#   "uptime_seconds": 86400,
#   "connections": {
#     "active": 15,
#     "idle": 5,
#     "max": 100
#   },
#   "cache_hit_ratio": 99.5,
#   "replication_lag_seconds": 0.5
# }
```

### Load Balancer Health Check

For HAProxy:
```
backend scratchbird
    option httpchk GET /health
    http-check expect status 200
    server db1 192.168.1.10:9090 check
    server db2 192.168.1.11:9090 check
```

For nginx:
```nginx
upstream scratchbird {
    server 192.168.1.10:5432;
    server 192.168.1.11:5432;
}

server {
    location /health {
        proxy_pass http://192.168.1.10:9090/health;
    }
}
```

### Custom Health Check Script

Create `/usr/local/bin/sb_healthcheck.sh`:
```bash
#!/bin/bash
# ScratchBird health check script

set -euo pipefail

DB_HOST="${1:-localhost}"
DB_PORT="${2:-3092}"
DB_USER="${3:-admin}"

# Check if server is responding
if ! sb_isql -H "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -c "SELECT 1" > /dev/null 2>&1; then
    echo "CRITICAL: Cannot connect to ScratchBird"
    exit 2
fi

# Check connection count
CONN_PCT=$(sb_isql -H "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -t -c "
SELECT ROUND(COUNT(*) * 100.0 / (SELECT setting::int FROM pg_settings WHERE name = 'max_connections'), 2)
FROM pg_stat_activity
WHERE backend_type = 'client backend'
")

if (( $(echo "$CONN_PCT > 90" | bc -l) )); then
    echo "WARNING: Connection usage at ${CONN_PCT}%"
    exit 1
fi

# Check cache hit ratio
CACHE_RATIO=$(sb_isql -H "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -t -c "
SELECT ROUND(blks_hit * 100.0 / NULLIF(blks_hit + blks_read, 0), 2)
FROM pg_stat_database
WHERE datname = current_database()
")

if (( $(echo "$CACHE_RATIO < 95" | bc -l) )); then
    echo "WARNING: Cache hit ratio at ${CACHE_RATIO}%"
    exit 1
fi

echo "OK: ScratchBird healthy (connections: ${CONN_PCT}%, cache: ${CACHE_RATIO}%)"
exit 0
```

---

## Part 7: Performance Baselines

### Establishing Baselines

Run baseline collection script regularly:

```sql
-- Create baseline tables
CREATE SCHEMA IF NOT EXISTS sb_monitoring;

CREATE TABLE sb_monitoring.performance_baseline (
    id SERIAL PRIMARY KEY,
    collected_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    metric_name VARCHAR(100) NOT NULL,
    metric_value NUMERIC NOT NULL,
    context JSONB
);

-- Collect baseline metrics
INSERT INTO sb_monitoring.performance_baseline (metric_name, metric_value, context)
SELECT 'queries_per_second',
       (SELECT COUNT(*) FROM pg_stat_statements) /
       EXTRACT(EPOCH FROM (NOW() - pg_postmaster_start_time())),
       jsonb_build_object('timestamp', NOW())
UNION ALL
SELECT 'avg_query_time_ms',
       AVG(mean_exec_time),
       jsonb_build_object('timestamp', NOW())
FROM pg_stat_statements
WHERE calls > 10
UNION ALL
SELECT 'cache_hit_ratio',
       (SELECT ROUND(blks_hit * 100.0 / NULLIF(blks_hit + blks_read, 0), 2)
        FROM pg_stat_database WHERE datname = current_database()),
       jsonb_build_object('timestamp', NOW())
UNION ALL
SELECT 'active_connections',
       COUNT(*),
       jsonb_build_object('timestamp', NOW())
FROM pg_stat_activity
WHERE state = 'active';
```

### Baseline Comparison

```sql
-- Compare current vs baseline
WITH current_metrics AS (
    SELECT
        'cache_hit_ratio' AS metric,
        ROUND(blks_hit * 100.0 / NULLIF(blks_hit + blks_read, 0), 2) AS current_value
    FROM pg_stat_database
    WHERE datname = current_database()
),
baseline AS (
    SELECT
        metric_name AS metric,
        AVG(metric_value) AS baseline_value
    FROM sb_monitoring.performance_baseline
    WHERE collected_at > NOW() - INTERVAL '7 days'
    AND metric_name = 'cache_hit_ratio'
    GROUP BY metric_name
)
SELECT
    c.metric,
    c.current_value,
    b.baseline_value,
    ROUND((c.current_value - b.baseline_value) / b.baseline_value * 100, 2) AS pct_change
FROM current_metrics c
JOIN baseline b ON c.metric = b.metric;
```

---

## Quick Reference

### Key Monitoring Queries

```sql
-- Active connections
SELECT COUNT(*) FROM pg_stat_activity WHERE state = 'active';

-- Cache hit ratio
SELECT ROUND(blks_hit * 100.0 / NULLIF(blks_hit + blks_read, 0), 2)
FROM pg_stat_database WHERE datname = current_database();

-- Long-running queries
SELECT pid, NOW() - query_start AS duration, query
FROM pg_stat_activity
WHERE state = 'active' AND query_start < NOW() - INTERVAL '1 minute';

-- Table bloat
SELECT schemaname || '.' || relname, n_dead_tup
FROM pg_stat_user_tables
WHERE n_dead_tup > 10000
ORDER BY n_dead_tup DESC;
```

### Important Thresholds

| Metric | Warning | Critical |
|--------|---------|----------|
| Connection usage | > 80% | > 95% |
| Cache hit ratio | < 95% | < 90% |
| Query latency (p95) | > 1s | > 5s |
| Rollback ratio | > 5% | > 10% |
| Replication lag | > 60s | > 300s |
| Dead tuples | > 1M | > 10M |

### Prometheus Metrics Port

Default: `9090`

```bash
curl http://localhost:9090/metrics
curl http://localhost:9090/health
```

---

## See Also

- [Backup and Restore](backup-restore.md)
- [Security Administration](security.md)
- [Performance Tuning](../user-guides/Performance-Tuning.md)
- [Troubleshooting](troubleshooting.md)
