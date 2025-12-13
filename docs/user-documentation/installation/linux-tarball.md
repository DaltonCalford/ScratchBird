# Linux Installation (Tarball)

Install ScratchBird on any Linux distribution using the portable tarball.

[Back to Installation Index](index.md) | [Back to Documentation Index](../index.md)

---

## When to Use Tarball

The tarball installation is ideal when:
- Your distribution doesn't have DEB or RPM packages
- You need to install without root privileges
- You want a portable installation
- You need multiple versions side-by-side

---

## Quick Install

```bash
# Download
wget https://github.com/DaltonCalford/ScratchBird/releases/download/v0.9.0-beta0/scratchbird-0.9.0-beta0-linux.tar.gz

# Extract to /opt
sudo tar -xzf scratchbird-0.9.0-beta0-linux.tar.gz -C /opt

# Create symlinks
sudo ln -s /opt/scratchbird/bin/* /usr/local/bin/

# Create system user
sudo useradd -r -s /sbin/nologin scratchbird

# Create directories
sudo mkdir -p /var/lib/scratchbird /var/log/scratchbird /var/run/scratchbird
sudo chown scratchbird:scratchbird /var/lib/scratchbird /var/log/scratchbird /var/run/scratchbird

# Start server
sudo -u scratchbird /opt/scratchbird/bin/sb_server --config /opt/scratchbird/conf/sb_server.conf
```

---

## Step-by-Step Installation

### 1. Download and Extract

```bash
# Download
wget https://github.com/DaltonCalford/ScratchBird/releases/download/v0.9.0-beta0/scratchbird-0.9.0-beta0-linux.tar.gz

# Verify checksum
sha256sum scratchbird-0.9.0-beta0-linux.tar.gz

# Extract to /opt (system-wide) or ~/scratchbird (user)
sudo tar -xzf scratchbird-0.9.0-beta0-linux.tar.gz -C /opt
# or
tar -xzf scratchbird-0.9.0-beta0-linux.tar.gz -C ~
```

### 2. Tarball Contents

```
scratchbird/
├── bin/
│   ├── sb_server       # Database server
│   ├── sb_isql         # Interactive SQL shell
│   ├── sb_verify       # Verification tool
│   ├── sb_backup       # Backup utility
│   └── sb_security     # Security management
├── conf/
│   ├── sb_server.conf.example  # Configuration template
│   └── hba.conf.example        # Authentication rules template
├── lib/
│   └── libscratchbird_client.so  # Client library
├── share/
│   ├── charsets/       # Character set definitions
│   ├── collations/     # Collation definitions
│   └── timezones/      # Timezone data
└── doc/
    └── README.md
```

### 3. Create System User (Recommended)

```bash
sudo useradd -r -s /sbin/nologin -d /var/lib/scratchbird scratchbird
```

### 4. Create Directories

```bash
# Data directory
sudo mkdir -p /var/lib/scratchbird
sudo chown scratchbird:scratchbird /var/lib/scratchbird
sudo chmod 700 /var/lib/scratchbird

# Log directory
sudo mkdir -p /var/log/scratchbird
sudo chown scratchbird:scratchbird /var/log/scratchbird
sudo chmod 750 /var/log/scratchbird

# Run directory (PID file, sockets)
sudo mkdir -p /var/run/scratchbird
sudo chown scratchbird:scratchbird /var/run/scratchbird
sudo chmod 755 /var/run/scratchbird

# Configuration directory
sudo mkdir -p /etc/scratchbird
sudo cp /opt/scratchbird/conf/sb_server.conf.example /etc/scratchbird/sb_server.conf
sudo chmod 640 /etc/scratchbird/sb_server.conf
```

### 5. Configure

Edit the configuration:

```bash
sudo nano /etc/scratchbird/sb_server.conf
```

Update paths if using non-standard locations:

```ini
[server]
data_dir = /var/lib/scratchbird
pid_file = /var/run/scratchbird/sb_server.pid

[logging]
file = /var/log/scratchbird/sb_server.log

[network]
unix_socket = /var/run/scratchbird/sb.sock
```

### 6. Add to PATH (Optional)

```bash
# Add to system PATH
sudo ln -s /opt/scratchbird/bin/* /usr/local/bin/

# Or add to user's .bashrc
echo 'export PATH="/opt/scratchbird/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

---

## Running the Server

### Manual Start

```bash
# As root (drops privileges to scratchbird user)
sudo /opt/scratchbird/bin/sb_server --config /etc/scratchbird/sb_server.conf

# As scratchbird user directly
sudo -u scratchbird /opt/scratchbird/bin/sb_server --config /etc/scratchbird/sb_server.conf

# Foreground mode (for debugging)
sudo -u scratchbird /opt/scratchbird/bin/sb_server --config /etc/scratchbird/sb_server.conf --foreground
```

### Using systemd

Copy the systemd service file:

```bash
sudo cp /opt/scratchbird/conf/scratchbird.service /etc/systemd/system/
```

Or create one:

```bash
sudo tee /etc/systemd/system/scratchbird.service << 'EOF'
[Unit]
Description=ScratchBird Database Server
After=network-online.target

[Service]
Type=notify
User=scratchbird
Group=scratchbird
ExecStart=/opt/scratchbird/bin/sb_server --config /etc/scratchbird/sb_server.conf
ExecReload=/bin/kill -HUP $MAINPID
TimeoutStopSec=30
Restart=on-failure

[Install]
WantedBy=multi-user.target
EOF
```

Enable and start:

```bash
sudo systemctl daemon-reload
sudo systemctl enable scratchbird
sudo systemctl start scratchbird
```

### Using init.d (SysV)

For systems without systemd:

```bash
sudo tee /etc/init.d/scratchbird << 'EOF'
#!/bin/sh
### BEGIN INIT INFO
# Provides:          scratchbird
# Required-Start:    $network $local_fs
# Required-Stop:     $network $local_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Description:       ScratchBird Database Server
### END INIT INFO

DAEMON=/opt/scratchbird/bin/sb_server
CONFIG=/etc/scratchbird/sb_server.conf
PIDFILE=/var/run/scratchbird/sb_server.pid
USER=scratchbird

case "$1" in
    start)
        echo "Starting ScratchBird..."
        su - $USER -s /bin/sh -c "$DAEMON --config $CONFIG"
        ;;
    stop)
        echo "Stopping ScratchBird..."
        if [ -f $PIDFILE ]; then
            kill $(cat $PIDFILE)
        fi
        ;;
    restart)
        $0 stop
        sleep 2
        $0 start
        ;;
    status)
        if [ -f $PIDFILE ] && kill -0 $(cat $PIDFILE) 2>/dev/null; then
            echo "ScratchBird is running"
        else
            echo "ScratchBird is not running"
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac
EOF

sudo chmod +x /etc/init.d/scratchbird
sudo update-rc.d scratchbird defaults
```

---

## User-Level Installation

Install without root privileges:

```bash
# Extract to home directory
tar -xzf scratchbird-0.9.0-beta0-linux.tar.gz -C ~

# Create data directories
mkdir -p ~/scratchbird-data ~/scratchbird-logs

# Create configuration
cp ~/scratchbird/conf/sb_server.conf.example ~/scratchbird/sb_server.conf

# Edit configuration
nano ~/scratchbird/sb_server.conf
```

Update paths in configuration:

```ini
[server]
data_dir = /home/youruser/scratchbird-data
pid_file = /home/youruser/scratchbird/sb_server.pid

[network]
# Use unprivileged ports
native_port = 13092
pg_port = 15432
mysql_port = 13306
fb_port = 13050
unix_socket = /home/youruser/scratchbird/sb.sock

[logging]
file = /home/youruser/scratchbird-logs/sb_server.log
```

Start the server:

```bash
~/scratchbird/bin/sb_server --config ~/scratchbird/sb_server.conf
```

---

## Library Path

If you see "library not found" errors:

```bash
# Option 1: Set LD_LIBRARY_PATH
export LD_LIBRARY_PATH="/opt/scratchbird/lib:$LD_LIBRARY_PATH"

# Option 2: Add to ldconfig
echo "/opt/scratchbird/lib" | sudo tee /etc/ld.so.conf.d/scratchbird.conf
sudo ldconfig

# Option 3: Set rpath during build (for developers)
```

---

## Upgrading

```bash
# Stop server
sudo systemctl stop scratchbird

# Backup configuration
cp /etc/scratchbird/sb_server.conf /etc/scratchbird/sb_server.conf.bak

# Extract new version
sudo tar -xzf scratchbird-X.Y.Z-linux.tar.gz -C /opt

# Start server
sudo systemctl start scratchbird
```

---

## Uninstalling

```bash
# Stop server
sudo systemctl stop scratchbird
sudo systemctl disable scratchbird

# Remove files
sudo rm -rf /opt/scratchbird
sudo rm -f /usr/local/bin/sb_*
sudo rm -f /etc/systemd/system/scratchbird.service

# Remove data (WARNING: deletes all databases!)
sudo rm -rf /var/lib/scratchbird
sudo rm -rf /var/log/scratchbird
sudo rm -rf /etc/scratchbird

# Remove user
sudo userdel scratchbird
```

---

## Next Steps

1. [Configure the server](../configuration/sb_server.conf.md)
2. [Create your first database](../getting-started/first-database.md)
3. [Connect with a client](../getting-started/first-connection.md)
