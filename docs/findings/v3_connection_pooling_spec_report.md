# CONNECTION_POOLING_SPECIFICATION.md - Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/api/CONNECTION_POOLING_SPECIFICATION.md`

Status notes:
- The document explicitly states it is **Non-Authoritative** (design/reference).

Implementation notes (partial mapping in this repo):
- Two different pooling implementations exist:
  - `src/core/connection_pool.cpp` (engine-side pool for `ConnectionContext`).
  - `src/pool/connection_pool.cpp` (feature-rich pool manager with modes, caches, health/eviction loops) but with placeholder connection logic (`TODO` for real connect/execute).
- Statement cache and result cache components exist:
  - `src/pool/statement_cache.cpp`, `src/pool/result_cache.cpp`
  - `src/sblr/query_result_cache.cpp` used by executor for result cache.
- The spec’s SQL interface for pool configuration (`ALTER USER ... SET pool_*`, pool config via SQL) is not found in V3 parser/emitter/executor.
- The spec’s config file `[pool]` sections are not parsed in `src/server/config_parser.cpp` (no pool-related keys found).
- Per-user/per-application pooling, load balancing, and detailed health checks are not verified against code.

Verification:
- Partial code-level review only. No end-to-end verification of pooling behavior.

Noted gaps vs spec (non-exhaustive):
- SQL interface for pool configuration/inspection not implemented in V3 parser/emitter.
- Pool configuration via server config (`[pool]` sections) not wired in server config parser.
- `src/pool/connection_pool.cpp` uses placeholder connection logic; does not establish real protocol connections.
