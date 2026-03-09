# Backup Types and Schedule Model

[Backup Architecture README](../README.md) | [Disaster Recovery README](../../README.md)

## Synopsis

ScratchBird provides multiple backup strategies leveraging MGA's inherent point-in-time recovery capabilities.

## Backup Types

### 1. Physical Backup

Copies database files at the storage level.

```bash
# Cold backup (database stopped)
sb_backup --type=physical --mode=cold --destination=/backup/

# Hot backup (database running, MGA guarantees consistency)
sb_backup --type=physical --mode=hot --destination=/backup/
```

**Characteristics:**
- Fast restore
- Complete database image
- MGA enables hot backups without locks

### 2. Logical Backup

Exports SQL DDL and data.

```bash
# Single database
sb_backup --type=logical --database=mydb --destination=mydb.sql

# All databases in environment
sb_backup --type=logical --environment=!:prod --destination=/backup/
```

**Characteristics:**
- Portable across versions
- Selective restore
- Human-readable

### 3. Incremental Backup (MGA-based)

Backs up only changed pages since last backup.

```bash
# Incremental backup
sb_backup --type=incremental --base-backup=/backup/base/ --destination=/backup/incr/
```

**Characteristics:**
- Very efficient (MGA tracks changes)
- Fast backup/restore
- Supports point-in-time recovery

### 4. Snapshot Backup

MGA-native snapshot at specific transaction.

```sql
-- Create named snapshot
CREATE SNAPSHOT quarterly_backup AS OF TRANSACTION 12345678;

-- Backup snapshot
sb_backup --type=snapshot --snapshot=quarterly_backup --destination=/backup/
```

## Backup Scheduling

### Recommended Schedule

| Backup Type | Frequency | Retention |
|-------------|-----------|-----------|
| Full Physical | Weekly | 4 weeks |
| Incremental | Daily | 7 days |
| Snapshot | Before major changes | Indefinite |
| Logical | Monthly | 12 months |

### Automated Schedule Configuration

```sql
-- Create backup job
CREATE JOB daily_incremental AS $$
BEGIN
    PERFORM sb_backup(
        type => 'incremental',
        destination => '/backup/daily/' || CURRENT_DATE,
        compress => true
    );
END;
$$;

-- Schedule the job
CREATE SCHEDULE daily_backup
    FOR JOB daily_incremental
    EVERY '1 day' AT '02:00';
```

## Point-in-Time Recovery (PITR)

MGA enables recovery to any transaction.

```bash
# Restore to specific timestamp
sb_restore --backup=/backup/base/ --to-timestamp='2024-01-15 14:30:00'

# Restore to specific transaction
sb_restore --backup=/backup/base/ --to-transaction=12345678
```

## Backup Verification

```bash
# Verify backup integrity
sb_verify --backup=/backup/weekly-2024-01-15/

# Test restore to temporary location
sb_restore --backup=/backup/weekly/ --destination=/tmp/test-restore --verify-only
```

## Storage Considerations

### Compression

```bash
# LZ4 compression (fast)
sb_backup --compress=lz4 --destination=/backup/

# Zstandard compression (better ratio)
sb_backup --compress=zstd --level=3 --destination=/backup/
```

### Encryption

```bash
# Encrypt backup
sb_backup --encrypt=aes-256-gcm --key-id=backup-key --destination=/backup/
```

### Remote Storage

```bash
# S3 backup
sb_backup --type=physical --destination=s3://bucket/backups/

# Azure Blob
sb_backup --type=physical --destination=azure://container/backups/
```

## Examples

### Complete Backup Strategy

```bash
#!/bin/bash

# Weekly full backup (Sunday 1 AM)
0 1 * * 0 sb_backup --type=physical --destination=/backup/full/$(date +%Y%m%d)

# Daily incremental (2 AM)
0 2 * * 1-6 sb_backup --type=incremental \
    --base-backup=/backup/full/$(ls -t /backup/full | head -1) \
    --destination=/backup/incremental/$(date +%Y%m%d)

# Monthly logical
0 3 1 * * sb_backup --type=logical --destination=/backup/logical/$(date +%Y%m)
```

### Environment Backup

```bash
# Backup entire production environment
sb_backup --type=physical \
    --environment=!:prod \
    --include-emulated=true \
    --destination=/backup/prod-$(date +%Y%m%d)
```

## Best Practices

1. **3-2-1 Rule**: 3 copies, 2 media types, 1 offsite
2. **Test Restores**: Verify backups monthly
3. **Monitor**: Alert on backup failures
4. **Document**: Keep recovery procedures updated

## See Also

- [Restore Runbooks](../restore_runbooks/01_single_database_restore.md)
- [Point-in-Time Restore](../restore_runbooks/02_point_in_time_restore.md)
