# Listener Configuration Profile Schema

## Current configuration authority

Listener configuration is currently file-backed and launch-argument backed.

It is not currently catalog-authoritative.

## `network` section keys

Current `network` section keys consumed by `ServiceController` are:
- `bind_address`
- `control_socket_dir`
- `spawn_strategy`
- `parser_max_requests`
- `parser_max_age_seconds`
- `unix_socket`
- `unix_socket_permissions`
- `unix_socket_group`
- `native_port`
- `native_pool_min`
- `native_pool_max`
- `pg_port`
- `pg_pool_min`
- `pg_pool_max`
- `mysql_port`
- `mysql_pool_min`
- `mysql_pool_max`
- `fb_port`
- `fb_pool_min`
- `fb_pool_max`

## `listener.<name>` section keys

Current explicit listener profile keys are:
- `protocol`
- `bind_address`
- `port`
- `enabled`
- `ssl_required`
- `pool_min`
- `pool_max`
- `owner_database`

If any explicit `listener.<name>` profiles are present, they replace the legacy
per-family `network` listener construction path.

## `manager` section keys

Current manager-proxy keys are:
- `bind_address`
- `port`
- `internal_native_bind`
- `internal_native_port`
- `owner_database`
- `binary`
- `mcp_auth_secret`
- `dbbt_keyring`
- `listener_id`
- `dbbt_ttl_ms`
- `dbbt_clock_skew_ms`
- `dbbt_replay_cache_size`

## Listener launch arguments

Current listener launch argument set includes:
- `--bind`
- `--port`
- `--pool-min`
- `--pool-max`
- `--spawn-strategy`
- `--max-requests`
- `--max-age-seconds`
- `--control-socket-dir`
- `--engine-endpoint`
- `--database-owner`
- `--listener-id`
- `--dbbt-clock-skew-ms`
- `--dbbt-replay-cache-size`
- `--require-proxy-binding`
- `--dbbt-keyring`
- `--config`
- `--log-level`

Current listener-local parsed settings also include:
- `listener_mode`
- `config_path`
- `tls_config`
- `health_check_interval_ms`

## Default values

Current default listener pool bounds are:
- `pool_min = 4`
- `pool_max = 64`

Current default protocol ports from `ServiceController` are:
- native: `3092`
- postgresql: `5432`
- mysql: `3306`
- firebird: `3050`

Current default listener bind address is:
- `0.0.0.0`

## Required launch refusal rules

Listener launch must fail if:
- the owner database engine endpoint is missing
- managed mode is configured with non-loopback bind
- direct mode also requests proxy binding
- pool bounds are invalid

## Hard boundaries

- File-backed configuration is the current authority.
- Catalog-owned full listener profile authority is not part of the current
  shipped runtime.
- Hot reload is limited to the runtime subset documented in
  `LISTENER_MANAGEMENT_IPC_CHANNEL.md`.
