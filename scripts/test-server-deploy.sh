#!/bin/bash
# ScratchBird Test Server - Production Deployment Script
# 
# PURPOSE: Deploy a running ScratchBird test server for other agents to use
# ASSUMES: Binaries are already built and tested
# 
# Usage: sudo ./test-server-deploy.sh [start|stop|restart|status|create-db|drop-db|logs]

set -e

# Configuration
SB_USER="scratchbird"
SB_GROUP="scratchbird"
PORT=13092
DB_DIR="/var/scratchbird/testdb"
DB_FILE="$DB_DIR/testdb.sdb"
LOG_DIR="/var/log/scratchbird"
CONFIG_DIR="/etc/scratchbird"

# Binary locations (adjust if installed elsewhere)
SB_BIN_DIR="${SB_BIN_DIR:-/opt/ScratchBird/build/bin}"
SB_SERVER="$SB_BIN_DIR/sb_server"
SB_ISQL="$SB_BIN_DIR/sb_isql"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# ============================================================================
# Helper Functions
# ============================================================================

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_root() {
    if [ "$EUID" -ne 0 ]; then
        log_error "Please run as root (use sudo)"
        exit 1
    fi
}

find_binaries() {
    # Check if binaries exist at configured location
    if [ -f "$SB_SERVER" ]; then
        return 0
    fi
    
    # Search common locations
    for dir in "/opt/ScratchBird/build/bin" "/usr/local/bin" "/usr/bin" "./build/bin"; do
        if [ -f "$dir/sb_server" ]; then
            SB_BIN_DIR="$dir"
            SB_SERVER="$dir/sb_server"
            SB_ISQL="$dir/sb_isql"
            log_info "Found binaries in: $dir"
            return 0
        fi
    done
    
    log_error "Cannot find ScratchBird binaries"
    log_info "Set SB_BIN_DIR environment variable to binary location:"
    log_info "  export SB_BIN_DIR=/path/to/binaries"
    log_info "  sudo -E ./test-server-deploy.sh start"
    exit 1
}

# ============================================================================
# Setup Functions (Run once)
# ============================================================================

setup_directories() {
    log_info "Setting up directories..."
    
    # Create directories
    mkdir -p "$DB_DIR" "$LOG_DIR" "$CONFIG_DIR"
    
    # Create user if doesn't exist
    if ! id "$SB_USER" &>/dev/null; then
        useradd -r -s /bin/false -d "$DB_DIR" -M "$SB_USER"
        log_info "Created user: $SB_USER"
    fi
    
    # Set ownership
    chown -R "$SB_USER:$SB_GROUP" "$DB_DIR" "$LOG_DIR" "$CONFIG_DIR"
    chmod 750 "$DB_DIR" "$LOG_DIR"
    
    log_info "Directories ready"
}

setup_certificates() {
    if [ -f "$CONFIG_DIR/server.crt" ]; then
        log_info "TLS certificates already exist"
        return 0
    fi
    
    log_info "Generating self-signed TLS certificates..."
    
    openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
        -keyout "$CONFIG_DIR/server.key" \
        -out "$CONFIG_DIR/server.crt" \
        -subj "/C=US/ST=Test/L=Test/O=ScratchBird/CN=localhost" \
        2>/dev/null
    
    chmod 600 "$CONFIG_DIR/server.key"
    chown "$SB_USER:$SB_GROUP" "$CONFIG_DIR/server.crt" "$CONFIG_DIR/server.key"
    
    log_info "Certificates generated"
}

setup_systemd() {
    if [ -f "/etc/systemd/system/scratchbird-test.service" ]; then
        log_info "Systemd service already configured"
        return 0
    fi
    
    log_info "Creating systemd service..."
    
    cat > /etc/systemd/system/scratchbird-test.service << EOF
[Unit]
Description=ScratchBird Test Server
After=network.target

[Service]
Type=simple
User=$SB_USER
Group=$SB_GROUP
WorkingDirectory=$DB_DIR

ExecStart=$SB_SERVER \\
    --database=$DB_FILE \\
    --port=$PORT \\
    --bind=127.0.0.1 \\
    --tls-cert=$CONFIG_DIR/server.crt \\
    --tls-key=$CONFIG_DIR/server.key \\
    --log-level=info \\
    --log-file=$LOG_DIR/testdb.log \\
    --max-connections=100

Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
    
    systemctl daemon-reload
    systemctl enable scratchbird-test.service
    
    log_info "Systemd service created"
}

# ============================================================================
# Database Operations
# ============================================================================

create_database() {
    log_info "Creating test database..."
    
    if [ -f "$DB_FILE" ]; then
        log_warn "Database already exists at $DB_FILE"
        read -p "Recreate? (y/N): " confirm
        if [[ ! $confirm =~ ^[Yy]$ ]]; then
            return 0
        fi
        rm -f "$DB_FILE"
    fi
    
    # Create empty database (sb_server will initialize on first start)
    mkdir -p "$DB_DIR"
    touch "$DB_FILE"
    chown "$SB_USER:$SB_GROUP" "$DB_FILE"
    chmod 640 "$DB_FILE"
    
    # Start server temporarily to create schema
    log_info "Starting temporary server for initialization..."
    
    sudo -u "$SB_USER" "$SB_SERVER" \
        --database="$DB_FILE" \
        --port=$PORT \
        --bind=127.0.0.1 \
        --tls-cert="$CONFIG_DIR/server.crt" \
        --tls-key="$CONFIG_DIR/server.key" &
    
    SERVER_PID=$!
    sleep 3
    
    # Create users
    log_info "Creating test users..."
    
    # SYSARCH - Full access
    $SB_ISQL --host=127.0.0.1 --port=$PORT --database=testdb --user=admin \
        --query="CREATE USER IF NOT EXISTS SYSARCH PASSWORD 'SysArch2026!';" 2>/dev/null || true
    $SB_ISQL --host=127.0.0.1 --port=$PORT --database=testdb --user=admin \
        --query="GRANT ALL ON DATABASE testdb TO SYSARCH;" 2>/dev/null || true
    
    # TESTUSER - DML only
    $SB_ISQL --host=127.0.0.1 --port=$PORT --database=testdb --user=admin \
        --query="CREATE USER IF NOT EXISTS TESTUSER PASSWORD 'TestUser2026!';" 2>/dev/null || true
    $SB_ISQL --host=127.0.0.1 --port=$PORT --database=testdb --user=admin \
        --query="GRANT SELECT, INSERT, UPDATE, DELETE ON DATABASE testdb TO TESTUSER;" 2>/dev/null || true
    
    # Create schema
    log_info "Creating test schema..."
    $SB_ISQL --host=127.0.0.1 --port=$PORT --database=testdb --user=admin << 'SQL' 2>/dev/null || true
CREATE SCHEMA IF NOT EXISTS test_schema;

CREATE TABLE IF NOT EXISTS test_schema.users (
    id INTEGER PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    username VARCHAR(50) NOT NULL,
    email VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    active BOOLEAN DEFAULT TRUE
);

INSERT INTO test_schema.users (username, email, active) VALUES
    ('alice', 'alice@example.com', TRUE),
    ('bob', 'bob@example.com', TRUE),
    ('charlie', 'charlie@example.com', FALSE)
ON CONFLICT DO NOTHING;
SQL
    
    # Stop temporary server
    kill $SERVER_PID 2>/dev/null || true
    sleep 1
    
    log_info "Database created successfully"
}

drop_database() {
    log_warn "This will DELETE the test database!"
    read -p "Are you sure? (type 'yes' to confirm): " confirm
    
    if [ "$confirm" != "yes" ]; then
        log_info "Cancelled"
        return 0
    fi
    
    # Stop server if running
    systemctl stop scratchbird-test.service 2>/dev/null || true
    
    # Remove database file
    if [ -f "$DB_FILE" ]; then
        rm -f "$DB_FILE"
        log_info "Database deleted"
    else
        log_warn "Database file not found"
    fi
}

# ============================================================================
# Service Control
# ============================================================================

start_server() {
    log_info "Starting ScratchBird test server..."
    
    if systemctl is-active --quiet scratchbird-test; then
        log_warn "Server is already running"
        return 0
    fi
    
    systemctl start scratchbird-test.service
    sleep 2
    
    if systemctl is-active --quiet scratchbird-test; then
        log_info "✅ Server started successfully"
        show_status
    else
        log_error "❌ Failed to start server"
        systemctl status scratchbird-test.service --no-pager
        exit 1
    fi
}

stop_server() {
    log_info "Stopping ScratchBird test server..."
    
    if ! systemctl is-active --quiet scratchbird-test; then
        log_warn "Server is not running"
        return 0
    fi
    
    systemctl stop scratchbird-test.service
    log_info "✅ Server stopped"
}

restart_server() {
    log_info "Restarting ScratchBird test server..."
    systemctl restart scratchbird-test.service
    sleep 2
    
    if systemctl is-active --quiet scratchbird-test; then
        log_info "✅ Server restarted successfully"
    else
        log_error "❌ Failed to restart server"
        exit 1
    fi
}

show_status() {
    echo ""
    echo "═══════════════════════════════════════════════════════════════"
    echo "  ScratchBird Test Server Status"
    echo "═══════════════════════════════════════════════════════════════"
    echo ""
    
    if systemctl is-active --quiet scratchbird-test; then
        echo -e "Status: ${GREEN}● Running${NC}"
    else
        echo -e "Status: ${RED}● Stopped${NC}"
    fi
    
    echo ""
    echo "Configuration:"
    echo "  Host:     127.0.0.1 (localhost)"
    echo "  Port:     $PORT"
    echo "  Database: $DB_FILE"
    echo "  Logs:     $LOG_DIR/testdb.log"
    echo ""
    echo "Users:"
    echo "  SYSARCH  / SysArch2026!    [Full DDL/DML access]"
    echo "  TESTUSER / TestUser2026!   [DML only]"
    echo ""
    echo "Connection:"
    echo "  scratchbird://SYSARCH:SysArch2026!@127.0.0.1:$PORT/testdb"
    echo "  scratchbird://TESTUSER:TestUser2026!@127.0.0.1:$PORT/testdb"
    echo ""
    echo "═══════════════════════════════════════════════════════════════"
}

show_logs() {
    if [ -f "$LOG_DIR/testdb.log" ]; then
        tail -f "$LOG_DIR/testdb.log"
    else
        log_warn "Log file not found"
        journalctl -u scratchbird-test.service -f
    fi
}

# ============================================================================
# Main
# ============================================================================

main() {
    check_root
    find_binaries
    
    COMMAND=${1:-status}
    
    case "$COMMAND" in
        setup)
            log_info "Running initial setup..."
            setup_directories
            setup_certificates
            setup_systemd
            create_database
            log_info "✅ Setup complete. Run './test-server-deploy.sh start' to begin."
            ;;
        
        start)
            start_server
            ;;
        
        stop)
            stop_server
            ;;
        
        restart)
            restart_server
            ;;
        
        status)
            show_status
            ;;
        
        create-db)
            create_database
            ;;
        
        drop-db)
            drop_database
            ;;
        
        logs)
            show_logs
            ;;
        
        *)
            echo "Usage: $0 [command]"
            echo ""
            echo "Commands:"
            echo "  setup      - Initial setup (run once)"
            echo "  start      - Start the test server"
            echo "  stop       - Stop the test server"
            echo "  restart    - Restart the test server"
            echo "  status     - Show server status"
            echo "  create-db  - Create/recreate test database"
            echo "  drop-db    - Delete test database"
            echo "  logs       - View server logs"
            echo ""
            echo "Examples:"
            echo "  sudo $0 setup      # First time setup"
            echo "  sudo $0 start      # Start server"
            echo "  sudo $0 status     # Check status"
            exit 1
            ;;
    esac
}

main "$@"
