# Built-in Function Categories

[Operations Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Function Categories

| Category | Description | Status |
|----------|-------------|--------|
| [String Functions](01_string_functions.md) | Text manipulation | ✅ Complete |
| [Numeric Functions](02_numeric_functions.md) | Math and statistics | ✅ Complete |
| [Date/Time Functions](03_date_time_functions.md) | Temporal operations | ✅ Complete |
| [JSON Functions](04_json_functions.md) | JSON/JSONB operations | ✅ Complete |
| [Array Functions](05_array_functions.md) | Array operations | ✅ Complete |
| [Window Functions](../window_functions.md) | Analytical functions | ✅ Complete |
| [Aggregate Functions](../aggregate_functions.md) | Summary operations | ✅ Complete |
| [Geospatial Functions](06_geospatial_functions.md) | Spatial operations | ✅ Complete |
| [Encoding Functions](07_encoding_functions.md) | Base64, hex, etc. | ✅ Complete |
| [UUID Functions](08_uuid_functions.md) | UUID generation | ✅ Complete |
| [System Functions](09_system_functions.md) | Database info | ✅ Complete |

## Quick Reference by Use Case

### Data Transformation
- `CAST(x AS type)` / `x::type` - Type casting
- `COALESCE(a, b, ...)` - First non-null value
- `NULLIF(a, b)` - NULL if equal
- `GREATEST(a, b, ...)` - Maximum value
- `LEAST(a, b, ...)` - Minimum value

### Conditional Logic
- `CASE WHEN ... THEN ... END` - Conditional expression
- `IF(cond, true_val, false_val)` - Simple conditional
- `IFNULL(val, default)` - NULL replacement

### Pattern Matching
- `LIKE` / `ILIKE` - Simple patterns
- `~` / `~*` - Regular expressions
- `SIMILAR TO` - SQL patterns

## Function Volatility

Functions are categorized by how often their result changes:

| Category | Description | Examples |
|----------|-------------|----------|
| `IMMUTABLE` | Always same result | `ABS()`, `LENGTH()`, `UPPER()` |
| `STABLE` | Same in single query | `CURRENT_DATE`, `current_user()` |
| `VOLATILE` | Can change anytime | `RANDOM()`, `NOW()`, `nextval()` |

## Complete Function List

For a complete alphabetical list of all built-in functions, see the per-function directory or query:

```sql
-- List all functions
SELECT proname, proargtypes::regtype[]
FROM pg_proc
WHERE pronamespace = 'pg_catalog'::regnamespace
ORDER BY proname;
```
