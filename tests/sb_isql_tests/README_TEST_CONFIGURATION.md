# ScratchBird Test Configuration Guide

## Overview

The ScratchBird test suite now uses centralized configuration through `test_config.sh`. This eliminates hardcoded paths and credentials, making tests portable and customizable.

## Quick Start

### Default Configuration (No Setup Required)
```bash
# Run tests with default settings
./run_all_tests.sh
```

### Custom Configuration
```bash
# Set environment variables before running tests
export SB_TEST_USER="your_user"
export SB_TEST_PASSWORD="your_password"
export SB_TEST_DB_LOCATION="local"
./run_all_tests.sh
```

## Configuration Options

### Database Connection
- `SB_TEST_USER` - Database user (default: SYSDBA)
- `SB_TEST_PASSWORD` - Database password (default: masterkey)
- `SB_TEST_CHARSET` - Character set (default: UTF8)
- `SB_TEST_PAGE_SIZE` - Database page size (default: 8192)

### Database Location
- `SB_TEST_DB_LOCATION` - Where to store test databases:
  - `"local"` - Store in test directory (default)
  - `"remote"` - Store on remote server
  - `"temp"` - Store in system temp directory

### Remote Server (when using "remote" location)
- `SB_TEST_SERVER` - Server hostname/IP
- `SB_TEST_PORT` - Server port (default: 3050)

### Directory Paths
- `SB_TEST_BASE_DIR` - Base test directory
- `SB_INSTALL_DIR` - ScratchBird installation directory

### Test Behavior
- `SB_TEST_CLEANUP` - Clean up databases after tests (default: true)
- `SB_TEST_VERBOSE` - Verbose output (default: false)
- `SB_TEST_STOP_ON_ERROR` - Stop on first error (default: false)

## Usage Examples

### Local Testing (Default)
```bash
# Uses local database storage
./run_all_tests.sh
```

### Remote Server Testing
```bash
export SB_TEST_DB_LOCATION="remote"
export SB_TEST_SERVER="database-server.company.com"
export SB_TEST_USER="testuser"
export SB_TEST_PASSWORD="testpass"
./run_all_tests.sh
```

### Temporary Database Testing
```bash
export SB_TEST_DB_LOCATION="temp"
export SB_TEST_CLEANUP="true"
./run_all_tests.sh
```

### Verbose Testing with Custom Installation
```bash
export SB_INSTALL_DIR="/opt/scratchbird"
export SB_TEST_VERBOSE="true"
export SB_TEST_STOP_ON_ERROR="true"
./run_all_tests.sh
```

## Configuration File Locations

### Generated Files
All test results and databases are stored relative to configuration:
- Results: `$SB_TEST_RESULTS_DIR/`
- Databases: `$SB_TEST_DB_DIR/` (for local/remote)
- Databases: `/tmp/` (for temp mode)
- Logs: `$SB_TEST_LOGS_DIR/`

### Where Files Are Created

**Local Mode** (`SB_TEST_DB_LOCATION="local"`):
- Databases: `./test_databases/`
- Results: `./results/`
- SQL files: `./results/`

**Remote Mode** (`SB_TEST_DB_LOCATION="remote"`):
- Databases: Created on remote server
- Results: `./results/` (local)
- SQL files: `./results/` (local)

**Temp Mode** (`SB_TEST_DB_LOCATION="temp"`):
- Databases: `/tmp/scratchbird_test_*`
- Results: `./results/`
- SQL files: `./results/`

## Validating Configuration

### Check Current Configuration
```bash
# Display current configuration settings
./test_config.sh
```

### Validate Test Setup
```bash
# Check if configuration is valid
source test_config.sh && echo "Configuration OK"
```

### Quick Results Check
```bash
# After running tests, validate results
./validate_results.sh
```

## Troubleshooting

### Permission Issues
```bash
# Ensure test directory is writable
chmod +x *.sh
```

### ScratchBird Not Found
```bash
# Set correct installation path
export SB_INSTALL_DIR="/path/to/your/scratchbird/installation"
```

### Remote Connection Issues
```bash
# Test remote connection manually
export SB_TEST_SERVER="your-server"
$SB_INSTALL_DIR/bin/sb_isql -z
```

## Environment File

Create a `.env` file in the test directory for persistent configuration:
```bash
# .env file example
SB_TEST_USER=myuser
SB_TEST_PASSWORD=mypassword
SB_TEST_DB_LOCATION=local
SB_TEST_VERBOSE=true
SB_INSTALL_DIR=/opt/scratchbird
```

Then source it before testing:
```bash
source .env
./run_all_tests.sh
```

## Integration with CI/CD

### GitHub Actions Example
```yaml
- name: Run ScratchBird Tests
  env:
    SB_TEST_DB_LOCATION: temp
    SB_TEST_CLEANUP: true
    SB_TEST_VERBOSE: false
  run: |
    cd tests/sb_isql_tests
    ./run_all_tests.sh
```

### Docker Environment
```bash
docker run -e SB_TEST_DB_LOCATION=temp \
           -e SB_TEST_CLEANUP=true \
           -v $(pwd):/tests \
           scratchbird:latest \
           /tests/run_all_tests.sh
```

This centralized configuration system makes ScratchBird tests flexible, portable, and suitable for various deployment scenarios while eliminating hardcoded paths and credentials.