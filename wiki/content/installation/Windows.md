# Windows Installation

**Status:** Alpha documentation
**Last Updated:** 2026-01-18

---

## Overview

This guide covers installing ScratchBird on Windows systems. During the Alpha phase, building from source or using Docker with WSL2 are the recommended approaches. Native Windows installers will be available in Beta.

---

## System Requirements

### Hardware Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| CPU | 2 cores | 4+ cores |
| RAM | 4 GB | 8+ GB |
| Disk | 2 GB free | 20+ GB SSD |
| Architecture | x86_64 | x86_64 |

### Supported Windows Versions

| Version | Status |
|---------|--------|
| Windows 11 | Tested |
| Windows 10 (1903+) | Tested |
| Windows Server 2022 | Tested |
| Windows Server 2019 | Tested |

---

## Method 1: Docker with WSL2 (Recommended)

The easiest way to run ScratchBird on Windows is using Docker Desktop with WSL2 backend.

### Step 1: Enable WSL2

Open PowerShell as Administrator:

```powershell
# Enable WSL
wsl --install

# Set WSL2 as default
wsl --set-default-version 2

# Restart your computer
Restart-Computer
```

### Step 2: Install Docker Desktop

1. Download Docker Desktop from [docker.com](https://www.docker.com/products/docker-desktop/)
2. Run the installer
3. During installation, ensure "Use WSL 2 instead of Hyper-V" is selected
4. Restart if prompted

### Step 3: Run ScratchBird Container

Open PowerShell or Command Prompt:

```powershell
# Pull and run ScratchBird
docker run -d `
    --name scratchbird `
    -p 3092:3092 `
    -p 5432:5432 `
    -v scratchbird_data:/var/lib/scratchbird `
    scratchbird/scratchbird:latest

# Verify container is running
docker ps

# View logs
docker logs scratchbird
```

### Step 4: Connect

```powershell
# Using native client (from container)
docker exec -it scratchbird sb_isql -U admin -d scratchbird

# Using psql (if installed)
psql -h localhost -p 5432 -U admin -d scratchbird
```

### Docker Compose Setup

Create a file named `docker-compose.yml`:

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
```powershell
docker compose up -d
```

---

## Method 2: Building from Source

### Prerequisites

#### Visual Studio 2022

1. Download [Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/) (free)
2. During installation, select:
   - "Desktop development with C++"
   - Windows 10/11 SDK
   - CMake tools for Windows

#### CMake

If not using Visual Studio's CMake:
1. Download from [cmake.org](https://cmake.org/download/)
2. Choose "Windows x64 Installer"
3. During installation, select "Add CMake to system PATH"

#### Git

1. Download from [git-scm.com](https://git-scm.com/download/win)
2. Install with default options

#### vcpkg (Package Manager)

```powershell
# Clone vcpkg
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Bootstrap
.\bootstrap-vcpkg.bat

# Install dependencies
.\vcpkg install openssl:x64-windows
.\vcpkg install lz4:x64-windows
.\vcpkg install geos:x64-windows
.\vcpkg install proj:x64-windows
.\vcpkg install libxml2:x64-windows

# Integrate with Visual Studio
.\vcpkg integrate install
```

### Step 1: Clone Repository

```powershell
cd C:\Projects
git clone https://github.com/scratchbird/scratchbird.git
cd scratchbird
```

### Step 2: Configure Build

**Using CMake GUI:**

1. Open CMake GUI
2. Set source directory: `C:\Projects\scratchbird`
3. Set build directory: `C:\Projects\scratchbird\build`
4. Click "Configure"
5. Select "Visual Studio 17 2022" and "x64"
6. Set `CMAKE_TOOLCHAIN_FILE` to `C:\vcpkg\scripts\buildsystems\vcpkg.cmake`
7. Click "Generate"

**Using Command Line:**

```powershell
# Create build directory
mkdir build
cd build

# Configure with vcpkg toolchain
cmake .. -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
    -DCMAKE_BUILD_TYPE=Release
```

### Step 3: Build

**Using Visual Studio:**

1. Open `C:\Projects\scratchbird\build\ScratchBird.sln`
2. Select "Release" configuration and "x64" platform
3. Build > Build Solution (Ctrl+Shift+B)

**Using Command Line:**

```powershell
cmake --build . --config Release -j
```

### Step 4: Run Tests

```powershell
cd build
ctest -C Release --output-on-failure
```

### Step 5: Install

```powershell
# Install to C:\Program Files\ScratchBird
cmake --install . --config Release

# Or specify custom prefix
cmake --install . --config Release --prefix C:\ScratchBird
```

---

## Post-Installation Setup

### Directory Structure

After installation:

```
C:\Program Files\ScratchBird\
├── bin\
│   ├── sb_server.exe
│   ├── sb_isql.exe
│   ├── sb_admin.exe
│   ├── sb_backup.exe
│   ├── sb_restore.exe
│   └── sb_security.exe
├── lib\
│   └── scratchbird.dll
└── etc\
    └── sb_server.conf.example
```

### Create Data Directories

```powershell
# Create data directory
New-Item -ItemType Directory -Path "C:\ProgramData\ScratchBird\data" -Force
New-Item -ItemType Directory -Path "C:\ProgramData\ScratchBird\log" -Force

# Set permissions (optional, for multi-user systems)
icacls "C:\ProgramData\ScratchBird" /grant "Users:(OI)(CI)F"
```

### Create Configuration File

Create `C:\ProgramData\ScratchBird\sb_server.conf`:

```ini
# ScratchBird Server Configuration (Windows)

[server]
mode = multi-database
data_dir = C:\ProgramData\ScratchBird\data
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
file = C:\ProgramData\ScratchBird\log\sb_server.log
```

### Add to PATH

```powershell
# Add to user PATH
$env:Path += ";C:\Program Files\ScratchBird\bin"
[Environment]::SetEnvironmentVariable("Path", $env:Path, "User")

# Or add to system PATH (requires admin)
[Environment]::SetEnvironmentVariable(
    "Path",
    [Environment]::GetEnvironmentVariable("Path", "Machine") + ";C:\Program Files\ScratchBird\bin",
    "Machine"
)
```

---

## Running as a Windows Service

### Install as Service

Using [NSSM](https://nssm.cc/) (Non-Sucking Service Manager):

```powershell
# Download and extract NSSM
Invoke-WebRequest -Uri "https://nssm.cc/release/nssm-2.24.zip" -OutFile nssm.zip
Expand-Archive nssm.zip -DestinationPath C:\Tools

# Install service
C:\Tools\nssm-2.24\win64\nssm.exe install ScratchBird "C:\Program Files\ScratchBird\bin\sb_server.exe"
C:\Tools\nssm-2.24\win64\nssm.exe set ScratchBird AppParameters "--config C:\ProgramData\ScratchBird\sb_server.conf"
C:\Tools\nssm-2.24\win64\nssm.exe set ScratchBird AppDirectory "C:\ProgramData\ScratchBird"
C:\Tools\nssm-2.24\win64\nssm.exe set ScratchBird Start SERVICE_AUTO_START
C:\Tools\nssm-2.24\win64\nssm.exe set ScratchBird AppStdout "C:\ProgramData\ScratchBird\log\stdout.log"
C:\Tools\nssm-2.24\win64\nssm.exe set ScratchBird AppStderr "C:\ProgramData\ScratchBird\log\stderr.log"

# Start service
Start-Service ScratchBird

# Check status
Get-Service ScratchBird
```

### Service Management

```powershell
# Start service
Start-Service ScratchBird

# Stop service
Stop-Service ScratchBird

# Restart service
Restart-Service ScratchBird

# View service status
Get-Service ScratchBird

# View logs
Get-Content "C:\ProgramData\ScratchBird\log\sb_server.log" -Tail 50 -Wait
```

---

## Firewall Configuration

### Windows Firewall

```powershell
# Allow ScratchBird ports (run as Administrator)
New-NetFirewallRule -DisplayName "ScratchBird Native" -Direction Inbound -LocalPort 3092 -Protocol TCP -Action Allow
New-NetFirewallRule -DisplayName "ScratchBird PostgreSQL" -Direction Inbound -LocalPort 5432 -Protocol TCP -Action Allow
New-NetFirewallRule -DisplayName "ScratchBird MySQL" -Direction Inbound -LocalPort 3306 -Protocol TCP -Action Allow
New-NetFirewallRule -DisplayName "ScratchBird Firebird" -Direction Inbound -LocalPort 3050 -Protocol TCP -Action Allow

# View rules
Get-NetFirewallRule -DisplayName "ScratchBird*"
```

---

## Connecting to ScratchBird

### Using sb_isql

```powershell
# Connect to local server
sb_isql -H localhost -p 3092 -U admin -d scratchbird

# With password prompt
sb_isql -H localhost -p 3092 -U admin -d scratchbird -W
```

### Using psql (PostgreSQL Client)

If you have PostgreSQL tools installed:

```powershell
psql -h localhost -p 5432 -U admin -d scratchbird
```

### Using GUI Tools

ScratchBird is compatible with:

- **DBeaver** - Connect using PostgreSQL driver to port 5432
- **HeidiSQL** - Connect using MySQL driver to port 3306
- **DataGrip** - Connect using PostgreSQL or MySQL driver

Connection settings:
- Host: `localhost`
- Port: `5432` (PostgreSQL) or `3306` (MySQL) or `3092` (Native)
- Username: `admin`
- Database: `scratchbird`

---

## Troubleshooting

### Server Won't Start

```powershell
# Check if ports are in use
netstat -an | findstr "3092 5432 3306 3050"

# Check configuration
sb_server --config C:\ProgramData\ScratchBird\sb_server.conf --check

# Run in foreground for debugging
sb_server -F --config C:\ProgramData\ScratchBird\sb_server.conf
```

### Connection Refused

```powershell
# Check if server is running
Get-Process sb_server -ErrorAction SilentlyContinue

# Check listening ports
netstat -an | findstr LISTENING | findstr "3092 5432"

# Check firewall
Get-NetFirewallRule -DisplayName "ScratchBird*" | Format-Table Name, Enabled, Action
```

### Build Errors

```powershell
# Check Visual Studio installation
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"

# Verify vcpkg packages
C:\vcpkg\vcpkg list

# Clean rebuild
Remove-Item -Recurse -Force build
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
```

### DLL Not Found Errors

```powershell
# Copy required DLLs to bin directory
Copy-Item C:\vcpkg\installed\x64-windows\bin\*.dll "C:\Program Files\ScratchBird\bin\"

# Or add vcpkg bin to PATH
$env:Path += ";C:\vcpkg\installed\x64-windows\bin"
```

---

## Uninstallation

### Remove Service

```powershell
# Stop and remove service
Stop-Service ScratchBird -Force
C:\Tools\nssm-2.24\win64\nssm.exe remove ScratchBird confirm
```

### Remove Files

```powershell
# Remove installation
Remove-Item -Recurse -Force "C:\Program Files\ScratchBird"

# Remove data (CAUTION: removes all databases)
Remove-Item -Recurse -Force "C:\ProgramData\ScratchBird"

# Remove firewall rules
Remove-NetFirewallRule -DisplayName "ScratchBird*"
```

---

## Next Steps

- [First Connection](../getting-started/first-connection.md) - Connect and run your first query
- [Basic SQL](../getting-started/basic-sql.md) - Learn ScratchBird SQL basics
- [Configuration Reference](../reference/Configuration.md) - Full configuration options

