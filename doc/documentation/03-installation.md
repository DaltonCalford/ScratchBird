# ScratchBird Installation Guide 🟢

This comprehensive guide walks you through installing ScratchBird database system on various platforms. ScratchBird provides multiple installation methods to suit different environments and use cases.

## 🎯 System Requirements

### **Minimum Requirements**
- **Operating System**: Linux (any modern distribution), Windows 10+, macOS 10.14+
- **RAM**: 512 MB (1 GB recommended for production)
- **Disk Space**: 100 MB for installation + database storage needs
- **CPU**: Any modern 64-bit processor
- **Network**: Not required for local installations

### **Recommended Requirements**
- **RAM**: 4 GB or more for better caching performance
- **Disk**: SSD storage for optimal I/O performance
- **CPU**: Multi-core processor for parallel operations
- **Network**: Gigabit Ethernet for multi-user environments

### **Platform Support**
| Platform | Architecture | Status | Notes |
|----------|--------------|--------|-------|
| **Linux** | x86_64 | ✅ Fully Supported | All major distributions |
| **Linux** | ARM64 | ✅ Fully Supported | Including Raspberry Pi 4+ |
| **Windows** | x86_64 | ✅ Fully Supported | Windows 10, 11, Server 2016+ |
| **macOS** | x86_64 | ✅ Fully Supported | Intel Macs |
| **macOS** | ARM64 | ✅ Fully Supported | Apple Silicon (M1/M2) |
| **FreeBSD** | x86_64 | ✅ Supported | Version 12+ |

---

## 📦 Installation Methods

### **Method 1: Package Manager Installation** (Recommended)

#### **Ubuntu/Debian**
```bash
# Add ScratchBird repository
curl -fsSL https://packages.scratchbird.org/gpg.key | sudo gpg --dearmor -o /usr/share/keyrings/scratchbird.gpg
echo "deb [signed-by=/usr/share/keyrings/scratchbird.gpg] https://packages.scratchbird.org/debian stable main" | sudo tee /etc/apt/sources.list.d/scratchbird.list

# Update package index
sudo apt update

# Install ScratchBird
sudo apt install scratchbird

# Start ScratchBird service
sudo systemctl enable scratchbird-guardian
sudo systemctl start scratchbird-guardian
```

#### **CentOS/RHEL/Rocky Linux**
```bash
# Add ScratchBird repository
sudo rpm --import https://packages.scratchbird.org/gpg.key
sudo tee /etc/yum.repos.d/scratchbird.repo << EOF
[scratchbird]
name=ScratchBird Database
baseurl=https://packages.scratchbird.org/rpm/stable
enabled=1
gpgcheck=1
gpgkey=https://packages.scratchbird.org/gpg.key
EOF

# Install ScratchBird
sudo dnf install scratchbird  # or 'yum install scratchbird' on older systems

# Start ScratchBird service
sudo systemctl enable scratchbird-guardian
sudo systemctl start scratchbird-guardian
```

#### **Arch Linux**
```bash
# Install from AUR
yay -S scratchbird

# Or using makepkg
git clone https://aur.archlinux.org/scratchbird.git
cd scratchbird
makepkg -si

# Start service
sudo systemctl enable scratchbird-guardian
sudo systemctl start scratchbird-guardian
```

### **Method 2: Binary Installation**

#### **Download and Install**
```bash
# Download latest release
curl -L https://github.com/dcalford/ScratchBird/releases/latest/download/scratchbird-v0.5.0-linux-x86_64.tar.gz -o scratchbird.tar.gz

# Extract to system directory
sudo tar -xzf scratchbird.tar.gz -C /opt/

# Create symlinks for global access
sudo ln -sf /opt/scratchbird/bin/* /usr/local/bin/

# Create service user
sudo useradd -r -s /bin/false -d /opt/scratchbird scratchbird

# Set ownership
sudo chown -R scratchbird:scratchbird /opt/scratchbird

# Install systemd service
sudo cp /opt/scratchbird/share/systemd/scratchbird-guardian.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable scratchbird-guardian
sudo systemctl start scratchbird-guardian
```

#### **User-Space Installation**
```bash
# Download to user directory
curl -L https://github.com/dcalford/ScratchBird/releases/latest/download/scratchbird-v0.5.0-linux-x86_64.tar.gz -o scratchbird.tar.gz

# Extract to home directory
tar -xzf scratchbird.tar.gz -C $HOME/

# Add to PATH in ~/.bashrc
echo 'export PATH="$HOME/scratchbird/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Verify installation
sb_isql -z
```

### **Method 3: Windows Installation**

#### **Windows Installer**
1. Download `ScratchBird-v0.5.0-Windows-x64.msi` from [releases page](https://github.com/dcalford/ScratchBird/releases)
2. Run installer as Administrator
3. Follow installation wizard
4. Choose installation directory (default: `C:\Program Files\ScratchBird`)
5. Select components to install:
   - ✅ **Core Database Engine** (required)
   - ✅ **Enhanced Utilities** (recommended)
   - ✅ **Development Tools** (optional)
   - ✅ **Documentation** (optional)
6. Configure Windows service options
7. Complete installation

#### **Windows Manual Installation**
```powershell
# Download and extract (PowerShell)
Invoke-WebRequest -Uri "https://github.com/dcalford/ScratchBird/releases/latest/download/scratchbird-v0.5.0-windows-x64.zip" -OutFile "scratchbird.zip"
Expand-Archive -Path "scratchbird.zip" -DestinationPath "C:\Program Files\ScratchBird"

# Add to PATH
$env:PATH += ";C:\Program Files\ScratchBird\bin"
[Environment]::SetEnvironmentVariable("PATH", $env:PATH, "Machine")

# Install Windows service
New-Service -Name "ScratchBirdGuardian" -BinaryPathName "C:\Program Files\ScratchBird\bin\sb_guard.exe -service" -StartupType Automatic
Start-Service -Name "ScratchBirdGuardian"
```

### **Method 4: macOS Installation**

#### **Homebrew Installation**
```bash
# Add ScratchBird tap
brew tap dcalford/scratchbird

# Install ScratchBird
brew install scratchbird

# Start service
brew services start scratchbird
```

#### **Manual macOS Installation**
```bash
# Download macOS package
curl -L https://github.com/dcalford/ScratchBird/releases/latest/download/scratchbird-v0.5.0-macos-universal.dmg -o scratchbird.dmg

# Mount and install
hdiutil attach scratchbird.dmg
sudo installer -pkg "/Volumes/ScratchBird/ScratchBird.pkg" -target /

# Or extract tarball
curl -L https://github.com/dcalford/ScratchBird/releases/latest/download/scratchbird-v0.5.0-macos-universal.tar.gz -o scratchbird.tar.gz
tar -xzf scratchbird.tar.gz -C /usr/local/

# Start launch daemon
sudo launchctl load /Library/LaunchDaemons/org.scratchbird.guardian.plist
```

---

## 🔧 Post-Installation Configuration

### **Verify Installation**
```bash
# Check version
sb_isql -z

# Expected output:
# sb_isql version SB-T0.5.0.1 ScratchBird 0.5 f90eae0

# Check all utilities
sb_gbak -z
sb_gstat -z
sb_gfix -z
sb_gsec -z

# Check service status
sudo systemctl status scratchbird-guardian  # Linux
brew services list | grep scratchbird       # macOS
Get-Service ScratchBirdGuardian             # Windows
```

### **Initial Configuration**
```bash
# Create configuration directory
sudo mkdir -p /etc/scratchbird
sudo chown scratchbird:scratchbird /etc/scratchbird

# Copy default configuration
sudo cp /opt/scratchbird/etc/scratchbird.conf /etc/scratchbird/
sudo cp /opt/scratchbird/etc/sbintl.conf /etc/scratchbird/

# Edit configuration file
sudo nano /etc/scratchbird/scratchbird.conf
```

### **Configuration File Example**
```ini
# /etc/scratchbird/scratchbird.conf
# ScratchBird Database Configuration

#======================================
# Database Configuration
#======================================

# Database cache size (in database pages)
DefaultDbCachePages = 10000

# Maximum memory for sorting operations (bytes)
TempCacheLimit = 268435456

# Lock memory size (bytes)
LockMemSize = 2097152

# Maximum concurrent connections
MaxUserConnections = 200

#======================================
# Schema Configuration
#======================================

# Maximum schema nesting depth (1-8)
MaxSchemaDepth = 8

# Maximum schema path length (63-511)
MaxSchemaPathLength = 511

# Enable schema path caching
EnableSchemaCache = 1

#======================================
# Network Configuration
#======================================

# Remote protocol configuration
RemoteServicePort = 3050
RemoteServiceName = scratchbird_db

# Enable wire compression
WireCompression = true

# Connection timeout (seconds)
ConnectionTimeout = 60

#======================================
# Security Configuration
#======================================

# Security database location
SecurityDatabase = /var/lib/scratchbird/security4.fdb

# Authentication method
AuthMethod = Srp256

# Enable SSL/TLS
WireCrypt = Enabled

#======================================
# Performance Tuning
#======================================

# Garbage collection policy
GCPolicy = cooperative

# Enable CPU affinity
CpuAffinityMask = 0

# Parallel workers for maintenance operations
MaxParallelWorkers = 4

#======================================
# Logging Configuration
#======================================

# Database event logging
LogFileSize = 10485760

# Enable audit trail
AuditTrailLog = /var/log/scratchbird/audit.log

# Debug level (0-4)
DebugLevel = 1
```

### **Security Database Setup**
```bash
# Create security database
sudo sb_gsec -database /var/lib/scratchbird/security4.fdb -add SYSDBA -password masterkey

# Set proper permissions
sudo chown scratchbird:scratchbird /var/lib/scratchbird/security4.fdb
sudo chmod 660 /var/lib/scratchbird/security4.fdb

# Create additional users
sudo sb_gsec -add database_admin -password admin123 -fname "Database" -lname "Administrator"
sudo sb_gsec -add app_user -password app123 -fname "Application" -lname "User"
```

---

## 🚀 Creating Your First Database

### **Method 1: Using sb_isql**
```bash
# Connect and create database
sb_isql -user SYSDBA -password masterkey

# In sb_isql prompt:
SQL> CREATE DATABASE '/var/lib/scratchbird/myapp.fdb'
CON> USER 'SYSDBA' PASSWORD 'masterkey'
CON> PAGE_SIZE 16384
CON> DEFAULT CHARACTER SET UTF8;

SQL> CONNECT '/var/lib/scratchbird/myapp.fdb' USER 'SYSDBA' PASSWORD 'masterkey';

# Create your first hierarchical schema
SQL> CREATE SCHEMA myapp;
SQL> CREATE SCHEMA myapp.users;
SQL> CREATE SCHEMA myapp.content;

# Create a table
SQL> SET SCHEMA 'myapp.users';
SQL> CREATE TABLE profiles (
CON>     user_id INTEGER PRIMARY KEY,
CON>     username VARCHAR(50) UNIQUE NOT NULL,
CON>     email VARCHAR(100) UNIQUE NOT NULL,
CON>     created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
CON> );

SQL> INSERT INTO profiles (user_id, username, email) 
CON> VALUES (1, 'admin', 'admin@example.com');

SQL> SELECT * FROM profiles;

SQL> QUIT;
```

### **Method 2: Using API**
```cpp
// example.cpp - Create database using ScratchBird API
#include "sb_database.h"
#include <iostream>

int main() {
    try {
        SBDatabase db;
        
        // Create and connect to database
        if (db.connect("/var/lib/scratchbird/myapp.fdb", "SYSDBA", "masterkey")) {
            std::cout << "Connected to database successfully!" << std::endl;
            
            // Create schema hierarchy
            db.executeQuery("CREATE SCHEMA myapp");
            db.executeQuery("CREATE SCHEMA myapp.users");
            
            // Create table
            db.executeQuery(R"(
                CREATE TABLE myapp.users.profiles (
                    user_id INTEGER PRIMARY KEY,
                    username VARCHAR(50) UNIQUE NOT NULL,
                    email VARCHAR(100) UNIQUE NOT NULL,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            )");
            
            // Insert sample data
            db.executeQuery(R"(
                INSERT INTO myapp.users.profiles (user_id, username, email)
                VALUES (1, 'admin', 'admin@example.com')
            )");
            
            std::cout << "Database setup completed!" << std::endl;
        }
    } catch (const SBException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### **Method 3: Using Script**
```bash
# create_database.sh
#!/bin/bash

DB_PATH="/var/lib/scratchbird/myapp.fdb"
DB_USER="SYSDBA"
DB_PASS="masterkey"

# Create database setup script
cat > setup_database.sql << EOF
CREATE DATABASE '$DB_PATH'
USER '$DB_USER' PASSWORD '$DB_PASS'
PAGE_SIZE 16384
DEFAULT CHARACTER SET UTF8;

CONNECT '$DB_PATH' USER '$DB_USER' PASSWORD '$DB_PASS';

-- Create schema hierarchy
CREATE SCHEMA myapp;
CREATE SCHEMA myapp.users;
CREATE SCHEMA myapp.content;
CREATE SCHEMA myapp.settings;

-- Set working schema
SET SCHEMA 'myapp.users';

-- Create users table
CREATE TABLE profiles (
    user_id INTEGER PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(128) NOT NULL,
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP
);

-- Create user roles table
CREATE TABLE roles (
    role_id INTEGER PRIMARY KEY,
    role_name VARCHAR(30) UNIQUE NOT NULL,
    description VARCHAR(200),
    permissions TEXT
);

-- Create user-role mapping
CREATE TABLE user_roles (
    user_id INTEGER,
    role_id INTEGER,
    assigned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, role_id),
    FOREIGN KEY (user_id) REFERENCES profiles(user_id),
    FOREIGN KEY (role_id) REFERENCES roles(role_id)
);

-- Switch to content schema
SET SCHEMA 'myapp.content';

-- Create content tables
CREATE TABLE articles (
    article_id INTEGER PRIMARY KEY,
    title VARCHAR(200) NOT NULL,
    content TEXT,
    author_id INTEGER,
    status VARCHAR(20) DEFAULT 'DRAFT',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP,
    published_at TIMESTAMP,
    FOREIGN KEY (author_id) REFERENCES myapp.users.profiles(user_id)
);

-- Insert sample data
SET SCHEMA 'myapp.users';

INSERT INTO roles (role_id, role_name, description) VALUES
(1, 'ADMIN', 'System administrator with full access'),
(2, 'EDITOR', 'Content editor with publish permissions'),
(3, 'AUTHOR', 'Content author with write permissions'),
(4, 'READER', 'Read-only access to published content');

INSERT INTO profiles (user_id, username, email, password_hash, first_name, last_name) VALUES
(1, 'admin', 'admin@example.com', 'hashed_password_here', 'System', 'Administrator'),
(2, 'editor', 'editor@example.com', 'hashed_password_here', 'Content', 'Editor'),
(3, 'author', 'author@example.com', 'hashed_password_here', 'Content', 'Author');

INSERT INTO user_roles (user_id, role_id) VALUES
(1, 1),  -- admin -> ADMIN
(2, 2),  -- editor -> EDITOR
(3, 3);  -- author -> AUTHOR

-- Create some sample content
SET SCHEMA 'myapp.content';

INSERT INTO articles (article_id, title, content, author_id, status, published_at) VALUES
(1, 'Welcome to ScratchBird', 'This is your first article using ScratchBird database with hierarchical schemas!', 1, 'PUBLISHED', CURRENT_TIMESTAMP),
(2, 'Getting Started Guide', 'Learn how to use ScratchBird''s powerful features...', 2, 'PUBLISHED', CURRENT_TIMESTAMP),
(3, 'Advanced Schema Features', 'Explore the hierarchical schema system...', 3, 'DRAFT', NULL);

COMMIT;

-- Show database structure
SHOW SCHEMAS;
SET SCHEMA 'myapp.users';
SHOW TABLES;
EOF

# Execute setup script
sb_isql -input setup_database.sql

# Clean up
rm setup_database.sql

echo "Database setup completed successfully!"
echo "Connection string: $DB_PATH"
echo "Default user: $DB_USER"
echo "Test connection:"
echo "sb_isql -user $DB_USER -password $DB_PASS $DB_PATH"
```

---

## 🔐 Security Hardening

### **File Permissions**
```bash
# Set proper ownership and permissions
sudo chown -R scratchbird:scratchbird /opt/scratchbird
sudo chown -R scratchbird:scratchbird /var/lib/scratchbird
sudo chown -R scratchbird:scratchbird /var/log/scratchbird

# Database files
sudo chmod 660 /var/lib/scratchbird/*.fdb
sudo chmod 750 /var/lib/scratchbird

# Configuration files
sudo chmod 640 /etc/scratchbird/*.conf
sudo chmod 750 /etc/scratchbird

# Log files
sudo chmod 640 /var/log/scratchbird/*.log
sudo chmod 750 /var/log/scratchbird
```

### **Firewall Configuration**
```bash
# UFW (Ubuntu/Debian)
sudo ufw allow 3050/tcp comment "ScratchBird Database"
sudo ufw reload

# FirewallD (CentOS/RHEL)
sudo firewall-cmd --permanent --add-port=3050/tcp
sudo firewall-cmd --reload

# iptables
sudo iptables -A INPUT -p tcp --dport 3050 -j ACCEPT
sudo iptables-save > /etc/iptables/rules.v4
```

### **SSL/TLS Configuration**
```bash
# Generate SSL certificates
sudo mkdir -p /etc/scratchbird/ssl
cd /etc/scratchbird/ssl

# Create private key
sudo openssl genrsa -out server.key 2048

# Create certificate signing request
sudo openssl req -new -key server.key -out server.csr

# Create self-signed certificate (for testing)
sudo openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt

# Set permissions
sudo chown scratchbird:scratchbird server.*
sudo chmod 600 server.key
sudo chmod 644 server.crt

# Update configuration
sudo tee -a /etc/scratchbird/scratchbird.conf << EOF

# SSL Configuration
WireCrypt = Required
SSLCertificate = /etc/scratchbird/ssl/server.crt
SSLPrivateKey = /etc/scratchbird/ssl/server.key
EOF

# Restart service
sudo systemctl restart scratchbird-guardian
```

---

## 🚀 Environment-Specific Setup

### **Development Environment**
```bash
# Development-friendly configuration
cat > ~/.scratchbird_dev.conf << EOF
# Development configuration
DefaultDbCachePages = 2000
DebugLevel = 3
LogFileSize = 1048576
MaxUserConnections = 50

# Relaxed security for development
AuthMethod = Legacy_Auth
WireCrypt = Enabled

# Fast restarts
GCPolicy = cooperative
EOF

# Set environment variable
echo 'export SCRATCHBIRD_CONF="$HOME/.scratchbird_dev.conf"' >> ~/.bashrc

# Create development databases directory
mkdir -p ~/scratchbird_databases
```

### **Production Environment**
```bash
# Production tuning
sudo tee /etc/scratchbird/production.conf << EOF
# Production configuration
DefaultDbCachePages = 50000
TempCacheLimit = 1073741824
LockMemSize = 8388608
MaxUserConnections = 500

# Security
AuthMethod = Srp256
WireCrypt = Required
SecurityDatabase = /var/lib/scratchbird/security4.fdb

# Performance
GCPolicy = background
MaxParallelWorkers = 8
CpuAffinityMask = 255

# Monitoring
AuditTrailLog = /var/log/scratchbird/audit.log
LogFileSize = 104857600
DebugLevel = 1
EOF

# Set production configuration
sudo ln -sf /etc/scratchbird/production.conf /etc/scratchbird/scratchbird.conf

# Restart with production settings
sudo systemctl restart scratchbird-guardian
```

### **Container Deployment**
```dockerfile
# Dockerfile for ScratchBird
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    curl \
    gnupg \
    && rm -rf /var/lib/apt/lists/*

# Add ScratchBird repository and install
RUN curl -fsSL https://packages.scratchbird.org/gpg.key | gpg --dearmor -o /usr/share/keyrings/scratchbird.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/scratchbird.gpg] https://packages.scratchbird.org/debian stable main" > /etc/apt/sources.list.d/scratchbird.list \
    && apt-get update \
    && apt-get install -y scratchbird \
    && rm -rf /var/lib/apt/lists/*

# Create directories
RUN mkdir -p /var/lib/scratchbird /var/log/scratchbird /etc/scratchbird

# Copy configuration
COPY scratchbird.conf /etc/scratchbird/

# Set permissions
RUN chown -R scratchbird:scratchbird /var/lib/scratchbird /var/log/scratchbird

# Expose port
EXPOSE 3050

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD sb_isql -execute "SELECT 1 FROM RDB\$DATABASE;" /var/lib/scratchbird/test.fdb || exit 1

# Switch to non-root user
USER scratchbird

# Start ScratchBird
CMD ["/opt/scratchbird/bin/sb_guard", "-daemon", "-config", "/etc/scratchbird/scratchbird.conf"]
```

```yaml
# docker-compose.yml
version: '3.8'

services:
  scratchbird:
    build: .
    ports:
      - "3050:3050"
    volumes:
      - scratchbird_data:/var/lib/scratchbird
      - scratchbird_logs:/var/log/scratchbird
      - ./config:/etc/scratchbird
    environment:
      - SCRATCHBIRD_CONF=/etc/scratchbird/scratchbird.conf
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "sb_isql", "-execute", "SELECT 1 FROM RDB$$DATABASE;", "/var/lib/scratchbird/test.fdb"]
      interval: 30s
      timeout: 10s
      retries: 3

volumes:
  scratchbird_data:
  scratchbird_logs:
```

---

## 🆘 Troubleshooting Installation

### **Common Issues**

**Issue**: "Permission denied" during installation
```bash
# Ensure you have sudo privileges
sudo whoami

# Check if user is in sudo group
groups $USER

# Add user to sudo group if needed
sudo usermod -aG sudo $USER
```

**Issue**: Service fails to start
```bash
# Check service status
sudo systemctl status scratchbird-guardian

# Check logs
sudo journalctl -u scratchbird-guardian -f

# Verify configuration
sudo sb_guard -config /etc/scratchbird/scratchbird.conf -validate

# Check file permissions
ls -la /etc/scratchbird/
ls -la /var/lib/scratchbird/
```

**Issue**: Cannot connect to database
```bash
# Test local connection
sb_isql -user SYSDBA -password masterkey

# Check if service is listening
sudo netstat -tlnp | grep 3050

# Test network connection
telnet localhost 3050

# Check firewall
sudo ufw status  # or sudo firewall-cmd --list-all
```

**Issue**: Database creation fails
```bash
# Check disk space
df -h /var/lib/scratchbird

# Check directory permissions
ls -la /var/lib/scratchbird

# Test with full path
sb_isql -user SYSDBA -password masterkey
SQL> CREATE DATABASE '/var/lib/scratchbird/test.fdb' USER 'SYSDBA' PASSWORD 'masterkey';
```

### **Uninstallation**
```bash
# Stop services
sudo systemctl stop scratchbird-guardian
sudo systemctl disable scratchbird-guardian

# Remove packages
sudo apt remove --purge scratchbird  # Debian/Ubuntu
sudo dnf remove scratchbird          # CentOS/RHEL

# Remove data (WARNING: This deletes all databases!)
sudo rm -rf /var/lib/scratchbird
sudo rm -rf /etc/scratchbird
sudo rm -rf /var/log/scratchbird

# Remove user
sudo userdel scratchbird
```

---

## 🎯 Next Steps

- **[Quick Start Guide](02-quick-start.md)** - Get started with your first database
- **[Configuration Guide](04-configuration.md)** - Detailed configuration options
- **[Database Engine](05-database-engine.md)** - Understanding ScratchBird architecture
- **[Security Guide](08-security.md)** - Secure your installation

## 📚 Related Documentation

- **[Utilities Overview](09-utilities-overview.md)** - Learn about ScratchBird tools
- **[API Reference](17-api-reference.md)** - Programming with ScratchBird
- **[Troubleshooting](25-troubleshooting.md)** - Comprehensive troubleshooting guide

---

## 💡 Installation Tips

> **Choose the Right Method**: Package managers are recommended for production, binary installs for development
> ```bash
> # Production: Use package manager
> sudo apt install scratchbird
> 
> # Development: Use binary install
> tar -xzf scratchbird.tar.gz -C ~/
> ```

> **Verify Installation**: Always test your installation before using in production
> ```bash
> # Quick verification test
> sb_isql -z && echo "Installation successful!"
> ```

> **Secure by Default**: Configure security settings during initial setup
> ```bash
> # Change default password immediately
> sb_gsec -modify SYSDBA -password new_secure_password
> ```

**🚀 Ready to install ScratchBird?** Choose your preferred installation method and follow the platform-specific instructions above!