# Date/Time Functions

[Categories README](./README.md) | [Operations Guide README](../../README.md)

## Synopsis

Functions for working with dates, times, timestamps, and intervals.

## Current Date/Time

| Function | Description | Return Type |
|----------|-------------|-------------|
| `CURRENT_DATE` | Current date | DATE |
| `CURRENT_TIME` | Current time with zone | TIMETZ |
| `CURRENT_TIMESTAMP` | Current timestamp with zone | TIMESTAMPTZ |
| `LOCALTIME` | Current time without zone | TIME |
| `LOCALTIMESTAMP` | Current timestamp without zone | TIMESTAMP |
| `NOW()` | Current timestamp | TIMESTAMPTZ |
| `TRANSACTION_TIMESTAMP()` | Transaction start time | TIMESTAMPTZ |
| `STATEMENT_TIMESTAMP()` | Statement start time | TIMESTAMPTZ |
| `CLOCK_TIMESTAMP()` | Real-time clock | TIMESTAMPTZ |

## Date/Time Arithmetic

| Function | Description | Example |
|----------|-------------|---------|
| `age(timestamp [, timestamp])` | Difference between timestamps | `age('2024-01-01')` |
| `date_part(field, source)` | Extract field | `date_part('year', NOW())` |
| `extract(field FROM source)` | Extract field | `extract(YEAR FROM NOW())` |
| `date_trunc(field, source)` | Truncate to precision | `date_trunc('month', NOW())` |

## Date/Time Formatting

| Function | Description | Example |
|----------|-------------|---------|
| `to_char(timestamp, fmt)` | Format timestamp | `to_char(NOW(), 'YYYY-MM-DD')` |
| `to_date(str, fmt)` | Parse date | `to_date('2024-01-01', 'YYYY-MM-DD')` |
| `to_timestamp(str, fmt)` | Parse timestamp | `to_timestamp('2024', 'YYYY')` |

## Interval Functions

| Function | Description | Example |
|----------|-------------|---------|
| `make_interval(...)` | Create interval | `make_interval(days => 7)` |
| `justify_days(interval)` | Adjust days | - |
| `justify_hours(interval)` | Adjust hours | - |
| `justify_interval(interval)` | Adjust both | - |

## Date/Time Components

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `year(timestamp)` | Extract year | `year('2024-03-15')` | `2024` |
| `month(timestamp)` | Extract month | `month('2024-03-15')` | `3` |
| `day(timestamp)` | Extract day | `day('2024-03-15')` | `15` |
| `hour(timestamp)` | Extract hour | `hour('12:34:56')` | `12` |
| `minute(timestamp)` | Extract minute | `minute('12:34:56')` | `34` |
| `second(timestamp)` | Extract second | `second('12:34:56')` | `56` |

## Overlap Functions

| Function | Description | Example |
|----------|-------------|---------|
| `overlaps(t1, t2, t3, t4)` | Check overlap | `overlaps(start1, end1, start2, end2)` |

## Examples

```sql
-- Current timestamp
SELECT NOW();

-- Add interval
SELECT NOW() + INTERVAL '7 days';

-- Extract year
SELECT EXTRACT(YEAR FROM order_date) FROM orders;

-- Truncate to month
SELECT DATE_TRUNC('month', created_at) AS month FROM events;

-- Age calculation
SELECT AGE(birth_date) FROM users;

-- Format date
SELECT TO_CHAR(order_date, 'Mon DD, YYYY') FROM orders;

-- Time between dates
SELECT order_date - shipped_date AS days_to_ship FROM orders;

-- First day of month
SELECT DATE_TRUNC('month', CURRENT_DATE);

-- Last day of month
SELECT DATE_TRUNC('month', CURRENT_DATE) + INTERVAL '1 month' - INTERVAL '1 day';
```

## Date/Time Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Add interval | `date + interval '1 day'` |
| `-` | Subtract | `date - interval '1 day'` |
| `-` | Difference | `date1 - date2` |

## See Also

- [Temporal Types](../../type_system/05_temporal_types.md)
