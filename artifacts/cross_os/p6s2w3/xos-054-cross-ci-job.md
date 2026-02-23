# XOS-054 Linux->Windows Cross-Compile CI Job and Artifacts
Last-Modified: 2026-02-22

## Implemented
- Added cross compile job in:
  - `.github/workflows/cross-os-gate.yml`
  - job id: `linux_mingw_cross`
- Job stages:
  - MinGW dependency install
  - OpenSSL/zlib bootstrap
  - configure/build using `linux-mingw-windows-x64` preset
  - artifact publish via `actions/upload-artifact@v4`

## Local Evidence
- Cross configure/build output with produced `.exe` inventory:
  - `artifacts/cross_os/p6s2w3/xos-054-mingw-cross-build.txt`
