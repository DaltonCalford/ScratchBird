# ScratchBird Configuration Guide 🟡

This comprehensive guide covers all aspects of configuring ScratchBird database system for optimal performance, security, and functionality. Learn how to tune your installation for development, production, and specialized environments.

## 📋 Configuration Overview

### **Configuration Files Location**
| Environment | Primary Config | Secondary Config | Location |
|-------------|----------------|------------------|----------|
| **Linux** | `scratchbird.conf` | `sbintl.conf` | `/etc/scratchbird/` |
| **Windows** | `scratchbird.conf` | `sbintl.conf` | `C:\Program Files\ScratchBird\conf\` |
| **macOS** | `scratchbird.conf` | `sbintl.conf` | `/usr/local/etc/scratchbird/` |
| **User Space** | `scratchbird.conf` | `sbintl.conf` | `$HOME/.config/scratchbird/` |

### **Configuration Priority**
ScratchBird uses the following priority order for configuration:

1. **Command-line parameters** (highest priority)
2. **Environment variables** (`SCRATCHBIRD_CONF`, `SCRATCHBIRD_HOME`)
3. **User configuration** (`~/.config/scratchbird/scratchbird.conf`)
4. **System configuration** (`/etc/scratchbird/scratchbird.conf`)
5. **Default values** (lowest priority)

---

## 🔧 Core Database Configuration

### **Memory Management**
```ini
#======================================
# Memory Configuration
#======================================

# Database page cache size (number of database pages)
# Formula: Available_RAM_MB * 0.75 / (Page_Size_KB / 1024)
# Example: For 8GB RAM with 16KB pages: 8192 * 0.75 / 16 = 384 pages per MB
# Recommendation: 10000-50000 for production systems
DefaultDbCachePages = 20000

# Temporary cache limit for sorting operations (bytes)
# Used for ORDER BY, GROUP BY, and JOIN operations
# Recommendation: 256MB to 2GB depending on query complexity
TempCacheLimit = 536870912

# Lock table memory size (bytes)
# Affects concurrent transaction capacity
# Recommendation: 1MB per 100 concurrent connections
LockMemSize = 4194304

# Sort memory for each connection (bytes)
# Memory allocated per connection for sorting
# Recommendation: 64KB to 1MB per connection
SortMemBlockSize = 1048576

# Maximum memory per statement (bytes)
# Prevents runaway queries from consuming too much memory
# Recommendation: 256MB to 1GB
MaxStatementCacheSize = 268435456
```

### **Connection Management**
```ini
#======================================
# Connection Configuration
#======================================

# Maximum concurrent user connections
# Recommendation: 50-500 depending on hardware and workload
MaxUserConnections = 200

# Connection timeout (seconds)
# How long to wait for client response
ConnectionTimeout = 60

# Authentication timeout (seconds)
# Time allowed for authentication process
AuthenticationTimeout = 30

# Dummy packet interval (seconds)
# Keep-alive packets for idle connections
DummyPacketInterval = 120

# TCP/IP configuration
RemoteServicePort = 3050
RemoteServiceName = scratchbird_db

# Enable IPv6 support
IPv6V6Only = 0

# Listen on specific interfaces (empty = all interfaces)
# Examples: 127.0.0.1 (localhost only), 0.0.0.0 (all IPv4)
RemoteBindAddress = 
```

### **Network and Protocol Settings**
```ini
#======================================
# Network Configuration
#======================================

# Wire protocol encryption
# Values: Enabled, Required, Disabled
WireCrypt = Enabled

# Wire compression for network traffic
# Reduces bandwidth usage at cost of CPU
WireCompression = true

# Remote protocol configuration
# inet (TCP/IP), xnet (local), wnet (Windows named pipes)
Providers = Remote,Engine13,Loopback

# Network packet size (bytes)
# Larger packets = better throughput, more memory usage
# Recommendation: 8192-32768
TcpRemoteBufferSize = 16384

# Connection pooling settings
ConnectionPoolSize = 100
ConnectionPoolTimeout = 60
```

---

## 🏗️ Schema and Hierarchical Configuration

### **Hierarchical Schema Settings**
```ini
#======================================
# Schema Configuration
#======================================

# Maximum schema nesting depth (1-8)
# ScratchBird supports up to 8 levels: schema.level1.level2...level8
MaxSchemaDepth = 8

# Maximum schema path length (63-511 characters)
# Total length including all separators
MaxSchemaPathLength = 511

# Enable schema path caching
# Improves performance for hierarchical schema operations
EnableSchemaCache = 1

# Schema cache size (number of cached paths)
# Higher values = better performance, more memory usage
SchemaCacheSize = 10000

# Schema cache timeout (seconds)
# How long to cache schema information
SchemaCacheTimeout = 300

# Default schema for new connections
# Leave empty for no default schema
DefaultSchema = 

# Enable schema inheritance for permissions
# Child schemas inherit parent permissions by default
EnableSchemaInheritance = 1

# Schema validation level
# 0=None, 1=Basic, 2=Full (recommended)
SchemaValidationLevel = 2
```

### **Namespace and Object Resolution**
```ini
#======================================
# Namespace Configuration
#======================================

# Package vs Schema resolution priority
# When both package and schema exist with same name
# Values: PACKAGE_FIRST (default), SCHEMA_FIRST
NamespaceResolutionMode = PACKAGE_FIRST

# Enable relative path resolution
# Allows ../parent and ./current syntax in schema paths
EnableRelativeSchemaPath = 1

# Case sensitivity for schema names
# Values: SENSITIVE, INSENSITIVE, AUTO (database default)
SchemaCaseSensitivity = AUTO

# Maximum objects per schema (0 = unlimited)
# Prevents accidentally creating too many objects
MaxObjectsPerSchema = 0

# Enable schema-aware statistics
# Maintains separate statistics for each schema
SchemaAwareStatistics = 1
```

---

## 🔐 Security Configuration

### **Authentication and Authorization**
```ini
#======================================
# Security Configuration
#======================================

# Authentication method
# Values: Srp256 (recommended), Srp, Legacy_Auth
AuthMethod = Srp256

# Security database location
SecurityDatabase = /var/lib/scratchbird/security4.fdb

# User management settings
UserManagerPlugin = Srp

# Password policy enforcement
MinPasswordLength = 8
MaxPasswordLength = 128
PasswordComplexity = 1  # 0=None, 1=Basic, 2=Strong

# Account lockout policy
MaxFailedLoginAttempts = 5
AccountLockoutDuration = 300  # seconds

# Session timeout (seconds)
SessionTimeout = 28800  # 8 hours

# Enable two-factor authentication
EnableTwoFactorAuth = 0

# Audit trail configuration
AuditTrailLog = /var/log/scratchbird/audit.log
AuditTrailLevel = 2  # 0=None, 1=Basic, 2=Detailed, 3=Verbose
```

### **SSL/TLS Configuration**
```ini
#======================================
# SSL/TLS Configuration
#======================================

# SSL certificate and key files
SSLCertificate = /etc/scratchbird/ssl/server.crt
SSLPrivateKey = /etc/scratchbird/ssl/server.key
SSLCACertificate = /etc/scratchbird/ssl/ca.crt

# SSL cipher suites (leave empty for defaults)
SSLCiphers = 

# SSL protocol versions
SSLMinProtocol = TLSv1.2
SSLMaxProtocol = TLSv1.3

# Certificate verification
SSLVerifyClient = 0  # 0=No, 1=Optional, 2=Required

# SSL session timeout (seconds)
SSLSessionTimeout = 3600

# Enable SSL session caching
SSLSessionCache = 1
```

### **Access Control**
```ini
#======================================
# Access Control Configuration
#======================================

# Database access control
# Values: None, ReadOnly, Full
DatabaseAccessMode = Full

# Remote connections allowed
AllowRemoteConnections = 1

# Trusted authentication for local connections
EnableTrustedAuth = 1

# IP address restrictions (comma-separated)
# Leave empty to allow all addresses
AllowedIPAddresses = 

# Deny specific IP addresses
DeniedIPAddresses = 

# Enable connection encryption requirement
RequireEncryption = 0

# File access restrictions
RestrictDatabaseAccess = 1
DatabasePathPrefix = /var/lib/scratchbird
```

---

## ⚡ Performance Tuning

### **Garbage Collection and Maintenance**
```ini
#======================================
# Garbage Collection Configuration
#======================================

# Garbage collection policy
# Values: cooperative (default), background, combined
GCPolicy = cooperative

# Background garbage collection interval (seconds)
BackgroundGCInterval = 300

# Garbage collection threshold (percentage)
# Trigger GC when this percentage of pages are garbage
GCThreshold = 50

# Maximum garbage collection time per sweep (milliseconds)
MaxGCSweepTime = 100

# Enable automatic index statistics updates
AutoIndexStatistics = 1

# Statistics update threshold (percentage of changes)
StatisticsUpdateThreshold = 20
```

### **I/O and Disk Configuration**
```ini
#======================================
# I/O Configuration
#======================================

# Forced writes to disk
# Values: 1 (safe, slower), 0 (faster, less safe)
ForcedWrites = 1

# File system cache usage
# Values: 1 (use OS cache), 0 (direct I/O)
FileSystemCacheThreshold = 65536

# Maximum parallel I/O operations
MaxParallelWorkers = 4

# Disk read/write optimization
OptimizeForSequentialIO = 0

# Temporary file directory
TempDirectories = /tmp/scratchbird

# Disk space monitoring
MinDiskSpaceThreshold = 1073741824  # 1GB
DiskSpaceCheckInterval = 300  # seconds
```

### **CPU and Parallel Processing**
```ini
#======================================
# CPU Configuration
#======================================

# CPU affinity mask (bitmask of allowed CPUs)
# 0 = use all CPUs, or specific mask like 0x0F for first 4 CPUs
CpuAffinityMask = 0

# Maximum worker threads for parallel operations
# Recommendation: Number of CPU cores
MaxWorkerThreads = 8

# Thread pool configuration
ThreadPoolSize = 20
ThreadPoolTimeout = 60

# Enable NUMA awareness
EnableNumaOptimization = 0

# Priority class for database process
# Values: Normal, High, Realtime (use with caution)
ProcessPriority = Normal

# Enable hyper-threading optimization
EnableHyperThreading = 1
```

---

## 🏭 Environment-Specific Configurations

### **Development Environment**
```ini
# development.conf - Development-friendly settings

#======================================
# Development Configuration
#======================================

# Reduced cache for development machines
DefaultDbCachePages = 5000
TempCacheLimit = 134217728  # 128MB

# Relaxed connection limits
MaxUserConnections = 50
ConnectionTimeout = 300  # Longer timeout for debugging

# Enhanced debugging
DebugLevel = 3
LogFileSize = 10485760  # 10MB
TraceConnectionPoolActivity = 1

# Relaxed security for development
AuthMethod = Legacy_Auth
WireCrypt = Enabled  # Not required
RequireEncryption = 0

# Development-friendly paths
DatabasePathPrefix = 
RestrictDatabaseAccess = 0

# Aggressive garbage collection for testing
GCPolicy = background
BackgroundGCInterval = 60

# Schema settings for testing
MaxSchemaDepth = 8
EnableSchemaCache = 1
SchemaCacheTimeout = 60  # Shorter cache for development

# Development logging
AuditTrailLevel = 3
TraceConfigErrors = 1
TraceConnectionErrors = 1
```

### **Production Environment**
```ini
# production.conf - Production-optimized settings

#======================================
# Production Configuration
#======================================

# High-performance memory settings
DefaultDbCachePages = 50000
TempCacheLimit = 1073741824  # 1GB
LockMemSize = 8388608  # 8MB
SortMemBlockSize = 2097152  # 2MB

# Production connection settings
MaxUserConnections = 500
ConnectionTimeout = 60
AuthenticationTimeout = 15

# Security hardening
AuthMethod = Srp256
WireCrypt = Required
RequireEncryption = 1
EnableTwoFactorAuth = 1

# Password policy
MinPasswordLength = 12
PasswordComplexity = 2
MaxFailedLoginAttempts = 3
AccountLockoutDuration = 600

# SSL/TLS required
SSLMinProtocol = TLSv1.2
SSLVerifyClient = 1

# Restricted access
RestrictDatabaseAccess = 1
DatabasePathPrefix = /var/lib/scratchbird

# Performance optimization
GCPolicy = combined
MaxParallelWorkers = 8
ForcedWrites = 1

# Production monitoring
AuditTrailLevel = 2
LogFileSize = 104857600  # 100MB

# Schema configuration for production
MaxSchemaDepth = 6  # Reasonable limit for production
SchemaCacheSize = 20000
SchemaCacheTimeout = 600  # Longer cache for stability

# Resource limits
MaxObjectsPerSchema = 10000
MaxStatementCacheSize = 536870912  # 512MB
```

### **High-Availability Environment**
```ini
# ha.conf - High-availability configuration

#======================================
# High Availability Configuration
#======================================

# Optimized for reliability
DefaultDbCachePages = 30000
TempCacheLimit = 536870912
MaxUserConnections = 1000

# Redundancy and reliability
ForcedWrites = 1
FileSystemCacheThreshold = 131072

# Connection resilience
ConnectionTimeout = 30
DummyPacketInterval = 60
ConnectionPoolTimeout = 30

# Monitoring and alerting
AuditTrailLevel = 2
TraceConnectionPoolActivity = 1
DiskSpaceCheckInterval = 60

# Security for HA environment
WireCrypt = Required
AuthMethod = Srp256
SessionTimeout = 14400  # 4 hours

# Performance with reliability focus
GCPolicy = background
BackgroundGCInterval = 180
MaxGCSweepTime = 50  # Shorter sweeps for responsiveness

# HA-specific schema settings
EnableSchemaCache = 1
SchemaCacheSize = 15000
SchemaValidationLevel = 2

# Load balancing friendly
MaxWorkerThreads = 12
ThreadPoolSize = 50
```

### **High-Performance Environment**
```ini
# performance.conf - Maximum performance configuration

#======================================
# High Performance Configuration
#======================================

# Maximum memory allocation
DefaultDbCachePages = 100000
TempCacheLimit = 2147483648  # 2GB
LockMemSize = 16777216  # 16MB
SortMemBlockSize = 4194304  # 4MB

# High connection capacity
MaxUserConnections = 1000
ConnectionPoolSize = 200

# Optimized I/O
ForcedWrites = 0  # Risk for maximum performance
FileSystemCacheThreshold = 262144
MaxParallelWorkers = 16

# CPU optimization
CpuAffinityMask = 0  # Use all CPUs
MaxWorkerThreads = 16
ThreadPoolSize = 100
EnableNumaOptimization = 1

# Aggressive garbage collection
GCPolicy = background
BackgroundGCInterval = 120
GCThreshold = 30

# Performance-oriented schema settings
EnableSchemaCache = 1
SchemaCacheSize = 50000
SchemaCacheTimeout = 1800  # 30 minutes

# Minimal logging for performance
DebugLevel = 1
AuditTrailLevel = 1

# Network optimization
TcpRemoteBufferSize = 32768
WireCompression = false  # Disable for maximum speed

# Statistics optimization
AutoIndexStatistics = 1
StatisticsUpdateThreshold = 10
```

---

## 🔧 Advanced Configuration

### **Monitoring and Logging**
```ini
#======================================
# Monitoring Configuration
#======================================

# Debug and trace levels
DebugLevel = 2  # 0=None, 1=Errors, 2=Warnings, 3=Info, 4=Debug

# Log file configuration
LogFileSize = 52428800  # 50MB
MaxLogFiles = 10
LogRotation = 1

# Specific trace options
TraceConnectionPoolActivity = 0
TraceConfigErrors = 1
TraceConnectionErrors = 1
TraceDSQLOperations = 0
TraceServiceOperations = 0

# Performance monitoring
EnablePerformanceMonitoring = 1
PerformanceMonitoringInterval = 300
CollectStatistics = 1

# Event logging
EventLogLevel = 2
EventLogSource = ScratchBird
```

### **Internationalization**
```ini
#======================================
# Internationalization Configuration
#======================================

# Default character set
DefaultCharacterSet = UTF8

# Message file location
MessageFile = /opt/scratchbird/lib/scratchbird.msg

# International collation library
IntlModule = /opt/scratchbird/lib/libfbintl.so

# Timezone configuration
TimeZone = 
TimeZoneFile = /opt/scratchbird/tzdata/timezones.conf

# Currency and number formatting
CurrencySymbol = $
DecimalSeparator = .
ThousandsSeparator = ,
```

### **Plugin Configuration**
```ini
#======================================
# Plugin Configuration
#======================================

# Database encryption plugin
DatabaseCryptPlugin = 

# User manager plugin
UserManagerPlugin = Srp

# Authentication plugins (order matters)
AuthServer = Srp256, Srp, Legacy_Auth
AuthClient = Srp256, Srp, Legacy_Auth, Legacy_List

# Trace plugin
TracePlugin = fbtrace

# External function libraries
ExternalFunctionModule = /opt/scratchbird/lib/libfbudf.so
```

---

## 🛠️ Configuration Management

### **Configuration Validation**
```bash
# Validate configuration file
sb_guard -config /etc/scratchbird/scratchbird.conf -validate

# Test configuration without starting service
sb_guard -config /etc/scratchbird/scratchbird.conf -test

# Display current configuration
sb_guard -config /etc/scratchbird/scratchbird.conf -show-config

# Check configuration syntax
sb_guard -config /etc/scratchbird/scratchbird.conf -syntax-check
```

### **Dynamic Configuration Updates**
```bash
# Reload configuration without restart (limited settings)
sudo systemctl reload scratchbird-guardian

# Full restart for major configuration changes
sudo systemctl restart scratchbird-guardian

# Check which settings require restart
sb_guard -config /etc/scratchbird/scratchbird.conf -check-restart-required
```

### **Configuration Backup and Recovery**
```bash
# Backup current configuration
sudo cp /etc/scratchbird/scratchbird.conf /etc/scratchbird/scratchbird.conf.backup.$(date +%Y%m%d)

# Create configuration template
sudo sb_guard -create-config-template > /etc/scratchbird/template.conf

# Restore from backup
sudo cp /etc/scratchbird/scratchbird.conf.backup.20250120 /etc/scratchbird/scratchbird.conf
sudo systemctl restart scratchbird-guardian
```

### **Environment Variable Overrides**
```bash
# Override specific settings via environment variables
export SCRATCHBIRD_DEFAULT_DB_CACHE_PAGES=25000
export SCRATCHBIRD_MAX_USER_CONNECTIONS=300
export SCRATCHBIRD_WIRE_CRYPT=Required

# Start with environment overrides
sudo -E systemctl restart scratchbird-guardian

# View effective configuration
sb_guard -show-effective-config
```

---

## 🔍 Performance Monitoring

### **Configuration for Monitoring Tools**
```ini
#======================================
# Monitoring Integration
#======================================

# Prometheus metrics endpoint
EnablePrometheusMetrics = 1
PrometheusMetricsPort = 9100
PrometheusMetricsPath = /metrics

# SNMP monitoring
EnableSNMP = 0
SNMPCommunity = public
SNMPPort = 161

# JMX monitoring (for Java applications)
EnableJMX = 0
JMXPort = 9999

# Health check endpoint
EnableHealthCheck = 1
HealthCheckPort = 8080
HealthCheckPath = /health

# Statistics collection
CollectDetailedStatistics = 1
StatisticsRetentionDays = 30
```

### **Performance Monitoring Script**
```bash
#!/bin/bash
# monitor_config.sh - Monitor configuration performance impact

CONFIG_FILE="/etc/scratchbird/scratchbird.conf"
LOG_FILE="/var/log/scratchbird/config_monitoring.log"

# Function to get current performance metrics
get_metrics() {
    echo "$(date): Performance Metrics" >> "$LOG_FILE"
    sb_gstat -cache -brief /var/lib/scratchbird/production.fdb >> "$LOG_FILE"
    sb_lock_print -summary /var/lib/scratchbird/production.fdb >> "$LOG_FILE"
    echo "---" >> "$LOG_FILE"
}

# Monitor before configuration change
echo "Pre-change metrics:" >> "$LOG_FILE"
get_metrics

# Apply configuration change (example: increase cache)
sudo sed -i 's/DefaultDbCachePages = 20000/DefaultDbCachePages = 30000/' "$CONFIG_FILE"
sudo systemctl restart scratchbird-guardian

# Wait for service to stabilize
sleep 30

# Monitor after configuration change
echo "Post-change metrics:" >> "$LOG_FILE"
get_metrics

echo "Configuration change monitoring completed. Check $LOG_FILE for results."
```

---

## 🆘 Troubleshooting Configuration

### **Common Configuration Issues**

**Issue**: Service fails to start after configuration change
```bash
# Check configuration syntax
sb_guard -config /etc/scratchbird/scratchbird.conf -validate

# Check service logs
sudo journalctl -u scratchbird-guardian -n 50

# Restore previous configuration
sudo cp /etc/scratchbird/scratchbird.conf.backup /etc/scratchbird/scratchbird.conf
sudo systemctl restart scratchbird-guardian
```

**Issue**: Poor performance after configuration changes
```bash
# Check memory usage
free -h
sb_gstat -cache /var/lib/scratchbird/database.fdb

# Verify settings took effect
sb_guard -show-effective-config | grep -i cache

# Monitor system resources
top -p $(pgrep sb_guard)
iostat -x 1 10
```

**Issue**: SSL/TLS configuration problems
```bash
# Test SSL certificate
openssl x509 -in /etc/scratchbird/ssl/server.crt -text -noout

# Verify private key
openssl rsa -in /etc/scratchbird/ssl/server.key -check

# Test SSL connection
openssl s_client -connect localhost:3050

# Check certificate permissions
ls -la /etc/scratchbird/ssl/
```

**Issue**: Schema configuration not working
```bash
# Verify schema settings
sb_guard -show-effective-config | grep -i schema

# Test schema creation
sb_isql -user SYSDBA -password masterkey
SQL> CREATE SCHEMA test;
SQL> CREATE SCHEMA test.child;

# Check schema limits
sb_isql -execute "SELECT COUNT(*) FROM RDB\$SCHEMAS;"
```

---

## 🎯 Next Steps

- **[Database Engine](05-database-engine.md)** - Understanding ScratchBird architecture
- **[Hierarchical Schemas](07-hierarchical-schemas.md)** - Configure schema features
- **[Security Guide](08-security.md)** - Secure your configuration
- **[Performance Tuning](20-performance.md)** - Advanced optimization

## 📚 Related Documentation

- **[Installation Guide](03-installation.md)** - Initial setup and installation
- **[Utilities Overview](09-utilities-overview.md)** - Configure utility settings
- **[Troubleshooting](25-troubleshooting.md)** - Configuration troubleshooting

---

## 💡 Configuration Tips

> **Start with Templates**: Use provided templates for your environment type
> ```bash
> # Copy appropriate template
> sudo cp /opt/scratchbird/conf/templates/production.conf /etc/scratchbird/scratchbird.conf
> ```

> **Monitor Configuration Changes**: Always monitor performance after changes
> ```bash
> # Before change
> sb_gstat -cache database.fdb > before.txt
> # Apply change and restart
> # After change
> sb_gstat -cache database.fdb > after.txt
> diff before.txt after.txt
> ```

> **Backup Before Changes**: Always backup configuration before modifications
> ```bash
> sudo cp /etc/scratchbird/scratchbird.conf /etc/scratchbird/scratchbird.conf.$(date +%Y%m%d)
> ```

**⚙️ Ready to optimize your ScratchBird configuration?** Use the appropriate template for your environment and customize based on your specific requirements!