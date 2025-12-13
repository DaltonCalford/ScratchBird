# Built-in Functions

SQL functions reference for ScratchBird.

[Back to Language Guide](../index.md)

---

## Function Categories

| Category | Description |
|----------|-------------|
| [String Functions](string-functions.md) | Text manipulation |
| [Numeric Functions](numeric-functions.md) | Math operations |
| [Date Functions](date-functions.md) | Date/time operations |
| [JSON Functions](json-functions.md) | JSON manipulation |
| [Aggregate Functions](aggregate-functions.md) | GROUP BY aggregates |
| [Window Functions](window-functions.md) | Analytical functions |

---

## Quick Reference

### String

```sql
UPPER('hello')           -- 'HELLO'
LOWER('HELLO')           -- 'hello'
LENGTH('hello')          -- 5
SUBSTRING('hello', 1, 3) -- 'hel'
CONCAT('a', 'b', 'c')    -- 'abc'
```

### Numeric

```sql
ABS(-5)                  -- 5
ROUND(3.14159, 2)        -- 3.14
FLOOR(3.7)               -- 3
CEIL(3.2)                -- 4
MOD(10, 3)               -- 1
```

### Date/Time

```sql
CURRENT_DATE             -- 2024-01-15
CURRENT_TIMESTAMP        -- 2024-01-15 10:30:00
DATE_TRUNC('month', ts)  -- Truncate to month
AGE(date1, date2)        -- Interval between dates
EXTRACT(YEAR FROM date)  -- Year component
```

### JSON

```sql
data->>'name'            -- Get text value
data->'address'          -- Get JSON value
jsonb_array_elements()   -- Expand array
json_build_object()      -- Build JSON
```

### Aggregate

```sql
COUNT(*)                 -- Row count
SUM(amount)              -- Total
AVG(price)               -- Average
MIN(date), MAX(date)     -- Min/max
```

### Window

```sql
ROW_NUMBER() OVER (...)  -- Sequential number
RANK() OVER (...)        -- Rank with gaps
LAG(col) OVER (...)      -- Previous row
LEAD(col) OVER (...)     -- Next row
```
