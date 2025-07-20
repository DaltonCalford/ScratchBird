# ScratchBird Database System Documentation

Welcome to the comprehensive documentation for ScratchBird v0.5, a powerful database management system built on proven Firebird technology with modern enhancements.

## Documentation Overview

This documentation is designed to guide users from novice to expert through all aspects of the ScratchBird database system.

### 📖 **Getting Started**
- [**ScratchBird Overview**](01-overview.md) - What is ScratchBird and why use it?
- [**Quick Start Guide**](02-quick-start.md) - Get up and running in minutes
- [**Installation Guide**](03-installation.md) - Complete installation instructions
- [**First Database**](04-first-database.md) - Create your first ScratchBird database

### 🔧 **Core Components**
- [**Database Engine**](05-database-engine.md) - Understanding the ScratchBird engine
- [**SQL Language**](06-sql-language.md) - ScratchBird SQL syntax and features
- [**Hierarchical Schemas**](07-hierarchical-schemas.md) - Advanced schema management
- [**Security System**](08-security.md) - User management and permissions

### 🛠️ **Utilities Reference**
- [**Utilities Overview**](09-utilities-overview.md) - All 11 enhanced utilities explained
- [**sb_isql - Interactive SQL**](10-sb_isql.md) - SQL command-line interface
- [**sb_gbak - Backup/Restore**](11-sb_gbak.md) - Database backup and restore
- [**sb_gstat - Statistics**](12-sb_gstat.md) - Database analysis and monitoring
- [**sb_gfix - Maintenance**](13-sb_gfix.md) - Database repair and validation
- [**sb_gsec - Security**](14-sb_gsec.md) - User and security management
- [**Advanced Utilities**](15-advanced-utilities.md) - Guard, Service Manager, Trace Manager
- [**Backup Utilities**](16-backup-utilities.md) - NBackup and file management

### 💻 **Development**
- [**API Reference**](17-api-reference.md) - Complete programming interface
- [**SBDatabase Class**](18-sbdatabase-class.md) - Database connection framework
- [**Error Handling**](19-error-handling.md) - Exception management
- [**Performance Tuning**](20-performance.md) - Optimization techniques

### 🏢 **Administration**
- [**Administrator Guide**](21-admin-guide.md) - Database administration
- [**Configuration**](22-configuration.md) - System configuration options
- [**Monitoring**](23-monitoring.md) - System monitoring and alerts
- [**Backup Strategies**](24-backup-strategies.md) - Enterprise backup planning

### 🆘 **Support**
- [**Troubleshooting**](25-troubleshooting.md) - Common issues and solutions
- [**FAQ**](26-faq.md) - Frequently asked questions
- [**Migration Guide**](27-migration.md) - Migrating from Firebird
- [**Best Practices**](28-best-practices.md) - Recommended practices

### 📚 **Reference**
- [**Command Reference**](29-command-reference.md) - All commands and options
- [**Configuration Reference**](30-configuration-reference.md) - Complete configuration
- [**Error Codes**](31-error-codes.md) - Complete error code reference
- [**Glossary**](32-glossary.md) - Terms and definitions

---

## Documentation Conventions

### **Skill Level Indicators**
- 🟢 **Beginner** - No prior database experience required
- 🟡 **Intermediate** - Basic database knowledge helpful
- 🔴 **Advanced** - Requires database administration experience

### **Platform Indicators**
- 🐧 **Linux** - Linux-specific information
- 🪟 **Windows** - Windows-specific information
- 🌐 **Cross-Platform** - Works on all platforms

### **Code Examples**
```sql
-- SQL examples use this formatting
SELECT * FROM my_table;
```

```bash
# Shell commands use this formatting
sb_isql -user SYSDBA mydatabase.fdb
```

```cpp
// C++ API examples use this formatting
SBDatabase db;
db.connect("mydatabase.fdb");
```

### **Important Notes**
> 💡 **Tip**: Helpful tips and best practices
> 
> ⚠️ **Warning**: Important warnings and cautions
> 
> 🔧 **Technical**: Technical details for advanced users

---

## Quick Navigation

### **I'm New to Databases**
Start with [ScratchBird Overview](01-overview.md) → [Quick Start](02-quick-start.md) → [First Database](04-first-database.md)

### **I'm Migrating from Firebird** 
Start with [Migration Guide](27-migration.md) → [Hierarchical Schemas](07-hierarchical-schemas.md) → [Enhanced Utilities](09-utilities-overview.md)

### **I'm a Developer**
Start with [API Reference](17-api-reference.md) → [SBDatabase Class](18-sbdatabase-class.md) → [Error Handling](19-error-handling.md)

### **I'm a Database Administrator**
Start with [Administrator Guide](21-admin-guide.md) → [Configuration](22-configuration.md) → [Monitoring](23-monitoring.md)

### **I Need Help Right Now**
Go to [Troubleshooting](25-troubleshooting.md) → [FAQ](26-faq.md) → [Error Codes](31-error-codes.md)

---

## About This Documentation

**Version**: ScratchBird v0.5.0  
**Last Updated**: July 2025  
**Target Audience**: Database novices to advanced administrators  
**Format**: Markdown with code examples and practical tutorials

For the most up-to-date documentation, visit the [ScratchBird Documentation Portal](https://scratchbird.org/docs/).

**Need help?** Join our community at [ScratchBird Community Forum](https://community.scratchbird.org/).