#!/bin/bash

# uninstall_scratchbird.sh
# ScratchBird Database Engine - Linux Uninstall Script
# 
# This script completely removes ScratchBird from the system:
# - Stops and disables service
# - Removes files and directories
# - Removes system user and group
# - Cleans up configuration files
# - Optional: Preserves databases and logs

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Installation configuration (should match installer)
INSTALL_PREFIX="/opt/Scratchbird"
SCRATCHBIRD_USER="scratchbird"
SCRATCHBIRD_GROUP="scratchbird"
SERVICE_NAME="scratchbird"

# Uninstall options
PRESERVE_DATA=false
PRESERVE_CONFIG=false
REMOVE_USER=true

# Function to display header
show_header() {
    echo -e "${RED}"
    echo "================================================================="
    echo "    SCRATCHBIRD DATABASE ENGINE - LINUX UNINSTALLER"
    echo "================================================================="
    echo -e "${NC}"
    echo -e "${YELLOW}⚠️  WARNING: This will remove ScratchBird from your system${NC}"
    echo -e "${BLUE}Installation Location: ${INSTALL_PREFIX}${NC}"
    echo ""
}

# Function to parse command line options
parse_options() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --preserve-data)
                PRESERVE_DATA=true
                shift
                ;;
            --preserve-config)
                PRESERVE_CONFIG=true
                shift
                ;;
            --keep-user)
                REMOVE_USER=false
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                echo -e "${RED}Unknown option: $1${NC}"
                echo "Use --help for usage information"
                exit 1
                ;;
        esac
    done
}

# Function to show help
show_help() {
    echo "ScratchBird Uninstaller Usage:"
    echo ""
    echo "  sudo ./uninstall_scratchbird.sh [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --preserve-data    Keep databases and data files"
    echo "  --preserve-config  Keep configuration files"
    echo "  --keep-user       Don't remove system user/group"
    echo "  --help, -h        Show this help message"
    echo ""
    echo "Examples:"
    echo "  # Complete removal (default)"
    echo "  sudo ./uninstall_scratchbird.sh"
    echo ""
    echo "  # Remove but keep databases"
    echo "  sudo ./uninstall_scratchbird.sh --preserve-data"
    echo ""
    echo "  # Keep data and config for reinstall"
    echo "  sudo ./uninstall_scratchbird.sh --preserve-data --preserve-config"
}

# Function to check prerequisites
check_prerequisites() {
    echo -e "${YELLOW}Checking system state...${NC}"
    
    # Check if running as root
    if [ "$EUID" -ne 0 ]; then
        echo -e "${RED}❌ This script must be run as root (sudo)${NC}"
        echo "   Usage: sudo ./uninstall_scratchbird.sh"
        exit 1
    fi
    
    # Check if ScratchBird is installed
    if [ ! -d "$INSTALL_PREFIX" ]; then
        echo -e "${YELLOW}⚠️  ScratchBird installation not found at $INSTALL_PREFIX${NC}"
        echo "Nothing to uninstall."
        exit 0
    fi
    
    echo -e "${GREEN}✅ ScratchBird installation found${NC}"
}

# Function to get confirmation
get_confirmation() {
    echo -e "${YELLOW}Uninstall Summary:${NC}"
    echo ""
    echo -e "${BLUE}What will be removed:${NC}"
    echo "• ScratchBird service (stopped and disabled)"
    echo "• All files in $INSTALL_PREFIX"
    echo "• System environment configuration"
    echo "• Systemd service file"
    
    if [ "$REMOVE_USER" = true ]; then
        echo "• System user '$SCRATCHBIRD_USER' and group '$SCRATCHBIRD_GROUP'"
    fi
    
    echo ""
    echo -e "${GREEN}What will be preserved:${NC}"
    
    if [ "$PRESERVE_DATA" = true ]; then
        echo "• Database files (moved to /opt/scratchbird-backup-data/)"
    fi
    
    if [ "$PRESERVE_CONFIG" = true ]; then
        echo "• Configuration files (moved to /opt/scratchbird-backup-config/)"
    fi
    
    if [ "$PRESERVE_DATA" = false ] && [ "$PRESERVE_CONFIG" = false ]; then
        echo -e "${RED}• NOTHING - Complete removal${NC}"
    fi
    
    echo ""
    echo -e "${RED}⚠️  This action cannot be undone!${NC}"
    echo ""
    
    read -p "Are you sure you want to uninstall ScratchBird? (yes/NO): " -r
    if [[ ! $REPLY =~ ^[Yy][Ee][Ss]$ ]]; then
        echo "Uninstall cancelled."
        exit 0
    fi
    
    echo ""
    echo -e "${YELLOW}Starting uninstall process...${NC}"
}

# Function to stop and disable service
stop_service() {
    echo -e "${YELLOW}Stopping ScratchBird service...${NC}"
    
    # Check if service exists and is active
    if systemctl is-active --quiet "$SERVICE_NAME" 2>/dev/null; then
        echo "   Stopping service..."
        systemctl stop "$SERVICE_NAME"
        echo -e "${GREEN}✅ Service stopped${NC}"
    else
        echo -e "${GREEN}✅ Service not running${NC}"
    fi
    
    # Disable service if it exists
    if systemctl is-enabled --quiet "$SERVICE_NAME" 2>/dev/null; then
        echo "   Disabling service..."
        systemctl disable "$SERVICE_NAME"
        echo -e "${GREEN}✅ Service disabled${NC}"
    else
        echo -e "${GREEN}✅ Service not enabled${NC}"
    fi
    
    # Remove service file
    if [ -f "/etc/systemd/system/${SERVICE_NAME}.service" ]; then
        rm -f "/etc/systemd/system/${SERVICE_NAME}.service"
        systemctl daemon-reload
        echo -e "${GREEN}✅ Service file removed${NC}"
    fi
}

# Function to backup data if requested
backup_data() {
    if [ "$PRESERVE_DATA" = true ]; then
        echo -e "${YELLOW}Backing up database files...${NC}"
        
        local backup_dir="/opt/scratchbird-backup-data-$(date +%Y%m%d_%H%M%S)"
        mkdir -p "$backup_dir"
        
        # Copy data directories
        if [ -d "$INSTALL_PREFIX/data" ]; then
            cp -r "$INSTALL_PREFIX/data" "$backup_dir/"
            echo -e "${GREEN}✅ Data files backed up to $backup_dir${NC}"
        fi
        
        if [ -d "$INSTALL_PREFIX/security" ]; then
            cp -r "$INSTALL_PREFIX/security" "$backup_dir/"
            echo -e "${GREEN}✅ Security database backed up${NC}"
        fi
        
        if [ -d "$INSTALL_PREFIX/log" ]; then
            cp -r "$INSTALL_PREFIX/log" "$backup_dir/"
            echo -e "${GREEN}✅ Log files backed up${NC}"
        fi
        
        # Create restore instructions
        cat > "$backup_dir/RESTORE_INSTRUCTIONS.md" << EOF
# ScratchBird Data Backup

This directory contains backed up data from ScratchBird uninstallation.

## Contents:
- **data/**: Database files and user data
- **security/**: Security database
- **log/**: Log files

## To Restore:
1. Reinstall ScratchBird
2. Stop the ScratchBird service: \`sudo systemctl stop scratchbird\`
3. Copy contents back to /opt/Scratchbird/
4. Set proper ownership: \`sudo chown -R scratchbird:scratchbird /opt/Scratchbird/\`
5. Restart service: \`sudo systemctl start scratchbird\`

## Backup Created: $(date)
EOF
        
        echo -e "${CYAN}📋 Restore instructions: $backup_dir/RESTORE_INSTRUCTIONS.md${NC}"
    fi
}

# Function to backup configuration if requested
backup_config() {
    if [ "$PRESERVE_CONFIG" = true ]; then
        echo -e "${YELLOW}Backing up configuration files...${NC}"
        
        local backup_dir="/opt/scratchbird-backup-config-$(date +%Y%m%d_%H%M%S)"
        mkdir -p "$backup_dir"
        
        # Copy configuration directory
        if [ -d "$INSTALL_PREFIX/conf" ]; then
            cp -r "$INSTALL_PREFIX/conf" "$backup_dir/"
            echo -e "${GREEN}✅ Configuration files backed up to $backup_dir${NC}"
        fi
        
        # Create restore instructions
        cat > "$backup_dir/RESTORE_INSTRUCTIONS.md" << EOF
# ScratchBird Configuration Backup

This directory contains backed up configuration from ScratchBird uninstallation.

## Contents:
- **conf/**: All configuration files

## To Restore:
1. Reinstall ScratchBird
2. Copy configuration files back: \`sudo cp -r conf/* /opt/Scratchbird/conf/\`
3. Set proper ownership: \`sudo chown -R scratchbird:scratchbird /opt/Scratchbird/conf/\`
4. Restart service: \`sudo systemctl restart scratchbird\`

## Backup Created: $(date)
EOF
        
        echo -e "${CYAN}📋 Restore instructions: $backup_dir/RESTORE_INSTRUCTIONS.md${NC}"
    fi
}

# Function to remove files
remove_files() {
    echo -e "${YELLOW}Removing ScratchBird files...${NC}"
    
    # Remove main installation directory
    if [ -d "$INSTALL_PREFIX" ]; then
        rm -rf "$INSTALL_PREFIX"
        echo -e "${GREEN}✅ Installation directory removed${NC}"
    fi
    
    # Remove environment configuration
    if [ -f "/etc/environment.d/scratchbird.conf" ]; then
        rm -f "/etc/environment.d/scratchbird.conf"
        echo -e "${GREEN}✅ Environment configuration removed${NC}"
    fi
    
    if [ -f "/etc/profile.d/scratchbird.sh" ]; then
        rm -f "/etc/profile.d/scratchbird.sh"
        echo -e "${GREEN}✅ Profile configuration removed${NC}"
    fi
    
    # Remove desktop entry
    if [ -f "/usr/share/applications/scratchbird.desktop" ]; then
        rm -f "/usr/share/applications/scratchbird.desktop"
        echo -e "${GREEN}✅ Desktop entry removed${NC}"
    fi
    
    # Remove any remaining systemd files
    if [ -f "/etc/systemd/system/${SERVICE_NAME}.service" ]; then
        rm -f "/etc/systemd/system/${SERVICE_NAME}.service"
        systemctl daemon-reload
        echo -e "${GREEN}✅ Systemd service file removed${NC}"
    fi
}

# Function to remove system user and group
remove_system_user() {
    if [ "$REMOVE_USER" = true ]; then
        echo -e "${YELLOW}Removing system user and group...${NC}"
        
        # Remove user if it exists
        if getent passwd "$SCRATCHBIRD_USER" > /dev/null 2>&1; then
            userdel "$SCRATCHBIRD_USER"
            echo -e "${GREEN}✅ User '$SCRATCHBIRD_USER' removed${NC}"
        else
            echo -e "${GREEN}✅ User '$SCRATCHBIRD_USER' not found${NC}"
        fi
        
        # Remove group if it exists
        if getent group "$SCRATCHBIRD_GROUP" > /dev/null 2>&1; then
            groupdel "$SCRATCHBIRD_GROUP"
            echo -e "${GREEN}✅ Group '$SCRATCHBIRD_GROUP' removed${NC}"
        else
            echo -e "${GREEN}✅ Group '$SCRATCHBIRD_GROUP' not found${NC}"
        fi
    else
        echo -e "${YELLOW}⚠️  Keeping system user and group${NC}"
    fi
}

# Function to clean up any remaining processes
cleanup_processes() {
    echo -e "${YELLOW}Checking for running ScratchBird processes...${NC}"
    
    # Kill any remaining processes
    local processes=$(pgrep -f "scratchbird\|sb_" 2>/dev/null || true)
    
    if [ -n "$processes" ]; then
        echo "   Found running processes, terminating..."
        pkill -TERM -f "scratchbird\|sb_" 2>/dev/null || true
        sleep 2
        pkill -KILL -f "scratchbird\|sb_" 2>/dev/null || true
        echo -e "${GREEN}✅ Processes terminated${NC}"
    else
        echo -e "${GREEN}✅ No running processes found${NC}"
    fi
}

# Function to verify removal
verify_removal() {
    echo -e "${YELLOW}Verifying removal...${NC}"
    
    local issues=()
    
    # Check if installation directory exists
    if [ -d "$INSTALL_PREFIX" ]; then
        issues+=("Installation directory still exists: $INSTALL_PREFIX")
    fi
    
    # Check if service file exists
    if [ -f "/etc/systemd/system/${SERVICE_NAME}.service" ]; then
        issues+=("Service file still exists")
    fi
    
    # Check if user exists (only if we tried to remove it)
    if [ "$REMOVE_USER" = true ] && getent passwd "$SCRATCHBIRD_USER" > /dev/null 2>&1; then
        issues+=("System user still exists: $SCRATCHBIRD_USER")
    fi
    
    # Check if group exists (only if we tried to remove it)
    if [ "$REMOVE_USER" = true ] && getent group "$SCRATCHBIRD_GROUP" > /dev/null 2>&1; then
        issues+=("System group still exists: $SCRATCHBIRD_GROUP")
    fi
    
    if [ ${#issues[@]} -eq 0 ]; then
        echo -e "${GREEN}✅ Removal verification passed${NC}"
    else
        echo -e "${YELLOW}⚠️  Some items may need manual cleanup:${NC}"
        for issue in "${issues[@]}"; do
            echo "   • $issue"
        done
    fi
}

# Function to display final summary
show_final_summary() {
    echo -e "${GREEN}"
    echo "================================================================="
    echo "    SCRATCHBIRD UNINSTALLATION COMPLETED"
    echo "================================================================="
    echo -e "${NC}"
    
    echo -e "${CYAN}Uninstall Summary:${NC}"
    echo "   ✅ ScratchBird service stopped and disabled"
    echo "   ✅ Installation files removed from $INSTALL_PREFIX"
    echo "   ✅ System configuration cleaned up"
    
    if [ "$REMOVE_USER" = true ]; then
        echo "   ✅ System user and group removed"
    else
        echo "   ⚠️  System user and group preserved"
    fi
    
    echo ""
    
    if [ "$PRESERVE_DATA" = true ] || [ "$PRESERVE_CONFIG" = true ]; then
        echo -e "${BLUE}Backup Information:${NC}"
        
        if [ "$PRESERVE_DATA" = true ]; then
            echo "   📁 Data backed up to: /opt/scratchbird-backup-data-*"
        fi
        
        if [ "$PRESERVE_CONFIG" = true ]; then
            echo "   📁 Config backed up to: /opt/scratchbird-backup-config-*"
        fi
        
        echo "   📋 See RESTORE_INSTRUCTIONS.md in backup directories"
        echo ""
    fi
    
    echo -e "${YELLOW}To complete the cleanup:${NC}"
    echo "   • Restart your shell session (or run: source /etc/profile)"
    echo "   • Restart services that may depend on ScratchBird"
    echo ""
    
    if [ "$PRESERVE_DATA" = true ] || [ "$PRESERVE_CONFIG" = true ]; then
        echo -e "${CYAN}To reinstall ScratchBird with preserved data:${NC}"
        echo "   1. Run the installer: sudo ./install_scratchbird.sh"
        echo "   2. Follow restore instructions in backup directories"
        echo ""
    fi
    
    echo -e "${GREEN}Thank you for using ScratchBird! 🚀${NC}"
    echo ""
}

# Main uninstall function
main() {
    parse_options "$@"
    show_header
    check_prerequisites
    get_confirmation
    
    # Execute uninstall steps
    stop_service
    backup_data
    backup_config
    cleanup_processes
    remove_files
    remove_system_user
    verify_removal
    
    show_final_summary
}

# Run main function
main "$@"