# Troubleshooting Guide

**Last Updated:** 2026-02-03


Common issues and solutions.


---

## Quick Diagnostics

```bash
# Check server status
systemctl status scratchbird

# View recent logs
journalctl -u scratchbird -n 50

# Test connectivity
sb_isql -H localhost -P 3092 -c "SELECT 1"

# Check listening ports
ss -tlnp | grep -E "3092|5432|3306|3050"
```

---

## Connection Issues

### "Connection refused"

**Cause:** Server not running or wrong port.

**Solution:**
```bash
# Check if running
systemctl status scratchbird

# If stopped, start it
sudo systemctl start scratchbird

# Check listening ports
ss -tlnp | grep sb_server
```

### "No pg_hba.conf entry"

**Cause:** Client IP not allowed in hba.conf.

**Solution:**
1. Check client IP:
   ```bash
   echo "My IP: $(curl -s ifconfig.me)"
   ```

2. Add entry to `/etc/scratchbird/hba.conf`:
   ```
   host    all    all    YOUR_IP/32    scram-sha-256
   ```

3. Reload:
   ```bash
   sudo systemctl reload scratchbird
   ```

### "Authentication failed"

**Cause:** Wrong username or password.

**Solution:**
```bash
# Reset password
sb_security password username --new-password

# Or via SQL (as admin)
ALTER USER username WITH PASSWORD 'new_password';
```

### "Too many connections"

**Cause:** Connection limit reached.

**Solution:**
1. Check current connections:
   ```sql
   SELECT COUNT(*) FROM pg_stat_activity;
   ```

2. Terminate idle connections:
   ```sql
   SELECT pg_terminate_backend(pid)
   FROM pg_stat_activity
   WHERE state = 'idle'
     AND query_start < NOW() - INTERVAL '1 hour';
   ```

3. Increase limit in `sb_server.conf`:
   ```ini
   [server]
   max_connections = 200
   ```

### "SSL required"

**Cause:** Server requires SSL but client not using it.

**Solution:**
```bash
# Connect with SSL
psql "host=server sslmode=require dbname=mydb"
```

---

## Database Issues

### "Database does not exist"

**Cause:** Database not created or wrong name.

**Solution:**
```bash
# List databases
sb_isql -H localhost -c "\l"

# Create if needed
sb_isql -H localhost -c "CREATE DATABASE mydb"
```

### "Permission denied"

**Cause:** User lacks privileges.

**Solution:**
```sql
-- Grant access
GRANT CONNECT ON DATABASE mydb TO username;
GRANT USAGE ON SCHEMA public TO username;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO username;
```

### "Relation does not exist"

**Cause:** Table not found (wrong schema or name).

**Solution:**
```sql
-- List tables
\dt

-- Check schema
\dt *.tablename

-- Set search path
SET search_path TO myschema, public;
```

### "Disk full"

**Cause:** Data directory out of space.

**Solution:**
1. Check disk:
   ```bash
   df -h /var/lib/scratchbird
   ```

2. Free space:
   - Delete old backups
   - Run sweep/GC: `SWEEP;` (VACUUM alias)
   - Archive old data

3. Expand disk if needed

---

## Performance Issues

### Slow Queries

**Diagnosis:**
```sql
-- Find slow queries
SELECT
    pid,
    NOW() - query_start AS duration,
    query
FROM pg_stat_activity
WHERE state = 'active'
ORDER BY query_start;
```

**Solutions:**
1. Add missing indexes:
   ```sql
   EXPLAIN ANALYZE SELECT ...;  -- Look for Seq Scan
   CREATE INDEX idx_name ON table(column);
   ```

2. Update statistics:
   ```sql
   ANALYZE tablename;
   ```

3. Increase work_mem for sorts

### High CPU Usage

**Diagnosis:**
```bash
top -p $(pgrep sb_server)
```

**Solutions:**
1. Check for long-running queries
2. Add indexes to avoid sequential scans
3. Optimize complex queries

### High Memory Usage

**Diagnosis:**
```bash
ps aux | grep sb_server
```

**Solutions:**
1. Reduce `buffer_pool_size`
2. Reduce `work_mem`
3. Limit connections

### Lock Contention

**Diagnosis:**
```sql
SELECT
    blocked.pid AS blocked_pid,
    blocking.pid AS blocking_pid,
    blocked.query AS blocked_query
FROM pg_stat_activity blocked
JOIN pg_stat_activity blocking
    ON blocking.pid = ANY(pg_blocking_pids(blocked.pid));
```

**Solutions:**
1. Kill blocking query:
   ```sql
   SELECT pg_terminate_backend(blocking_pid);
   ```

2. Optimize transaction design
3. Add indexes to reduce lock duration

---

## Server Issues

### Server Won't Start

**Check logs:**
```bash
journalctl -u scratchbird -n 100
```

**Common causes:**

1. **Configuration error:**
   ```bash
   sb_server --config /etc/scratchbird/sb_server.conf --check
   ```

2. **Port in use:**
   ```bash
   ss -tlnp | grep 5432
   # Kill conflicting process or change port
   ```

3. **Permission denied:**
   ```bash
   ls -la /var/lib/scratchbird
   chown -R scratchbird:scratchbird /var/lib/scratchbird
   ```

4. **Corrupted data:**
   ```bash
   sb_verify /var/lib/scratchbird/mydb.sbdb --all
   ```

### Server Crashes

**Check core dump:**
```bash
coredumpctl list
coredumpctl info -1
```

**Report bug with:**
- ScratchBird version
- Operating system
- Error messages
- Steps to reproduce

### Out of Memory

**Symptoms:** Server killed by OOM killer.

**Check:**
```bash
dmesg | grep -i "out of memory"
journalctl -k | grep oom
```

**Solutions:**
1. Reduce memory settings:
   ```ini
   [memory]
   buffer_pool_size = 1GB
   work_mem = 4MB
   ```

2. Reduce max_connections
3. Add swap (temporary)
4. Increase RAM

---

## Data Issues

### Data Corruption

**Detection:**
```bash
sb_verify mydb --all
```

**Recovery:**
1. Restore from backup:
   ```bash
   sb_backup restore /backup/latest.sbdb mydb_recovered
   ```

2. If no backup, try recovery mode:
   ```bash
   sb_server --database /var/lib/scratchbird/mydb.sbdb --recovery
   ```

### Accidentally Deleted Data

**If within transaction:**
```sql
ROLLBACK;
```

**If committed:**
1. Restore from backup
2. Use point-in-time recovery (if WAL archiving enabled)

### Wrong Data Modified

```sql
-- If still in transaction
ROLLBACK;

-- Otherwise restore from backup
```

---

## Backup Issues

### Backup Fails

**Check disk space:**
```bash
df -h /backup
```

**Check permissions:**
```bash
ls -la /backup
```

**Run manually with verbose:**
```bash
sb_backup create mydb /backup/test.sbdb --verbose
```

### Restore Fails

**Verify backup integrity:**
```bash
sb_backup verify /backup/mydb.sbdb
```

**Check for conflicts:**
```bash
# Database exists?
sb_isql -c "\l" | grep mydb
```

---

## Network Issues

### Timeout Connecting

**Check firewall:**
```bash
# Linux
sudo ufw status
sudo firewall-cmd --list-ports

# Test port
nc -zv server 5432
```

### Intermittent Connections

**Check network:**
```bash
ping server
traceroute server
```

**Check server load:**
```bash
uptime
vmstat 1 5
```

---

## Logging

### Increase Log Verbosity

```ini
# sb_server.conf
[logging]
level = debug
```

### Find Specific Errors

```bash
grep -i error /var/log/scratchbird/sb_server.log
grep -i "failed" /var/log/scratchbird/sb_server.log
```

### Log Rotation Issues

Check logrotate config:
```bash
cat /etc/logrotate.d/scratchbird
```

---

## Getting Help

### Information to Gather

1. **Version:**
   ```sql
   SELECT version();
   ```

2. **Configuration:**
   ```sql
   SHOW ALL;
   ```

3. **Recent logs:**
   ```bash
   tail -100 /var/log/scratchbird/sb_server.log
   ```

4. **System info:**
   ```bash
   uname -a
   free -h
   df -h
   ```

### Where to Get Help

- [FAQ](../FAQ.md)
- Review server logs and error details

---

## Emergency Procedures

### Emergency Shutdown

```bash
# Graceful
sudo systemctl stop scratchbird

# If unresponsive
sudo kill -TERM $(cat /var/run/scratchbird/sb_server.pid)

# Last resort
sudo kill -9 $(pgrep sb_server)
```

### Kill All Connections

```sql
SELECT pg_terminate_backend(pid)
FROM pg_stat_activity
WHERE pid != pg_backend_pid();
```

### Read-Only Mode

```sql
-- Prevent writes during emergency
ALTER DATABASE mydb SET default_transaction_read_only = on;
```

---

## Next Steps

- [Monitoring](monitoring.md)
- [Backup and restore](backup-restore.md)
- [Performance tuning](performance-tuning.md)
