# ScratchBird Utilities Overview 🟢

ScratchBird includes 11 enhanced utilities that make database management effortless. Each utility provides 100% backward compatibility with original Firebird tools while adding powerful new features.

## 🛠️ Complete Utility Suite

### **📊 Core Database Utilities**

| Utility | Purpose | Skill Level | Key Features |
|---------|---------|-------------|--------------|
| [**sb_isql**](10-sb_isql.md) | Interactive SQL Shell | 🟢 Beginner | Schema support, enhanced editing, export formats |
| [**sb_gbak**](11-sb_gbak.md) | Backup & Restore | 🟢 Beginner | Compression, parallel processing, validation |
| [**sb_gstat**](12-sb_gstat.md) | Database Statistics | 🟡 Intermediate | Performance analysis, web interface, reporting |
| [**sb_gfix**](13-sb_gfix.md) | Database Maintenance | 🟡 Intermediate | Multi-level validation, repair, optimization |
| [**sb_gsec**](14-sb_gsec.md) | Security Management | 🟡 Intermediate | Enterprise security, MFA, audit trails |

### **🔧 Advanced System Utilities**

| Utility | Purpose | Skill Level | Key Features |
|---------|---------|-------------|--------------|
| **sb_guard** | Database Guardian | 🔴 Advanced | Multi-database monitoring, predictive analytics |
| **sb_svcmgr** | Service Manager | 🔴 Advanced | Queue optimization, bulk operations, scheduling |
| **sb_tracemgr** | Trace Analysis | 🔴 Advanced | Performance monitoring, security analysis |
| **sb_nbackup** | Incremental Backup | 🟡 Intermediate | 9-level backup, encryption, chain validation |
| **sb_gssplit** | File Management | 🟡 Intermediate | File splitting, compression, integrity checking |
| **sb_lock_print** | Lock Monitoring | 🔴 Advanced | Real-time monitoring, deadlock analysis |

---

## 🚀 Enhanced Features Overview

### **Universal Enhancements**
All ScratchBird utilities share these improvements:

#### **🎯 User Experience**
- **Intuitive Command Line**: Clear, helpful error messages
- **Progress Reporting**: Real-time progress bars and status updates
- **Multiple Output Formats**: JSON, XML, CSV, HTML export options
- **Cross-Platform**: Identical behavior on Linux, Windows, and other platforms

#### **⚡ Performance**
- **Multi-Threading**: Parallel processing where applicable
- **Memory Optimization**: Efficient memory usage for large operations
- **Compression**: Built-in compression for backups and file operations
- **Caching**: Intelligent caching for repeated operations

#### **🔐 Security**
- **Enhanced Authentication**: Support for modern authentication methods
- **Audit Logging**: Comprehensive audit trails for all operations
- **Encryption**: Data encryption capabilities throughout
- **Access Control**: Fine-grained permission controls

#### **📊 Monitoring**
- **Real-Time Statistics**: Live performance metrics
- **Export Capabilities**: Export results in multiple formats
- **Integration Ready**: API-friendly for automation and monitoring tools
- **Alerting**: Built-in alerting for critical conditions

---

## 💻 Command-Line Interface

### **Common Patterns**
All utilities follow consistent command-line patterns:

```bash
# Basic syntax
sb_<utility> [global_options] <operation> [operation_options] <target>

# Examples
sb_isql -user SYSDBA -password masterkey mydatabase.fdb
sb_gbak -backup -user SYSDBA mydatabase.fdb backup.fbk
sb_gstat -header -user SYSDBA mydatabase.fdb
```

### **Global Options**
Available across all utilities:

| Option | Description | Example |
|--------|-------------|---------|
| `-user <name>` | Database username | `-user SYSDBA` |
| `-password <pass>` | Database password | `-password masterkey` |
| `-role <role>` | SQL role name | `-role DB_ADMIN` |
| `-trusted` | Use trusted authentication | `-trusted` |
| `-z` | Show version information | `-z` |
| `-?` or `-help` | Show help information | `-help` |
| `-verbose` | Verbose output | `-verbose` |
| `-quiet` | Minimal output | `-quiet` |

### **Enhanced Help System**
Every utility provides comprehensive help:

```bash
# Quick help
sb_gbak -?

# Detailed help with examples
sb_gbak -help

# Operation-specific help
sb_gbak -backup -help
```

---

## 🎯 Getting Started with Each Utility

### **1. sb_isql - Interactive SQL** 🟢
**What it does**: Provides a command-line interface for executing SQL commands

**Quick Start**:
```bash
# Connect to database
sb_isql -user SYSDBA -password masterkey mydatabase.fdb

# Run SQL script
sb_isql -input script.sql -user SYSDBA mydatabase.fdb

# Export query results
sb_isql -output results.csv -format csv mydatabase.fdb
```

**Key Features**:
- Hierarchical schema support
- Command history and editing
- Multiple export formats
- Interactive and batch modes

### **2. sb_gbak - Backup & Restore** 🟢
**What it does**: Creates backups and restores databases

**Quick Start**:
```bash
# Create backup
sb_gbak -backup -user SYSDBA mydatabase.fdb backup.fbk

# Restore database
sb_gbak -restore -user SYSDBA backup.fbk newdatabase.fdb

# Compressed backup
sb_gbak -backup -compress -user SYSDBA mydatabase.fdb backup.fbk
```

**Key Features**:
- Compression and encryption
- Parallel processing
- Backup validation
- Progress monitoring

### **3. sb_gstat - Database Statistics** 🟡
**What it does**: Analyzes database performance and structure

**Quick Start**:
```bash
# Database overview
sb_gstat -header -user SYSDBA mydatabase.fdb

# Table analysis
sb_gstat -table customers -user SYSDBA mydatabase.fdb

# Full analysis with recommendations
sb_gstat -analyze -recommendations -user SYSDBA mydatabase.fdb
```

**Key Features**:
- Performance analysis
- Optimization recommendations
- Web-based reports
- Historical trending

### **4. sb_gfix - Database Maintenance** 🟡
**What it does**: Validates, repairs, and optimizes databases

**Quick Start**:
```bash
# Validate database
sb_gfix -validate -user SYSDBA mydatabase.fdb

# Repair database
sb_gfix -mend -user SYSDBA mydatabase.fdb

# Full optimization
sb_gfix -optimize -all -user SYSDBA mydatabase.fdb
```

**Key Features**:
- Multi-level validation
- Automatic repair
- Performance optimization
- Integrity checking

### **5. sb_gsec - Security Management** 🟡
**What it does**: Manages database users and security

**Quick Start**:
```bash
# Add new user
sb_gsec -add newuser -password secret123

# List all users
sb_gsec -display

# Set up role-based access
sb_gsec -role -create sales_team -members user1,user2
```

**Key Features**:
- Multi-factor authentication
- Role-based access control
- Password policies
- Security auditing

---

## 🔧 Advanced Utilities Deep Dive

### **sb_guard - Database Guardian** 🔴
**Purpose**: Monitors and protects multiple databases

**Capabilities**:
- **Automatic Restart**: Restarts failed database services
- **Health Monitoring**: Continuously monitors database health
- **Predictive Analytics**: Predicts potential issues before they occur
- **Multi-Database**: Manages multiple database instances

**Example**:
```bash
# Start guardian for multiple databases
sb_guard -config guardian.conf -databases db1.fdb,db2.fdb,db3.fdb

# Monitor specific metrics
sb_guard -monitor -metrics cpu,memory,locks -interval 30
```

### **sb_svcmgr - Service Manager** 🔴
**Purpose**: Advanced service and task management

**Capabilities**:
- **Queue Management**: Manages service request queues
- **Bulk Operations**: Executes multiple operations efficiently
- **Scheduling**: Schedules maintenance tasks
- **Load Balancing**: Distributes work across resources

**Example**:
```bash
# Queue multiple backup operations
sb_svcmgr -queue -operation backup -databases *.fdb -schedule daily

# Monitor service status
sb_svcmgr -status -verbose
```

### **sb_tracemgr - Trace Analysis** 🔴
**Purpose**: Performance monitoring and security analysis

**Capabilities**:
- **Performance Tracing**: Tracks query performance
- **Security Monitoring**: Detects suspicious activities
- **Bottleneck Analysis**: Identifies performance bottlenecks
- **Real-time Alerts**: Sends alerts for critical conditions

**Example**:
```bash
# Start performance trace
sb_tracemgr -start -config perf_trace.conf -database mydb.fdb

# Analyze security events
sb_tracemgr -analyze -security -timeframe "last 24 hours"
```

### **sb_nbackup - Incremental Backup** 🟡
**Purpose**: Advanced incremental backup management

**Capabilities**:
- **9-Level Hierarchy**: Up to 9 incremental backup levels
- **Encryption**: Built-in backup encryption
- **Chain Validation**: Validates backup chain integrity
- **Compression**: Advanced compression algorithms

**Example**:
```bash
# Full backup (level 0)
sb_nbackup -level 0 -user SYSDBA mydb.fdb mydb_full.nb

# Incremental backup (level 1)
sb_nbackup -level 1 -user SYSDBA mydb.fdb mydb_incr1.nb

# Validate backup chain
sb_nbackup -validate -chain mydb_full.nb,mydb_incr1.nb
```

### **sb_gssplit - File Management** 🟡
**Purpose**: Database file splitting and management

**Capabilities**:
- **File Splitting**: Splits large database files
- **Compression**: Compresses database files
- **Integrity Checking**: Validates file integrity
- **Reconstruction**: Rebuilds files from splits

**Example**:
```bash
# Split large database file
sb_gssplit -split -size 2GB -user SYSDBA largdb.fdb

# Compress database file
sb_gssplit -compress -algorithm lz4 -user SYSDBA mydb.fdb
```

### **sb_lock_print - Lock Monitoring** 🔴
**Purpose**: Real-time lock monitoring and analysis

**Capabilities**:
- **Real-time Monitoring**: Live lock status monitoring
- **Deadlock Detection**: Automatic deadlock detection
- **Contention Analysis**: Identifies lock contention hotspots
- **Historical Analysis**: Tracks lock patterns over time

**Example**:
```bash
# Monitor locks in real-time
sb_lock_print -monitor -real-time -user SYSDBA mydb.fdb

# Analyze deadlock patterns
sb_lock_print -analyze -deadlocks -timeframe "last week"
```

---

## 🔗 Utility Integration

### **Automation Scripts**
Combine utilities for powerful automation:

```bash
#!/bin/bash
# Complete database maintenance script

# 1. Validate database
sb_gfix -validate -user SYSDBA mydb.fdb || exit 1

# 2. Update statistics
sb_gstat -update-stats -user SYSDBA mydb.fdb

# 3. Create backup
sb_gbak -backup -compress -user SYSDBA mydb.fdb backup_$(date +%Y%m%d).fbk

# 4. Analyze performance
sb_gstat -analyze -export report_$(date +%Y%m%d).html mydb.fdb

# 5. Clean up old backups
find /backups -name "backup_*.fbk" -mtime +30 -delete

echo "Maintenance completed successfully"
```

### **Monitoring Dashboard**
Create comprehensive monitoring:

```bash
# Real-time dashboard script
#!/bin/bash
while true; do
    clear
    echo "=== ScratchBird Database Dashboard ==="
    echo
    
    # Database status
    sb_gfix -info -brief -user SYSDBA mydb.fdb
    
    # Current connections
    sb_isql -execute "SELECT COUNT(*) FROM MON\$ATTACHMENTS" mydb.fdb
    
    # Lock status
    sb_lock_print -summary -user SYSDBA mydb.fdb
    
    # Performance metrics
    sb_gstat -performance -brief mydb.fdb
    
    sleep 30
done
```

---

## 📚 Documentation Links

### **Detailed Utility Guides**
- [**sb_isql - Interactive SQL**](10-sb_isql.md) - Complete SQL shell guide
- [**sb_gbak - Backup/Restore**](11-sb_gbak.md) - Backup and restore operations
- [**sb_gstat - Statistics**](12-sb_gstat.md) - Database analysis and monitoring
- [**sb_gfix - Maintenance**](13-sb_gfix.md) - Database maintenance and repair
- [**sb_gsec - Security**](14-sb_gsec.md) - User and security management
- [**Advanced Utilities**](15-advanced-utilities.md) - Guard, Service Manager, Trace Manager
- [**Backup Utilities**](16-backup-utilities.md) - NBackup and file management

### **Related Topics**
- [**Command Reference**](29-command-reference.md) - All commands and options
- [**Best Practices**](28-best-practices.md) - Recommended usage patterns
- [**Troubleshooting**](25-troubleshooting.md) - Common issues and solutions

---

## 💡 Pro Tips

> **Combine Utilities**: Use multiple utilities together for powerful workflows
> ```bash
> sb_gstat -analyze mydb.fdb | sb_gbak -optimize-from-stats mydb.fdb
> ```

> **Automate Everything**: Create scripts for routine maintenance
> ```bash
> # Daily maintenance script
> sb_gfix -validate && sb_gbak -backup && sb_gstat -update-stats
> ```

> **Monitor Continuously**: Set up real-time monitoring
> ```bash
> # Start background monitoring
> sb_guard -daemon -alert-email admin@company.com
> ```

**🎯 Ready to master the utilities?** Start with [**sb_isql**](10-sb_isql.md) for interactive database work, then explore the other utilities based on your needs!