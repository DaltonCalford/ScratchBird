# Installer Features and Config Generator Specification

## 1. Purpose

Define a modular installer strategy for Linux and Windows that allows
selective installation of ScratchBird components (server, tools, drivers,
emulation listeners, GUI), and specify a post-install configuration generator
that can be rerun to add/remove features and optimize settings.

## 2. Scope

- Alpha: Native server + tools + core client libraries + configuration wizard.
- Beta: Optional components (emulation listeners, drivers, cluster features).
- Non-goal: AppImage privilege elevation (not supported by AppImage).

## 3. Constraints

- AppImage is user-space only. It cannot elevate privileges or install
  services. Use .deb/.rpm or MSI for system-wide installs and service setup.
- Configuration changes that touch system directories require root/admin.
- Config generator must support re-run without reinstall.

## 4. Installer Feature Matrix (Linux + Windows)

Legend: R = Required, O = Optional, N/A = Not applicable

| Component | Linux (DEB/RPM) | Windows (MSI) | Default | Notes |
| --- | --- | --- | --- | --- |
| Core server (`sb_server`) | R (`scratchbird-server`) | R (Feature: CoreServer) | ON | Installs service, config, data/log dirs |
| Admin tools (`sb_admin`, `sb_isql`, `sb_backup`, `sb_verify`, `sb_security`) | R (`scratchbird-tools`) | R (Feature: Tools) | ON | CLI utilities |
| Client libs (native) | O (`scratchbird-client-libs`) | O (Feature: ClientLibs) | OFF | For app integration |
| Dev headers/SDK | O (`scratchbird-dev`) | O (Feature: DevHeaders) | OFF | C/C++ headers + pkg-config |
| ScratchRobin GUI | O (`scratchrobin`) | O (Feature: ScratchRobin) | OFF | GUI admin tool |
| Emulation: PostgreSQL | O (`scratchbird-emulation-pg`) | O (Feature: EmulationPG) | OFF | Listener + parser |
| Emulation: MySQL | O (`scratchbird-emulation-mysql`) | O (Feature: EmulationMySQL) | OFF | Listener + parser |
| Emulation: Firebird | O (`scratchbird-emulation-firebird`) | O (Feature: EmulationFirebird) | OFF | Listener + parser |
| ODBC driver | O (`scratchbird-odbc`) | O (Feature: ODBCDriver) | OFF | Optional connectivity |
| JDBC driver | O (`scratchbird-jdbc`) | O (Feature: JDBCDriver) | OFF | Optional connectivity |
| Language drivers (Go/Python/Node/etc) | O (`scratchbird-driver-*`) | O (Features: DriverGo/DriverPython/etc) | OFF | Pure userland drivers |
| Config generator (`sb_setup`) | R (in server/tools) | R (in server/tools) | ON | Used for first-run + reconfigure |
| Service registration | R (`systemd`) | R (Windows Service) | ON | Installed with server |
| Signed runtime bundle | O (`scratchbird-bundle`) | O (Feature: SignedRuntime) | OFF | Optional security mode |

## 5. WiX Feature Tree (MSI)

```
ScratchBird
├── CoreServer (R)
│   ├── Service (R)
│   ├── ConfigFiles (R)
│   └── DataDirs (R)
├── Tools (R)
├── ClientLibs (O)
├── DevHeaders (O)
├── ScratchRobin (O)
├── Emulation
│   ├── EmulationPG (O)
│   ├── EmulationMySQL (O)
│   └── EmulationFirebird (O)
├── Drivers
│   ├── DriverGo (O)
│   ├── DriverPython (O)
│   ├── DriverNode (O)
│   ├── DriverDotNet (O)
│   ├── DriverJDBC (O)
│   ├── DriverPHP (O)
│   ├── DriverRuby (O)
│   ├── DriverR (O)
│   ├── DriverRust (O)
│   └── DriverPascal (O)
├── ODBCDriver (O)
├── JDBCDriver (O)
└── SignedRuntime (O)
```

**WiX notes**
- All optional features must be selectable in UI and via command line
  (`ADDLOCAL`, `REMOVE`, `REINSTALL`).
- Feature conditions must enforce dependencies (e.g., EmulationPG requires
  CoreServer).
- Upgrade must preserve feature selection and config unless explicit reset.

## 6. Linux Package Layout (DEB/RPM)

**Base packages**
- `scratchbird-server` (depends: `scratchbird-tools`)
- `scratchbird-tools` (CLI tools + `sb_setup`)
- `scratchbird-client-libs`
- `scratchbird-dev`
- `scratchrobin`

**Emulation packages**
- `scratchbird-emulation-pg`
- `scratchbird-emulation-mysql`
- `scratchbird-emulation-firebird`

**Drivers**
- `scratchbird-driver-go`
- `scratchbird-driver-python`
- `scratchbird-driver-node`
- `scratchbird-driver-dotnet`
- `scratchbird-driver-jdbc`
- `scratchbird-driver-php`
- `scratchbird-driver-ruby`
- `scratchbird-driver-r`
- `scratchbird-driver-rust`
- `scratchbird-driver-pascal`

**Connectivity**
- `scratchbird-odbc`
- `scratchbird-jdbc`

**Meta packages**
- `scratchbird` (server + tools + client libs)
- `scratchbird-emulation-all`
- `scratchbird-drivers-all`

**Dependencies**
- Emulation packages depend on `scratchbird-server`.
- Driver packages are standalone; no server dependency required.
- `scratchbird-dev` depends on `scratchbird-client-libs`.

## 7. Post-Install Config Generator (sb_setup)

### 7.1 Purpose

Provide a rerunnable configuration assistant that can:
- Enable/disable features after install.
- Tune settings based on system analysis.
- Generate or update config files safely.
- Restart services as needed.

### 7.2 Invocation

Binary: `sb_setup`

Modes:
- `sb_setup --interactive`
- `sb_setup --apply profile.json`
- `sb_setup --enable emulation:pg`
- `sb_setup --disable emulation:mysql`
- `sb_setup --optimize preset=balanced`
- `sb_setup --revert last`

### 7.3 Config Targets

Linux:
- `/etc/scratchbird/sb_server.conf`
- `/etc/scratchbird/listeners.conf`
- `/etc/scratchbird/security/` (optional)

Windows:
- `%ProgramData%\\ScratchBird\\sb_server.conf`
- `%ProgramData%\\ScratchBird\\listeners.conf`

### 7.4 System Analysis (Optimization)

Collect:
- CPU count and topology
- RAM and available memory
- Disk type and throughput (NVMe/SATA/HDD)
- Network interface count and bandwidth

Suggest:
- Cache sizes (shared buffers, temp buffers)
- Worker thread limits
- I/O concurrency
- Default tablespace layout (optional)

### 7.5 Emulation Listener Flags

Defaults:
- Native: 3092
- PostgreSQL: 5432
- MySQL: 3306
- Firebird: 3050

Config options:
- `listeners.native.enabled`
- `listeners.postgresql.enabled`
- `listeners.mysql.enabled`
- `listeners.firebird.enabled`
- `listeners.*.port`
- `listeners.*.bind_address`

### 7.6 Reconfiguration Rules

- Always write a timestamped backup of configs.
- Validate config syntax before applying.
- If validation fails: rollback to last known good.
- When feature removal occurs, disable listeners and update services but do
  not delete data directories unless explicitly requested.

### 7.7 Service Control

Linux (systemd):
- Reload or restart `scratchbird.service` when config changes.

Windows (Service Control Manager):
- Restart `ScratchBird` service when config changes.

### 7.8 User Prompts

Interactive flow:
1. Install type (server-only, client-only, full).
2. Enable emulation listeners (pg/mysql/firebird).
3. Ports and bind addresses.
4. Data directory + log directory.
5. Security profile (baseline/strict).
6. Performance tuning (auto/manual).
7. Apply + restart.

### 7.9 Output Artifacts

- `sb_setup_report.json` with:
  - Detected hardware summary
  - Selected features
  - Config diffs
  - Service restart results

## 8. Security and Privileges

- `sb_setup` must detect privilege level.
- If not elevated, it can only write user-level config and will warn.
- System-wide changes require root/admin.

## 9. Related Specifications

- `INSTALLATION_AND_BUILD_SPECIFICATION.md`
- `SYSTEMD_SERVICE_SPECIFICATION.md`
- `WINDOWS_CROSS_COMPILE_SPECIFICATION.md`

