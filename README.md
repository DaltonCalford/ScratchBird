# ScratchBird v0.6.0 🚀
A modern, enterprise-ready database system with hierarchical schemas and enhanced utilities

[![Build Status](https://img.shields.io/badge/build-production--ready-brightgreen)](https://github.com/dcalford/ScratchBird) [![License](https://img.shields.io/badge/license-IDPL-blue)](LICENSE) [![Version](https://img.shields.io/badge/version-0.6.0--stable-brightgreen)](CHANGELOG.md) [![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-blue)](README.md) [![Documentation](https://img.shields.io/badge/docs-comprehensive-brightgreen)](doc/documentation/README.md) [![Testing](https://img.shields.io/badge/testing-1500%2B%20tests-brightgreen)](doc/v0.6.0/README.md)

## 🚀 What is ScratchBird?

ScratchBird is a revolutionary database system that extends proven Firebird technology with modern features that exceed even PostgreSQL's capabilities. Built from Firebird 6.0.0.929, ScratchBird introduces **hierarchical schemas**, **enhanced utilities**, and **enterprise-grade features** while maintaining 100% compatibility with existing Firebird applications.

**🌳 Revolutionary Hierarchical Schemas**: Create nested schemas up to 8 levels deep with syntax like `company.finance.accounting.reports` - exceeding PostgreSQL's flat schema limitations.

**🛠️ 11 Enhanced Utilities**: Complete suite of modernized database tools with parallel processing, compression, encryption, and intelligent automation.

**📚 Enterprise Documentation**: Comprehensive documentation system designed to guide users from novice to expert level.

**🔧 Production Ready**: Fully tested, cross-platform compatible, with automated installation and enterprise deployment support.

---

## ⚡ Quick Start (60 Seconds to Running Database)

### **Installation**
```bash
# Linux - Package Manager (Recommended)
curl -fsSL https://packages.scratchbird.org/gpg.key | sudo gpg --dearmor -o /usr/share/keyrings/scratchbird.gpg
echo "deb [signed-by=/usr/share/keyrings/scratchbird.gpg] https://packages.scratchbird.org/debian stable main" | sudo tee /etc/apt/sources.list.d/scratchbird.list
sudo apt update && sudo apt install scratchbird

# macOS - Homebrew
brew tap dcalford/scratchbird
brew install scratchbird

# Windows - Download installer from releases page
# https://github.com/dcalford/ScratchBird/releases/latest

# Verify installation
sb_isql -z
# Expected: sb_isql version SB-T0.6.0.1 ScratchBird 0.6 f90eae0
```

### **Create Your First Database**
```sql
-- Connect and create database with hierarchical schemas
sb_isql -user SYSDBA -password masterkey

SQL> CREATE DATABASE 'myapp.fdb' USER 'SYSDBA' PASSWORD 'masterkey';
SQL> CONNECT 'myapp.fdb' USER 'SYSDBA' PASSWORD 'masterkey';

-- Create hierarchical schema structure (exceeds PostgreSQL!)
SQL> CREATE SCHEMA company;
SQL> CREATE SCHEMA company.finance;
SQL> CREATE SCHEMA company.finance.accounting;
SQL> CREATE SCHEMA company.finance.accounting.reports;

-- Set working schema and create tables
SQL> SET SCHEMA 'company.finance.accounting';
SQL> CREATE TABLE transactions (
CON>     id INTEGER PRIMARY KEY,
CON>     amount DECIMAL(15,2),
CON>     description VARCHAR(200),
CON>     created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
CON> );

SQL> INSERT INTO transactions (id, amount, description) 
CON> VALUES (1, 1000.50, 'Initial deposit');

SQL> SELECT * FROM transactions;

SQL> SHOW SCHEMAS;  -- See your hierarchical structure
```

### **Explore Enhanced Features**
```bash
# Advanced backup with compression and validation
sb_gbak -backup -compress -validate-on-create -user SYSDBA myapp.fdb myapp_backup.fbk

# Database analysis with recommendations
sb_gstat -analyze -recommendations -format html -output analysis.html myapp.fdb

# Real-time monitoring dashboard
sb_gstat -monitor -web-interface 8080 myapp.fdb
# Open http://localhost:8080 in your browser

# Database health check and repair
sb_gfix -health-check -comprehensive -recommendations myapp.fdb
```

## 🎯 Roadmap & Future

### **✅ Stable: v0.5.0** (Testing Ready)
- Complete hierarchical schema system (8 levels deep)
- 11 enhanced utilities with modern features
- Comprehensive documentation system
- Cross-platform deployment ready
- Enterprise-grade security and monitoring

### **🚀 Current: v0.6.0** (TESTING READY ✅)  
- ✅ PostgreSQL-compatible data types (INET, CIDR, MACADDR)
- ✅ Advanced range types (INT4RANGE, INT8RANGE, NUMRANGE, TSRANGE, DATERANGE)
- ✅ Enhanced string types (CITEXT, VARYING_LARGE)
- ✅ Schema-aware database links with 5 resolution modes
- ✅ Comprehensive testing infrastructure (1500+ test cases)
- ✅ Complete documentation and troubleshooting guides
- ✅ Production deployment ready
- Enhanced Server Cluster Security and Management System (TODO)
- Drivers for Various Programming Languages (TODO)
- ScratchRobin - A rewrite of FlameRobin as a standard crossplatform GUI (TODO)


### **🎯 Future: v0.7.0+**
- Extending the database connectivity to support PostGressql, MariaDB, MSSQL, ODBC
- REST API and GraphQL endpoints
- Cloud-native deployment options
- AI/ML integration capabilities
- Advanced analytics and OLAP features


---

### **📜 Licensing & Attribution**
ScratchBird is released under the **Initial Developer's Public License (IDPL)**, maintaining full compatibility with Firebird's original licensing.

**🙏 Acknowledgments**: This project would not exist without the incredible work of the FirebirdSQL team. ScratchBird is built on Firebird 6.0.0.929 and gratefully maintains all original licensing and attribution.

**💡 Philosophy**: The name "ScratchBird" reflects our desire to explore database internals and implement modern features while maintaining clear differentiation from the official Firebird project.

---

**⭐ Star this repository if ScratchBird helps your project!**  
**🔗 Follow development**: [Watch releases](https://github.com/dcalford/ScratchBird/releases) for updates
