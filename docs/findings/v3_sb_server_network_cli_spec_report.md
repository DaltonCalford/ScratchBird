# SB_SERVER_NETWORK_CLI_SPECIFICATION.md - Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/admin/SB_SERVER_NETWORK_CLI_SPECIFICATION.md`

Status notes:
- The document explicitly states it is **Non-Authoritative** but also labels itself "Status: Authoritative (V3)" (conflict). Treat as non-authoritative.

Implementation notes (sb_server in this repo):
- Listener binaries exist and are built as separate executables: `sb_listener_native`, `sb_listener_pg`, `sb_listener_mysql`, `sb_listener_fb` (`src/CMakeLists.txt`, `src/network/sb_listener_main.cpp`, `src/server/service_controller.cpp`).
- `sb_server` CLI options are implemented in `src/server/service_controller.cpp` and include:
  - `--config`, `-c`
  - `--enable-*` / `--disable-*` for native/postgres/mysql/firebird
  - Port flags: `--pg-port`, `--mysql-port`, `--fb-port` (native uses `-p/--port`)
  - Bind flags: `--native-bind`, `--postgres-bind`, `--mysql-bind`, `--firebird-bind`
  - Pool flags: `--native-pool-min/max`, `--postgres-pool-min/max`, `--mysql-pool-min/max`, `--firebird-pool-min/max`
  - Foreground/daemon: `-F/--foreground` (daemonization is default)
- The spec’s `--postgres-port` / `--firebird-port` flag names do not match the implemented `--pg-port` / `--fb-port`.
- The spec’s `--daemon` CLI option is not implemented; daemonization is the default with `--foreground` to disable.
- The spec’s `--log-level` CLI option is not implemented for `sb_server`; log level is read from config/env (`SCRATCHBIRD_LOG_LEVEL`) and is passed to listener processes.
- Precedence order (CLI > env > config > defaults) is not explicitly encoded as stated in the spec; `SCRATCHBIRD_CONFIG` env is used to locate the config, and CLI overrides appear to apply after config parsing.

Verification:
- Partial code-level verification for CLI flags and listener process model only (no runtime tests).
