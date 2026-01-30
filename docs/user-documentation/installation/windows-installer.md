# Windows Installation (Installer)

Install ScratchBird on Windows using the NSIS installer.

[Back to Installation Index](index.md) | [Back to Documentation Index](../index.md)

---

## Supported Windows Versions

- Windows 10 (x64)
- Windows 11 (x64)
- Windows Server 2019 (x64)
- Windows Server 2022 (x64)

---

## Quick Install

1. Download `scratchbird-0.9.0-beta0-win64-setup.exe`
2. Run the installer
3. Follow the installation wizard
4. Start ScratchBird from the Start Menu

---

## Step-by-Step Installation

### 1. Download Installer

Download from the releases page:
- [scratchbird-0.9.0-beta0-win64-setup.exe](https://github.com/DaltonCalford/ScratchBird/releases/download/v0.9.0-beta0/scratchbird-0.9.0-beta0-win64-setup.exe)

### 2. Run Installer

Right-click the installer and select "Run as administrator" (recommended).

### 3. Installation Wizard

**Welcome Screen:**
- Read the beta warning
- Click "Next" to continue

**License Agreement:**
- Read the IDPL 1.0 license
- Accept to continue

**Design Goals:**
- Read about ScratchBird's philosophy
- Click "Next" to continue

**Installation Directory:**
- Default: `C:\Program Files\ScratchBird`
- Change if needed
- Click "Next"

**Data Directory:**
- Default: `C:\ProgramData\ScratchBird`
- This is where databases are stored
- Click "Next"

**Components:**
- Server (required)
- Command-line tools (recommended)
- Documentation (optional)
- Click "Next"

**Feature IDs (MSI/NSIS component names):**

| Feature ID | Description |
| --- | --- |
| `CoreServer` | Server + service |
| `Tools` | CLI tools (`sb_isql`, `sb_admin`, `sb_backup`, `sb_verify`, `sb_security`) |
| `ClientLibs` | Native client libraries |
| `DevHeaders` | Headers/SDK for embedding |
| `ScratchRobin` | ScratchRobin GUI |
| `EmulationPG` | PostgreSQL listener/parser |
| `EmulationMySQL` | MySQL listener/parser |
| `EmulationFirebird` | Firebird listener/parser |
| `ODBCDriver` | ODBC driver |
| `JDBCDriver` | JDBC driver |
| `DriverGo` | Go driver |
| `DriverPython` | Python driver |
| `DriverNode` | Node.js driver |
| `DriverDotNet` | .NET driver |
| `DriverJDBC` | JDBC driver |
| `DriverPHP` | PHP driver |
| `DriverRuby` | Ruby driver |
| `DriverR` | R driver |
| `DriverRust` | Rust driver |
| `DriverPascal` | Pascal/Delphi driver |
| `SignedRuntime` | Signed runtime bundle |

Full installer feature matrix: see
[Installer Features + Config Generator](../../specifications/deployment/INSTALLER_FEATURES_AND_CONFIG_GENERATOR.md).

**Start Menu:**
- Create Start Menu shortcuts
- Click "Install"

### 4. Complete Installation

- Wait for files to be copied
- Optionally start the server now
- Click "Finish"

---

## Directory Layout

After installation:

| Path | Description |
|------|-------------|
| `C:\Program Files\ScratchBird\bin\` | Executables |
| `C:\Program Files\ScratchBird\lib\` | Libraries |
| `C:\Program Files\ScratchBird\doc\` | Documentation |
| `C:\ProgramData\ScratchBird\` | Databases and config |
| `C:\ProgramData\ScratchBird\data\` | Database files |
| `C:\ProgramData\ScratchBird\logs\` | Log files |
| `C:\ProgramData\ScratchBird\sb_server.conf` | Configuration |

---

## Running as a Windows Service

### Install Service

Open Command Prompt as Administrator:

```cmd
cd "C:\Program Files\ScratchBird\bin"
sb_server --install-service
```

### Start Service

```cmd
net start ScratchBird
```

Or use Services Manager (services.msc):
1. Open Services
2. Find "ScratchBird Database Server"
3. Right-click and select "Start"

### Configure Automatic Start

1. Open Services (services.msc)
2. Find "ScratchBird Database Server"
3. Right-click and select "Properties"
4. Set "Startup type" to "Automatic"
5. Click "OK"

### Stop Service

```cmd
net stop ScratchBird
```

### Uninstall Service

```cmd
net stop ScratchBird
sb_server --uninstall-service
```

---

## Running Manually

Open Command Prompt:

```cmd
cd "C:\Program Files\ScratchBird\bin"
sb_server --config "C:\ProgramData\ScratchBird\sb_server.conf"
```

For foreground mode (shows output):

```cmd
sb_server --config "C:\ProgramData\ScratchBird\sb_server.conf" --foreground
```

---

## Configuration

Edit configuration file:

```cmd
notepad "C:\ProgramData\ScratchBird\sb_server.conf"
```

Key settings for Windows:

```ini
[server]
mode = multi-database
data_dir = C:\ProgramData\ScratchBird\data

[network]
bind_address = 0.0.0.0
native_port = 3092
pg_port = 5432
mysql_port = 3306
fb_port = 3050

[logging]
file = C:\ProgramData\ScratchBird\logs\sb_server.log
```

You can also run the post-install configuration wizard to add/remove features,
set ports, and tune performance:

```cmd
sb_setup --interactive
```

---

## Firewall Configuration

The installer can automatically create firewall rules. If you need to add them manually:

### Using Windows Firewall GUI

1. Open "Windows Defender Firewall with Advanced Security"
2. Click "Inbound Rules" → "New Rule..."
3. Select "Port" → Next
4. Select "TCP" and enter port (3092, 5432, 3306, or 3050)
5. Select "Allow the connection"
6. Apply to Domain, Private, and Public (as appropriate)
7. Name the rule (e.g., "ScratchBird Native Protocol")

### Using Command Line

Open Command Prompt as Administrator:

```cmd
:: ScratchBird Native Protocol
netsh advfirewall firewall add rule name="ScratchBird Native" dir=in action=allow protocol=tcp localport=3092

:: PostgreSQL Protocol
netsh advfirewall firewall add rule name="ScratchBird PostgreSQL" dir=in action=allow protocol=tcp localport=5432

:: MySQL Protocol
netsh advfirewall firewall add rule name="ScratchBird MySQL" dir=in action=allow protocol=tcp localport=3306

:: Firebird Protocol
netsh advfirewall firewall add rule name="ScratchBird Firebird" dir=in action=allow protocol=tcp localport=3050
```

---

## Verify Installation

### Check Service Status

```cmd
sc query ScratchBird
```

### View Logs

```cmd
type "C:\ProgramData\ScratchBird\logs\sb_server.log"
```

Or use PowerShell:

```powershell
Get-Content "C:\ProgramData\ScratchBird\logs\sb_server.log" -Tail 50
```

### Test Connection

```cmd
cd "C:\Program Files\ScratchBird\bin"
sb_isql -H localhost -P 3092
```

---

## Start Menu Shortcuts

After installation:

- **ScratchBird Server** - Start server (if not running as service)
- **ScratchBird SQL Shell** - Open sb_isql
- **ScratchBird Configuration** - Open configuration file
- **ScratchBird Documentation** - Open documentation
- **Uninstall ScratchBird** - Remove ScratchBird

---

## Environment Variables

Add to PATH for command-line access:

1. Open System Properties → Advanced → Environment Variables
2. Under "System variables", find "Path"
3. Click "Edit"
4. Add: `C:\Program Files\ScratchBird\bin`
5. Click "OK"

Or via command line:

```cmd
setx PATH "%PATH%;C:\Program Files\ScratchBird\bin" /M
```

---

## Upgrading

1. Stop the service: `net stop ScratchBird`
2. Backup configuration: Copy `C:\ProgramData\ScratchBird\sb_server.conf`
3. Run the new installer
4. Restore configuration if overwritten
5. Start the service: `net start ScratchBird`

---

## Uninstalling

### Via Control Panel

1. Open "Settings" → "Apps" → "Apps & features"
2. Find "ScratchBird Database Engine"
3. Click "Uninstall"
4. Follow the prompts

### Via Command Line

```cmd
"C:\Program Files\ScratchBird\uninstall.exe"
```

### Complete Removal

After uninstalling, manually remove data if desired:

```cmd
rmdir /s /q "C:\ProgramData\ScratchBird"
```

**Warning:** This permanently deletes all databases!

---

## Troubleshooting

### Service Won't Start

1. Check Windows Event Viewer:
   - Open Event Viewer
   - Navigate to Windows Logs → Application
   - Look for ScratchBird entries

2. Check log file:
   ```cmd
   type "C:\ProgramData\ScratchBird\logs\sb_server.log"
   ```

3. Try running manually to see errors:
   ```cmd
   sb_server --config "C:\ProgramData\ScratchBird\sb_server.conf" --foreground
   ```

### Port Already in Use

Check what's using the port:

```cmd
netstat -ano | findstr :3092
netstat -ano | findstr :5432
```

### Permission Denied

Ensure the service account has access to:
- Data directory
- Log directory
- Configuration file

### Missing DLL

ScratchBird is statically linked and should not require additional DLLs. If you see DLL errors, try:
1. Reinstalling the Visual C++ Redistributable
2. Reinstalling ScratchBird

---

## Next Steps

1. [Configure the server](../configuration/sb_server.conf.md)
2. [Create your first database](../getting-started/first-database.md)
3. [Connect with a client](../getting-started/first-connection.md)
