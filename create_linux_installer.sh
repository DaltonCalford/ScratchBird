#!/bin/bash

# ScratchBird v0.6 Linux Standalone Installer Creator
# Creates a self-extracting installer package for Linux
# Author: ScratchBird Development Team
# Date: July 14, 2025

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="$SCRIPT_DIR"
INSTALLER_DIR="$BUILD_ROOT/installers"
VERSION="0.6.0"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Create installer directory
mkdir -p "$INSTALLER_DIR"

# Function to create installation files
create_install_files() {
    log_info "Creating installation file structure"
    
    local install_staging="$INSTALLER_DIR/staging_linux"
    rm -rf "$install_staging"
    mkdir -p "$install_staging"
    
    # Copy ScratchBird binaries and libraries
    if [[ -d "gen/Release/scratchbird" ]]; then
        cp -r gen/Release/scratchbird/* "$install_staging/"
        log_success "Copied ScratchBird binaries"
    else
        log_error "ScratchBird binaries not found. Run build script first."
        exit 1
    fi
    
    # Create installation directories
    mkdir -p "$install_staging/etc/systemd/system"
    mkdir -p "$install_staging/etc/scratchbird"
    mkdir -p "$install_staging/var/lib/scratchbird"
    mkdir -p "$install_staging/var/log/scratchbird"
    
    # Copy systemd service file
    if [[ -f "builds/install/arch-specific/linux/scratchbird.service.in" ]]; then
        cp "builds/install/arch-specific/linux/scratchbird.service.in" "$install_staging/etc/systemd/system/scratchbird.service"
        log_success "Copied systemd service file"
    fi
    
    # Create default configuration
    cat > "$install_staging/etc/scratchbird/scratchbird.conf" << 'EOF'
# ScratchBird Database Server Configuration
# Version 0.6.0

# Network settings
RemoteServicePort = 4050
RemoteServiceName = sb_fdb

# File locations
DatabaseAccess = Full
LogFileSize = 1048576
UserManager = Legacy_UserManager

# Performance settings
DefaultDbCachePages = 2048
TempCacheLimit = 67108864

# Security settings
AuthServer = Legacy_Auth, Srp, Win_Sspi
UserManager = Legacy_UserManager
SecurityDatabase = $(dir_secdb)/security6.fdb

# Character sets
DefaultCharacterSet = UTF8
ConnectionCharacterSet = UTF8
EOF
    
    # Create databases.conf
    cat > "$install_staging/etc/scratchbird/databases.conf" << 'EOF'
# ScratchBird Database Configuration
# Version 0.6.0

# Default security database
security.fdb = $(dir_secdb)/security6.fdb
{
    SecurityDatabase = true
}

# Template databases
employee.fdb = $(dir_sampleDb)/employee.fdb
{
    ReadOnly = true
}
EOF
    
    log_success "Created configuration files"
}

# Function to create installer script
create_installer_script() {
    log_info "Creating installer script"
    
    local installer_script="$INSTALLER_DIR/scratchbird-${VERSION}-linux-installer.run"
    
    cat > "$installer_script" << 'INSTALLER_EOF'
#!/bin/bash

# ScratchBird v0.6 Linux Installer
# Self-extracting installer package

INSTALLER_VERSION="0.6.0"
INSTALL_PREFIX="/opt/scratchbird"
SERVICE_NAME="scratchbird"
SERVICE_PORT="4050"

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This installer must be run as root"
        log_info "Please run: sudo $0"
        exit 1
    fi
}

# Create system user
create_user() {
    if ! id "scratchbird" &>/dev/null; then
        log_info "Creating scratchbird system user"
        useradd -r -s /bin/false -d /var/lib/scratchbird -c "ScratchBird Database Server" scratchbird
        log_success "Created scratchbird user"
    else
        log_info "User scratchbird already exists"
    fi
}

# Extract and install files
install_files() {
    log_info "Installing ScratchBird files"
    
    # Create installation directory
    mkdir -p "$INSTALL_PREFIX"
    
    # Extract archive (appended to this script)
    local archive_line=$(awk '/^__ARCHIVE_BELOW__/ {print NR + 1; exit 0; }' "$0")
    tail -n +$archive_line "$0" | tar -xzf - -C "$INSTALL_PREFIX"
    
    # Set correct permissions
    chmod -R 755 "$INSTALL_PREFIX/bin"
    chmod -R 644 "$INSTALL_PREFIX/lib"
    chmod 755 "$INSTALL_PREFIX"/lib/*.so*
    
    # Create symlinks for system-wide access
    ln -sf "$INSTALL_PREFIX/bin/sb_isql" /usr/local/bin/sb_isql
    ln -sf "$INSTALL_PREFIX/bin/sb_gbak" /usr/local/bin/sb_gbak
    ln -sf "$INSTALL_PREFIX/bin/sb_gfix" /usr/local/bin/sb_gfix
    ln -sf "$INSTALL_PREFIX/bin/sb_gsec" /usr/local/bin/sb_gsec
    ln -sf "$INSTALL_PREFIX/bin/sb_gstat" /usr/local/bin/sb_gstat
    
    log_success "Files installed to $INSTALL_PREFIX"
}

# Configure systemd service
setup_service() {
    if command -v systemctl &> /dev/null; then
        log_info "Setting up systemd service"
        
        # Copy service file
        cp "$INSTALL_PREFIX/etc/systemd/system/scratchbird.service" /etc/systemd/system/
        
        # Replace template variables
        sed -i "s|@FB_SBINDIR@|$INSTALL_PREFIX/bin|g" /etc/systemd/system/scratchbird.service
        sed -i "s|@FB_PREFIX@|$INSTALL_PREFIX|g" /etc/systemd/system/scratchbird.service
        
        # Reload systemd and enable service
        systemctl daemon-reload
        systemctl enable scratchbird.service
        
        log_success "Systemd service configured and enabled"
    else
        log_warning "Systemctl not available, manual service setup required"
    fi
}

# Set up firewall rules
setup_firewall() {
    if command -v ufw &> /dev/null; then
        log_info "Configuring UFW firewall for port $SERVICE_PORT"
        ufw allow $SERVICE_PORT/tcp comment "ScratchBird Database Server"
        log_success "UFW firewall rule added"
    elif command -v firewall-cmd &> /dev/null; then
        log_info "Configuring firewalld for port $SERVICE_PORT"
        firewall-cmd --permanent --add-port=$SERVICE_PORT/tcp
        firewall-cmd --reload
        log_success "Firewalld rule added"
    else
        log_warning "No supported firewall found. Manually open port $SERVICE_PORT"
    fi
}

# Configure directories and permissions
setup_directories() {
    log_info "Setting up directories and permissions"
    
    # Create data directories
    mkdir -p /var/lib/scratchbird
    mkdir -p /var/log/scratchbird
    mkdir -p /etc/scratchbird
    
    # Copy configuration files
    cp -r "$INSTALL_PREFIX/etc/scratchbird"/* /etc/scratchbird/
    
    # Set ownership
    chown -R scratchbird:scratchbird /var/lib/scratchbird
    chown -R scratchbird:scratchbird /var/log/scratchbird
    chown -R scratchbird:scratchbird /etc/scratchbird
    
    # Set permissions
    chmod 750 /var/lib/scratchbird
    chmod 750 /var/log/scratchbird
    chmod 750 /etc/scratchbird
    
    log_success "Directory structure and permissions configured"
}

# Main installation function
main() {
    log_info "Starting ScratchBird v$INSTALLER_VERSION installation"
    
    check_root
    create_user
    install_files
    setup_directories
    setup_service
    setup_firewall
    
    log_success "ScratchBird installation completed successfully!"
    log_info ""
    log_info "Installation details:"
    log_info "  Installation directory: $INSTALL_PREFIX"
    log_info "  Configuration files: /etc/scratchbird/"
    log_info "  Data directory: /var/lib/scratchbird"
    log_info "  Log directory: /var/log/scratchbird"
    log_info "  Service port: $SERVICE_PORT"
    log_info ""
    log_info "To start ScratchBird:"
    log_info "  sudo systemctl start scratchbird"
    log_info ""
    log_info "To connect:"
    log_info "  sb_isql localhost/4050:employee"
    log_info ""
    log_warning "Please secure your installation by changing default passwords!"
}

# Show usage information
if [[ "$1" == "--help" ]] || [[ "$1" == "-h" ]]; then
    cat << 'HELP_EOF'
ScratchBird v0.6 Linux Installer

This is a self-extracting installer for ScratchBird Database Server.

Usage: sudo ./scratchbird-0.6.0-linux-installer.run

The installer will:
1. Create a system user 'scratchbird'
2. Install binaries to /opt/scratchbird
3. Configure systemd service
4. Set up firewall rules (if supported)
5. Create necessary directories and permissions

Requirements:
- Root privileges (use sudo)
- Linux with systemd (recommended)

For more information: https://github.com/dcalford/ScratchBird
HELP_EOF
    exit 0
fi

# Run main installation
main "$@"
exit 0

__ARCHIVE_BELOW__
INSTALLER_EOF

    chmod +x "$installer_script"
    log_success "Created installer script: $installer_script"
    
    # Append tar archive to installer
    log_info "Creating installer archive"
    cd "$INSTALLER_DIR/staging_linux"
    tar -czf - * >> "$installer_script"
    cd "$BUILD_ROOT"
    
    log_success "Linux installer created: $installer_script"
    log_info "Installer size: $(du -h "$installer_script" | cut -f1)"
}

# Function to create Windows installer
create_windows_installer() {
    log_info "Creating Windows installer package"
    
    local win_staging="$INSTALLER_DIR/staging_windows"
    rm -rf "$win_staging"
    mkdir -p "$win_staging"
    
    # Check if Windows binaries exist
    if [[ ! -d "gen/Release/scratchbird" ]]; then
        log_warning "Windows binaries not found. Build for Windows first with:"
        log_warning "  ./build_scratchbird_complete.sh Release windows"
        return 1
    fi
    
    # Copy Windows binaries
    cp -r gen/Release/scratchbird/* "$win_staging/"
    
    # Create Windows configuration
    cat > "$win_staging/scratchbird.conf" << 'EOF'
# ScratchBird Database Server Configuration - Windows
# Version 0.6.0

RemoteServicePort = 4050
RemoteServiceName = sb_fdb
DatabaseAccess = Full
DefaultDbCachePages = 2048
EOF
    
    # Create installer NSIS script (if available)
    if command -v makensis &> /dev/null; then
        create_nsis_installer "$win_staging"
    else
        # Create ZIP package as fallback
        log_warning "NSIS not available, creating ZIP package"
        cd "$INSTALLER_DIR"
        zip -r "scratchbird-${VERSION}-windows.zip" staging_windows/
        log_success "Windows ZIP package created: scratchbird-${VERSION}-windows.zip"
    fi
}

# Main function
main() {
    log_info "Creating ScratchBird v$VERSION installers"
    
    # Check if build exists
    if [[ ! -d "gen/Release/scratchbird" ]]; then
        log_error "ScratchBird build not found"
        log_info "Please run the build script first:"
        log_info "  ./build_scratchbird_complete.sh Release linux"
        exit 1
    fi
    
    create_install_files
    create_installer_script
    
    log_success "Linux installer creation completed!"
    log_info "Installer location: $INSTALLER_DIR/"
    log_info ""
    log_info "To test the installer:"
    log_info "  sudo $INSTALLER_DIR/scratchbird-${VERSION}-linux-installer.run"
}

# Run installer creation
main "$@"