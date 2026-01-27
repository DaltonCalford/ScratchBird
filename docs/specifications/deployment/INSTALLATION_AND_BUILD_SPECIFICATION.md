# Installation and Build Specification (ScratchBird)

## 1. Purpose

Define the supported installation methods, build requirements, and artifact
layout for ScratchBird. This document is the canonical reference for building
and installing ScratchBird from source, plus packaging expectations for
distributions.

## 2. Scope

- Alpha scope: Linux builds and native install methods.
- Windows support: via cross-compilation (see
  `WINDOWS_CROSS_COMPILE_SPECIFICATION.md`).
- Container deployment is referenced but detailed in beta requirements.

## 3. Supported Installation Methods

### 3.1 Build From Source (Primary, Alpha)

**Use case:** Development, CI builds, on-prem installs.

**Required tools**
- CMake 3.20+
- C++17 compiler (GCC 10+, Clang 12+)
- pkg-config

**Required libraries**
- OpenSSL (required; build fails without it)
- zlib (required)

**Optional libraries (feature-gated)**
- liblz4 (compression)
- libgeos (spatial functions)
- libproj (coordinate systems)
- libxml2 (XML/XPath)

**Build steps**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

**Install**
```bash
cmake --install build --prefix /usr/local
```

### 3.2 Developer "In-Tree" Run (No Install)

**Use case:** Local development and debugging.

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
./build/bin/sb_server
```

### 3.3 System Packages (Planned)

**Use case:** Production installs via OS package managers.

Planned package formats:
- Debian/Ubuntu: `.deb`
- RHEL/Fedora: `.rpm`
- Arch: PKGBUILD

Expected package contents:
- `sb_server`, `sb_admin`, `sb_isql`, `sb_backup`, `sb_verify`, `sb_security`
- Default configs: `/etc/scratchbird/`
- Data directory: `/var/lib/scratchbird/`
- Logs: `/var/log/scratchbird/`
- Systemd unit (see `SYSTEMD_SERVICE_SPECIFICATION.md`)

### 3.4 Container Images (Beta Reference)

Container image specifications are tracked under:
`docs/specifications/beta_requirements/cloud-container/`.

### 3.5 Embedded Library (Planned)

ScratchBird supports an embedded usage model. The install layout must expose:
- `libscratchbird_core` (static or shared)
- public headers under `${prefix}/include/scratchbird/`

## 4. Build Dependencies by OS

### 4.1 Ubuntu/Debian

```bash
sudo apt install -y \
  build-essential cmake pkg-config \
  libssl-dev zlib1g-dev \
  liblz4-dev libgeos-dev libproj-dev libxml2-dev
```

### 4.2 Fedora/RHEL

```bash
sudo dnf install -y \
  gcc-c++ cmake pkgconfig \
  openssl-devel zlib-devel \
  lz4-devel geos-devel proj-devel libxml2-devel
```

### 4.3 Arch Linux

```bash
sudo pacman -S --needed \
  base-devel cmake pkgconf \
  openssl zlib lz4 geos proj libxml2
```

## 5. CMake Options

| Option | Default | Description |
| --- | --- | --- |
| `SCRATCHBIRD_WITH_COMPILER` | `ON` | Build SQL compiler/parsers |

## 6. Dependency Linking Modes (Planned)

ScratchBird can be built in one of the following dependency modes. These are
planned build options to reduce third-party runtime exposure.

| Mode | Goal | Notes |
| --- | --- | --- |
| `system` | Use OS-provided shared libraries | Default for development; simplest patch flow |
| `bundled` | Ship private shared libs in the install | Reduces reliance on host libs; requires RPATH |
| `static` | Link dependencies into binaries | Minimizes runtime library swapping; larger binaries |

**Security tradeoffs**
- Static linking reduces runtime tampering risk but requires fast rebuilds for
  CVE patches.
- Bundled shared libs should be version-pinned and optionally checksummed on
  startup.
- Full elimination of third-party code is not possible while OpenSSL and zlib
  are required; this mode embeds them instead of loading from the OS.

**License note**
- Some optional dependencies are LGPL (e.g., GEOS/PROJ/libxml2). Static linking
  requires compliance with their terms (e.g., providing relinkable objects).

## 7. Signed Runtime Bundle (Planned)

ScratchBird may ship with a sealed runtime bundle that includes all required
third-party libraries and enforces integrity checks at startup.

**Goals**
- Avoid host-installed dependencies.
- Prevent library swapping without detection.
- Provide a verifiable, reproducible runtime payload.

**Bundle contents**
- `bin/` ScratchBird executables
- `lib/` bundled OpenSSL + zlib (and optional libs when enabled)
- `manifest.json` with SHA-256 for each bundled file
- `manifest.sig` signed manifest
- `certs/` trusted signing certs (or embedded certs in binary)

**Build options (naming)**
- `SCRATCHBIRD_SIGNED_RUNTIME_BUNDLE` (ON|OFF) - enable bundle layout + manifest verification
- `SCRATCHBIRD_BUNDLE_SIGNATURE_ALG` (default: `ed25519`) - signature algorithm
- `SCRATCHBIRD_BUNDLE_HASH_ALG` (default: `sha256`) - manifest hash algorithm

**Manifest format (bundle v1)**
```json
{
  "schema_version": "1.0",
  "bundle_id": "scratchbird-runtime",
  "product_version": "0.0.1",
  "build_id": "uuid-or-hash",
  "created_utc": "2026-02-05T00:00:00Z",
  "hash_alg": "sha256",
  "signature_alg": "ed25519",
  "files": [
    { "path": "bin/sb_server", "sha256": "<hex>", "size": 123456 },
    { "path": "lib/libssl.so.3", "sha256": "<hex>", "size": 234567 }
  ]
}
```

**Signature**
- `manifest.sig` is a detached signature of the raw `manifest.json` bytes (UTF-8).
- The public key is embedded or shipped in `certs/` and pinned at build time.

**Runtime enforcement**
- On startup, verify `manifest.sig` and the manifest hash.
- Verify each bundled file hash before loading.
- If verification fails: refuse to start (default `strict`).

**Config policy**
- `runtime_integrity = strict|warn|off`
  - `strict`: fail startup on any mismatch
  - `warn`: log warning, continue
  - `off`: no verification (dev only)

**Loader hardening**
- Set private library search paths only (no global search).
- Bundle libraries in a non-writable directory when possible.

## 8. Install Layout

Default layout for `/usr/local`:

```
bin/
  sb_server
  sb_admin
  sb_isql
  sb_backup
  sb_verify
  sb_security
lib/
  libscratchbird_core.so
include/
  scratchbird/
etc/
  scratchbird/
    sb_server.conf
    sb_hba.conf
var/
  lib/scratchbird/      # data
  log/scratchbird/      # logs
```

## 9. Runtime Requirements

- OpenSSL shared libraries must be present on the host.
- zlib shared libraries must be present on the host.
- Optional libraries enable optional functionality; absence must not prevent
  server startup.
- In `static` mode, OpenSSL/zlib are embedded; host `libc` is still required.

## 10. Cross-Compile Reference

See `WINDOWS_CROSS_COMPILE_SPECIFICATION.md` for the Windows toolchain,
dependencies, and packaging requirements.
