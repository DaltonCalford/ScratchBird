# ScratchBird v0.5.0 - Windows x64 Release

## Platform Information
- **Architecture**: Windows x64 (64-bit Intel/AMD)
- **Minimum Requirements**: Windows 7 SP1 / Windows Server 2008 R2 SP1
- **Tested On**: Windows 10, Windows 11, Windows Server 2019/2022

## Package Contents

### Core Binaries (`bin/`)
- `sb_gbak.exe` - Database backup and restore utility
- `sb_gstat.exe` - Database statistics and analysis tool  
- `sb_gfix.exe` - Database maintenance and repair utility
- `sb_gsec.exe` - Security database management tool
- `sb_isql.exe` - Interactive SQL command-line interface
- `gpre.exe` - General Purpose Relation Engine preprocessor

### Libraries (`lib/`)
- `sbclient.dll` - ScratchBird client library (primary)
- `sbclient.lib` - Import library for development
- `sbintl.dll` - International character set support
- `ib_util.dll` - Utility functions library

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
1. Run `ScratchBirdInstaller-v0.5.0-x64.msi` as Administrator
2. Follow the installation wizard
3. Configure Windows services if needed

### Manual Installation
1. Extract all files to `C:\Program Files\ScratchBird\`
2. Add `C:\Program Files\ScratchBird\bin` to system PATH
3. Register DLLs if needed:
   ```cmd
   regsvr32 C:\Program Files\ScratchBird\lib\sbclient.dll
   ```

## Features

### New in v0.5.0
- **Complete ScratchBird Branding**: All Firebird references removed
- **Hierarchical Schema Support**: PostgreSQL-style nested schemas (finance.accounting.reports.table)
- **Schema-Aware Database Links**: Remote schema targeting with 5 resolution modes
- **2PC External Data Sources**: Two-Phase Commit protocol for distributed transactions
- **GPRE-Free Utilities**: 96.3% code reduction with modern C++ implementation
- **Enhanced Client Tools**: Full support for new schema features

### Windows-Specific Features
- **Windows Service**: Optional service installation for server mode
- **Event Log Integration**: Windows Event Log support
- **NTLM Authentication**: Windows integrated authentication
- **UNC Path Support**: Network database paths support

## Quick Start

### Create Database
```cmd
sb_isql -user SYSDBA -password masterkey
SQL> CREATE DATABASE 'C:\Databases\mydb.fdb';
SQL> quit;
```

### Backup Database
```cmd
sb_gbak -b -user SYSDBA -password masterkey C:\Databases\mydb.fdb backup.fbk
```

### Check Database Statistics
```cmd
sb_gstat -user SYSDBA -password masterkey -h C:\Databases\mydb.fdb
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

## Windows Service Configuration

### Install Service
```cmd
sb_service -install -name ScratchBird -config "C:\Program Files\ScratchBird\conf\scratchbird.conf"
```

### Start/Stop Service
```cmd
net start ScratchBird
net stop ScratchBird
```

## Development

### Visual Studio Integration
- Include path: `C:\Program Files\ScratchBird\include`
- Library path: `C:\Program Files\ScratchBird\lib`
- Link against: `sbclient.lib`

### Sample Connection Code
```cpp
#include <scratchbird/api.h>

// Connection example code here
```

## Technical Support

- **Documentation**: `C:\Program Files\ScratchBird\doc\`
- **Examples**: `examples\` directory in this package
- **Configuration**: `C:\Program Files\ScratchBird\conf\`
- **Logs**: `C:\ProgramData\ScratchBird\logs\`

## Version Information
- **Version**: 0.5.0
- **Build**: SB-T0.5.0.1
- **Release Date**: July 2025
- **Compatibility**: ScratchBird v0.5.x series

## License
ScratchBird is distributed under the Initial Developer's Public License (IDPL).
See LICENSE file for complete terms.