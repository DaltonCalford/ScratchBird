# ScratchBird sb_isql Comprehensive Test Suite

## Overview

This directory contains comprehensive test scripts that validate all ScratchBird database features using sb_isql. Each test script captures both input and output to provide complete validation of functionality.

## Test Organization

### Core Database Operations
- `01_basic_database_operations.sh` - Database creation, connection, basic DDL
- `02_table_operations.sh` - Table creation, constraints, data manipulation
- `03_view_operations.sh` - View creation, updatable views
- `04_procedure_function_operations.sh` - Stored procedures and functions

### Advanced Features
- `05_hierarchical_schemas.sh` - Nested schema functionality (up to 11 levels)
- `06_partial_hash_indexes.sh` - O(1) lookup with WHERE clause filtering
- `07_gin_indexes.sh` - Full-text search and array indexing
- `08_spatial_data.sh` - Geographic and geometric data management

### Security and Administration
- `09_security_authentication.sh` - User/role management, permissions
- `10_database_links.sh` - Schema-aware cross-database connectivity
- `11_utility_programs.sh` - All 12 ScratchBird utilities
- `12_administrative_features.sh` - Monitoring, backup/restore, maintenance

### Comprehensive Testing
- `run_all_tests.sh` - Execute all test scripts in sequence
- `validate_results.sh` - Analyze test outputs for failures

## Running Tests

### Individual Test
```bash
cd /home/dcalford/Documents/claude/GitHubRepo/ScratchBird/tests/sb_isql_tests
./01_basic_database_operations.sh
```

### All Tests
```bash
./run_all_tests.sh
```

### Results Analysis
```bash
./validate_results.sh
```

## Test Output Format

Each test creates:
- `testname_input.sql` - SQL commands executed
- `testname_output.txt` - Complete sb_isql output
- `testname_results.log` - Test execution summary

## Requirements

- ScratchBird Alpha 0.5.0+ installation
- sb_isql executable in PATH
- Write permissions in test directory
- Minimum 500MB free disk space for test databases

## Test Coverage

- **48 documented features** from ScratchBird/doc/ directory
- **12 utility programs** complete functionality
- **All DDL operations** (CREATE/ALTER/DROP lifecycle)
- **Advanced SQL features** (CTEs, window functions, JSON, arrays)
- **Security model** (users, roles, permissions, mappings)
- **Performance features** (indexing, optimization, monitoring)