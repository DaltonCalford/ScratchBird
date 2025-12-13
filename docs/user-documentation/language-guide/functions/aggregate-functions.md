# Aggregate Functions

Functions that operate on groups of rows.

[Back to Functions Index](index.md) | [Back to Language Guide](../index.md)

---

## Basic Aggregates

| Function | Description | Example |
|----------|-------------|---------|
| `COUNT(*)` | Count all rows | `COUNT(*)` → `100` |
| `COUNT(col)` | Count non-NULL | `COUNT(email)` → `95` |
| `COUNT(DISTINCT col)` | Count unique | `COUNT(DISTINCT country)` → `25` |
| `SUM(col)` | Sum values | `SUM(total)` → `15000.00` |
| `AVG(col)` | Average | `AVG(price)` → `29.99` |
| `MIN(col)` | Minimum | `MIN(created_at)` → `2020-01-01` |
| `MAX(col)` | Maximum | `MAX(price)` → `999.99` |

---

## Statistical Aggregates

| Function | Description |
|----------|-------------|
| `STDDEV(col)` | Standard deviation (sample) |
| `STDDEV_POP(col)` | Standard deviation (population) |
| `VARIANCE(col)` | Variance (sample) |
| `VAR_POP(col)` | Variance (population) |
| `CORR(y, x)` | Correlation coefficient |
| `COVAR_POP(y, x)` | Population covariance |
| `COVAR_SAMP(y, x)` | Sample covariance |
| `REGR_SLOPE(y, x)` | Linear regression slope |
| `REGR_INTERCEPT(y, x)` | Linear regression intercept |

---

## Boolean Aggregates

| Function | Description | Example |
|----------|-------------|---------|
| `BOOL_AND(col)` | All true? | `BOOL_AND(active)` |
| `BOOL_OR(col)` | Any true? | `BOOL_OR(verified)` |
| `EVERY(col)` | Same as BOOL_AND | `EVERY(active)` |

---

## Array Aggregates

| Function | Description | Example |
|----------|-------------|---------|
| `ARRAY_AGG(col)` | Collect to array | `ARRAY_AGG(name)` → `{Alice,Bob}` |
| `STRING_AGG(col, sep)` | Concatenate with separator | See below |

```sql
-- Comma-separated list
SELECT STRING_AGG(name, ', ') FROM users;
→ 'Alice, Bob, Carol'

-- With ordering
SELECT STRING_AGG(name, ', ' ORDER BY name) FROM users;
→ 'Alice, Bob, Carol'

-- Distinct values
SELECT STRING_AGG(DISTINCT category, ', ') FROM products;
```

---

## JSON Aggregates

| Function | Description |
|----------|-------------|
| `JSON_AGG(col)` | Collect to JSON array |
| `JSONB_AGG(col)` | Collect to JSONB array |
| `JSON_OBJECT_AGG(k, v)` | Collect to JSON object |
| `JSONB_OBJECT_AGG(k, v)` | Collect to JSONB object |

```sql
SELECT JSON_AGG(name) FROM users;
→ '["Alice", "Bob", "Carol"]'

SELECT JSON_OBJECT_AGG(id, name) FROM users;
→ '{"1": "Alice", "2": "Bob"}'
```

---

## GROUP BY

```sql
-- Basic grouping
SELECT country, COUNT(*) AS user_count
FROM users
GROUP BY country;

-- Multiple columns
SELECT country, city, COUNT(*)
FROM users
GROUP BY country, city;

-- With expressions
SELECT DATE_TRUNC('month', created_at) AS month, SUM(total)
FROM orders
GROUP BY DATE_TRUNC('month', created_at);
```

---

## HAVING

Filter groups after aggregation:

```sql
SELECT country, COUNT(*) AS user_count
FROM users
GROUP BY country
HAVING COUNT(*) > 100;

-- Multiple conditions
SELECT category, AVG(price) AS avg_price
FROM products
GROUP BY category
HAVING COUNT(*) > 10 AND AVG(price) < 100;
```

---

## ORDER BY with Aggregates

```sql
SELECT category, SUM(sales) AS total_sales
FROM products
GROUP BY category
ORDER BY total_sales DESC;

-- Using column number
SELECT category, SUM(sales) AS total_sales
FROM products
GROUP BY 1
ORDER BY 2 DESC;
```

---

## ROLLUP

Hierarchical subtotals:

```sql
SELECT
    country,
    city,
    SUM(sales) AS total
FROM stores
GROUP BY ROLLUP (country, city);
```

Result:
```
country | city     | total
--------|----------|------
USA     | New York | 1000
USA     | Chicago  | 800
USA     | NULL     | 1800    ← Country subtotal
Canada  | Toronto  | 600
Canada  | NULL     | 600     ← Country subtotal
NULL    | NULL     | 2400    ← Grand total
```

---

## CUBE

All combinations:

```sql
SELECT
    country,
    category,
    SUM(sales) AS total
FROM sales
GROUP BY CUBE (country, category);
```

---

## GROUPING SETS

Specific groupings:

```sql
SELECT
    country,
    city,
    SUM(sales)
FROM stores
GROUP BY GROUPING SETS (
    (country, city),  -- Detail
    (country),        -- By country
    ()                -- Grand total
);
```

---

## FILTER Clause

Conditional aggregation:

```sql
SELECT
    COUNT(*) AS total,
    COUNT(*) FILTER (WHERE status = 'active') AS active,
    COUNT(*) FILTER (WHERE status = 'inactive') AS inactive
FROM users;

-- With sum
SELECT
    SUM(amount) AS total,
    SUM(amount) FILTER (WHERE type = 'credit') AS credits,
    SUM(amount) FILTER (WHERE type = 'debit') AS debits
FROM transactions;
```

---

## DISTINCT in Aggregates

```sql
-- Count unique values
SELECT COUNT(DISTINCT category) FROM products;

-- Average of distinct values
SELECT AVG(DISTINCT price) FROM products;

-- Concatenate unique
SELECT STRING_AGG(DISTINCT category, ', ') FROM products;
```

---

## Examples

### Sales Summary

```sql
SELECT
    DATE_TRUNC('month', order_date) AS month,
    COUNT(*) AS order_count,
    SUM(total) AS revenue,
    AVG(total) AS avg_order,
    COUNT(DISTINCT customer_id) AS unique_customers
FROM orders
GROUP BY DATE_TRUNC('month', order_date)
ORDER BY month;
```

### Top Categories

```sql
SELECT
    category,
    COUNT(*) AS product_count,
    ROUND(AVG(price), 2) AS avg_price,
    SUM(quantity_sold) AS total_sold
FROM products
GROUP BY category
HAVING SUM(quantity_sold) > 100
ORDER BY total_sold DESC
LIMIT 10;
```

### Running Percentage

```sql
SELECT
    category,
    SUM(sales) AS category_sales,
    ROUND(SUM(sales) * 100.0 / SUM(SUM(sales)) OVER (), 2) AS percentage
FROM products
GROUP BY category
ORDER BY category_sales DESC;
```

### Conditional Counts

```sql
SELECT
    DATE_TRUNC('day', created_at) AS day,
    COUNT(*) AS total_orders,
    COUNT(*) FILTER (WHERE status = 'completed') AS completed,
    COUNT(*) FILTER (WHERE status = 'cancelled') AS cancelled,
    ROUND(
        COUNT(*) FILTER (WHERE status = 'completed') * 100.0 / COUNT(*),
        2
    ) AS completion_rate
FROM orders
GROUP BY DATE_TRUNC('day', created_at)
ORDER BY day;
```

---

## NULL Handling

- `COUNT(*)` counts all rows including NULLs
- `COUNT(column)` excludes NULLs
- `SUM`, `AVG` ignore NULLs
- `SUM` of all NULLs returns NULL (use `COALESCE`)

```sql
SELECT
    COALESCE(SUM(amount), 0) AS total,  -- 0 if all NULL
    COUNT(amount) AS non_null_count,
    COUNT(*) AS total_count
FROM transactions;
```

---

## See Also

- [Window Functions](window-functions.md)
- [SELECT](../dml/select.md)
- [GROUP BY](../dml/select.md#group-by)
