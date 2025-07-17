#!/bin/bash

# ScratchBird v0.5.0 - Backup and Restore Examples
# 
# This script demonstrates advanced backup and restore operations
# using ScratchBird's schema-aware backup utilities.
# 
# Features demonstrated:
# - Schema-aware backup with sb_gbak
# - Incremental backup with sb_nbackup
# - Schema-selective restore operations
# - Backup validation and verification
# - Performance monitoring during backup
# 
# Usage:
#   ./backup_restore.sh [options]
# 
# Options:
#   -h, --help      Show this help message
#   -v, --verbose   Verbose output
#   -d, --demo      Run demonstration mode
#   -s, --schema    Backup specific schema only
# 
# The contents of this file are subject to the Initial
# Developer's Public License Version 1.0 (the "License");
# you may not use this file except in compliance with the
# License. You may obtain a copy of the License at
# http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.

# Configuration
SCRATCHBIRD_VERSION="SB-T0.5.0.1 ScratchBird 0.5 f90eae0"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Default configuration
SB_USER="${SB_USER:-SYSDBA}"
SB_PASSWORD="${SB_PASSWORD:-masterkey}"
SB_DATABASE="${SB_DATABASE:-localhost:employee.fdb}"
SB_PORT="${SB_PORT:-4050}"
BACKUP_DIR="${BACKUP_DIR:-./backups}"
VERBOSE=false
DEMO_MODE=false
SCHEMA_ONLY=""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_verbose() {
    if [ "$VERBOSE" = true ]; then
        echo -e "${BLUE}[VERBOSE]${NC} $1"
    fi
}

# Help function
show_help() {
    cat << EOF
ScratchBird v0.5.0 - Backup and Restore Examples

This script demonstrates advanced backup and restore operations using
ScratchBird's schema-aware backup utilities.

USAGE:
    ./backup_restore.sh [OPTIONS]

OPTIONS:
    -h, --help          Show this help message
    -v, --verbose       Enable verbose output
    -d, --demo          Run demonstration mode
    -s, --schema SCHEMA Backup specific schema only

ENVIRONMENT VARIABLES:
    SB_USER             ScratchBird username (default: SYSDBA)
    SB_PASSWORD         ScratchBird password (default: masterkey)
    SB_DATABASE         Database connection string (default: localhost:employee.fdb)
    SB_PORT             ScratchBird port (default: 4050)
    BACKUP_DIR          Backup directory (default: ./backups)

EXAMPLES:
    # Full database backup
    ./backup_restore.sh

    # Verbose backup with monitoring
    ./backup_restore.sh -v

    # Schema-specific backup
    ./backup_restore.sh -s company.finance.accounting

    # Demonstration mode
    ./backup_restore.sh -d

FEATURES DEMONSTRATED:
    - Schema-aware backup with sb_gbak
    - Incremental backup with sb_nbackup
    - Schema-selective restore operations
    - Backup validation and verification
    - Performance monitoring during backup
    - Cross-schema backup coordination

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -d|--demo)
            DEMO_MODE=true
            shift
            ;;
        -s|--schema)
            SCHEMA_ONLY="$2"
            shift 2
            ;;
        *)
            log_error "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

# Initialize backup environment
init_backup_environment() {
    log_info "Initializing ScratchBird backup environment..."
    log_info "Version: $SCRATCHBIRD_VERSION"
    
    # Create backup directory
    mkdir -p "$BACKUP_DIR"
    log_verbose "Created backup directory: $BACKUP_DIR"
    
    # Check ScratchBird tools
    if ! command -v sb_gbak &> /dev/null; then
        log_error "sb_gbak not found. Please ensure ScratchBird is installed."
        exit 1
    fi
    
    if ! command -v sb_nbackup &> /dev/null; then
        log_error "sb_nbackup not found. Please ensure ScratchBird is installed."
        exit 1
    fi
    
    # Test database connection
    log_verbose "Testing database connection..."
    if sb_isql -user "$SB_USER" -password "$SB_PASSWORD" -database "$SB_DATABASE" -q << EOF
SELECT 'Connection test successful' FROM RDB\$DATABASE;
EOF
    then
        log_success "Database connection successful"
    else
        log_error "Failed to connect to database: $SB_DATABASE"
        exit 1
    fi
}

# Create sample hierarchical schema for demonstration
create_demo_schema() {
    if [ "$DEMO_MODE" = false ]; then
        return 0
    fi
    
    log_info "Creating demonstration schema hierarchy..."
    
    sb_isql -user "$SB_USER" -password "$SB_PASSWORD" -database "$SB_DATABASE" -q << 'EOF'
-- Create hierarchical schema structure
CREATE SCHEMA demo_backup;
CREATE SCHEMA demo_backup.level1;
CREATE SCHEMA demo_backup.level1.level2;
CREATE SCHEMA demo_backup.level1.level2.level3;

-- Set schema context
SET SCHEMA 'demo_backup';

-- Create tables in different schema levels
CREATE TABLE root_table (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    name VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

SET SCHEMA 'demo_backup.level1';
CREATE TABLE level1_table (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    description TEXT,
    amount DECIMAL(15,2),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

SET SCHEMA 'demo_backup.level1.level2';
CREATE TABLE level2_table (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    ip_address INET,
    mac_address MACADDR,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

SET SCHEMA 'demo_backup.level1.level2.level3';
CREATE TABLE level3_table (
    id UUID GENERATED ALWAYS AS IDENTITY (GENERATOR GEN_UUID(7)),
    deep_data VARCHAR(500),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert sample data
SET SCHEMA 'demo_backup';
INSERT INTO root_table (name) VALUES ('Root Level Data'), ('Another Root Entry');

SET SCHEMA 'demo_backup.level1';
INSERT INTO level1_table (description, amount) VALUES 
    ('Level 1 Description', 1000.00),
    ('Another Level 1 Entry', 2500.50);

SET SCHEMA 'demo_backup.level1.level2';
INSERT INTO level2_table (ip_address, mac_address) VALUES 
    ('192.168.1.100', '00:11:22:33:44:55'),
    ('10.0.0.50', '00:aa:bb:cc:dd:ee');

SET SCHEMA 'demo_backup.level1.level2.level3';
INSERT INTO level3_table (deep_data) VALUES 
    ('Deep hierarchical data'),
    ('Another deep entry');

-- Commit the transaction
COMMIT;
EOF
    
    log_success "Demo schema created successfully"
}

# Full database backup
full_database_backup() {
    log_info "Performing full database backup..."
    
    local backup_file="$BACKUP_DIR/full_backup_$(date +%Y%m%d_%H%M%S).fbk"
    local log_file="$BACKUP_DIR/full_backup_$(date +%Y%m%d_%H%M%S).log"
    
    log_verbose "Backup file: $backup_file"
    log_verbose "Log file: $log_file"
    
    # Perform backup with statistics
    local backup_cmd="sb_gbak -backup -user $SB_USER -password $SB_PASSWORD -statistics -verbose $SB_DATABASE $backup_file"
    
    if [ "$VERBOSE" = true ]; then
        log_verbose "Running: $backup_cmd"
    fi
    
    if eval "$backup_cmd" 2>&1 | tee "$log_file"; then
        log_success "Full backup completed: $backup_file"
        
        # Show backup statistics
        local backup_size=$(du -h "$backup_file" | cut -f1)
        log_info "Backup size: $backup_size"
        
        # Validate backup
        validate_backup "$backup_file"
        
    else
        log_error "Full backup failed"
        return 1
    fi
}

# Schema-specific backup
schema_specific_backup() {
    local schema_name="$1"
    
    if [ -z "$schema_name" ]; then
        log_warning "No schema specified for schema-specific backup"
        return 0
    fi
    
    log_info "Performing schema-specific backup for: $schema_name"
    
    local backup_file="$BACKUP_DIR/schema_${schema_name//\./_}_$(date +%Y%m%d_%H%M%S).fbk"
    local log_file="$BACKUP_DIR/schema_${schema_name//\./_}_$(date +%Y%m%d_%H%M%S).log"
    
    log_verbose "Schema backup file: $backup_file"
    log_verbose "Schema log file: $log_file"
    
    # Perform schema-specific backup
    local backup_cmd="sb_gbak -backup -user $SB_USER -password $SB_PASSWORD -schema $schema_name -verbose $SB_DATABASE $backup_file"
    
    if [ "$VERBOSE" = true ]; then
        log_verbose "Running: $backup_cmd"
    fi
    
    if eval "$backup_cmd" 2>&1 | tee "$log_file"; then
        log_success "Schema backup completed: $backup_file"
        
        # Show backup statistics
        local backup_size=$(du -h "$backup_file" | cut -f1)
        log_info "Schema backup size: $backup_size"
        
        # Validate schema backup
        validate_backup "$backup_file"
        
    else
        log_error "Schema backup failed"
        return 1
    fi
}

# Incremental backup using sb_nbackup
incremental_backup() {
    log_info "Performing incremental backup..."
    
    local base_backup="$BACKUP_DIR/incremental_base_$(date +%Y%m%d_%H%M%S).nbk"
    local inc_backup="$BACKUP_DIR/incremental_level1_$(date +%Y%m%d_%H%M%S).nbk"
    
    log_verbose "Base backup file: $base_backup"
    log_verbose "Incremental backup file: $inc_backup"
    
    # Level 0 backup (base)
    log_info "Creating level 0 (base) backup..."
    local base_cmd="sb_nbackup -backup -level 0 -user $SB_USER -password $SB_PASSWORD $SB_DATABASE $base_backup"
    
    if [ "$VERBOSE" = true ]; then
        log_verbose "Running: $base_cmd"
    fi
    
    if eval "$base_cmd"; then
        log_success "Base backup completed: $base_backup"
        
        # Simulate some database activity
        log_info "Simulating database activity..."
        sb_isql -user "$SB_USER" -password "$SB_PASSWORD" -database "$SB_DATABASE" -q << 'EOF'
-- Simulate database changes
UPDATE RDB$DATABASE SET RDB$CHARACTER_SET_NAME = RDB$CHARACTER_SET_NAME;
COMMIT;
EOF
        
        # Level 1 backup (incremental)
        log_info "Creating level 1 (incremental) backup..."
        local inc_cmd="sb_nbackup -backup -level 1 -user $SB_USER -password $SB_PASSWORD $SB_DATABASE $inc_backup"
        
        if [ "$VERBOSE" = true ]; then
            log_verbose "Running: $inc_cmd"
        fi
        
        if eval "$inc_cmd"; then
            log_success "Incremental backup completed: $inc_backup"
            
            # Show backup sizes
            local base_size=$(du -h "$base_backup" | cut -f1)
            local inc_size=$(du -h "$inc_backup" | cut -f1)
            log_info "Base backup size: $base_size"
            log_info "Incremental backup size: $inc_size"
            
            # Validate incremental backup
            validate_incremental_backup "$base_backup" "$inc_backup"
            
        else
            log_error "Incremental backup failed"
            return 1
        fi
        
    else
        log_error "Base backup failed"
        return 1
    fi
}

# Validate backup file
validate_backup() {
    local backup_file="$1"
    
    log_info "Validating backup: $(basename "$backup_file")"
    
    if sb_gbak -validate -user "$SB_USER" -password "$SB_PASSWORD" "$backup_file" 2>/dev/null; then
        log_success "Backup validation passed"
    else
        log_error "Backup validation failed"
        return 1
    fi
}

# Validate incremental backup
validate_incremental_backup() {
    local base_backup="$1"
    local inc_backup="$2"
    
    log_info "Validating incremental backup chain..."
    
    if sb_nbackup -validate "$base_backup" "$inc_backup" 2>/dev/null; then
        log_success "Incremental backup validation passed"
    else
        log_error "Incremental backup validation failed"
        return 1
    fi
}

# Demonstrate restore operation
demonstrate_restore() {
    log_info "Demonstrating restore operation..."
    
    # Find the most recent backup
    local latest_backup=$(ls -t "$BACKUP_DIR"/*.fbk 2>/dev/null | head -n1)
    
    if [ -z "$latest_backup" ]; then
        log_warning "No backup files found for restore demonstration"
        return 0
    fi
    
    log_info "Using backup file: $(basename "$latest_backup")"
    
    # Create test restore database
    local restore_db="localhost:restored_test_$(date +%Y%m%d_%H%M%S).fdb"
    
    log_info "Restoring to test database: $restore_db"
    
    local restore_cmd="sb_gbak -restore -user $SB_USER -password $SB_PASSWORD -verbose $latest_backup $restore_db"
    
    if [ "$VERBOSE" = true ]; then
        log_verbose "Running: $restore_cmd"
    fi
    
    if eval "$restore_cmd" 2>&1 | tee "$BACKUP_DIR/restore_$(date +%Y%m%d_%H%M%S).log"; then
        log_success "Restore completed successfully"
        
        # Verify restored database
        log_info "Verifying restored database..."
        if sb_isql -user "$SB_USER" -password "$SB_PASSWORD" -database "$restore_db" -q << 'EOF'
SELECT COUNT(*) as SCHEMA_COUNT FROM RDB$SCHEMAS;
EOF
        then
            log_success "Restored database verification passed"
        else
            log_error "Restored database verification failed"
        fi
        
        # Clean up test database
        log_info "Cleaning up test database..."
        sb_isql -user "$SB_USER" -password "$SB_PASSWORD" -q << EOF
DROP DATABASE '$restore_db';
EOF
        
    else
        log_error "Restore failed"
        return 1
    fi
}

# Monitor backup performance
monitor_backup_performance() {
    log_info "Monitoring backup performance..."
    
    # Start monitoring in background
    (
        while true; do
            if [ "$VERBOSE" = true ]; then
                echo "$(date): Backup monitoring active"
                # Show database activity
                sb_lock_print "$SB_DATABASE" 2>/dev/null | grep -E "(Active|Backup)" || true
            fi
            sleep 5
        done
    ) &
    local monitor_pid=$!
    
    # Store monitor PID for cleanup
    echo $monitor_pid > "$BACKUP_DIR/monitor.pid"
    
    log_verbose "Performance monitoring started (PID: $monitor_pid)"
}

# Stop performance monitoring
stop_performance_monitoring() {
    if [ -f "$BACKUP_DIR/monitor.pid" ]; then
        local monitor_pid=$(cat "$BACKUP_DIR/monitor.pid")
        kill $monitor_pid 2>/dev/null || true
        rm -f "$BACKUP_DIR/monitor.pid"
        log_verbose "Performance monitoring stopped"
    fi
}

# Cleanup function
cleanup() {
    log_info "Cleaning up..."
    
    # Stop monitoring
    stop_performance_monitoring
    
    # Remove demo schema if created
    if [ "$DEMO_MODE" = true ]; then
        log_info "Removing demo schema..."
        sb_isql -user "$SB_USER" -password "$SB_PASSWORD" -database "$SB_DATABASE" -q << 'EOF'
-- Clean up demo schema (from deepest to shallowest)
DROP TABLE demo_backup.level1.level2.level3.level3_table;
DROP TABLE demo_backup.level1.level2.level2_table;
DROP TABLE demo_backup.level1.level1_table;
DROP TABLE demo_backup.root_table;
DROP SCHEMA demo_backup.level1.level2.level3;
DROP SCHEMA demo_backup.level1.level2;
DROP SCHEMA demo_backup.level1;
DROP SCHEMA demo_backup;
COMMIT;
EOF
        log_success "Demo schema cleaned up"
    fi
    
    log_info "Cleanup completed"
}

# Main execution
main() {
    # Set up cleanup trap
    trap cleanup EXIT
    
    log_info "Starting ScratchBird backup and restore examples..."
    
    # Initialize environment
    init_backup_environment
    
    # Create demo schema if requested
    create_demo_schema
    
    # Start performance monitoring
    monitor_backup_performance
    
    # Perform full database backup
    full_database_backup
    
    # Perform schema-specific backup if requested
    if [ -n "$SCHEMA_ONLY" ]; then
        schema_specific_backup "$SCHEMA_ONLY"
    fi
    
    # Perform incremental backup
    incremental_backup
    
    # Demonstrate restore
    demonstrate_restore
    
    # Show backup summary
    log_info "Backup Summary:"
    log_info "Backup directory: $BACKUP_DIR"
    log_info "Backup files created:"
    ls -la "$BACKUP_DIR"/*.fbk "$BACKUP_DIR"/*.nbk 2>/dev/null || log_warning "No backup files found"
    
    log_success "ScratchBird backup and restore examples completed!"
    log_info "Features demonstrated:"
    log_info "- Full database backup with sb_gbak"
    log_info "- Schema-specific backup operations"
    log_info "- Incremental backup with sb_nbackup"
    log_info "- Backup validation and verification"
    log_info "- Restore operation with verification"
    log_info "- Performance monitoring during backup"
}

# Run main function
main "$@"