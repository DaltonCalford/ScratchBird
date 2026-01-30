# Windows Installation (Portable)

Run ScratchBird from a portable ZIP archive without installation.

[Back to Installation Index](index.md) | [Back to Documentation Index](../index.md)

---

## When to Use Portable

The portable version is ideal when:
- You don't have administrator privileges
- You want to run from a USB drive
- You need multiple versions side-by-side
- You're evaluating ScratchBird

---

## Quick Start

```cmd
:: Extract ZIP
powershell -command "Expand-Archive scratchbird-0.9.0-beta0-win64.zip -DestinationPath C:\scratchbird"

:: Run server
cd C:\scratchbird
bin\sb_server --config conf\sb_server.conf --foreground
```

---

## Portable Package Contents (Feature Matrix)

The portable ZIP includes a minimal feature set:

| Component | Included | Notes |
| --- | --- | --- |
| Server (`sb_server`) | Yes | Core server binary |
| CLI tools | Yes | `sb_isql`, `sb_backup`, `sb_verify`, `sb_security` |
| Client libs | Yes | Included in `lib/` for local apps |
| Emulation listeners | Optional | If present, configured in `conf/` |
| GUI (ScratchRobin) | Optional | Only in GUI bundle |
| Drivers/ODBC/JDBC | Optional | Separate downloads |

Full installer feature matrix: see
[Installer Features + Config Generator](../../specifications/deployment/INSTALLER_FEATURES_AND_CONFIG_GENERATOR.md).

---

## Step-by-Step Setup

### 1. Download ZIP

Download from the releases page:
- [scratchbird-0.9.0-beta0-win64.zip](https://github.com/DaltonCalford/ScratchBird/releases/download/v0.9.0-beta0/scratchbird-0.9.0-beta0-win64.zip)

### 2. Extract Archive

Extract to any location:

```cmd
:: Using PowerShell
powershell -command "Expand-Archive scratchbird-0.9.0-beta0-win64.zip -DestinationPath C:\scratchbird"

:: Or right-click ZIP and select "Extract All..."
```

### 3. Directory Structure

```
scratchbird/
├── bin/
│   ├── sb_server.exe      # Database server
│   ├── sb_isql.exe        # SQL shell
│   ├── sb_verify.exe      # Verification tool
│   ├── sb_backup.exe      # Backup utility
│   └── sb_security.exe    # Security tool
├── conf/
│   └── sb_server.conf     # Configuration file
├── data/                  # Database files (created on first run)
├── logs/                  # Log files
└── README.txt
```

### 4. Configure

Edit `conf\sb_server.conf`:

```ini
[server]
mode = multi-database
data_dir = data

[network]
bind_address = 127.0.0.1    # Local only for portable use
native_port = 3092
pg_port = 5432

[logging]
file = logs\sb_server.log
```

### 5. Start Server

```cmd
cd C:\scratchbird
bin\sb_server --config conf\sb_server.conf --foreground
```

---

## Running from USB Drive

### Setup on USB

1. Extract ZIP to USB drive (e.g., `E:\scratchbird\`)
2. Edit configuration to use relative paths:

```ini
[server]
data_dir = data

[logging]
file = logs\sb_server.log
```

### Run from USB

```cmd
E:
cd \scratchbird
bin\sb_server --config conf\sb_server.conf --foreground
```

---

## Using Different Ports

For portable use, you may want non-standard ports to avoid conflicts:

```ini
[network]
native_port = 13092    # Instead of 3092
pg_port = 15432        # Instead of 5432
mysql_port = 13306     # Instead of 3306
fb_port = 13050        # Instead of 3050
```

Connect using the custom port:

```cmd
bin\sb_isql -H localhost -P 13092
```

---

## Starting and Stopping

### Start (Foreground)

```cmd
bin\sb_server --config conf\sb_server.conf --foreground
```

Press Ctrl+C to stop.

### Start (Background)

```cmd
start /B bin\sb_server --config conf\sb_server.conf
```

### Stop (Background)

Find and kill the process:

```cmd
tasklist | findstr sb_server
taskkill /F /PID <pid>
```

Or use PowerShell:

```powershell
Stop-Process -Name sb_server
```

---

## Creating a Batch File

Create `start_server.bat`:

```batch
@echo off
cd /d "%~dp0"
echo Starting ScratchBird Server...
bin\sb_server --config conf\sb_server.conf --foreground
pause
```

Create `stop_server.bat`:

```batch
@echo off
echo Stopping ScratchBird Server...
taskkill /F /IM sb_server.exe
pause
```

---

## Connecting

### Using sb_isql

```cmd
cd C:\scratchbird
bin\sb_isql -H localhost -P 3092
```

### Using psql

```cmd
psql -h localhost -p 5432 -U admin
```

---

## Upgrading

1. Stop the server
2. Backup your data directory
3. Extract new ZIP to a new folder
4. Copy your data directory to the new folder
5. Copy your configuration file
6. Start the new version

---

## Troubleshooting

### Antivirus Blocking

Some antivirus software may block executables from USB drives. You may need to:
- Add an exception for the ScratchBird folder
- Run from a local drive instead

### Permission Denied

Ensure you have write access to the data and logs directories:

```cmd
icacls data /grant %USERNAME%:F
icacls logs /grant %USERNAME%:F
```

### Port in Use

Check if the port is already in use:

```cmd
netstat -ano | findstr :3092
```

Change to a different port in the configuration if needed.

---

## Next Steps

1. [Configure the server](../configuration/sb_server.conf.md)
2. [Create your first database](../getting-started/first-database.md)
3. [Connect with a client](../getting-started/first-connection.md)
