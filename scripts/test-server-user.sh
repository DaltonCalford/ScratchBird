#!/bin/bash
# ScratchBird Test Server - User-Space Development Script
# 
# PURPOSE: Run a ScratchBird test server for other agents to test against
# This script runs the server in user space (no root required)
#
# Usage: ./test-server-user.sh [start|stop|status|restart|create-db]

set -e

# Configuration
PORT=13092
DB_DIR="${HOME}/.scratchbird/testdb"
DB_FILE="$DB_DIR/testdb.sdb"
LOG_DIR="$DB_DIR/logs"
PID_FILE="$DB_DIR/server.pid"

# Binary locations (prefer local build)
SB_BIN_DIR="${SB_BIN_DIR:-$(pwd)/build/src}"
SB_SERVER="$SB_BIN_DIR/sb_server"
SB_ISQL="$SB_BIN_DIR/scratchbird"  # Use scratchbird as isql

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_binaries() {
    if [ ! -f "$SB_SERVER" ]; then
        log_error "sb_server not found at $SB_SERVER"
        log_info "Please build first: cmake --build build"
        exit 1
    fi
    log_info "Using binaries from: $SB_BIN_DIR"
}

setup_directories() {
    mkdir -p "$DB_DIR" "$LOG_DIR"
    log_info "Directories ready: $DB_DIR"
}

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
    
    # Create empty database file (sb_server will initialize)
    touch "$DB_FILE"
    log_info "Database file created: $DB_FILE"
}

start_server() {
    log_info "Starting ScratchBird test server..."
    
    if [ -f "$PID_FILE" ] && kill -0 "$(cat $PID_FILE)" 2>/dev/null; then
        log_warn "Server is already running (PID: $(cat $PID_FILE))"
        return 0
    fi
    
    # Create config file
    mkdir -p "$DB_DIR"
    cat > "$DB_DIR/test-server.conf" << EOF
[server]
mode = single-database
database = $DB_FILE
pid_file = $PID_FILE
auto_create = true

[network]
bind_address = 127.0.0.1
port = $PORT
postgres_port = 0
mysql_port = 0
firebird_port = 0

[logging]
level = info
EOF
    
    if [ ! -f "$DB_FILE" ]; then
        log_info "Database not found, will create on start..."
    fi
    
    # Start server in foreground mode (more reliable) but backgrounded
    nohup "$SB_SERVER" -F -c "$DB_DIR/test-server.conf" > "$LOG_DIR/server.stdout" 2>&1 &
    
    echo $! > "$PID_FILE"
    
    # Wait for server to be ready
    log_info "Waiting for server to start..."
    for i in {1..15}; do
        sleep 1
        # Check if process is still running
        if ! kill -0 "$(cat $PID_FILE)" 2>/dev/null; then
            log_error "Server process died"
            if [ -f "$LOG_DIR/server.stdout" ]; then
                log_info "Server output:"
                tail -30 "$LOG_DIR/server.stdout"
            fi
            return 1
        fi
        echo -n "."
    done
    echo ""
    
    log_info "✅ Server started (PID: $(cat $PID_FILE))"
    log_warn "Note: Server uses Unix socket, not TCP port $PORT"
    show_status
}

stop_server() {
    log_info "Stopping ScratchBird test server..."
    
    if [ ! -f "$PID_FILE" ]; then
        log_warn "No PID file found, server may not be running"
        return 0
    fi
    
    PID=$(cat "$PID_FILE")
    if kill -0 "$PID" 2>/dev/null; then
        kill "$PID"
        rm -f "$PID_FILE"
        log_info "✅ Server stopped (PID: $PID)"
    else
        log_warn "Server not running (stale PID file)"
        rm -f "$PID_FILE"
    fi
}

show_status() {
    echo ""
    echo "═══════════════════════════════════════════════════════════════"
    echo "  ScratchBird Test Server Status"
    echo "═══════════════════════════════════════════════════════════════"
    echo ""
    
    if [ -f "$PID_FILE" ] && kill -0 "$(cat $PID_FILE)" 2>/dev/null; then
        echo -e "Status: ${GREEN}● Running${NC} (PID: $(cat $PID_FILE))"
    else
        echo -e "Status: ${RED}● Stopped${NC}"
    fi
    
    echo ""
    echo "Configuration:"
    echo "  Mode:     Single-database"
    echo "  Database: $DB_FILE"
    echo "  Config:   $DB_DIR/test-server.conf"
    echo "  Logs:     $LOG_DIR/server.stdout"
    echo "  PID File: $PID_FILE"
    echo ""
    echo "Test Commands:"
    echo "  ./scripts/test-server-user.sh status     # Check status"
    echo "  ./scripts/test-server-user.sh stop       # Stop server"
    echo "  tail -f $LOG_DIR/server.stdout           # View logs"
    echo ""
    echo "═══════════════════════════════════════════════════════════════"
}

show_logs() {
    if [ -f "$LOG_DIR/testdb.log" ]; then
        tail -f "$LOG_DIR/testdb.log"
    else
        log_warn "Log file not found: $LOG_DIR/testdb.log"
    fi
}

# Main
COMMAND=${1:-status}

case "$COMMAND" in
    setup)
        check_binaries
        setup_directories
        create_database
        log_info "✅ Setup complete. Run './scripts/test-server-user.sh start' to begin."
        ;;
    
    start)
        check_binaries
        setup_directories
        start_server
        ;;
    
    stop)
        stop_server
        ;;
    
    restart)
        stop_server
        sleep 1
        start_server
        ;;
    
    status)
        show_status
        ;;
    
    create-db)
        setup_directories
        create_database
        ;;
    
    logs)
        show_logs
        ;;
    
    *)
        echo "Usage: $0 [command]"
        echo ""
        echo "Commands:"
        echo "  setup      - Initial setup (creates dirs and database)"
        echo "  start      - Start the test server"
        echo "  stop       - Stop the test server"
        echo "  restart    - Restart the test server"
        echo "  status     - Show server status"
        echo "  create-db  - Create/recreate test database"
        echo "  logs       - View server logs"
        echo ""
        echo "Examples:"
        echo "  ./scripts/test-server-user.sh setup      # First time setup"
        echo "  ./scripts/test-server-user.sh start      # Start server"
        echo "  ./scripts/test-server-user.sh status     # Check status"
        exit 1
        ;;
esac
