# Linux Installation (RPM)

Install ScratchBird on RHEL, Fedora, and derived distributions.

[Back to Installation Index](index.md) | [Back to Documentation Index](../index.md)

---

## Supported Distributions

- Red Hat Enterprise Linux 8, 9
- Fedora 38, 39, 40
- CentOS Stream 8, 9
- Rocky Linux 8, 9
- AlmaLinux 8, 9
- Oracle Linux 8, 9

---

## Quick Install

```bash
# Download the package
wget https://github.com/DaltonCalford/ScratchBird/releases/download/v0.9.0-beta0/scratchbird-0.9.0-beta0-1.x86_64.rpm

# Install
sudo rpm -i scratchbird-0.9.0-beta0-1.x86_64.rpm

# Or using dnf
sudo dnf install ./scratchbird-0.9.0-beta0-1.x86_64.rpm

# Enable and start the service
sudo systemctl enable scratchbird
sudo systemctl start scratchbird
```

---

## Package Options

If you install from a repository (recommended for production), ScratchBird is
split into modular packages:

**Core**
- `scratchbird-server` (server + service)
- `scratchbird-tools` (CLI tools + `sb_setup`)
- `scratchbird-client-libs` (native client libraries)
- `scratchbird-dev` (headers/SDK)
- `scratchrobin` (GUI)

**Emulation listeners**
- `scratchbird-emulation-pg`
- `scratchbird-emulation-mysql`
- `scratchbird-emulation-firebird`

**Connectivity**
- `scratchbird-odbc`
- `scratchbird-jdbc`

**Language drivers**
- `scratchbird-driver-go`, `scratchbird-driver-python`, `scratchbird-driver-node`
- `scratchbird-driver-dotnet`, `scratchbird-driver-jdbc`, `scratchbird-driver-php`
- `scratchbird-driver-ruby`, `scratchbird-driver-r`, `scratchbird-driver-rust`
- `scratchbird-driver-pascal`

**Meta packages**
- `scratchbird` (server + tools + client libs)
- `scratchbird-emulation-all`
- `scratchbird-drivers-all`

Full installer feature matrix: see
[Installer Features + Config Generator](../../specifications/deployment/INSTALLER_FEATURES_AND_CONFIG_GENERATOR.md).

---

## Step-by-Step Installation

### 1. Prerequisites

Install required dependencies:

```bash
sudo dnf install glibc libstdc++ openssl-libs systemd-libs
```

Optional dependencies for additional features:

```bash
# Compression support
sudo dnf install lz4-libs

# Spatial functions
sudo dnf install geos

# Coordinate systems
sudo dnf install proj
```

### 2. Download Package

```bash
wget https://github.com/DaltonCalford/ScratchBird/releases/download/v0.9.0-beta0/scratchbird-0.9.0-beta0-1.x86_64.rpm
```

### 3. Verify Package (Recommended)

```bash
# Check SHA256 checksum
sha256sum scratchbird-0.9.0-beta0-1.x86_64.rpm

# View package contents
rpm -qlp scratchbird-0.9.0-beta0-1.x86_64.rpm

# View package information
rpm -qip scratchbird-0.9.0-beta0-1.x86_64.rpm
```

### 4. Install Package

Using dnf (recommended):
```bash
sudo dnf install ./scratchbird-0.9.0-beta0-1.x86_64.rpm
```

Using rpm directly:
```bash
sudo rpm -i scratchbird-0.9.0-beta0-1.x86_64.rpm
```

### 5. Configure the Server

Edit the configuration file:

```bash
sudo vi /etc/scratchbird/sb_server.conf
```

See [Configuration Reference](../configuration/sb_server.conf.md) for all options.

You can also run the post-install configuration wizard to add/remove features,
set ports, and tune performance:

```bash
sudo sb_setup --interactive
```

### 6. Start the Service

```bash
sudo systemctl enable scratchbird
sudo systemctl start scratchbird
sudo systemctl status scratchbird
```

---

## Firewall Configuration (firewalld)

```bash
# Allow ScratchBird native protocol
sudo firewall-cmd --add-port=3092/tcp --permanent

# Allow PostgreSQL protocol
sudo firewall-cmd --add-port=5432/tcp --permanent

# Allow MySQL protocol (if needed)
sudo firewall-cmd --add-port=3306/tcp --permanent

# Allow Firebird protocol (if needed)
sudo firewall-cmd --add-port=3050/tcp --permanent

# Reload firewall
sudo firewall-cmd --reload

# Verify
sudo firewall-cmd --list-ports
```

---

## SELinux Configuration

If SELinux is enforcing:

```bash
# Check SELinux status
getenforce

# Allow ScratchBird to bind to network ports
sudo semanage port -a -t http_port_t -p tcp 3092

# If you encounter permission issues
sudo ausearch -c 'sb_server' --raw | audit2allow -M scratchbird
sudo semodule -i scratchbird.pp
```

---

## Directory Layout

After installation:

| Path | Description |
|------|-------------|
| `/usr/bin/sb_server` | Server daemon |
| `/usr/bin/sb_isql` | Interactive SQL shell |
| `/usr/bin/sb_verify` | Database verification tool |
| `/usr/bin/sb_backup` | Backup utility |
| `/usr/bin/sb_security` | Security management |
| `/etc/scratchbird/` | Configuration files |
| `/var/lib/scratchbird/` | Database files |
| `/var/log/scratchbird/` | Log files |
| `/var/run/scratchbird/` | PID file and sockets |

---

## Managing the Service

```bash
# Start/Stop/Restart
sudo systemctl start scratchbird
sudo systemctl stop scratchbird
sudo systemctl restart scratchbird
sudo systemctl reload scratchbird

# Check status
sudo systemctl status scratchbird

# View logs
sudo journalctl -u scratchbird -f
```

---

## Upgrading

```bash
# Stop service
sudo systemctl stop scratchbird

# Backup configuration
sudo cp /etc/scratchbird/sb_server.conf /etc/scratchbird/sb_server.conf.bak

# Upgrade package
sudo dnf upgrade ./scratchbird-X.Y.Z-1.x86_64.rpm

# Start service
sudo systemctl start scratchbird
```

---

## Uninstalling

### Remove Package (Keep Data)

```bash
sudo systemctl stop scratchbird
sudo systemctl disable scratchbird
sudo dnf remove scratchbird
```

### Complete Removal

```bash
sudo systemctl stop scratchbird
sudo systemctl disable scratchbird
sudo dnf remove scratchbird
sudo rm -rf /var/lib/scratchbird
sudo rm -rf /var/log/scratchbird
sudo rm -rf /etc/scratchbird
```

---

## Troubleshooting

### Service Won't Start

```bash
# Check logs
sudo journalctl -u scratchbird -n 50

# Check SELinux denials
sudo ausearch -m AVC -ts recent
```

### SELinux Issues

```bash
# Temporarily set to permissive (for testing)
sudo setenforce 0

# If it works, create proper policy
sudo ausearch -c 'sb_server' --raw | audit2allow -M scratchbird
sudo semodule -i scratchbird.pp

# Re-enable enforcing
sudo setenforce 1
```

---

## Next Steps

1. [Configure the server](../configuration/sb_server.conf.md)
2. [Create your first database](../getting-started/first-database.md)
3. [Connect with a client](../getting-started/first-connection.md)
