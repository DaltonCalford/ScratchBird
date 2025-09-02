### Installation

**What it is**

ScratchBird installation involves setting up the database server, configuring system resources, initializing data directories, and establishing security settings. The installation process supports multiple platforms (Linux, macOS, Windows), deployment methods (bare metal, containers, cloud), and configuration options (single node, cluster, high availability).

**Why it matters**

- **Foundation**: Proper installation ensures stability and performance
- **Security**: Initial setup establishes security boundaries
- **Scalability**: Architecture decisions affect future growth
- **Maintenance**: Good setup simplifies administration
- **Recovery**: Proper configuration enables disaster recovery

**How to use it**

Choose an installation method based on your platform and requirements. Follow the platform-specific instructions to install binaries, create required users and directories, initialize the database cluster, configure networking and security, then start the service. Verify the installation with connection tests and initial queries.

## System Requirements

### Hardware Requirements

```bash
# Minimum Requirements
CPU: 2 cores (x86_64 or ARM64)
RAM: 2 GB
Storage: 10 GB free space
Network: 100 Mbps

# Recommended Production
CPU: 8+ cores
RAM: 16+ GB
Storage: 100+ GB SSD
Network: 1+ Gbps

# High Performance
CPU: 32+ cores
RAM: 128+ GB
Storage: NVMe SSD array
Network: 10+ Gbps
```

### Operating Systems

```bash
# Linux (Recommended)
- Ubuntu 20.04 LTS, 22.04 LTS
- Debian 10, 11, 12
- RHEL/CentOS/Rocky 8, 9
- Amazon Linux 2, 2023
- SUSE Linux Enterprise 15

# macOS
- macOS 11 (Big Sur) or later
- Apple Silicon (M1/M2) native support

# Windows
- Windows Server 2019, 2022
- Windows 10, 11 (development only)

# Container Platforms
- Docker 20.10+
- Kubernetes 1.21+
- Podman 3.0+
```

### Software Dependencies

```bash
# Build dependencies (for source installation)
gcc 9+ or clang 11+
cmake 3.16+
make or ninja
git

# Runtime dependencies
libc 2.28+
libssl 1.1.1+
libz 1.2.11+
libreadline 8.0+

# Optional dependencies
libicu 60+ (for collations)
libxml2 2.9+ (for XML support)
liblz4 1.9+ (for compression)
```

## Installation Methods

### Package Manager Installation

#### Ubuntu/Debian

```bash
# Add repository
wget -O - https://repo.scratchbird.io/gpg.key | sudo apt-key add -
echo "deb https://repo.scratchbird.io/apt $(lsb_release -cs) main" | \
    sudo tee /etc/apt/sources.list.d/scratchbird.list

# Install
sudo apt update
sudo apt install scratchbird scratchbird-client scratchbird-contrib

# Start service
sudo systemctl start scratchbird
sudo systemctl enable scratchbird
```

#### RHEL/CentOS/Rocky

```bash
# Add repository
sudo dnf config-manager --add-repo https://repo.scratchbird.io/rpm/scratchbird.repo

# Install
sudo dnf install scratchbird scratchbird-client scratchbird-contrib

# Initialize database
sudo /usr/bin/scratchbird-initdb -D /var/lib/scratchbird/data

# Start service
sudo systemctl start scratchbird
sudo systemctl enable scratchbird
```

#### macOS (Homebrew)

```bash
# Install
brew tap scratchbird/tap
brew install scratchbird

# Initialize database
scratchbird-initdb -D /usr/local/var/scratchbird

# Start service
brew services start scratchbird

# Or run in foreground
scratchbird-server -D /usr/local/var/scratchbird
```

### Binary Installation

```bash
# Download binary
wget https://github.com/scratchbird/releases/download/v1.0.0/scratchbird-1.0.0-linux-x64.tar.gz
tar xzf scratchbird-1.0.0-linux-x64.tar.gz
cd scratchbird-1.0.0

# Install to system
sudo cp bin/* /usr/local/bin/
sudo cp -r lib/* /usr/local/lib/
sudo cp -r share/* /usr/local/share/

# Create user and directories
sudo useradd -r -s /bin/false scratchbird
sudo mkdir -p /var/lib/scratchbird/data
sudo chown -R scratchbird:scratchbird /var/lib/scratchbird

# Initialize database
sudo -u scratchbird scratchbird-initdb -D /var/lib/scratchbird/data
```

### Source Installation

```bash
# Clone repository
git clone https://github.com/DaltonCalford/ScratchBird.git
cd ScratchBird

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Test
make test

# Install
sudo make install

# Initialize database
sudo scratchbird-initdb -D /var/lib/scratchbird/data
```

### Docker Installation

```bash
# Pull image
docker pull scratchbird/scratchbird:latest

# Run container
docker run -d \
    --name scratchbird \
    -p 5439:5439 \
    -v scratchbird-data:/var/lib/scratchbird/data \
    -e SCRATCHBIRD_PASSWORD=mysecretpassword \
    scratchbird/scratchbird:latest

# Connect to container
docker exec -it scratchbird scratchbird-cli

# Docker Compose
cat > docker-compose.yml << 'EOF'
version: '3.8'
services:
  scratchbird:
    image: scratchbird/scratchbird:latest
    container_name: scratchbird
    ports:
      - "5439:5439"
    volumes:
      - scratchbird-data:/var/lib/scratchbird/data
      - ./scratchbird.conf:/etc/scratchbird/scratchbird.conf
    environment:
      SCRATCHBIRD_USER: admin
      SCRATCHBIRD_PASSWORD: secretpass
      SCRATCHBIRD_DB: myapp
    restart: unless-stopped

volumes:
  scratchbird-data:
EOF

docker-compose up -d
```

## Initial Configuration

### Initialize Database Cluster

```bash
# Basic initialization
scratchbird-initdb -D /var/lib/scratchbird/data

# With specific locale
scratchbird-initdb -D /var/lib/scratchbird/data \
    --locale=en_US.UTF-8 \
    --encoding=UTF8

# With custom settings
scratchbird-initdb -D /var/lib/scratchbird/data \
    --auth=scram-sha-256 \
    --pwprompt \
    --data-checksums
```

### Configuration Files

```bash
# Main configuration
/etc/scratchbird/scratchbird.conf
# or
/var/lib/scratchbird/data/scratchbird.conf

# Example configuration
cat > /etc/scratchbird/scratchbird.conf << 'EOF'
# Network
BIND_ADDRESS=0.0.0.0
PORT=5439
MAX_CONNECTIONS=100

# Memory
SHARED_BUFFERS=256MB
WORK_MEM=4MB

# Logging
LOG_LEVEL=info
LOG_FILE=/var/log/scratchbird/server.log

# Data
DATA_DIR=/var/lib/scratchbird/data
EOF

# Host-based authentication
cat > /etc/scratchbird/hba.conf << 'EOF'
# TYPE  DATABASE  USER  ADDRESS         METHOD
local   all       all                   peer
host    all       all   127.0.0.1/32    scram-sha-256
host    all       all   ::1/128         scram-sha-256
host    all       all   192.168.0.0/16  scram-sha-256
EOF
```

### Systemd Service

```bash
# Service file
cat > /etc/systemd/system/scratchbird.service << 'EOF'
[Unit]
Description=ScratchBird Database Server
After=network.target

[Service]
Type=simple
User=scratchbird
Group=scratchbird
ExecStart=/usr/bin/scratchbird-server -D /var/lib/scratchbird/data
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=2s
LimitNOFILE=65536
Environment=SCRATCHBIRD_LOG_LEVEL=info

[Install]
WantedBy=multi-user.target
EOF

# Reload and start
sudo systemctl daemon-reload
sudo systemctl start scratchbird
sudo systemctl enable scratchbird
```

## Post-Installation Setup

### Create Initial Users

```bash
# Connect as superuser
scratchbird-cli -U scratchbird

# Create application user
CREATE USER app_user PASSWORD 'SecurePass123!';
CREATE DATABASE app_db OWNER app_user;
GRANT ALL PRIVILEGES ON DATABASE app_db TO app_user;

# Create read-only user
CREATE USER reader PASSWORD 'ReadOnly456!';
GRANT CONNECT ON DATABASE app_db TO reader;
GRANT USAGE ON SCHEMA public TO reader;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO reader;
```

### Security Hardening

```bash
# File permissions
chmod 700 /var/lib/scratchbird/data
chmod 600 /var/lib/scratchbird/data/*.conf

# Firewall rules (UFW)
sudo ufw allow from 192.168.0.0/16 to any port 5439
sudo ufw reload

# Firewall rules (firewalld)
sudo firewall-cmd --permanent --add-rich-rule='
  rule family="ipv4" 
  source address="192.168.0.0/16" 
  port protocol="tcp" port="5439" accept'
sudo firewall-cmd --reload

# SSL/TLS setup
openssl req -new -x509 -days 365 -nodes \
    -out /etc/scratchbird/server.crt \
    -keyout /etc/scratchbird/server.key \
    -subj "/CN=scratchbird.example.com"

chmod 600 /etc/scratchbird/server.key
chown scratchbird:scratchbird /etc/scratchbird/server.*

# Enable SSL in configuration
echo "SSL_MODE=require" >> /etc/scratchbird/scratchbird.conf
echo "SSL_CERT_FILE=/etc/scratchbird/server.crt" >> /etc/scratchbird/scratchbird.conf
echo "SSL_KEY_FILE=/etc/scratchbird/server.key" >> /etc/scratchbird/scratchbird.conf
```

### Performance Tuning

```bash
# Kernel parameters
cat > /etc/sysctl.d/99-scratchbird.conf << 'EOF'
# Shared memory
kernel.shmmax = 17179869184
kernel.shmall = 4194304
kernel.shmmni = 4096

# Semaphores
kernel.sem = 250 32000 100 128

# Network
net.core.rmem_default = 262144
net.core.rmem_max = 4194304
net.core.wmem_default = 262144
net.core.wmem_max = 1048576
net.ipv4.tcp_keepalive_time = 120
net.ipv4.tcp_keepalive_intvl = 30
net.ipv4.tcp_keepalive_probes = 3

# File handles
fs.file-max = 65536
EOF

sudo sysctl -p /etc/sysctl.d/99-scratchbird.conf

# User limits
cat > /etc/security/limits.d/99-scratchbird.conf << 'EOF'
scratchbird soft nofile 65536
scratchbird hard nofile 65536
scratchbird soft nproc 16384
scratchbird hard nproc 16384
EOF
```

## Verification

### Connection Test

```bash
# Local connection
scratchbird-cli -U scratchbird -d postgres -c "SELECT version();"

# Remote connection
scratchbird-cli -h localhost -p 5439 -U app_user -d app_db

# Test query
echo "SELECT current_database(), current_user, version();" | scratchbird-cli

# Health check
curl http://localhost:5439/health
```

### Performance Baseline

```bash
# Initialize test database
scratchbird-cli -c "CREATE DATABASE testdb;"

# Run benchmark
scratchbird-bench -i -s 10 testdb
scratchbird-bench -c 10 -j 2 -T 60 testdb

# Check results
scratchbird-cli testdb -c "
SELECT 
    tablename,
    pg_size_pretty(pg_total_relation_size(schemaname||'.'||tablename)) AS size
FROM pg_tables
WHERE schemaname = 'public'
ORDER BY pg_total_relation_size(schemaname||'.'||tablename) DESC;"
```

## Cluster Installation

### Multi-Node Setup

```bash
# Node 1 (Primary)
scratchbird-initdb -D /var/lib/scratchbird/data
echo "listen_addresses = '*'" >> /var/lib/scratchbird/data/scratchbird.conf
echo "wal_level = replica" >> /var/lib/scratchbird/data/scratchbird.conf
echo "max_wal_senders = 3" >> /var/lib/scratchbird/data/scratchbird.conf

# Node 2 (Replica)
scratchbird-basebackup -h node1 -D /var/lib/scratchbird/data -R
echo "hot_standby = on" >> /var/lib/scratchbird/data/scratchbird.conf

# Start both nodes
systemctl start scratchbird
```

### High Availability

```bash
# Install cluster manager
apt install scratchbird-ha

# Configure automatic failover
cat > /etc/scratchbird-ha/config.yml << 'EOF'
cluster:
  name: production
  nodes:
    - host: node1
      role: primary
      priority: 100
    - host: node2
      role: standby
      priority: 90
    - host: node3
      role: standby
      priority: 80
  
  failover:
    automatic: true
    timeout: 30
    max_retries: 3
EOF

# Start HA service
systemctl start scratchbird-ha
```

## Troubleshooting

### Common Issues

```bash
# Port already in use
lsof -i :5439
# Solution: Change port or stop conflicting service

# Permission denied
# Solution: Check file ownership
chown -R scratchbird:scratchbird /var/lib/scratchbird

# Cannot allocate memory
# Solution: Increase shared memory
sysctl -w kernel.shmmax=17179869184

# Connection refused
# Solution: Check bind address and firewall
netstat -tlnp | grep 5439
```

### Logs and Diagnostics

```bash
# View logs
journalctl -u scratchbird -f
tail -f /var/log/scratchbird/server.log

# Check configuration
scratchbird-server --check-config -D /var/lib/scratchbird/data

# Database integrity
scratchbird-check /var/lib/scratchbird/data

# Space usage
scratchbird-space /var/lib/scratchbird/data
```

## Uninstallation

```bash
# Stop service
sudo systemctl stop scratchbird
sudo systemctl disable scratchbird

# Package manager
sudo apt remove scratchbird  # Debian/Ubuntu
sudo dnf remove scratchbird  # RHEL/CentOS
brew uninstall scratchbird   # macOS

# Remove data (CAUTION: destroys all data)
sudo rm -rf /var/lib/scratchbird
sudo rm -rf /etc/scratchbird
sudo rm -rf /var/log/scratchbird

# Remove user
sudo userdel scratchbird
```

## Implementation Details

**Installer** (`packaging/`):
- Platform-specific packages
- Service definitions
- Default configurations

**Configuration** (`packaging/config/scratchbird.conf`):
- Default settings
- Environment variables
- Runtime parameters

**Service Management** (`packaging/systemd/scratchbird.service`):
- Systemd unit file
- Process management
- Resource limits

**Code Anchors**:
- Default config: `packaging/config/scratchbird.conf`
- Systemd service: `packaging/systemd/scratchbird.service`
- Docker files: `packaging/docker/`
- Build system: `CMakeLists.txt`

## See also

- [Configuration](./configuration.md) - Detailed configuration options
- [CLI Tools](./cli-tools.md) - Administrative utilities
- [Security](./ddl-roles-users-grants.md) - User management
- [Performance](./explain-analyze.md) - Tuning queries
- [Cluster](./ddl-cluster.md) - Cluster configuration