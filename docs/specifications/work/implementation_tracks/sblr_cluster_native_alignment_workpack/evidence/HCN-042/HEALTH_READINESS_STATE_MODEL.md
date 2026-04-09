# Health Readiness State Model

## Liveness inputs
- `process_running`
- `event_loop_responding`

`live = process_running AND event_loop_responding`

## Readiness inputs
- `database_open`
- `catalog_available`
- `cluster_epoch_loaded`
- `listener_pool_available`
- `control_plane_reachable`
- `leader_leases_valid`
- `shard_map_loaded`

`ready = live AND all readiness inputs`

## Export surfaces
- `/healthz`: liveness status and component details
- `/readyz`: readiness status and component details
- `healthComponentRows(...)`: SQL-friendly component state rows
