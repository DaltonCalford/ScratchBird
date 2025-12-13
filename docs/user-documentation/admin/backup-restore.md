# Backup and Restore

Protect your data with regular backups.

[Back to Admin Index](index.md) | [Back to Documentation Index](../index.md)

---

## Backup Methods

| Method | Tool | Use Case |
|--------|------|----------|
| **Logical** | sb_backup, pg_dump | Cross-version, portable |
| **Physical** | File copy | Fast, full server backup |
| **Continuous** | WAL archiving | Point-in-time recovery |

---

## Using sb_backup

### Create Backup

```bash
# Basic backup
sb_backup create mydb /backup/mydb_backup.sbdb

# With compression
sb_backup create mydb /backup/mydb_backup.sbdb --compress

# Specific tables only
sb_backup create mydb /backup/partial.sbdb --tables users,orders
```

### Restore Backup

```bash
# Restore to new database
sb_backup restore /backup/mydb_backup.sbdb new_database

# Restore with overwrite
sb_backup restore /backup/mydb_backup.sbdb existing_db --overwrite
```

### List Backups

```bash
sb_backup list /backup/
```

### Verify Backup

```bash
sb_backup verify /backup/mydb_backup.sbdb
```

---

## Using pg_dump (Compatible)

ScratchBird supports PostgreSQL dump tools.

### Create Dump

```bash
# Plain SQL format
pg_dump -h localhost -p 5432 -U admin mydb > backup.sql

# Custom format (compressed)
pg_dump -h localhost -p 5432 -U admin -Fc mydb > backup.dump

# Directory format (parallel)
pg_dump -h localhost -p 5432 -U admin -Fd -j4 mydb -f backup_dir/
```

### Restore Dump

```bash
# Plain SQL
psql -h localhost -p 5432 -U admin -d newdb < backup.sql

# Custom format
pg_restore -h localhost -p 5432 -U admin -d newdb backup.dump

# Directory format (parallel)
pg_restore -h localhost -p 5432 -U admin -d newdb -j4 backup_dir/
```

---

## Physical Backups

### Stop Server Method

```bash
# Stop server
sudo systemctl stop scratchbird

# Copy data directory
sudo cp -r /var/lib/scratchbird /backup/scratchbird_$(date +%Y%m%d)

# Restart server
sudo systemctl start scratchbird
```

### Hot Backup (Online)

```bash
# Lock for backup
psql -c "SELECT pg_start_backup('backup_label');"

# Copy files
sudo rsync -a /var/lib/scratchbird/ /backup/hot_backup/

# Unlock
psql -c "SELECT pg_stop_backup();"
```

---

## Backup Strategies

### Daily Backups

```bash
#!/bin/bash
# /opt/scripts/daily_backup.sh

BACKUP_DIR=/backup/daily
DATE=$(date +%Y%m%d)
RETENTION_DAYS=7

# Create backup
sb_backup create mydb ${BACKUP_DIR}/mydb_${DATE}.sbdb --compress

# Remove old backups
find ${BACKUP_DIR} -name "*.sbdb" -mtime +${RETENTION_DAYS} -delete

# Log completion
echo "$(date): Backup completed" >> /var/log/backup.log
```

Schedule with cron:
```bash
# Daily at 2 AM
0 2 * * * /opt/scripts/daily_backup.sh
```

### Weekly Full + Daily Incremental

```bash
#!/bin/bash
# Weekly full backup (Sunday)
if [ $(date +%u) -eq 7 ]; then
    sb_backup create mydb /backup/weekly/full_$(date +%Y%m%d).sbdb
fi

# Daily incremental
sb_backup create mydb /backup/daily/incr_$(date +%Y%m%d).sbdb --incremental
```

### Monthly Archives

```bash
# Monthly archive to off-site storage
0 0 1 * * tar czf /backup/monthly/archive_$(date +%Y%m).tar.gz /backup/weekly/*.sbdb
```

---

## Point-in-Time Recovery

### Enable WAL Archiving

In `sb_server.conf`:

```ini
[wal]
archive_mode = on
archive_command = 'cp %p /backup/wal/%f'
```

### Recover to Point in Time

```bash
# Stop server
sudo systemctl stop scratchbird

# Restore base backup
sb_backup restore /backup/base.sbdb mydb

# Configure recovery
cat > /var/lib/scratchbird/recovery.conf << EOF
restore_command = 'cp /backup/wal/%f %p'
recovery_target_time = '2024-01-15 14:30:00'
EOF

# Start server
sudo systemctl start scratchbird
```

---

## Remote Backups

### SSH/SCP

```bash
# Backup directly to remote
sb_backup create mydb - --compress | ssh backup-server "cat > /backup/mydb.sbdb"

# Or with scp
sb_backup create mydb /tmp/mydb.sbdb
scp /tmp/mydb.sbdb backup-server:/backup/
rm /tmp/mydb.sbdb
```

### S3-Compatible Storage

```bash
# Using AWS CLI
sb_backup create mydb /tmp/mydb.sbdb --compress
aws s3 cp /tmp/mydb.sbdb s3://my-bucket/backups/mydb_$(date +%Y%m%d).sbdb
rm /tmp/mydb.sbdb
```

---

## Restore Scenarios

### Complete Database Loss

```bash
# Install ScratchBird (if needed)
# ...

# Restore from latest backup
sb_backup restore /backup/mydb_latest.sbdb mydb

# Verify
sb_verify mydb --all
```

### Single Table Recovery

```bash
# Extract table from backup
sb_backup restore /backup/mydb.sbdb temp_db --tables customers

# Copy data to production
pg_dump -h localhost -t customers temp_db | \
    psql -h localhost production_db

# Cleanup
DROP DATABASE temp_db;
```

### Point-in-Time Recovery

```bash
# 1. Restore base backup
# 2. Apply WAL logs up to target time
# 3. Verify data
```

---

## Verification

### Verify Backup Integrity

```bash
sb_backup verify /backup/mydb.sbdb
```

### Test Restore

```bash
# Restore to test database
sb_backup restore /backup/mydb.sbdb test_restore

# Verify data
sb_verify test_restore --all

# Compare row counts
psql -c "SELECT COUNT(*) FROM important_table" test_restore
psql -c "SELECT COUNT(*) FROM important_table" production

# Cleanup
DROP DATABASE test_restore;
```

### Monthly Restore Drill

Schedule regular restore tests:

```bash
#!/bin/bash
# Monthly restore test
DATE=$(date +%Y%m%d)

# Find latest backup
LATEST=$(ls -t /backup/daily/*.sbdb | head -1)

# Restore to test
sb_backup restore ${LATEST} restore_test_${DATE}

# Run verification
sb_verify restore_test_${DATE} --all > /var/log/restore_test_${DATE}.log

# Check critical tables
psql -d restore_test_${DATE} -c "SELECT COUNT(*) FROM users"

# Cleanup
psql -c "DROP DATABASE restore_test_${DATE}"

# Send report
mail -s "Restore Test ${DATE}" admin@example.com < /var/log/restore_test_${DATE}.log
```

---

## Monitoring Backups

### Check Last Backup Time

```bash
# List recent backups
ls -lt /backup/daily/ | head -5
```

### Backup Size Tracking

```bash
# Track backup sizes
du -sh /backup/daily/*.sbdb | tee -a /var/log/backup_sizes.log
```

### Alert on Missing Backup

```bash
#!/bin/bash
# Check backup exists for today
TODAY=$(date +%Y%m%d)
if [ ! -f /backup/daily/mydb_${TODAY}.sbdb ]; then
    echo "ALERT: No backup found for ${TODAY}" | \
        mail -s "Backup Missing" admin@example.com
fi
```

---

## Storage Considerations

### Compression

```bash
# Built-in compression
sb_backup create mydb backup.sbdb --compress

# External compression
sb_backup create mydb - | gzip > backup.sbdb.gz
```

### Encryption

```bash
# Encrypt backup
sb_backup create mydb - | \
    gpg --encrypt --recipient admin@example.com > backup.sbdb.gpg

# Decrypt for restore
gpg --decrypt backup.sbdb.gpg | sb_backup restore - mydb
```

### Retention Policy

| Type | Retention |
|------|-----------|
| Daily | 7 days |
| Weekly | 4 weeks |
| Monthly | 12 months |
| Yearly | 7 years |

---

## Best Practices

1. **Test restores regularly** - Backup is useless if restore fails
2. **Multiple locations** - Store backups off-site
3. **Encryption** - Protect sensitive data
4. **Monitoring** - Alert on backup failures
5. **Documentation** - Document restore procedures
6. **Retention policy** - Balance storage vs. recovery needs
7. **Verify integrity** - Check backups aren't corrupted

---

## Next Steps

- [Database verification](../tools/sb-verify.md)
- [Monitoring](monitoring.md)
- [Disaster recovery planning](troubleshooting.md)
