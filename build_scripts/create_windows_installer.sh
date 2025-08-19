#!/bin/bash

# ScratchBird v0.6 Windows Installer Creator
# Creates Windows installer packages (NSIS and ZIP formats)
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

# Function to create Windows installation files
create_windows_files() {
    log_info "Creating Windows installation file structure"
    
    local win_staging="$INSTALLER_DIR/staging_windows"
    rm -rf "$win_staging"
    mkdir -p "$win_staging"
    
    # Check if Windows binaries exist
    if [[ ! -d "gen/Release/scratchbird" ]]; then
        log_error "Windows binaries not found"
        log_info "Build for Windows first with:"
        log_info "  ./build_scratchbird_complete.sh Release windows"
        exit 1
    fi
    
    # Copy Windows binaries
    cp -r gen/Release/scratchbird/* "$win_staging/"
    log_success "Copied Windows binaries"
    
    # Create Windows-specific directories
    mkdir -p "$win_staging/config"
    mkdir -p "$win_staging/databases"
    mkdir -p "$win_staging/log"
    mkdir -p "$win_staging/doc"
    
    # Create Windows configuration files
    cat > "$win_staging/config/scratchbird.conf" << 'EOF'
# ScratchBird Database Server Configuration - Windows
# Version 0.6.0

# Network Configuration
RemoteServicePort = 4050
RemoteServiceName = sb_fdb
RemoteAccess = true

# File Locations
RootDirectory = C:\Program Files\ScratchBird
DatabaseAccess = Full
ExternalFileAccess = Restrict C:\Program Files\ScratchBird\databases

# Security
AuthServer = Legacy_Auth, Srp256, Win_Sspi
UserManager = Legacy_UserManager
SecurityDatabase = $(this_folder)\databases\security6.fdb

# Performance Settings  
DefaultDbCachePages = 2048
TempCacheLimit = 67108864
MaxUnflushedWrites = 100
MaxUnflushedWriteTime = 5

# Character Sets
DefaultCharacterSet = UTF8
ConnectionCharacterSet = UTF8

# Logging
LogFileSize = 1048576
MaxLogFiles = 5
EOF

    cat > "$win_staging/config/databases.conf" << 'EOF'
# ScratchBird Database Configuration - Windows
# Version 0.6.0

# Security Database
security6.fdb = $(this_folder)\databases\security6.fdb
{
    SecurityDatabase = true
}

# Sample Database
employee.fdb = $(this_folder)\databases\employee.fdb
{
    ReadOnly = true
}

# Template for new databases
# mydatabase.fdb = C:\MyDatabases\mydatabase.fdb
# {
#     DefaultCharSet = UTF8
# }
EOF

    # Create Windows service registration script
    cat > "$win_staging/install_service.bat" << 'EOF'
@echo off
REM ScratchBird Windows Service Installation Script

echo Installing ScratchBird Database Service...

REM Check for administrator privileges
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo This script must be run as Administrator.
    echo Right-click and select "Run as administrator"
    pause
    exit /b 1
)

REM Set installation directory
set INSTALL_DIR=%~dp0
set SERVICE_NAME=ScratchBird
set SERVICE_PORT=4050

echo Installation Directory: %INSTALL_DIR%

REM Create Windows service
sc create %SERVICE_NAME% binPath= "\"%INSTALL_DIR%bin\sb_fbserver.exe\" -a" DisplayName= "ScratchBird Database Server" start= auto

if %errorlevel% neq 0 (
    echo Failed to create service. Service may already exist.
    echo To reinstall, run uninstall_service.bat first.
) else (
    echo Service created successfully.
)

REM Configure firewall
echo Configuring Windows Firewall...
netsh advfirewall firewall add rule name="ScratchBird Database" dir=in action=allow protocol=TCP localport=%SERVICE_PORT%

if %errorlevel% neq 0 (
    echo Warning: Failed to configure firewall. You may need to manually allow port %SERVICE_PORT%.
) else (
    echo Firewall configured for port %SERVICE_PORT%.
)

REM Start service
echo Starting ScratchBird service...
sc start %SERVICE_NAME%

if %errorlevel% neq 0 (
    echo Warning: Failed to start service automatically.
    echo You can start it manually from Services or run: sc start %SERVICE_NAME%
) else (
    echo ScratchBird service started successfully.
)

echo.
echo Installation completed!
echo.
echo Service Name: %SERVICE_NAME%
echo Service Port: %SERVICE_PORT%
echo Configuration: %INSTALL_DIR%config\scratchbird.conf
echo.
echo To connect: sb_isql localhost/%SERVICE_PORT%:employee
echo.
pause
EOF

    # Create Windows service uninstallation script
    cat > "$win_staging/uninstall_service.bat" << 'EOF'
@echo off
REM ScratchBird Windows Service Uninstallation Script

echo Uninstalling ScratchBird Database Service...

REM Check for administrator privileges
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo This script must be run as Administrator.
    echo Right-click and select "Run as administrator"
    pause
    exit /b 1
)

set SERVICE_NAME=ScratchBird

REM Stop service
echo Stopping ScratchBird service...
sc stop %SERVICE_NAME%

REM Wait a moment for service to stop
timeout /t 3 /nobreak > nul

REM Delete service
echo Removing ScratchBird service...
sc delete %SERVICE_NAME%

if %errorlevel% neq 0 (
    echo Warning: Failed to remove service completely.
) else (
    echo Service removed successfully.
)

REM Remove firewall rule
echo Removing firewall rule...
netsh advfirewall firewall delete rule name="ScratchBird Database"

echo.
echo Uninstallation completed!
echo The ScratchBird files remain in this directory.
echo You can safely delete this folder if no longer needed.
echo.
pause
EOF

    # Create README for Windows
    cat > "$win_staging/README.txt" << 'EOF'
ScratchBird Database Server v0.6.0 for Windows
===============================================

INSTALLATION INSTRUCTIONS:
1. Extract all files to a directory (e.g., C:\Program Files\ScratchBird)
2. Right-click "install_service.bat" and select "Run as administrator"
3. The service will be installed and started automatically

CONNECTING TO THE DATABASE:
- Default port: 4050
- Service name: sb_fdb
- Example connection: sb_isql localhost/4050:employee

COMMAND LINE TOOLS:
- sb_isql.exe   - Interactive SQL shell
- sb_gbak.exe   - Backup and restore utility
- sb_gfix.exe   - Database maintenance tool
- sb_gsec.exe   - User security management
- sb_gstat.exe  - Database statistics

CONFIGURATION:
- Main config: config\scratchbird.conf
- Database config: config\databases.conf

UNINSTALLING:
1. Right-click "uninstall_service.bat" and select "Run as administrator"
2. Delete the installation directory

FIREWALL:
The installer automatically configures Windows Firewall to allow
connections on port 4050. If you have third-party firewall software,
you may need to manually allow this port.

SECURITY:
Please change default passwords and review security settings in the
configuration files before connecting to the database from other machines.

For more information: https://github.com/dcalford/ScratchBird

Support: Report issues at https://github.com/dcalford/ScratchBird/issues
EOF

    log_success "Created Windows installation files"
}

# Function to create NSIS installer script
create_nsis_installer() {
    local win_staging="$1"
    log_info "Creating NSIS installer script"
    
    cat > "$INSTALLER_DIR/scratchbird-windows.nsi" << 'NSIS_EOF'
; ScratchBird v0.6.0 Windows Installer
; NSIS Script for creating Windows installer

!define APP_NAME "ScratchBird"
!define APP_VERSION "0.6.0"
!define APP_PUBLISHER "ScratchBird Development Team"
!define APP_URL "https://github.com/dcalford/ScratchBird"
!define APP_DESCRIPTION "ScratchBird Database Server"

; Modern UI
!include "MUI2.nsh"
!include "Sections.nsh"

; General settings
Name "${APP_NAME} ${APP_VERSION}"
OutFile "scratchbird-${APP_VERSION}-windows-installer.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "Software\${APP_NAME}" "InstallDir"
RequestExecutionLevel admin

; Interface settings
!define MUI_ABORTWARNING
!define MUI_ICON "scratchbird.ico"
!define MUI_UNICON "scratchbird.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME

; License page
!insertmacro MUI_PAGE_LICENSE "LICENSE"

; Directory page
!insertmacro MUI_PAGE_DIRECTORY

; Components page
!insertmacro MUI_PAGE_COMPONENTS

; Install files page
!insertmacro MUI_PAGE_INSTFILES

; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\install_service.bat"
!define MUI_FINISHPAGE_RUN_TEXT "Install and start ScratchBird service"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Languages
!insertmacro MUI_LANGUAGE "English"

; Version info
VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey "ProductName" "${APP_NAME}"
VIAddVersionKey "ProductVersion" "${APP_VERSION}"
VIAddVersionKey "CompanyName" "${APP_PUBLISHER}"
VIAddVersionKey "FileDescription" "${APP_DESCRIPTION}"
VIAddVersionKey "FileVersion" "${APP_VERSION}"

; Installer sections
Section "ScratchBird Core" SecCore
    SectionIn RO
    
    SetOutPath "$INSTDIR"
    
    ; Copy all files from staging
    File /r "staging_windows\*"
    
    ; Create shortcuts
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortCut "$SMPROGRAMS\${APP_NAME}\ScratchBird ISQL.lnk" "$INSTDIR\bin\sb_isql.exe"
    CreateShortCut "$SMPROGRAMS\${APP_NAME}\Install Service.lnk" "$INSTDIR\install_service.bat"
    CreateShortCut "$SMPROGRAMS\${APP_NAME}\Uninstall Service.lnk" "$INSTDIR\uninstall_service.bat"
    CreateShortCut "$SMPROGRAMS\${APP_NAME}\Configuration.lnk" "$INSTDIR\config"
    CreateShortCut "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe"
    
    ; Registry entries
    WriteRegStr HKLM "Software\${APP_NAME}" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\${APP_NAME}" "Version" "${APP_VERSION}"
    
    ; Uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"
    
    ; Add/Remove Programs entry
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayName" "${APP_NAME} ${APP_VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "Publisher" "${APP_PUBLISHER}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "URLInfoAbout" "${APP_URL}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayVersion" "${APP_VERSION}"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "NoRepair" 1
SectionEnd

Section "Windows Service" SecService
    ; Service will be installed when user runs install_service.bat
    ; This section just adds the option to run it automatically
SectionEnd

Section "Desktop Shortcuts" SecDesktop
    CreateShortCut "$DESKTOP\ScratchBird ISQL.lnk" "$INSTDIR\bin\sb_isql.exe"
SectionEnd

; Component descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecCore} "Core ScratchBird database files and tools (required)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecService} "Windows service installation scripts"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "Desktop shortcuts for quick access"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; Uninstaller section
Section "Uninstall"
    ; Stop and remove service if it exists
    ExecWait '"$INSTDIR\uninstall_service.bat"'
    
    ; Remove files
    RMDir /r "$INSTDIR\bin"
    RMDir /r "$INSTDIR\lib"
    RMDir /r "$INSTDIR\config"
    RMDir /r "$INSTDIR\databases"
    RMDir /r "$INSTDIR\doc"
    Delete "$INSTDIR\*.*"
    RMDir "$INSTDIR"
    
    ; Remove shortcuts
    RMDir /r "$SMPROGRAMS\${APP_NAME}"
    Delete "$DESKTOP\ScratchBird ISQL.lnk"
    
    ; Remove registry entries
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
    DeleteRegKey HKLM "Software\${APP_NAME}"
SectionEnd
NSIS_EOF

    if command -v makensis &> /dev/null; then
        log_info "Building NSIS installer"
        cd "$INSTALLER_DIR"
        makensis scratchbird-windows.nsi
        if [[ $? -eq 0 ]]; then
            log_success "NSIS installer created successfully"
        else
            log_error "NSIS installer build failed"
        fi
        cd "$BUILD_ROOT"
    else
        log_warning "NSIS not installed, installer script created but not compiled"
        log_info "To build installer: install NSIS and run 'makensis scratchbird-windows.nsi'"
    fi
}

# Function to create ZIP package
create_zip_package() {
    log_info "Creating Windows ZIP package"
    
    local zip_name="scratchbird-${VERSION}-windows-portable.zip"
    cd "$INSTALLER_DIR"
    
    if command -v zip &> /dev/null; then
        zip -r "$zip_name" staging_windows/
        log_success "ZIP package created: $zip_name"
    else
        log_warning "ZIP command not available"
        log_info "Manually create ZIP from staging_windows/ directory"
    fi
    
    cd "$BUILD_ROOT"
}

# Function to create self-extracting archive
create_self_extracting() {
    log_info "Creating self-extracting Windows installer"
    
    local sfx_script="$INSTALLER_DIR/scratchbird-${VERSION}-windows-sfx.exe"
    
    # Create SFX header script
    cat > "$INSTALLER_DIR/sfx_header.bat" << 'SFX_EOF'
@echo off
setlocal

echo ScratchBird v0.6.0 Windows Installer
echo =====================================

REM Check for administrator privileges
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo This installer requires administrator privileges.
    echo Please right-click and select "Run as administrator"
    pause
    exit /b 1
)

REM Create temporary directory
set TEMP_DIR=%TEMP%\scratchbird_install_%RANDOM%
mkdir "%TEMP_DIR%"

echo Extracting files...

REM Extract embedded archive to temp directory
powershell -command "& {$archive = Get-Content '%~f0' -Raw; $start = $archive.IndexOf('__ARCHIVE_START__') + '__ARCHIVE_START__'.Length; $data = $archive.Substring($start); $bytes = [System.Convert]::FromBase64String($data); [System.IO.File]::WriteAllBytes('%TEMP_DIR%\archive.zip', $bytes)}"

REM Extract ZIP
powershell -command "Expand-Archive -Path '%TEMP_DIR%\archive.zip' -DestinationPath '%TEMP_DIR%\extracted'"

REM Default installation directory
set INSTALL_DIR=%ProgramFiles%\ScratchBird

echo.
set /p INSTALL_DIR="Installation directory [%INSTALL_DIR%]: "

REM Create installation directory
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"

REM Copy files
echo Installing files to %INSTALL_DIR%...
xcopy /s /e /q "%TEMP_DIR%\extracted\staging_windows\*" "%INSTALL_DIR%\"

REM Run service installation
echo.
choice /c YN /m "Install Windows service now"
if errorlevel 2 goto skip_service
call "%INSTALL_DIR%\install_service.bat"

:skip_service
REM Cleanup
rmdir /s /q "%TEMP_DIR%"

echo.
echo Installation completed successfully!
echo ScratchBird is installed in: %INSTALL_DIR%
echo.
pause
exit /b 0

__ARCHIVE_START__
SFX_EOF

    # Convert ZIP to base64 and append to script
    if [[ -f "$INSTALLER_DIR/scratchbird-${VERSION}-windows-portable.zip" ]]; then
        cat "$INSTALLER_DIR/sfx_header.bat" > "$sfx_script"
        base64 -w 0 "$INSTALLER_DIR/scratchbird-${VERSION}-windows-portable.zip" >> "$sfx_script"
        
        # Make it executable
        chmod +x "$sfx_script"
        
        log_success "Self-extracting installer created: $sfx_script"
    else
        log_warning "ZIP package not found, cannot create self-extracting archive"
    fi
}

# Main function
main() {
    log_info "Creating ScratchBird v$VERSION Windows installer"
    
    # Check if Windows build exists
    if [[ ! -d "gen/Release/scratchbird" ]]; then
        log_error "Windows build not found"
        log_info "Build for Windows first with:"
        log_info "  ./build_scratchbird_complete.sh Release windows"
        exit 1
    fi
    
    create_windows_files
    create_zip_package
    create_nsis_installer "$INSTALLER_DIR/staging_windows"
    create_self_extracting
    
    log_success "Windows installer creation completed!"
    log_info ""
    log_info "Created installers:"
    find "$INSTALLER_DIR" -name "*windows*" -type f -exec basename {} \; | sort
    log_info ""
    log_info "Installer location: $INSTALLER_DIR/"
}

# Show help
if [[ "$1" == "--help" ]] || [[ "$1" == "-h" ]]; then
    cat << 'HELP_EOF'
ScratchBird v0.6 Windows Installer Creator

This script creates Windows installer packages in multiple formats:
1. ZIP package (portable)
2. NSIS installer (if NSIS is available)
3. Self-extracting archive

Prerequisites:
- Windows build of ScratchBird (run build script with 'windows' platform)
- Optional: NSIS for creating professional installer

Usage: ./create_windows_installer.sh

The script will create installer packages in the 'installers/' directory.
HELP_EOF
    exit 0
fi

# Run installer creation
main "$@"