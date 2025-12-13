# Date/Time Types

Date, time, and interval types.

[Back to Data Types Index](index.md) | [Back to Language Guide](../index.md)

---

## Types Overview

| Type | Storage | Description | Range |
|------|---------|-------------|-------|
| `DATE` | 4 bytes | Date only | 4713 BC to 5874897 AD |
| `TIME` | 8 bytes | Time only | 00:00:00 to 24:00:00 |
| `TIME WITH TIME ZONE` | 12 bytes | Time + TZ | Same |
| `TIMESTAMP` | 8 bytes | Date + time | 4713 BC to 294276 AD |
| `TIMESTAMP WITH TIME ZONE` | 8 bytes | Date + time + TZ | Same |
| `INTERVAL` | 16 bytes | Duration | ±178M years |

---

## DATE

Date without time:

```sql
CREATE TABLE events (
    event_date DATE
);

INSERT INTO events VALUES ('2024-01-15');
INSERT INTO events VALUES (DATE '2024-01-15');
INSERT INTO events VALUES (CURRENT_DATE);
```

### Date Formats

```sql
'2024-01-15'        -- ISO format (recommended)
'January 15, 2024'  -- Text format
'01/15/2024'        -- US format (if configured)
'15-01-2024'        -- European format (if configured)
```

---

## TIME

Time without date:

```sql
CREATE TABLE schedule (
    start_time TIME,
    end_time TIME
);

INSERT INTO schedule VALUES ('09:00:00', '17:00:00');
INSERT INTO schedule VALUES (TIME '14:30', '18:00');
```

### With Time Zone

```sql
CREATE TABLE meetings (
    meeting_time TIME WITH TIME ZONE
);

INSERT INTO meetings VALUES ('10:00:00-05');  -- EST
INSERT INTO meetings VALUES ('10:00:00+00');  -- UTC
```

---

## TIMESTAMP

Date and time combined:

```sql
CREATE TABLE logs (
    created_at TIMESTAMP
);

INSERT INTO logs VALUES ('2024-01-15 10:30:00');
INSERT INTO logs VALUES (TIMESTAMP '2024-01-15 10:30:00');
INSERT INTO logs VALUES (CURRENT_TIMESTAMP);
INSERT INTO logs VALUES (NOW());
```

### With Time Zone

```sql
CREATE TABLE events (
    event_time TIMESTAMP WITH TIME ZONE
);

-- Automatically converted to server timezone
INSERT INTO events VALUES ('2024-01-15 10:30:00-05');
INSERT INTO events VALUES ('2024-01-15 15:30:00+00');
```

**Best Practice:** Use `TIMESTAMP WITH TIME ZONE` for most applications.

---

## INTERVAL

Duration or time span:

```sql
INTERVAL '1 year'
INTERVAL '2 months'
INTERVAL '3 days'
INTERVAL '4 hours'
INTERVAL '5 minutes'
INTERVAL '6 seconds'

-- Combined
INTERVAL '1 year 2 months 3 days'
INTERVAL '1 day 2:30:00'  -- 1 day, 2 hours, 30 minutes
INTERVAL 'P1Y2M3D'        -- ISO 8601 format
```

---

## Current Date/Time

| Function | Returns |
|----------|---------|
| `CURRENT_DATE` | Today's date |
| `CURRENT_TIME` | Current time |
| `CURRENT_TIMESTAMP` | Current date+time |
| `NOW()` | Same as CURRENT_TIMESTAMP |
| `LOCALTIME` | Time without TZ |
| `LOCALTIMESTAMP` | Timestamp without TZ |

---

## Arithmetic

### Adding Intervals

```sql
DATE '2024-01-15' + INTERVAL '1 month'
-- Result: 2024-02-15

TIMESTAMP '2024-01-15 10:00' + INTERVAL '2 hours'
-- Result: 2024-01-15 12:00:00

CURRENT_DATE + 7
-- Result: 7 days from now
```

### Subtracting

```sql
DATE '2024-01-15' - INTERVAL '1 week'
-- Result: 2024-01-08

DATE '2024-01-15' - DATE '2024-01-01'
-- Result: 14 (integer days)

TIMESTAMP '2024-01-15 10:00' - TIMESTAMP '2024-01-14 08:00'
-- Result: 1 day 02:00:00 (interval)
```

---

## Extracting Parts

```sql
EXTRACT(YEAR FROM DATE '2024-01-15')   -- 2024
EXTRACT(MONTH FROM DATE '2024-01-15')  -- 1
EXTRACT(DAY FROM DATE '2024-01-15')    -- 15
EXTRACT(DOW FROM DATE '2024-01-15')    -- 1 (Monday)
EXTRACT(DOY FROM DATE '2024-01-15')    -- 15 (day of year)
EXTRACT(WEEK FROM DATE '2024-01-15')   -- 3
EXTRACT(QUARTER FROM DATE '2024-01-15') -- 1
EXTRACT(EPOCH FROM TIMESTAMP '2024-01-15 00:00:00') -- Unix timestamp
```

---

## Truncation

```sql
DATE_TRUNC('year', TIMESTAMP '2024-03-15 10:30:00')
-- Result: 2024-01-01 00:00:00

DATE_TRUNC('month', TIMESTAMP '2024-03-15 10:30:00')
-- Result: 2024-03-01 00:00:00

DATE_TRUNC('day', TIMESTAMP '2024-03-15 10:30:00')
-- Result: 2024-03-15 00:00:00

DATE_TRUNC('hour', TIMESTAMP '2024-03-15 10:30:00')
-- Result: 2024-03-15 10:00:00
```

---

## Formatting

```sql
TO_CHAR(CURRENT_DATE, 'YYYY-MM-DD')        -- '2024-01-15'
TO_CHAR(CURRENT_DATE, 'Month DD, YYYY')    -- 'January  15, 2024'
TO_CHAR(CURRENT_TIME, 'HH:MI AM')          -- '10:30 AM'
TO_CHAR(NOW(), 'YYYY-MM-DD HH24:MI:SS')    -- '2024-01-15 10:30:00'
```

---

## Parsing

```sql
TO_DATE('2024-01-15', 'YYYY-MM-DD')
TO_TIMESTAMP('2024-01-15 10:30', 'YYYY-MM-DD HH24:MI')
```

---

## Time Zones

```sql
-- Convert to timezone
TIMESTAMP '2024-01-15 10:00:00' AT TIME ZONE 'America/New_York'
TIMESTAMP '2024-01-15 10:00:00 UTC' AT TIME ZONE 'America/Los_Angeles'

-- Show timezone
SHOW timezone;

-- Set session timezone
SET timezone = 'America/New_York';
```

---

## Common Queries

### Recent Records

```sql
SELECT * FROM orders
WHERE created_at > NOW() - INTERVAL '24 hours';

SELECT * FROM orders
WHERE created_at > CURRENT_DATE - 7;
```

### Date Range

```sql
SELECT * FROM events
WHERE event_date BETWEEN '2024-01-01' AND '2024-12-31';
```

### Group by Period

```sql
SELECT DATE_TRUNC('month', created_at) AS month, COUNT(*)
FROM orders
GROUP BY DATE_TRUNC('month', created_at);
```

### Age Calculation

```sql
SELECT
    name,
    birth_date,
    EXTRACT(YEAR FROM AGE(birth_date)) AS age
FROM users;
```

### Business Days

```sql
SELECT * FROM logs
WHERE EXTRACT(DOW FROM created_at) BETWEEN 1 AND 5  -- Mon-Fri
  AND EXTRACT(HOUR FROM created_at) BETWEEN 9 AND 17; -- 9am-5pm
```

---

## Best Practices

1. **Use TIMESTAMPTZ** for events - handles timezones correctly
2. **Use DATE** for birthdays - no timezone needed
3. **Store UTC** - convert for display
4. **Use ISO format** - unambiguous (YYYY-MM-DD)
5. **Index date columns** - for range queries

---

## See Also

- [Date Functions](../functions/date-functions.md)
- [Window Functions](../functions/window-functions.md)
