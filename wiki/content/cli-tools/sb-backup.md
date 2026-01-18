# sb_backup

Database backup and restore utility.

[Back to CLI Tools](README.md) | [Back to Home](../Home.md)

---

## Synopsis

```
sb_backup <command> [OPTIONS] [ARGS]
```

---

## Description

`sb_backup` creates and restores database backups. It supports full backups, incremental backups, compression, and verification.

---

## Commands

| Command | Description |
|---------|-------------|
| `create` | Create a backup |
| `restore` | Restore from backup |
| `list` | List backups in directory |
| `verify` | Verify backup integrity |
| `info` | Show backup metadata |

---

## Create Options

| Option | Description |
|--------|-------------|
| `--compress` | Compress backup |
| `--incremental` | Incremental backup |
| `--tables TABLE,...` | Backup specific tables |
| `--exclude TABLE,...` | Exclude tables |
| `--schema-only` | Schema without data |
| `--data-only` | Data without schema |
| `--verbose` | Show progress |

---

## Restore Options

| Option | Description |
|--------|-------------|
| `--overwrite` | Overwrite existing database |
| `--tables TABLE,...` | Restore specific tables |
| `--schema-only` | Restore schema only |
| `--data-only` | Restore data only |
| `--no-owner` | Don't restore ownership |
| `--verbose` | Show progress |

---

## Connection Options

| Option | Description |
|--------|-------------|
| `-H, --host HOST` | Server hostname |
| `-P, --port PORT` | Server port |
| `-U, --user USER` | Username |
| `-p, --password` | Prompt for password |

---

## Usage Examples

### Create Backup

```bash
# Basic backup
sb_backup create mydb /backup/mydb.sbdb

# Compressed backup
sb_backup create mydb /backup/mydb.sbdb --compress

# With progress
sb_backup create mydb /backup/mydb.sbdb --verbose
```

### Backup Specific Tables

```bash
# Only users and orders tables
sb_backup create mydb /backup/partial.sbdb --tables users,orders

# Exclude large tables
sb_backup create mydb /backup/small.sbdb --exclude logs,audit_trail
```

### Schema Only

```bash
# Just the schema, no data
sb_backup create mydb /backup/schema.sbdb --schema-only
```

### Incremental Backup

```bash
# First, create base backup
sb_backup create mydb /backup/base.sbdb

# Then incremental
sb_backup create mydb /backup/incr1.sbdb --incremental
```

---

## Restore Examples

### Basic Restore

```bash
# Restore to new database
sb_backup restore /backup/mydb.sbdb new_database

# Restore with overwrite
sb_backup restore /backup/mydb.sbdb existing_db --overwrite
```

### Restore Specific Tables

```bash
# Restore only users table
sb_backup restore /backup/mydb.sbdb mydb --tables users
```

### Schema Only Restore

```bash
# Restore just schema
sb_backup restore /backup/mydb.sbdb mydb --schema-only
```

---

## List and Verify

### List Backups

```bash
sb_backup list /backup/
```

Output:
```
Backups in /backup/:
  mydb_20240115.sbdb      2024-01-15 02:00:00   145 MB  Full
  mydb_20240116.sbdb      2024-01-16 02:00:00   12 MB   Incremental
  mydb_20240117.sbdb      2024-01-17 02:00:00   15 MB   Incremental
```

### Verify Backup

```bash
sb_backup verify /backup/mydb.sbdb
```

Output:
```
Verifying /backup/mydb.sbdb...
  Header: OK
  Checksum: OK
  Tables: 42
  Total size: 145 MB
Backup is valid.
```

### Backup Info

```bash
sb_backup info /backup/mydb.sbdb
```

Output:
```
Backup: /backup/mydb.sbdb
  Created: 2024-01-15 02:00:00
  Type: Full
  Database: mydb
  Tables: 42
  Size: 145 MB
  Compressed: Yes
  Checksum: sha256:abc123...
```

---

## Streaming Backups

### To stdout

```bash
# Stream backup (for piping)
sb_backup create mydb - --compress | gzip > backup.sbdb.gz
```

### From stdin

```bash
# Restore from stream
gunzip -c backup.sbdb.gz | sb_backup restore - mydb
```

### To Remote

```bash
# Backup directly to remote server
sb_backup create mydb - | ssh backup-server "cat > /backup/mydb.sbdb"
```

---

## Automation

### Daily Backup Script

```bash
#!/bin/bash
# /opt/scripts/daily_backup.sh

BACKUP_DIR=/backup/daily
DATE=$(date +%Y%m%d)
DATABASE=mydb
RETENTION=7

# Create backup
sb_backup create ${DATABASE} ${BACKUP_DIR}/${DATABASE}_${DATE}.sbdb --compress --verbose

# Verify
sb_backup verify ${BACKUP_DIR}/${DATABASE}_${DATE}.sbdb

# Rotate old backups
find ${BACKUP_DIR} -name "*.sbdb" -mtime +${RETENTION} -delete

echo "Backup completed: ${DATABASE}_${DATE}.sbdb"
```

### Cron Schedule

```bash
# Daily at 2 AM
0 2 * * * /opt/scripts/daily_backup.sh >> /var/log/backup.log 2>&1
```

---

## Backup Strategies

### Full Backup Weekly

```bash
# Sunday full backup
0 2 * * 0 sb_backup create mydb /backup/weekly/full_$(date +\%Y\%m\%d).sbdb --compress
```

### Daily Incremental

```bash
# Monday-Saturday incremental
0 2 * * 1-6 sb_backup create mydb /backup/daily/incr_$(date +\%Y\%m\%d).sbdb --incremental
```

### Monthly Archive

```bash
# First of month, archive to cold storage
0 3 1 * * tar czf /archive/monthly_$(date +\%Y\%m).tar.gz /backup/weekly/*.sbdb
```

---

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | General error |
| 2 | Connection error |
| 3 | File error |
| 4 | Verification failed |
| 5 | Database exists (restore without --overwrite) |

---

## Troubleshooting

### "Permission denied"

```bash
# Check write permission
ls -la /backup/
# Fix if needed
sudo chown -R scratchbird:scratchbird /backup/
```

### "Database exists"

```bash
# Use --overwrite
sb_backup restore backup.sbdb mydb --overwrite

# Or restore to different name
sb_backup restore backup.sbdb mydb_restored
```

### "Backup corrupted"

```bash
# Verify to see details
sb_backup verify backup.sbdb

# If corrupted, use previous backup
```

---

## Best Practices

1. **Verify backups** - Always verify after creation
2. **Test restores** - Regularly test restore process
3. **Multiple locations** - Store backups off-site
4. **Retention policy** - Balance storage vs. recovery needs
5. **Compress large backups** - Saves space and transfer time
6. **Monitor completion** - Alert on backup failures

---

## See Also

- [Backup and Restore Guide](../admin/backup-restore.md)
- [sb_verify](sb-verify.md)
- [Automation](../admin/monitoring.md)
