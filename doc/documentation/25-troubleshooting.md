# Troubleshooting Guide 🟢

This comprehensive troubleshooting guide helps you diagnose and resolve common issues with ScratchBird database system. It's organized by problem category for quick navigation.

## 🔍 Quick Diagnostic Checklist

Before diving into specific issues, run this quick diagnostic:

```bash
# 1. Check ScratchBird installation
sb_isql -z

# 2. Test basic connectivity
sb_isql -user SYSDBA -password masterkey /path/to/test.fdb

# 3. Verify database file
ls -la /path/to/your/database.fdb

# 4. Check system resources
df -h  # Disk space
free -m  # Memory
ps aux | grep sb_  # ScratchBird processes
```

---

## 🔗 Connection Issues

### **Problem: "Connection failed" or "Database file not found"**

#### **Symptoms**
```
ERROR: connection failed
ERROR: I/O error during "open" operation for file "/path/to/database.fdb"
ERROR: Error while trying to open file
```

#### **Solutions**

**1. Check Database File Path**
```bash
# Verify file exists and is accessible
ls -la /path/to/database.fdb

# Check file permissions
stat /path/to/database.fdb

# Ensure directory exists
ls -la /path/to/directory/
```

**2. Create Database if Missing**
```bash
# Create new database
sb_isql -user SYSDBA -password masterkey
```
```sql
SQL> CREATE DATABASE '/path/to/new_database.fdb'
CON> USER 'SYSDBA' PASSWORD 'masterkey';
SQL> QUIT;
```

**3. Fix File Permissions**
```bash
# Make file readable/writable by user
chmod 660 /path/to/database.fdb

# Ensure directory permissions
chmod 755 /path/to/directory/

# Check ownership
chown user:group /path/to/database.fdb
```

### **Problem: "Login failed" or "Authentication error"**

#### **Symptoms**
```
ERROR: Your user name and password are not defined
ERROR: connection rejected by remote interface
ERROR: unsuccessful metadata update
```

#### **Solutions**

**1. Verify Credentials**
```bash
# Test with default credentials
sb_isql -user SYSDBA -password masterkey database.fdb

# Try trusted authentication
sb_isql -trusted database.fdb

# Check if user exists
sb_gsec -display
```

**2. Reset SYSDBA Password**
```bash
# Using GSEC
sb_gsec -modify SYSDBA -password new_password

# Or recreate user
sb_gsec -delete SYSDBA
sb_gsec -add SYSDBA -password new_password
```

**3. Database Security Database Issues**
```bash
# Check security database
ls -la security4.fdb

# Recreate security database if corrupted
sb_gsec -database new_security.fdb -add SYSDBA -password masterkey
```

### **Problem: "Database is locked" or "Shutdown"**

#### **Symptoms**
```
ERROR: database shutdown
ERROR: database is locked
ERROR: database unavailable
```

#### **Solutions**

**1. Check Database State**
```bash
# Get database information
sb_gfix -info database.fdb

# Check if database is shutdown
sb_gstat -header database.fdb
```

**2. Bring Database Online**
```bash
# Set database to normal state
sb_gfix -online database.fdb

# Force database online if needed
sb_gfix -online -force database.fdb
```

**3. Remove Shutdown**
```bash
# Remove shutdown state
sb_gfix -shutdown full -attach 0 database.fdb
sb_gfix -online database.fdb
```

---

## 🚀 Performance Issues

### **Problem: Slow Query Performance**

#### **Symptoms**
- Queries taking unusually long time
- High CPU or memory usage
- Database appears unresponsive

#### **Diagnostic Steps**

**1. Analyze Query Execution**
```sql
-- Enable plan and statistics
SET PLAN ON;
SET STATS ON;

-- Run your slow query
SELECT * FROM large_table WHERE condition = 'value';

-- Look for:
-- - NATURAL scans (table scans without indexes)
-- - High page reads
-- - Long execution times
```

**2. Check Database Statistics**
```bash
# Get comprehensive database statistics
sb_gstat -all database.fdb

# Check specific table statistics
sb_gstat -table problematic_table database.fdb

# Look for:
# - Low index selectivity
# - High fragmentation
# - Missing statistics
```

#### **Solutions**

**1. Update Index Statistics**
```sql
-- Update statistics for specific index
SET STATISTICS INDEX idx_table_column;

-- Update all table statistics
SET STATISTICS INDEX ALL;
```

**2. Create Missing Indexes**
```sql
-- Analyze query plan for missing indexes
SET PLAN ON;
-- If you see NATURAL scans, add indexes:

CREATE INDEX idx_table_column ON table_name (column_name);
CREATE INDEX idx_table_multi ON table_name (col1, col2);
```

**3. Optimize Database Configuration**
```bash
# Increase cache size in scratchbird.conf
DefaultDbCachePages = 10000  # Increase from default

# Adjust other parameters
TempCacheLimit = 134217728   # 128MB
LockMemSize = 1048576        # 1MB
```

### **Problem: High Memory Usage**

#### **Symptoms**
- System running out of memory
- Database processes consuming excessive RAM
- Swap usage increasing

#### **Solutions**

**1. Adjust Cache Settings**
```bash
# In scratchbird.conf, reduce cache size
DefaultDbCachePages = 5000   # Reduce if too high

# Monitor memory usage
sb_gstat -cache database.fdb
```

**2. Close Unused Connections**
```sql
-- Check active connections
SELECT COUNT(*) FROM MON$ATTACHMENTS;

-- Close specific connection (as SYSDBA)
DELETE FROM MON$ATTACHMENTS WHERE MON$ATTACHMENT_ID = <id>;
```

**3. Optimize Queries**
```sql
-- Avoid SELECT * on large tables
SELECT specific_columns FROM large_table WHERE condition;

-- Use FIRST to limit results
SELECT FIRST 100 * FROM large_table;

-- Close cursors promptly in applications
```

---

## 🔒 Security Issues

### **Problem: "Permission denied" Errors**

#### **Symptoms**
```
ERROR: no permission for SELECT access to TABLE customers
ERROR: no permission for INSERT access to TABLE orders
ERROR: unsuccessful metadata update
```

#### **Solutions**

**1. Check User Permissions**
```sql
-- As SYSDBA, check user roles
SELECT * FROM RDB$USER_PRIVILEGES WHERE RDB$USER = 'USERNAME';

-- Grant necessary permissions
GRANT SELECT, INSERT, UPDATE, DELETE ON table_name TO username;
GRANT ALL ON table_name TO role_name;
```

**2. Manage Roles**
```sql
-- Create role if needed
CREATE ROLE app_user;

-- Grant permissions to role
GRANT SELECT, INSERT ON customers TO app_user;

-- Grant role to user
GRANT app_user TO username;
```

**3. Schema-Level Permissions**
```sql
-- Grant permissions on schema
GRANT USAGE ON SCHEMA schema_name TO username;

-- Grant permissions on all tables in schema
GRANT SELECT ON ALL TABLES IN SCHEMA schema_name TO username;
```

### **Problem: "User does not exist" Errors**

#### **Solutions**

**1. Create Missing User**
```bash
# Using sb_gsec
sb_gsec -add username -password userpass

# Or in SQL
CREATE USER username PASSWORD 'userpass';
```

**2. Check Security Database**
```bash
# Verify user exists
sb_gsec -display

# If security database is corrupted, recreate
# (This is a last resort - backup first!)
```

---

## 💾 Database Corruption Issues

### **Problem: "Internal error" or "Checksum error"**

#### **Symptoms**
```
ERROR: internal Firebird consistency check
ERROR: bad checksum
ERROR: database page corruption
ERROR: index is corrupt
```

#### **Diagnostic Steps**

**1. Validate Database Integrity**
```bash
# Full database validation
sb_gfix -validate -full database.fdb

# Check for specific errors
sb_gfix -validate -no_update database.fdb
```

**2. Check Database Pages**
```bash
# Get database header info
sb_gstat -header database.fdb

# Check for page errors
sb_gfix -validate -full database.fdb 2>&1 | grep -i error
```

#### **Solutions**

**1. Minor Corruption - Try Repair**
```bash
# Attempt automatic repair
sb_gfix -mend database.fdb

# Validate after repair
sb_gfix -validate database.fdb
```

**2. Moderate Corruption - Backup/Restore**
```bash
# Create backup (may skip corrupted data)
sb_gbak -backup -ignore_checksums database.fdb backup.fbk

# Restore to new database
sb_gbak -restore backup.fbk new_database.fdb

# Validate restored database
sb_gfix -validate new_database.fdb
```

**3. Severe Corruption - Data Pumping**
```bash
# Extract what data you can
sb_isql -extract database.fdb > schema.sql

# Create new database
sb_isql -input schema.sql new_database.fdb

# Manually extract data table by table
sb_isql database.fdb
```
```sql
SQL> OUTPUT data_export.sql;
SQL> SELECT * FROM table1;
SQL> SELECT * FROM table2;
-- Continue for each table
SQL> OUTPUT;
```

### **Problem: "Lock conflict" or "Deadlock"**

#### **Symptoms**
```
ERROR: lock conflict on no wait transaction
ERROR: deadlock
ERROR: update conflicts with concurrent update
```

#### **Solutions**

**1. Monitor Lock Activity**
```bash
# Real-time lock monitoring
sb_lock_print -real-time database.fdb

# Check for waiting locks
sb_lock_print -wait database.fdb
```

**2. Optimize Transaction Management**
```sql
-- Use shorter transactions
BEGIN TRANSACTION;
-- Keep transactions brief
UPDATE table SET column = value WHERE id = specific_id;
COMMIT;

-- Use appropriate isolation levels
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
```

**3. Application-Level Solutions**
```cpp
// In application code, implement retry logic
bool executeWithRetry(SBDatabase& db, const std::string& sql, int max_retries = 3) {
    for (int i = 0; i < max_retries; ++i) {
        try {
            return db.executeQuery(sql);
        } catch (const SBException& e) {
            if (e.getErrorCode() == isc_lock_conflict && i < max_retries - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100 * (i + 1)));
                continue;
            }
            throw;
        }
    }
    return false;
}
```

---

## 🛠️ Utility-Specific Issues

### **sb_isql Issues**

**Problem: "Command not found" or readline errors**
```bash
# Install readline if missing
sudo apt-get install libreadline-dev  # Ubuntu/Debian
sudo yum install readline-devel       # CentOS/RHEL

# Rebuild sb_isql with readline support
g++ -std=c++17 -O3 -o sb_isql src/utilities/sb_isql.cpp src/utilities/sb_database.cpp \
    -I./src/include -L./release/alpha0.6.0/lib -lfbclient -lreadline
```

**Problem: Schema commands not working**
```sql
-- Ensure you're using ScratchBird-specific features correctly
SHOW SCHEMAS;  -- List all schemas
SET SCHEMA 'schema.path';  -- Set current schema
```

### **sb_gbak Issues**

**Problem: "Cannot open backup file"**
```bash
# Check backup file permissions
ls -la backup.fbk

# Ensure directory exists and is writable
ls -la /backup/directory/

# Test backup to different location
sb_gbak -backup database.fdb /tmp/test_backup.fbk
```

**Problem: "Restore failed"**
```bash
# Validate backup file first
sb_gbak -verify backup.fbk

# Restore with verbose output for debugging
sb_gbak -restore -verbose backup.fbk new_database.fdb

# Try restoring with different page size
sb_gbak -restore -page_size 16384 backup.fbk new_database.fdb
```

### **sb_gfix Issues**

**Problem: "Cannot repair database"**
```bash
# Try read-only validation first
sb_gfix -validate -no_update database.fdb

# If validation shows errors, try forced repair
sb_gfix -mend -force database.fdb

# If still failing, try backup/restore approach
```

---

## 🖥️ Platform-Specific Issues

### **Linux Issues**

**Problem: "Permission denied" on database files**
```bash
# Check SELinux status
sestatus

# If SELinux is enforcing, check context
ls -Z database.fdb

# Set appropriate SELinux context
sudo setsebool -P allow_user_mysql_connect 1
```

**Problem: "Too many open files"**
```bash
# Check current limits
ulimit -n

# Increase file descriptor limit
ulimit -n 4096

# Make permanent in /etc/security/limits.conf
echo "* soft nofile 4096" >> /etc/security/limits.conf
echo "* hard nofile 8192" >> /etc/security/limits.conf
```

### **Windows Issues**

**Problem: Path issues with spaces**
```cmd
REM Use quotes around paths with spaces
sb_isql -user SYSDBA -password masterkey "C:\Program Files\MyApp\database.fdb"

REM Or use short paths
sb_isql -user SYSDBA -password masterkey C:\PROGRA~1\MYAPP\database.fdb
```

**Problem: Windows service issues**
```cmd
REM Check service status
sc query ScratchBirdGuardian

REM Restart service
sc stop ScratchBirdGuardian
sc start ScratchBirdGuardian

REM Check service logs
type "C:\ProgramData\ScratchBird\logs\guardian.log"
```

---

## 📊 Monitoring and Alerts

### **Set Up Monitoring**

**1. Database Health Check Script**
```bash
#!/bin/bash
# health_check.sh - Database health monitoring

DB_FILE="/path/to/database.fdb"
LOG_FILE="/var/log/scratchbird_health.log"

echo "$(date): Starting health check" >> $LOG_FILE

# 1. Check database connectivity
if ! sb_isql -user SYSDBA -password masterkey -execute "SELECT 1 FROM RDB\$DATABASE;" $DB_FILE > /dev/null 2>&1; then
    echo "$(date): ERROR - Database connection failed" >> $LOG_FILE
    # Send alert
    mail -s "ScratchBird Database Alert: Connection Failed" admin@company.com < /dev/null
fi

# 2. Check database validation
if ! sb_gfix -validate $DB_FILE > /dev/null 2>&1; then
    echo "$(date): ERROR - Database validation failed" >> $LOG_FILE
    # Send alert
    mail -s "ScratchBird Database Alert: Validation Failed" admin@company.com < /dev/null
fi

# 3. Check disk space
DISK_USAGE=$(df $(dirname $DB_FILE) | tail -1 | awk '{print $5}' | sed 's/%//')
if [ $DISK_USAGE -gt 90 ]; then
    echo "$(date): WARNING - Disk usage is $DISK_USAGE%" >> $LOG_FILE
fi

# 4. Check database size growth
DB_SIZE=$(stat -c%s $DB_FILE)
echo "$(date): Database size: $DB_SIZE bytes" >> $LOG_FILE

echo "$(date): Health check completed" >> $LOG_FILE
```

**2. Performance Monitoring**
```bash
#!/bin/bash
# performance_monitor.sh - Monitor database performance

DB_FILE="/path/to/database.fdb"

# Get cache hit ratio
CACHE_INFO=$(sb_gstat -cache $DB_FILE)
echo "Cache statistics: $CACHE_INFO"

# Monitor active connections
CONNECTION_COUNT=$(sb_isql -user SYSDBA -password masterkey -execute "SELECT COUNT(*) FROM MON\$ATTACHMENTS;" $DB_FILE)
echo "Active connections: $CONNECTION_COUNT"

# Check for lock conflicts
LOCK_CONFLICTS=$(sb_lock_print -wait $DB_FILE | wc -l)
if [ $LOCK_CONFLICTS -gt 0 ]; then
    echo "WARNING: $LOCK_CONFLICTS lock conflicts detected"
fi
```

### **Automated Alerts**

**1. Email Alert Setup**
```bash
# Install mail utility
sudo apt-get install mailutils  # Ubuntu/Debian

# Configure /etc/postfix/main.cf for SMTP
# Or use external SMTP service

# Test email
echo "Test message" | mail -s "Test Subject" admin@company.com
```

**2. Log Monitoring with logrotate**
```bash
# Create /etc/logrotate.d/scratchbird
/var/log/scratchbird/*.log {
    daily
    rotate 30
    compress
    delaycompress
    missingok
    notifempty
    create 644 scratchbird scratchbird
    postrotate
        # Restart logging if needed
        killall -USR1 sb_guard 2>/dev/null || true
    endscript
}
```

---

## 🆘 Emergency Procedures

### **Database Won't Start**

**1. Immediate Steps**
```bash
# Check if database file exists and isn't corrupted
ls -la database.fdb

# Try to get basic info
sb_gstat -header database.fdb

# If header is readable, try validation
sb_gfix -validate -no_update database.fdb
```

**2. Recovery Options (in order of preference)**
```bash
# Option 1: Simple restart
sb_gfix -online database.fdb

# Option 2: Force online
sb_gfix -online -force database.fdb

# Option 3: Backup/restore
sb_gbak -backup database.fdb emergency_backup.fbk
sb_gbak -restore emergency_backup.fbk database_recovered.fdb

# Option 4: Data pumping (last resort)
# Extract schema and data manually
```

### **Complete System Failure**

**1. Assessment**
```bash
# Check system resources
df -h
free -m
ps aux | grep sb_

# Check logs
tail -f /var/log/scratchbird/error.log
dmesg | grep -i error
```

**2. Emergency Backup**
```bash
# If possible, backup critical databases immediately
for db in /path/to/databases/*.fdb; do
    sb_gbak -backup "$db" "/emergency/backup/$(basename $db .fdb)_emergency.fbk"
done
```

**3. System Recovery**
```bash
# Restart ScratchBird services
sudo systemctl restart scratchbird-guardian
sudo systemctl status scratchbird-guardian

# Check service logs
journalctl -u scratchbird-guardian -f
```

---

## 📞 Getting Additional Help

### **Before Contacting Support**

Gather this information:
1. **ScratchBird version**: `sb_isql -z`
2. **Operating system**: `uname -a`
3. **Error messages**: Full error text and context
4. **Database information**: `sb_gstat -header database.fdb`
5. **Recent changes**: Any recent configuration or application changes

### **Support Resources**

- 📖 **Documentation**: Check related documentation sections
- 💬 **Community Forum**: [ScratchBird Community](https://community.scratchbird.org/)
- 🐛 **Bug Reports**: [GitHub Issues](https://github.com/dcalford/ScratchBird/issues)
- 📧 **Professional Support**: Contact ScratchBird support team

### **Useful Documentation Links**

- [**Database Engine**](05-database-engine.md) - Understanding internals
- [**Error Codes**](31-error-codes.md) - Complete error reference
- [**Performance Tuning**](20-performance.md) - Optimization guide
- [**Best Practices**](28-best-practices.md) - Recommended practices

---

## 💡 Prevention Tips

**Regular Maintenance**
- Validate databases weekly: `sb_gfix -validate database.fdb`
- Update statistics monthly: `SET STATISTICS INDEX ALL;`
- Monitor disk space and database growth
- Keep regular backups

**Configuration Management**
- Document configuration changes
- Test changes in development first
- Monitor performance after changes
- Keep configuration backups

**Monitoring Setup**
- Implement automated health checks
- Set up alerting for critical issues
- Monitor key performance metrics
- Review logs regularly

Remember: Most issues can be prevented with proper monitoring, regular maintenance, and following best practices!