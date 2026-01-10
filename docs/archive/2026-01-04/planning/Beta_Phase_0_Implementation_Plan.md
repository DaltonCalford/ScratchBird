# **Specification: Alpha Phase 4 / Beta Phase 0 Implementation Plan**

**Status:** READY FOR IMPLEMENTATION
**Dependencies:** `docs/archive/2026-01-04/planning/alpha_exit_checklist_matrix.md`
**Goal:** Transition ScratchBird from a development codebase to a distributable, installable product.
**Output:** Installable packages (Windows/Linux), updated tools, and end-user documentation.

---

## **1. Cross-Compilation & Packaging Specification**

**Objective:** Enable the Ubuntu host to build native Windows binaries and generate all installer formats via CMake and CPack.

### **1.1 Build Environment (Ubuntu Host)**

Configure the build system for dual-target compilation (Linux Native + Windows Cross-Compile via MinGW-w64).

- **Compiler Standard:** MinGW-w64 (POSIX threading variant).

- **Target Architectures:** `x86_64-linux-gnu` and `x86_64-w64-mingw32`.

- **CMake Toolchain File:** Create `cmake/Toolchain-MinGW-w64.cmake`:
  
  - System Name: `Windows`
  
  - Compilers: `x86_64-w64-mingw32-gcc`, `x86_64-w64-mingw32-g++`
  
  - Linker Flags: `-static-libgcc -static-libstdc++ -static -lpthread` (Ensure standalone `.exe` with no DLL dependencies).

### **1.2 CPack Configuration**

Update root `CMakeLists.txt` to include CPack rules for generating artifacts.

- **Metadata:**
  
  - Name: `scratchbird`
  
  - Version: `0.9.0-beta0`
  
  - Vendor: `[Your Name/Entity]`
  
  - License: `LICENSE` (MIT)

- **Generators & Layouts:**
  
  1. **DEB (Linux):** Install to `/opt/scratchbird/bin`, config to `/opt/scratchbird/conf`.
  
  2. **RPM (Linux):** Same paths as DEB.
  
  3. **TGZ (Linux Portable):** Flat structure.
  
  4. **NSIS (Windows .exe):**
     
     - Install to `C:\Program Files\ScratchBird`.
     
     - Config to `C:\ProgramData\ScratchBird`.
     
     - Includes Start Menu shortcuts and Uninstaller.
     
     - **Installer UI:** Integrate `installer/welcome.txt`, `installer/license.txt`, `installer/notice.txt`, and `installer/feedback.txt` (see Section 9).
  
  5. **ZIP (Windows Portable):** Flat structure containing static `.exe` files and default `.conf`.
  
  6. **AppImage:** Specific generator for the GUI tool (ScratchRobin).
  
  7. **Docker:** Create official `Dockerfile` and `docker-compose.yml`.

- **Packaging Requirements:**
  
  - All packages include `sb_install`.
  
  - All packages include `sb_timezone_loader` and `sb_charset_loader`.
  
  - Timezone/charset data is either bundled with the package or referenced with an explicit download/source location in the installation docs.

### **1.3 Build Profiles**

Modify `CMakeLists.txt` to support four distinct build configurations (selectable via `-DBUILD_PROFILE=...`):

1. **Linux Release:** `-O3`, stripped symbols.

2. **Linux Debug:** `-g`, assertions enabled, symbols retained.

3. **Windows Release:** Optimized, statically linked.

4. **Windows Debug:** Debug symbols (PDBs where possible/DWARF), statically linked.

---

## **2. Client Library API**

**Objective:** Formalize the C API for network communication.

- **Task:** Create `/docs/specifications/CLIENT_LIBRARY_API_SPECIFICATION.md`.

- **Content Scope:**
  
  - Connection functions: `sb_connect(const char* conn_str, ...)`
  
  - Execution functions: `sb_execute`, `sb_prepare`, `sb_fetch`
  
  - Type Mapping: C types <-> SBLR types.
  
  - Error Handling: SQLSTATE mapping.
  
  - **Connection String Standard:** Define syntax for both network (`sb://user:pass@host:port/db`) and embedded (`embedded:/path/to/db.sb`) modes.

---

## **3. Administration Client (sb_admin)**

**Objective:** Specification for the master admin CLI.

- **Task:** Create `/docs/specifications/SB_ADMIN_CLI_SPECIFICATION.md`.

- **Command Scope:**
  
  - Server Control: `shutdown`, `restart`, `reload_config`.
  
  - Monitoring: `show connections`, `show pool_status`, `show stats`.
  
  - Session Control: `kill <session_id>`.
  
  - User Management: `create user`, `alter user`, `grant`.

---

## **4. Installation Utility (sb_install)**

**Objective:** Provide a single install/config/bootstrap utility and data asset loader.

- **Task:** Create `/docs/specifications/SB_INSTALL_TOOL_SPECIFICATION.md`.

- **Required Capabilities:**
  
  - `sb_install init`: Create default directories, write `sb_server.conf`, and register a service when requested.
  
  - `sb_install load-timezone`: Import timezone data files, record source/version/checksum, and verify load.
  
  - `sb_install load-charset`: Import charset/collation data files, record source/version/checksum, and verify load.
  
  - `sb_install verify`: Validate required assets and config presence; non-zero exit on failure.
  
  - `sb_install status`: Print installed versions, data locations, and last update timestamps.

- **Source Inputs:**
  
  - Offline-first: The utility accepts local files; no network access required.
  
  - Document authoritative sources and release cadence for timezone and charset data (default: IANA TZDB for timezones, Unicode CLDR/ICU for charset/collation).
  
  - Store provenance (source URL, version, checksum, import timestamp) in catalogs.

- **Loader Integration:**
  
  - `sb_timezone_loader` and `sb_charset_loader` remain standalone maintenance tools.
  
  - `sb_install` may invoke the loaders for initial setup, but ongoing updates are performed via the standalone tools.

---

## **5. Tool Update: Wire Protocol Adoption**

**Objective:** Update existing tools to use `libscratchbird_client` instead of direct engine linking.

- **Targets:** `sb_isql`, `sb_verify`, `sb_backup`, `sb_security`.

- **Requirement:**
  
  - Refactor build linking to use the Client Library (Step 2).
  
  - Implement dual-mode support:
    
    - **Default:** Network mode (connects to `localhost:3092`).
    
    - **Override:** Embedded mode (connects via direct file access if connection string specifies `embedded:`).
  
  - Add CLI flags: `-H <host>`, `-P <port>`, `-u <user>`, `-p <password>`.

---

## **6. ScratchRobin (GUI Tool)**

**Objective:** A visual management tool based on FlameRobin.

- **Task 5a:** Create specification `/docs/specifications/FlameRobin_Specification_for_AI.md`.
  
  - Define Rebranding: Change Name/Logo/About box.
  
  - Define Driver Swap: Replace `fbclient` with `libscratchbird_client`.
  
  - Define UI Updates: Add support for displaying ScratchBird's 11 index types and new security roles.

- **Task 5b:** Packaging.
  
  - Linux: Build as **AppImage**.
  
  - Windows: Include as an optional component in the NSIS installer.

---

## **7. End-User Documentation Suite**

**Objective:** Create the structured user guide (factual, implementation-focused, no marketing copy).

- **Root Directory:** `/docs/end-user/`

- **Structure (Required):**
  
  - `installation/` (Guides for Win/Linux/Docker, service setup, default paths, upgrade/uninstall, `sb_install` usage, timezone/charset updates)
  
  - `configuration/` (`sb_server.conf` reference, environment variables, defaults)
  
  - `admin/` (Security, backup, restore, auditing, monitoring)
  
  - `utilities/` (CLI tools: `sb_install`, `sb_admin`, `sb_isql`, `sb_fb_isql`, `sb_backup`, `sb_verify`, `sb_security`, `sb_charset_loader`, `sb_timezone_loader`)
  
  - `design/` (Architecture overview, storage layout, catalogs, wire protocols, security model)

  - `transaction-model/` (Always-in-transaction behavior, MGA visibility, isolation, commit/rollback retaining, 2PC, read-only)

  - `language-guide/` (SQL Reference)
    
    - `ddl/` (Index file + `create_table.md`, `create_index.md`, `create_domain.md`, `create_sequence.md`, `alter_*.md`, `drop_*.md`)
    
    - `dml/` (Index file + `select.md`, `insert.md`, `update.md`, `delete.md`, `merge.md`)
    
    - `psql/` (Index file + `procedures.md`, `triggers.md`, `variables.md`, `trigger_context.md`, `exception_handling.md`, `transaction_controls.md`)
    
    - `functions/` (Index file + `json_functions.md`, `string_functions.md`, `date_time_functions.md`, `math_functions.md`)
  
  - `version-control/` (Git integration, repository model, commit/tag/branch, diff/merge, rollback, audit)

  - `drivers/` (Where to obtain client drivers, supported versions, release schedules, upgrade guidance)
  
  - `testing/` (How to run tests, expected runtimes, result submission, feedback workflow)
  
  - `status/` (Summary of implemented features, beta plans, known limitations, deprecations)
  
  - `feedback/` (Bug report template, log collection, required diagnostics)
  
  - `how-to/` (Task guides: create database, configure auth, set default namespace, enable networking, backup/restore, verify, migrate legacy, install drivers, run tests, report bugs)
  
  - `faq/`

- **Indexing Logic:**
  
  - Create `/docs/end-user/README.md` as the top-level index linking to every top-level directory.
  
  - Create a `LANGUAGE-REFERENCE.md` master index.
  
  - Each sub-directory has a local `index.md`.
  
  - Every file includes navigation: `[Back to Index]`.

- **Minimum Content Requirements:**
  
  - Installation docs cover each platform and include upgrade/uninstall steps.
  
  - Installation docs specify where to obtain timezone/charset data, required versions, integrity checks, and update cadence.
  
  - Utilities docs list purpose, syntax, options, exit codes, and examples (including loader input formats and `sb_install` usage).
  
  - `sb_charset_loader` and `sb_timezone_loader` are documented as standalone maintenance tools.
  
  - Transaction model doc specifies defaults, autocommit toggle semantics, start-transaction conflict actions, read-only rules, and 2PC behavior.
  
  - PSQL docs include variable scope, trigger context variables, custom variables inside triggers, and exception handling (`WHEN <error>` and `WHEN ANY`).
  
  - Version control docs describe repository location, commit/tag/branch flow, conflict resolution, rollback behavior, and access control.
  
  - Drivers docs include acquisition sources, supported versions, release schedules, and compatibility notes.
  
  - Testing docs include environment requirements, expected runtimes, and a feedback submission checklist.
  
  - Feedback docs include required logs, repro steps, and where to submit results.
  
  - Status docs include implemented feature summary, beta plan summary, and known limitations.
  
  - Each how-to includes prerequisites, steps, verification, and cleanup/rollback notes.

---

## **8. Wiki Automation**

**Objective:** Sync documentation to GitHub Wiki.

- **Task:** Create script `scripts/publish_wiki.sh`.

- **Logic:**
  
  - Clone the wiki repo (`scratchbird.wiki.git`).
  
  - Copy contents of `/docs/end-user/*` to wiki root.
  
  - Commit and push changes.

---

## **9. Installer Assets (Factual Text Only)**

**Objective:** Provide required installer text with neutral, factual language (no marketing copy).

- **Assets to Create:**
  
  - `installer/welcome.txt`: Beta warning, compatibility notice, and data format volatility notice.
  
  - `installer/license.txt`: License text and standard warranty disclaimer.
  
  - `installer/notice.txt`: Default install paths, config paths, and service start/stop commands.
  
  - `installer/feedback.txt`: Issue tracker URL and minimum bug report details (version, OS, repro steps).
  
  - `README_INSTALL.txt`: Same content as above in a single file for non-NSIS packages.

---

## **10. Beta 1 "Early Access" Package Generation**

**Objective:** Generate final artifacts for distribution.

- **Action:** Execute full build and package cycle.

- **Required Artifacts:**
  
  1. `scratchbird_0.9.0-beta0_amd64.deb`
  
  2. `scratchbird-0.9.0-beta0-1.x86_64.rpm`
  
  3. `scratchbird-0.9.0-beta0-linux.tar.gz`
  
  4. `scratchbird-0.9.0-beta0-win64-setup.exe` (NSIS)
  
  5. `scratchbird-0.9.0-beta0-win64.zip`
  
  6. `ScratchRobin-0.9.0-x86_64.AppImage`
  
  7. `scratchbird-docker.tar` (or Dockerfile)

## **11. Release Engineering Requirements (No Marketing)**

**Objective:** Provide clear, repeatable release steps and gating checks.

- **CI/CD Pipeline (Required):**
  
  - Triggered on version tag.
  
  - Builds Linux and Windows artifacts using the same CMake/CPack configuration.
  
  - Runs the full test suite before packaging (all tests registered under `/tests/`).
  
  - Publishes artifacts with checksums.

- **Default Configuration (Required):**
  
  - Provide OS-specific defaults for data, log, and temp paths.
  
  - Package `sb_server.conf` with commented defaults and security warnings.

- **Compatibility Gate (Required):**
  
  - Validate FlameRobin connection against Firebird protocol where applicable.
  
  - Document the exact client versions used for validation.

- **Documentation Gate (Required):**
  
  - All required `/docs/end-user/` sections exist and are indexed.
  
  - Utilities docs, installation docs (including `sb_install`), transaction model docs, PSQL docs, version-control docs, drivers docs, testing docs, status docs, and feedback docs are present.
  
  - All how-to guides include verification steps.
