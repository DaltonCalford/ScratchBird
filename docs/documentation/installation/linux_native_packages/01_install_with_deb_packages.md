# Install on Debian/Ubuntu

[Linux Native Packages README](../README.md) | [Installation README](../../README.md)

## Synopsis

Install ScratchBird using Debian packages (.deb).

## Supported Versions

| Distribution | Versions |
|--------------|----------|
| Ubuntu | 22.04 LTS, 24.04 LTS |
| Debian | 11, 12 |

## Quick Install

### Add Repository

```bash
# Add SB repository key
wget -qO- https://packages.scratchbird.io/gpg-key | sudo tee /etc/apt/keyrings/scratchbird.asc

# Add repository
echo "deb [signed-by=/etc/apt/keyrings/scratchbird.asc] https://packages.scratchbird.io/apt stable main" | \
    sudo tee /etc/apt/sources.list.d/scratchbird.list

# Update package list
sudo apt update
```

### Install

```bash
# Install server and client
sudo apt install scratchbird-server scratchbird-client

# Or minimal install (client only)
sudo apt install scratchbird-client

# Or full install (with extensions)
sudo apt install scratchbird-server scratchbird-client scratchbird-contrib
```

## Manual Package Installation

### Download Package

```bash
# Download from releases
wget https://github.com/DaltonCalford/ScratchBird/releases/download/v0.5.1/scratchbird-server_0.5.1_amd64.deb
wget https://github.com/DaltonCalford/ScratchBird/releases/download/v0.5.1/scratchbird-client_0.5.1_amd64.deb
```

### Install

```bash
# Install with dependencies
sudo dpkg -i scratchbird-server_0.5.1_amd64.deb
sudo apt-get install -f  # Fix dependencies

# Or with gdebi (handles dependencies)
sudo gdebi scratchbird-server_0.5.1_amd64.deb
```

## Post-Installation

### Initialize Database

```bash
# Create data directory
sudo mkdir -p /var/lib/scratchbird/data
sudo chown scratchbird:scratchbird /var/lib/scratchbird/data

# Initialize
sudo -u scratchbird sb_initdb -D /var/lib/scratchbird/data
```

### Configure

```bash
# Edit configuration
sudo nano /etc/scratchbird/scratchbird.conf

# Key settings:
# listen_addresses = 'localhost'
# port = 3092
# max_connections = 100
```

### Start Service

```bash
# Start with systemd
sudo systemctl enable --now scratchbird

# Check status
sudo systemctl status scratchbird

# View logs
sudo journalctl -u scratchbird -f
```

### Create Database

```bash
# Create first database
sudo -u scratchbird sb_isql -c "CREATE DATABASE myapp;"

# Create user
sudo -u scratchbird sb_isql -c "CREATE USER app WITH PASSWORD 'secret';"
sudo -u scratchbird sb_isql -c "GRANT ALL ON DATABASE myapp TO app;"
```

## Verification

```bash
# Check version
sb_isql --version

# Test connection
sb_isql -c "SELECT version();"

# List databases
sb_isql -l
```

## Upgrade

```bash
# Update package list
sudo apt update

# Upgrade
sudo apt upgrade scratchbird-server scratchbird-client

# Or full upgrade
sudo apt upgrade

# Restart service
sudo systemctl restart scratchbird
```

## Uninstall

```bash
# Stop service
sudo systemctl stop scratchbird

# Remove packages
sudo apt remove scratchbird-server scratchbird-client

# Remove data (caution!)
sudo rm -rf /var/lib/scratchbird
sudo rm -rf /etc/scratchbird

# Remove repository
sudo rm /etc/apt/sources.list.d/scratchbird.list
sudo apt update
```

## Troubleshooting

### "Package not found"

```bash
# Check repository is added correctly
cat /etc/apt/sources.list.d/scratchbird.list

# Update apt
sudo apt update

# Search for package
apt search scratchbird
```

### "Failed to start service"

```bash
# Check logs
sudo journalctl -u scratchbird -n 50

# Check permissions
ls -la /var/lib/scratchbird/data

# Verify configuration
sudo -u scratchbird sb_checkconfig
```

### Port already in use

```bash
# Check what's using port 3092
sudo ss -tlnp | grep 3092

# Change port in config
sudo nano /etc/scratchbird/scratchbird.conf
# port = 3093

sudo systemctl restart scratchbird
```

## See Also

- [Install with RPM](../02_install_with_rpm_packages.md)
- [Docker Install](../container_and_image_install/01_docker_quickstart.md)
- [Post-Install Configuration](../post_install_configuration/README.md)
