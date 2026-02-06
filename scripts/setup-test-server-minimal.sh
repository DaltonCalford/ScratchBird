#!/bin/bash
# ScratchBird Test Server - Minimal Setup (No Build Required)
# This script assumes ScratchBird is already built
# Usage: sudo ./setup-test-server-minimal.sh

set -e

PORT=13092
DB_DIR="/var/scratchbird/testdb"
DB_FILE="$DB_DIR/testdb.sdb"
LOG_DIR="/var/log/scratchbird"
SB_USER="scratchbird"

# Binary locations - adjust as needed
SB_SERVER="/opt/ScratchBird/build/bin/sb_server"
SB_ISQL="/opt/ScratchBird/build/bin/sb_isql"
SB_ADMIN="/opt/ScratchBird/build/bin/sb_admin"

echo "═══════════════════════════════════════════════════════════════"
echo "  ScratchBird Test Server - Minimal Setup"
echo "  Port: $PORT"
echo "═══════════════════════════════════════════════════════════════"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "❌ Please run as root (use sudo)"
    exit 1
fi

# Find binaries if not at expected location
find_binaries() {
    if [ ! -f "$SB_SERVER" ]; then
        echo "🔍 Searching for ScratchBird binaries..."
        
        # Try common locations
        for dir in "/opt/ScratchBird/build/bin" "./build/bin" "../build/bin" "/usr/local/bin"; do
            if [ -f "$dir/sb_server" ]; then
                SB_SERVER="$dir/sb_server"
                SB_ISQL="$dir/sb_isql"
                SB_ADMIN="$dir/sb_admin"
                echo "✅ Found binaries in $dir"
                return 0
            fi
        done
        
        # Search entire system (slow)
        SB_SERVER=$(find /opt /home /root -name "sb_server" -type f 2>/dev/null | head -1)
        if [ -n "$SB_SERVER" ]; then
            SB_DIR=$(dirname "$SB_SERVER")
            SB_ISQL="$SB_DIR/sb_isql"
            SB_ADMIN="$SB_DIR/sb_admin"
            echo "✅ Found binaries at $SB_DIR"
            return 0
        fi
        
        return 1
    fi
    return 0
}

if ! find_binaries; then
    echo "❌ Could not find ScratchBird binaries."
    echo ""
    echo "Please build ScratchBird first:"
    echo "  cd /opt/ScratchBird"
    echo "  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release"
    echo "  cmake --build build"
    echo ""
    echo "Or specify the binary location:"
    echo "  export SB_SERVER=/path/to/sb_server"
    exit 1
fi

echo ""
echo "📋 Using binaries:"
echo "  Server: $SB_SERVER"
echo "  ISQL:   $SB_ISQL"
echo ""

# Create user
echo "👤 Creating scratchbird user..."
if ! id "$SB_USER" &>/dev/null; then
    useradd -r -s /bin/false -d "$DB_DIR" -M "$SB_USER"
    echo "✅ User $SB_USER created"
else
    echo "✅ User $SB_USER already exists"
fi

# Create directories
echo "📁 Creating directories..."
mkdir -p "$DB_DIR" "$LOG_DIR" /etc/scratchbird
chown -R "$SB_USER:$SB_USER" "$DB_DIR" "$LOG_DIR"
chmod 750 "$DB_DIR" "$LOG_DIR"

# Generate TLS certificates
echo "🔐 Generating self-signed TLS certificates..."
if [ ! -f "/etc/scratchbird/server.crt" ]; then
    openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
        -keyout /etc/scratchbird/server.key \
        -out /etc/scratchbird/server.crt \
        -subj "/C=US/ST=Test/L=Test/O=ScratchBird/CN=localhost" \
        2>/dev/null
    
    chmod 600 /etc/scratchbird/server.key
    chown "$SB_USER:$SB_USER" /etc/scratchbird/*.pem
    echo "✅ Certificates generated"
fi

# Create database directory (database will be created on first start)
if [ ! -d "$DB_DIR" ]; then
    mkdir -p "$DB_DIR"
    chown "$SB_USER:$SB_USER" "$DB_DIR"
fi

# Create simple config file
echo "⚙️  Creating configuration..."
cat > /etc/scratchbird/testdb.conf << EOF
# ScratchBird Test Server Configuration
[database]
path = $DB_FILE
page_size = 16384
encoding = UTF8

[network]
bind = 127.0.0.1
port = $PORT
tls_cert = /etc/scratchbird/server.crt
tls_key = /etc/scratchbird/server.key

[limits]
max_connections = 100
query_timeout = 0
idle_timeout = 300

[logging]
level = info
file = $LOG_DIR/testdb.log
EOF

chown "$SB_USER:$SB_USER" /etc/scratchbird/testdb.conf

# Create systemd service
echo "⚙️  Creating systemd service..."
cat > /etc/systemd/system/scratchbird-test.service << EOF
[Unit]
Description=ScratchBird Test Server
After=network.target

[Service]
Type=simple
User=$SB_USER
Group=$SB_USER
WorkingDirectory=$DB_DIR

ExecStart=$SB_SERVER \\
    --database=$DB_FILE \\
    --port=$PORT \\
    --bind=127.0.0.1 \\
    --tls-cert=/etc/scratchbird/server.crt \\
    --tls-key=/etc/scratchbird/server.key \\
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
echo "✅ Service created"

# Start service
echo "🚀 Starting test server..."
systemctl start scratchbird-test.service
sleep 3

# Check status
if systemctl is-active --quiet scratchbird-test.service; then
    echo "✅ Service is running"
else
    echo "❌ Service failed to start. Checking logs..."
    systemctl status scratchbird-test.service --no-pager
    journalctl -u scratchbird-test.service --no-pager -n 20
    exit 1
fi

# Test connection
echo ""
echo "🧪 Testing server..."
sleep 2

# Try to connect (server may need more time to initialize)
for i in 1 2 3; do
    if $SB_ISQL \
        --host=127.0.0.1 \
        --port=$PORT \
        --database=testdb \
        --user=admin \
        --query="SELECT 'Server is up' AS status;" 2>/dev/null | grep -q "up"; then
        echo "✅ Server is responding"
        break
    fi
    echo "  Waiting for server to be ready... ($i/3)"
    sleep 2
done

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo "  ✅ SETUP COMPLETE"
echo "═══════════════════════════════════════════════════════════════"
echo ""
echo "📋 Connection Parameters:"
echo "   Host:     127.0.0.1 (localhost)"
echo "   Port:     $PORT"
echo "   Database: testdb"
echo "   TLS:      Required (self-signed cert)"
echo ""
echo "👤 Default User:"
echo "   admin / (no password required for local)"
echo ""
echo "⚙️  Service Commands:"
echo "   sudo systemctl start scratchbird-test"
echo "   sudo systemctl stop scratchbird-test"
echo "   sudo systemctl status scratchbird-test"
echo "   sudo tail -f $LOG_DIR/testdb.log"
echo ""
echo "📝 Test Connection:"
echo "   $SB_ISQL --host=127.0.0.1 --port=$PORT \\"
echo "       --database=testdb --user=admin \\"
echo "       --query=\"SELECT 'Hello World';\""
echo ""
echo "═══════════════════════════════════════════════════════════════"
echo ""
echo "⚠️  NOTE: This is a minimal setup. For full security testing:"
echo "   1. Create SYSARCH user manually:"
echo "      $SB_ISQL --host=127.0.0.1 --port=$PORT \\"
echo "          --query=\"CREATE USER SYSARCH PASSWORD 'SysArch2026!';\""
echo ""
echo "   2. Create TESTUSER with limited privileges"
echo ""
echo "   See PUBLIC_TEST_SERVER_SETUP.md for complete instructions."
echo ""
