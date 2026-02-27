# ScratchBird Requirements

## Supported Build Hosts

Primary validated host: Linux (x86_64).

Cross-platform support exists in the codebase, but Linux is the default engineering/test baseline for beta gates.

## Required Tools

- `cmake` >= 3.20
- C++17 compiler (`g++` or `clang++` on Linux)
- `make` or `ninja`
- `pkg-config` (recommended for dependency discovery)
- `python3` (used by build/test helper scripts)
- `git`

## Core Required Libraries

- `zlib` (required by CMake)
- `openssl` (declared dependency)
- `libxml2` (declared dependency)

## Optional Feature Libraries

The build auto-detects these; features are disabled if missing.

- `lz4`
- `zstd`
- `geos`
- `proj`

## Driver/Test Tooling Requirements

Compatibility/emulation test workflows expect ScratchBird CLI clients from `ScratchBird-driver`:

- `sb_isql`
- `sb_pg_isql`
- `sb_my_isql`
- `sb_fb_isql`

## Ubuntu/Debian Reference Install

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build pkg-config git python3 \
  zlib1g-dev libssl-dev libxml2-dev liblz4-dev libzstd-dev \
  libgeos-dev libproj-dev
```
