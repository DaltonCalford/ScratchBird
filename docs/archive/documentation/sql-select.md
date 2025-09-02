### SELECT Queries

**What it is**

The SELECT statement is the cornerstone of SQL data retrieval, providing a powerful and flexible way to query data from tables, views, and subqueries. ScratchBird implements comprehensive SELECT functionality including CTEs, complex joins, window functions, set operations, and advanced features like LATERAL joins and recursive queries.

**Why it matters**

- **Data Analysis**: SELECT enables complex analytical queries with grouping, aggregation, and window functions
- **Performance**: Understanding join algorithms and query planning helps write efficient queries
- **Flexibility**: CTEs and subqueries allow breaking complex logic into manageable pieces
- **Set Operations**: UNION, INTERSECT, and EXCEPT enable powerful data combination patterns

**How to use it**

Build SELECT queries progressively: start with basic projections and filters, then add joins, grouping, and advanced features as needed. Use EXPLAIN to understand query plans and optimize performance.

## SELECT Statement Structure

The complete SELECT syntax (`src/engine/parser_select.cpp`):

```sql
[WITH [RECURSIVE] cte_name [(columns)] AS (query) [, ...]]
SELECT [DISTINCT [ON (columns)]] projection_list
FROM table_references
[JOIN clauses]
[WHERE conditions]
[GROUP BY expressions]
[HAVING conditions]
[WINDOW window_definitions]
[ORDER BY expressions]
[LIMIT/FETCH clauses]
[FOR UPDATE options]
[PLAN hints]
```

## Common Table Expressions (CTEs)

### Basic CTEs

CTEs provide named temporary result sets within a query:

```sql
WITH regional_sales AS (
    SELECT region, SUM(amount) as total_sales
    FROM orders
    GROUP BY region
),
top_regions AS (
    SELECT region
    FROM regional_sales
    WHERE total_sales > 1000000
)
SELECT * FROM orders
WHERE region IN (SELECT region FROM top_regions);
```

### Recursive CTEs

Recursive CTEs enable hierarchical and graph traversal:

```sql
WITH RECURSIVE employee_hierarchy AS (
    -- Anchor: top-level employees
    SELECT emp_id, name, manager_id, 1 as level
    FROM employees
    WHERE manager_id IS NULL
    
    UNION ALL
    
    -- Recursive: employees under current level
    SELECT e.emp_id, e.name, e.manager_id, h.level + 1
    FROM employees e
    JOIN employee_hierarchy h ON e.manager_id = h.emp_id
)
SELECT * FROM employee_hierarchy
ORDER BY level, name;
```

## FROM Clause and Table References

### Basic Table References

```sql
-- Simple table
SELECT * FROM users;

-- Table with alias
SELECT u.* FROM users u;

-- Qualified schema.table
SELECT * FROM public.users;

-- Database link notation
SELECT * FROM users@remote_db;
```

### Subqueries as Tables

```sql
-- Derived table
SELECT dept_name, avg_salary
FROM (
    SELECT d.name as dept_name, AVG(e.salary) as avg_salary
    FROM departments d
    JOIN employees e ON d.id = e.dept_id
    GROUP BY d.name
) dept_stats
WHERE avg_salary > 50000;
```

### LATERAL Joins

LATERAL allows subqueries to reference columns from preceding FROM items:

```sql
SELECT u.username, recent.order_date, recent.total
FROM users u,
LATERAL (
    SELECT order_date, total
    FROM orders o
    WHERE o.user_id = u.id
    ORDER BY order_date DESC
    LIMIT 3
) recent;
```

## JOIN Operations

ScratchBird supports all standard SQL join types (`include/scratchbird/engine/parser_select.h`):

### Join Types

```sql
-- INNER JOIN (default)
SELECT * FROM orders o
JOIN customers c ON o.customer_id = c.id;

-- LEFT OUTER JOIN
SELECT c.name, COUNT(o.id) as order_count
FROM customers c
LEFT JOIN orders o ON c.id = o.customer_id
GROUP BY c.id, c.name;

-- RIGHT OUTER JOIN
SELECT * FROM orders o
RIGHT JOIN customers c ON o.customer_id = c.id;

-- FULL OUTER JOIN
SELECT COALESCE(s.date, t.date) as date,
       s.amount as source_amount,
       t.amount as target_amount
FROM source_data s
FULL JOIN target_data t ON s.date = t.date;

-- CROSS JOIN (Cartesian product)
SELECT * FROM colors
CROSS JOIN sizes;

-- NATURAL JOIN (join on common columns)
SELECT * FROM employees
NATURAL JOIN departments;
```

### Join Conditions

```sql
-- ON clause
SELECT * FROM orders o
JOIN order_items oi ON o.id = oi.order_id;

-- USING clause (when column names match)
SELECT * FROM employees e
JOIN departments d USING (dept_id);

-- Multiple join conditions
SELECT * FROM orders o
JOIN customers c ON o.customer_id = c.id
                AND o.order_date >= c.registration_date;
```

### Complex Join Trees

The parser builds right-deep join trees for complex multi-table joins:

```sql
-- Multiple joins
SELECT 
    c.name as customer,
    p.name as product,
    oi.quantity,
    oi.price
FROM customers c
JOIN orders o ON c.id = o.customer_id
JOIN order_items oi ON o.id = oi.order_id
JOIN products p ON oi.product_id = p.id
WHERE o.order_date >= DATE '2024-01-01';

-- Parenthesized join groups
SELECT *
FROM (customers c JOIN orders o ON c.id = o.customer_id)
JOIN (order_items oi JOIN products p ON oi.product_id = p.id)
  ON o.id = oi.order_id;
```

## Projection and Expressions

### Column Selection

```sql
-- All columns
SELECT * FROM users;

-- Specific columns
SELECT id, username, email FROM users;

-- Qualified columns
SELECT u.id, u.username, p.bio
FROM users u
JOIN profiles p ON u.id = p.user_id;

-- Expressions
SELECT 
    id,
    first_name || ' ' || last_name AS full_name,
    EXTRACT(YEAR FROM AGE(birth_date)) AS age,
    salary * 1.1 AS projected_salary
FROM employees;
```

### DISTINCT and DISTINCT ON

```sql
-- Remove duplicates
SELECT DISTINCT department FROM employees;

-- DISTINCT ON (PostgreSQL-style)
SELECT DISTINCT ON (customer_id) 
    customer_id, 
    order_date, 
    total
FROM orders
ORDER BY customer_id, order_date DESC;  -- Latest order per customer
```

## WHERE Clause Filtering

WHERE conditions are normalized via the expression parser:

```sql
-- Simple conditions
SELECT * FROM products
WHERE price > 100;

-- Complex boolean logic
SELECT * FROM orders
WHERE status = 'pending'
  AND (priority = 'high' OR customer_type = 'premium')
  AND order_date BETWEEN DATE '2024-01-01' AND DATE '2024-12-31';

-- Subquery conditions
SELECT * FROM employees e
WHERE salary > (
    SELECT AVG(salary) 
    FROM employees 
    WHERE dept_id = e.dept_id
);

-- EXISTS/NOT EXISTS
SELECT * FROM customers c
WHERE EXISTS (
    SELECT 1 FROM orders o
    WHERE o.customer_id = c.id
      AND o.order_date >= DATE '2024-01-01'
);
```

## GROUP BY and Aggregation

### Basic Grouping

```sql
-- Simple GROUP BY
SELECT department, COUNT(*) as emp_count, AVG(salary) as avg_salary
FROM employees
GROUP BY department;

-- Multiple grouping columns
SELECT 
    EXTRACT(YEAR FROM order_date) as year,
    EXTRACT(MONTH FROM order_date) as month,
    COUNT(*) as order_count,
    SUM(total) as revenue
FROM orders
GROUP BY EXTRACT(YEAR FROM order_date), 
         EXTRACT(MONTH FROM order_date);
```

### HAVING Clause

```sql
-- Filter groups
SELECT 
    department,
    COUNT(*) as emp_count,
    AVG(salary) as avg_salary
FROM employees
GROUP BY department
HAVING COUNT(*) > 10 
   AND AVG(salary) > 50000;
```

### Advanced Grouping Sets

```sql
-- ROLLUP
SELECT 
    region,
    product_category,
    SUM(sales) as total_sales
FROM sales_data
GROUP BY ROLLUP(region, product_category);

-- CUBE
SELECT 
    year,
    quarter,
    region,
    SUM(revenue) as total_revenue
FROM quarterly_results
GROUP BY CUBE(year, quarter, region);

-- GROUPING SETS
SELECT 
    department,
    job_title,
    COUNT(*) as count
FROM employees
GROUP BY GROUPING SETS (
    (department),
    (job_title),
    (department, job_title),
    ()  -- Grand total
);
```

## Window Functions

Window specifications (`include/scratchbird/engine/parser_select.h::WindowSpec`):

```sql
-- ROW_NUMBER
SELECT 
    name,
    department,
    salary,
    ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) as rank
FROM employees;

-- Running totals
SELECT 
    order_date,
    amount,
    SUM(amount) OVER (ORDER BY order_date 
                      ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) as running_total
FROM orders;

-- Moving average
SELECT 
    date,
    value,
    AVG(value) OVER (ORDER BY date 
                     ROWS BETWEEN 2 PRECEDING AND 2 FOLLOWING) as moving_avg_5
FROM time_series;

-- Named windows
SELECT 
    name,
    department,
    salary,
    RANK() OVER w as dept_rank,
    PERCENT_RANK() OVER w as percentile
FROM employees
WINDOW w AS (PARTITION BY department ORDER BY salary DESC);
```

## ORDER BY and Result Ordering

### Basic Ordering

```sql
-- Simple ORDER BY
SELECT * FROM products
ORDER BY price DESC;

-- Multiple columns
SELECT * FROM employees
ORDER BY department, salary DESC, hire_date;

-- Expressions
SELECT name, birth_date
FROM users
ORDER BY EXTRACT(MONTH FROM birth_date), 
         EXTRACT(DAY FROM birth_date);
```

### NULLS Handling

```sql
-- NULLS FIRST/LAST
SELECT * FROM products
ORDER BY discount_rate DESC NULLS LAST;

-- Multiple columns with different NULL handling
SELECT * FROM employees
ORDER BY department NULLS FIRST,
         termination_date DESC NULLS LAST;
```

## LIMIT/FETCH Clauses

### LIMIT/OFFSET

```sql
-- Basic pagination
SELECT * FROM products
ORDER BY created_at DESC
LIMIT 10 OFFSET 20;

-- Variables in LIMIT
SELECT * FROM logs
ORDER BY timestamp DESC
LIMIT :page_size OFFSET :page_size * (:page_number - 1);
```

### SQL Standard FETCH

```sql
-- FETCH FIRST
SELECT * FROM products
ORDER BY price DESC
FETCH FIRST 10 ROWS ONLY;

-- FETCH NEXT with OFFSET
SELECT * FROM products
ORDER BY price DESC
OFFSET 10 ROWS
FETCH NEXT 10 ROWS ONLY;

-- WITH TIES (include tied rows)
SELECT * FROM employees
ORDER BY salary DESC
FETCH FIRST 5 ROWS WITH TIES;
```

## Set Operations

Set operations follow SQL precedence: INTERSECT > UNION/EXCEPT

```sql
-- UNION (remove duplicates)
SELECT customer_id FROM orders_2023
UNION
SELECT customer_id FROM orders_2024;

-- UNION ALL (keep duplicates)
SELECT product_id, 'order' as source FROM order_items
UNION ALL
SELECT product_id, 'cart' as source FROM shopping_carts;

-- INTERSECT (common rows)
SELECT customer_id FROM high_value_customers
INTERSECT
SELECT customer_id FROM loyal_customers;

-- EXCEPT (difference)
SELECT product_id FROM all_products
EXCEPT
SELECT product_id FROM discontinued_products;

-- Complex set operations with precedence
SELECT id FROM table1
UNION
SELECT id FROM table2
INTERSECT  -- Higher precedence, evaluated first
SELECT id FROM table3;
-- Equivalent to: table1 UNION (table2 INTERSECT table3)
```

## FOR UPDATE Locking

Row-level locking for transaction control:

```sql
-- Basic FOR UPDATE
SELECT * FROM accounts
WHERE id = 123
FOR UPDATE;

-- With options
SELECT * FROM inventory
WHERE product_id = 456
FOR UPDATE NOWAIT;  -- Don't wait for locks

-- SKIP LOCKED rows
SELECT * FROM job_queue
WHERE status = 'pending'
ORDER BY priority DESC
LIMIT 1
FOR UPDATE SKIP LOCKED;

-- Lock specific columns
SELECT id, balance FROM accounts
WHERE customer_id = 789
FOR UPDATE OF balance;
```

## PLAN Hints

Query optimizer hints (`include/scratchbird/engine/parser_select.h::PlanOp`):

```sql
-- Index hints
SELECT * FROM large_table
PLAN (large_table INDEX (idx_column));

-- Join order hints
SELECT * FROM a
JOIN b ON a.id = b.a_id
JOIN c ON b.id = c.b_id
PLAN (a NATURAL, b INDEX (idx_a_id), c INDEX (idx_b_id));

-- Access method hints
SELECT * FROM employees e
JOIN departments d ON e.dept_id = d.id
PLAN (e ORDER idx_salary, d NATURAL);
```

## Complex Query Examples

### Analytics Query with CTEs and Windows

```sql
WITH monthly_sales AS (
    SELECT 
        DATE_TRUNC('month', order_date) as month,
        product_category,
        SUM(amount) as total_sales
    FROM orders o
    JOIN order_items oi ON o.id = oi.order_id
    JOIN products p ON oi.product_id = p.id
    WHERE order_date >= DATE '2024-01-01'
    GROUP BY DATE_TRUNC('month', order_date), product_category
),
ranked_categories AS (
    SELECT 
        month,
        product_category,
        total_sales,
        RANK() OVER (PARTITION BY month ORDER BY total_sales DESC) as category_rank,
        total_sales / SUM(total_sales) OVER (PARTITION BY month) * 100 as pct_of_month
    FROM monthly_sales
)
SELECT 
    TO_CHAR(month, 'YYYY-MM') as month_str,
    product_category,
    total_sales,
    ROUND(pct_of_month, 2) as market_share_pct
FROM ranked_categories
WHERE category_rank <= 5
ORDER BY month, category_rank;
```

### Hierarchical Query with Recursive CTE

```sql
WITH RECURSIVE category_tree AS (
    -- Root categories
    SELECT 
        id,
        name,
        parent_id,
        name as path,
        0 as depth
    FROM categories
    WHERE parent_id IS NULL
    
    UNION ALL
    
    -- Subcategories
    SELECT 
        c.id,
        c.name,
        c.parent_id,
        t.path || ' > ' || c.name,
        t.depth + 1
    FROM categories c
    JOIN category_tree t ON c.parent_id = t.id
    WHERE t.depth < 10  -- Prevent infinite recursion
)
SELECT 
    REPEAT('  ', depth) || name as indented_name,
    path,
    depth
FROM category_tree
ORDER BY path;
```

## Implementation Details

**Parser Structure** (`src/engine/parser_select.cpp`):
- `parse_select_minimal`: Main entry point
- `SelectQuery` struct: Captures all SELECT components
- `JoinTree`: Represents join relationships
- `SetTree`: Handles set operations with precedence

**Key Data Structures**:
- `FromItem`: Table references and subqueries
- `JoinClause`: Join specifications
- `OrderItem`: ORDER BY expressions
- `WindowSpec`: Window function definitions
- `ForUpdateSpec`: Locking options

**Code Anchors**:
- Main parser: `src/engine/parser_select.cpp` (parse_select_minimal)
- Query structure: `include/scratchbird/engine/parser_select.h` (SelectQuery)
- Join tree building: Right-deep tree construction for optimizer
- Set operation precedence: INTERSECT binds tighter than UNION/EXCEPT

## See also

- [Operators](./sql-operators.md) - Expression operators and precedence
- [DML Operations](./sql-dml.md) - Data modification statements
- [EXPLAIN/ANALYZE](./explain-analyze.md) - Query plan analysis
- [Window Functions](./sql-overview.md) - Advanced analytical functions
- [PSQL Runtime](./psql-runtime.md) - Using SELECT in procedural code