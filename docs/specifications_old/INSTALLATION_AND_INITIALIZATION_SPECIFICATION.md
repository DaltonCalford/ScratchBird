# Installation and Initialization Specification

**Version:** 1.0  
**Status:** Authoritative (V3)
**Last Updated:** February 2026  

## 1. Purpose

Define the complete installation and initialization process for ScratchBird server, including:
- Package installation and directory creation
- TLS certificate generation and management
- Configuration file initialization
- Database registry creation
- Security database setup
- First-run configuration wizard

## 2. Installation Methods

### 2.1 Linux Package Installation (DEB/RPM)

#### Pre-Installation Checks

```bash
#!/bin/bash
# preinst script

# Check for required system resources
MIN_RAM_MB=512
MIN_DISK_MB=1024

AVAILABLE_RAM=$(free -m | awk 'NR==2{print $7}')
AVAILABLE_DISK=$(df -m /var/lib | awk 'NR==2{print $4}')

if [ "$AVAILABLE_RAM" -lt "$MIN_RAM_MB" ]; then
    echo "WARNING: Less than ${MIN_RAM_MB}MB RAM available"
fi

if [ "$AVAILABLE_DISK" -lt "$MIN_DISK_MB" ]; then
    echo "ERROR: Less than ${MIN_DISK_MB}MB disk space available"
    exit 1
fi

# Check for conflicting packages
if dpkg -l | grep -q "^ii.*firebird"; then
    echo "WARNING: Firebird detected - ensure port 3050 is not conflicting"
fi

# Check for existing installation
if [ -f /etc/scratchbird/sb_server.conf ]; then
    echo "Existing configuration found - will preserve during upgrade"
fi
```

#### Directory Creation (postinst)

```bash
#!/bin/bash
# postinst script

set -e

SB_USER="scratchbird"
SB_GROUP="scratchbird"

# Create system user (if not exists)
if ! id "$SB_USER" &>/dev/null; then
    useradd --system --home-dir /var/lib/scratchbird \
            --shell /bin/false --group "$SB_GROUP" "$SB_USER"
fi

# Create directory structure
DIRS=(
    "/etc/scratchbird"
    "/var/lib/scratchbird/databases"
    "/var/lib/scratchbird/run"
    "/var/lib/scratchbird/cache"
    "/var/log/scratchbird/audit"
    "/usr/share/scratchbird/resources/languages"
    "/usr/share/scratchbird/resources/timezones"
    "/usr/share/scratchbird/udr"
)

for dir in "${DIRS[@]}"; do
    mkdir -p "$dir"
done

# Set permissions
chown root:root /etc/scratchbird
chmod 755 /etc/scratchbird

chown -R "$SB_USER:$SB_GROUP" /var/lib/scratchbird
chmod 750 /var/lib/scratchbird
chmod 750 /var/lib/scratchbird/databases
chmod 755 /var/lib/scratchbird/run
chmod 750 /var/lib/scratchbird/cache

chown -R "$SB_USER:$SB_GROUP" /var/log/scratchbird
chmod 755 /var/log/scratchbird
chmod 750 /var/log/scratchbird/audit

# Create log files with correct permissions
touch /var/log/scratchbird/server.log
touch /var/log/scratchbird/listener_native.log
touch /var/log/scratchbird/listener_pg.log
chown "$SB_USER:$SB_GROUP" /var/log/scratchbird/*.log
chmod 640 /var/log/scratchbird/*.log

# Set up log rotation
cat > /etc/logrotate.d/scratchbird << 'EOF'
/var/log/scratchbird/*.log {
    daily
    rotate 14
    compress
    delaycompress
    missingok
    notifempty
    create 640 scratchbird scratchbird
    sharedscripts
    postrotate
        /bin/kill -HUP $(cat /var/lib/scratchbird/run/scratchbird.pid 2>/dev/null) 2>/dev/null || true
    endscript
}
EOF

echo "Directory structure created successfully"
```

### 2.2 Windows MSI Installation

#### WiX Component Structure

```xml
<!-- Directory structure -->
<Directory Id="TARGETDIR" Name="SourceDir">
    <Directory Id="ProgramFiles64Folder">
        <Directory Id="INSTALLFOLDER" Name="ScratchBird">
            <Directory Id="BIN" Name="bin" />
            <Directory Id="LIB" Name="lib" />
            <Directory Id="CONFIG" Name="config" />
            <Directory Id="RESOURCES" Name="resources">
                <Directory Id="LANGUAGES" Name="languages" />
                <Directory Id="TIMEZONES" Name="timezones" />
            </Directory>
            <Directory Id="SSL" Name="ssl" />
        </Directory>
    </Directory>
    
    <Directory Id="ProgramDataFolder">
        <Directory Id="DATADIR" Name="ScratchBird">
            <Directory Id="DATA" Name="data" />
            <Directory Id="LOGS" Name="logs" />
            <Directory Id="TEMP" Name="temp" />
            <Directory Id="CACHE" Name="cache" />
        </Directory>
    </Directory>
</Directory>

<!-- Component for TLS certificates -->
<Component Id="SSLCertificates" Directory="SSL" Guid="*">
    <Condition>NOT SKIP_CERTIFICATE_GENERATION</Condition>
    <CreateFolder />
    <CustomActionRef Id="GenerateCertificates" />
</Component>
```

## 3. TLS Certificate Generation

### 3.1 Automatic Self-Signed Certificate

```bash
#!/bin/bash
# generate_server_cert.sh

CERT_DIR="${1:-/etc/scratchbird/ssl}"
CERT_DAYS=365
KEY_BITS=4096

# Ensure directory exists
mkdir -p "$CERT_DIR"

# Generate CA key and certificate (for internal use)
if [ ! -f "$CERT_DIR/ca.key" ]; then
    openssl genrsa -out "$CERT_DIR/ca.key" $KEY_BITS
    openssl req -new -x509 -days $CERT_DAYS \
        -key "$CERT_DIR/ca.key" \
        -out "$CERT_DIR/ca.crt" \
        -subj "/CN=ScratchBird Internal CA/O=ScratchBird Server"
    chmod 600 "$CERT_DIR/ca.key"
    chmod 644 "$CERT_DIR/ca.crt"
fi

# Generate server private key
openssl genrsa -out "$CERT_DIR/server.key" $KEY_BITS
chmod 600 "$CERT_DIR/server.key"

# Get hostname
HOSTNAME=$(hostname -f 2>/dev/null || hostname)
IP_ADDRESSES=$(hostname -I 2>/dev/null | tr ' ' ',' | sed 's/,$//')

# Create config for SAN (Subject Alternative Names)
cat > "$CERT_DIR/server.cnf" << EOF
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no

[req_distinguished_name]
CN = $HOSTNAME
O = ScratchBird Server
OU = Database Server

[v3_req]
keyUsage = keyEncipherment, dataEncipherment, digitalSignature
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = localhost
DNS.2 = $HOSTNAME
IP.1 = 127.0.0.1
EOF

# Add additional IP addresses if available
IP_NUM=2
for IP in $(hostname -I 2>/dev/null); do
    echo "IP.$IP_NUM = $IP" >> "$CERT_DIR/server.cnf"
    IP_NUM=$((IP_NUM + 1))
done

# Generate CSR and certificate
openssl req -new \
    -key "$CERT_DIR/server.key" \
    -out "$CERT_DIR/server.csr" \
    -config "$CERT_DIR/server.cnf"

openssl x509 -req -days $CERT_DAYS \
    -in "$CERT_DIR/server.csr" \
    -CA "$CERT_DIR/ca.crt" \
    -CAkey "$CERT_DIR/ca.key" \
    -CAcreateserial \
    -out "$CERT_DIR/server.crt" \
    -extensions v3_req \
    -extfile "$CERT_DIR/server.cnf"

# Clean up
rm "$CERT_DIR/server.csr" "$CERT_DIR/server.cnf"

# Set ownership
if id -u scratchbird &>/dev/null; then
    chown scratchbird:scratchbird "$CERT_DIR/server.key" "$CERT_DIR/server.crt"
fi

echo "Certificate generated successfully:"
echo "  Server cert: $CERT_DIR/server.crt"
echo "  Server key:  $CERT_DIR/server.key"
echo "  CA cert:     $CERT_DIR/ca.crt"
```

### 3.2 Certificate Management Tool (sb_security)

```bash
# Generate new certificate
sb_security certificate generate --output-dir /etc/scratchbird/ssl

# Show certificate info
sb_security certificate info /etc/scratchbird/ssl/server.crt

# Renew certificate
sb_security certificate renew --cert /etc/scratchbird/ssl/server.crt

# Create client certificate
sb_security certificate client --username john_doe --output john_doe.p12
```

## 4. Initial Configuration

### 4.1 Default Configuration Template

```ini
; /etc/scratchbird/sb_server.conf - Default Template
; Generated during installation
; Modify as needed for your environment

;==============================================================================
; SERVER SECTION
;==============================================================================
[server]
; Server instance name (for multi-instance setups)
instance_name = default

; Data directory for databases and runtime files
data_directory = /var/lib/scratchbird

; Database registry location
registry_path = /var/lib/scratchbird/registry.sb

; PID file location
pid_file = /var/lib/scratchbird/run/scratchbird.pid

; Default database (used if client doesn't specify)
default_database = 

; Resource directories (can be overridden per database)
resource_directory = /usr/share/scratchbird/resources
udr_directory = /usr/share/scratchbird/udr

;==============================================================================
; LOGGING SECTION
;==============================================================================
[logging]
; Log level: debug, info, warning, error, fatal
level = info

; Log destination: file, syslog, stdout, stderr
destination = file
file = /var/log/scratchbird/server.log

; Log format: text, json
format = text

; Audit logging
audit_enabled = true
audit_directory = /var/log/scratchbird/audit

; Slow query logging (milliseconds, 0 = disabled)
slow_query_threshold = 1000

;==============================================================================
; NETWORK SECTION
;==============================================================================
[network]
; Bind address (0.0.0.0 for all interfaces, 127.0.0.1 for localhost)
bind_address = 127.0.0.1

; ScratchBird native protocol port (0 to disable)
native_port = 3092
native_pool_min = 4
native_pool_max = 64

; PostgreSQL protocol port (0 to disable)
postgresql_port = 5432
postgresql_pool_min = 8
postgresql_pool_max = 128

; MySQL protocol port (0 to disable)
mysql_port = 3306
mysql_pool_min = 8
mysql_pool_max = 128

; Firebird protocol port (0 to disable)
firebird_port = 3050
firebird_pool_min = 4
firebird_pool_max = 64

; Unix domain socket (empty to disable)
unix_socket = /var/run/scratchbird/sb.sock
unix_socket_permissions = 0770
unix_socket_group = scratchbird

; Connection limits
max_connections = 200
max_connections_per_database = 100

; Timeouts (seconds)
connect_timeout = 30
idle_timeout = 3600

;==============================================================================
; SSL/TLS SECTION
;==============================================================================
[ssl]
; Enable SSL/TLS
enabled = true

; Certificate files
cert_file = /etc/scratchbird/ssl/server.crt
key_file = /etc/scratchbird/ssl/server.key

; Certificate authority (for client certificate verification)
ca_file = /etc/scratchbird/ssl/ca.crt

; TLS versions
min_protocol = TLSv1.2
preferred_protocol = TLSv1.3

; Cipher suites
ciphers = TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256

; Client certificate verification: none, optional, require
verify_mode = none

;==============================================================================
; AUTHENTICATION SECTION
;==============================================================================
[authentication]
; Default authentication method: local, ldap, kerberos
default_method = local

; Security database location (for shared security)
; If empty, uses database-local security
security_database = /var/lib/scratchbird/security.db

; Allow trust authentication for local connections
allow_local_trust = false

; Password policy
password_min_length = 8
password_require_complexity = true
password_expire_days = 0  ; 0 = never

; Failed login handling
max_failed_logins = 5
lockout_duration_minutes = 30

;==============================================================================
; MEMORY SECTION
;==============================================================================
[memory]
; Buffer pool size
buffer_pool_size = 128MB

; Page size for new databases
page_size = 8192

; Temporary memory
sort_buffer_size = 8MB
join_buffer_size = 8MB

;==============================================================================
; REGISTRY SECTION
;==============================================================================
[registry]
; Registry database path
path = /var/lib/scratchbird/registry.sb

; Auto-create registry if not exists
auto_create = true

; Backup settings
backup_enabled = true
backup_directory = /var/lib/scratchbird/backups
backup_retention_days = 30
```

### 4.2 Configuration Validation

```cpp
class ConfigValidator {
public:
    ValidationResult validate(const ServerConfig& config) {
        ValidationResult result;
        
        // Check paths exist and are accessible
        if (!directoryExists(config.data_directory)) {
            result.addError("data_directory", "Directory does not exist");
        }
        
        if (!directoryWritable(config.data_directory)) {
            result.addError("data_directory", "Directory not writable by server user");
        }
        
        // Check TLS configuration
        if (config.ssl_enabled) {
            if (!fileExists(config.ssl_cert_file)) {
                result.addError("ssl.cert_file", "Certificate file not found");
            }
            if (!fileExists(config.ssl_key_file)) {
                result.addError("ssl.key_file", "Key file not found");
            }
            if (!validateCertificate(config.ssl_cert_file, config.ssl_key_file)) {
                result.addError("ssl", "Certificate and key do not match");
            }
        }
        
        // Check port availability
        if (config.native_port > 0 && !portAvailable(config.native_port)) {
            result.addWarning("network.native_port", "Port may be in use");
        }
        
        // Check memory settings
        auto available_memory = getAvailableMemory();
        if (config.buffer_pool_size > available_memory / 2) {
            result.addWarning("memory.buffer_pool_size", 
                "Buffer pool exceeds 50% of available RAM");
        }
        
        return result;
    }
};
```

## 5. Database Registry Initialization

### 5.1 Registry Creation

```cpp
bool DatabaseRegistry::initializeNewRegistry(core::ErrorContext* ctx) {
    // Create SQLite database
    sqlite3* db;
    int rc = sqlite3_open(registry_path_.c_str(), &db);
    if (rc != SQLITE_OK) {
        SET_ERROR(ctx, INTERNAL_ERROR, 
            std::string("Cannot create registry: ") + sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }
    
    // Enable WAL mode for better concurrency
    sqlite3_exec(db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    
    // Create schema
    const char* schema_sql = R"(
        -- Tables from DATABASE_REGISTRY_SPECIFICATION.md
        CREATE TABLE registered_databases (...);
        CREATE TABLE database_aliases (...);
        CREATE TABLE database_permissions (...);
        CREATE TABLE database_statistics (...);
        CREATE TABLE registry_metadata (...);
        
        -- Insert version info
        INSERT INTO registry_metadata (key, value, description) VALUES
        ('schema_version', '1.0', 'Registry schema version'),
        ('registry_created_at', datetime('now'), 'Creation timestamp'),
        ('server_instance', 'default', 'Instance name');
    )";
    
    char* err_msg = nullptr;
    rc = sqlite3_exec(db, schema_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        SET_ERROR(ctx, INTERNAL_ERROR, 
            std::string("Schema creation failed: ") + err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return false;
    }
    
    sqlite3_close(db);
    
    LOG_INFO("Database registry initialized: {}", registry_path_);
    return true;
}
```

### 5.2 Security Database Creation

```cpp
bool SecurityDatabase::initialize(const std::string& path, core::ErrorContext* ctx) {
    // Create security database
    auto db = core::Database::create(path, &ctx);
    if (!db) {
        return false;
    }
    
    // Create system tables
    const char* security_schema = R"(
        -- Users table
        CREATE TABLE security_users (
            user_id UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
            user_name VARCHAR(128) NOT NULL UNIQUE,
            password_hash VARCHAR(256),  -- SCRAM-SHA-256 format
            password_salt VARCHAR(64),
            password_iterations INTEGER DEFAULT 4096,
            password_changed_at TIMESTAMP,
            password_expires_at TIMESTAMP,
            
            -- Status
            status VARCHAR(20) DEFAULT 'active',  -- active, disabled, locked
            failed_login_count INTEGER DEFAULT 0,
            locked_until TIMESTAMP,
            
            -- Profile
            first_name VARCHAR(128),
            last_name VARCHAR(128),
            email VARCHAR(256),
            default_schema VARCHAR(128) DEFAULT 'PUBLIC',
            
            -- External auth
            auth_method VARCHAR(20) DEFAULT 'local',  -- local, ldap, kerberos
            external_id VARCHAR(256),  -- LDAP DN or Kerberos principal
            
            -- Audit
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            created_by VARCHAR(128),
            last_login_at TIMESTAMP,
            last_login_from VARCHAR(45)  -- IP address
        );
        
        -- Roles table
        CREATE TABLE security_roles (
            role_id UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
            role_name VARCHAR(128) NOT NULL UNIQUE,
            description TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        
        -- User roles mapping
        CREATE TABLE security_user_roles (
            user_id UUID REFERENCES security_users(user_id) ON DELETE CASCADE,
            role_id UUID REFERENCES security_roles(role_id) ON DELETE CASCADE,
            granted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            granted_by VARCHAR(128),
            PRIMARY KEY (user_id, role_id)
        );
        
        -- Role hierarchy
        CREATE TABLE security_role_hierarchy (
            parent_role_id UUID REFERENCES security_roles(role_id) ON DELETE CASCADE,
            child_role_id UUID REFERENCES security_roles(role_id) ON DELETE CASCADE,
            PRIMARY KEY (parent_role_id, child_role_id)
        );
        
        -- Create default admin user (SYSDBA equivalent)
        INSERT INTO security_users (
            user_name, status, auth_method, 
            first_name, last_name, default_schema
        ) VALUES (
            'SYSDBA', 'active', 'local',
            'System', 'Administrator', 'PUBLIC'
        );
        
        -- Create default roles
        INSERT INTO security_roles (role_name, description) VALUES
        ('DBA', 'Full database administration'),
        ('SECURITY_ADMIN', 'Security administration'),
        ('BACKUP_OPERATOR', 'Can perform backups'),
        ('READONLY', 'Read-only access');
    )";
    
    auto result = db->execute(security_schema);
    if (!result.ok()) {
        SET_ERROR(ctx, INTERNAL_ERROR, "Security schema creation failed");
        return false;
    }
    
    LOG_INFO("Security database initialized: {}", path);
    return true;
}
```

## 6. First-Run Configuration Wizard

### 6.1 Interactive Setup (sb_setup)

```cpp
class SetupWizard {
public:
    int runInteractive() {
        printHeader();
        
        // Step 1: Installation type
        auto install_type = promptChoice(
            "Select installation type:",
            {"Development", "Production", "Embedded"}
        );
        
        // Step 2: Network configuration
        auto bind_address = promptString(
            "Bind address", 
            detectDefaultBindAddress()
        );
        
        auto enable_native = promptYesNo("Enable native protocol (port 3092)?", true);
        auto enable_pg = promptYesNo("Enable PostgreSQL protocol (port 5432)?", true);
        auto enable_mysql = promptYesNo("Enable MySQL protocol (port 3306)?", false);
        
        // Step 3: Security configuration
        auto auth_method = promptChoice(
            "Authentication method:",
            {"Local security database", "LDAP/Active Directory", "Kerberos"}
        );
        
        if (auth_method == 0) {  // Local
            auto admin_password = promptPassword("Set SYSDBA password:");
            auto confirm_password = promptPassword("Confirm password:");
            if (admin_password != confirm_password) {
                std::cerr << "Passwords do not match!" << std::endl;
                return 1;
            }
        }
        
        // Step 4: TLS configuration
        auto tls_choice = promptChoice(
            "TLS certificate:",
            {"Generate self-signed certificate", 
             "Use existing certificate",
             "Disable TLS (not recommended)"}
        );
        
        if (tls_choice == 0) {
            generateSelfSignedCertificate();
        } else if (tls_choice == 1) {
            auto cert_path = promptString("Path to certificate:");
            auto key_path = promptString("Path to private key:");
            validateCertificate(cert_path, key_path);
        }
        
        // Step 5: Memory configuration
        auto system_ram = detectSystemRAM();
        auto suggested_buffer_size = system_ram / 4;
        
        auto buffer_size = promptSize(
            "Buffer pool size",
            suggested_buffer_size,
            {"128MB", "256MB", "512MB", "1GB", "2GB", "Custom"}
        );
        
        // Step 6: Review and apply
        printSummary();
        
        if (promptYesNo("Apply configuration?", true)) {
            applyConfiguration();
            std::cout << "Configuration applied successfully!" << std::endl;
            std::cout << "Start server with: sudo systemctl start scratchbird" << std::endl;
            return 0;
        }
        
        return 1;
    }
    
private:
    void generateSelfSignedCertificate() {
        std::cout << "Generating self-signed certificate..." << std::endl;
        
        auto cert_dir = config_.data_directory + "/ssl";
        fs::create_directories(cert_dir);
        
        // Generate using openssl
        std::string cmd = fmt::format(
            "openssl req -new -x509 -days 365 -nodes "
            "-out {0}/server.crt "
            "-keyout {0}/server.key "
            "-subj \"/CN={1}\" "
            "2>/dev/null",
            cert_dir, getHostname()
        );
        
        std::system(cmd.c_str());
        
        // Set permissions
        fs::permissions(cert_dir + "/server.key", 
            fs::perms::owner_read | fs::perms::owner_write);
        
        config_.ssl_cert_file = cert_dir + "/server.crt";
        config_.ssl_key_file = cert_dir + "/server.key";
        config_.ssl_enabled = true;
    }
    
    void applyConfiguration() {
        // Write config file
        writeConfigFile("/etc/scratchbird/sb_server.conf");
        
        // Initialize registry
        auto registry = DatabaseRegistry::open(config_.registry_path);
        registry->initializeNewRegistry();
        
        // Initialize security database
        if (config_.auth_method == "local") {
            auto sec_db = SecurityDatabase::create(config_.security_database_path);
            sec_db->setAdminPassword(admin_password_);
        }
        
        // Set permissions
        setSecurePermissions();
    }
};
```

### 6.2 Non-Interactive Setup

```bash
# Create configuration from template
sb_setup --apply-template production \
         --bind-address 0.0.0.0 \
         --enable-native \
         --enable-postgresql \
         --generate-certificate \
         --buffer-pool 1GB

# Or apply from JSON
sb_setup --apply-config setup.json
```

Example `setup.json`:
```json
{
    "installation_type": "production",
    "network": {
        "bind_address": "0.0.0.0",
        "native_port": 3092,
        "postgresql_port": 5432
    },
    "ssl": {
        "enabled": true,
        "generate_self_signed": true
    },
    "authentication": {
        "method": "local",
        "admin_password_hash": "SCRAM-SHA-256$..."
    },
    "memory": {
        "buffer_pool_size": "1GB"
    }
}
```

## 7. Post-Installation Verification

### 7.1 Verification Script

```bash
#!/bin/bash
# verify_installation.sh

echo "ScratchBird Installation Verification"
echo "======================================"
ERRORS=0

# Check directories
echo -n "Checking directories... "
for dir in /etc/scratchbird /var/lib/scratchbird /var/log/scratchbird \
           /usr/share/scratchbird/resources; do
    if [ ! -d "$dir" ]; then
        echo "MISSING: $dir"
        ERRORS=$((ERRORS + 1))
    fi
done
echo "OK"

# Check executables
echo -n "Checking executables... "
for bin in sb_server sb_isql sb_admin sb_security; do
    if ! which "$bin" &>/dev/null; then
        echo "MISSING: $bin"
        ERRORS=$((ERRORS + 1))
    fi
done
echo "OK"

# Check TLS certificates
echo -n "Checking TLS certificates... "
if [ -f /etc/scratchbird/ssl/server.crt ]; then
    if openssl x509 -in /etc/scratchbird/ssl/server.crt -noout 2>/dev/null; then
        echo "OK"
        CERT_DAYS=$(openssl x509 -in /etc/scratchbird/ssl/server.crt -noout -enddate | \
                    cut -d= -f2 | xargs -I {} date -d "{}" +%s)
        NOW=$(date +%s)
        DAYS_LEFT=$(( (CERT_DAYS - NOW) / 86400 ))
        echo "  Certificate expires in $DAYS_LEFT days"
    else
        echo "INVALID CERTIFICATE"
        ERRORS=$((ERRORS + 1))
    fi
else
    echo "NOT FOUND"
    ERRORS=$((ERRORS + 1))
fi

# Check registry
echo -n "Checking database registry... "
if [ -f /var/lib/scratchbird/registry.sb ]; then
    echo "OK"
else
    echo "NOT INITIALIZED"
    ERRORS=$((ERRORS + 1))
fi

# Check security database
echo -n "Checking security database... "
if [ -f /var/lib/scratchbird/security.db ]; then
    echo "OK"
else
    echo "NOT INITIALIZED"
    ERRORS=$((ERRORS + 1))
fi

# Test server start
echo -n "Testing server startup... "
if sb_server --config /etc/scratchbird/sb_server.conf --check-config 2>/dev/null; then
    echo "OK"
else
    echo "FAILED"
    ERRORS=$((ERRORS + 1))
fi

echo ""
if [ $ERRORS -eq 0 ]; then
    echo "All checks passed!"
    exit 0
else
    echo "$ERRORS error(s) found."
    exit 1
fi
```

### 7.2 Health Check Endpoint

```cpp
// Server health check
class HealthCheck {
public:
    HealthStatus check() {
        HealthStatus status;
        
        // Check registry accessible
        status.registry_ok = registry_->ping();
        
        // Check security database
        status.security_db_ok = security_db_->ping();
        
        // Check listeners
        for (auto& listener : listeners_) {
            status.listeners[listener->name()] = listener->isHealthy();
        }
        
        // Check disk space
        auto data_space = getDiskSpace(config_.data_directory);
        status.disk_ok = data_space.free_bytes > 1_GB;
        status.disk_free_gb = data_space.free_bytes / 1_GB;
        
        // Check memory
        auto mem = getMemoryInfo();
        status.memory_ok = mem.available > config_.buffer_pool_size;
        status.memory_available_mb = mem.available / 1_MB;
        
        status.overall_ok = status.registry_ok && status.security_db_ok &&
                           status.disk_ok && status.memory_ok;
        
        return status;
    }
};
```

## 8. Related Specifications

- [Server Architecture](SERVER_ARCHITECTURE_AND_CONNECTION_LIFECYCLE.md)
- [Database Registry](DATABASE_REGISTRY_SPECIFICATION.md)
- [Installation and Build](deployment/INSTALLATION_AND_BUILD_SPECIFICATION.md)
- [Systemd Service](deployment/SYSTEMD_SERVICE_SPECIFICATION.md)
- [TLS and Security](Security%20Design%20Specification/AUTH_CERTIFICATE_TLS.md)
