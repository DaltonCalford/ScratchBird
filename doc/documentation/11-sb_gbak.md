# sb_gbak - Backup & Restore Utility 🟢

sb_gbak is ScratchBird's enhanced backup and restore utility that provides reliable database backup, restore, and validation operations. It offers 100% compatibility with Firebird's GBAK while adding modern features like parallel processing, compression, encryption, and intelligent validation.

## 🚀 Quick Start

### **Basic Backup Operations**
```bash
# Create database backup
sb_gbak -backup -user SYSDBA -password masterkey mydatabase.fdb backup.fbk

# Create compressed backup
sb_gbak -backup -compress -user SYSDBA mydatabase.fdb backup.fbk

# Backup with progress monitoring
sb_gbak -backup -verbose -user SYSDBA mydatabase.fdb backup.fbk
```

### **Basic Restore Operations**
```bash
# Restore database from backup
sb_gbak -restore -user SYSDBA backup.fbk newdatabase.fdb

# Restore with different page size
sb_gbak -restore -page_size 16384 -user SYSDBA backup.fbk newdatabase.fdb

# Replace existing database
sb_gbak -restore -replace -user SYSDBA backup.fbk existing.fdb
```

---

## 📋 Command Reference

### **Operation Modes**
| Option | Description | Example |
|--------|-------------|---------|
| `-backup` | Create backup | `-backup database.fdb backup.fbk` |
| `-restore` | Restore database | `-restore backup.fbk database.fdb` |
| `-create` | Create database from backup | `-create backup.fbk database.fdb` |
| `-replace` | Replace existing database | `-replace backup.fbk database.fdb` |

### **Backup Options**
| Option | Description | Example |
|--------|-------------|---------|
| `-compress` | Enable compression | `-compress` |
| `-no_data` | Backup metadata only | `-no_data` |
| `-no_triggers` | Skip trigger definitions | `-no_triggers` |
| `-no_validity` | Skip validation | `-no_validity` |
| `-ignore_checksums` | Ignore checksum errors | `-ignore_checksums` |
| `-limbo` | Backup limbo transactions | `-limbo` |

### **Restore Options**
| Option | Description | Example |
|--------|-------------|---------|
| `-page_size <size>` | Set page size (4096, 8192, 16384) | `-page_size 16384` |
| `-buffers <count>` | Set buffer count | `-buffers 2048` |
| `-no_shadow` | Don't restore shadow files | `-no_shadow` |
| `-no_validity` | Skip constraint validation | `-no_validity` |
| `-one_at_a_time` | Restore one table at a time | `-one_at_a_time` |
| `-use_all_space` | Use all space on data pages | `-use_all_space` |

### **Enhanced Options** (ScratchBird Features)
| Option | Description | Example |
|--------|-------------|---------|
| `-parallel <threads>` | Enable parallel processing | `-parallel 4` |
| `-encrypt <key>` | Encrypt backup file | `-encrypt mySecretKey` |
| `-decrypt <key>` | Decrypt backup file | `-decrypt mySecretKey` |
| `-validate` | Validate backup integrity | `-validate backup.fbk` |
| `-schema <name>` | Backup specific schema | `-schema finance.accounting` |
| `-progress` | Show detailed progress | `-progress` |

### **Connection Options**
| Option | Description | Example |
|--------|-------------|---------|
| `-user <username>` | Database username | `-user SYSDBA` |
| `-password <password>` | Database password | `-password masterkey` |
| `-role <role>` | SQL role name | `-role DB_ADMIN` |
| `-trusted` | Use trusted authentication | `-trusted` |

### **Output Options**
| Option | Description | Example |
|--------|-------------|---------|
| `-verbose` | Verbose output | `-verbose` |
| `-stats` | Show statistics | `-stats` |
| `-log <file>` | Write log to file | `-log backup.log` |
| `-quiet` | Minimal output | `-quiet` |

---

## 🔧 Advanced Features

### **Parallel Processing**
ScratchBird's enhanced sb_gbak can utilize multiple CPU cores for faster operations:

```bash
# Use 4 threads for backup
sb_gbak -backup -parallel 4 -user SYSDBA large_database.fdb backup.fbk

# Use optimal thread count (auto-detected)
sb_gbak -backup -parallel auto -user SYSDBA database.fdb backup.fbk

# Parallel restore with progress monitoring
sb_gbak -restore -parallel 4 -progress -user SYSDBA backup.fbk database.fdb
```

### **Compression Options**
Multiple compression algorithms available:

```bash
# Standard compression (LZ4 - fast)
sb_gbak -backup -compress -user SYSDBA database.fdb backup.fbk

# High compression (ZSTD - smaller files)
sb_gbak -backup -compress high -user SYSDBA database.fdb backup.fbk

# Maximum compression (BZIP2 - smallest files)
sb_gbak -backup -compress max -user SYSDBA database.fdb backup.fbk

# Compare compression levels
sb_gbak -backup -compress benchmark -user SYSDBA database.fdb backup.fbk
```

### **Backup Encryption**
Secure your backups with encryption:

```bash
# Encrypt backup with password
sb_gbak -backup -encrypt "MySecretPassword123" -user SYSDBA database.fdb backup.fbk

# Encrypt with key file
sb_gbak -backup -encrypt-keyfile encryption.key -user SYSDBA database.fdb backup.fbk

# Restore encrypted backup
sb_gbak -restore -decrypt "MySecretPassword123" -user SYSDBA backup.fbk database.fdb
```

### **Schema-Specific Backups**
Backup specific schemas in hierarchical databases:

```bash
# Backup specific schema only
sb_gbak -backup -schema "finance.accounting" -user SYSDBA database.fdb finance_backup.fbk

# Backup multiple schemas
sb_gbak -backup -schema "finance.*" -user SYSDBA database.fdb finance_all_backup.fbk

# Exclude specific schemas
sb_gbak -backup -exclude-schema "temp.*" -user SYSDBA database.fdb production_backup.fbk
```

### **Backup Validation**
Enhanced validation capabilities:

```bash
# Validate backup file integrity
sb_gbak -validate backup.fbk

# Deep validation (check all data)
sb_gbak -validate -deep backup.fbk

# Validate during backup creation
sb_gbak -backup -validate-on-create -user SYSDBA database.fdb backup.fbk

# Compare backup with source database
sb_gbak -validate -compare-source database.fdb backup.fbk
```

---

## 💼 Real-World Examples

### **Daily Backup Script**
```bash
#!/bin/bash
# daily_backup.sh - Automated daily backup script

DB_NAME="production.fdb"
BACKUP_DIR="/backups/daily"
DATE_STAMP=$(date +%Y%m%d_%H%M%S)
BACKUP_FILE="$BACKUP_DIR/production_$DATE_STAMP.fbk"
LOG_FILE="$BACKUP_DIR/backup_$DATE_STAMP.log"

# Ensure backup directory exists
mkdir -p "$BACKUP_DIR"

echo "Starting daily backup at $(date)" >> "$LOG_FILE"

# Create compressed, encrypted backup with validation
sb_gbak -backup \
    -user SYSDBA \
    -password "$(cat ~/.db_password)" \
    -compress high \
    -encrypt "$(cat ~/.backup_encryption_key)" \
    -parallel auto \
    -validate-on-create \
    -verbose \
    -log "$LOG_FILE" \
    "$DB_NAME" \
    "$BACKUP_FILE"

if [ $? -eq 0 ]; then
    echo "Backup completed successfully at $(date)" >> "$LOG_FILE"
    
    # Validate the backup
    sb_gbak -validate -deep "$BACKUP_FILE" >> "$LOG_FILE" 2>&1
    
    if [ $? -eq 0 ]; then
        echo "Backup validation passed" >> "$LOG_FILE"
        
        # Clean up backups older than 30 days
        find "$BACKUP_DIR" -name "production_*.fbk" -mtime +30 -delete
        
        # Send success notification
        echo "Daily backup completed successfully" | \
            mail -s "Backup Success - $(date +%Y-%m-%d)" admin@company.com
    else
        echo "ERROR: Backup validation failed" >> "$LOG_FILE"
        # Send failure notification
        mail -s "Backup Validation Failed - $(date +%Y-%m-%d)" \
             admin@company.com < "$LOG_FILE"
    fi
else
    echo "ERROR: Backup failed at $(date)" >> "$LOG_FILE"
    # Send failure notification
    mail -s "Backup Failed - $(date +%Y-%m-%d)" \
         admin@company.com < "$LOG_FILE"
fi
```

### **Disaster Recovery Script**
```bash
#!/bin/bash
# disaster_recovery.sh - Emergency database restoration

BACKUP_FILE="$1"
TARGET_DB="$2"
RECOVERY_LOG="/var/log/disaster_recovery_$(date +%Y%m%d_%H%M%S).log"

if [ $# -ne 2 ]; then
    echo "Usage: $0 <backup_file> <target_database>"
    exit 1
fi

echo "=== DISASTER RECOVERY STARTED ===" >> "$RECOVERY_LOG"
echo "Backup file: $BACKUP_FILE" >> "$RECOVERY_LOG"
echo "Target database: $TARGET_DB" >> "$RECOVERY_LOG"
echo "Started at: $(date)" >> "$RECOVERY_LOG"

# Validate backup file first
echo "Validating backup file..." >> "$RECOVERY_LOG"
sb_gbak -validate -deep "$BACKUP_FILE" >> "$RECOVERY_LOG" 2>&1

if [ $? -ne 0 ]; then
    echo "ERROR: Backup file validation failed" >> "$RECOVERY_LOG"
    echo "Recovery aborted" >> "$RECOVERY_LOG"
    exit 1
fi

# Backup existing database if it exists
if [ -f "$TARGET_DB" ]; then
    EMERGENCY_BACKUP="${TARGET_DB}.emergency_$(date +%Y%m%d_%H%M%S).fbk"
    echo "Creating emergency backup of existing database..." >> "$RECOVERY_LOG"
    
    sb_gbak -backup -ignore_checksums -user SYSDBA \
        "$TARGET_DB" "$EMERGENCY_BACKUP" >> "$RECOVERY_LOG" 2>&1
    
    if [ $? -eq 0 ]; then
        echo "Emergency backup created: $EMERGENCY_BACKUP" >> "$RECOVERY_LOG"
    else
        echo "WARNING: Could not create emergency backup" >> "$RECOVERY_LOG"
    fi
fi

# Restore database
echo "Restoring database from backup..." >> "$RECOVERY_LOG"
sb_gbak -restore \
    -replace \
    -parallel auto \
    -progress \
    -user SYSDBA \
    -password "$(cat ~/.db_password)" \
    -decrypt "$(cat ~/.backup_encryption_key)" \
    -verbose \
    "$BACKUP_FILE" \
    "$TARGET_DB" >> "$RECOVERY_LOG" 2>&1

if [ $? -eq 0 ]; then
    echo "Database restoration completed successfully" >> "$RECOVERY_LOG"
    
    # Validate restored database
    echo "Validating restored database..." >> "$RECOVERY_LOG"
    sb_gfix -validate "$TARGET_DB" >> "$RECOVERY_LOG" 2>&1
    
    if [ $? -eq 0 ]; then
        echo "=== DISASTER RECOVERY COMPLETED SUCCESSFULLY ===" >> "$RECOVERY_LOG"
        echo "Recovery completed at: $(date)" >> "$RECOVERY_LOG"
        
        # Send success notification
        echo "Disaster recovery completed successfully" | \
            mail -s "Recovery Success - $(date)" admin@company.com
    else
        echo "ERROR: Restored database validation failed" >> "$RECOVERY_LOG"
        mail -s "Recovery Validation Failed - $(date)" \
             admin@company.com < "$RECOVERY_LOG"
    fi
else
    echo "ERROR: Database restoration failed" >> "$RECOVERY_LOG"
    echo "=== DISASTER RECOVERY FAILED ===" >> "$RECOVERY_LOG"
    
    # Send failure notification
    mail -s "Disaster Recovery Failed - $(date)" \
         admin@company.com < "$RECOVERY_LOG"
    exit 1
fi
```

### **Database Migration Script**
```bash
#!/bin/bash
# migrate_database.sh - Migrate database to new server/version

SOURCE_DB="$1"
TARGET_SERVER="$2"
TARGET_DB="$3"
MIGRATION_DIR="/tmp/migration_$(date +%Y%m%d_%H%M%S)"

# Create migration workspace
mkdir -p "$MIGRATION_DIR"
cd "$MIGRATION_DIR"

echo "=== DATABASE MIGRATION STARTED ==="
echo "Source: $SOURCE_DB"
echo "Target: $TARGET_SERVER:$TARGET_DB"
echo "Workspace: $MIGRATION_DIR"

# Step 1: Create backup of source database
echo "Step 1: Creating backup of source database..."
sb_gbak -backup \
    -user SYSDBA \
    -password "$(cat ~/.db_password)" \
    -compress high \
    -parallel auto \
    -verbose \
    -stats \
    "$SOURCE_DB" \
    "source_backup.fbk"

if [ $? -ne 0 ]; then
    echo "ERROR: Source backup failed"
    exit 1
fi

# Step 2: Validate backup
echo "Step 2: Validating backup..."
sb_gbak -validate -deep "source_backup.fbk"

if [ $? -ne 0 ]; then
    echo "ERROR: Backup validation failed"
    exit 1
fi

# Step 3: Transfer backup to target server (if remote)
if [ "$TARGET_SERVER" != "localhost" ]; then
    echo "Step 3: Transferring backup to target server..."
    scp "source_backup.fbk" "$TARGET_SERVER:$MIGRATION_DIR/"
    
    if [ $? -ne 0 ]; then
        echo "ERROR: Backup transfer failed"
        exit 1
    fi
fi

# Step 4: Restore on target server
echo "Step 4: Restoring database on target server..."
if [ "$TARGET_SERVER" = "localhost" ]; then
    sb_gbak -restore \
        -user SYSDBA \
        -password "$(cat ~/.db_password)" \
        -page_size 16384 \
        -parallel auto \
        -verbose \
        "source_backup.fbk" \
        "$TARGET_DB"
else
    ssh "$TARGET_SERVER" "
        sb_gbak -restore \
            -user SYSDBA \
            -password \"\$(cat ~/.db_password)\" \
            -page_size 16384 \
            -parallel auto \
            -verbose \
            \"$MIGRATION_DIR/source_backup.fbk\" \
            \"$TARGET_DB\"
    "
fi

if [ $? -ne 0 ]; then
    echo "ERROR: Database restore failed"
    exit 1
fi

# Step 5: Validate migrated database
echo "Step 5: Validating migrated database..."
if [ "$TARGET_SERVER" = "localhost" ]; then
    sb_gfix -validate "$TARGET_DB"
else
    ssh "$TARGET_SERVER" "sb_gfix -validate \"$TARGET_DB\""
fi

if [ $? -eq 0 ]; then
    echo "=== DATABASE MIGRATION COMPLETED SUCCESSFULLY ==="
    echo "Database successfully migrated to $TARGET_SERVER:$TARGET_DB"
    
    # Cleanup
    rm -rf "$MIGRATION_DIR"
    
    # Send notification
    echo "Database migration completed successfully" | \
        mail -s "Migration Success - $(date)" admin@company.com
else
    echo "ERROR: Migrated database validation failed"
    exit 1
fi
```

---

## 🔍 Performance Optimization

### **Backup Performance Tips**
```bash
# Optimize for speed (large databases)
sb_gbak -backup \
    -parallel auto \
    -compress \
    -no_validity \
    -buffers 10000 \
    -user SYSDBA \
    large_database.fdb \
    backup.fbk

# Optimize for size (archival backups)
sb_gbak -backup \
    -compress max \
    -no_triggers \
    -user SYSDBA \
    database.fdb \
    archive_backup.fbk

# Optimize for integrity (critical backups)
sb_gbak -backup \
    -validate-on-create \
    -parallel 2 \
    -stats \
    -verbose \
    -user SYSDBA \
    critical_database.fdb \
    critical_backup.fbk
```

### **Restore Performance Tips**
```bash
# Fast restore for development
sb_gbak -restore \
    -parallel auto \
    -no_validity \
    -use_all_space \
    -buffers 5000 \
    -user SYSDBA \
    backup.fbk \
    dev_database.fdb

# Safe restore for production
sb_gbak -restore \
    -parallel 2 \
    -page_size 16384 \
    -buffers 10000 \
    -one_at_a_time \
    -user SYSDBA \
    backup.fbk \
    prod_database.fdb
```

### **Memory and I/O Optimization**
```bash
# For systems with limited RAM
sb_gbak -backup \
    -buffers 1000 \
    -compress \
    -one_at_a_time \
    -user SYSDBA \
    database.fdb \
    backup.fbk

# For high-memory systems
sb_gbak -backup \
    -buffers 50000 \
    -parallel auto \
    -user SYSDBA \
    database.fdb \
    backup.fbk

# For slow storage
sb_gbak -backup \
    -compress high \
    -parallel 1 \
    -verbose \
    -user SYSDBA \
    database.fdb \
    backup.fbk
```

---

## 🔒 Security Best Practices

### **Secure Backup Storage**
```bash
# Create encrypted backup with strong encryption
sb_gbak -backup \
    -encrypt-keyfile "/secure/keys/backup.key" \
    -compress \
    -user SYSDBA \
    sensitive_database.fdb \
    encrypted_backup.fbk

# Store backup on encrypted filesystem
sb_gbak -backup \
    -user SYSDBA \
    database.fdb \
    "/encrypted/volume/backup.fbk"

# Remote encrypted backup
sb_gbak -backup \
    -encrypt "$(openssl rand -base64 32)" \
    -user SYSDBA \
    database.fdb \
    - | \
    ssh backup_server "cat > /secure/backups/database_$(date +%Y%m%d).fbk"
```

### **Access Control**
```bash
# Backup with specific role
sb_gbak -backup \
    -user backup_user \
    -password "$(cat ~/.backup_password)" \
    -role BACKUP_OPERATOR \
    database.fdb \
    backup.fbk

# Audit backup operations
sb_gbak -backup \
    -user SYSDBA \
    -log "/audit/backup_$(date +%Y%m%d_%H%M%S).log" \
    -stats \
    database.fdb \
    backup.fbk
```

---

## 🆘 Troubleshooting

### **Common Backup Issues**

**Issue**: "Cannot open backup file"
```bash
# Check permissions
ls -la backup.fbk

# Ensure directory exists and is writable
mkdir -p /backup/directory
chmod 755 /backup/directory

# Test with different location
sb_gbak -backup -user SYSDBA database.fdb /tmp/test_backup.fbk
```

**Issue**: "Lock conflict" during backup
```bash
# Use snapshot isolation
sb_gbak -backup \
    -user SYSDBA \
    -no_validity \
    database.fdb \
    backup.fbk

# Monitor locks during backup
sb_lock_print -monitor database.fdb &
sb_gbak -backup -user SYSDBA database.fdb backup.fbk
```

**Issue**: "Checksum error" in source database
```bash
# Backup with checksum error recovery
sb_gbak -backup \
    -ignore_checksums \
    -user SYSDBA \
    damaged_database.fdb \
    recovery_backup.fbk

# Validate what was backed up
sb_gbak -validate recovery_backup.fbk
```

### **Common Restore Issues**

**Issue**: "Database already exists"
```bash
# Use replace option
sb_gbak -restore -replace -user SYSDBA backup.fbk database.fdb

# Or remove existing database first
rm database.fdb
sb_gbak -restore -user SYSDBA backup.fbk database.fdb
```

**Issue**: "Page size mismatch"
```bash
# Specify correct page size
sb_gbak -restore \
    -page_size 16384 \
    -user SYSDBA \
    backup.fbk \
    database.fdb

# Check backup page size first
sb_gbak -validate -verbose backup.fbk | grep "Page size"
```

**Issue**: "Insufficient disk space"
```bash
# Check available space
df -h /path/to/database/

# Use different location with more space
sb_gbak -restore \
    -user SYSDBA \
    backup.fbk \
    /larger/partition/database.fdb

# Restore to smaller page size if needed
sb_gbak -restore \
    -page_size 4096 \
    -use_all_space \
    -user SYSDBA \
    backup.fbk \
    database.fdb
```

---

## 🎯 Next Steps

- **[sb_gstat - Database Statistics](12-sb_gstat.md)** - Learn database analysis
- **[sb_gfix - Database Maintenance](13-sb_gfix.md)** - Master database repair
- **[sb_nbackup - Incremental Backup](16-backup-utilities.md)** - Advanced backup strategies
- **[Best Practices](28-best-practices.md)** - Backup and restore best practices

## 📚 Related Documentation

- **[Database Engine](05-database-engine.md)** - Understanding ScratchBird internals
- **[Troubleshooting](25-troubleshooting.md)** - Comprehensive troubleshooting guide
- **[Performance Tuning](20-performance.md)** - Optimizing backup/restore performance

---

## 💡 Pro Tips

> **Schedule Regular Backups**: Automate backups with cron jobs for consistent protection
> ```bash
> # Daily at 2 AM
> 0 2 * * * /scripts/daily_backup.sh
> ```

> **Test Your Backups**: Regularly validate and test restore procedures
> ```bash
> # Monthly backup validation
> sb_gbak -validate -deep /backups/monthly/*.fbk
> ```

> **Monitor Backup Size**: Track backup sizes to detect issues early
> ```bash
> # Compare backup sizes
> ls -lh /backups/ | grep "$(date +%Y-%m)"
> ```

**🔧 Ready to master database backups?** sb_gbak provides enterprise-grade backup and restore capabilities with modern enhancements for reliability, performance, and security!