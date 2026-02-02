# Linux Installation

**Last Updated:** 2026-01-30

---

## Overview

This guide covers installing ScratchBird on Linux systems. During the Alpha phase, building from source is the recommended approach. Package repositories will be available in Beta.

---

## System Requirements

### Hardware Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| CPU | 2 cores | 4+ cores |
| RAM | 2 GB | 8+ GB |
| Disk | 1 GB free | 20+ GB SSD |
| Architecture | x86_64, ARM64 | x86_64 |

### Supported Distributions

| Distribution | Version | Status |
|--------------|---------|--------|
| Ubuntu | 22.04 LTS, 24.04 LTS | Tested |
| Debian | 12 (Bookworm) | Tested |
| Fedora | 38, 39, 40 | Tested |
| RHEL/Rocky/Alma | 8, 9 | Tested |
| Arch Linux | Rolling | Community |
| openSUSE | Leap 15.5+ | Community |

---

## Method 1: Building from Source (Recommended for Alpha)

### Step 1: Install Build Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    pkg-config \
    libssl-dev \
    liblz4-dev \
    libgeos-dev \
    libproj-dev \
    libxml2-dev \
    git
```

**Fedora:**
```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    pkg-config \
    openssl-devel \
    lz4-devel \
    geos-devel \
    proj-devel \
    libxml2-devel \
    git
```

**RHEL/Rocky/Alma 8/9:**
```bash
sudo dnf install -y epel-release
sudo dnf install -y \
    gcc-c++ \
    cmake \
    pkg-config \
    openssl-devel \
    lz4-devel \
    geos-devel \
    proj-devel \
    libxml2-devel \
    git
```

**Arch Linux:**
```bash
sudo pacman -S --needed \
    base-devel \
    cmake \
    pkg-config \
    openssl \
    lz4 \
    geos \
    proj \
    libxml2 \
    git
```

### Step 2: Clone the Repository

```bash
git clone https://github.com/scratchbird/scratchbird.git
cd scratchbird
```

### Step 3: Configure the Build

```bash
# Create build directory
mkdir -p build && cd build

# Configure with CMake (Release build)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Or configure with debug symbols
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Or configure without compiler/parsers (engine-only)
cmake .. -DCMAKE_BUILD_TYPE=Release -DSCRATCHBIRD_WITH_COMPILER=OFF
```

**Build Configuration Options:**

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Release | Build type (Release, Debug, RelWithDebInfo) |
| `SCRATCHBIRD_WITH_COMPILER` | ON | Include SQL compiler/parsers |
| `BUILD_TESTING` | ON | Build test suite |

The build system automatically detects optional libraries:
- **LZ4**: Page compression (recommended)
- **GEOS**: Spatial functions
- **PROJ**: Coordinate system transformations
- **libxml2**: XML data type support

### Step 4: Build

```bash
# Build using all available cores
cmake --build . -j$(nproc)

# Or use make directly
make -j$(nproc)
```

Build output will show which optional features are enabled:
```
-- LZ4 compression support: ENABLED
-- GEOS spatial library: ENABLED
-- PROJ coordinate system library: ENABLED
-- nlohmann/json library: ENABLED (FetchContent)
```

### Step 5: Run Tests (Optional but Recommended)

```bash
# Run all tests
ctest --output-on-failure

# Run specific test categories
ctest -R unit --output-on-failure
ctest -R integration --output-on-failure
```

### Step 6: Install

```bash
# Install to /usr/local (requires root)
sudo cmake --install .

# Or install to custom prefix
cmake --install . --prefix /opt/scratchbird
```

**Installed Files:**

```
/usr/local/
├── bin/
│   ├── sb_server          # Database server
│   ├── sb_isql            # Interactive SQL client
│   ├── sb_admin           # Administration utility
│   ├── sb_backup          # Backup utility
│   ├── sb_restore         # Restore utility
│   ├── sb_verify          # Database verification
│   └── sb_security        # Security management
└── lib/
    └── libscratchbird.so  # Shared library
```

---

## Method 2: Docker Container

For quick evaluation or when you prefer containerized deployments.

### Install Docker

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y docker.io docker-compose-v2
sudo systemctl enable --now docker
sudo usermod -aG docker $USER
```

**Fedora:**
```bash
sudo dnf install -y docker docker-compose
sudo systemctl enable --now docker
sudo usermod -aG docker $USER
```

Log out and back in for group changes to take effect.

### Run ScratchBird Container

```bash
# Pull and run (when official images are available)
docker run -d \
    --name scratchbird \
    -p 3092:3092 \
    -p 5432:5432 \
    -v scratchbird_data:/var/lib/scratchbird \
    scratchbird/scratchbird:latest

# Or build locally
cd /path/to/scratchbird
docker build -t scratchbird:local .
docker run -d \
    --name scratchbird \
    -p 3092:3092 \
    -p 5432:5432 \
    -v scratchbird_data:/var/lib/scratchbird \
    scratchbird:local
```

### Docker Compose

Create `docker-compose.yml`:

```yaml
version: '3.8'

services:
  scratchbird:
    image: scratchbird/scratchbird:latest
    container_name: scratchbird
    restart: unless-stopped
    ports:
      - "3092:3092"   # Native protocol
      - "5432:5432"   # PostgreSQL protocol
      - "3306:3306"   # MySQL protocol
      - "3050:3050"   # Firebird protocol
    environment:
      SCRATCHBIRD_USER: admin
      SCRATCHBIRD_PASSWORD: ${DB_PASSWORD:-changeme}
      SCRATCHBIRD_DB: scratchbird
    volumes:
      - scratchbird_data:/var/lib/scratchbird
    healthcheck:
      test: ["CMD", "sb_isql", "-c", "SELECT 1"]
      interval: 10s
      timeout: 5s
      retries: 5

volumes:
  scratchbird_data:
```

Start with:
```bash
docker compose up -d
```

---

## Post-Installation Setup

### Create System User and Directories

```bash
# Create dedicated user
sudo useradd --system \
    --home-dir /var/lib/scratchbird \
    --shell /usr/sbin/nologin \
    scratchbird

# Create directories
sudo mkdir -p /var/lib/scratchbird
sudo mkdir -p /var/log/scratchbird
sudo mkdir -p /var/run/scratchbird
sudo mkdir -p /etc/scratchbird
sudo mkdir -p /etc/scratchbird/ssl

# Set ownership
sudo chown -R scratchbird:scratchbird /var/lib/scratchbird
sudo chown -R scratchbird:scratchbird /var/log/scratchbird
sudo chown -R scratchbird:scratchbird /var/run/scratchbird

# Set permissions
sudo chmod 700 /var/lib/scratchbird
sudo chmod 750 /var/log/scratchbird
sudo chmod 755 /var/run/scratchbird
```

### Create Configuration File

```bash
sudo tee /etc/scratchbird/sb_server.conf << 'EOF'
# ScratchBird Server Configuration

[server]
mode = multi-database
data_dir = /var/lib/scratchbird
max_connections = 100
worker_threads = 0  # Auto-detect based on CPU cores
shutdown_timeout = 30

[network]
bind_address = 0.0.0.0
native_port = 3092
pg_port = 5432
mysql_port = 3306
fb_port = 3050
unix_socket = /var/run/scratchbird/sb.sock

[ssl]
enabled = false
# Enable for production:
# enabled = true
# cert_file = /etc/scratchbird/ssl/server.crt
# key_file = /etc/scratchbird/ssl/server.key

[memory]
buffer_pool_size = 128MB
work_mem = 4MB

[logging]
level = info
destination = file
file = /var/log/scratchbird/sb_server.log

[authentication]
methods = scram-sha-256
EOF

sudo chown scratchbird:scratchbird /etc/scratchbird/sb_server.conf
sudo chmod 640 /etc/scratchbird/sb_server.conf
```

### Set Up systemd Service

```bash
sudo tee /etc/systemd/system/scratchbird.service << 'EOF'
[Unit]
Description=ScratchBird Database Server
Documentation=https://github.com/scratchbird/scratchbird
After=network-online.target
Wants=network-online.target

[Service]
Type=notify
User=scratchbird
Group=scratchbird
Environment=SCRATCHBIRD_CONFIG=/etc/scratchbird/sb_server.conf
WorkingDirectory=/var/lib/scratchbird

ExecStart=/usr/local/bin/sb_server --config ${SCRATCHBIRD_CONFIG}
ExecReload=/bin/kill -HUP $MAINPID
ExecStop=/bin/kill -TERM $MAINPID

Restart=on-failure
RestartSec=5
TimeoutStopSec=30

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/scratchbird
ReadWritePaths=/var/log/scratchbird
ReadWritePaths=/var/run/scratchbird

[Install]
WantedBy=multi-user.target
EOF

# Reload systemd
sudo systemctl daemon-reload

# Enable service to start on boot
sudo systemctl enable scratchbird

# Start service
sudo systemctl start scratchbird

# Check status
sudo systemctl status scratchbird
```

### Verify Installation

```bash
# Check service status
sudo systemctl status scratchbird

# Check listening ports
ss -tlnp | grep -E '3092|5432|3306|3050'

# Connect with native client
sb_isql -H localhost -p 3092 -U admin

# Or with PostgreSQL client
psql -h localhost -p 5432 -U admin -d scratchbird
```

---

## Firewall Configuration

### UFW (Ubuntu)

```bash
# Allow ScratchBird ports
sudo ufw allow 3092/tcp comment 'ScratchBird Native'
sudo ufw allow 5432/tcp comment 'ScratchBird PostgreSQL'
sudo ufw allow 3306/tcp comment 'ScratchBird MySQL'
sudo ufw allow 3050/tcp comment 'ScratchBird Firebird'

# Reload firewall
sudo ufw reload
```

### firewalld (Fedora/RHEL)

```bash
# Allow ScratchBird ports
sudo firewall-cmd --permanent --add-port=3092/tcp
sudo firewall-cmd --permanent --add-port=5432/tcp
sudo firewall-cmd --permanent --add-port=3306/tcp
sudo firewall-cmd --permanent --add-port=3050/tcp

# Reload firewall
sudo firewall-cmd --reload
```

---

## Service Management

### Common Commands

```bash
# Start service
sudo systemctl start scratchbird

# Stop service
sudo systemctl stop scratchbird

# Restart service
sudo systemctl restart scratchbird

# Reload configuration (no restart)
sudo systemctl reload scratchbird

# View logs
sudo journalctl -u scratchbird -f

# View recent logs
sudo journalctl -u scratchbird --since "1 hour ago"
```

### Log Locations

| Log | Location |
|-----|----------|
| Server log | `/var/log/scratchbird/sb_server.log` |
| Audit log | `/var/log/scratchbird/audit.log` |
| systemd journal | `journalctl -u scratchbird` |

---

## Troubleshooting

### Server Won't Start

```bash
# Check configuration syntax
sb_server --config /etc/scratchbird/sb_server.conf --check

# Check if ports are in use
ss -tlnp | grep -E '3092|5432|3306|3050'

# Check permissions
ls -la /var/lib/scratchbird
ls -la /var/run/scratchbird

# Check SELinux (RHEL/Fedora)
sudo ausearch -m avc -ts recent
```

### Connection Refused

```bash
# Verify service is running
sudo systemctl status scratchbird

# Check bind address in config
grep bind_address /etc/scratchbird/sb_server.conf

# Check firewall
sudo ufw status  # Ubuntu
sudo firewall-cmd --list-all  # Fedora/RHEL
```

### Build Errors

```bash
# Missing dependencies - check CMake output
cmake .. 2>&1 | grep -i "not found"

# Clean rebuild
rm -rf build/*
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## Uninstallation

### Remove Installed Files

```bash
# If installed with cmake --install
sudo rm -f /usr/local/bin/sb_*
sudo rm -f /usr/local/lib/libscratchbird*

# Remove service
sudo systemctl stop scratchbird
sudo systemctl disable scratchbird
sudo rm /etc/systemd/system/scratchbird.service
sudo systemctl daemon-reload

# Remove user and directories (CAUTION: removes data)
sudo userdel scratchbird
sudo rm -rf /var/lib/scratchbird
sudo rm -rf /var/log/scratchbird
sudo rm -rf /var/run/scratchbird
sudo rm -rf /etc/scratchbird
```

---

## Next Steps

- [First Connection](../getting-started/first-connection.md) - Connect and run your first query
- [Basic SQL](../getting-started/basic-sql.md) - Learn ScratchBird SQL basics
- [Configuration Reference](../reference/Configuration.md) - Full configuration options
- [Security Setup](../admin/security.md) - Enable TLS and authentication

