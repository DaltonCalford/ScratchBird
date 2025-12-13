# Linux Installation (DEB)

Install ScratchBird on Debian, Ubuntu, and derived distributions.

[Back to Installation Index](index.md) | [Back to Documentation Index](../index.md)

---

## Supported Distributions

- Ubuntu 20.04 LTS, 22.04 LTS, 24.04 LTS
- Debian 11 (Bullseye), 12 (Bookworm)
- Linux Mint 20, 21
- Pop!_OS 22.04
- Other Debian-based distributions

---

## Quick Install

```bash
# Download the package
wget https://github.com/DaltonCalford/ScratchBird/releases/download/v0.9.0-beta0/scratchbird_0.9.0-beta0_amd64.deb

# Install
sudo dpkg -i scratchbird_0.9.0-beta0_amd64.deb

# Install any missing dependencies
sudo apt-get install -f

# Enable and start the service
sudo systemctl enable scratchbird
sudo systemctl start scratchbird
```

---

## Step-by-Step Installation

### 1. Prerequisites

Install required dependencies:

```bash
sudo apt update
sudo apt install libc6 libstdc++6 libssl3 libsystemd0
```

Optional dependencies for additional features:

```bash
# Compression support
sudo apt install liblz4-1

# Spatial functions
sudo apt install libgeos-c1v5

# Coordinate systems
sudo apt install libproj25
```

### 2. Download Package

Download from the releases page:

```bash
wget https://github.com/DaltonCalford/ScratchBird/releases/download/v0.9.0-beta0/scratchbird_0.9.0-beta0_amd64.deb
```

Or using curl:

```bash
curl -LO https://github.com/DaltonCalford/ScratchBird/releases/download/v0.9.0-beta0/scratchbird_0.9.0-beta0_amd64.deb
```

### 3. Verify Package (Recommended)

Verify the package integrity:

```bash
# Check SHA256 checksum
sha256sum scratchbird_0.9.0-beta0_amd64.deb
# Compare with published checksum

# View package contents
dpkg -c scratchbird_0.9.0-beta0_amd64.deb

# View package information
dpkg -I scratchbird_0.9.0-beta0_amd64.deb
```

### 4. Install Package

```bash
sudo dpkg -i scratchbird_0.9.0-beta0_amd64.deb
```

If there are dependency errors:

```bash
sudo apt-get install -f
```

### 5. Post-Installation Setup

The package automatically:
- Creates the `scratchbird` user and group
- Creates data directory `/var/lib/scratchbird`
- Creates log directory `/var/log/scratchbird`
- Creates run directory `/var/run/scratchbird`
- Installs the systemd service file

### 6. Configure the Server

Edit the configuration file:

```bash
sudo nano /etc/scratchbird/sb_server.conf
```

Key settings to review:

```ini
[server]
mode = multi-database          # or single-database
data_dir = /var/lib/scratchbird

[network]
bind_address = 0.0.0.0        # 127.0.0.1 for local only
native_port = 3092
pg_port = 5432
mysql_port = 3306
fb_port = 3050

[authentication]
methods = scram-sha-256
```

See [Configuration Reference](../configuration/sb_server.conf.md) for all options.

### 7. Start the Service

```bash
# Enable service to start on boot
sudo systemctl enable scratchbird

# Start the service
sudo systemctl start scratchbird

# Check status
sudo systemctl status scratchbird
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
| `/etc/scratchbird/sb_server.conf` | Main configuration |
| `/var/lib/scratchbird/` | Database files |
| `/var/log/scratchbird/` | Log files |
| `/var/run/scratchbird/` | PID file and sockets |

---

## Firewall Configuration

If using UFW (Uncomplicated Firewall):

```bash
# Allow ScratchBird native protocol
sudo ufw allow 3092/tcp comment 'ScratchBird Native'

# Allow PostgreSQL protocol
sudo ufw allow 5432/tcp comment 'ScratchBird PostgreSQL'

# Allow MySQL protocol (if needed)
sudo ufw allow 3306/tcp comment 'ScratchBird MySQL'

# Allow Firebird protocol (if needed)
sudo ufw allow 3050/tcp comment 'ScratchBird Firebird'

# Check rules
sudo ufw status
```

---

## Verify Installation

### Check Service Status

```bash
sudo systemctl status scratchbird
```

Expected output:
```
● scratchbird.service - ScratchBird Database Server
     Loaded: loaded (/etc/systemd/system/scratchbird.service; enabled)
     Active: active (running) since ...
```

### View Logs

```bash
# systemd journal
sudo journalctl -u scratchbird -f

# Log file
sudo tail -f /var/log/scratchbird/sb_server.log
```

### Test Connection

```bash
# Using sb_isql
sb_isql -H localhost -P 3092

# Using psql (if PostgreSQL client installed)
psql -h localhost -p 5432 -U admin
```

---

## Managing the Service

### Start/Stop/Restart

```bash
# Start
sudo systemctl start scratchbird

# Stop
sudo systemctl stop scratchbird

# Restart
sudo systemctl restart scratchbird

# Reload configuration (no restart)
sudo systemctl reload scratchbird
```

### Check Status

```bash
sudo systemctl status scratchbird
```

### View Logs

```bash
# Recent logs
sudo journalctl -u scratchbird -n 100

# Follow logs in real-time
sudo journalctl -u scratchbird -f

# Logs since boot
sudo journalctl -u scratchbird -b
```

---

## Upgrading

### Minor Version Upgrade

```bash
# Stop the service
sudo systemctl stop scratchbird

# Backup configuration
sudo cp /etc/scratchbird/sb_server.conf /etc/scratchbird/sb_server.conf.bak

# Install new package
sudo dpkg -i scratchbird_X.Y.Z_amd64.deb

# Start the service
sudo systemctl start scratchbird
```

### Major Version Upgrade

Major versions may require data migration. Always:

1. Backup your databases before upgrading
2. Read the release notes for migration instructions
3. Test the upgrade in a non-production environment first

---

## Uninstalling

### Remove Package (Keep Data)

```bash
sudo systemctl stop scratchbird
sudo systemctl disable scratchbird
sudo apt remove scratchbird
```

### Complete Removal (Including Data)

**Warning:** This permanently deletes all databases!

```bash
sudo systemctl stop scratchbird
sudo systemctl disable scratchbird
sudo apt purge scratchbird

# Manually remove data if not purged
sudo rm -rf /var/lib/scratchbird
sudo rm -rf /var/log/scratchbird
sudo rm -rf /etc/scratchbird
```

---

## Troubleshooting

### Service Won't Start

1. Check logs:
   ```bash
   sudo journalctl -u scratchbird -n 50
   ```

2. Verify configuration:
   ```bash
   sudo sb_server --config /etc/scratchbird/sb_server.conf --check
   ```

3. Check permissions:
   ```bash
   ls -la /var/lib/scratchbird
   ls -la /var/run/scratchbird
   ```

### Permission Denied

Ensure directories are owned by scratchbird:

```bash
sudo chown -R scratchbird:scratchbird /var/lib/scratchbird
sudo chown -R scratchbird:scratchbird /var/log/scratchbird
sudo chown -R scratchbird:scratchbird /var/run/scratchbird
```

### Port Already in Use

Check what's using the port:

```bash
sudo ss -tlnp | grep 3092
sudo ss -tlnp | grep 5432
```

Change ports in `/etc/scratchbird/sb_server.conf` if needed.

### Dependency Issues

```bash
# Fix broken dependencies
sudo apt-get install -f

# Check what's missing
ldd /usr/bin/sb_server | grep "not found"
```

---

## Next Steps

1. [Configure the server](../configuration/sb_server.conf.md)
2. [Create your first database](../getting-started/first-database.md)
3. [Connect with a client](../getting-started/first-connection.md)
