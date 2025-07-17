# ScratchBird v0.5.0 - Linux x86_64 Release

## Platform Information
- **Architecture**: Linux x86_64 (64-bit Intel/AMD)
- **Minimum Requirements**: Linux kernel 3.2+, glibc 2.17+
- **Tested On**: Ubuntu 18.04+, CentOS 7+, RHEL 7+, SUSE 12+

## Package Contents

### Core Binaries (`bin/`)
- `sb_gbak` - Database backup and restore utility
- `sb_gstat` - Database statistics and analysis tool  
- `sb_gfix` - Database maintenance and repair utility
- `sb_gsec` - Security database management tool
- `sb_isql` - Interactive SQL command-line interface
- `gpre` - General Purpose Relation Engine preprocessor

### Libraries (`lib/`)
- `libsbclient.so.0.5.0` - ScratchBird client library (primary)
- `libsbclient.so.2` - Compatibility symlink for Firebird applications
- `libsbclient.so` - Development symlink

### Configuration Files (`conf/`)
- `scratchbird.conf` - Main ScratchBird server configuration
- `databases.conf` - Database aliases and connection settings
- `plugins.conf` - Plugin loading configuration
- `sbintl.conf` - International character set configuration
- `sbtrace.conf` - Performance tracing configuration
- `replication.conf` - Database replication settings

### Headers (`include/`)
- `scratchbird/` - Complete ScratchBird API headers for development
- C/C++ development headers for client applications

## Installation

### Using the Installer
```bash
sudo ./install.sh
```

### Manual Installation
```bash
# Copy binaries to system location
sudo cp bin/* /usr/local/bin/
sudo cp lib/* /usr/local/lib/
sudo ldconfig

# Set up configuration
sudo mkdir -p /etc/scratchbird
sudo cp conf/* /etc/scratchbird/
```

## Features

### New in v0.5.0
- **Complete ScratchBird Branding**: All Firebird references removed
- **Hierarchical Schema Support**: PostgreSQL-style nested schemas (finance.accounting.reports.table)
- **Schema-Aware Database Links**: Remote schema targeting with 5 resolution modes
- **2PC External Data Sources**: Two-Phase Commit protocol for distributed transactions
- **GPRE-Free Utilities**: 96.3% code reduction with modern C++ implementation
- **Enhanced Client Tools**: Full support for new schema features

### Core Database Features
- **ACID Compliance**: Full transaction support with MVCC
- **SQL Standards**: Enhanced SQL compliance with modern extensions
- **Multi-Platform**: Cross-platform compatibility
- **Embedded Mode**: No-server embedded database option
- **Network Protocol**: Efficient wire protocol for client/server

## Quick Start

### Create Database
```bash
sb_isql -user SYSDBA -password masterkey
SQL> CREATE DATABASE '/path/to/mydb.fdb';
SQL> quit;
```

### Backup Database
```bash
sb_gbak -b -user SYSDBA -password masterkey /path/to/mydb.fdb backup.fbk
```

### Check Database Statistics
```bash
sb_gstat -user SYSDBA -password masterkey -h /path/to/mydb.fdb
```

## Hierarchical Schema Examples

```sql
-- Create nested schema hierarchy
CREATE SCHEMA finance;
CREATE SCHEMA finance.accounting;
CREATE SCHEMA finance.accounting.reports;

-- Create table in nested schema
CREATE TABLE finance.accounting.reports.monthly_summary (
    id INTEGER,
    month_name VARCHAR(20),
    total_amount DECIMAL(15,2)
);

-- Query with full hierarchical path
SELECT * FROM finance.accounting.reports.monthly_summary;
```

## Technical Support

- **Documentation**: `/usr/share/doc/scratchbird/`
- **Examples**: `examples/` directory in this package
- **Configuration**: `/etc/scratchbird/`
- **Logs**: `/var/log/scratchbird/` (if using system installation)

## Version Information
- **Version**: 0.5.0
- **Build**: SB-T0.5.0.1
- **Release Date**: July 2025
- **Compatibility**: ScratchBird v0.5.x series

## License
ScratchBird is distributed under the Initial Developer's Public License (IDPL).
See LICENSE file for complete terms.