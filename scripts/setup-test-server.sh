#!/bin/bash
# ScratchBird Public Test Server Setup Script
# Usage: sudo ./setup-test-server.sh [hostname]

set -e

HOSTNAME=${1:-"scratchbird-test.local"}
PORT=13092
DB_DIR="/var/scratchbird/testdb"
DB_FILE="$DB_DIR/testdb.sdb"
LOG_DIR="/var/log/scratchbird"
SB_USER="scratchbird"
SB_VERSION="Alpha-2026-02-06"

echo "═══════════════════════════════════════════════════════════════"
echo "  ScratchBird Public Test Server Setup"
echo "  Version: $SB_VERSION"
echo "  Hostname: $HOSTNAME"
echo "  Port: $PORT"
echo "═══════════════════════════════════════════════════════════════"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "❌ Please run as root (use sudo)"
    exit 1
fi

# Detect OS
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$NAME
else
    OS=$(uname -s)
fi

echo "📋 Detected OS: $OS"
echo ""

# Install dependencies
echo "📦 Installing dependencies..."
if [[ "$OS" == *"Ubuntu"* ]] || [[ "$OS" == *"Debian"* ]]; then
    apt-get update -qq
    apt-get install -y -qq build-essential cmake libssl-dev liblz4-dev git wget openssl
elif [[ "$OS" == *"CentOS"* ]] || [[ "$OS" == *"Red Hat"* ]] || [[ "$OS" == *"Fedora"* ]]; then
    yum install -y -q gcc-c++ cmake openssl-devel lz4-devel git wget openssl
elif [[ "$OS" == *"Darwin"* ]]; then
    if ! command -v brew &> /dev/null; then
        echo "❌ Homebrew not found. Please install Homebrew first."
        exit 1
    fi
    brew install cmake openssl lz4 git wget
else
    echo "⚠️  Unknown OS. Please install manually: cmake, libssl-dev, liblz4-dev, git"
fi

echo "✅ Dependencies installed"
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
echo "✅ Directories created"
echo ""

# Build ScratchBird
echo "🔨 Building ScratchBird..."
if [ ! -d "/opt/ScratchBird" ]; then
    cd /opt
    git clone https://github.com/DaltonCalford/ScratchBird.git
fi

cd /opt/ScratchBird
git pull origin main 2>/dev/null || true

cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DSCRATCHBIRD_ENABLE_TLS=ON 2>&1 | tail -5

cmake --build build -j$(nproc) 2>&1 | tail -10

echo "✅ Build complete"
echo ""

# Create database
echo "🗄️  Creating test database..."
if [ -f "$DB_FILE" ]; then
    echo "⚠️  Database already exists. Skipping creation."
else
    ./build/bin/sb_createdb \
        --database="$DB_FILE" \
        --page-size=16384 \
        --encoding=UTF8 \
        --owner=admin 2>&1
    
    chown "$SB_USER:$SB_USER" "$DB_FILE"
    echo "✅ Database created at $DB_FILE"
fi
echo ""

# Generate TLS certificates
echo "🔐 Generating TLS certificates..."
if [ -f "/etc/scratchbird/server.crt" ]; then
    echo "⚠️  Certificates already exist. Skipping generation."
else
    openssl req -x509 -nodes -days 365 -newkey rsa:4096 \
        -keyout /etc/scratchbird/server.key \
        -out /etc/scratchbird/server.crt \
        -subj "/C=US/ST=Test/L=Test/O=ScratchBird/CN=$HOSTNAME" \
        2>/dev/null
    
    chmod 600 /etc/scratchbird/server.key
    chown "$SB_USER:$SB_USER" /etc/scratchbird/*.pem
    echo "✅ Self-signed certificates generated"
    echo "   Note: For production, use Let's Encrypt or proper CA certificates"
fi
echo ""

# Start server temporarily
echo "🚀 Starting temporary server for setup..."
./build/bin/sb_server \
    --database="$DB_FILE" \
    --port=$PORT \
    --tls-cert=/etc/scratchbird/server.crt \
    --tls-key=/etc/scratchbird/server.key &

SERVER_PID=$!
sleep 2

# Create test user
echo "👤 Creating test user..."
./build/bin/sb_isql \
    --host=localhost \
    --port=$PORT \
    --database=testdb \
    --user=admin \
    --query="
CREATE USER IF NOT EXISTS testuser PASSWORD 'SbTest2026!Alpha';
GRANT ALL ON DATABASE testdb TO testuser;
GRANT ALL ON SCHEMA test_schema TO testuser;
" 2>/dev/null || echo "User may already exist"

# Create schema
echo "📊 Creating test schema..."
./build/bin/sb_isql \
    --host=localhost \
    --port=$PORT \
    --database=testdb \
    --user=admin \
    --query="
CREATE SCHEMA IF NOT EXISTS test_schema;

CREATE TABLE IF NOT EXISTS test_schema.users (
    id INTEGER PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    username VARCHAR(50) NOT NULL,
    email VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    active BOOLEAN DEFAULT TRUE
);

INSERT INTO test_schema.users (username, email, active) 
SELECT 'alice', 'alice@example.com', TRUE
WHERE NOT EXISTS (SELECT 1 FROM test_schema.users WHERE username = 'alice');

INSERT INTO test_schema.users (username, email, active) 
SELECT 'bob', 'bob@example.com', TRUE
WHERE NOT EXISTS (SELECT 1 FROM test_schema.users WHERE username = 'bob');

INSERT INTO test_schema.users (username, email, active) 
SELECT 'charlie', 'charlie@example.com', FALSE
WHERE NOT EXISTS (SELECT 1 FROM test_schema.users WHERE username = 'charlie');
" 2>/dev/null

# Stop temporary server
kill $SERVER_PID 2>/dev/null || true
sleep 1

echo "✅ Database setup complete"
echo ""

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

ExecStart=/opt/ScratchBird/build/bin/sb_server \\
    --database=$DB_FILE \\
    --port=$PORT \\
    --bind=0.0.0.0 \\
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
echo "✅ Service created and enabled"
echo ""

# Configure firewall
echo "🔥 Configuring firewall..."
if command -v ufw &> /dev/null; then
    ufw allow $PORT/tcp comment 'ScratchBird Test Server' 2>/dev/null || true
    echo "✅ UFW rule added"
elif command -v firewall-cmd &> /dev/null; then
    firewall-cmd --permanent --add-port=$PORT/tcp 2>/dev/null || true
    firewall-cmd --reload 2>/dev/null || true
    echo "✅ firewalld rule added"
elif command -v iptables &> /dev/null; then
    iptables -A INPUT -p tcp --dport $PORT -j ACCEPT 2>/dev/null || true
    echo "✅ iptables rule added"
else
    echo "⚠️  No firewall detected. Please manually open port $PORT."
fi
echo ""

# Start service
echo "🚀 Starting ScratchBird test server..."
systemctl start scratchbird-test.service
sleep 2

# Check status
if systemctl is-active --quiet scratchbird-test.service; then
    echo "✅ Service is running"
else
    echo "❌ Service failed to start. Check logs:"
    systemctl status scratchbird-test.service
    exit 1
fi
echo ""

# Test connection
echo "🧪 Testing connection..."
if /opt/ScratchBird/build/bin/sb_isql \
    --host=localhost \
    --port=$PORT \
    --database=testdb \
    --user=testuser \
    --password='SbTest2026!Alpha' \
    --query="SELECT 'Connection successful' AS status;" 2>/dev/null | grep -q "successful"; then
    echo "✅ Connection test passed"
else
    echo "⚠️  Connection test failed. Server may still be starting."
fi
echo ""

# Display summary
echo "═══════════════════════════════════════════════════════════════"
echo "  ✅ SETUP COMPLETE"
echo "═══════════════════════════════════════════════════════════════"
echo ""
echo "📋 Connection Parameters:"
echo "   Host:     $HOSTNAME"
echo "   Port:     $PORT"
echo "   Database: testdb"
echo "   Username: testuser"
echo "   Password: SbTest2026!Alpha"
echo "   TLS:      Required (TLS 1.3)"
echo ""
echo "🔗 Connection String:"
echo "   scratchbird://testuser:SbTest2026!Alpha@$HOSTNAME:$PORT/testdb"
echo ""
echo "⚙️  Service Commands:"
echo "   Start:   sudo systemctl start scratchbird-test"
echo "   Stop:    sudo systemctl stop scratchbird-test"
echo "   Status:  sudo systemctl status scratchbird-test"
echo "   Logs:    sudo tail -f $LOG_DIR/testdb.log"
echo ""
echo "📝 Quick Test:"
echo "   /opt/ScratchBird/build/bin/sb_isql \\"
echo "       --host=localhost --port=$PORT \\"
echo "       --user=testuser --password='SbTest2026!Alpha' \\"
echo "       --query=\"SELECT * FROM test_schema.users;\""
echo ""
echo "═══════════════════════════════════════════════════════════════"
