# ScratchBird Scripts Directory

Utility scripts for development, testing, validation, and build environment setup.

---

## Build Environment Setup (Beta Preparation)

### install-build-environment.sh
**Purpose:** Install complete cross-platform build environment
**Usage:** `./install-build-environment.sh`
**Requires:** sudo access, Ubuntu/Debian system
**Time:** ~20-30 minutes
**Size:** ~3 GB download, ~15 GB installed

**Installs:**
- Compilers: GCC 11/12/13, Clang 14/15/16
- Cross-compilation: MinGW-w64 (Windows), OSXCross deps (macOS)
- Build tools: CMake, Ninja, ccache
- Containers: Docker, Podman
- Packaging: DEB, RPM, AppImage tools
- Development: clang-format, clang-tidy, valgrind, gdb
- CI/CD: GitHub CLI, jq

**Post-install:** Logout and login (required for docker group)

### verify-build-environment.sh
**Purpose:** Verify build environment installation
**Usage:** `./verify-build-environment.sh`
**Requires:** None

**Checks:**
- All compilers and toolchains
- Cross-compilation tools
- Build dependencies
- Container tools
- Packaging tools
- Development tools
- User group memberships
- System resources and disk space

**Output:** Color-coded status (✓ green, ✗ red, ℹ yellow)

---

## Project Validation Scripts

### validate_project.py
**Purpose:** Comprehensive project structure and configuration validation
**Usage:** `./validate_project.py`

### verify_mga_compliance.sh
**Purpose:** Verify Firebird MGA transaction model compliance
**Usage:** `./verify_mga_compliance.sh`

### verify_foundation.sh
**Purpose:** Verify foundational infrastructure before implementation
**Usage:** `./verify_foundation.sh`

### verify_completion.sh
**Purpose:** Verify implementation completion against checklist
**Usage:** `./verify_completion.sh`

---

## Development Scripts

### apply_remediation.py
**Purpose:** Apply automated code remediation
**Usage:** `./apply_remediation.py`

### fix_integration_tests.py
**Purpose:** Fix integration test issues
**Usage:** `./fix_integration_tests.py`

### convert_to_googletest.py
**Purpose:** Convert tests to GoogleTest format
**Usage:** `./convert_to_googletest.py`

---

## Migration Scripts

### migrate_to_tid.sh
**Purpose:** Migrate to TID-based transaction system
**Usage:** `./migrate_to_tid.sh`

### complete_tid_migration.py
**Purpose:** Complete TID migration process
**Usage:** `./complete_tid_migration.py`

---

## Documentation Scripts

### doc_lint.py
**Purpose:** Lint and validate documentation
**Usage:** `./doc_lint.py`

### phase_runner.py
**Purpose:** Run specific project phases
**Usage:** `./phase_runner.py <phase>`

---

## Quick Reference

**First time setup:**
```bash
# Install build environment
./install-build-environment.sh

# Logout and login (for docker group)

# Verify installation
./verify-build-environment.sh

# Validate project
./validate_project.py
```

**Before implementing features:**
```bash
./verify_foundation.sh
./verify_mga_compliance.sh
```

**After implementation:**
```bash
./verify_completion.sh
```

---

## Documentation

Full build requirements documentation:
- **Location:** `docs/specifications/beta_requirements/builds/`
- **Index:** `00_BUILD_REQUIREMENTS_INDEX.md`
- **Complete guide:** `COMPLETE_BUILD_ENVIRONMENT_SETUP.md`

Platform-specific guides:
- Linux: `01_LINUX_NATIVE.md`
- Windows: `02_WINDOWS_NATIVE.md`
- macOS: `03_MACOS_NATIVE.md`
- Cross-compilation: `10-12_*.md`
- Packaging: `20-27_*.md`
- CI/CD: `40_GITHUB_ACTIONS.md`

---

**Note:** All scripts should be run from the project root directory unless otherwise specified.
