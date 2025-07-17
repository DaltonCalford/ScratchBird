# ScratchBird v0.5.0 - macOS x86_64 Release

## Platform Information
- **Architecture**: macOS x86_64 (64-bit Intel)
- **Minimum Requirements**: macOS 10.14 (Mojave)
- **Tested On**: macOS 10.15+, macOS 11 Big Sur, macOS 12 Monterey, macOS 13 Ventura

## Package Contents

### Core Binaries (`bin/`)
- `sb_gbak` - Database backup and restore utility
- `sb_gstat` - Database statistics and analysis tool  
- `sb_gfix` - Database maintenance and repair utility
- `sb_gsec` - Security database management tool
- `sb_isql` - Interactive SQL command-line interface
- `gpre` - General Purpose Relation Engine preprocessor

### Libraries (`lib/`)
- `libsbclient.0.5.0.dylib` - ScratchBird client library (primary)
- `libsbclient.2.dylib` - Compatibility symlink for Firebird applications
- `libsbclient.dylib` - Development symlink
- `libsbintl.dylib` - International character set support

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

### Using Homebrew (Recommended)
```bash
brew tap scratchbird/tap
brew install scratchbird
```

### Manual Installation
```bash
# Copy binaries to system location
sudo cp bin/* /usr/local/bin/
sudo cp lib/* /usr/local/lib/

# Update dynamic library cache
sudo update_dyld_shared_cache

# Set up configuration
sudo mkdir -p /usr/local/etc/scratchbird
sudo cp conf/* /usr/local/etc/scratchbird/
```

### Using the Package Installer
1. Download `ScratchBird-v0.5.0-x86_64.pkg`
2. Double-click to run the installer
3. Follow the installation wizard

## Features

### New in v0.5.0
- **Complete ScratchBird Branding**: All Firebird references removed
- **Hierarchical Schema Support**: PostgreSQL-style nested schemas (finance.accounting.reports.table)
- **Schema-Aware Database Links**: Remote schema targeting with 5 resolution modes
- **2PC External Data Sources**: Two-Phase Commit protocol for distributed transactions
- **GPRE-Free Utilities**: 96.3% code reduction with modern C++ implementation
- **Enhanced Client Tools**: Full support for new schema features

### macOS-Specific Features
- **Keychain Integration**: Secure password storage
- **Launchd Services**: Native macOS service management
- **Code Signing**: All binaries are properly signed
- **Notarization**: Notarized for Gatekeeper compatibility

## Quick Start

### Create Database
```bash
sb_isql -user SYSDBA -password masterkey
SQL> CREATE DATABASE '/Users/$(whoami)/Documents/mydb.fdb';
SQL> quit;
```

### Backup Database
```bash
sb_gbak -b -user SYSDBA -password masterkey ~/Documents/mydb.fdb backup.fbk
```

### Check Database Statistics
```bash
sb_gstat -user SYSDBA -password masterkey -h ~/Documents/mydb.fdb
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

## Service Management

### Using launchctl
```bash
# Load ScratchBird service
sudo launchctl load /Library/LaunchDaemons/com.scratchbird.server.plist

# Start service
sudo launchctl start com.scratchbird.server

# Stop service
sudo launchctl stop com.scratchbird.server
```

## Development

### Xcode Integration
- Add to Header Search Paths: `/usr/local/include`
- Add to Library Search Paths: `/usr/local/lib`
- Link against: `libsbclient.dylib`

### Command Line Tools
```bash
# Compile with ScratchBird
clang++ -I/usr/local/include -L/usr/local/lib -lsbclient myapp.cpp -o myapp
```

## Environment Variables

```bash
# Add to ~/.zshrc or ~/.bash_profile
export SCRATCHBIRD_ROOT=/usr/local
export PATH=$SCRATCHBIRD_ROOT/bin:$PATH
export DYLD_LIBRARY_PATH=$SCRATCHBIRD_ROOT/lib:$DYLD_LIBRARY_PATH
```

## Technical Support

- **Documentation**: `/usr/local/share/doc/scratchbird/`
- **Examples**: `examples/` directory in this package
- **Configuration**: `/usr/local/etc/scratchbird/`
- **Logs**: `/usr/local/var/log/scratchbird/`

## Troubleshooting

### Gatekeeper Issues
```bash
# Allow execution if blocked by Gatekeeper
sudo xattr -rd com.apple.quarantine /usr/local/bin/sb_*
```

### Library Loading Issues
```bash
# Check library dependencies
otool -L /usr/local/bin/sb_isql
```

## Version Information
- **Version**: 0.5.0
- **Build**: SB-T0.5.0.1
- **Release Date**: July 2025
- **Compatibility**: ScratchBird v0.5.x series

## License
ScratchBird is distributed under the Initial Developer's Public License (IDPL).
See LICENSE file for complete terms.