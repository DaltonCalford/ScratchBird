#!/bin/bash

# test_config.sh
# Centralized configuration for ScratchBird test scripts
# Source this file in all test scripts for consistent configuration

# =================================================================
# SCRATCHBIRD TEST CONFIGURATION
# =================================================================

# Database Connection Configuration
# =================================
# Default connection parameters (can be overridden by environment variables)
export SB_TEST_USER="${SB_TEST_USER:-SYSDBA}"
export SB_TEST_PASSWORD="${SB_TEST_PASSWORD:-masterkey}"
export SB_TEST_CHARSET="${SB_TEST_CHARSET:-UTF8}"
export SB_TEST_PAGE_SIZE="${SB_TEST_PAGE_SIZE:-8192}"

# Database Server Configuration
# =============================
# For local databases, leave SERVER empty or set to "localhost"
# For remote databases, set to server hostname/IP
export SB_TEST_SERVER="${SB_TEST_SERVER:-}"
export SB_TEST_PORT="${SB_TEST_PORT:-3050}"

# Directory Configuration
# =======================
# Base test directory (can be overridden)
export SB_TEST_BASE_DIR="${SB_TEST_BASE_DIR:-/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/tests/sb_isql_tests}"

# ScratchBird Installation Directory
export SB_INSTALL_DIR="${SB_INSTALL_DIR:-/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/release/alpha0.5.0/linux-x86_64}"

# Derived paths (automatically calculated)
export SB_TEST_RESULTS_DIR="$SB_TEST_BASE_DIR/results"
export SB_TEST_DB_DIR="$SB_TEST_BASE_DIR/test_databases"
export SB_TEST_LOGS_DIR="$SB_TEST_BASE_DIR/logs"
export SB_TEST_TEMP_DIR="$SB_TEST_BASE_DIR/temp"

# ScratchBird binary paths
export SB_ISQL="$SB_INSTALL_DIR/bin/sb_isql"
export SB_GBAK="$SB_INSTALL_DIR/bin/sb_gbak"
export SB_GSTAT="$SB_INSTALL_DIR/bin/sb_gstat"
export SB_GFIX="$SB_INSTALL_DIR/bin/sb_gfix"
export SB_GSEC="$SB_INSTALL_DIR/bin/sb_gsec"
export SCRATCHBIRD="$SB_INSTALL_DIR"

# Database Location Configuration
# ===============================
# Choose database storage location:
# - "local": Store databases in local test directory
# - "remote": Store databases on remote server (requires SB_TEST_SERVER)
# - "temp": Store databases in system temp directory
export SB_TEST_DB_LOCATION="${SB_TEST_DB_LOCATION:-local}"

# Test Execution Configuration
# ============================
export SB_TEST_CLEANUP="${SB_TEST_CLEANUP:-true}"        # Clean up test databases after completion
export SB_TEST_VERBOSE="${SB_TEST_VERBOSE:-false}"       # Verbose output during tests
export SB_TEST_STOP_ON_ERROR="${SB_TEST_STOP_ON_ERROR:-false}"  # Stop testing on first error
export SB_TEST_PARALLEL="${SB_TEST_PARALLEL:-false}"     # Run tests in parallel (experimental)

# =================================================================
# FUNCTIONS FOR DATABASE PATH GENERATION
# =================================================================

# Generate database path based on configuration
generate_db_path() {
    local test_name="$1"
    local db_name="$2"
    
    case "$SB_TEST_DB_LOCATION" in
        "local")
            echo "$SB_TEST_DB_DIR/${test_name}_${db_name}.fdb"
            ;;
        "remote")
            if [ -n "$SB_TEST_SERVER" ]; then
                echo "$SB_TEST_SERVER:$SB_TEST_DB_DIR/${test_name}_${db_name}.fdb"
            else
                echo "$SB_TEST_DB_DIR/${test_name}_${db_name}.fdb"
            fi
            ;;
        "temp")
            echo "/tmp/scratchbird_test_${test_name}_${db_name}.fdb"
            ;;
        *)
            echo "$SB_TEST_DB_DIR/${test_name}_${db_name}.fdb"
            ;;
    esac
}

# Generate connection string for sb_isql
generate_connection_string() {
    local db_path="$1"
    
    if [ -n "$SB_TEST_SERVER" ] && [ "$SB_TEST_DB_LOCATION" = "remote" ]; then
        echo "$SB_TEST_SERVER/$SB_TEST_PORT:$db_path"
    else
        echo "$db_path"
    fi
}

# Create database creation SQL with proper connection parameters
generate_create_db_sql() {
    local db_path="$1"
    
    cat << EOF
CREATE DATABASE '$db_path'
    USER '$SB_TEST_USER' PASSWORD '$SB_TEST_PASSWORD'
    DEFAULT CHARACTER SET $SB_TEST_CHARSET
    PAGE_SIZE $SB_TEST_PAGE_SIZE;
EOF
}

# =================================================================
# UTILITY FUNCTIONS
# =================================================================

# Initialize test environment
init_test_environment() {
    # Create necessary directories
    mkdir -p "$SB_TEST_RESULTS_DIR"
    mkdir -p "$SB_TEST_DB_DIR"
    mkdir -p "$SB_TEST_LOGS_DIR"
    mkdir -p "$SB_TEST_TEMP_DIR"
    
    # Verify ScratchBird installation
    if [ ! -f "$SB_ISQL" ]; then
        echo "ERROR: ScratchBird sb_isql not found at: $SB_ISQL"
        echo "Please set SB_INSTALL_DIR environment variable or update test_config.sh"
        return 1
    fi
    
    # Test database connection (if server specified)
    if [ -n "$SB_TEST_SERVER" ] && [ "$SB_TEST_DB_LOCATION" = "remote" ]; then
        echo "Testing connection to remote server: $SB_TEST_SERVER:$SB_TEST_PORT"
        # Add connection test logic here if needed
    fi
    
    return 0
}

# Clean up test databases
cleanup_test_databases() {
    local test_name="$1"
    
    if [ "$SB_TEST_CLEANUP" = "true" ]; then
        case "$SB_TEST_DB_LOCATION" in
            "local")
                rm -f "$SB_TEST_DB_DIR"/${test_name}_*.fdb
                ;;
            "temp")
                rm -f /tmp/scratchbird_test_${test_name}_*.fdb
                ;;
            "remote")
                # For remote databases, you might need special cleanup logic
                echo "Note: Remote database cleanup may require manual intervention"
                ;;
        esac
    fi
}

# Execute sb_isql with proper environment
execute_sb_isql() {
    local input_file="$1"
    local output_file="$2"
    
    SCRATCHBIRD="$SB_INSTALL_DIR" "$SB_ISQL" -i "$input_file" > "$output_file" 2>&1
    return $?
}

# Log test execution
log_test_execution() {
    local test_name="$1"
    local status="$2"
    local message="$3"
    
    local log_file="$SB_TEST_LOGS_DIR/test_execution.log"
    echo "$(date '+%Y-%m-%d %H:%M:%S') [$status] $test_name: $message" >> "$log_file"
    
    if [ "$SB_TEST_VERBOSE" = "true" ]; then
        echo "$(date '+%Y-%m-%d %H:%M:%S') [$status] $test_name: $message"
    fi
}

# =================================================================
# CONFIGURATION VALIDATION
# =================================================================

validate_test_config() {
    local errors=0
    
    # Check required binaries
    if [ ! -f "$SB_ISQL" ]; then
        echo "ERROR: sb_isql binary not found: $SB_ISQL"
        errors=$((errors + 1))
    fi
    
    # Check directory permissions
    if [ ! -w "$SB_TEST_BASE_DIR" ]; then
        echo "ERROR: No write permission to test base directory: $SB_TEST_BASE_DIR"
        errors=$((errors + 1))
    fi
    
    # Validate database location setting
    case "$SB_TEST_DB_LOCATION" in
        "local"|"remote"|"temp")
            # Valid options
            ;;
        *)
            echo "ERROR: Invalid SB_TEST_DB_LOCATION: $SB_TEST_DB_LOCATION (must be: local, remote, or temp)"
            errors=$((errors + 1))
            ;;
    esac
    
    # Check remote server configuration
    if [ "$SB_TEST_DB_LOCATION" = "remote" ] && [ -z "$SB_TEST_SERVER" ]; then
        echo "ERROR: SB_TEST_SERVER must be set when using remote database location"
        errors=$((errors + 1))
    fi
    
    return $errors
}

# =================================================================
# CONFIGURATION DISPLAY
# =================================================================

display_test_config() {
    echo "==================================================================="
    echo "SCRATCHBIRD TEST CONFIGURATION"
    echo "==================================================================="
    echo "Database Connection:"
    echo "  User: $SB_TEST_USER"
    echo "  Password: ${SB_TEST_PASSWORD:0:3}*** (hidden)"
    echo "  Character Set: $SB_TEST_CHARSET"
    echo "  Page Size: $SB_TEST_PAGE_SIZE"
    echo
    echo "Database Location:"
    echo "  Mode: $SB_TEST_DB_LOCATION"
    if [ "$SB_TEST_DB_LOCATION" = "remote" ]; then
        echo "  Server: $SB_TEST_SERVER:$SB_TEST_PORT"
    fi
    echo "  Database Directory: $SB_TEST_DB_DIR"
    echo
    echo "ScratchBird Installation:"
    echo "  Installation Directory: $SB_INSTALL_DIR"
    echo "  sb_isql: $SB_ISQL"
    echo
    echo "Test Directories:"
    echo "  Base Directory: $SB_TEST_BASE_DIR"
    echo "  Results Directory: $SB_TEST_RESULTS_DIR"
    echo "  Logs Directory: $SB_TEST_LOGS_DIR"
    echo
    echo "Test Options:"
    echo "  Cleanup: $SB_TEST_CLEANUP"
    echo "  Verbose: $SB_TEST_VERBOSE"
    echo "  Stop on Error: $SB_TEST_STOP_ON_ERROR"
    echo "==================================================================="
}

# =================================================================
# AUTO-INITIALIZATION
# =================================================================

# Automatically initialize when this config is sourced
if [ "${BASH_SOURCE[0]}" != "${0}" ]; then
    # Being sourced, not executed directly
    if ! validate_test_config; then
        echo "ERROR: Test configuration validation failed!"
        return 1
    fi
    
    if ! init_test_environment; then
        echo "ERROR: Failed to initialize test environment!"
        return 1
    fi
    
    if [ "$SB_TEST_VERBOSE" = "true" ]; then
        display_test_config
    fi
else
    # Being executed directly - show configuration
    display_test_config
    validate_test_config
    init_test_environment
fi