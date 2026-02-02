# Troubleshooting (Admin)

**Last Updated:** 2026-01-30

---

## Overview

This guide helps administrators diagnose and resolve common ScratchBird issues. For general user troubleshooting, see the [Troubleshooting section](../troubleshooting/).

**Topics covered:**
- Server startup issues
- Connection problems
- Performance issues
- Storage problems
- Replication issues

---

## Part 1: Server Startup Issues

### Server Won't Start

**Symptom:** `systemctl start scratchbird` fails

**Check the logs:**
```bash
# View service status
systemctl status scratchbird

# Check journal logs
journalctl -u scratchbird -n 100 --no-pager

# Check server log
tail -100 /var/log/scratchbird/server.log
```

**Common causes and solutions:**

**1. Port already in use**
```bash
# Check what's using the port
ss -tlnp | grep 3092
lsof -i :3092

# Solution: Stop the other process or change port
# Or kill existing ScratchBird process
pkill -f sb_server
```

**2. Data directory permissions**
```bash
# Check ownership
ls -la /var/lib/scratchbird/

# Fix permissions
chown -R scratchbird:scratchbird /var/lib/scratchbird/
chmod 700 /var/lib/scratchbird/
```

**3. Invalid configuration**
```bash
# Validate configuration
sb_server --config /etc/scratchbird/sb_server.conf --check

# Common issues:
# - Missing required settings
# - Invalid values
# - Syntax errors
```

**4. Corrupted data files**
```bash
# Check for corruption
sb_admin --check /var/lib/scratchbird/data

# Attempt repair
sb_admin --repair /var/lib/scratchbird/data
```

**5. Lock file exists**
```bash
# Remove stale lock file
rm /var/lib/scratchbird/data/postmaster.pid

# Only if server is definitely not running!
ps aux | grep sb_server
```

### Server Crashes on Startup

**Check for core dumps:**
```bash
# Find core dumps
ls -la /var/lib/scratchbird/core*
coredumpctl list

# Analyze core dump
gdb /usr/bin/sb_server /var/lib/scratchbird/core.12345
```

**Memory issues:**
```bash
# Check available memory
free -h

# Reduce buffer_pool_size if needed
# Edit sb_server.conf: buffer_pool_size = 256MB
```

---

## Part 2: Connection Issues

### Cannot Connect to Server

**Symptom:** `sb_isql: could not connect to server`

**Diagnostic steps:**

```bash
# 1. Is the server running?
systemctl status scratchbird
ps aux | grep sb_server

# 2. Is the port listening?
ss -tlnp | grep 3092

# 3. Is the firewall blocking?
iptables -L -n | grep 3092
ufw status

# 4. Test local connection
sb_isql -H localhost -p 3092 -U admin -d postgres

# 5. Test network connection
telnet db-server 3092
nc -zv db-server 3092
```

### Authentication Failed

**Symptom:** `FATAL: password authentication failed for user`

**Solutions:**

```bash
# 1. Reset password
sb_isql -U admin -d postgres -c "ALTER USER app_user WITH PASSWORD 'newpassword'"

# 2. Check sb_hba.conf
cat /etc/scratchbird/sb_hba.conf

# 3. Check authentication method
# Ensure SCRAM-SHA-256 is used consistently:
# - In sb_server.conf: password_encryption = scram-sha-256
# - In sb_hba.conf: use scram-sha-256 method
# - Re-set passwords after changing encryption

# 4. Reload configuration
systemctl reload scratchbird
```

### Too Many Connections

**Symptom:** `FATAL: too many connections for role`

**Immediate fix:**
```sql
-- Check current connections
SELECT usename, COUNT(*) FROM pg_stat_activity GROUP BY usename;

-- Terminate idle connections
SELECT pg_terminate_backend(pid)
FROM pg_stat_activity
WHERE state = 'idle'
AND query_start < NOW() - INTERVAL '1 hour';
```

**Long-term fix:**
```ini
# Increase max_connections in sb_server.conf
[server]
max_connections = 200
```

```sql
-- Or limit per-user connections
ALTER USER app_user CONNECTION LIMIT 20;
```

### SSL Connection Required

**Symptom:** `FATAL: SSL connection is required`

**Client needs SSL:**
```bash
# Connect with SSL
sb_isql "host=db-server dbname=mydb user=admin sslmode=require"

# Or configure in sb_hba.conf to allow non-SSL for specific hosts
hostnossl  all  all  192.168.1.0/24  scram-sha-256
```

---

## Part 3: Performance Issues

### Slow Queries

**Find slow queries:**
```sql
-- Currently running slow queries
SELECT
    pid,
    NOW() - query_start AS duration,
    state,
    LEFT(query, 100) AS query
FROM pg_stat_activity
WHERE state = 'active'
AND query_start < NOW() - INTERVAL '10 seconds'
ORDER BY query_start;

-- Historical slow queries (requires pg_stat_statements)
SELECT
    LEFT(query, 80) AS query,
    calls,
    ROUND(mean_exec_time::numeric, 2) AS avg_ms,
    ROUND(total_exec_time::numeric, 2) AS total_ms
FROM pg_stat_statements
ORDER BY mean_exec_time DESC
LIMIT 20;
```

**Analyze query performance:**
```sql
-- Get query plan
EXPLAIN (ANALYZE, BUFFERS, FORMAT TEXT)
SELECT * FROM orders WHERE user_id = 123;
```

**Common solutions:**

```sql
-- 1. Missing index
CREATE INDEX idx_orders_user ON orders(user_id);

-- 2. Outdated statistics
ANALYZE orders;

-- 3. Table bloat
VACUUM ANALYZE orders;

-- 4. Bad query plan - force index
SET enable_seqscan = off;
-- Then run query and re-enable
SET enable_seqscan = on;
```

### High CPU Usage

**Identify CPU-intensive queries:**
```sql
SELECT
    pid,
    usename,
    state,
    NOW() - query_start AS duration,
    LEFT(query, 60) AS query
FROM pg_stat_activity
WHERE state = 'active'
ORDER BY query_start;
```

**Kill runaway query:**
```sql
-- Cancel query (graceful)
SELECT pg_cancel_backend(12345);

-- Terminate connection (forceful)
SELECT pg_terminate_backend(12345);
```

### High Memory Usage

**Check memory allocation:**
```sql
-- Check shared buffer usage
SELECT
    c.relname,
    pg_size_pretty(count(*) * 8192) AS buffered,
    round(100.0 * count(*) / (
        SELECT setting::integer FROM pg_settings WHERE name = 'buffer_pool_size'
    ), 2) AS buffer_percent
FROM pg_buffercache b
JOIN pg_class c ON b.relfilenode = c.relfilenode
GROUP BY c.relname
ORDER BY count(*) DESC
LIMIT 20;
```

**Reduce memory usage:**
```ini
# In sb_server.conf
[memory]
buffer_pool_size = 256MB      # Reduce if needed
work_mem = 4MB              # Per-operation memory
maintenance_work_mem = 64MB # For VACUUM, CREATE INDEX
```

### Connection Pooling Issues

**Symptom:** Connections exhausted despite low traffic

**Solution:** Implement connection pooling

```ini
# PgBouncer configuration
[databases]
mydb = host=localhost port=5432 dbname=mydb

[pgbouncer]
listen_port = 6432
listen_addr = *
auth_type = scram-sha-256
auth_file = /etc/pgbouncer/userlist.txt
pool_mode = transaction
max_client_conn = 1000
default_pool_size = 20
```

---

## Part 4: Storage Issues

### Disk Space Full

**Immediate actions:**
```bash
# Check disk usage
df -h /var/lib/scratchbird

# Find large files
du -sh /var/lib/scratchbird/* | sort -h

# Check WAL accumulation
ls -la /var/lib/scratchbird/data/pg_wal/
du -sh /var/lib/scratchbird/data/pg_wal/
```

**Clear space:**
```bash
# Remove old WAL files (if not needed for recovery)
# WARNING: Only if you have recent backups!
sb_isql -U admin -c "SELECT pg_switch_wal()"
# Then remove old files

# Clear old logs
find /var/log/scratchbird -name "*.log" -mtime +7 -delete

# Vacuum to reclaim space
sb_isql -U admin -d mydb -c "VACUUM FULL"
```

### Table Bloat

**Detect bloat:**
```sql
-- Check dead tuple ratio
SELECT
    schemaname || '.' || relname AS table_name,
    n_live_tup AS live_tuples,
    n_dead_tup AS dead_tuples,
    ROUND(n_dead_tup * 100.0 / NULLIF(n_live_tup, 0), 2) AS dead_pct,
    last_vacuum,
    last_autovacuum
FROM pg_stat_user_tables
WHERE n_dead_tup > 10000
ORDER BY n_dead_tup DESC;
```

**Fix bloat:**
```sql
-- Regular vacuum (doesn't lock table)
VACUUM ANALYZE orders;

-- Full vacuum (locks table, reclaims disk space)
VACUUM FULL orders;

-- Or use pg_repack for online rebuild
-- pg_repack -d mydb -t orders
```

### WAL Accumulation

**Symptom:** `pg_wal` directory growing

**Causes:**
1. Failed archive command
2. Replication lag
3. Long-running transactions

**Solutions:**
```bash
# 1. Check archive status
sb_isql -U admin -c "SELECT * FROM pg_stat_archiver"

# 2. Check replication lag
sb_isql -U admin -c "SELECT * FROM pg_stat_replication"

# 3. Find long transactions
sb_isql -U admin -c "
SELECT pid, xact_start, NOW() - xact_start AS duration
FROM pg_stat_activity
WHERE xact_start IS NOT NULL
ORDER BY xact_start
LIMIT 10"

# 4. Force checkpoint
sb_isql -U admin -c "CHECKPOINT"
```

### Data Corruption

**Detect corruption:**
```bash
# Check data integrity
sb_admin --check /var/lib/scratchbird/data

# Verify specific table
sb_isql -U admin -d mydb -c "
SELECT COUNT(*) FROM orders;
-- If this errors, table may be corrupt
"
```

**Recovery options:**

```bash
# 1. Try repair
sb_admin --repair /var/lib/scratchbird/data

# 2. Restore from backup
systemctl stop scratchbird
sb_restore -U admin -d mydb /backups/latest.sbk
systemctl start scratchbird

# 3. Point-in-time recovery (if WAL archiving enabled)
# See Backup and Restore guide
```

---

## Part 5: Replication Issues

### Replica Not Syncing

**Check replication status:**
```sql
-- On primary
SELECT
    client_addr,
    state,
    sent_lsn,
    write_lsn,
    flush_lsn,
    replay_lsn,
    pg_wal_lsn_diff(sent_lsn, replay_lsn) AS lag_bytes
FROM pg_stat_replication;

-- On replica
SELECT
    pg_is_in_recovery() AS is_replica,
    pg_last_wal_receive_lsn() AS receive_lsn,
    pg_last_wal_replay_lsn() AS replay_lsn,
    pg_last_xact_replay_timestamp() AS last_replay_time;
```

**Common issues:**

**1. Network connectivity**
```bash
# Test connection from replica to primary
nc -zv primary-server 5432
```

**2. Authentication**
```bash
# Check replication user can connect
sb_isql -h primary-server -U repl_user -d postgres
```

**3. WAL not available**
```sql
-- Increase wal_keep_size on primary
ALTER SYSTEM SET wal_keep_size = '2GB';
SELECT pg_reload_conf();
```

### Replication Lag

**Monitor lag:**
```sql
-- Lag in bytes
SELECT
    client_addr,
    pg_wal_lsn_diff(sent_lsn, replay_lsn) AS lag_bytes,
    pg_size_pretty(pg_wal_lsn_diff(sent_lsn, replay_lsn)) AS lag_pretty
FROM pg_stat_replication;

-- Lag in time (approximate)
SELECT NOW() - pg_last_xact_replay_timestamp() AS lag_time;
```

**Reduce lag:**
```ini
# On replica - tune recovery settings
[recovery]
recovery_min_apply_delay = 0
```

```sql
-- Pause/resume recovery to debug
SELECT pg_wal_replay_pause();
-- Debug
SELECT pg_wal_replay_resume();
```

---

## Part 6: Lock Issues

### Detecting Locks

```sql
-- All locks
SELECT
    l.locktype,
    l.relation::regclass AS table_name,
    l.mode,
    l.granted,
    a.usename,
    a.query,
    a.pid
FROM pg_locks l
JOIN pg_stat_activity a ON l.pid = a.pid
WHERE l.relation IS NOT NULL;

-- Blocking queries
SELECT
    blocked.pid AS blocked_pid,
    blocked.usename AS blocked_user,
    blocking.pid AS blocking_pid,
    blocking.usename AS blocking_user,
    blocked.query AS blocked_query,
    blocking.query AS blocking_query
FROM pg_stat_activity blocked
JOIN pg_locks bl ON blocked.pid = bl.pid
JOIN pg_locks kl ON bl.locktype = kl.locktype
    AND bl.database IS NOT DISTINCT FROM kl.database
    AND bl.relation IS NOT DISTINCT FROM kl.relation
    AND bl.page IS NOT DISTINCT FROM kl.page
    AND bl.tuple IS NOT DISTINCT FROM kl.tuple
    AND bl.virtualxid IS NOT DISTINCT FROM kl.virtualxid
    AND bl.transactionid IS NOT DISTINCT FROM kl.transactionid
    AND bl.classid IS NOT DISTINCT FROM kl.classid
    AND bl.objid IS NOT DISTINCT FROM kl.objid
    AND bl.objsubid IS NOT DISTINCT FROM kl.objsubid
    AND bl.pid != kl.pid
JOIN pg_stat_activity blocking ON kl.pid = blocking.pid
WHERE NOT bl.granted;
```

### Resolving Deadlocks

```sql
-- View deadlock info in logs
-- Then terminate one of the transactions
SELECT pg_terminate_backend(12345);
```

**Prevent deadlocks:**
```sql
-- Set lock timeout
SET lock_timeout = '10s';

-- Set deadlock detection interval
-- In sb_server.conf: deadlock_timeout = 1s
```

---

## Part 7: Diagnostic Queries

### System Health Check

```sql
-- Comprehensive health check
SELECT 'Version' AS metric, version() AS value
UNION ALL
SELECT 'Uptime', NOW() - pg_postmaster_start_time()::text
UNION ALL
SELECT 'Active connections', COUNT(*)::text FROM pg_stat_activity WHERE state = 'active'
UNION ALL
SELECT 'Database size', pg_size_pretty(pg_database_size(current_database()))
UNION ALL
SELECT 'Cache hit ratio', ROUND(
    (SELECT blks_hit * 100.0 / NULLIF(blks_hit + blks_read, 0)
     FROM pg_stat_database WHERE datname = current_database()), 2)::text || '%';
```

### Activity Summary

```sql
SELECT
    state,
    COUNT(*) AS connections,
    MAX(NOW() - query_start) AS longest_query
FROM pg_stat_activity
WHERE backend_type = 'client backend'
GROUP BY state;
```

### Table Statistics

```sql
SELECT
    schemaname || '.' || relname AS table_name,
    seq_scan,
    idx_scan,
    n_live_tup AS rows,
    n_dead_tup AS dead_rows,
    last_vacuum,
    last_analyze
FROM pg_stat_user_tables
ORDER BY n_dead_tup DESC
LIMIT 20;
```

---

## Quick Reference

### Emergency Commands

```bash
# Stop server gracefully
systemctl stop scratchbird

# Force stop
systemctl kill scratchbird

# Check logs
journalctl -u scratchbird -f

# Kill specific backend
sb_isql -U admin -c "SELECT pg_terminate_backend(12345)"
```

### Common Fixes

| Problem | Quick Fix |
|---------|-----------|
| Can't connect | Check `systemctl status scratchbird` |
| Auth failed | Reset password with ALTER USER |
| Too many connections | Kill idle: `pg_terminate_backend()` |
| Slow queries | Run `VACUUM ANALYZE` |
| Disk full | Clear logs, vacuum full |
| Table locked | Find and kill blocking query |

### Health Check Commands

```sql
-- Connections
SELECT COUNT(*) FROM pg_stat_activity;

-- Cache ratio
SELECT blks_hit * 100 / (blks_hit + blks_read) FROM pg_stat_database;

-- Long queries
SELECT pid, query_start, query FROM pg_stat_activity WHERE state = 'active';

-- Replication lag
SELECT pg_wal_lsn_diff(sent_lsn, replay_lsn) FROM pg_stat_replication;
```

---

## See Also

- [Monitoring](monitoring.md)
- [Backup and Restore](backup-restore.md)
- [Security Administration](security.md)
- [Performance Tuning](../user-guides/Performance-Tuning.md)
- [Connection Problems](../troubleshooting/Connection-Problems.md)

