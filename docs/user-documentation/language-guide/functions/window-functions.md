# Window Functions

Analytical functions that operate over partitions.

[Back to Functions Index](index.md) | [Back to Language Guide](../index.md)

---

## Overview

Window functions perform calculations across rows related to the current row, without grouping rows together.

```sql
function() OVER (
    [PARTITION BY columns]
    [ORDER BY columns]
    [frame_clause]
)
```

---

## Ranking Functions

| Function | Description |
|----------|-------------|
| `ROW_NUMBER()` | Sequential number 1, 2, 3, ... |
| `RANK()` | Rank with gaps on ties |
| `DENSE_RANK()` | Rank without gaps |
| `NTILE(n)` | Divide into n buckets |
| `PERCENT_RANK()` | Relative rank (0 to 1) |
| `CUME_DIST()` | Cumulative distribution |

### Examples

```sql
SELECT
    name,
    score,
    ROW_NUMBER() OVER (ORDER BY score DESC) AS row_num,
    RANK() OVER (ORDER BY score DESC) AS rank,
    DENSE_RANK() OVER (ORDER BY score DESC) AS dense_rank
FROM students;
```

Result:
```
name  | score | row_num | rank | dense_rank
------|-------|---------|------|------------
Alice | 95    | 1       | 1    | 1
Bob   | 90    | 2       | 2    | 2
Carol | 90    | 3       | 2    | 2
Dave  | 85    | 4       | 4    | 3
```

---

## Navigation Functions

| Function | Description |
|----------|-------------|
| `LAG(col, n, default)` | Value from n rows before |
| `LEAD(col, n, default)` | Value from n rows after |
| `FIRST_VALUE(col)` | First value in window |
| `LAST_VALUE(col)` | Last value in window |
| `NTH_VALUE(col, n)` | Nth value in window |

### LAG and LEAD

```sql
SELECT
    date,
    revenue,
    LAG(revenue) OVER (ORDER BY date) AS prev_day,
    LEAD(revenue) OVER (ORDER BY date) AS next_day,
    revenue - LAG(revenue) OVER (ORDER BY date) AS change
FROM daily_sales;
```

Result:
```
date       | revenue | prev_day | next_day | change
-----------|---------|----------|----------|-------
2024-01-01 | 1000    | NULL     | 1100     | NULL
2024-01-02 | 1100    | 1000     | 900      | 100
2024-01-03 | 900     | 1100     | 1200     | -200
2024-01-04 | 1200    | 900      | NULL     | 300
```

### FIRST_VALUE and LAST_VALUE

```sql
SELECT
    name,
    department,
    salary,
    FIRST_VALUE(name) OVER (
        PARTITION BY department
        ORDER BY salary DESC
    ) AS highest_paid
FROM employees;
```

---

## Aggregate Window Functions

Regular aggregates can be used as window functions:

```sql
SELECT
    date,
    amount,
    SUM(amount) OVER (ORDER BY date) AS running_total,
    AVG(amount) OVER (ORDER BY date) AS running_avg,
    COUNT(*) OVER (ORDER BY date) AS cumulative_count
FROM transactions;
```

---

## PARTITION BY

Divide rows into groups:

```sql
SELECT
    department,
    name,
    salary,
    AVG(salary) OVER (PARTITION BY department) AS dept_avg,
    salary - AVG(salary) OVER (PARTITION BY department) AS diff_from_avg
FROM employees;
```

---

## ORDER BY

Define ordering within partitions:

```sql
SELECT
    customer_id,
    order_date,
    total,
    SUM(total) OVER (
        PARTITION BY customer_id
        ORDER BY order_date
    ) AS cumulative_total
FROM orders;
```

---

## Frame Clauses

Define which rows to include in the window:

```sql
{ROWS | RANGE | GROUPS} BETWEEN frame_start AND frame_end
```

### Frame Boundaries

| Boundary | Description |
|----------|-------------|
| `UNBOUNDED PRECEDING` | Start of partition |
| `n PRECEDING` | n rows before current |
| `CURRENT ROW` | Current row |
| `n FOLLOWING` | n rows after current |
| `UNBOUNDED FOLLOWING` | End of partition |

### Examples

```sql
-- 3-day moving average
SELECT
    date,
    value,
    AVG(value) OVER (
        ORDER BY date
        ROWS BETWEEN 2 PRECEDING AND CURRENT ROW
    ) AS moving_avg_3
FROM metrics;

-- Sum of current + next 2 rows
SELECT
    date,
    value,
    SUM(value) OVER (
        ORDER BY date
        ROWS BETWEEN CURRENT ROW AND 2 FOLLOWING
    ) AS sum_next_3
FROM metrics;

-- All rows from start to current
SELECT
    date,
    value,
    SUM(value) OVER (
        ORDER BY date
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) AS running_total
FROM metrics;
```

---

## Named Windows

Define reusable windows:

```sql
SELECT
    name,
    department,
    salary,
    RANK() OVER w AS dept_rank,
    AVG(salary) OVER w AS dept_avg
FROM employees
WINDOW w AS (PARTITION BY department ORDER BY salary DESC);
```

---

## Common Patterns

### Running Total

```sql
SELECT
    date,
    amount,
    SUM(amount) OVER (ORDER BY date) AS running_total
FROM transactions;
```

### Percentage of Total

```sql
SELECT
    category,
    sales,
    ROUND(sales * 100.0 / SUM(sales) OVER (), 2) AS percentage
FROM category_sales;
```

### Top N per Group

```sql
SELECT *
FROM (
    SELECT
        *,
        ROW_NUMBER() OVER (
            PARTITION BY department
            ORDER BY salary DESC
        ) AS rn
    FROM employees
) sub
WHERE rn <= 3;
```

### Moving Average

```sql
SELECT
    date,
    price,
    AVG(price) OVER (
        ORDER BY date
        ROWS BETWEEN 6 PRECEDING AND CURRENT ROW
    ) AS ma_7
FROM stock_prices;
```

### Year-over-Year Comparison

```sql
SELECT
    year,
    month,
    revenue,
    LAG(revenue, 12) OVER (ORDER BY year, month) AS prev_year_revenue,
    ROUND((revenue - LAG(revenue, 12) OVER (ORDER BY year, month)) * 100.0
        / LAG(revenue, 12) OVER (ORDER BY year, month), 2) AS yoy_change
FROM monthly_revenue;
```

### Gap Analysis

```sql
SELECT
    id,
    event_time,
    event_time - LAG(event_time) OVER (ORDER BY event_time) AS time_since_last
FROM events;
```

### Cumulative Distribution

```sql
SELECT
    score,
    CUME_DIST() OVER (ORDER BY score) AS percentile
FROM exam_results;
```

---

## Performance Tips

1. **Indexes** help ORDER BY in windows
2. **Partition wisely** - smaller partitions = faster
3. **Limit frame size** when possible
4. **Named windows** are clearer but don't improve performance

---

## See Also

- [Aggregate Functions](aggregate-functions.md)
- [SELECT](../dml/select.md)
- [Performance Tuning](../../admin/performance-tuning.md)
