# Emulated Catalog Analysis: Redis 7.x

## Purpose
Identify Redis catalog and metadata surfaces and how they map to ScratchBird canonical data and runtime metrics.

## Classification
- `canonical`: requires persisted ScratchBird catalog data.
- `virtual`: derived from canonical data.
- `runtime`: derived from runtime state.
- `gated`: exposed only if feature is enabled.

## Mapping Table
| Redis surface | Purpose | SB source | Storage class | Notes |
| --- | --- | --- | --- | --- |
| INFO | Server stats | runtime metrics | runtime | Runtime metrics mapping. |
| CONFIG GET/SET | Configuration | `sys.config.key`, `sys.config.value` | canonical | Configuration catalog. |
| CLIENT LIST | Connection info | `connection` | on-disk | Canonical connection catalog. |
| SLOWLOG | Slow query log | `audit_log` | runtime | Derived from audit/metrics. |
| DBSIZE / KEYSPACE | Keyspace stats | `table_stats` | runtime | Derived stats. |
| COMMAND | Supported commands | emulation profile | virtual | Derived from emulation profile. |

## Notes
- Redis has no persistent catalog; all surfaces are **virtual** or **runtime** overlays.
- Redis logical structures (list/set/zset/hash/stream/geo/hll/bitmap) are modeled as domains over canonical list/set/map types.

## Resolved Decisions
- Redis module command metadata is out of Alpha scope.
- Alpha emulation covers core Redis command surfaces and uses canonical/runtime overlays defined in this section.

## Open Questions
- None.
