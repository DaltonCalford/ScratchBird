# ScratchBird v0.5 - Developer Setup Guide

## Overview

ScratchBird is a PostgreSQL-style enhanced database engine based on Firebird 6.0.0.929, featuring SQL Dialect 4 enhancements, hierarchical schemas, and comprehensive cross-platform build support.

## Quick Start

### 1. Clone and Setup

```bash
git clone <repository-url>
cd ScratchBird
./setup_dev_environment.sh
```

The setup script will:
- Detect your operating system and architecture
- Check for required build tools and libraries
- Create a personalized build configuration
- Set up cross-compilation support if available

### 2. Build Options

```bash
# Build for Linux (native)
./build_scratchbird.sh build-linux

# Build for Windows (cross-compilation with MinGW)
./build_scratchbird.sh build-windows

# Build for all platforms
./build_scratchbird.sh build-all

# Clean build artifacts
./build_scratchbird.sh clean

# Show renamed tools and features
./build_scratchbird.sh tools
```

### 3. Alternative Build Methods

```bash
# Using cross-platform Makefile
make linux          # Build for Linux
make windows         # Build for Windows (if MinGW available)
make all            # Build for all platforms
make clean          # Clean build artifacts

# Using platform-specific configuration
./configure_platform.sh linux    # Configure for Linux
./configure_platform.sh windows  # Configure for Windows
```

## System Requirements

### Essential Build Tools

- **GCC/G++**: C/C++ compiler
- **Make**: Build automation tool
- **CMake**: Cross-platform build system
- **Autotools**: autoconf, automake, libtool

### System Libraries

- **libcds-dev**: Concurrent Data Structures library
- **libre2-dev**: RE2 regular expression library
- **libreadline-dev**: Command-line editing library

### Cross-Compilation (Windows)

- **mingw-w64**: Windows cross-compilation toolchain

## Platform-Specific Installation

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install build-essential cmake autoconf automake libtool
sudo apt-get install libcds-dev libre2-dev libreadline-dev
sudo apt-get install mingw-w64  # For Windows cross-compilation
```

### Fedora/RHEL/CentOS

```bash
sudo dnf install gcc gcc-c++ make cmake autoconf automake libtool
sudo dnf install libcds-devel re2-devel readline-devel
sudo dnf install mingw64-gcc mingw64-gcc-c++  # For Windows cross-compilation
```

### Arch Linux

```bash
sudo pacman -S base-devel cmake autoconf automake libtool
sudo pacman -S libcds re2 readline
sudo pacman -S mingw-w64-gcc  # For Windows cross-compilation
```

## Project Structure

```
ScratchBird/
├── build_scratchbird.sh      # Main build script
├── setup_dev_environment.sh  # Development environment setup
├── configure_platform.sh     # Platform-specific configuration
├── Makefile.cross            # Cross-platform Makefile
├── build_config.env          # Generated build configuration
├── src/                      # Source code
│   ├── include/              # Header files
│   ├── jrd/                  # Database engine
│   ├── dsql/                 # SQL parser
│   ├── isql/                 # Interactive SQL
│   └── ...
├── gen/                      # Generated files and build outputs
├── extern/                   # External libraries
└── packages/                 # Generated packages
```

## ScratchBird v0.5 Features

### Renamed Tools (sb_ prefix)

| Original | ScratchBird | Description |
|----------|-------------|-------------|
| isql | sb_isql | Interactive SQL shell |
| gbak | sb_gbak | Backup/restore utility |
| gfix | sb_gfix | Database repair utility |
| gsec | sb_gsec | Security manager |
| gstat | sb_gstat | Database statistics |
| nbackup | sb_nbackup | Physical backup utility |
| fbsvcmgr | sb_fbsvcmgr | Service manager |
| fbtracemgr | sb_fbtracemgr | Trace manager |

### SQL Dialect 4 Enhancements

1. **FROM-less SELECT statements**
   ```sql
   SELECT GEN_UUID();
   SELECT CURRENT_TIMESTAMP;
   SELECT 1 + 2 * 3;
   ```

2. **Multi-row INSERT VALUES**
   ```sql
   INSERT INTO employees (id, name, salary) VALUES 
     (1, 'John Doe', 50000),
     (2, 'Jane Smith', 55000),
     (3, 'Bob Johnson', 60000);
   ```

3. **Comprehensive SYNONYM support**
   ```sql
   CREATE SYNONYM emp FOR employees;
   CREATE SYNONYM finance.emp FOR accounting.employees;
   ```

4. **Hierarchical schemas (8 levels deep)**
   ```sql
   CREATE SCHEMA company.division.department;
   CREATE TABLE company.division.department.employees (
     id INTEGER,
     name VARCHAR(50)
   );
   ```

5. **Database links with schema resolution**
   ```sql
   CREATE DATABASE LINK remote_hr 
     TO 'server2:hr_db' 
     SCHEMA_MODE HIERARCHICAL
     LOCAL_SCHEMA 'hr'
     REMOTE_SCHEMA 'human_resources';
   ```

## Build Configuration

The `build_config.env` file contains environment-specific settings:

```bash
# System information
OS=linux
DISTRO=ubuntu
ARCH=x64

# Build settings
BUILD_TYPE=Release
PARALLEL_JOBS=8

# Cross-compilation
MINGW_AVAILABLE=true

# Library paths (auto-detected)
LIBCDS_PATH=/usr/lib/x86_64-linux-gnu/libcds-s.a
RE2_PATH=/usr/lib/x86_64-linux-gnu/libre2.so
```

## Troubleshooting

### Missing Libraries

If you get library errors:
```bash
# Re-run setup to check dependencies
./setup_dev_environment.sh

# Manually install missing libraries
sudo apt-get install libcds-dev libre2-dev  # Ubuntu/Debian
sudo dnf install libcds-devel re2-devel     # Fedora/RHEL
```

### Cross-Compilation Issues

For Windows cross-compilation problems:
```bash
# Check if MinGW is installed
which x86_64-w64-mingw32-gcc

# Install MinGW if missing
sudo apt-get install mingw-w64  # Ubuntu/Debian
sudo dnf install mingw64-gcc    # Fedora/RHEL
```

### Build Errors

For general build issues:
```bash
# Clean and rebuild
./build_scratchbird.sh clean
./build_scratchbird.sh build-linux

# Check build logs
tail -f build_linux.log
tail -f build_windows.log
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Run `./setup_dev_environment.sh` to configure your environment
4. Make your changes
5. Test on both Linux and Windows (if possible)
6. Submit a pull request

## Testing

```bash
# Build and test Linux version
./build_scratchbird.sh build-linux
./gen/Release/firebird/bin/sb_isql

# Test SQL Dialect 4 features
./gen/Release/firebird/bin/sb_isql -sql_dialect 4
```

## License

This project maintains the same license as the original Firebird project.

## Support

For build issues or questions:
- Check the troubleshooting section above
- Review build logs in `build_*.log` files
- Ensure all system dependencies are installed

---

**ScratchBird v0.5** - Enhanced database engine with PostgreSQL-style features and comprehensive cross-platform support.