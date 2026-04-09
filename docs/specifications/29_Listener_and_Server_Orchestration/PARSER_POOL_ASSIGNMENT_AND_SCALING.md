# Parser Pool Assignment and Scaling

## Pool configuration

Current runtime pool controls are:
- `pool_min`
- `pool_max`
- `spawn_strategy`
- `max_requests`
- `max_age_seconds`
- `health_check_interval_ms`

Validation rules:
- `pool_min > 0`
- `pool_max > 0`
- `pool_min <= pool_max`

## Spawn strategies

Current listener runtime accepts:
- `prefork`
- `hybrid`
- `on_demand`

Current behavior:
- non-`on_demand` startup spawns up to `pool_min` workers before service
  admission
- `waitForWarm` must satisfy the warm minimum before listener startup succeeds

## Worker state model

Current worker states are:
- `IDLE`
- `BUSY`
- `DRAINING`
- `FAULT`

Listener pool state tracks:
- active session count
- warm worker count
- running worker count
- drain flag

## Assignment rules

1. Accepted connections may only be handed to registered workers.
2. Draining workers must not receive new sessions.
3. When the listener or pool is draining, new client work is rejected.
4. Worker registration is rejected if protocol mismatches or the pool is full.
5. Worker assignment is bounded by `pool_max`.

## Runtime updates

Current runtime updates are limited to:
- config-file reload
- `POOL SET <min> <max>` local management command

Update side effects:
- pool config copy is updated
- `ensureMinWorkers` replenishes toward `pool_min`
- waiting threads are notified

## Forced termination path

Current admin connection termination:
- finds the worker holding `active_connection_id`
- terminates the worker process
- marks the worker faulted
- clears session ownership
- records recycle metrics
- replenishes toward `pool_min`

## Metrics owned by the pool

Current pool metrics are:
- `scratchbird_parser_spawn_total`
- `scratchbird_parser_recycle_total`
- `scratchbird_parser_errors_total`
- `scratchbird_parser_pool_size`
- `scratchbird_parser_pool_idle`
- `scratchbird_parser_pool_busy`
- `scratchbird_parser_session_seconds`
- `scratchbird_parser_healthcheck_seconds`

## Hard boundaries

- Current pool authority is for shipped listener families only:
  - `native`
  - `postgresql`
  - `mysql`
  - `firebird`
- No generalized nine-family assignment contract exists.
- No distributed parser pool or cross-host scheduling contract exists.
