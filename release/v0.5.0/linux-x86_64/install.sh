#!/bin/bash
# ScratchBird v0.5.0 Installation Script for Linux x86_64
# Copyright (C) 2025 ScratchBird Development Team

set -e

SCRATCHBIRD_VERSION="0.5.0"
INSTALL_ROOT="/opt/scratchbird"
SERVICE_NAME="scratchbird"
DEFAULT_PORT="4050"

echo "====================================================================="
echo "          ScratchBird v${SCRATCHBIRD_VERSION} Installation Script"
echo "====================================================================="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root (use sudo)"
    echo "Usage: sudo ./install.sh"
    exit 1
fi

# Check system architecture
ARCH=$(uname -m)
if [ "$ARCH" != "x86_64" ]; then
    echo "Error: This package is for x86_64 architecture, but system is $ARCH"
    exit 1
fi

# Detect Linux distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
    VERSION=$VERSION_ID
else
    echo "Warning: Cannot detect Linux distribution"
    DISTRO="unknown"
fi

echo "Detected system: $DISTRO $VERSION on $ARCH"
echo "Installation directory: $INSTALL_ROOT"
echo ""

# Check for existing Firebird installation
if systemctl is-active --quiet firebird 2>/dev/null; then
    echo "Warning: Firebird service is running. ScratchBird uses port $DEFAULT_PORT to avoid conflicts."
fi

# Create installation directory
echo "Creating installation directories..."
mkdir -p "$INSTALL_ROOT"/{bin,lib,include,conf,doc,examples,plugins}

# Copy files
echo "Installing ScratchBird files..."
cp -r bin/* "$INSTALL_ROOT/bin/"
cp -r lib/* "$INSTALL_ROOT/lib/"
cp -r include/* "$INSTALL_ROOT/include/"
cp -r doc/* "$INSTALL_ROOT/doc/"
cp -r examples/* "$INSTALL_ROOT/examples/"

# Create configuration directory if it exists
if [ -d "conf" ]; then
    cp -r conf/* "$INSTALL_ROOT/conf/"
fi

# Set permissions
echo "Setting file permissions..."
chmod +x "$INSTALL_ROOT/bin/"*
chmod 755 "$INSTALL_ROOT"
chmod -R 644 "$INSTALL_ROOT/doc/"*
chmod -R 644 "$INSTALL_ROOT/examples/"*

# Create scratchbird user and group
echo "Creating scratchbird user and group..."
if ! getent group scratchbird >/dev/null 2>&1; then
    groupadd -r scratchbird
fi

if ! getent passwd scratchbird >/dev/null 2>&1; then
    useradd -r -g scratchbird -d "$INSTALL_ROOT" -s /bin/false \
        -c "ScratchBird Database System" scratchbird
fi

# Set ownership
chown -R scratchbird:scratchbird "$INSTALL_ROOT"

# Update library cache
echo "Updating library cache..."
echo "$INSTALL_ROOT/lib" > /etc/ld.so.conf.d/scratchbird.conf
ldconfig

# Create systemd service file
echo "Creating systemd service..."
cat > /etc/systemd/system/scratchbird.service << EOF
[Unit]
Description=ScratchBird Database Server v${SCRATCHBIRD_VERSION}
After=network.target

[Service]
Type=forking
User=scratchbird
Group=scratchbird
ExecStart=$INSTALL_ROOT/bin/scratchbird -daemon
ExecStop=/bin/kill -TERM \$MAINPID
PIDFile=/var/run/scratchbird/scratchbird.pid
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/scratchbird /var/log/scratchbird /var/run/scratchbird

[Install]
WantedBy=multi-user.target
EOF

# Create runtime directories
echo "Creating runtime directories..."
mkdir -p /var/lib/scratchbird
mkdir -p /var/log/scratchbird  
mkdir -p /var/run/scratchbird
chown scratchbird:scratchbird /var/lib/scratchbird
chown scratchbird:scratchbird /var/log/scratchbird
chown scratchbird:scratchbird /var/run/scratchbird

# Enable and start service
echo "Configuring service..."
systemctl daemon-reload
systemctl enable scratchbird

# Add to PATH for all users
echo "Configuring PATH..."
cat > /etc/profile.d/scratchbird.sh << EOF
# ScratchBird Database System
export SCRATCHBIRD_HOME="$INSTALL_ROOT"
export PATH="\$SCRATCHBIRD_HOME/bin:\$PATH"
EOF

# Create desktop shortcut if desktop environment is available
if [ -d "/usr/share/applications" ]; then
    cat > /usr/share/applications/scratchbird-isql.desktop << EOF
[Desktop Entry]
Name=ScratchBird Interactive SQL
Comment=ScratchBird Database Interactive SQL Utility
Exec=gnome-terminal -- $INSTALL_ROOT/bin/sb_isql
Icon=database
Terminal=false
Type=Application
Categories=Development;Database;
EOF
fi

echo ""
echo "====================================================================="
echo "                    Installation Complete!"
echo "====================================================================="
echo ""
echo "ScratchBird v${SCRATCHBIRD_VERSION} has been successfully installed to:"
echo "  $INSTALL_ROOT"
echo ""
echo "Tools installed:"
echo "  sb_isql     - Interactive SQL utility"
echo "  sb_gbak     - Backup and restore"
echo "  sb_gfix     - Database maintenance"
echo "  sb_gsec     - Security management"
echo "  sb_gstat    - Database statistics"
echo "  sb_guard    - Process monitor"
echo "  sb_svcmgr   - Service manager"
echo ""
echo "Service configuration:"
echo "  Service name: scratchbird"
echo "  Default port: $DEFAULT_PORT"
echo "  Status: systemctl status scratchbird"
echo "  Start:  systemctl start scratchbird"
echo "  Stop:   systemctl stop scratchbird"
echo ""
echo "Usage:"
echo "  1. Reload your shell or run: source /etc/profile.d/scratchbird.sh"
echo "  2. Start service: systemctl start scratchbird"
echo "  3. Test connection: sb_isql -user SYSDBA -password masterkey"
echo "  4. Create database: CREATE DATABASE '/path/to/database.fdb';"
echo ""
echo "Documentation: $INSTALL_ROOT/doc/"
echo "Examples: $INSTALL_ROOT/examples/"
echo ""
echo "For troubleshooting, see: $INSTALL_ROOT/doc/RELEASE_NOTES.md"
echo ""
echo "Thank you for using ScratchBird!"