# Backup and Restore

**Status:** Alpha documentation
**Last Updated:** 2026-01-19

---

## Overview

ScratchBird provides comprehensive backup and restore capabilities for protecting your data. This guide covers backup strategies, tools, procedures, and disaster recovery planning.

**Topics covered:**
- Backup types and strategies
- Using sb_backup and sb_restore
- Automated backup scheduling
- Point-in-time recovery
- Disaster recovery procedures

---

## Part 1: Backup Fundamentals

### Backup Types

ScratchBird supports three backup types:

| Type | Description | Use Case | Recovery Time |
|------|-------------|----------|---------------|
| **Full** | Complete database copy | Weekly backups, initial backup | Longest |
| **Incremental** | Changes since last backup | Daily backups | Medium |
| **Continuous** | Write-ahead log archiving | Point-in-time recovery | Fastest |

### Backup Locations

```
/var/lib/scratchbird/
├── data/                    # Database files (back this up)
│   ├── base/               # Table data
│   ├── global/             # Cluster-wide data
│   └── pg_wal/             # Write-ahead logs
├── backups/                # Local backup storage
└── archive/                # WAL archive destination
```

### Storage Requirements

Estimate backup storage needs:

```sql
-- Check database size
SELECT
    datname AS database,
    pg_size_pretty(pg_database_size(datname)) AS size
FROM pg_database
WHERE datname NOT IN ('template0', 'template1');

-- Check table sizes
SELECT
    schemaname || '.' || tablename AS table_name,
    pg_size_pretty(pg_total_relation_size(schemaname || '.' || tablename)) AS total_size
FROM pg_tables
WHERE schemaname NOT IN ('pg_catalog', 'information_schema')
ORDER BY pg_total_relation_size(schemaname || '.' || tablename) DESC
LIMIT 10;
```

**Rule of thumb:** Plan for 3x database size for backup storage (full + incrementals + growth).

---

## Part 2: Using sb_backup

### Basic Usage

```bash
# Full database backup
sb_backup -U admin -d mydb -o /backups/mydb_full.sbk

# Backup with compression
sb_backup -U admin -d mydb -o /backups/mydb_full.sbk.gz --compress

# Backup specific tables
sb_backup -U admin -d mydb -t users -t orders -o /backups/tables.sbk

# Backup with progress
sb_backup -U admin -d mydb -o /backups/mydb_full.sbk --verbose --progress
```

### Command Options

| Option | Description | Example |
|--------|-------------|---------|
| `-U, --user` | Username | `-U admin` |
| `-d, --database` | Database name | `-d mydb` |
| `-o, --output` | Output file | `-o backup.sbk` |
| `-t, --table` | Specific table(s) | `-t users -t orders` |
| `-s, --schema` | Specific schema(s) | `-s public -s app` |
| `--compress` | Enable compression | `--compress` |
| `--parallel` | Parallel workers | `--parallel 4` |
| `--format` | Output format | `--format custom` |
| `--verbose` | Verbose output | `--verbose` |
| `--progress` | Show progress | `--progress` |

### Output Formats

```bash
# Custom format (recommended for large databases)
sb_backup -U admin -d mydb --format custom -o /backups/mydb.sbk

# Plain SQL (human-readable, for small databases)
sb_backup -U admin -d mydb --format plain -o /backups/mydb.sql

# Directory format (parallel backup/restore)
sb_backup -U admin -d mydb --format directory -o /backups/mydb_dir/

# Tar format (single file, no compression)
sb_backup -U admin -d mydb --format tar -o /backups/mydb.tar
```

### Parallel Backups

For large databases, use parallel backup:

```bash
# 4 parallel workers
sb_backup -U admin -d mydb --parallel 4 --format directory -o /backups/mydb_parallel/

# With compression (each file compressed separately)
sb_backup -U admin -d mydb --parallel 4 --compress --format directory -o /backups/mydb_parallel/
```

### Backup Verification

Always verify backups:

```bash
# List backup contents
sb_backup --list /backups/mydb.sbk

# Verify backup integrity
sb_backup --verify /backups/mydb.sbk

# Test restore to temporary location (doesn't affect production)
sb_restore --dry-run -d test_restore /backups/mydb.sbk
```

---

## Part 3: Using sb_restore

### Basic Usage

```bash
# Restore full database
sb_restore -U admin -d mydb /backups/mydb.sbk

# Restore to new database
createdb -U admin mydb_restored
sb_restore -U admin -d mydb_restored /backups/mydb.sbk

# Restore specific tables
sb_restore -U admin -d mydb -t users -t orders /backups/mydb.sbk

# Restore with progress
sb_restore -U admin -d mydb --verbose --progress /backups/mydb.sbk
```

### Command Options

| Option | Description | Example |
|--------|-------------|---------|
| `-U, --user` | Username | `-U admin` |
| `-d, --database` | Target database | `-d mydb` |
| `-t, --table` | Specific table(s) | `-t users` |
| `-s, --schema` | Specific schema(s) | `-s public` |
| `--parallel` | Parallel workers | `--parallel 4` |
| `--clean` | Drop objects before restore | `--clean` |
| `--create` | Create database | `--create` |
| `--data-only` | Restore data only | `--data-only` |
| `--schema-only` | Restore schema only | `--schema-only` |
| `--no-owner` | Skip ownership | `--no-owner` |
| `--dry-run` | Test without changes | `--dry-run` |

### Restore Strategies

**Strategy 1: Clean Restore (Replace existing)**
```bash
# Drop and recreate
sb_restore -U admin -d mydb --clean --create /backups/mydb.sbk
```

**Strategy 2: Merge Restore (Add to existing)**
```bash
# Restore data only, skip conflicts
sb_restore -U admin -d mydb --data-only --disable-triggers /backups/mydb.sbk
```

**Strategy 3: Schema Migration**
```bash
# Restore schema to new database, then migrate data
sb_restore -U admin -d mydb_new --schema-only /backups/mydb.sbk
# Then use custom migration scripts for data
```

**Strategy 4: Selective Restore**
```bash
# Restore only specific tables
sb_restore -U admin -d mydb -t orders -t order_items /backups/mydb.sbk
```

### Parallel Restore

```bash
# Restore with 4 parallel workers (requires directory format backup)
sb_restore -U admin -d mydb --parallel 4 /backups/mydb_dir/

# Parallel restore with index rebuild at end
sb_restore -U admin -d mydb --parallel 4 --jobs 4 /backups/mydb_dir/
```

---

## Part 4: Automated Backups

### Backup Script

Create `/usr/local/bin/sb_backup_daily.sh`:

```bash
#!/bin/bash
#
# ScratchBird Daily Backup Script
#

set -euo pipefail

# Configuration
BACKUP_DIR="/var/backups/scratchbird"
DB_USER="admin"
DB_NAME="production"
RETENTION_DAYS=30
LOG_FILE="/var/log/scratchbird/backup.log"

# Derived variables
DATE=$(date +%Y%m%d_%H%M%S)
DAY_OF_WEEK=$(date +%u)  # 1=Monday, 7=Sunday
BACKUP_FILE="${BACKUP_DIR}/${DB_NAME}_${DATE}.sbk.gz"

# Logging function
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

# Create backup directory
mkdir -p "$BACKUP_DIR"

log "Starting backup of ${DB_NAME}"

# Determine backup type
if [ "$DAY_OF_WEEK" -eq 7 ]; then
    # Sunday: Full backup
    BACKUP_TYPE="full"
    log "Performing FULL backup (weekly)"
else
    # Other days: Incremental
    BACKUP_TYPE="incremental"
    log "Performing INCREMENTAL backup (daily)"
fi

# Perform backup
if sb_backup -U "$DB_USER" -d "$DB_NAME" -o "$BACKUP_FILE" --compress --verbose >> "$LOG_FILE" 2>&1; then
    BACKUP_SIZE=$(du -h "$BACKUP_FILE" | cut -f1)
    log "Backup completed successfully: ${BACKUP_FILE} (${BACKUP_SIZE})"
else
    log "ERROR: Backup failed!"
    exit 1
fi

# Verify backup
log "Verifying backup integrity..."
if sb_backup --verify "$BACKUP_FILE" >> "$LOG_FILE" 2>&1; then
    log "Backup verification passed"
else
    log "ERROR: Backup verification failed!"
    exit 1
fi

# Cleanup old backups
log "Cleaning up backups older than ${RETENTION_DAYS} days..."
DELETED_COUNT=$(find "$BACKUP_DIR" -name "*.sbk*" -mtime +${RETENTION_DAYS} -delete -print | wc -l)
log "Deleted ${DELETED_COUNT} old backup files"

# Report storage usage
TOTAL_SIZE=$(du -sh "$BACKUP_DIR" | cut -f1)
log "Total backup storage: ${TOTAL_SIZE}"

log "Backup job completed"
```

Make executable:
```bash
chmod +x /usr/local/bin/sb_backup_daily.sh
```

### Cron Schedule

Edit crontab:
```bash
sudo crontab -e
```

Add entries:
```cron
# Daily backup at 2:00 AM
0 2 * * * /usr/local/bin/sb_backup_daily.sh

# Verify last backup at 6:00 AM
0 6 * * * sb_backup --verify /var/backups/scratchbird/$(ls -t /var/backups/scratchbird/*.sbk.gz | head -1) >> /var/log/scratchbird/backup-verify.log 2>&1
```

### Systemd Timer (Alternative)

Create `/etc/systemd/system/sb-backup.service`:
```ini
[Unit]
Description=ScratchBird Database Backup
After=scratchbird.service

[Service]
Type=oneshot
ExecStart=/usr/local/bin/sb_backup_daily.sh
User=scratchbird
Group=scratchbird
```

Create `/etc/systemd/system/sb-backup.timer`:
```ini
[Unit]
Description=Run ScratchBird backup daily

[Timer]
OnCalendar=*-*-* 02:00:00
Persistent=true
RandomizedDelaySec=300

[Install]
WantedBy=timers.target
```

Enable:
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now sb-backup.timer
```

---

## Part 5: Point-in-Time Recovery (PITR)

### Enable WAL Archiving

Edit `sb_server.conf`:
```ini
[wal]
# Enable archiving
archive_mode = on
archive_command = 'cp %p /var/lib/scratchbird/archive/%f'
archive_timeout = 300

# WAL settings for PITR
wal_level = replica
max_wal_senders = 3
wal_keep_size = 1GB
```

Restart server:
```bash
sudo systemctl restart scratchbird
```

### Create Base Backup

```bash
# Create base backup for PITR
sb_backup -U admin --wal -o /backups/base_backup.sbk

# Or use low-level backup
sb_isql -U admin -c "SELECT pg_start_backup('pitr_base', true)"
tar -cvf /backups/base.tar /var/lib/scratchbird/data/
sb_isql -U admin -c "SELECT pg_stop_backup()"
```

### Perform PITR

**Step 1: Stop the server**
```bash
sudo systemctl stop scratchbird
```

**Step 2: Clear data directory**
```bash
sudo rm -rf /var/lib/scratchbird/data/*
```

**Step 3: Restore base backup**
```bash
sb_restore --target-time="2026-01-19 14:30:00" -d postgres /backups/base_backup.sbk
```

**Step 4: Create recovery configuration**

Create `/var/lib/scratchbird/data/recovery.conf`:
```ini
restore_command = 'cp /var/lib/scratchbird/archive/%f %p'
recovery_target_time = '2026-01-19 14:30:00'
recovery_target_action = 'promote'
```

**Step 5: Start recovery**
```bash
sudo systemctl start scratchbird
# Monitor recovery progress in logs
sudo journalctl -u scratchbird -f
```

### Recovery Targets

```ini
# Recover to specific time
recovery_target_time = '2026-01-19 14:30:00'

# Recover to specific transaction
recovery_target_xid = '12345678'

# Recover to named restore point
recovery_target_name = 'before_migration'

# Recover to latest available
recovery_target = 'latest'
```

---

## Part 6: Remote Backups

### SSH/SCP Backup

```bash
# Direct backup to remote server
sb_backup -U admin -d mydb --compress | ssh backup-server "cat > /backups/mydb_$(date +%Y%m%d).sbk.gz"

# With progress
sb_backup -U admin -d mydb --compress --progress | pv | ssh backup-server "cat > /backups/mydb.sbk.gz"
```

### S3-Compatible Storage

Using AWS CLI or compatible tool:

```bash
#!/bin/bash
# Backup to S3

BUCKET="s3://my-backups/scratchbird"
DATE=$(date +%Y%m%d_%H%M%S)

# Create backup
sb_backup -U admin -d mydb -o /tmp/backup.sbk.gz --compress

# Upload to S3
aws s3 cp /tmp/backup.sbk.gz "${BUCKET}/mydb_${DATE}.sbk.gz"

# Cleanup local
rm /tmp/backup.sbk.gz

# Remove old backups (keep 30 days)
aws s3 ls "${BUCKET}/" | while read -r line; do
    FILE_DATE=$(echo "$line" | awk '{print $1}')
    if [[ $(date -d "$FILE_DATE" +%s) -lt $(date -d "30 days ago" +%s) ]]; then
        FILE_NAME=$(echo "$line" | awk '{print $4}')
        aws s3 rm "${BUCKET}/${FILE_NAME}"
    fi
done
```

### Backup to NFS

```bash
# Mount NFS share
sudo mount -t nfs backup-server:/backups /mnt/backups

# Backup directly to NFS
sb_backup -U admin -d mydb -o /mnt/backups/mydb_$(date +%Y%m%d).sbk.gz --compress
```

---

## Part 7: Backup Monitoring

### Backup Status Table

Create a backup tracking table:

```sql
CREATE TABLE IF NOT EXISTS sb_admin.backup_history (
    id SERIAL PRIMARY KEY,
    backup_type VARCHAR(20) NOT NULL,
    database_name VARCHAR(100) NOT NULL,
    file_path TEXT NOT NULL,
    file_size BIGINT,
    started_at TIMESTAMP WITH TIME ZONE NOT NULL,
    completed_at TIMESTAMP WITH TIME ZONE,
    duration_seconds INTEGER,
    status VARCHAR(20) NOT NULL,
    verified BOOLEAN DEFAULT FALSE,
    error_message TEXT
);

CREATE INDEX idx_backup_history_date ON sb_admin.backup_history(started_at);
```

### Logging Backups

Update backup script to log:

```bash
# At backup start
sb_isql -U admin -d admin -c "
INSERT INTO sb_admin.backup_history (backup_type, database_name, file_path, started_at, status)
VALUES ('full', 'mydb', '/backups/mydb.sbk', NOW(), 'running')
RETURNING id
" > /tmp/backup_id.txt

BACKUP_ID=$(cat /tmp/backup_id.txt | grep -oP '\d+')

# At backup completion
sb_isql -U admin -d admin -c "
UPDATE sb_admin.backup_history
SET completed_at = NOW(),
    file_size = $(stat -c%s /backups/mydb.sbk),
    duration_seconds = EXTRACT(EPOCH FROM NOW() - started_at),
    status = 'completed'
WHERE id = $BACKUP_ID
"
```

### Monitoring Queries

```sql
-- Recent backup status
SELECT
    backup_type,
    database_name,
    started_at,
    duration_seconds,
    pg_size_pretty(file_size) AS size,
    status,
    verified
FROM sb_admin.backup_history
ORDER BY started_at DESC
LIMIT 10;

-- Check for missing backups (no backup in 24 hours)
SELECT database_name
FROM (SELECT DISTINCT database_name FROM sb_admin.backup_history) dbs
WHERE NOT EXISTS (
    SELECT 1 FROM sb_admin.backup_history h
    WHERE h.database_name = dbs.database_name
    AND h.started_at > NOW() - INTERVAL '24 hours'
    AND h.status = 'completed'
);

-- Backup size trend
SELECT
    DATE_TRUNC('day', started_at) AS backup_date,
    database_name,
    pg_size_pretty(AVG(file_size)::bigint) AS avg_size,
    COUNT(*) AS backup_count
FROM sb_admin.backup_history
WHERE started_at > NOW() - INTERVAL '30 days'
GROUP BY DATE_TRUNC('day', started_at), database_name
ORDER BY backup_date DESC;
```

### Alerting

Create alert script `/usr/local/bin/sb_backup_alert.sh`:

```bash
#!/bin/bash
# Check backup status and send alerts

ALERT_EMAIL="dba@example.com"
SLACK_WEBHOOK="https://hooks.slack.com/services/XXX"

# Check for failed backups in last 24 hours
FAILED=$(sb_isql -U admin -d admin -t -c "
SELECT COUNT(*) FROM sb_admin.backup_history
WHERE started_at > NOW() - INTERVAL '24 hours'
AND status = 'failed'
")

if [ "$FAILED" -gt 0 ]; then
    MESSAGE="ALERT: $FAILED backup(s) failed in the last 24 hours"

    # Email alert
    echo "$MESSAGE" | mail -s "ScratchBird Backup Alert" "$ALERT_EMAIL"

    # Slack alert
    curl -X POST -H 'Content-type: application/json' \
        --data "{\"text\":\"$MESSAGE\"}" \
        "$SLACK_WEBHOOK"
fi

# Check for missing daily backups
MISSING=$(sb_isql -U admin -d admin -t -c "
SELECT COUNT(DISTINCT database_name)
FROM sb_admin.backup_history
WHERE NOT EXISTS (
    SELECT 1 FROM sb_admin.backup_history h2
    WHERE h2.database_name = sb_admin.backup_history.database_name
    AND h2.started_at > NOW() - INTERVAL '25 hours'
    AND h2.status = 'completed'
)
")

if [ "$MISSING" -gt 0 ]; then
    MESSAGE="WARNING: $MISSING database(s) have no backup in 25 hours"
    echo "$MESSAGE" | mail -s "ScratchBird Backup Warning" "$ALERT_EMAIL"
fi
```

---

## Part 8: Disaster Recovery

### DR Planning Checklist

- [ ] Document all databases and their criticality
- [ ] Define Recovery Time Objective (RTO) for each database
- [ ] Define Recovery Point Objective (RPO) for each database
- [ ] Establish backup frequency based on RPO
- [ ] Test restore procedures quarterly
- [ ] Document restore procedures step-by-step
- [ ] Train team on recovery procedures
- [ ] Store backups in geographically separate locations

### Recovery Runbook

**Scenario: Complete Server Failure**

1. **Assess damage and gather information**
   ```bash
   # What was the last successful backup?
   ls -la /backups/*.sbk* | tail -5

   # Check backup logs
   tail -100 /var/log/scratchbird/backup.log
   ```

2. **Provision new server**
   ```bash
   # Install ScratchBird
   apt install scratchbird

   # Copy configuration
   scp backup-server:/etc/scratchbird/sb_server.conf /etc/scratchbird/
   ```

3. **Restore from backup**
   ```bash
   # Stop server
   systemctl stop scratchbird

   # Restore
   sb_restore -U admin --create -d mydb /backups/latest.sbk

   # Start server
   systemctl start scratchbird
   ```

4. **Verify recovery**
   ```bash
   # Check database
   sb_isql -U admin -d mydb -c "SELECT COUNT(*) FROM critical_table"

   # Verify data integrity
   sb_isql -U admin -d mydb -c "SELECT * FROM sb_admin.integrity_check()"
   ```

5. **Update DNS/connections**
   - Update application connection strings
   - Update DNS records if applicable
   - Notify stakeholders

### Recovery Testing

Schedule quarterly recovery tests:

```bash
#!/bin/bash
# Quarterly DR Test Script

TEST_SERVER="dr-test-server"
BACKUP_FILE="/backups/production_latest.sbk"
TEST_DB="dr_test_$(date +%Y%m%d)"

echo "=== DR Test Started: $(date) ==="

# Create test database
ssh $TEST_SERVER "createdb -U admin $TEST_DB"

# Restore backup
ssh $TEST_SERVER "sb_restore -U admin -d $TEST_DB $BACKUP_FILE"

# Run verification queries
ssh $TEST_SERVER "sb_isql -U admin -d $TEST_DB -f /opt/dr_test/verify_queries.sql"

# Record results
ssh $TEST_SERVER "sb_isql -U admin -d $TEST_DB -c \"
SELECT 'users' AS table_name, COUNT(*) AS row_count FROM users
UNION ALL SELECT 'orders', COUNT(*) FROM orders
UNION ALL SELECT 'products', COUNT(*) FROM products
\""

# Cleanup
ssh $TEST_SERVER "dropdb -U admin $TEST_DB"

echo "=== DR Test Completed: $(date) ==="
```

---

## Part 9: Best Practices

### Backup Best Practices

1. **Follow 3-2-1 Rule**
   - 3 copies of data
   - 2 different storage types
   - 1 offsite location

2. **Test Restores Regularly**
   - Monthly: Verify backup files
   - Quarterly: Full restore test
   - Annually: Complete DR exercise

3. **Encrypt Sensitive Backups**
   ```bash
   # Encrypt backup with GPG
   sb_backup -U admin -d mydb | gpg --encrypt -r backup@company.com > /backups/mydb.sbk.gpg

   # Decrypt for restore
   gpg --decrypt /backups/mydb.sbk.gpg | sb_restore -U admin -d mydb
   ```

4. **Monitor Backup Growth**
   - Track backup sizes over time
   - Alert on unexpected growth
   - Plan storage capacity

5. **Document Everything**
   - Backup procedures
   - Restore procedures
   - Contact information
   - Escalation paths

### Common Mistakes to Avoid

| Mistake | Consequence | Prevention |
|---------|-------------|------------|
| Never testing restores | Discover corruption at worst time | Schedule regular tests |
| Single backup location | Loss in disaster | Use 3-2-1 rule |
| No monitoring | Miss failures | Implement alerts |
| Manual backups only | Human error, forgotten | Automate everything |
| No encryption | Data breach risk | Encrypt sensitive data |

---

## Quick Reference

### Common Commands

```bash
# Full backup
sb_backup -U admin -d mydb -o /backups/full.sbk --compress

# Incremental backup
sb_backup -U admin -d mydb -o /backups/incr.sbk --incremental

# Restore database
sb_restore -U admin -d mydb /backups/full.sbk

# Verify backup
sb_backup --verify /backups/full.sbk

# List backup contents
sb_backup --list /backups/full.sbk

# Point-in-time recovery
sb_restore --target-time="2026-01-19 14:30:00" -d mydb /backups/base.sbk
```

### Backup Schedule Template

| Day | Time | Type | Retention |
|-----|------|------|-----------|
| Sunday | 02:00 | Full | 4 weeks |
| Mon-Sat | 02:00 | Incremental | 7 days |
| Continuous | - | WAL Archive | 7 days |

---

## See Also

- [Monitoring](monitoring.md)
- [Security Administration](security.md)
- [Docker Deployment](../tutorials/Docker-Deployment.md)
- [Performance Tuning](../user-guides/Performance-Tuning.md)

