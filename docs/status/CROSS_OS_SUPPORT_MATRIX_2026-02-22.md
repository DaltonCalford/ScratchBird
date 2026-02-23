# Cross-OS Support Matrix (Linux/Windows)
Last modified: 2026-02-22

## Matrix

| Area | Linux x64 | Windows x64 (MSVC) | Linux->Windows x64 (MinGW) | Notes |
|---|---|---|---|---|
| Configure/Build presets | Supported | Supported | Supported | `CMakePresets.json` |
| Runtime server/listener/parser executables | Supported | Supported | Supported (cross-built artifacts) | `.exe` artifacts available from MinGW build |
| Portable test lane | Supported | Supported (CI lane) | N/A | `windows_portable` excludes linux-only tags |
| Linux-only tests | Supported | Not run | Not run | `UnixSocketTest.*`, `TSAN_*` |
| Full suite gate | Supported | Pending host execution evidence | N/A | Windows host execution required for parity signoff |
| Performance baseline | Supported | Pending host execution evidence | N/A | Windows go/no-go stays open until native run |

## Known Gaps

1. No local Windows host runtime execution evidence in this environment.
2. No `wine` runtime available for cross-built executable smoke execution.
3. Windows perf baseline still requires native Windows run.

## Migration Notes

1. Keep branch protection tied to `Cross OS Required Checks`.
2. Keep all cross-OS evidence in `artifacts/cross_os/`.
3. Promote Windows run/perf evidence from CI to this matrix before release signoff.

