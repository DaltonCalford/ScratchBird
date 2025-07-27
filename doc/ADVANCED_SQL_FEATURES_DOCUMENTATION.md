# ScratchBird Advanced SQL Features Documentation

**Database Engine**: ScratchBird Alpha 0.6.0  
**Document Version**: 1.0  
**Date**: July 2025  
**SQL Dialect**: 4 (Enhanced) - Default  

## Table of Contents

1. [Common Table Expressions (CTEs)](#common-table-expressions-ctes)
2. [Window Functions](#window-functions)
3. [Recursive Queries](#recursive-queries)
4. [Set Operations](#set-operations)
5. [CASE Expressions](#case-expressions)
6. [Subquery Expressions](#subquery-expressions)
7. [Lateral Joins](#lateral-joins)
8. [Array Operations](#array-operations)
9. [JSON Operations](#json-operations)
10. [Pivot Operations](#pivot-operations)
11. [Time Series Functions](#time-series-functions)
12. [Analytical Functions](#analytical-functions)
13. [Full-Text Search](#full-text-search)
14. [Spatial Operations](#spatial-operations)
15. [Advanced Aggregation](#advanced-aggregation)
16. [Dynamic SQL](#dynamic-sql)
17. [Hierarchical Schema Operations](#hierarchical-schema-operations)
18. [Database Link Operations](#database-link-operations)
19. [Performance Optimization Features](#performance-optimization-features)
20. [Error Handling](#error-handling)

---

## Common Table Expressions (CTEs)

### Overview
Common Table Expressions provide a way to define temporary result sets that exist only for the duration of a single SQL statement. ScratchBird supports both recursive and non-recursive CTEs.

### Basic CTE Syntax

```sql
WITH cte_name [(column_list)] AS (
    -- CTE query definition
    SELECT ...
)
SELECT ... FROM cte_name;
```

### Simple CTE Examples

```sql
-- Basic CTE usage
WITH regional_sales AS (
    SELECT region, SUM(sales_amount) as total_sales
    FROM sales
    GROUP BY region
)
SELECT region, total_sales
FROM regional_sales
WHERE total_sales > 100000;

-- Multiple CTEs
WITH 
product_totals AS (
    SELECT product_id, SUM(quantity) as total_qty
    FROM order_items
    GROUP BY product_id
),
high_volume_products AS (
    SELECT product_id
    FROM product_totals
    WHERE total_qty > 1000
)
SELECT p.product_name, pt.total_qty
FROM products p
JOIN product_totals pt ON p.product_id = pt.product_id
JOIN high_volume_products hvp ON p.product_id = hvp.product_id;
```

### Recursive CTEs

```sql
-- Hierarchical data traversal
WITH RECURSIVE employee_hierarchy AS (
    -- Anchor member: top-level managers
    SELECT employee_id, name, manager_id, 0 as level
    FROM employees
    WHERE manager_id IS NULL
    
    UNION ALL
    
    -- Recursive member: subordinates
    SELECT e.employee_id, e.name, e.manager_id, eh.level + 1
    FROM employees e
    JOIN employee_hierarchy eh ON e.manager_id = eh.employee_id
)
SELECT employee_id, name, level,
       LPAD('', level * 2, ' ') || name as indented_name
FROM employee_hierarchy
ORDER BY level, name;

-- Number series generation
WITH RECURSIVE number_series AS (
    SELECT 1 as n
    UNION ALL
    SELECT n + 1
    FROM number_series
    WHERE n < 100
)
SELECT n FROM number_series;

-- Path finding in graphs
WITH RECURSIVE path_finder AS (
    SELECT start_node, end_node, ARRAY[start_node] as path, 0 as distance
    FROM connections
    WHERE start_node = 'A'
    
    UNION ALL
    
    SELECT c.start_node, c.end_node,
           pf.path || c.end_node,
           pf.distance + c.weight
    FROM connections c
    JOIN path_finder pf ON c.start_node = pf.end_node
    WHERE NOT c.end_node = ANY(pf.path)  -- Prevent cycles
      AND pf.distance < 10  -- Limit search depth
)
SELECT path, distance
FROM path_finder
WHERE end_node = 'Z'
ORDER BY distance
LIMIT 1;
```

---

## Window Functions

### Overview
Window functions perform calculations across sets of rows that are related to the current row, without requiring GROUP BY clauses.

### Basic Window Function Syntax

```sql
function_name([arguments]) OVER (
    [PARTITION BY partition_expression]
    [ORDER BY sort_expression]
    [frame_clause]
)
```

### Ranking Functions

```sql
-- ROW_NUMBER: Unique sequential integer
SELECT employee_id, department, salary,
       ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) as rank_in_dept
FROM employees;

-- RANK: Handles ties with gaps
SELECT employee_id, department, salary,
       RANK() OVER (ORDER BY salary DESC) as salary_rank
FROM employees;

-- DENSE_RANK: Handles ties without gaps
SELECT employee_id, department, salary,
       DENSE_RANK() OVER (ORDER BY salary DESC) as dense_rank
FROM employees;

-- NTILE: Divides rows into specified number of groups
SELECT employee_id, salary,
       NTILE(4) OVER (ORDER BY salary) as salary_quartile
FROM employees;

-- PERCENT_RANK: Relative rank as percentage
SELECT employee_id, salary,
       PERCENT_RANK() OVER (ORDER BY salary) as pct_rank
FROM employees;
```

### Analytical Functions

```sql
-- LAG/LEAD: Access previous/next row values
SELECT order_date, sales_amount,
       LAG(sales_amount, 1) OVER (ORDER BY order_date) as prev_sales,
       LEAD(sales_amount, 1) OVER (ORDER BY order_date) as next_sales,
       sales_amount - LAG(sales_amount, 1) OVER (ORDER BY order_date) as sales_diff
FROM daily_sales;

-- FIRST_VALUE/LAST_VALUE: First/last value in window
SELECT employee_id, department, salary,
       FIRST_VALUE(salary) OVER (
           PARTITION BY department 
           ORDER BY salary DESC
           ROWS UNBOUNDED PRECEDING
       ) as highest_dept_salary,
       LAST_VALUE(salary) OVER (
           PARTITION BY department 
           ORDER BY salary DESC
           ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
       ) as lowest_dept_salary
FROM employees;

-- NTH_VALUE: Nth value in window
SELECT order_id, order_date, sales_amount,
       NTH_VALUE(sales_amount, 2) OVER (
           ORDER BY order_date
           ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING
       ) as second_value
FROM orders;
```

### Aggregate Window Functions

```sql
-- Running totals and moving averages
SELECT order_date, sales_amount,
       SUM(sales_amount) OVER (
           ORDER BY order_date
           ROWS UNBOUNDED PRECEDING
       ) as running_total,
       AVG(sales_amount) OVER (
           ORDER BY order_date
           ROWS BETWEEN 6 PRECEDING AND CURRENT ROW
       ) as seven_day_avg,
       COUNT(*) OVER (
           ORDER BY order_date
           ROWS UNBOUNDED PRECEDING
       ) as cumulative_orders
FROM daily_sales;

-- Moving window calculations
SELECT product_id, sale_date, quantity,
       MIN(quantity) OVER (
           PARTITION BY product_id
           ORDER BY sale_date
           ROWS BETWEEN 2 PRECEDING AND 2 FOLLOWING
       ) as min_5day_qty,
       MAX(quantity) OVER (
           PARTITION BY product_id
           ORDER BY sale_date
           ROWS BETWEEN 2 PRECEDING AND 2 FOLLOWING
       ) as max_5day_qty
FROM product_sales;
```

### Frame Specifications

```sql
-- Different frame types
SELECT order_date, sales_amount,
       -- Rows frame: based on physical rows
       SUM(sales_amount) OVER (
           ORDER BY order_date
           ROWS BETWEEN 2 PRECEDING AND 2 FOLLOWING
       ) as sum_5_rows,
       
       -- Range frame: based on value ranges
       SUM(sales_amount) OVER (
           ORDER BY order_date
           RANGE BETWEEN INTERVAL '2' DAY PRECEDING 
               AND INTERVAL '2' DAY FOLLOWING
       ) as sum_5_days,
       
       -- Groups frame: based on peer groups
       AVG(sales_amount) OVER (
           ORDER BY order_date
           GROUPS BETWEEN 1 PRECEDING AND 1 FOLLOWING
       ) as avg_3_groups
FROM daily_sales;
```

---

## Recursive Queries

### Hierarchical Data Processing

```sql
-- Organization chart traversal
WITH RECURSIVE org_chart AS (
    -- Start with CEO
    SELECT employee_id, name, manager_id, 
           CAST(name AS VARCHAR(1000)) as hierarchy_path,
           0 as level
    FROM employees
    WHERE title = 'CEO'
    
    UNION ALL
    
    -- Add direct reports
    SELECT e.employee_id, e.name, e.manager_id,
           oc.hierarchy_path || ' -> ' || e.name,
           oc.level + 1
    FROM employees e
    JOIN org_chart oc ON e.manager_id = oc.employee_id
    WHERE oc.level < 10  -- Prevent infinite recursion
)
SELECT level, hierarchy_path, employee_id, name
FROM org_chart
ORDER BY level, name;

-- Bill of Materials explosion
WITH RECURSIVE bom_explosion AS (
    SELECT part_id, component_id, quantity, 1 as level
    FROM bill_of_materials
    WHERE part_id = 'PRODUCT_A'
    
    UNION ALL
    
    SELECT bom.part_id, bom.component_id, 
           be.quantity * bom.quantity,
           be.level + 1
    FROM bill_of_materials bom
    JOIN bom_explosion be ON bom.part_id = be.component_id
    WHERE be.level < 20
)
SELECT component_id, SUM(quantity) as total_quantity, MAX(level) as max_level
FROM bom_explosion
GROUP BY component_id
ORDER BY component_id;
```

### Graph Traversal

```sql
-- Shortest path algorithm
WITH RECURSIVE shortest_path AS (
    SELECT node_from, node_to, distance, 
           ARRAY[node_from, node_to] as path
    FROM graph_edges
    WHERE node_from = 'START'
    
    UNION ALL
    
    SELECT sp.node_from, ge.node_to,
           sp.distance + ge.distance,
           sp.path || ge.node_to
    FROM shortest_path sp
    JOIN graph_edges ge ON sp.node_to = ge.node_from
    WHERE NOT ge.node_to = ANY(sp.path)
      AND sp.distance + ge.distance < 1000
)
SELECT path, distance
FROM shortest_path
WHERE node_to = 'END'
ORDER BY distance
LIMIT 1;

-- Connected components
WITH RECURSIVE components AS (
    SELECT node_id, node_id as component_id
    FROM nodes
    WHERE node_id = (SELECT MIN(node_id) FROM nodes)
    
    UNION ALL
    
    SELECT e.node_to, c.component_id
    FROM components c
    JOIN edges e ON c.node_id = e.node_from
    WHERE e.node_to NOT IN (SELECT node_id FROM components)
)
SELECT component_id, COUNT(*) as component_size
FROM components
GROUP BY component_id;
```

---

## Set Operations

### UNION Operations

```sql
-- Basic UNION
SELECT customer_id, 'Active' as status
FROM active_customers
UNION
SELECT customer_id, 'Inactive' as status
FROM inactive_customers;

-- UNION ALL (includes duplicates)
SELECT product_name, 'Current' as category
FROM current_products
UNION ALL
SELECT product_name, 'Discontinued' as category
FROM discontinued_products;

-- Complex UNION with ordering
(SELECT order_id, order_date, 'Online' as channel
 FROM online_orders
 WHERE order_date >= DATE '2025-01-01')
UNION
(SELECT order_id, order_date, 'Retail' as channel
 FROM retail_orders
 WHERE order_date >= DATE '2025-01-01')
ORDER BY order_date DESC;
```

### INTERSECT and EXCEPT

```sql
-- INTERSECT: Common records
SELECT customer_id
FROM customers_2024
INTERSECT
SELECT customer_id
FROM customers_2025;

-- EXCEPT: Records in first set but not second
SELECT customer_id
FROM all_customers
EXCEPT
SELECT customer_id
FROM inactive_customers;

-- Complex set operations
SELECT product_id
FROM products_sold_q1
INTERSECT
(SELECT product_id
 FROM products_sold_q2
 UNION
 SELECT product_id
 FROM products_sold_q3);
```

---

## CASE Expressions

### Simple CASE

```sql
-- Simple CASE expression
SELECT employee_id, name,
       CASE department
           WHEN 'SALES' THEN 'Revenue Generation'
           WHEN 'IT' THEN 'Technology'
           WHEN 'HR' THEN 'Human Resources'
           ELSE 'Other'
       END as department_category
FROM employees;
```

### Searched CASE

```sql
-- Searched CASE expression
SELECT product_id, price,
       CASE 
           WHEN price < 10 THEN 'Budget'
           WHEN price BETWEEN 10 AND 50 THEN 'Standard'
           WHEN price BETWEEN 50 AND 100 THEN 'Premium'
           ELSE 'Luxury'
       END as price_category,
       
       CASE
           WHEN inventory_level = 0 THEN 'Out of Stock'
           WHEN inventory_level < 10 THEN 'Low Stock'
           WHEN inventory_level < 50 THEN 'Adequate'
           ELSE 'Well Stocked'
       END as stock_status
FROM products;

-- Nested CASE expressions
SELECT order_id, total_amount, customer_type,
       CASE customer_type
           WHEN 'PREMIUM' THEN
               CASE
                   WHEN total_amount > 1000 THEN total_amount * 0.15
                   WHEN total_amount > 500 THEN total_amount * 0.10
                   ELSE total_amount * 0.05
               END
           WHEN 'STANDARD' THEN
               CASE
                   WHEN total_amount > 500 THEN total_amount * 0.05
                   ELSE 0
               END
           ELSE 0
       END as discount_amount
FROM orders;
```

### Conditional Aggregation

```sql
-- Using CASE in aggregates
SELECT department,
       COUNT(*) as total_employees,
       COUNT(CASE WHEN salary > 50000 THEN 1 END) as high_earners,
       SUM(CASE WHEN gender = 'F' THEN 1 ELSE 0 END) as female_count,
       AVG(CASE WHEN experience_years > 5 THEN salary END) as avg_senior_salary
FROM employees
GROUP BY department;
```

---

## Subquery Expressions

### Scalar Subqueries

```sql
-- Scalar subquery in SELECT
SELECT customer_id, order_date, total_amount,
       (SELECT AVG(total_amount) FROM orders) as avg_order_amount,
       total_amount - (SELECT AVG(total_amount) FROM orders) as diff_from_avg
FROM orders;

-- Scalar subquery in WHERE
SELECT product_id, product_name, price
FROM products
WHERE price > (SELECT AVG(price) FROM products);
```

### Correlated Subqueries

```sql
-- EXISTS with correlated subquery
SELECT customer_id, customer_name
FROM customers c
WHERE EXISTS (
    SELECT 1
    FROM orders o
    WHERE o.customer_id = c.customer_id
      AND o.order_date >= DATE '2025-01-01'
);

-- NOT EXISTS
SELECT product_id, product_name
FROM products p
WHERE NOT EXISTS (
    SELECT 1
    FROM order_items oi
    WHERE oi.product_id = p.product_id
);

-- Correlated subquery with aggregation
SELECT customer_id, customer_name,
       (SELECT COUNT(*)
        FROM orders o
        WHERE o.customer_id = c.customer_id) as order_count,
       (SELECT MAX(order_date)
        FROM orders o
        WHERE o.customer_id = c.customer_id) as last_order_date
FROM customers c;
```

### ANY/ALL/SOME

```sql
-- ANY operator
SELECT product_id, price
FROM products
WHERE price > ANY (
    SELECT price
    FROM products
    WHERE category = 'Electronics'
);

-- ALL operator
SELECT customer_id, total_amount
FROM orders
WHERE total_amount > ALL (
    SELECT AVG(total_amount)
    FROM orders
    GROUP BY EXTRACT(MONTH FROM order_date)
);

-- SOME (synonym for ANY)
SELECT employee_id, salary
FROM employees
WHERE salary >= SOME (
    SELECT MAX(salary)
    FROM employees
    GROUP BY department
);
```

---

## Lateral Joins

### Basic LATERAL Usage

```sql
-- LATERAL with function calls
SELECT c.customer_id, c.customer_name, 
       recent_orders.order_id, recent_orders.order_date
FROM customers c
CROSS JOIN LATERAL (
    SELECT order_id, order_date
    FROM orders o
    WHERE o.customer_id = c.customer_id
    ORDER BY order_date DESC
    LIMIT 3
) as recent_orders;

-- LATERAL with aggregation
SELECT d.department_name,
       dept_stats.avg_salary,
       dept_stats.employee_count
FROM departments d
LEFT JOIN LATERAL (
    SELECT AVG(salary) as avg_salary,
           COUNT(*) as employee_count
    FROM employees e
    WHERE e.department_id = d.department_id
) as dept_stats ON true;
```

### Advanced LATERAL Examples

```sql
-- LATERAL with complex calculations
SELECT p.product_id, p.product_name,
       sales_analysis.total_revenue,
       sales_analysis.units_sold,
       sales_analysis.avg_price
FROM products p
LEFT JOIN LATERAL (
    SELECT SUM(oi.quantity * oi.unit_price) as total_revenue,
           SUM(oi.quantity) as units_sold,
           AVG(oi.unit_price) as avg_price
    FROM order_items oi
    JOIN orders o ON oi.order_id = o.order_id
    WHERE oi.product_id = p.product_id
      AND o.order_date >= CURRENT_DATE - INTERVAL '90' DAY
) as sales_analysis ON true;

-- LATERAL with window functions
SELECT region,
       monthly_data.month,
       monthly_data.sales,
       monthly_data.running_total
FROM (SELECT DISTINCT region FROM sales) r
CROSS JOIN LATERAL (
    SELECT DATE_TRUNC('MONTH', sale_date) as month,
           SUM(amount) as sales,
           SUM(SUM(amount)) OVER (ORDER BY DATE_TRUNC('MONTH', sale_date)) as running_total
    FROM sales s
    WHERE s.region = r.region
    GROUP BY DATE_TRUNC('MONTH', sale_date)
    ORDER BY month
) as monthly_data;
```

---

## Array Operations

### Array Construction and Access

```sql
-- Array literals and construction
SELECT ARRAY[1, 2, 3, 4, 5] as numbers;
SELECT ARRAY['apple', 'banana', 'cherry'] as fruits;

-- Array from subquery
SELECT customer_id,
       ARRAY(SELECT product_name 
             FROM products p 
             JOIN order_items oi ON p.product_id = oi.product_id
             JOIN orders o ON oi.order_id = o.order_id
             WHERE o.customer_id = c.customer_id) as purchased_products
FROM customers c;

-- Array element access
SELECT customer_id, 
       purchased_products[1] as first_product,
       purchased_products[ARRAY_LENGTH(purchased_products)] as last_product
FROM customer_product_arrays;
```

### Array Operators

```sql
-- Array contains operator @>
SELECT customer_id, product_array
FROM customer_products
WHERE product_array @> ARRAY['laptop', 'mouse'];

-- Array contained by operator <@
SELECT customer_id, product_array
FROM customer_products
WHERE ARRAY['laptop'] <@ product_array;

-- Array overlap operator &&
SELECT customer_id, product_array
FROM customer_products
WHERE product_array && ARRAY['electronics', 'computer'];

-- Array concatenation ||
SELECT customer_id, 
       product_array || ARRAY['warranty'] as extended_products
FROM customer_products;
```

### Array Functions

```sql
-- Array manipulation functions
SELECT customer_id,
       ARRAY_LENGTH(product_array) as product_count,
       ARRAY_POSITION(product_array, 'laptop') as laptop_position,
       ARRAY_REMOVE(product_array, 'warranty') as products_no_warranty,
       ARRAY_REPLACE(product_array, 'old_item', 'new_item') as updated_products
FROM customer_products;

-- Array aggregation
SELECT category,
       ARRAY_AGG(product_name ORDER BY price DESC) as products_by_price,
       ARRAY_AGG(DISTINCT supplier_name) as suppliers
FROM products
GROUP BY category;

-- Unnesting arrays
SELECT customer_id, product_name
FROM customer_products,
     UNNEST(product_array) as product_name;

-- Array to string conversion
SELECT customer_id,
       ARRAY_TO_STRING(product_array, ', ') as product_list
FROM customer_products;
```

---

## JSON Operations

### JSON Data Types and Construction

```sql
-- JSON construction
SELECT JSON_OBJECT('name', customer_name, 
                   'email', email,
                   'orders', order_count) as customer_json
FROM customers;

-- JSON arrays
SELECT JSON_ARRAY(product_name, price, category) as product_json
FROM products;

-- Complex JSON construction
SELECT customer_id,
       JSON_OBJECT(
           'customer_info', JSON_OBJECT(
               'name', customer_name,
               'email', email,
               'phone', phone
           ),
           'recent_orders', (
               SELECT JSON_ARRAYAGG(
                   JSON_OBJECT(
                       'order_id', order_id,
                       'date', order_date,
                       'total', total_amount
                   )
               )
               FROM orders o
               WHERE o.customer_id = c.customer_id
               ORDER BY order_date DESC
               LIMIT 5
           )
       ) as customer_profile
FROM customers c;
```

### JSON Path Queries

```sql
-- JSON path extraction
SELECT customer_id,
       customer_data->'name' as customer_name,
       customer_data->'address'->'city' as city,
       customer_data->'orders'->0->'total' as first_order_total
FROM customer_json_data;

-- JSON array operations
SELECT customer_id,
       JSON_LENGTH(customer_data->'orders') as order_count,
       JSON_EXTRACT(customer_data, '$.orders[*].total') as all_order_totals
FROM customer_json_data;

-- JSON search and filtering
SELECT customer_id, customer_data
FROM customer_json_data
WHERE JSON_EXTRACT(customer_data, '$.address.country') = 'USA'
  AND JSON_EXTRACT(customer_data, '$.orders[*].total') > 1000;
```

### JSON Modification

```sql
-- JSON updates
UPDATE customer_json_data 
SET customer_data = JSON_SET(customer_data, 
                             '$.last_login', CURRENT_TIMESTAMP,
                             '$.preferences.newsletter', true)
WHERE customer_id = 12345;

-- JSON array manipulation
UPDATE product_json_data
SET product_data = JSON_ARRAY_APPEND(product_data, '$.tags', 'featured')
WHERE product_id = 67890;

-- JSON merging
SELECT customer_id,
       JSON_MERGE_PATCH(customer_data, 
                        JSON_OBJECT('status', 'premium',
                                   'updated_at', CURRENT_TIMESTAMP)) as updated_data
FROM customer_json_data;
```

---

## Pivot Operations

### Manual Pivot

```sql
-- Manual pivot using CASE
SELECT product_category,
       SUM(CASE WHEN EXTRACT(QUARTER FROM sale_date) = 1 THEN sales_amount ELSE 0 END) as Q1_sales,
       SUM(CASE WHEN EXTRACT(QUARTER FROM sale_date) = 2 THEN sales_amount ELSE 0 END) as Q2_sales,
       SUM(CASE WHEN EXTRACT(QUARTER FROM sale_date) = 3 THEN sales_amount ELSE 0 END) as Q3_sales,
       SUM(CASE WHEN EXTRACT(QUARTER FROM sale_date) = 4 THEN sales_amount ELSE 0 END) as Q4_sales
FROM sales
GROUP BY product_category;

-- Dynamic column creation
SELECT customer_segment,
       COUNT(CASE WHEN age_group = '18-25' THEN 1 END) as "18-25",
       COUNT(CASE WHEN age_group = '26-35' THEN 1 END) as "26-35",
       COUNT(CASE WHEN age_group = '36-45' THEN 1 END) as "36-45",
       COUNT(CASE WHEN age_group = '46-55' THEN 1 END) as "46-55",
       COUNT(CASE WHEN age_group = '55+' THEN 1 END) as "55+"
FROM customers
GROUP BY customer_segment;
```

### Unpivot Operations

```sql
-- Manual unpivot using UNION
SELECT product_id, 'Q1' as quarter, Q1_sales as sales_amount
FROM quarterly_sales
WHERE Q1_sales IS NOT NULL
UNION ALL
SELECT product_id, 'Q2' as quarter, Q2_sales as sales_amount
FROM quarterly_sales
WHERE Q2_sales IS NOT NULL
UNION ALL
SELECT product_id, 'Q3' as quarter, Q3_sales as sales_amount
FROM quarterly_sales
WHERE Q3_sales IS NOT NULL
UNION ALL
SELECT product_id, 'Q4' as quarter, Q4_sales as sales_amount
FROM quarterly_sales
WHERE Q4_sales IS NOT NULL;
```

---

## Time Series Functions

### Date/Time Generation

```sql
-- Generate date series
WITH RECURSIVE date_series AS (
    SELECT DATE '2025-01-01' as date_val
    UNION ALL
    SELECT date_val + INTERVAL '1' DAY
    FROM date_series
    WHERE date_val < DATE '2025-12-31'
)
SELECT date_val, 
       EXTRACT(DAYOFWEEK FROM date_val) as day_of_week,
       EXTRACT(WEEK FROM date_val) as week_number
FROM date_series;

-- Business days calculation
WITH RECURSIVE business_days AS (
    SELECT DATE '2025-01-01' as business_date
    WHERE EXTRACT(DAYOFWEEK FROM DATE '2025-01-01') BETWEEN 2 AND 6
    
    UNION ALL
    
    SELECT business_date + INTERVAL '1' DAY
    FROM business_days
    WHERE business_date + INTERVAL '1' DAY <= DATE '2025-12-31'
      AND EXTRACT(DAYOFWEEK FROM business_date + INTERVAL '1' DAY) BETWEEN 2 AND 6
)
SELECT COUNT(*) as total_business_days
FROM business_days;
```

### Time-Based Analytics

```sql
-- Moving averages and trends
SELECT sale_date,
       daily_sales,
       AVG(daily_sales) OVER (
           ORDER BY sale_date
           ROWS BETWEEN 6 PRECEDING AND CURRENT ROW
       ) as seven_day_ma,
       AVG(daily_sales) OVER (
           ORDER BY sale_date
           ROWS BETWEEN 29 PRECEDING AND CURRENT ROW
       ) as thirty_day_ma,
       
       -- Percent change calculations
       (daily_sales - LAG(daily_sales, 1) OVER (ORDER BY sale_date)) * 100.0 /
       LAG(daily_sales, 1) OVER (ORDER BY sale_date) as daily_pct_change,
       
       -- Year-over-year comparison
       LAG(daily_sales, 365) OVER (ORDER BY sale_date) as sales_year_ago,
       (daily_sales - LAG(daily_sales, 365) OVER (ORDER BY sale_date)) * 100.0 /
       LAG(daily_sales, 365) OVER (ORDER BY sale_date) as yoy_pct_change
       
FROM daily_sales_summary
ORDER BY sale_date;

-- Seasonality analysis
SELECT EXTRACT(MONTH FROM sale_date) as month,
       EXTRACT(DAYOFWEEK FROM sale_date) as day_of_week,
       AVG(daily_sales) as avg_sales,
       STDDEV(daily_sales) as sales_volatility,
       MIN(daily_sales) as min_sales,
       MAX(daily_sales) as max_sales
FROM daily_sales_summary
GROUP BY EXTRACT(MONTH FROM sale_date), EXTRACT(DAYOFWEEK FROM sale_date)
ORDER BY month, day_of_week;
```

### Gap and Island Analysis

```sql
-- Find consecutive sequences
WITH consecutive_sales AS (
    SELECT sale_date,
           daily_sales,
           sale_date - ROW_NUMBER() OVER (ORDER BY sale_date) * INTERVAL '1' DAY as group_date
    FROM daily_sales_summary
    WHERE daily_sales > 10000
),
sales_islands AS (
    SELECT group_date,
           MIN(sale_date) as island_start,
           MAX(sale_date) as island_end,
           COUNT(*) as consecutive_days,
           AVG(daily_sales) as avg_island_sales
    FROM consecutive_sales
    GROUP BY group_date
)
SELECT island_start, island_end, consecutive_days, avg_island_sales
FROM sales_islands
WHERE consecutive_days >= 5
ORDER BY island_start;

-- Gap analysis
WITH date_gaps AS (
    SELECT sale_date,
           LEAD(sale_date) OVER (ORDER BY sale_date) as next_sale_date,
           LEAD(sale_date) OVER (ORDER BY sale_date) - sale_date as gap_days
    FROM daily_sales_summary
)
SELECT sale_date, next_sale_date, gap_days
FROM date_gaps
WHERE gap_days > INTERVAL '1' DAY
ORDER BY gap_days DESC;
```

---

## Analytical Functions

### Statistical Functions

```sql
-- Descriptive statistics
SELECT product_category,
       COUNT(*) as sample_size,
       AVG(price) as mean_price,
       MEDIAN(price) as median_price,
       MODE(price) as mode_price,
       STDDEV(price) as std_deviation,
       VARIANCE(price) as variance,
       MIN(price) as min_price,
       MAX(price) as max_price,
       MAX(price) - MIN(price) as price_range,
       
       -- Percentiles
       PERCENTILE_CONT(0.25) WITHIN GROUP (ORDER BY price) as q1,
       PERCENTILE_CONT(0.75) WITHIN GROUP (ORDER BY price) as q3,
       PERCENTILE_CONT(0.75) WITHIN GROUP (ORDER BY price) - 
       PERCENTILE_CONT(0.25) WITHIN GROUP (ORDER BY price) as iqr
       
FROM products
GROUP BY product_category;

-- Correlation analysis
SELECT CORR(advertising_spend, sales_amount) as correlation_coefficient,
       REGR_SLOPE(sales_amount, advertising_spend) as regression_slope,
       REGR_INTERCEPT(sales_amount, advertising_spend) as regression_intercept,
       REGR_R2(sales_amount, advertising_spend) as r_squared
FROM monthly_marketing_data;
```

### Cohort Analysis

```sql
-- Customer cohort analysis
WITH first_purchase AS (
    SELECT customer_id,
           MIN(order_date) as first_order_date
    FROM orders
    GROUP BY customer_id
),
cohort_data AS (
    SELECT fp.customer_id,
           DATE_TRUNC('MONTH', fp.first_order_date) as cohort_month,
           DATE_TRUNC('MONTH', o.order_date) as order_month,
           EXTRACT(MONTH FROM AGE(o.order_date, fp.first_order_date)) as period_number
    FROM first_purchase fp
    JOIN orders o ON fp.customer_id = o.customer_id
),
cohort_table AS (
    SELECT cohort_month,
           period_number,
           COUNT(DISTINCT customer_id) as customers
    FROM cohort_data
    GROUP BY cohort_month, period_number
),
cohort_sizes AS (
    SELECT cohort_month,
           customers as cohort_size
    FROM cohort_table
    WHERE period_number = 0
)
SELECT ct.cohort_month,
       ct.period_number,
       ct.customers,
       cs.cohort_size,
       ROUND(ct.customers * 100.0 / cs.cohort_size, 2) as retention_rate
FROM cohort_table ct
JOIN cohort_sizes cs ON ct.cohort_month = cs.cohort_month
ORDER BY ct.cohort_month, ct.period_number;
```

### RFM Analysis

```sql
-- Recency, Frequency, Monetary analysis
WITH customer_metrics AS (
    SELECT customer_id,
           MAX(order_date) as last_order_date,
           COUNT(*) as frequency,
           SUM(total_amount) as monetary_value,
           CURRENT_DATE - MAX(order_date) as recency_days
    FROM orders
    WHERE order_date >= CURRENT_DATE - INTERVAL '365' DAY
    GROUP BY customer_id
),
rfm_scores AS (
    SELECT customer_id,
           recency_days,
           frequency,
           monetary_value,
           
           -- Recency score (lower is better)
           CASE 
               WHEN recency_days <= 30 THEN 5
               WHEN recency_days <= 60 THEN 4
               WHEN recency_days <= 90 THEN 3
               WHEN recency_days <= 180 THEN 2
               ELSE 1
           END as recency_score,
           
           -- Frequency score
           NTILE(5) OVER (ORDER BY frequency) as frequency_score,
           
           -- Monetary score
           NTILE(5) OVER (ORDER BY monetary_value) as monetary_score
    FROM customer_metrics
),
rfm_segments AS (
    SELECT customer_id,
           recency_score,
           frequency_score,
           monetary_score,
           recency_score || frequency_score || monetary_score as rfm_string,
           
           CASE
               WHEN recency_score >= 4 AND frequency_score >= 4 AND monetary_score >= 4 THEN 'Champions'
               WHEN recency_score >= 3 AND frequency_score >= 3 AND monetary_score >= 3 THEN 'Loyal Customers'
               WHEN recency_score >= 4 AND frequency_score <= 2 THEN 'New Customers'
               WHEN recency_score <= 2 AND frequency_score >= 3 THEN 'At Risk'
               WHEN recency_score <= 2 AND frequency_score <= 2 THEN 'Lost Customers'
               ELSE 'Regular Customers'
           END as customer_segment
    FROM rfm_scores
)
SELECT customer_segment,
       COUNT(*) as customer_count,
       AVG(recency_score) as avg_recency,
       AVG(frequency_score) as avg_frequency,
       AVG(monetary_score) as avg_monetary
FROM rfm_segments
GROUP BY customer_segment
ORDER BY customer_count DESC;
```

---

## Full-Text Search

### Basic Text Search

```sql
-- Text search with ranking
SELECT product_id, product_name, description,
       TS_RANK(to_tsvector('english', product_name || ' ' || description), 
               to_tsquery('english', 'laptop & wireless')) as rank
FROM products
WHERE to_tsvector('english', product_name || ' ' || description) 
      @@ to_tsquery('english', 'laptop & wireless')
ORDER BY rank DESC;

-- Phrase search
SELECT document_id, title, content
FROM documents
WHERE to_tsvector('english', title || ' ' || content)
      @@ to_tsquery('english', '"machine learning" & algorithm')
ORDER BY TS_RANK(to_tsvector('english', title || ' ' || content),
                 to_tsquery('english', '"machine learning" & algorithm')) DESC;
```

### Advanced Text Search

```sql
-- Headline generation and snippets
SELECT product_id,
       TS_HEADLINE('english', product_name, 
                   to_tsquery('english', 'wireless & mouse'),
                   'MaxFragments=2, FragmentDelimiter=" ... "') as highlighted_name,
       TS_HEADLINE('english', description,
                   to_tsquery('english', 'wireless & mouse'),
                   'MaxWords=20, MinWords=10') as snippet
FROM products
WHERE to_tsvector('english', product_name || ' ' || description)
      @@ to_tsquery('english', 'wireless & mouse');

-- Fuzzy search with similarity
SELECT product_name,
       SIMILARITY(product_name, 'wireles mouse') as similarity
FROM products
WHERE SIMILARITY(product_name, 'wireles mouse') > 0.3
ORDER BY similarity DESC;

-- Multi-field weighted search
SELECT product_id, product_name,
       TS_RANK_CD('{0.1, 0.2, 0.4, 1.0}',
                  SETWEIGHT(to_tsvector('english', product_name), 'A') ||
                  SETWEIGHT(to_tsvector('english', description), 'B') ||
                  SETWEIGHT(to_tsvector('english', category), 'C'),
                  to_tsquery('english', 'gaming & keyboard')) as rank
FROM products
WHERE (SETWEIGHT(to_tsvector('english', product_name), 'A') ||
       SETWEIGHT(to_tsvector('english', description), 'B') ||
       SETWEIGHT(to_tsvector('english', category), 'C'))
      @@ to_tsquery('english', 'gaming & keyboard')
ORDER BY rank DESC;
```

---

## Spatial Operations

### Basic Geometric Operations

```sql
-- Point operations
SELECT location_id,
       location_point,
       ST_X(location_point) as longitude,
       ST_Y(location_point) as latitude,
       ST_Distance(location_point, ST_MakePoint(-74.006, 40.7128)) as distance_from_nyc
FROM locations
WHERE ST_DWithin(location_point, ST_MakePoint(-74.006, 40.7128), 50000); -- Within 50km

-- Area calculations
SELECT region_id,
       region_polygon,
       ST_Area(region_polygon) as area_sq_meters,
       ST_Perimeter(region_polygon) as perimeter_meters
FROM regions
WHERE ST_Area(region_polygon) > 1000000; -- Larger than 1 sq km

-- Containment queries
SELECT store_id, store_location
FROM stores s
WHERE EXISTS (
    SELECT 1
    FROM delivery_zones dz
    WHERE ST_Contains(dz.zone_polygon, s.store_location)
      AND dz.zone_name = 'Downtown'
);
```

### Advanced Spatial Analysis

```sql
-- Buffer operations
SELECT customer_id, address_point,
       ST_Buffer(address_point, 1000) as one_km_radius
FROM customers;

-- Find nearest neighbors
SELECT c1.customer_id,
       c1.customer_name,
       (SELECT c2.customer_id
        FROM customers c2
        WHERE c2.customer_id != c1.customer_id
        ORDER BY ST_Distance(c1.address_point, c2.address_point)
        LIMIT 1) as nearest_customer_id
FROM customers c1;

-- Spatial clustering
WITH customer_clusters AS (
    SELECT customer_id,
           address_point,
           ST_ClusterKMeans(address_point, 5) OVER () as cluster_id
    FROM customers
)
SELECT cluster_id,
       COUNT(*) as customers_in_cluster,
       ST_Centroid(ST_Collect(address_point)) as cluster_center
FROM customer_clusters
GROUP BY cluster_id;
```

---

## Advanced Aggregation

### GROUPING SETS

```sql
-- Multiple grouping levels
SELECT product_category,
       brand,
       region,
       SUM(sales_amount) as total_sales,
       COUNT(*) as transaction_count
FROM sales
GROUP BY GROUPING SETS (
    (product_category, brand, region),  -- Detailed level
    (product_category, brand),          -- By category and brand
    (product_category, region),         -- By category and region
    (product_category),                 -- By category only
    (brand),                           -- By brand only
    (region),                          -- By region only
    ()                                 -- Grand total
)
ORDER BY GROUPING(product_category), 
         GROUPING(brand), 
         GROUPING(region);
```

### ROLLUP and CUBE

```sql
-- ROLLUP for hierarchical totals
SELECT year, quarter, month,
       SUM(sales_amount) as total_sales
FROM monthly_sales
GROUP BY ROLLUP (year, quarter, month)
ORDER BY year, quarter, month;

-- CUBE for all possible combinations
SELECT product_category, region, customer_segment,
       SUM(sales_amount) as total_sales,
       AVG(sales_amount) as avg_sales
FROM sales
GROUP BY CUBE (product_category, region, customer_segment)
ORDER BY GROUPING(product_category),
         GROUPING(region),
         GROUPING(customer_segment);

-- Using GROUPING function for clarity
SELECT 
    CASE WHEN GROUPING(product_category) = 1 THEN 'ALL CATEGORIES' 
         ELSE product_category END as category,
    CASE WHEN GROUPING(region) = 1 THEN 'ALL REGIONS'
         ELSE region END as region,
    SUM(sales_amount) as total_sales
FROM sales
GROUP BY ROLLUP (product_category, region);
```

### Custom Aggregates

```sql
-- String aggregation
SELECT department,
       STRING_AGG(employee_name, ', ' ORDER BY hire_date) as employees_by_seniority,
       STRING_AGG(DISTINCT skill, ' | ') as department_skills
FROM employees
GROUP BY department;

-- Array aggregation with filtering
SELECT product_category,
       ARRAY_AGG(product_name ORDER BY price DESC) 
       FILTER (WHERE price > 100) as premium_products,
       ARRAY_AGG(DISTINCT brand) as brands_in_category
FROM products
GROUP BY product_category;

-- Conditional aggregation
SELECT region,
       COUNT(*) as total_sales,
       COUNT(*) FILTER (WHERE sales_amount > 1000) as high_value_sales,
       SUM(sales_amount) FILTER (WHERE EXTRACT(QUARTER FROM sale_date) = 1) as q1_sales,
       AVG(sales_amount) FILTER (WHERE customer_type = 'PREMIUM') as avg_premium_sale
FROM sales
GROUP BY region;
```

---

## Dynamic SQL

### EXECUTE STATEMENT

```sql
-- Dynamic query execution
EXECUTE BLOCK
RETURNS (
    table_name VARCHAR(100),
    record_count INTEGER
)
AS
DECLARE table_cursor CURSOR FOR (
    SELECT rdb$relation_name
    FROM rdb$relations
    WHERE rdb$system_flag = 0
      AND rdb$relation_type = 0
);
DECLARE sql_stmt VARCHAR(1000);
DECLARE table_name_var VARCHAR(100);
BEGIN
    OPEN table_cursor;
    WHILE (1 = 1) DO
    BEGIN
        FETCH table_cursor INTO table_name_var;
        IF (ROW_COUNT = 0) THEN LEAVE;
        
        sql_stmt = 'SELECT COUNT(*) FROM ' || table_name_var;
        EXECUTE STATEMENT sql_stmt INTO record_count;
        
        table_name = table_name_var;
        SUSPEND;
    END
    CLOSE table_cursor;
END;

-- Dynamic schema operations
EXECUTE BLOCK (schema_name VARCHAR(100) = ?)
AS
DECLARE sql_stmt VARCHAR(1000);
BEGIN
    sql_stmt = 'CREATE SCHEMA ' || schema_name;
    EXECUTE STATEMENT sql_stmt;
    
    sql_stmt = 'CREATE TABLE ' || schema_name || '.audit_log (
        id INTEGER GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
        table_name VARCHAR(100),
        operation VARCHAR(10),
        timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
    )';
    EXECUTE STATEMENT sql_stmt;
END;
```

### Prepared Statements

```sql
-- Prepared statement with parameters
EXECUTE BLOCK (
    start_date DATE = ?,
    end_date DATE = ?,
    min_amount DECIMAL(10,2) = ?
)
RETURNS (
    customer_id INTEGER,
    total_amount DECIMAL(10,2),
    order_count INTEGER
)
AS
DECLARE sql_stmt VARCHAR(2000);
BEGIN
    sql_stmt = '
        SELECT customer_id, 
               SUM(total_amount) as total_amount,
               COUNT(*) as order_count
        FROM orders
        WHERE order_date BETWEEN ? AND ?
          AND total_amount >= ?
        GROUP BY customer_id
        HAVING SUM(total_amount) >= ?
        ORDER BY total_amount DESC';
        
    FOR EXECUTE STATEMENT sql_stmt (start_date, end_date, min_amount, min_amount * 2)
        INTO customer_id, total_amount, order_count
    DO
        SUSPEND;
END;
```

---

## Hierarchical Schema Operations

### Schema Navigation

```sql
-- Current schema context functions
SELECT CURRENT_SCHEMA as current_schema,
       CURRENT_SCHEMA_QUALIFIED as qualified_path,
       HOME_SCHEMA as home_schema,
       CURRENT_SCHEMA_PARENT as parent_schema,
       CURRENT_SCHEMA_ROOT as root_schema,
       CURRENT_SCHEMA_LEVEL as nesting_level;

-- Schema hierarchy traversal
WITH RECURSIVE schema_tree AS (
    SELECT schema_name, parent_schema_name, 0 as level,
           CAST(schema_name AS VARCHAR(1000)) as path
    FROM rdb$schemas
    WHERE parent_schema_name IS NULL
    
    UNION ALL
    
    SELECT s.schema_name, s.parent_schema_name, st.level + 1,
           st.path || '.' || s.schema_name
    FROM rdb$schemas s
    JOIN schema_tree st ON s.parent_schema_name = st.schema_name
    WHERE st.level < 10
)
SELECT schema_name, path, level,
       LPAD('', level * 2, ' ') || schema_name as indented_name
FROM schema_tree
ORDER BY path;
```

### Cross-Schema Operations

```sql
-- Query across schema hierarchy
SELECT 'finance.accounting' as schema_context,
       (SELECT COUNT(*) FROM finance.accounting.transactions) as transaction_count,
       (SELECT SUM(amount) FROM finance.accounting.transactions 
        WHERE transaction_date >= CURRENT_DATE - 30) as monthly_total;

-- Schema-aware object resolution
WITH schema_objects AS (
    SELECT COALESCE(schema_path, schema_name) as full_schema,
           object_name,
           object_type
    FROM rdb$schemas s
    LEFT JOIN (
        SELECT 'TABLE' as object_type, table_name as object_name, table_schema
        FROM information_schema.tables
        UNION ALL
        SELECT 'VIEW' as object_type, table_name as object_name, table_schema
        FROM information_schema.views
        UNION ALL
        SELECT 'PROCEDURE' as object_type, routine_name as object_name, routine_schema
        FROM information_schema.routines
    ) objects ON s.schema_name = objects.table_schema OR s.schema_name = objects.routine_schema
)
SELECT full_schema, object_type, COUNT(*) as object_count
FROM schema_objects
WHERE object_name IS NOT NULL
GROUP BY full_schema, object_type
ORDER BY full_schema, object_type;
```

---

## Database Link Operations

### Cross-Database Queries

```sql
-- Basic database link usage
SELECT customer_id, customer_name, order_count
FROM customers@remote_server
WHERE region = 'NORTH';

-- Join across database links
SELECT c.customer_name, o.order_date, o.total_amount
FROM customers c
JOIN orders@sales_db o ON c.customer_id = o.customer_id
WHERE o.order_date >= CURRENT_DATE - 30;

-- Schema-aware database link queries
SELECT product_name, inventory_level
FROM inventory.products@warehouse_link
WHERE category = 'Electronics';
```

### Advanced Link Operations

```sql
-- Distributed aggregation
SELECT region,
       local_sales.total as local_total,
       remote_sales.total as remote_total,
       local_sales.total + remote_sales.total as combined_total
FROM (
    SELECT 'LOCAL' as region, SUM(amount) as total
    FROM sales
    WHERE sale_date >= CURRENT_DATE - 30
) local_sales,
(
    SELECT 'REMOTE' as region, SUM(amount) as total
    FROM sales@remote_db
    WHERE sale_date >= CURRENT_DATE - 30
) remote_sales;

-- Cross-database transaction coordination
EXECUTE BLOCK
AS
BEGIN
    -- Start distributed transaction
    INSERT INTO orders (customer_id, order_date, total_amount)
    VALUES (12345, CURRENT_DATE, 199.99);
    
    -- Update inventory on remote system
    EXECUTE STATEMENT 'UPDATE inventory SET quantity = quantity - 1 
                       WHERE product_id = 67890'
    ON EXTERNAL DATA SOURCE 'warehouse_link';
    
    -- Log transaction in audit system
    INSERT INTO audit_log@audit_db (table_name, operation, timestamp)
    VALUES ('orders', 'INSERT', CURRENT_TIMESTAMP);
    
    COMMIT;
END;
```

---

## Performance Optimization Features

### Query Hints and Optimization

```sql
-- Index hints
SELECT /*+ INDEX(customers, idx_customer_region) */
       customer_id, customer_name
FROM customers
WHERE region = 'WEST'
  AND status = 'ACTIVE';

-- Join order hints
SELECT /*+ ORDERED */
       c.customer_name, o.order_date, oi.product_name
FROM customers c,
     orders o,
     order_items oi
WHERE c.customer_id = o.customer_id
  AND o.order_id = oi.order_id;

-- Parallel processing hints
SELECT /*+ PARALLEL(sales, 4) */
       product_category,
       SUM(sales_amount) as total_sales
FROM sales
WHERE sale_date >= DATE '2025-01-01'
GROUP BY product_category;
```

### Execution Plan Analysis

```sql
-- Plan analysis
SET PLAN ON;
SELECT customer_id, SUM(total_amount)
FROM orders
WHERE order_date BETWEEN DATE '2025-01-01' AND DATE '2025-12-31'
GROUP BY customer_id
HAVING SUM(total_amount) > 10000;
SET PLAN OFF;

-- Statistics gathering
UPDATE STATISTICS TABLE orders;
UPDATE STATISTICS INDEX idx_orders_customer_date;

-- Query optimization with statistics
SELECT customer_id,
       COUNT(*) as order_count,
       AVG(total_amount) as avg_order_value
FROM orders
WHERE order_date >= CURRENT_DATE - INTERVAL '90' DAY
GROUP BY customer_id
HAVING COUNT(*) >= 5;
```

### Materialized Views

```sql
-- Create materialized view for performance
CREATE OR ALTER VIEW mv_monthly_sales AS
SELECT EXTRACT(YEAR FROM order_date) as year,
       EXTRACT(MONTH FROM order_date) as month,
       product_category,
       SUM(total_amount) as total_sales,
       COUNT(*) as order_count,
       AVG(total_amount) as avg_order_value
FROM orders o
JOIN order_items oi ON o.order_id = oi.order_id
JOIN products p ON oi.product_id = p.product_id
GROUP BY EXTRACT(YEAR FROM order_date),
         EXTRACT(MONTH FROM order_date),
         product_category;

-- Use materialized view
SELECT year, month, product_category, total_sales
FROM mv_monthly_sales
WHERE year = 2025
  AND total_sales > 50000
ORDER BY total_sales DESC;
```

---

## Error Handling

### Exception Handling in Blocks

```sql
-- Basic exception handling
EXECUTE BLOCK
RETURNS (status VARCHAR(100))
AS
BEGIN
    BEGIN
        INSERT INTO customers (customer_name, email)
        VALUES ('John Doe', 'john.doe@email.com');
        
        status = 'Customer inserted successfully';
    WHEN ANY DO
    BEGIN
        status = 'Error: ' || GDSCODE || ' - ' || SQLCODE;
    END
    END
    
    SUSPEND;
END;

-- Specific exception handling
EXECUTE BLOCK (customer_id INTEGER = ?)
RETURNS (operation_result VARCHAR(200))
AS
BEGIN
    BEGIN
        DELETE FROM customers WHERE customer_id = :customer_id;
        
        IF (ROW_COUNT = 0) THEN
            operation_result = 'No customer found with ID: ' || customer_id;
        ELSE
            operation_result = 'Customer deleted successfully';
            
    WHEN SQLCODE -530 DO  -- Foreign key constraint
        operation_result = 'Cannot delete customer: has related orders';
    WHEN SQLCODE -803 DO  -- Unique constraint
        operation_result = 'Unique constraint violation';
    WHEN ANY DO
        operation_result = 'Unexpected error: ' || SQLSTATE || ' - ' || GDSCODE;
    END
    
    SUSPEND;
END;
```

### Custom Exception Handling

```sql
-- Custom exception definition and handling
EXECUTE BLOCK (
    order_amount DECIMAL(10,2) = ?,
    customer_id INTEGER = ?
)
RETURNS (order_id INTEGER, status VARCHAR(100))
AS
DECLARE max_credit_limit DECIMAL(10,2);
DECLARE current_balance DECIMAL(10,2);
BEGIN
    -- Check customer credit limit
    SELECT credit_limit, current_balance
    FROM customers
    WHERE customer_id = :customer_id
    INTO max_credit_limit, current_balance;
    
    IF (max_credit_limit IS NULL) THEN
    BEGIN
        EXCEPTION EX_CUSTOMER_NOT_FOUND 'Customer not found: ' || customer_id;
    END
    
    IF (current_balance + order_amount > max_credit_limit) THEN
    BEGIN
        EXCEPTION EX_CREDIT_LIMIT_EXCEEDED 
            'Order amount would exceed credit limit. ' ||
            'Available credit: ' || (max_credit_limit - current_balance);
    END
    
    -- Process order
    INSERT INTO orders (customer_id, order_date, total_amount)
    VALUES (:customer_id, CURRENT_DATE, :order_amount)
    RETURNING order_id INTO order_id;
    
    -- Update customer balance
    UPDATE customers 
    SET current_balance = current_balance + :order_amount
    WHERE customer_id = :customer_id;
    
    status = 'Order processed successfully';
    SUSPEND;
    
WHEN EXCEPTION EX_CUSTOMER_NOT_FOUND DO
BEGIN
    status = 'Error: ' || EXCEPTION_MESSAGE;
    SUSPEND;
END
WHEN EXCEPTION EX_CREDIT_LIMIT_EXCEEDED DO
BEGIN
    status = 'Credit Error: ' || EXCEPTION_MESSAGE;
    SUSPEND;
END
WHEN ANY DO
BEGIN
    status = 'System Error: ' || SQLSTATE || ' - Code: ' || SQLCODE;
    SUSPEND;
END
END;
```

### Transaction Management

```sql
-- Savepoint usage
EXECUTE BLOCK
AS
DECLARE savepoint_name VARCHAR(50);
BEGIN
    BEGIN
        -- Main transaction work
        INSERT INTO orders (customer_id, order_date, total_amount)
        VALUES (12345, CURRENT_DATE, 299.99);
        
        savepoint_name = 'after_order_insert';
        SAVEPOINT after_order_insert;
        
        -- Risky operation
        BEGIN
            UPDATE inventory 
            SET quantity = quantity - 10
            WHERE product_id = 67890;
            
            IF (ROW_COUNT = 0) THEN
                EXCEPTION EX_PRODUCT_NOT_FOUND 'Product not found in inventory';
                
        WHEN EXCEPTION EX_PRODUCT_NOT_FOUND DO
        BEGIN
            ROLLBACK TO SAVEPOINT after_order_insert;
            -- Continue with alternative logic
            INSERT INTO backorder_items (order_id, product_id, quantity)
            VALUES (GEN_ID(gen_order_id, 0), 67890, 10);
        END
        END
        
        COMMIT;
        
    WHEN ANY DO
    BEGIN
        ROLLBACK;
        -- Log error for investigation
        INSERT INTO error_log (error_time, error_code, error_message)
        VALUES (CURRENT_TIMESTAMP, SQLCODE, SQLSTATE);
        COMMIT;
    END
    END
END;
```

---

## Best Practices and Performance Tips

### Query Optimization Guidelines

1. **Use appropriate indexes**: Create indexes on frequently queried columns
2. **Limit result sets**: Use LIMIT/OFFSET for pagination
3. **Optimize JOIN operations**: Use appropriate join types and order
4. **Avoid SELECT ***: Specify only needed columns
5. **Use EXISTS vs IN**: EXISTS is often more efficient for subqueries
6. **Optimize GROUP BY**: Consider using covering indexes
7. **Use UNION ALL**: When duplicates are not a concern
8. **Batch operations**: Use batch inserts/updates for large datasets

### Advanced SQL Features Usage

1. **CTEs for readability**: Break complex queries into manageable parts
2. **Window functions for analytics**: Avoid self-joins when possible
3. **Recursive CTEs for hierarchical data**: Be careful with termination conditions
4. **Array operations for collections**: Efficient handling of list data
5. **JSON for semi-structured data**: Balance flexibility with performance
6. **Full-text search for text queries**: Use proper indexing strategies

### ScratchBird-Specific Optimizations

1. **Hierarchical schemas**: Use qualified names for optimal resolution
2. **Database links**: Consider data locality and network latency
3. **SQL Dialect 4 features**: Leverage modern syntax for cleaner code
4. **Context variables**: Use schema context for multi-tenant applications
5. **Array and JSON operators**: Take advantage of PostgreSQL-compatible features

---

*This documentation covers ScratchBird Alpha 0.6.0 features. For the latest updates and additional examples, refer to the official ScratchBird documentation and release notes.*