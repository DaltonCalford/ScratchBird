# Data Types

SQL data type reference.

[Back to Language Guide](../index.md)

---

## Type Categories

| Category | Description |
|----------|-------------|
| [Numeric Types](numeric-types.md) | Integers, decimals, floats |
| [String Types](string-types.md) | Text and character data |
| [Date/Time Types](date-time-types.md) | Dates, times, intervals |
| [JSON Types](json-types.md) | JSON and JSONB |
| [Special Types](special-types.md) | UUID, boolean, arrays, etc. |

---

## Quick Reference

### Numeric

| Type | Storage | Range |
|------|---------|-------|
| `SMALLINT` | 2 bytes | -32768 to 32767 |
| `INTEGER` | 4 bytes | ±2 billion |
| `BIGINT` | 8 bytes | ±9 quintillion |
| `DECIMAL(p,s)` | Variable | Exact |
| `REAL` | 4 bytes | 6 decimal precision |
| `DOUBLE PRECISION` | 8 bytes | 15 decimal precision |

### String

| Type | Description |
|------|-------------|
| `CHAR(n)` | Fixed-length |
| `VARCHAR(n)` | Variable up to n |
| `TEXT` | Unlimited |

### Date/Time

| Type | Description |
|------|-------------|
| `DATE` | Date only |
| `TIME` | Time only |
| `TIMESTAMP` | Date and time |
| `INTERVAL` | Duration |

### Other

| Type | Description |
|------|-------------|
| `BOOLEAN` | TRUE/FALSE |
| `UUID` | 128-bit identifier |
| `JSON/JSONB` | JSON data |
| `BYTEA` | Binary data |
| `type[]` | Array |
