# macOS Installation

**Status:** Alpha documentation
**Last Updated:** 2026-01-18

---

## Overview

This guide covers installing ScratchBird on macOS. During the Alpha phase, building from source or using Docker Desktop are the recommended approaches. Homebrew packages will be available in Beta.

---

## System Requirements

### Hardware Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| CPU | Apple Silicon or Intel | Apple Silicon |
| RAM | 4 GB | 8+ GB |
| Disk | 2 GB free | 20+ GB SSD |

### Supported macOS Versions

| Version | Architecture | Status |
|---------|--------------|--------|
| macOS 14 (Sonoma) | Apple Silicon, Intel | Tested |
| macOS 13 (Ventura) | Apple Silicon, Intel | Tested |
| macOS 12 (Monterey) | Apple Silicon, Intel | Tested |

---

## Method 1: Docker Desktop (Recommended for Quick Start)

The easiest way to run ScratchBird on macOS.

### Step 1: Install Docker Desktop

1. Download Docker Desktop from [docker.com](https://www.docker.com/products/docker-desktop/)
2. Open the `.dmg` file and drag Docker to Applications
3. Launch Docker Desktop and complete the setup
4. For Apple Silicon Macs, Docker will use Rosetta 2 or native ARM images

### Step 2: Run ScratchBird Container

Open Terminal:

```bash
# Pull and run ScratchBird
docker run -d \
    --name scratchbird \
    -p 3092:3092 \
    -p 5432:5432 \
    -v scratchbird_data:/var/lib/scratchbird \
    scratchbird/scratchbird:latest

# Verify container is running
docker ps

# View logs
docker logs -f scratchbird
```

### Step 3: Connect

```bash
# Using native client (from container)
docker exec -it scratchbird sb_isql -U admin -d scratchbird

# Using psql (if installed via Homebrew)
psql -h localhost -p 5432 -U admin -d scratchbird
```

### Docker Compose Setup

Create `docker-compose.yml`:

```yaml
version: '3.8'

services:
  scratchbird:
    image: scratchbird/scratchbird:latest
    container_name: scratchbird
    restart: unless-stopped
    ports:
      - "3092:3092"
      - "5432:5432"
      - "3306:3306"
      - "3050:3050"
    environment:
      SCRATCHBIRD_USER: admin
      SCRATCHBIRD_PASSWORD: changeme
      SCRATCHBIRD_DB: scratchbird
    volumes:
      - scratchbird_data:/var/lib/scratchbird

volumes:
  scratchbird_data:
```

Run with:
```bash
docker compose up -d
```

---

## Method 2: Building from Source

### Prerequisites

#### Xcode Command Line Tools

```bash
xcode-select --install
```

#### Homebrew

If not already installed:
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

#### Install Dependencies

```bash
brew install cmake pkg-config openssl@3 lz4 geos proj libxml2 git
```

### Step 1: Clone Repository

```bash
cd ~/Projects
git clone https://github.com/scratchbird/scratchbird.git
cd scratchbird
```

### Step 2: Configure Build

```bash
# Create build directory
mkdir -p build && cd build

# Configure with CMake
# For Apple Silicon (M1/M2/M3)
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3) \
    -DCMAKE_PREFIX_PATH="$(brew --prefix lz4);$(brew --prefix geos);$(brew --prefix proj)"

# For Intel Macs (if needed)
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
```

**Build Options:**

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Release | Build type (Release, Debug) |
| `CMAKE_OSX_ARCHITECTURES` | Native | Target architecture (arm64, x86_64) |
| `SCRATCHBIRD_WITH_COMPILER` | ON | Include SQL compiler/parsers |

### Step 3: Build

```bash
# Build using all available cores
cmake --build . -j$(sysctl -n hw.ncpu)

# Or use make
make -j$(sysctl -n hw.ncpu)
```

### Step 4: Run Tests

```bash
ctest --output-on-failure
```

### Step 5: Install

```bash
# Install to /usr/local
sudo cmake --install .

# Or install to custom location
cmake --install . --prefix ~/scratchbird
```

**Installed Files:**

```
/usr/local/
├── bin/
│   ├── sb_server
│   ├── sb_isql
│   ├── sb_admin
│   ├── sb_backup
│   ├── sb_restore
│   └── sb_security
└── lib/
    └── libscratchbird.dylib
```

---

## Post-Installation Setup

### Create Directories

```bash
# Create data and log directories
sudo mkdir -p /usr/local/var/scratchbird/data
sudo mkdir -p /usr/local/var/log/scratchbird

# Set ownership to your user (for development)
sudo chown -R $(whoami) /usr/local/var/scratchbird
sudo chown -R $(whoami) /usr/local/var/log/scratchbird

# Or create dedicated user (for production)
sudo dscl . -create /Users/scratchbird
sudo dscl . -create /Users/scratchbird UserShell /usr/bin/false
sudo dscl . -create /Users/scratchbird UniqueID 400
sudo dscl . -create /Users/scratchbird PrimaryGroupID 400
sudo chown -R scratchbird /usr/local/var/scratchbird
```

### Create Configuration File

```bash
sudo mkdir -p /usr/local/etc/scratchbird

cat << 'EOF' | sudo tee /usr/local/etc/scratchbird/sb_server.conf
# ScratchBird Server Configuration (macOS)

[server]
mode = multi-database
data_dir = /usr/local/var/scratchbird/data
max_connections = 100
worker_threads = 0

[network]
bind_address = 127.0.0.1
native_port = 3092
pg_port = 5432
mysql_port = 3306
fb_port = 3050

[ssl]
enabled = false

[memory]
buffer_pool_size = 128MB
work_mem = 4MB

[logging]
level = info
destination = file
file = /usr/local/var/log/scratchbird/sb_server.log
EOF
```

---

## Running ScratchBird

### Manual Start (Foreground)

```bash
# Run in foreground (for development/debugging)
sb_server -F --config /usr/local/etc/scratchbird/sb_server.conf
```

### Manual Start (Background)

```bash
# Start in background
sb_server --config /usr/local/etc/scratchbird/sb_server.conf &

# Check if running
pgrep sb_server
```

### Using launchd (Recommended for Production)

Create a launchd plist:

```bash
cat << 'EOF' | sudo tee /Library/LaunchDaemons/com.scratchbird.server.plist
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.scratchbird.server</string>

    <key>ProgramArguments</key>
    <array>
        <string>/usr/local/bin/sb_server</string>
        <string>--config</string>
        <string>/usr/local/etc/scratchbird/sb_server.conf</string>
    </array>

    <key>RunAtLoad</key>
    <true/>

    <key>KeepAlive</key>
    <true/>

    <key>WorkingDirectory</key>
    <string>/usr/local/var/scratchbird</string>

    <key>StandardOutPath</key>
    <string>/usr/local/var/log/scratchbird/stdout.log</string>

    <key>StandardErrorPath</key>
    <string>/usr/local/var/log/scratchbird/stderr.log</string>

    <key>SoftResourceLimits</key>
    <dict>
        <key>NumberOfFiles</key>
        <integer>65536</integer>
    </dict>
</dict>
</plist>
EOF

# Load the service
sudo launchctl load /Library/LaunchDaemons/com.scratchbird.server.plist

# Check status
sudo launchctl list | grep scratchbird
```

### Service Management with launchd

```bash
# Start service
sudo launchctl load /Library/LaunchDaemons/com.scratchbird.server.plist

# Stop service
sudo launchctl unload /Library/LaunchDaemons/com.scratchbird.server.plist

# Restart service
sudo launchctl unload /Library/LaunchDaemons/com.scratchbird.server.plist
sudo launchctl load /Library/LaunchDaemons/com.scratchbird.server.plist

# Check if running
sudo launchctl list | grep scratchbird

# View logs
tail -f /usr/local/var/log/scratchbird/sb_server.log
```

---

## Connecting to ScratchBird

### Using sb_isql (Native Client)

```bash
# Connect to local server
sb_isql -H localhost -p 3092 -U admin -d scratchbird

# With explicit database
sb_isql -H localhost -p 3092 -U admin -d mydb
```

### Using psql (PostgreSQL Client)

Install psql via Homebrew if needed:
```bash
brew install libpq
echo 'export PATH="/opt/homebrew/opt/libpq/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

Connect:
```bash
psql -h localhost -p 5432 -U admin -d scratchbird
```

### Using GUI Tools

ScratchBird is compatible with:

- **TablePlus** - Native macOS app, supports PostgreSQL and MySQL
- **DBeaver** - Cross-platform, connect using PostgreSQL driver
- **Postico** - PostgreSQL client for macOS
- **DataGrip** - JetBrains IDE with database support

Connection settings:
- Host: `localhost`
- Port: `5432` (PostgreSQL) or `3306` (MySQL) or `3092` (Native)
- Username: `admin`
- Database: `scratchbird`

---

## Firewall Configuration

macOS firewall typically doesn't block localhost connections. For remote access:

```bash
# Allow ScratchBird through firewall (if needed)
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --add /usr/local/bin/sb_server
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --unblockapp /usr/local/bin/sb_server
```

---

## Troubleshooting

### Server Won't Start

```bash
# Check if ports are in use
lsof -i :3092
lsof -i :5432

# Check configuration
sb_server --config /usr/local/etc/scratchbird/sb_server.conf --check

# Run in foreground for debugging
sb_server -F --config /usr/local/etc/scratchbird/sb_server.conf
```

### Connection Refused

```bash
# Check if server is running
pgrep sb_server

# Check listening ports
netstat -an | grep LISTEN | grep -E '3092|5432'

# Check launchd status
sudo launchctl list | grep scratchbird
```

### Build Errors

```bash
# Verify Xcode tools
xcode-select -p

# Check Homebrew packages
brew list | grep -E 'cmake|openssl|lz4|geos'

# Clean rebuild
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
make -j$(sysctl -n hw.ncpu)
```

### Library Not Found Errors

```bash
# Check library paths
otool -L /usr/local/bin/sb_server

# Update library paths if needed
export DYLD_LIBRARY_PATH="/usr/local/lib:$DYLD_LIBRARY_PATH"

# Or fix with install_name_tool
sudo install_name_tool -add_rpath /usr/local/lib /usr/local/bin/sb_server
```

### Apple Silicon Specific Issues

```bash
# Check architecture of binary
file /usr/local/bin/sb_server

# Force rebuild for arm64
cmake .. -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_BUILD_TYPE=Release
make clean && make -j$(sysctl -n hw.ncpu)
```

---

## Uninstallation

### Stop and Remove Service

```bash
# Unload launchd service
sudo launchctl unload /Library/LaunchDaemons/com.scratchbird.server.plist
sudo rm /Library/LaunchDaemons/com.scratchbird.server.plist
```

### Remove Files

```bash
# Remove binaries
sudo rm -f /usr/local/bin/sb_*
sudo rm -f /usr/local/lib/libscratchbird*

# Remove configuration
sudo rm -rf /usr/local/etc/scratchbird

# Remove data (CAUTION: removes all databases)
sudo rm -rf /usr/local/var/scratchbird
sudo rm -rf /usr/local/var/log/scratchbird
```

---

## Next Steps

- [First Connection](../getting-started/first-connection.md) - Connect and run your first query
- [Basic SQL](../getting-started/basic-sql.md) - Learn ScratchBird SQL basics
- [Configuration Reference](../reference/Configuration.md) - Full configuration options

