# System Catalog

**Last Updated:** 2026-02-03

---

ScratchBird exposes MySQL metadata through information_schema and mysql.*
views mapped to its native catalog.

Common information_schema tables:
- information_schema.tables
- information_schema.columns
- information_schema.statistics
- information_schema.schemata

## Column Notes: information_schema.tables

| Column | Notes |
|--------|-------|
| `TABLE_SCHEMA`, `TABLE_NAME` | Mapped from ScratchBird schema/table names. |
| `TABLE_TYPE` | BASE TABLE or VIEW. |
| `ENGINE` | Emulated; does not affect storage engine. |
| `TABLE_ROWS` | Estimated row count if available. |
| `AVG_ROW_LENGTH`, `DATA_LENGTH`, `INDEX_LENGTH` | Derived or NULL if not tracked. |
| `CREATE_TIME`, `UPDATE_TIME` | Available if timestamps tracked. |

## Column Notes: information_schema.columns

| Column | Notes |
|--------|-------|
| `TABLE_SCHEMA`, `TABLE_NAME`, `COLUMN_NAME` | Mapped names. |
| `ORDINAL_POSITION` | Column position in table definition. |
| `COLUMN_DEFAULT` | Default expression if defined. |
| `IS_NULLABLE` | YES/NO. |
| `DATA_TYPE`, `COLUMN_TYPE` | MySQL type name mapped from ScratchBird types. |
| `CHARACTER_SET_NAME`, `COLLATION_NAME` | Mapped when available. |

## Column Notes: information_schema.statistics

| Column | Notes |
|--------|-------|
| `TABLE_SCHEMA`, `TABLE_NAME`, `INDEX_NAME` | Mapped names. |
| `NON_UNIQUE` | 0 for UNIQUE, 1 for non‑unique. |
| `SEQ_IN_INDEX` | Column position in index. |
| `COLUMN_NAME` | Indexed column name. |
| `INDEX_TYPE` | Mapped index method where available. |

## Differences

- performance_schema tables are mapped when possible; some columns are NULL if
  ScratchBird does not track the metric. See
  [performance_schema column notes](performance_schema.md).
- MySQL‑specific storage engine metadata is emulated.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
