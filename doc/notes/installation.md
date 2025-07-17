# ScratchBird Installation Guide

## System Requirements

### Linux x86_64
- **OS**: Ubuntu 18.04+, CentOS 7+, Debian 9+, or compatible Linux distribution
- **Architecture**: x86_64 (64-bit Intel/AMD)
- **Memory**: 512 MB RAM minimum, 2 GB recommended
- **Disk Space**: 100 MB for installation, additional space for databases
- **Dependencies**: glibc 2.17+, readline library (for sb_isql)

### Windows x64
- **OS**: Windows 10, Windows 11, Windows Server 2016+
- **Architecture**: x64 (64-bit Intel/AMD)
- **Memory**: 512 MB RAM minimum, 2 GB recommended
- **Disk Space**: 100 MB for installation, additional space for databases
- **Dependencies**: None (statically linked)

## Installation Methods

### Option 1: Release Package Installation

**Linux x86_64**
```bash
# Download release package
wget https://github.com/dcalford/ScratchBird/releases/download/v0.5.0/scratchbird-v0.5.0-linux-x86_64.tar.gz

# Extract package
tar -xzf scratchbird-v0.5.0-linux-x86_64.tar.gz
cd scratchbird-v0.5.0-linux-x86_64

# Install (requires root privileges)
sudo ./install.sh

# Verify installation
sb_isql -z
```

**Windows x64**
```bash
# Download from GitHub releases page:
# https://github.com/dcalford/ScratchBird/releases/download/v0.5.0/scratchbird-v0.5.0-windows-x64.zip

# Extract ZIP file to desired location
# Run install.bat as administrator
# Add bin/ directory to PATH environment variable

# Verify installation
sb_isql.exe -z
```

### Option 2: Build from Source

See [Build Instructions](build-instructions.md) for complete compilation guide.

## Post-Installation Configuration

### Linux Service Setup (Optional)
```bash
# Create service user
sudo useradd -r -s /bin/false scratchbird

# Create service file
sudo tee /etc/systemd/system/scratchbird.service > /dev/null <<EOF
[Unit]
Description=ScratchBird Database Server
After=network.target

[Service]
Type=forking
User=scratchbird
Group=scratchbird
ExecStart=/usr/local/bin/sb_server -d
ExecReload=/bin/kill -HUP \$MAINPID
KillMode=mixed
KillSignal=SIGINT
TimeoutSec=300

[Install]
WantedBy=multi-user.target
EOF

# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable scratchbird
sudo systemctl start scratchbird
```

### Windows Service Setup (Optional)
```batch
REM Run as administrator
cd C:\Program Files\ScratchBird\bin
sb_server.exe -install
net start ScratchBird
```

## Environment Configuration

### Linux Environment Variables
```bash
# Add to ~/.bashrc or ~/.profile
export SCRATCHBIRD_HOME=/usr/local/scratchbird
export PATH=$PATH:/usr/local/scratchbird/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/scratchbird/lib
```

### Windows Environment Variables
```batch
REM Add to system PATH
set PATH=%PATH%;C:\Program Files\ScratchBird\bin
set SCRATCHBIRD_HOME=C:\Program Files\ScratchBird
```

## Verification Steps

### Basic Functionality Test
```bash
# Check version
sb_isql -z

# Expected output:
# sb_isql version SB-T0.5.0.1 ScratchBird 0.5 f90eae0

# Test database creation
sb_isql
SQL> CREATE DATABASE '/tmp/test.fdb';
SQL> CONNECT '/tmp/test.fdb';
SQL> SELECT 'Hello ScratchBird!' FROM RDB$DATABASE;
SQL> EXIT;
```

### Advanced Features Test
```sql
-- Connect to test database
sb_isql /tmp/test.fdb

-- Test hierarchical schemas
CREATE SCHEMA test;
CREATE SCHEMA test.demo;
CREATE TABLE test.demo.sample (id INTEGER, name VARCHAR(50));
INSERT INTO test.demo.sample VALUES (1, 'Test Data');
SELECT * FROM test.demo.sample;

-- Test schema navigation
SET SCHEMA 'test';
SELECT * FROM demo.sample;
```

## Troubleshooting

### Common Installation Issues

**Issue**: `sb_isql: command not found`
**Solution**: 
```bash
# Check PATH
echo $PATH

# Add ScratchBird bin directory
export PATH=$PATH:/usr/local/scratchbird/bin

# Make permanent
echo 'export PATH=$PATH:/usr/local/scratchbird/bin' >> ~/.bashrc
```

**Issue**: `error while loading shared libraries: libsbclient.so.2`
**Solution**:
```bash
# Add library path
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/scratchbird/lib

# Or create symlink
sudo ln -s /usr/local/scratchbird/lib/libsbclient.so.2 /usr/lib/x86_64-linux-gnu/
```

**Issue**: Windows "DLL not found" error
**Solution**:
- Ensure all files are in the same directory
- Run from the installation directory
- Check Windows PATH environment variable

### Database Connection Issues

**Issue**: Cannot connect to database
**Solution**:
```bash
# Check if database file exists
ls -la /path/to/database.fdb

# Verify file permissions
chmod 664 /path/to/database.fdb
chown user:group /path/to/database.fdb

# Test with full path
sb_isql /full/path/to/database.fdb
```

**Issue**: "Database unavailable" error
**Solution**:
```bash
# Check database integrity
sb_gfix -v -full /path/to/database.fdb

# Repair if needed
sb_gfix -mend /path/to/database.fdb
```

## Security Considerations

### File Permissions
```bash
# Set appropriate permissions (Linux)
chmod 750 /usr/local/scratchbird/bin/*
chmod 644 /usr/local/scratchbird/lib/*
chmod 600 /path/to/database/*.fdb
```

### Network Security
- ScratchBird uses default port 4050 (not 3050 like Firebird)
- Configure firewall rules if needed
- Use authentication plugins for production

### Database Security
```sql
-- Create database users
CREATE USER 'appuser' PASSWORD 'strong_password';
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES TO 'appuser';

-- Use role-based security
CREATE ROLE 'app_role';
GRANT app_role TO 'appuser';
```

## Uninstallation

### Linux Uninstall
```bash
# Stop service if running
sudo systemctl stop scratchbird
sudo systemctl disable scratchbird

# Remove service file
sudo rm /etc/systemd/system/scratchbird.service

# Remove installation
sudo rm -rf /usr/local/scratchbird
sudo rm /usr/local/bin/sb_*

# Remove user
sudo userdel scratchbird
```

### Windows Uninstall
```batch
REM Stop service if running
net stop ScratchBird
sc delete ScratchBird

REM Remove installation directory
rmdir /s "C:\Program Files\ScratchBird"

REM Remove from PATH (manual or via System Properties)
```

## Migration from Firebird

### Database Compatibility
- ScratchBird can directly use Firebird .fdb files
- No database conversion required
- Backup/restore recommended for production migration

### Application Changes
```bash
# Update connection strings
# From: localhost:3050/database.fdb
# To:   localhost:4050/database.fdb

# Update tool names
# From: isql, gbak, gfix, gsec, gstat
# To:   sb_isql, sb_gbak, sb_gfix, sb_gsec, sb_gstat
```

### Configuration Migration
```bash
# Copy Firebird configuration
cp /opt/firebird/firebird.conf /usr/local/scratchbird/conf/scratchbird.conf

# Update port setting
sed -i 's/RemoteServicePort = 3050/RemoteServicePort = 4050/' scratchbird.conf
```

## Next Steps

After successful installation:
1. Read [Quick Start Guide](quick-start.md) for basic usage
2. Explore [Core Features](core-features.md) for advanced capabilities
3. Review [Build Instructions](build-instructions.md) for development setup