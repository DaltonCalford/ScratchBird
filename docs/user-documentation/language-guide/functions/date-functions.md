# Date/Time Functions

Date and time operations.

[Back to Functions Index](index.md) | [Back to Language Guide](../index.md)

---

## Current Date/Time

| Function | Description | Example Result |
|----------|-------------|----------------|
| `CURRENT_DATE` | Today's date | `2024-01-15` |
| `CURRENT_TIME` | Current time | `10:30:45.123` |
| `CURRENT_TIMESTAMP` | Date and time | `2024-01-15 10:30:45.123` |
| `NOW()` | Same as CURRENT_TIMESTAMP | `2024-01-15 10:30:45.123` |
| `LOCALTIME` | Time without TZ | `10:30:45.123` |
| `LOCALTIMESTAMP` | Timestamp without TZ | `2024-01-15 10:30:45.123` |

---

## Constructing Dates

| Function | Description | Example |
|----------|-------------|---------|
| `MAKE_DATE(y, m, d)` | Create date | `MAKE_DATE(2024, 1, 15)` → `2024-01-15` |
| `MAKE_TIME(h, m, s)` | Create time | `MAKE_TIME(10, 30, 0)` → `10:30:00` |
| `MAKE_TIMESTAMP(y, m, d, h, m, s)` | Create timestamp | `MAKE_TIMESTAMP(2024, 1, 15, 10, 30, 0)` |
| `TO_DATE(s, fmt)` | Parse date | `TO_DATE('2024-01-15', 'YYYY-MM-DD')` |
| `TO_TIMESTAMP(s, fmt)` | Parse timestamp | `TO_TIMESTAMP('2024-01-15 10:30', 'YYYY-MM-DD HH24:MI')` |

---

## Extracting Parts

| Function | Description | Example |
|----------|-------------|---------|
| `EXTRACT(field FROM d)` | Extract part | `EXTRACT(YEAR FROM DATE '2024-01-15')` → `2024` |
| `DATE_PART(field, d)` | Same as EXTRACT | `DATE_PART('month', DATE '2024-01-15')` → `1` |

### Extract Fields

| Field | Description |
|-------|-------------|
| `YEAR` | Year |
| `MONTH` | Month (1-12) |
| `DAY` | Day of month |
| `HOUR` | Hour (0-23) |
| `MINUTE` | Minute (0-59) |
| `SECOND` | Second (0-59) |
| `DOW` | Day of week (0=Sun, 6=Sat) |
| `DOY` | Day of year (1-366) |
| `WEEK` | Week of year |
| `QUARTER` | Quarter (1-4) |
| `EPOCH` | Unix timestamp |

---

## Date Truncation

| Function | Description | Example |
|----------|-------------|---------|
| `DATE_TRUNC(precision, ts)` | Truncate to precision | See below |

### Truncation Levels

```sql
DATE_TRUNC('year', TIMESTAMP '2024-03-15 10:30:00')    → '2024-01-01 00:00:00'
DATE_TRUNC('quarter', TIMESTAMP '2024-03-15 10:30:00') → '2024-01-01 00:00:00'
DATE_TRUNC('month', TIMESTAMP '2024-03-15 10:30:00')   → '2024-03-01 00:00:00'
DATE_TRUNC('week', TIMESTAMP '2024-03-15 10:30:00')    → '2024-03-11 00:00:00'
DATE_TRUNC('day', TIMESTAMP '2024-03-15 10:30:00')     → '2024-03-15 00:00:00'
DATE_TRUNC('hour', TIMESTAMP '2024-03-15 10:30:00')    → '2024-03-15 10:00:00'
```

---

## Date Arithmetic

### Adding Intervals

```sql
DATE '2024-01-15' + INTERVAL '1 month'    → '2024-02-15'
DATE '2024-01-15' + INTERVAL '7 days'     → '2024-01-22'
TIMESTAMP '2024-01-15 10:00' + INTERVAL '2 hours' → '2024-01-15 12:00'
```

### Subtracting

```sql
DATE '2024-01-15' - INTERVAL '1 month'    → '2023-12-15'
DATE '2024-01-15' - DATE '2024-01-01'     → 14 (integer days)
```

### AGE Function

```sql
AGE(DATE '2024-01-15', DATE '2020-06-01') → '3 years 7 mons 14 days'
AGE(DATE '2024-01-15')                     → Interval from date to today
```

---

## Formatting

| Function | Description | Example |
|----------|-------------|---------|
| `TO_CHAR(d, fmt)` | Format date/time | See below |

### Format Patterns

| Pattern | Description | Example |
|---------|-------------|---------|
| `YYYY` | 4-digit year | `2024` |
| `YY` | 2-digit year | `24` |
| `MM` | Month (01-12) | `01` |
| `Mon` | Abbreviated month | `Jan` |
| `Month` | Full month name | `January` |
| `DD` | Day (01-31) | `15` |
| `Day` | Day name | `Monday` |
| `Dy` | Abbreviated day | `Mon` |
| `HH24` | Hour (00-23) | `10` |
| `HH12` | Hour (01-12) | `10` |
| `MI` | Minute (00-59) | `30` |
| `SS` | Second (00-59) | `45` |
| `AM/PM` | Meridiem | `AM` |

```sql
TO_CHAR(CURRENT_TIMESTAMP, 'YYYY-MM-DD HH24:MI:SS')  → '2024-01-15 10:30:45'
TO_CHAR(CURRENT_DATE, 'Day, Month DD, YYYY')         → 'Monday   , January  15, 2024'
TO_CHAR(CURRENT_TIME, 'HH12:MI AM')                  → '10:30 AM'
```

---

## Time Zones

| Function | Description | Example |
|----------|-------------|---------|
| `timezone(tz, ts)` | Convert to timezone | `timezone('America/New_York', NOW())` |
| `AT TIME ZONE tz` | Convert to timezone | `NOW() AT TIME ZONE 'UTC'` |

```sql
-- Convert to UTC
SELECT created_at AT TIME ZONE 'UTC' FROM events;

-- Convert from UTC to local
SELECT created_at AT TIME ZONE 'America/Los_Angeles' FROM events;
```

---

## Comparison and Ranges

| Function | Description |
|----------|-------------|
| `d1 < d2` | Before |
| `d1 > d2` | After |
| `d BETWEEN d1 AND d2` | In range |
| `OVERLAPS` | Ranges overlap |

```sql
-- Check if ranges overlap
SELECT (DATE '2024-01-01', DATE '2024-01-31')
       OVERLAPS
       (DATE '2024-01-15', DATE '2024-02-15');
-- Returns TRUE
```

---

## Examples

### Group by Month

```sql
SELECT
    DATE_TRUNC('month', created_at) AS month,
    COUNT(*) AS order_count
FROM orders
GROUP BY DATE_TRUNC('month', created_at)
ORDER BY month;
```

### Calculate Age

```sql
SELECT
    name,
    birth_date,
    EXTRACT(YEAR FROM AGE(birth_date)) AS age_years
FROM users;
```

### Recent Records

```sql
SELECT *
FROM orders
WHERE created_at > CURRENT_TIMESTAMP - INTERVAL '24 hours';
```

### Format for Display

```sql
SELECT
    TO_CHAR(created_at, 'Mon DD, YYYY at HH:MI AM') AS formatted_date
FROM orders;
```

### Business Hours Check

```sql
SELECT *
FROM logs
WHERE EXTRACT(DOW FROM created_at) BETWEEN 1 AND 5  -- Mon-Fri
  AND EXTRACT(HOUR FROM created_at) BETWEEN 9 AND 17; -- 9am-5pm
```

### Month-over-Month

```sql
SELECT
    DATE_TRUNC('month', created_at) AS month,
    SUM(total) AS revenue,
    LAG(SUM(total)) OVER (ORDER BY DATE_TRUNC('month', created_at)) AS prev_month
FROM orders
GROUP BY DATE_TRUNC('month', created_at);
```

---

## See Also

- [Data Types - Date/Time](../data-types/date-time-types.md)
- [Window Functions](window-functions.md)
