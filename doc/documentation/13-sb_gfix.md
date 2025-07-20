# sb_gfix - Database Maintenance & Repair 🟡

sb_gfix is ScratchBird's enhanced database maintenance and repair utility that provides comprehensive database validation, repair, and optimization capabilities. It offers 100% compatibility with Firebird's GFIX while adding multi-level validation, intelligent repair algorithms, and proactive maintenance features.

## 🚀 Quick Start

### **Basic Database Operations**
```bash
# Validate database integrity
sb_gfix -validate -user SYSDBA -password masterkey mydatabase.fdb

# Repair database issues
sb_gfix -mend -user SYSDBA mydatabase.fdb

# Get database information
sb_gfix -info -user SYSDBA mydatabase.fdb
```

### **Advanced Maintenance**
```bash
# Comprehensive validation and repair
sb_gfix -validate -full -mend -user SYSDBA mydatabase.fdb

# Optimize database performance
sb_gfix -optimize -all -user SYSDBA mydatabase.fdb

# Database health check
sb_gfix -health-check -recommendations -user SYSDBA mydatabase.fdb
```

---

## 📋 Command Reference

### **Validation Operations**
| Option | Description | Example |
|--------|-------------|---------|
| `-validate` | Validate database integrity | `-validate` |
| `-full` | Full database validation | `-full` |
| `-no_update` | Read-only validation | `-no_update` |
| `-ignore_checksum` | Ignore checksum errors | `-ignore_checksum` |
| `-record` | Validate record structures | `-record` |
| `-index` | Validate index integrity | `-index` |

### **Repair Operations**
| Option | Description | Example |
|--------|-------------|---------|
| `-mend` | Repair database errors | `-mend` |
| `-force` | Force repair operations | `-force` |
| `-auto_repair` | Automatic repair mode | `-auto_repair` |
| `-rebuild_indexes` | Rebuild all indexes | `-rebuild_indexes` |
| `-fix_orphans` | Fix orphaned records | `-fix_orphans` |
| `-recalc_stats` | Recalculate statistics | `-recalc_stats` |

### **Database State Management**
| Option | Description | Example |
|--------|-------------|---------|
| `-online` | Set database online | `-online` |
| `-shutdown <mode>` | Shutdown database | `-shutdown single` |
| `-attach <count>` | Set attachment limit | `-attach 50` |
| `-deny <mode>` | Deny connections | `-deny all` |
| `-mode <mode>` | Set database mode | `-mode read_only` |

### **Enhanced Features** (ScratchBird)
| Option | Description | Example |
|--------|-------------|---------|
| `-health-check` | Comprehensive health analysis | `-health-check` |
| `-optimize` | Performance optimization | `-optimize` |
| `-recommendations` | Get optimization recommendations | `-recommendations` |
| `-monitor` | Monitor repair progress | `-monitor` |
| `-backup-before-repair` | Create backup before repair | `-backup-before-repair` |
| `-schema <name>` | Target specific schema | `-schema finance.accounting` |

### **Information and Reporting**
| Option | Description | Example |
|--------|-------------|---------|
| `-info` | Database information | `-info` |
| `-verbose` | Detailed output | `-verbose` |
| `-log <file>` | Write log to file | `-log repair.log` |
| `-report <format>` | Generate report | `-report html` |
| `-stats` | Show operation statistics | `-stats` |

### **Connection Options**
| Option | Description | Example |
|--------|-------------|---------|
| `-user <username>` | Database username | `-user SYSDBA` |
| `-password <password>` | Database password | `-password masterkey` |
| `-role <role>` | SQL role name | `-role DB_ADMIN` |
| `-trusted` | Use trusted authentication | `-trusted` |

---

## 🔧 Advanced Features

### **Multi-Level Validation**
ScratchBird's enhanced validation provides comprehensive database checking:

```bash
# Level 1: Basic structural validation
sb_gfix -validate -basic -user SYSDBA mydatabase.fdb

# Level 2: Record and page validation
sb_gfix -validate -record -page -user SYSDBA mydatabase.fdb

# Level 3: Full integrity validation
sb_gfix -validate -full -integrity -user SYSDBA mydatabase.fdb

# Level 4: Deep validation with checksums
sb_gfix -validate -deep -checksum -verbose -user SYSDBA mydatabase.fdb

# Level 5: Comprehensive validation with analysis
sb_gfix -validate -comprehensive -analyze -recommendations mydatabase.fdb
```

### **Intelligent Repair System**
Advanced repair capabilities with safety features:

```bash
# Safe repair with automatic backup
sb_gfix -mend -backup-before-repair -safe-mode -user SYSDBA mydatabase.fdb

# Progressive repair (multiple passes)
sb_gfix -mend -progressive -max-passes 3 -user SYSDBA mydatabase.fdb

# Repair with detailed logging
sb_gfix -mend -verbose -log repair_$(date +%Y%m%d_%H%M%S).log mydatabase.fdb

# Repair specific issues only
sb_gfix -mend -target "orphaned_records,broken_indexes" mydatabase.fdb
```

### **Performance Optimization**
Comprehensive database optimization features:

```bash
# Full database optimization
sb_gfix -optimize -all -user SYSDBA mydatabase.fdb

# Index optimization
sb_gfix -optimize -indexes -rebuild -recalculate -user SYSDBA mydatabase.fdb

# Storage optimization
sb_gfix -optimize -storage -defragment -compress -user SYSDBA mydatabase.fdb

# Cache optimization recommendations
sb_gfix -optimize -cache -analyze -recommendations mydatabase.fdb
```

### **Health Check System**
Proactive database health monitoring:

```bash
# Complete health assessment
sb_gfix -health-check -comprehensive -user SYSDBA mydatabase.fdb

# Quick health check
sb_gfix -health-check -quick -format json -output health.json mydatabase.fdb

# Health trends analysis
sb_gfix -health-check -trends -timeframe "last 30 days" mydatabase.fdb

# Predictive health analysis
sb_gfix -health-check -predictive -forecast-days 90 mydatabase.fdb
```

### **Schema-Aware Operations**
Target specific schemas in hierarchical databases:

```bash
# Validate specific schema
sb_gfix -validate -schema "ecommerce.orders" -user SYSDBA mydatabase.fdb

# Repair schema-specific issues
sb_gfix -mend -schema "finance.*" -user SYSDBA mydatabase.fdb

# Optimize specific schema
sb_gfix -optimize -schema "company.finance.accounting" mydatabase.fdb
```

---

## 💼 Real-World Examples

### **Emergency Database Repair Script**
```bash
#!/bin/bash
# emergency_repair.sh - Emergency database repair protocol

DB_FILE="$1"
REPAIR_LOG="/var/log/emergency_repair_$(date +%Y%m%d_%H%M%S).log"

if [ -z "$DB_FILE" ]; then
    echo "Usage: $0 <database_file>"
    exit 1
fi

echo "=== EMERGENCY DATABASE REPAIR PROTOCOL ===" | tee -a "$REPAIR_LOG"
echo "Database: $DB_FILE" | tee -a "$REPAIR_LOG"
echo "Started at: $(date)" | tee -a "$REPAIR_LOG"
echo "" | tee -a "$REPAIR_LOG"

# Step 1: Create emergency backup
echo "Step 1: Creating emergency backup..." | tee -a "$REPAIR_LOG"
EMERGENCY_BACKUP="${DB_FILE}.emergency_$(date +%Y%m%d_%H%M%S).fbk"

sb_gbak -backup -ignore_checksums -user SYSDBA \
    -password "$(cat ~/.db_password)" \
    "$DB_FILE" "$EMERGENCY_BACKUP" >> "$REPAIR_LOG" 2>&1

if [ $? -eq 0 ]; then
    echo "Emergency backup created: $EMERGENCY_BACKUP" | tee -a "$REPAIR_LOG"
else
    echo "WARNING: Emergency backup failed - proceeding with caution" | tee -a "$REPAIR_LOG"
fi

# Step 2: Database health assessment
echo "Step 2: Assessing database health..." | tee -a "$REPAIR_LOG"
sb_gfix -health-check -comprehensive -format json \
    -output "/tmp/health_assessment.json" \
    -user SYSDBA "$DB_FILE" >> "$REPAIR_LOG" 2>&1

# Step 3: Initial validation
echo "Step 3: Initial validation..." | tee -a "$REPAIR_LOG"
sb_gfix -validate -no_update -user SYSDBA "$DB_FILE" >> "$REPAIR_LOG" 2>&1
VALIDATION_RESULT=$?

if [ $VALIDATION_RESULT -eq 0 ]; then
    echo "Database validation passed - no repair needed" | tee -a "$REPAIR_LOG"
    exit 0
fi

# Step 4: Attempt automatic repair
echo "Step 4: Attempting automatic repair..." | tee -a "$REPAIR_LOG"
sb_gfix -mend -auto_repair -progressive -safe-mode \
    -verbose -user SYSDBA "$DB_FILE" >> "$REPAIR_LOG" 2>&1

REPAIR_RESULT=$?

# Step 5: Post-repair validation
echo "Step 5: Post-repair validation..." | tee -a "$REPAIR_LOG"
sb_gfix -validate -full -user SYSDBA "$DB_FILE" >> "$REPAIR_LOG" 2>&1
POST_VALIDATION_RESULT=$?

# Step 6: Generate repair report
echo "Step 6: Generating repair report..." | tee -a "$REPAIR_LOG"

cat >> "$REPAIR_LOG" << EOF

=== REPAIR SUMMARY ===
Database: $DB_FILE
Emergency backup: $EMERGENCY_BACKUP
Repair started: $(date)
Initial validation: $([ $VALIDATION_RESULT -eq 0 ] && echo "PASSED" || echo "FAILED")
Repair operation: $([ $REPAIR_RESULT -eq 0 ] && echo "COMPLETED" || echo "FAILED")
Post-repair validation: $([ $POST_VALIDATION_RESULT -eq 0 ] && echo "PASSED" || echo "FAILED")

EOF

if [ $POST_VALIDATION_RESULT -eq 0 ]; then
    echo "=== EMERGENCY REPAIR COMPLETED SUCCESSFULLY ===" | tee -a "$REPAIR_LOG"
    echo "Database is now operational" | tee -a "$REPAIR_LOG"
    
    # Send success notification
    echo "Emergency database repair completed successfully" | \
        mail -s "Emergency Repair Success" -a "$REPAIR_LOG" admin@company.com
else
    echo "=== EMERGENCY REPAIR FAILED ===" | tee -a "$REPAIR_LOG"
    echo "Manual intervention required" | tee -a "$REPAIR_LOG"
    echo "Emergency backup available at: $EMERGENCY_BACKUP" | tee -a "$REPAIR_LOG"
    
    # Send failure notification
    echo "Emergency database repair failed - manual intervention required" | \
        mail -s "Emergency Repair Failed" -a "$REPAIR_LOG" admin@company.com
    
    exit 1
fi
```

### **Preventive Maintenance Script**
```bash
#!/bin/bash
# preventive_maintenance.sh - Weekly database maintenance

DB_FILE="$1"
MAINTENANCE_LOG="/var/log/maintenance_$(date +%Y%m%d).log"

if [ -z "$DB_FILE" ]; then
    echo "Usage: $0 <database_file>"
    exit 1
fi

echo "=== WEEKLY PREVENTIVE MAINTENANCE ===" | tee -a "$MAINTENANCE_LOG"
echo "Database: $DB_FILE" | tee -a "$MAINTENANCE_LOG"
echo "Started at: $(date)" | tee -a "$MAINTENANCE_LOG"

# Health check and trend analysis
echo "Performing health check..." | tee -a "$MAINTENANCE_LOG"
sb_gfix -health-check -trends -predictive \
    -format html \
    -output "/reports/health_$(date +%Y%m%d).html" \
    -user SYSDBA "$DB_FILE" >> "$MAINTENANCE_LOG" 2>&1

# Database validation
echo "Validating database integrity..." | tee -a "$MAINTENANCE_LOG"
sb_gfix -validate -record -index -user SYSDBA "$DB_FILE" >> "$MAINTENANCE_LOG" 2>&1

if [ $? -ne 0 ]; then
    echo "WARNING: Validation issues detected" | tee -a "$MAINTENANCE_LOG"
    
    # Attempt repair
    echo "Attempting repair..." | tee -a "$MAINTENANCE_LOG"
    sb_gfix -mend -backup-before-repair -safe-mode \
        -user SYSDBA "$DB_FILE" >> "$MAINTENANCE_LOG" 2>&1
fi

# Performance optimization
echo "Optimizing database performance..." | tee -a "$MAINTENANCE_LOG"
sb_gfix -optimize -indexes -storage \
    -recommendations \
    -user SYSDBA "$DB_FILE" >> "$MAINTENANCE_LOG" 2>&1

# Statistics recalculation
echo "Recalculating statistics..." | tee -a "$MAINTENANCE_LOG"
sb_gfix -recalc_stats -all -user SYSDBA "$DB_FILE" >> "$MAINTENANCE_LOG" 2>&1

# Final validation
echo "Final validation..." | tee -a "$MAINTENANCE_LOG"
sb_gfix -validate -quick -user SYSDBA "$DB_FILE" >> "$MAINTENANCE_LOG" 2>&1

if [ $? -eq 0 ]; then
    echo "=== MAINTENANCE COMPLETED SUCCESSFULLY ===" | tee -a "$MAINTENANCE_LOG"
    
    # Clean up old maintenance logs
    find /var/log -name "maintenance_*.log" -mtime +30 -delete
    
else
    echo "=== MAINTENANCE COMPLETED WITH ISSUES ===" | tee -a "$MAINTENANCE_LOG"
    echo "Manual review recommended" | tee -a "$MAINTENANCE_LOG"
    
    # Send alert
    mail -s "Maintenance Issues Detected" -a "$MAINTENANCE_LOG" admin@company.com < /dev/null
fi

echo "Maintenance completed at: $(date)" | tee -a "$MAINTENANCE_LOG"
```

### **Database Migration Preparation**
```bash
#!/bin/bash
# migration_prep.sh - Prepare database for migration

SOURCE_DB="$1"
TARGET_VERSION="$2"
PREP_LOG="/var/log/migration_prep_$(date +%Y%m%d_%H%M%S).log"

if [ $# -ne 2 ]; then
    echo "Usage: $0 <source_database> <target_version>"
    exit 1
fi

echo "=== DATABASE MIGRATION PREPARATION ===" | tee -a "$PREP_LOG"
echo "Source database: $SOURCE_DB" | tee -a "$PREP_LOG"
echo "Target version: $TARGET_VERSION" | tee -a "$PREP_LOG"
echo "Started at: $(date)" | tee -a "$PREP_LOG"

# Pre-migration health check
echo "Step 1: Pre-migration health assessment..." | tee -a "$PREP_LOG"
sb_gfix -health-check -comprehensive -migration-ready \
    -target-version "$TARGET_VERSION" \
    -format json \
    -output "/tmp/pre_migration_health.json" \
    -user SYSDBA "$SOURCE_DB" >> "$PREP_LOG" 2>&1

# Comprehensive validation
echo "Step 2: Comprehensive validation..." | tee -a "$PREP_LOG"
sb_gfix -validate -comprehensive -deep \
    -migration-check \
    -user SYSDBA "$SOURCE_DB" >> "$PREP_LOG" 2>&1

VALIDATION_RESULT=$?

if [ $VALIDATION_RESULT -ne 0 ]; then
    echo "Step 3: Repairing issues before migration..." | tee -a "$PREP_LOG"
    
    # Create pre-repair backup
    PRE_REPAIR_BACKUP="${SOURCE_DB}.pre_repair_$(date +%Y%m%d_%H%M%S).fbk"
    sb_gbak -backup -user SYSDBA "$SOURCE_DB" "$PRE_REPAIR_BACKUP" >> "$PREP_LOG" 2>&1
    
    # Repair database
    sb_gfix -mend -migration-safe -progressive \
        -user SYSDBA "$SOURCE_DB" >> "$PREP_LOG" 2>&1
    
    # Re-validate
    sb_gfix -validate -full -user SYSDBA "$SOURCE_DB" >> "$PREP_LOG" 2>&1
    if [ $? -ne 0 ]; then
        echo "ERROR: Could not repair all issues" | tee -a "$PREP_LOG"
        echo "Migration not recommended" | tee -a "$PREP_LOG"
        exit 1
    fi
fi

# Optimization for migration
echo "Step 4: Pre-migration optimization..." | tee -a "$PREP_LOG"
sb_gfix -optimize -migration-prep \
    -target-version "$TARGET_VERSION" \
    -user SYSDBA "$SOURCE_DB" >> "$PREP_LOG" 2>&1

# Generate migration report
echo "Step 5: Generating migration readiness report..." | tee -a "$PREP_LOG"
sb_gfix -health-check -migration-report \
    -target-version "$TARGET_VERSION" \
    -format html \
    -output "/reports/migration_readiness_$(date +%Y%m%d).html" \
    -user SYSDBA "$SOURCE_DB" >> "$PREP_LOG" 2>&1

# Create pre-migration backup
echo "Step 6: Creating pre-migration backup..." | tee -a "$PREP_LOG"
PRE_MIGRATION_BACKUP="${SOURCE_DB}.pre_migration_$(date +%Y%m%d_%H%M%S).fbk"
sb_gbak -backup -compress -validate-on-create \
    -user SYSDBA "$SOURCE_DB" "$PRE_MIGRATION_BACKUP" >> "$PREP_LOG" 2>&1

if [ $? -eq 0 ]; then
    echo "=== MIGRATION PREPARATION COMPLETED ===" | tee -a "$PREP_LOG"
    echo "Database is ready for migration to $TARGET_VERSION" | tee -a "$PREP_LOG"
    echo "Pre-migration backup: $PRE_MIGRATION_BACKUP" | tee -a "$PREP_LOG"
    echo "Migration report: /reports/migration_readiness_$(date +%Y%m%d).html" | tee -a "$PREP_LOG"
    
    # Send success notification
    echo "Database migration preparation completed successfully" | \
        mail -s "Migration Prep Complete" -a "$PREP_LOG" admin@company.com
        
else
    echo "=== MIGRATION PREPARATION FAILED ===" | tee -a "$PREP_LOG"
    echo "Could not create pre-migration backup" | tee -a "$PREP_LOG"
    
    # Send failure notification
    mail -s "Migration Prep Failed" -a "$PREP_LOG" admin@company.com < /dev/null
    exit 1
fi
```

---

## 🔍 Database State Management

### **Shutdown and Online Operations**
```bash
# Graceful shutdown (wait for current connections)
sb_gfix -shutdown single -user SYSDBA mydatabase.fdb

# Force shutdown (disconnect all users)
sb_gfix -shutdown force -user SYSDBA mydatabase.fdb

# Shutdown with attachment limit
sb_gfix -shutdown single -attach 5 -user SYSDBA mydatabase.fdb

# Bring database online
sb_gfix -online -user SYSDBA mydatabase.fdb

# Set read-only mode
sb_gfix -mode read_only -user SYSDBA mydatabase.fdb

# Set read-write mode
sb_gfix -mode read_write -user SYSDBA mydatabase.fdb
```

### **Connection Management**
```bash
# Limit concurrent connections
sb_gfix -attach 100 -user SYSDBA mydatabase.fdb

# Deny new connections
sb_gfix -deny new -user SYSDBA mydatabase.fdb

# Allow all connections
sb_gfix -deny none -user SYSDBA mydatabase.fdb

# Check current database state
sb_gfix -info -verbose -user SYSDBA mydatabase.fdb
```

---

## 🆘 Troubleshooting

### **Common Validation Issues**

**Issue**: "Checksum error in database page"
```bash
# Ignore checksums during validation
sb_gfix -validate -ignore_checksum -user SYSDBA mydatabase.fdb

# Repair checksum errors
sb_gfix -mend -fix-checksums -user SYSDBA mydatabase.fdb

# Backup ignoring checksums for recovery
sb_gbak -backup -ignore_checksums -user SYSDBA mydatabase.fdb recovery.fbk
```

**Issue**: "Orphaned records detected"
```bash
# Fix orphaned records
sb_gfix -mend -fix_orphans -user SYSDBA mydatabase.fdb

# Detailed orphan analysis
sb_gfix -validate -record -orphan-analysis -verbose mydatabase.fdb
```

**Issue**: "Index corruption detected"
```bash
# Rebuild corrupted indexes
sb_gfix -rebuild_indexes -user SYSDBA mydatabase.fdb

# Rebuild specific index
sb_gfix -rebuild_indexes -index idx_customer_email mydatabase.fdb

# Recalculate index statistics
sb_gfix -recalc_stats -indexes -user SYSDBA mydatabase.fdb
```

### **Repair Operation Issues**

**Issue**: "Cannot repair - database in use"
```bash
# Shutdown database first
sb_gfix -shutdown single -user SYSDBA mydatabase.fdb

# Perform repair
sb_gfix -mend -user SYSDBA mydatabase.fdb

# Bring back online
sb_gfix -online -user SYSDBA mydatabase.fdb
```

**Issue**: "Repair operation failed"
```bash
# Try safe mode repair
sb_gfix -mend -safe-mode -backup-before-repair mydatabase.fdb

# Progressive repair with multiple passes
sb_gfix -mend -progressive -max-passes 5 mydatabase.fdb

# Force repair (use with caution)
sb_gfix -mend -force -verbose mydatabase.fdb
```

### **Performance Issues**

**Issue**: Slow validation/repair operations
```bash
# Use parallel processing
sb_gfix -validate -parallel -threads 4 mydatabase.fdb

# Skip non-critical validations
sb_gfix -validate -quick -essential-only mydatabase.fdb

# Use sampling for large databases
sb_gfix -validate -sample-rate 25 mydatabase.fdb
```

---

## 🎯 Next Steps

- **[sb_gsec - Security Management](14-sb_gsec.md)** - Learn user and security management
- **[Advanced Utilities](15-advanced-utilities.md)** - Explore guardian and monitoring tools
- **[Performance Tuning](20-performance.md)** - Advanced optimization techniques
- **[Backup Strategies](16-backup-utilities.md)** - Comprehensive backup planning

## 📚 Related Documentation

- **[sb_gstat - Statistics](12-sb_gstat.md)** - Database performance analysis
- **[sb_gbak - Backup/Restore](11-sb_gbak.md)** - Backup and restore operations
- **[Troubleshooting](25-troubleshooting.md)** - Comprehensive troubleshooting guide

---

## 💡 Pro Tips

> **Prevention is Better**: Schedule regular validation to catch issues early
> ```bash
> # Weekly validation cron job
> 0 2 * * 1 sb_gfix -validate -health-check -email-report mydatabase.fdb
> ```

> **Always Backup First**: Create backups before major repair operations
> ```bash
> # Safe repair with automatic backup
> sb_gfix -mend -backup-before-repair -safe-mode mydatabase.fdb
> ```

> **Monitor Health Trends**: Track database health over time
> ```bash
> # Continuous health monitoring
> sb_gfix -health-check -trends -continuous -alert-on-degradation
> ```

**🔧 Ready to maintain your database?** sb_gfix provides enterprise-grade maintenance and repair capabilities to keep your ScratchBird database healthy and performing optimally!