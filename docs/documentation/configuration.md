### Configuration

What it is
- Engine configuration (storage/performance/feature flags) and packaging/service configuration (process/network settings).

Why it matters
- Separates engine behavior from deployment environment; enables reproducible setups and safe feature toggles.

How to use it
- Use SB_CONFIG for engine keys; use packaging config or environment for server process settings.

Engine configuration (read via `SB_CONFIG`):
- Keys and defaults from `include/scratchbird/engine/config.h` and `src/engine/config.cpp`:
  - allowed_page_sizes: comma-separated ints (bytes); default from `kAllowedPageSizesBytes`
  - default_page_size: int (default 4096)
  - prealloc_mb: int
  - direct_io: bool
  - checksum_policy: off|crc32c|verify_on_read
  - fsync_policy: always|group|os-default
  - prefetch_on_alloc: bool
  - prefetch_horizon_pages: int
  - bootstrap_execute: bool
  - tablespaces_enabled: bool
  - enable_partition_pruning, enable_partition_wise_ops, enable_materialized_views,
    enable_mv_incremental, enable_mv_concurrent_refresh, enable_query_rewrite,
    enable_global_indexes: bools

Format:
```
key=value
# comments supported
```

Packaging/service configuration (environment/config file): `packaging/config/scratchbird.conf`
- BIND_ADDRESS, PORT, LOG_LEVEL, DATA_DIR, TLS_* (commented examples)

Scope:
- Engine config controls storage and performance features
- Packaging config controls server process environment and network binding

See also
- [Installation](./installation.md) · [Session & transaction](./session-and-transaction.md)

