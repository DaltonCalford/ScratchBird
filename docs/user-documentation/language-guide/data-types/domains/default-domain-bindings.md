# Default Domain Bindings
Last modified: 2026-02-19

Back links:
- [Domains README](README.md)
- [Data Types README](../README.md)

Series navigation:
- Previous: [System Domain Families](system-domain-families.md)
- Next: [Custom Domain Lifecycle](custom-domain-lifecycle.md)

System schema/domain binding flow in catalog runtime:
1. Table+column-specific map (`kSystemDomainByTableColumn`)
2. Column-name map (`kSystemDomainByColumn`)
3. Type fallback map (`defaultDomainForType(DataType)`)

Deterministic rule:
- If no domain mapping resolves, DDL/bootstrap fails deterministically.

Representative type-to-domain fallback examples:
- `UUID` -> `[sb_dom]UUID_V7`
- `BOOLEAN` -> `[sb_dom]BOOL`
- `INT32` -> `[sb_dom]I32`
- `INT64` -> `[sb_dom]I64`
- `FLOAT64` -> `[sb_dom]F64`
- `DATE` -> `[sb_dom]DATE`
- `TIMESTAMP` -> `[sb_dom]TIMESTAMP`
- `TIMESTAMP_WITH_ZONE` -> `[sb_dom]TIMESTAMPTZ`
- `JSON` -> `[sb_dom]JSON`
- `JSONB` -> `[sb_dom]JSONB`
- `VECTOR` -> `[sb_dom]VECTOR`
- `TSVECTOR` -> `[sb_dom]TSVECTOR`
- `INT4RANGE` -> `[sb_dom]RANGE_INT4`
- `ARRAY` -> `[sb_dom]ARRAY`
- `ENUM` -> `[sb_dom]ENUM`
