# Windows Cross-Compile Specification

## 1. Purpose

Define the canonical toolchains, dependencies, and build steps for producing
Windows binaries of ScratchBird from a Linux host. This is the required
reference for CI and release builds.

## 2. Scope

- Target: Windows x86_64 (64-bit).
- Host: Linux (Ubuntu 22.04+ recommended).
- Toolchain: MinGW-w64 (primary), LLVM-mingw (optional).

## 3. Required Toolchains

### 3.1 MinGW-w64 (Primary)

Required packages:
- `mingw-w64`
- `cmake`
- `ninja` (optional, recommended)
- `pkg-config` (optional, for host discovery)

### 3.2 LLVM-mingw (Optional)

Use when Clang/LLD is preferred for cross builds.

## 4. Target Dependencies (Windows)

ScratchBird requires the following libraries for Windows:

**Required**
- OpenSSL (TLS + crypto)
- zlib

**Optional**
- liblz4
- libgeos
- libproj
- libxml2

### 4.1 Dependency Provisioning Options

**Option A: vcpkg**
- Recommended for reproducible Windows dependency builds.
- Use a vcpkg toolchain file in CMake.
  - Static linkage: use `x64-windows-static` (MSVC) or `x64-mingw-static`.

**Option B: Prebuilt MinGW Packages**
- Use MinGW-w64 packaged libraries (distro-provided).
- Ensure the libraries match the toolchain ABI.

**Option C: Build From Source**
- For controlled versions when vcpkg is not available.
- Requires a cross-build of OpenSSL and zlib for Windows.

### 4.2 Static/Bundled Builds (Planned)

- Static builds are allowed for security-hardening.
- When using vcpkg, choose a static triplet and set:
  `-DVCPKG_LIBRARY_LINKAGE=static`.
- For MinGW, prefer:
  `-static -static-libgcc -static-libstdc++`
  and `-DOPENSSL_USE_STATIC_LIBS=TRUE`.
- Optional libraries may remain dynamic if static builds are unavailable.

### 4.3 Signed Runtime Bundle (Planned)

Windows builds may ship as a sealed bundle with signed libraries.

**Bundle requirements**
- All DLLs shipped alongside `sb_server.exe` in a private `lib/` directory.
- A signed `manifest.json` + `manifest.sig` are shipped with the bundle.
- ScratchBird verifies signatures and hashes at startup before loading DLLs.
  - Build option: `SCRATCHBIRD_SIGNED_RUNTIME_BUNDLE=ON`
  - Signature algorithm: `SCRATCHBIRD_BUNDLE_SIGNATURE_ALG=ed25519`

**Signature enforcement**
- Use `WinVerifyTrust` for Authenticode validation.
- If verification fails, startup must fail when `runtime_integrity = strict`.
- Optional enterprise enforcement via WDAC/Code Integrity policies.

## 5. CMake Toolchain File (Required)

Example `cmake/toolchains/mingw64.cmake`:

```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

## 6. Build Steps (MinGW-w64)

```bash
cmake -S . -B build-win \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw64.cmake \
  -DSCRATCHBIRD_WITH_COMPILER=ON

cmake --build build-win -j$(nproc)
```

## 7. Artifact Layout

Expected outputs:

```
build-win/bin/
  sb_server.exe
  sb_admin.exe
  sb_isql.exe
  sb_backup.exe
  sb_verify.exe
  sb_security.exe
build-win/lib/
  libscratchbird_core.dll
```

Required runtime DLLs (bundle alongside binaries or ship in installer):
- OpenSSL DLLs (libssl, libcrypto)
- zlib DLL
- Optional: lz4, geos, proj, libxml2 DLLs

## 8. Packaging

Windows packaging formats (planned):
- ZIP bundle (portable)
- MSI installer (full system install)

Install layout must mirror:
```
ScratchBird/
  bin/
  lib/
  etc/
  data/
  logs/
```

## 9. Testing

Options:
- Run unit tests under Wine (optional).
- CI should at minimum validate that binaries link and start.

## 10. Security Requirements

OpenSSL is mandatory for Windows builds. If OpenSSL is missing, builds must
fail with a clear error.
