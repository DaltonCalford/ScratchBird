# Type Family: Network, Geo, And Range
Last modified: 2026-02-19

Back links:
- [Type Families README](README.md)
- [Data Types README](../README.md)

Series navigation:
- Previous: [JSON, Vector, And Search](json-vector-search.md)
- Next: [Advanced And Container](advanced-container.md)

## Parser-Accepted Names
- Network: `INET`, `CIDR`, `MACADDR`, `MACADDR8`.
- Geometry: `GEOMETRY`, `POINT`, `LINESTRING`, `POLYGON`, `MULTIPOINT`, `MULTILINESTRING`, `MULTIPOLYGON`, `GEOMETRYCOLLECTION`.
- Range: `INT4RANGE`, `INT8RANGE`, `NUMRANGE`, `DATERANGE`, `TSRANGE`, `TSTZRANGE`.

## Runtime Notes
- Parser and emitter accept the full token set above.
- Engine-level semantic depth (operator classes, planner usage, storage behavior) varies by backend capability and is not uniformly closed for all extension families in 0.1.0.
