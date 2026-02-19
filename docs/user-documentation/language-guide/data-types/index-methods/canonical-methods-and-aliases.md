# Canonical Methods And Aliases
Last modified: 2026-02-19

Back links:
- [Index Methods README](README.md)
- [Data Types README](../README.md)

Series navigation:
- Previous: [Parser-Accepted Methods](parser-accepted-methods.md)

Catalog canonical index type names include 58 validated values (subset of parser spellings).

Alias spellings normalized by parser before catalog validation:
- `VECTOR` -> `HNSW`
- `SPATIAL` -> `RTREE`
- `SP-GIST` -> `SPGIST`
- `ZONE_MAP` -> `ZONEMAP`

Operational guidance:
- prefer canonical names in DDL and migration scripts
- treat alias forms as input compatibility, not canonical persisted names
