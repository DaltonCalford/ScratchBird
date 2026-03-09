# Window Functions

[Prev](./04_select_filters_grouping_having.md) | [Next](./06_select_set_operations_and_subqueries.md) | [Topic README](./README.md) | [DML README](./README.md) | [Syntax Guide README](../README.md)

## Synopsis

Window functions perform calculations across sets of rows related to the current row.

## Syntax

```sql
function_name ([ expression [, expression ... ] ]) [ FILTER ( WHERE filter_clause ) ] OVER window_name
function_name ([ expression [, expression ... ] ]) [ FILTER ( WHERE filter_clause ) ] OVER ( window_definition )

where window_definition is:
    [ existing_window_name ]
    [ PARTITION BY expression [, ...] ]
    [ ORDER BY expression [ ASC | DESC | USING operator ] [ NULLS { FIRST | LAST } ] [, ...] ]
    [ frame_clause ]

where frame_clause is:
    { RANGE | ROWS | GROUPS } frame_start [ frame_exclusion ]
    { RANGE | ROWS | GROUPS } BETWEEN frame_start AND frame_end [ frame_exclusion ]

where frame_start and frame_end are:
    UNBOUNDED PRECEDING
    offset PRECEDING
    CURRENT ROW
    offset FOLLOWING
    UNBOUNDED FOLLOWING
```

## Built-in Window Functions

### Aggregate Window Functions

| Function | Description |
|----------|-------------|
| `AVG(expr)` | Average |
| `SUM(expr)` | Sum |
| `COUNT(*)` | Count rows |
| `COUNT(expr)` | Count non-null |
| `MIN(expr)` | Minimum |
| `MAX(expr)` | Maximum |
| `STDDEV(expr)` | Standard deviation |

### Ranking Functions

| Function | Description |
|----------|-------------|
| `ROW_NUMBER()` | Unique row number (1, 2, 3...) |
| `RANK()` | Rank with gaps (1, 1, 3...) |
| `DENSE_RANK()` | Rank without gaps (1, 1, 2...) |
| `PERCENT_RANK()` | Percent rank (0 to 1) |
| `CUME_DIST()` | Cumulative distribution |
| `NTILE(num_buckets)` | Divide into buckets |

### Value Functions

| Function | Description |
|----------|-------------|
| `FIRST_VALUE(expr)` | First value in frame |
| `LAST_VALUE(expr)` | Last value in frame |
| `NTH_VALUE(expr, n)` | Nth value in frame |
| `LAG(expr [, offset [, default]])` | Value from previous row |
| `LEAD(expr [, offset [, default]])` | Value from next row |

## Examples

### Basic Window Functions

```sql
-- Row number
SELECT 
    name,
    department,
    salary,
    ROW_NUMBER() OVER (ORDER BY salary DESC) as rank
FROM employees;

-- Partition by department
SELECT 
    name,
    department,
    salary,
    RANK() OVER (PARTITION BY department ORDER BY salary DESC) as dept_rank
FROM employees;
```

### Running Totals

```sql
-- Running total
SELECT 
    date,
    amount,
    SUM(amount) OVER (ORDER BY date) as running_total
FROM sales;

-- Running total by category
SELECT 
    date,
    category,
    amount,
    SUM(amount) OVER (PARTITION BY category ORDER BY date) as category_running_total
FROM sales;
```

### Moving Averages

```sql
-- 7-day moving average
SELECT 
    date,
    amount,
    AVG(amount) OVER (
        ORDER BY date 
        ROWS BETWEEN 6 PRECEDING AND CURRENT ROW
    ) as moving_avg
FROM daily_sales;
```

### LAG/LEAD

```sql
-- Compare to previous day
SELECT 
    date,
    amount,
    LAG(amount) OVER (ORDER BY date) as previous_day,
    amount - LAG(amount) OVER (ORDER BY date) as change
FROM daily_sales;

-- Year-over-year comparison
SELECT 
    month,
    year,
    sales,
    LAG(sales, 12) OVER (ORDER BY year, month) as sales_last_year
FROM monthly_sales;
```

### FIRST_VALUE/LAST_VALUE

```sql
-- First and last sale in each department
SELECT 
    name,
    department,
    salary,
    FIRST_VALUE(name) OVER (
        PARTITION BY department 
        ORDER BY salary DESC
    ) as highest_paid_in_dept,
    LAST_VALUE(name) OVER (
        PARTITION BY department 
        ORDER BY salary DESC
        RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
    ) as lowest_paid_in_dept
FROM employees;
```

### NTILE

```sql
-- Quartiles
SELECT 
    name,
    salary,
    NTILE(4) OVER (ORDER BY salary) as quartile
FROM employees;

-- Percentiles
SELECT 
    name,
    salary,
    NTILE(100) OVER (ORDER BY salary) as percentile
FROM employees;
```

### Named Windows

```sql
-- Reuse window definition
SELECT 
    name,
    department,
    salary,
    ROW_NUMBER() OVER w as row_num,
    RANK() OVER w as rank,
    AVG(salary) OVER w as avg_salary
FROM employees
WINDOW w AS (PARTITION BY department ORDER BY salary DESC);
```

## Frame Specifications

### ROWS

```sql
-- Current row and 2 preceding
ROWS BETWEEN 2 PRECEDING AND CURRENT ROW

-- All rows from start to current
ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW

-- 5 rows centered on current
ROWS BETWEEN 2 PRECEDING AND 2 FOLLOWING
```

### RANGE

```sql
-- All rows with same ORDER BY value
RANGE CURRENT ROW

-- All rows up to 10 units before
RANGE BETWEEN 10 PRECEDING AND CURRENT ROW
```

## Common Patterns

### Top N per Group

```sql
-- Top 3 salaries per department
WITH ranked AS (
    SELECT 
        name,
        department,
        salary,
        ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) as rn
    FROM employees
)
SELECT * FROM ranked WHERE rn <= 3;
```

### Gaps and Islands

```sql
-- Find gaps in sequence
SELECT 
    id,
    LEAD(id) OVER (ORDER BY id) - id as gap
FROM sequence_table;
```

### Running Difference

```sql
-- Day-over-day change
SELECT 
    date,
    amount,
    amount - LAG(amount) OVER (ORDER BY date) as daily_change
FROM sales;
```

## See Also

- [SELECT Core](01_select_core_syntax.md)
- [CTEs](12_cte_and_recursive_query_syntax.md)
