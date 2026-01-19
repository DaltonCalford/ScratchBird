# sb_server Network CLI Specification

Version: 1.0
Status: Draft (Alpha IP layer)
Last Updated: January 2026

## Purpose

Define sb_server CLI options for enabling network listeners,
parser pools, and network-related configuration overrides.

## Scope

- Listener enable/disable
- Per-protocol port overrides
- Parser pool sizing overrides
- Config file selection

## Listener Process Model

Listeners are separate executables:
- sb_listener_native
- sb_listener_pg
- sb_listener_mysql
- sb_listener_fb

sb_server starts the native listener by default. Emulated listeners are
spawned only when enabled by config or CLI flags.

## Global Options

```
--config <file>                 Configuration file path
--log-level <level>             info|debug|warn|error
--daemon                         Run as daemon
```

## Listener Options

```
--enable-native                 Enable ScratchBird native listener
--enable-postgres               Enable PostgreSQL listener
--enable-mysql                  Enable MySQL listener
--enable-firebird               Enable Firebird listener

--native-port <port>            Override native listener port
--postgres-port <port>          Override PostgreSQL listener port
--mysql-port <port>             Override MySQL listener port
--firebird-port <port>          Override Firebird listener port

--native-bind <addr>            Bind address for native listener
--postgres-bind <addr>          Bind address for PostgreSQL listener
--mysql-bind <addr>             Bind address for MySQL listener
--firebird-bind <addr>          Bind address for Firebird listener
```

## Parser Pool Options

```
--native-pool-min <n>
--native-pool-max <n>
--postgres-pool-min <n>
--postgres-pool-max <n>
--mysql-pool-min <n>
--mysql-pool-max <n>
--firebird-pool-min <n>
--firebird-pool-max <n>
```

## Defaults

- Ports: native=3092, postgres=5432, mysql=3306, firebird=3050
- Pool sizes: min=4, max=64 per protocol

## Precedence

1. CLI flags
2. Environment variables
3. Config file
4. Built-in defaults

## Related Specs

- docs/specifications/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md
- docs/specifications/deployment/SYSTEMD_SERVICE_SPECIFICATION.md
