# ScratchBird SQL Statements - Complete Reference Documentation

## Overview

**SQL Statements** in ScratchBird provide comprehensive data query and manipulation capabilities with significant enhancements over standard SQL. ScratchBird implements complete SQL:2016 compliance while adding advanced features like hierarchical schema support, database links, and modern SQL capabilities including Common Table Expressions (CTEs), window functions, and advanced merge operations.

### SQL Statement Features

ScratchBird's SQL statement system provides enterprise-grade capabilities:

- **Complete SQL Standard Compliance**: Full SQL:2016 implementation with SQL Dialect 4 enhancements
- **Hierarchical Schema Support**: Advanced multi-level schema navigation and qualification
- **Database Link Integration**: Schema-aware remote database connectivity
- **Advanced Query Features**: CTEs, window functions, advanced joins, and set operations
- **Modern DML Operations**: MERGE statements, multi-row inserts, and RETURNING clauses
- **Performance Optimization**: Query planning, indexing hints, and parallel processing support

---

## SELECT Statement

The SELECT statement retrieves data from tables, views, and other data sources with comprehensive filtering, grouping, and ordering capabilities.

### Basic SELECT Syntax

#### **Core SELECT Structure**
```sql
SELECT [FIRST n] [SKIP n] [DISTINCT | ALL] 
       select_list 
FROM   from_clause 
WHERE  search_condition 
GROUP BY group_by_list 
HAVING search_condition 
WINDOW window_definition_list
ORDER BY order_list 
[ROWS clause | OFFSET clause FETCH FIRST clause]
[FOR UPDATE [OF column_list]] 
[WITH LOCK [SKIP LOCKED]]
[OPTIMIZE FOR {FIRST | ALL} ROWS]
```

#### **Simple SELECT Examples**
```sql
-- Basic column selection
SELECT customer_id, customer_name, email FROM customers;

-- All columns
SELECT * FROM products;

-- Calculated columns with aliases
SELECT 
    product_name,
    price,
    price * 1.1 as price_with_tax,
    UPPER(category) as category_upper
FROM products;

-- FROM-less SELECT (SQL Dialect 4 enhancement)
SELECT CURRENT_DATE;                    -- Implicit FROM RDB$DATABASE
SELECT CURRENT_USER;
SELECT 'Hello, World!';
SELECT 42 * 2 as answer;
```

### SELECT List Variations

#### **Column Specifications**
```sql
-- Specific columns
SELECT first_name, last_name, email FROM employees;

-- Table-qualified columns
SELECT 
    c.customer_name,
    o.order_date,
    o.total_amount
FROM customers c, orders o
WHERE c.customer_id = o.customer_id;

-- Schema-qualified columns (hierarchical schema support)
SELECT 
    finance.accounting.accounts.account_number,
    finance.accounting.accounts.balance
FROM finance.accounting.accounts;

-- All columns from specific table
SELECT c.*, o.order_date
FROM customers c
JOIN orders o ON c.customer_id = o.customer_id;
```

#### **Expressions and Functions**
```sql
-- Mathematical expressions
SELECT 
    quantity,
    unit_price,
    quantity * unit_price as line_total,
    ROUND(quantity * unit_price * 1.08, 2) as total_with_tax
FROM order_items;

-- String functions
SELECT 
    UPPER(last_name) || ', ' || LOWER(first_name) as formatted_name,
    LENGTH(email) as email_length,
    SUBSTRING(phone FROM 1 FOR 3) as area_code
FROM customers;

-- Date functions
SELECT 
    order_date,
    EXTRACT(YEAR FROM order_date) as order_year,
    CURRENT_DATE - order_date as days_since_order,
    order_date + 30 as payment_due_date
FROM orders;
```

### FROM Clause and Data Sources

#### **Table References**
```sql
-- Simple table reference
SELECT * FROM customers;

-- Table with alias
SELECT c.customer_name, c.email 
FROM customers c;

-- Multiple tables (implicit cross join)
SELECT c.customer_name, o.order_date
FROM customers c, orders o
WHERE c.customer_id = o.customer_id;

-- Hierarchical schema table references
SELECT * FROM finance.accounting.transactions;
SELECT * FROM company.americas.sales.customers;
SELECT * FROM enterprise.hr.payroll.employees;
```

#### **Derived Tables (Subqueries in FROM)**
```sql
-- Basic derived table
SELECT customer_type, AVG(order_total) as avg_order
FROM (
    SELECT 
        customer_id,
        CASE 
            WHEN total_orders > 10 THEN 'Frequent'
            WHEN total_orders > 5 THEN 'Regular'
            ELSE 'Occasional'
        END as customer_type,
        avg_order_amount as order_total
    FROM customer_summary
) customer_categories
GROUP BY customer_type;

-- Derived table with parameters
SELECT *
FROM (
    SELECT customer_id, order_date, total_amount,
           ROW_NUMBER() OVER (PARTITION BY customer_id ORDER BY order_date DESC) as rn
    FROM orders
    WHERE order_date >= :start_date
) recent_orders
WHERE rn <= 3;  -- Last 3 orders per customer
```

#### **Table-Valued Functions**
```sql
-- Procedure as table source
SELECT * FROM get_monthly_sales('2024-01', '2024-12');

-- Function with parameters
SELECT employee_id, department, salary
FROM get_employee_hierarchy(:manager_id);

-- Schema-qualified function calls
SELECT * FROM hr.payroll.get_payroll_summary(:year, :month);
```

### Database Link Support (ScratchBird Enhancement)

#### **Remote Table Access**
```sql
-- Basic database link usage
SELECT * FROM employees@hr_link;
SELECT * FROM products@inventory_link;

-- Schema-qualified remote access
SELECT * FROM finance.accounting.ledger@finance_link;
SELECT * FROM sales.customers@regional_link;

-- Join local and remote tables
SELECT 
    local_orders.order_id,
    local_orders.order_date,
    remote_customers.customer_name
FROM orders local_orders
JOIN customers@customer_link remote_customers 
  ON local_orders.customer_id = remote_customers.customer_id;
```

#### **Schema Resolution with Database Links**
```sql
-- Context-aware schema resolution
SELECT * FROM CURRENT.employees@hr_link;        -- Current schema on remote
SELECT * FROM HOME.projects@project_link;       -- Home schema on remote
SELECT * FROM USER.preferences@config_link;     -- User schema on remote

-- Hierarchical schema mapping
SELECT * FROM finance.accounting.reports@reporting_link;
-- Maps to configured remote schema based on link's SCHEMA_MODE
```

### JOIN Operations

#### **Inner Joins**
```sql
-- Basic inner join
SELECT c.customer_name, o.order_date, o.total_amount
FROM customers c
INNER JOIN orders o ON c.customer_id = o.customer_id;

-- Multiple table joins
SELECT 
    c.customer_name,
    o.order_date,
    oi.product_name,
    oi.quantity,
    oi.unit_price
FROM customers c
INNER JOIN orders o ON c.customer_id = o.customer_id
INNER JOIN order_items oi ON o.order_id = oi.order_id;

-- Self-join
SELECT 
    emp.employee_name,
    mgr.employee_name as manager_name
FROM employees emp
INNER JOIN employees mgr ON emp.manager_id = mgr.employee_id;
```

#### **Outer Joins**
```sql
-- Left outer join
SELECT c.customer_name, o.order_date
FROM customers c
LEFT OUTER JOIN orders o ON c.customer_id = o.customer_id;

-- Right outer join
SELECT c.customer_name, o.order_date
FROM customers c
RIGHT OUTER JOIN orders o ON c.customer_id = o.customer_id;

-- Full outer join
SELECT 
    COALESCE(c.customer_name, 'Unknown') as customer,
    COALESCE(o.order_date, DATE '1900-01-01') as order_date
FROM customers c
FULL OUTER JOIN orders o ON c.customer_id = o.customer_id;

-- Multiple outer joins
SELECT 
    c.customer_name,
    o.order_date,
    s.shipment_date,
    p.payment_date
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id
LEFT JOIN shipments s ON o.order_id = s.order_id
LEFT JOIN payments p ON o.order_id = p.order_id;
```

#### **Cross and Natural Joins**
```sql
-- Cross join (Cartesian product)
SELECT p.product_name, s.store_name
FROM products p
CROSS JOIN stores s;

-- Natural join (automatic column matching)
SELECT customer_name, order_date
FROM customers
NATURAL JOIN orders;

-- Natural left join
SELECT customer_name, order_date
FROM customers
NATURAL LEFT JOIN orders;
```

#### **Using Clause Joins**
```sql
-- JOIN with USING clause
SELECT customer_name, order_date, total_amount
FROM customers
JOIN orders USING (customer_id);

-- Multiple column USING
SELECT 
    department_name,
    employee_name,
    project_name
FROM departments
JOIN employees USING (department_id)
JOIN projects USING (employee_id, department_id);
```

### WHERE Clause and Filtering

#### **Basic Filtering**
```sql
-- Single condition
SELECT * FROM customers WHERE city = 'New York';

-- Multiple conditions with AND
SELECT * FROM products 
WHERE category = 'Electronics' 
  AND price BETWEEN 100 AND 500
  AND in_stock = TRUE;

-- Multiple conditions with OR
SELECT * FROM customers
WHERE city = 'New York' 
   OR city = 'Los Angeles' 
   OR city = 'Chicago';

-- Complex logical combinations
SELECT * FROM orders
WHERE (status = 'SHIPPED' OR status = 'DELIVERED')
  AND order_date >= DATE '2024-01-01'
  AND (priority = 'HIGH' OR total_amount > 1000);
```

#### **Pattern Matching**
```sql
-- LIKE pattern matching
SELECT * FROM customers WHERE last_name LIKE 'Smith%';
SELECT * FROM products WHERE description LIKE '%wireless%';

-- Case-insensitive searching
SELECT * FROM customers WHERE UPPER(last_name) LIKE 'SMITH%';
SELECT * FROM products WHERE description CONTAINING 'wireless';

-- SIMILAR TO (regex patterns)
SELECT * FROM customers 
WHERE phone SIMILAR TO '[0-9]{3}-[0-9]{3}-[0-9]{4}';

-- STARTING WITH operator
SELECT * FROM customers WHERE last_name STARTING WITH 'Mc';
```

#### **Range and Set Operations**
```sql
-- BETWEEN for ranges
SELECT * FROM products WHERE price BETWEEN 50 AND 200;
SELECT * FROM orders WHERE order_date BETWEEN DATE '2024-01-01' AND DATE '2024-12-31';

-- IN with value lists
SELECT * FROM customers WHERE city IN ('New York', 'Los Angeles', 'Chicago');
SELECT * FROM products WHERE category IN ('Electronics', 'Books', 'Clothing');

-- IN with subqueries
SELECT * FROM customers
WHERE customer_id IN (
    SELECT DISTINCT customer_id 
    FROM orders 
    WHERE order_date >= DATE '2024-01-01'
);

-- NOT IN
SELECT * FROM customers
WHERE customer_id NOT IN (
    SELECT customer_id 
    FROM orders 
    WHERE status = 'CANCELLED'
);
```

#### **NULL Handling**
```sql
-- NULL testing
SELECT * FROM customers WHERE middle_name IS NULL;
SELECT * FROM customers WHERE middle_name IS NOT NULL;

-- Three-value logic with UNKNOWN
SELECT * FROM survey_responses WHERE rating IS UNKNOWN;
SELECT * FROM test_results WHERE passed IS NOT UNKNOWN;

-- NULL-safe comparisons
SELECT * FROM customers
WHERE COALESCE(preferred_contact, 'email') = 'email';
```

### Subqueries

#### **Scalar Subqueries**
```sql
-- Single value subqueries
SELECT 
    customer_name,
    total_orders,
    (SELECT AVG(total_orders) FROM customer_summary) as avg_orders
FROM customer_summary;

-- Subquery in WHERE clause
SELECT * FROM orders
WHERE total_amount > (
    SELECT AVG(total_amount) 
    FROM orders 
    WHERE order_date >= DATE '2024-01-01'
);
```

#### **Correlated Subqueries**
```sql
-- Correlated subquery
SELECT customer_id, customer_name
FROM customers c
WHERE EXISTS (
    SELECT 1 
    FROM orders o 
    WHERE o.customer_id = c.customer_id 
      AND o.order_date >= DATE '2024-01-01'
);

-- Correlated with aggregation
SELECT 
    customer_id,
    customer_name,
    (SELECT COUNT(*) 
     FROM orders o 
     WHERE o.customer_id = c.customer_id) as order_count
FROM customers c;

-- Complex correlated subquery
SELECT product_id, product_name, price
FROM products p
WHERE price > (
    SELECT AVG(price) 
    FROM products p2 
    WHERE p2.category = p.category
);
```

#### **EXISTS and NOT EXISTS**
```sql
-- EXISTS predicate
SELECT customer_id, customer_name
FROM customers c
WHERE EXISTS (
    SELECT 1 
    FROM orders o 
    WHERE o.customer_id = c.customer_id 
      AND o.status = 'PENDING'
);

-- NOT EXISTS predicate  
SELECT customer_id, customer_name
FROM customers c
WHERE NOT EXISTS (
    SELECT 1 
    FROM orders o 
    WHERE o.customer_id = c.customer_id
);

-- EXISTS with multiple conditions
SELECT product_id, product_name
FROM products p
WHERE EXISTS (
    SELECT 1 
    FROM order_items oi
    JOIN orders o ON oi.order_id = o.order_id
    WHERE oi.product_id = p.product_id
      AND o.order_date >= DATE '2024-01-01'
      AND oi.quantity > 10
);
```

#### **Quantified Comparisons**
```sql
-- ALL quantifier
SELECT * FROM products
WHERE price >= ALL (
    SELECT price 
    FROM products 
    WHERE category = 'Electronics'
);

-- ANY/SOME quantifier
SELECT * FROM customers
WHERE customer_id = ANY (
    SELECT customer_id 
    FROM orders 
    WHERE total_amount > 1000
);

-- Comparison with quantifiers
SELECT product_id, product_name, price
FROM products p1
WHERE price > ANY (
    SELECT price 
    FROM products p2 
    WHERE p2.category = p1.category 
      AND p2.product_id != p1.product_id
);
```

### Common Table Expressions (CTEs)

#### **Basic CTE Usage**
```sql
-- Simple CTE
WITH sales_summary AS (
    SELECT 
        customer_id,
        COUNT(*) as order_count,
        SUM(total_amount) as total_sales
    FROM orders
    WHERE order_date >= DATE '2024-01-01'
    GROUP BY customer_id
)
SELECT 
    c.customer_name,
    ss.order_count,
    ss.total_sales
FROM customers c
JOIN sales_summary ss ON c.customer_id = ss.customer_id;

-- CTE with column list
WITH monthly_sales (month_name, sales_total) AS (
    SELECT 
        EXTRACT(MONTH FROM order_date),
        SUM(total_amount)
    FROM orders
    GROUP BY EXTRACT(MONTH FROM order_date)
)
SELECT month_name, sales_total
FROM monthly_sales
ORDER BY month_name;
```

#### **Multiple CTEs**
```sql
-- Multiple CTEs in single query
WITH 
customer_metrics AS (
    SELECT 
        customer_id,
        COUNT(*) as order_count,
        AVG(total_amount) as avg_order
    FROM orders
    GROUP BY customer_id
),
customer_categories AS (
    SELECT 
        customer_id,
        CASE 
            WHEN order_count >= 10 THEN 'VIP'
            WHEN order_count >= 5 THEN 'Regular'
            ELSE 'Occasional'
        END as category
    FROM customer_metrics
)
SELECT 
    c.customer_name,
    cc.category,
    cm.order_count,
    cm.avg_order
FROM customers c
JOIN customer_metrics cm ON c.customer_id = cm.customer_id
JOIN customer_categories cc ON c.customer_id = cc.customer_id;
```

#### **Recursive CTEs**
```sql
-- Recursive CTE for hierarchical data
WITH RECURSIVE employee_hierarchy AS (
    -- Anchor: Top-level managers
    SELECT 
        employee_id,
        employee_name,
        manager_id,
        1 as level,
        CAST(employee_name AS VARCHAR(1000)) as path
    FROM employees
    WHERE manager_id IS NULL
    
    UNION ALL
    
    -- Recursive: Subordinates
    SELECT 
        e.employee_id,
        e.employee_name,
        e.manager_id,
        eh.level + 1,
        eh.path || ' > ' || e.employee_name
    FROM employees e
    JOIN employee_hierarchy eh ON e.manager_id = eh.employee_id
    WHERE eh.level < 10  -- Prevent infinite recursion
)
SELECT 
    employee_id,
    employee_name,
    level,
    path as hierarchy_path
FROM employee_hierarchy
ORDER BY level, employee_name;

-- Recursive CTE for bill of materials
WITH RECURSIVE bom_explosion AS (
    -- Anchor: Top-level product
    SELECT 
        product_id,
        component_id,
        quantity,
        1 as level,
        quantity as total_quantity
    FROM bill_of_materials
    WHERE product_id = :top_level_product_id
    
    UNION ALL
    
    -- Recursive: Sub-components
    SELECT 
        bom.product_id,
        bom.component_id,
        bom.quantity,
        be.level + 1,
        be.total_quantity * bom.quantity
    FROM bill_of_materials bom
    JOIN bom_explosion be ON bom.product_id = be.component_id
    WHERE be.level < 20
)
SELECT 
    component_id,
    SUM(total_quantity) as total_needed
FROM bom_explosion
GROUP BY component_id;
```

### Window Functions

#### **Ranking Functions**
```sql
-- ROW_NUMBER
SELECT 
    customer_id,
    order_date,
    total_amount,
    ROW_NUMBER() OVER (ORDER BY total_amount DESC) as overall_rank,
    ROW_NUMBER() OVER (PARTITION BY customer_id ORDER BY order_date) as customer_order_seq
FROM orders;

-- RANK and DENSE_RANK
SELECT 
    employee_id,
    department,
    salary,
    RANK() OVER (ORDER BY salary DESC) as salary_rank,
    DENSE_RANK() OVER (ORDER BY salary DESC) as salary_dense_rank,
    RANK() OVER (PARTITION BY department ORDER BY salary DESC) as dept_rank
FROM employees;

-- NTILE for quartiles
SELECT 
    product_id,
    price,
    NTILE(4) OVER (ORDER BY price) as price_quartile
FROM products;

-- PERCENT_RANK and CUME_DIST
SELECT 
    student_id,
    score,
    PERCENT_RANK() OVER (ORDER BY score) as percentile_rank,
    CUME_DIST() OVER (ORDER BY score) as cumulative_distribution
FROM test_scores;
```

#### **Aggregate Window Functions**
```sql
-- Running totals and averages
SELECT 
    order_date,
    daily_sales,
    SUM(daily_sales) OVER (ORDER BY order_date) as running_total,
    AVG(daily_sales) OVER (ORDER BY order_date ROWS 6 PRECEDING) as seven_day_avg
FROM daily_sales_summary;

-- Cumulative and moving aggregates
SELECT 
    month_num,
    monthly_revenue,
    SUM(monthly_revenue) OVER (ORDER BY month_num) as ytd_revenue,
    AVG(monthly_revenue) OVER (ORDER BY month_num ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) as quarterly_avg
FROM monthly_revenue;

-- Window frames with RANGE
SELECT 
    transaction_date,
    amount,
    SUM(amount) OVER (
        ORDER BY transaction_date 
        RANGE BETWEEN INTERVAL '7 days' PRECEDING AND CURRENT ROW
    ) as seven_day_total
FROM transactions;
```

#### **Offset Functions**
```sql
-- LAG and LEAD
SELECT 
    order_date,
    total_amount,
    LAG(total_amount, 1) OVER (ORDER BY order_date) as previous_amount,
    LEAD(total_amount, 1) OVER (ORDER BY order_date) as next_amount,
    total_amount - LAG(total_amount, 1) OVER (ORDER BY order_date) as amount_change
FROM daily_sales;

-- LAG with default value
SELECT 
    employee_id,
    salary_date,
    salary,
    LAG(salary, 1, 0) OVER (PARTITION BY employee_id ORDER BY salary_date) as previous_salary
FROM salary_history;

-- FIRST_VALUE and LAST_VALUE
SELECT 
    department,
    employee_name,
    salary,
    FIRST_VALUE(salary) OVER (PARTITION BY department ORDER BY salary DESC) as highest_salary,
    LAST_VALUE(salary) OVER (
        PARTITION BY department 
        ORDER BY salary DESC 
        ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
    ) as lowest_salary
FROM employees;

-- NTH_VALUE
SELECT 
    product_category,
    product_name,
    sales_amount,
    NTH_VALUE(sales_amount, 2) OVER (
        PARTITION BY product_category 
        ORDER BY sales_amount DESC
    ) as second_highest_sales
FROM product_sales;
```

#### **Named Windows**
```sql
-- WINDOW clause for reusable window definitions
SELECT 
    employee_id,
    department,
    salary,
    RANK() OVER department_salary_window as dept_rank,
    PERCENT_RANK() OVER department_salary_window as dept_percentile,
    AVG(salary) OVER department_window as dept_avg_salary
FROM employees
WINDOW 
    department_window AS (PARTITION BY department),
    department_salary_window AS (PARTITION BY department ORDER BY salary DESC);
```

### Set Operations

#### **UNION Operations**
```sql
-- UNION (removes duplicates)
SELECT customer_id, 'Customer' as source FROM customers
UNION
SELECT supplier_id, 'Supplier' as source FROM suppliers;

-- UNION ALL (keeps duplicates)
SELECT product_name FROM products WHERE category = 'Electronics'
UNION ALL
SELECT product_name FROM products WHERE category = 'Books';

-- Complex UNION with ordering
SELECT 
    'Q1' as quarter,
    SUM(amount) as total
FROM sales 
WHERE order_date BETWEEN DATE '2024-01-01' AND DATE '2024-03-31'
UNION ALL
SELECT 
    'Q2' as quarter,
    SUM(amount) as total
FROM sales 
WHERE order_date BETWEEN DATE '2024-04-01' AND DATE '2024-06-30'
ORDER BY quarter;

-- UNION with different column expressions
SELECT customer_name as name, 'Customer' as type, city
FROM customers
UNION
SELECT supplier_name as name, 'Supplier' as type, location as city
FROM suppliers
ORDER BY name;
```

### Grouping and Aggregation

#### **GROUP BY Clause**
```sql
-- Basic grouping
SELECT 
    category,
    COUNT(*) as product_count,
    AVG(price) as avg_price,
    MIN(price) as min_price,
    MAX(price) as max_price
FROM products
GROUP BY category;

-- Multiple column grouping
SELECT 
    category,
    brand,
    COUNT(*) as product_count,
    SUM(quantity_sold) as total_sold
FROM products
GROUP BY category, brand
ORDER BY category, brand;

-- Grouping with expressions
SELECT 
    EXTRACT(YEAR FROM order_date) as order_year,
    EXTRACT(MONTH FROM order_date) as order_month,
    COUNT(*) as order_count,
    SUM(total_amount) as monthly_sales
FROM orders
GROUP BY EXTRACT(YEAR FROM order_date), EXTRACT(MONTH FROM order_date)
ORDER BY order_year, order_month;
```

#### **HAVING Clause**
```sql
-- HAVING with aggregates
SELECT 
    customer_id,
    COUNT(*) as order_count,
    SUM(total_amount) as total_spent
FROM orders
GROUP BY customer_id
HAVING COUNT(*) > 5 
   AND SUM(total_amount) > 1000;

-- HAVING with complex conditions
SELECT 
    category,
    AVG(price) as avg_price,
    COUNT(*) as product_count
FROM products
GROUP BY category
HAVING AVG(price) > (SELECT AVG(price) FROM products)
   AND COUNT(*) >= 10;
```

#### **Advanced Grouping (ROLLUP, CUBE, GROUPING SETS)**
```sql
-- ROLLUP for hierarchical totals
SELECT 
    region,
    country,
    city,
    SUM(sales_amount) as total_sales
FROM sales_data
GROUP BY ROLLUP (region, country, city);

-- CUBE for all combinations
SELECT 
    product_category,
    sales_quarter,
    SUM(sales_amount) as total_sales
FROM quarterly_sales
GROUP BY CUBE (product_category, sales_quarter);

-- GROUPING SETS for specific combinations
SELECT 
    department,
    job_title,
    COUNT(*) as employee_count
FROM employees
GROUP BY GROUPING SETS (
    (department),
    (job_title),
    (department, job_title),
    ()  -- Grand total
);

-- GROUPING function to identify aggregate levels
SELECT 
    CASE WHEN GROUPING(category) = 1 THEN 'ALL CATEGORIES' ELSE category END as category,
    CASE WHEN GROUPING(brand) = 1 THEN 'ALL BRANDS' ELSE brand END as brand,
    SUM(sales_amount) as total_sales
FROM product_sales
GROUP BY ROLLUP (category, brand);
```

### Ordering and Limiting

#### **ORDER BY Clause**
```sql
-- Single column ordering
SELECT * FROM customers ORDER BY last_name;
SELECT * FROM products ORDER BY price DESC;

-- Multiple column ordering
SELECT * FROM employees 
ORDER BY department ASC, salary DESC, hire_date ASC;

-- Ordering by expressions
SELECT 
    customer_name,
    total_orders,
    avg_order_amount
FROM customer_summary
ORDER BY total_orders * avg_order_amount DESC;

-- NULL handling in ordering
SELECT customer_name, phone
FROM customers
ORDER BY phone NULLS LAST;

-- Ordering by column position
SELECT customer_name, total_orders, avg_order_amount
FROM customer_summary
ORDER BY 2 DESC, 3 ASC;  -- Order by 2nd column DESC, then 3rd column ASC
```

#### **Limiting Results**
```sql
-- FIRST/SKIP clauses (ScratchBird/Firebird style)
SELECT FIRST 10 * FROM customers ORDER BY customer_name;
SELECT FIRST 20 SKIP 10 * FROM products ORDER BY price;

-- OFFSET/FETCH clauses (SQL standard style)
SELECT * FROM orders 
ORDER BY order_date DESC
OFFSET 50 ROWS FETCH FIRST 25 ROWS ONLY;

-- ROWS clause (simpler syntax)
SELECT * FROM products 
ORDER BY price DESC
ROWS 10;

-- Pagination examples
-- Page 1: First 20 records
SELECT FIRST 20 * FROM customers ORDER BY customer_id;

-- Page 2: Records 21-40
SELECT FIRST 20 SKIP 20 * FROM customers ORDER BY customer_id;

-- Page 3: Records 41-60
SELECT FIRST 20 SKIP 40 * FROM customers ORDER BY customer_id;
```

### Query Optimization and Hints

#### **PLAN Clause**
```sql
-- Specify execution plan
SELECT * FROM customers c
JOIN orders o ON c.customer_id = o.customer_id
PLAN (C NATURAL, O INDEX (IDX_ORDERS_CUSTOMER_ID));

-- Complex plan specification
SELECT c.customer_name, SUM(o.total_amount)
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id
WHERE c.city = 'New York'
GROUP BY c.customer_id, c.customer_name
PLAN (C INDEX (IDX_CUSTOMERS_CITY), O INDEX (IDX_ORDERS_CUSTOMER_ID));
```

#### **Optimization Hints**
```sql
-- Optimize for first rows
SELECT * FROM large_table
WHERE indexed_column = 'value'
OPTIMIZE FOR FIRST ROWS;

-- Optimize for all rows
SELECT COUNT(*) FROM large_table
WHERE condition = 'value'
OPTIMIZE FOR ALL ROWS;
```

#### **Locking Hints**
```sql
-- FOR UPDATE
SELECT * FROM inventory
WHERE product_id = 12345
FOR UPDATE;

-- FOR UPDATE with specific columns
SELECT * FROM accounts
WHERE account_id = 67890
FOR UPDATE OF balance;

-- WITH LOCK for immediate locking
SELECT * FROM critical_data
WHERE id = 100
WITH LOCK;

-- SKIP LOCKED for non-blocking reads
SELECT * FROM queue_table
WHERE status = 'PENDING'
WITH LOCK SKIP LOCKED
ROWS 1;
```

---

## INSERT Statement

The INSERT statement adds new rows to tables with support for single-row, multi-row, and SELECT-based insertions.

### Basic INSERT Syntax

#### **INSERT INTO ... VALUES**
```sql
-- Single row insert
INSERT INTO customers (customer_name, email, city)
VALUES ('John Smith', 'john@example.com', 'New York');

-- Insert with all columns
INSERT INTO products (product_id, product_name, category, price, in_stock)
VALUES (1001, 'Wireless Mouse', 'Electronics', 29.99, TRUE);

-- Insert with some columns (others get defaults)
INSERT INTO orders (customer_id, order_date)
VALUES (12345, CURRENT_DATE);

-- Insert with NULL values
INSERT INTO customers (customer_name, email, phone)
VALUES ('Jane Doe', 'jane@example.com', NULL);
```

#### **Multi-Row INSERT (SQL Dialect 4 Enhancement)**
```sql
-- Multiple rows in single statement
INSERT INTO categories (category_name, description)
VALUES 
    ('Electronics', 'Electronic devices and accessories'),
    ('Books', 'Books and literature'),
    ('Clothing', 'Apparel and accessories'),
    ('Home & Garden', 'Home improvement and gardening');

-- Multi-row with different value patterns
INSERT INTO products (product_name, category, price, in_stock)
VALUES 
    ('Laptop Computer', 'Electronics', 899.99, TRUE),
    ('SQL Programming Book', 'Books', 49.99, TRUE),
    ('Cotton T-Shirt', 'Clothing', 19.99, FALSE),
    ('Garden Hose', 'Home & Garden', 34.99, TRUE);

-- Large multi-row insert
INSERT INTO sales_data (sale_date, product_id, quantity, amount)
VALUES 
    (DATE '2024-01-01', 1001, 2, 59.98),
    (DATE '2024-01-01', 1002, 1, 899.99),
    (DATE '2024-01-02', 1001, 1, 29.99),
    (DATE '2024-01-02', 1003, 3, 149.97),
    (DATE '2024-01-03', 1002, 1, 899.99);
```

### INSERT with SELECT

#### **INSERT INTO ... SELECT**
```sql
-- Copy data from another table
INSERT INTO archived_orders (order_id, customer_id, order_date, total_amount)
SELECT order_id, customer_id, order_date, total_amount
FROM orders
WHERE order_date < DATE '2023-01-01';

-- Insert with calculated values
INSERT INTO customer_summary (customer_id, total_orders, total_spent, avg_order)
SELECT 
    customer_id,
    COUNT(*),
    SUM(total_amount),
    AVG(total_amount)
FROM orders
WHERE order_date >= DATE '2024-01-01'
GROUP BY customer_id;

-- Insert with joins
INSERT INTO customer_analysis (customer_name, total_orders, total_amount)
SELECT 
    c.customer_name,
    COUNT(o.order_id),
    SUM(o.total_amount)
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.customer_name;
```

#### **INSERT with Complex SELECT**
```sql
-- Insert with subqueries
INSERT INTO high_value_customers (customer_id, tier)
SELECT 
    customer_id,
    CASE 
        WHEN total_spent > 10000 THEN 'Platinum'
        WHEN total_spent > 5000 THEN 'Gold'
        WHEN total_spent > 1000 THEN 'Silver'
        ELSE 'Bronze'
    END
FROM (
    SELECT 
        customer_id,
        SUM(total_amount) as total_spent
    FROM orders
    WHERE order_date >= DATE '2023-01-01'
    GROUP BY customer_id
) customer_totals;

-- Insert with CTEs
WITH monthly_sales AS (
    SELECT 
        EXTRACT(YEAR FROM order_date) as year,
        EXTRACT(MONTH FROM order_date) as month,
        SUM(total_amount) as monthly_total
    FROM orders
    GROUP BY EXTRACT(YEAR FROM order_date), EXTRACT(MONTH FROM order_date)
)
INSERT INTO sales_summary (year, month, sales_amount)
SELECT year, month, monthly_total
FROM monthly_sales
WHERE monthly_total > 50000;
```

### Default Values and Identity

#### **DEFAULT VALUES**
```sql
-- Insert row with all default values
INSERT INTO audit_log DEFAULT VALUES;

-- Insert with some defaults
INSERT INTO products (product_name, category, price)
VALUES ('New Product', 'Electronics', DEFAULT);

-- Explicit DEFAULT keyword
INSERT INTO orders (customer_id, order_date, status, priority)
VALUES (12345, CURRENT_DATE, DEFAULT, 'HIGH');

-- DEFAULT in multi-row insert
INSERT INTO settings (setting_name, setting_value, is_active)
VALUES 
    ('max_connections', '100', TRUE),
    ('timeout', '300', DEFAULT),
    ('debug_mode', 'false', FALSE);
```

#### **Identity and Auto-Generated Values**
```sql
-- Insert with identity column (auto-generated)
INSERT INTO customers (customer_name, email)
VALUES ('Auto ID Customer', 'auto@example.com');
-- customer_id is automatically generated

-- Override identity generation
INSERT INTO customers (customer_id, customer_name, email)
OVERRIDING SYSTEM VALUE
VALUES (999999, 'Manual ID Customer', 'manual@example.com');

-- Restore auto-generation after override
INSERT INTO customers (customer_name, email)
OVERRIDING USER VALUE
VALUES ('Back to Auto', 'back@example.com');
```

### RETURNING Clause

#### **Basic RETURNING Usage**
```sql
-- Return generated primary key
INSERT INTO customers (customer_name, email, city)
VALUES ('New Customer', 'new@example.com', 'Boston')
RETURNING customer_id;

-- Return multiple columns
INSERT INTO orders (customer_id, order_date, total_amount)
VALUES (12345, CURRENT_DATE, 299.99)
RETURNING order_id, order_date, total_amount;

-- Return calculated values
INSERT INTO products (product_name, category, cost, markup_percent)
VALUES ('New Product', 'Electronics', 50.00, 60)
RETURNING product_id, cost * (1 + markup_percent/100) as selling_price;
```

#### **RETURNING with Multi-Row INSERT**
```sql
-- Return values from multi-row insert
INSERT INTO inventory_adjustments (product_id, adjustment_type, quantity)
VALUES 
    (1001, 'RESTOCK', 100),
    (1002, 'RESTOCK', 50),
    (1003, 'CORRECTION', -5)
RETURNING adjustment_id, product_id, quantity;

-- Return aggregated information
INSERT INTO batch_import (import_date, record_count, status)
SELECT 
    CURRENT_DATE,
    COUNT(*),
    'COMPLETED'
FROM staging_table
WHERE processed = FALSE
RETURNING import_id, record_count;
```

### Schema-Qualified INSERT

#### **Hierarchical Schema Support**
```sql
-- Insert into schema-qualified table
INSERT INTO finance.accounting.transactions (account_id, amount, description)
VALUES (12345, 1500.00, 'Monthly payment received');

-- Insert into deeply nested schema
INSERT INTO company.americas.sales.customer_contacts 
    (customer_id, contact_type, contact_value)
VALUES (67890, 'EMAIL', 'contact@customer.com');

-- Insert with schema context
SET SCHEMA 'finance.accounting';
INSERT INTO ledger_entries (entry_date, account, debit, credit)
VALUES (CURRENT_DATE, 'Cash', 1000.00, 0.00);
```

#### **Database Link INSERT**
```sql
-- Insert into remote table via database link
INSERT INTO employees@hr_link (employee_name, department, hire_date)
VALUES ('Remote Employee', 'IT', CURRENT_DATE);

-- Insert with schema-qualified remote table
INSERT INTO finance.payroll.timesheets@payroll_link 
    (employee_id, week_ending, hours_worked)
VALUES (12345, DATE '2024-01-07', 40.0);

-- Copy from local to remote
INSERT INTO backup_customers@backup_link
SELECT * FROM customers
WHERE last_modified >= CURRENT_DATE - 7;
```

---

## UPDATE Statement

The UPDATE statement modifies existing rows in tables with comprehensive filtering and value assignment capabilities.

### Basic UPDATE Syntax

#### **Simple UPDATE Operations**
```sql
-- Basic UPDATE
UPDATE customers 
SET email = 'newemail@example.com'
WHERE customer_id = 12345;

-- Update multiple columns
UPDATE products
SET price = 89.99,
    in_stock = TRUE,
    last_updated = CURRENT_TIMESTAMP
WHERE product_id = 1001;

-- Update with expressions
UPDATE employees
SET salary = salary * 1.05,
    last_review_date = CURRENT_DATE
WHERE department = 'SALES'
  AND performance_rating >= 4;

-- Update with calculations
UPDATE order_items
SET line_total = quantity * unit_price,
    discount_amount = CASE 
        WHEN quantity >= 10 THEN unit_price * quantity * 0.1
        ELSE 0
    END
WHERE order_id = 5678;
```

#### **UPDATE with Functions and Expressions**
```sql
-- String manipulation
UPDATE customers
SET customer_name = UPPER(TRIM(customer_name)),
    email = LOWER(TRIM(email))
WHERE customer_name LIKE '  %' OR customer_name LIKE '%  ';

-- Date calculations
UPDATE subscriptions
SET expiration_date = start_date + INTERVAL '1 year',
    renewal_reminder_date = expiration_date - INTERVAL '30 days'
WHERE subscription_type = 'ANNUAL';

-- Mathematical calculations
UPDATE financial_accounts
SET current_balance = previous_balance + credits - debits,
    interest_earned = previous_balance * interest_rate / 100
WHERE account_type = 'SAVINGS';

-- Conditional updates
UPDATE inventory
SET status = CASE
    WHEN quantity <= reorder_point THEN 'LOW_STOCK'
    WHEN quantity <= critical_point THEN 'CRITICAL'
    WHEN quantity > maximum_stock THEN 'OVERSTOCK'
    ELSE 'NORMAL'
END,
last_checked = CURRENT_TIMESTAMP;
```

### UPDATE with Subqueries

#### **Subqueries in SET Clause**
```sql
-- Update with scalar subquery
UPDATE customers
SET total_orders = (
    SELECT COUNT(*)
    FROM orders
    WHERE orders.customer_id = customers.customer_id
),
total_spent = (
    SELECT COALESCE(SUM(total_amount), 0)
    FROM orders
    WHERE orders.customer_id = customers.customer_id
);

-- Update with complex subquery
UPDATE products
SET avg_rating = (
    SELECT AVG(rating)
    FROM product_reviews
    WHERE product_reviews.product_id = products.product_id
),
review_count = (
    SELECT COUNT(*)
    FROM product_reviews
    WHERE product_reviews.product_id = products.product_id
)
WHERE EXISTS (
    SELECT 1 
    FROM product_reviews 
    WHERE product_reviews.product_id = products.product_id
);

-- Update with aggregated data
UPDATE departments
SET headcount = (
    SELECT COUNT(*)
    FROM employees
    WHERE employees.department_id = departments.department_id
      AND employees.status = 'ACTIVE'
),
avg_salary = (
    SELECT AVG(salary)
    FROM employees
    WHERE employees.department_id = departments.department_id
      AND employees.status = 'ACTIVE'
);
```

#### **UPDATE with EXISTS**
```sql
-- Update based on existence of related data
UPDATE customers
SET status = 'ACTIVE',
    last_order_date = (
        SELECT MAX(order_date)
        FROM orders
        WHERE orders.customer_id = customers.customer_id
    )
WHERE EXISTS (
    SELECT 1
    FROM orders
    WHERE orders.customer_id = customers.customer_id
      AND orders.order_date >= CURRENT_DATE - 365
);

-- Update with NOT EXISTS
UPDATE products
SET status = 'DISCONTINUED',
    discontinued_date = CURRENT_DATE
WHERE NOT EXISTS (
    SELECT 1
    FROM order_items
    WHERE order_items.product_id = products.product_id
      AND EXISTS (
          SELECT 1
          FROM orders
          WHERE orders.order_id = order_items.order_id
            AND orders.order_date >= CURRENT_DATE - 730
      )
);
```

### UPDATE with Joins (Non-Standard Extension)

#### **UPDATE with FROM Clause**
```sql
-- Update using data from another table
UPDATE inventory i
SET quantity_available = i.quantity_on_hand - COALESCE(r.reserved_quantity, 0)
FROM (
    SELECT 
        product_id,
        SUM(quantity) as reserved_quantity
    FROM reservations
    WHERE status = 'ACTIVE'
    GROUP BY product_id
) r
WHERE i.product_id = r.product_id;

-- Update with multiple table join
UPDATE employee_metrics em
SET performance_score = (ps.quality_score + ps.productivity_score) / 2,
    department_rank = ps.dept_rank
FROM (
    SELECT 
        employee_id,
        quality_score,
        productivity_score,
        RANK() OVER (PARTITION BY department_id ORDER BY (quality_score + productivity_score) DESC) as dept_rank
    FROM performance_scores
    WHERE review_period = '2024-Q1'
) ps
WHERE em.employee_id = ps.employee_id;
```

### UPDATE with DEFAULT Values

#### **Setting DEFAULT Values**
```sql
-- Reset columns to their default values
UPDATE customers
SET status = DEFAULT,
    credit_limit = DEFAULT
WHERE customer_type = 'TRIAL'
  AND created_date < CURRENT_DATE - 30;

-- Partial reset with conditions
UPDATE products
SET discount_percent = DEFAULT,
    promotion_end_date = DEFAULT
WHERE promotion_end_date <= CURRENT_DATE;

-- Mixed DEFAULT and explicit values
UPDATE user_preferences
SET theme = DEFAULT,
    language = 'EN',
    notifications = DEFAULT,
    last_updated = CURRENT_TIMESTAMP
WHERE user_id = 12345;
```

### Positioned UPDATE (Cursor-Based)

#### **UPDATE WHERE CURRENT OF**
```sql
-- Positioned update within cursor loop
-- (Used within stored procedures)
FOR SELECT customer_id, balance
    FROM accounts
    WHERE account_type = 'CHECKING'
    INTO :customer_id, :balance
    AS CURSOR account_cursor
DO
BEGIN
    -- Calculate new balance
    new_balance = balance * 1.02;  -- 2% interest
    
    -- Positioned update
    UPDATE accounts
    SET balance = :new_balance,
        last_interest_date = CURRENT_DATE
    WHERE CURRENT OF account_cursor;
END
```

### UPDATE with RETURNING

#### **RETURNING Updated Values**
```sql
-- Return updated values
UPDATE products
SET price = price * 1.1,
    last_price_change = CURRENT_DATE
WHERE category = 'Electronics'
RETURNING product_id, product_name, price;

-- Return calculated values
UPDATE inventory
SET quantity = quantity - :sold_quantity,
    last_sale_date = CURRENT_DATE
WHERE product_id = :product_id
RETURNING product_id, quantity, 
         CASE WHEN quantity <= reorder_point THEN 'REORDER' ELSE 'OK' END as status;

-- Return aggregate information
UPDATE batch_processing
SET status = 'COMPLETED',
    end_time = CURRENT_TIMESTAMP
WHERE batch_id = :batch_id
RETURNING batch_id, start_time, end_time, 
         end_time - start_time as processing_duration;
```

### Schema-Qualified UPDATE

#### **UPDATE with Hierarchical Schemas**
```sql
-- Update in specific schema
UPDATE finance.accounting.ledger
SET reconciled = TRUE,
    reconciliation_date = CURRENT_DATE
WHERE entry_date = DATE '2024-01-31'
  AND reconciled = FALSE;

-- Update with schema context
SET SCHEMA 'inventory.warehouse';
UPDATE stock_levels
SET quantity = quantity + :adjustment_quantity,
    last_adjustment = CURRENT_TIMESTAMP
WHERE product_id = :product_id
  AND location_id = :location_id;

-- Cross-schema update
UPDATE company.americas.sales.opportunities
SET assigned_rep = (
    SELECT employee_id
    FROM company.americas.hr.employees
    WHERE territory = 'WEST_COAST'
      AND status = 'ACTIVE'
      AND specialization = 'ENTERPRISE_SALES'
    ORDER BY current_workload
    LIMIT 1
)
WHERE territory = 'WEST_COAST'
  AND assigned_rep IS NULL;
```

#### **UPDATE via Database Links**
```sql
-- Update remote table via database link
UPDATE employees@hr_link
SET salary = salary * 1.03,
    last_review_date = CURRENT_DATE
WHERE department = 'ENGINEERING'
  AND performance_rating >= 3;

-- Update with remote data
UPDATE local_inventory
SET supplier_cost = (
    SELECT current_cost
    FROM supplier_catalog@supplier_link sc
    WHERE sc.product_code = local_inventory.product_code
)
WHERE supplier_id = 'SUPPLIER_001';
```

### Performance Optimization for UPDATE

#### **UPDATE with Indexes and Plans**
```sql
-- UPDATE with plan hint
UPDATE large_table
SET status = 'PROCESSED',
    processed_date = CURRENT_TIMESTAMP
WHERE unprocessed_flag = 'Y'
PLAN (LARGE_TABLE INDEX (IDX_UNPROCESSED_FLAG));

-- Batch processing with ROWS limit
UPDATE payment_queue
SET status = 'PROCESSING',
    started_at = CURRENT_TIMESTAMP
WHERE status = 'PENDING'
ROWS 1000;  -- Process in batches

-- UPDATE with locking hints
UPDATE critical_inventory
SET reserved_quantity = reserved_quantity + :reservation_amount
WHERE product_id = :product_id
  AND location_id = :location_id
SKIP LOCKED  -- Skip if locked by another transaction
RETURNING available_quantity;
```

---

## DELETE Statement

The DELETE statement removes rows from tables with comprehensive filtering and performance optimization options.

### Basic DELETE Syntax

#### **Simple DELETE Operations**
```sql
-- Basic DELETE
DELETE FROM customers
WHERE customer_id = 12345;

-- DELETE with multiple conditions
DELETE FROM products
WHERE category = 'DISCONTINUED'
  AND last_sale_date < DATE '2022-01-01'
  AND inventory_quantity = 0;

-- DELETE all rows (use with caution)
DELETE FROM temp_data;

-- DELETE with calculated conditions
DELETE FROM session_logs
WHERE session_start < CURRENT_TIMESTAMP - INTERVAL '30 days'
  AND session_duration < 60;  -- Less than 1 minute sessions
```

#### **DELETE with Complex WHERE Clauses**
```sql
-- DELETE with subquery
DELETE FROM customers
WHERE customer_id IN (
    SELECT customer_id
    FROM customer_blacklist
    WHERE reason = 'FRAUD'
);

-- DELETE with NOT EXISTS
DELETE FROM products
WHERE NOT EXISTS (
    SELECT 1
    FROM order_items
    WHERE order_items.product_id = products.product_id
)
AND creation_date < CURRENT_DATE - 365;

-- DELETE with correlated subquery
DELETE FROM inventory_transactions
WHERE transaction_date < (
    SELECT MIN(fiscal_year_start)
    FROM fiscal_periods
    WHERE status = 'OPEN'
)
AND reconciled = TRUE;

-- DELETE with complex logic
DELETE FROM user_sessions
WHERE (last_activity < CURRENT_TIMESTAMP - INTERVAL '24 hours')
   OR (session_type = 'TEMP' AND created_at < CURRENT_TIMESTAMP - INTERVAL '1 hour')
   OR (user_id IN (SELECT user_id FROM suspended_users));
```

### DELETE with Joins and FROM Clause

#### **DELETE with Additional Tables**
```sql
-- DELETE using data from related tables
DELETE FROM order_items oi
WHERE EXISTS (
    SELECT 1
    FROM orders o
    WHERE o.order_id = oi.order_id
      AND o.status = 'CANCELLED'
      AND o.order_date < CURRENT_DATE - 30
);

-- DELETE with multiple table references
DELETE FROM customer_preferences cp
WHERE cp.customer_id IN (
    SELECT c.customer_id
    FROM customers c
    LEFT JOIN orders o ON c.customer_id = o.customer_id
    WHERE o.customer_id IS NULL  -- No orders
      AND c.registration_date < CURRENT_DATE - 365
);

-- DELETE orphaned records
DELETE FROM product_images
WHERE product_id NOT IN (
    SELECT product_id
    FROM products
    WHERE status != 'DELETED'
);
```

### Positioned DELETE (Cursor-Based)

#### **DELETE WHERE CURRENT OF**
```sql
-- Positioned delete within cursor loop
-- (Used within stored procedures)
FOR SELECT log_id, log_date
    FROM system_logs
    WHERE log_level = 'DEBUG'
      AND log_date < CURRENT_DATE - 7
    AS CURSOR log_cursor
DO
BEGIN
    -- Archive the log entry first
    INSERT INTO archived_logs 
    SELECT * FROM system_logs WHERE log_id = :log_id;
    
    -- Then delete the original
    DELETE FROM system_logs
    WHERE CURRENT OF log_cursor;
END
```

### DELETE with RETURNING

#### **RETURNING Deleted Values**
```sql
-- Return deleted row information
DELETE FROM expired_coupons
WHERE expiration_date <= CURRENT_DATE
RETURNING coupon_id, coupon_code, discount_amount;

-- Return calculated values for deleted rows
DELETE FROM completed_tasks
WHERE completion_date < CURRENT_DATE - 90
  AND status = 'COMPLETED'
RETURNING task_id, 
         completion_date,
         CURRENT_DATE - completion_date as days_since_completion;

-- Return aggregate information
DELETE FROM log_entries
WHERE log_date < :cutoff_date
RETURNING COUNT(*) as deleted_count,
         MIN(log_date) as earliest_deleted,
         MAX(log_date) as latest_deleted;
```

#### **RETURNING for Audit Trails**
```sql
-- Create audit trail while deleting
WITH deleted_records AS (
    DELETE FROM sensitive_data
    WHERE data_classification = 'OBSOLETE'
      AND retention_period_end < CURRENT_DATE
    RETURNING record_id, data_type, deletion_reason, CURRENT_TIMESTAMP as deleted_at
)
INSERT INTO deletion_audit (record_id, data_type, reason, deleted_at, deleted_by)
SELECT record_id, data_type, deletion_reason, deleted_at, CURRENT_USER
FROM deleted_records;
```

### Cascading DELETE Operations

#### **Manual Cascade DELETE**
```sql
-- Delete in proper order to maintain referential integrity
-- First delete child records
DELETE FROM order_items
WHERE order_id IN (
    SELECT order_id
    FROM orders
    WHERE customer_id = :customer_to_delete
);

-- Then delete parent records
DELETE FROM orders
WHERE customer_id = :customer_to_delete;

-- Finally delete the main record
DELETE FROM customers
WHERE customer_id = :customer_to_delete;

-- Alternative: Delete related data in single statement
DELETE FROM customer_addresses
WHERE customer_id = :customer_to_delete;

DELETE FROM customer_preferences  
WHERE customer_id = :customer_to_delete;

DELETE FROM customer_notes
WHERE customer_id = :customer_to_delete;

DELETE FROM customers
WHERE customer_id = :customer_to_delete;
```

#### **Bulk Cascade DELETE**
```sql
-- Bulk delete with cascading to related tables
-- Delete all data for inactive customers
WITH inactive_customers AS (
    SELECT customer_id
    FROM customers
    WHERE status = 'INACTIVE'
      AND last_login < CURRENT_DATE - 730  -- 2 years
)
DELETE FROM customer_sessions
WHERE customer_id IN (SELECT customer_id FROM inactive_customers);

-- Continue with other related tables
WITH inactive_customers AS (
    SELECT customer_id
    FROM customers
    WHERE status = 'INACTIVE'
      AND last_login < CURRENT_DATE - 730
)
DELETE FROM customer_preferences
WHERE customer_id IN (SELECT customer_id FROM inactive_customers);

-- Finally delete the main customer records
DELETE FROM customers
WHERE status = 'INACTIVE'
  AND last_login < CURRENT_DATE - 730;
```

### Schema-Qualified DELETE

#### **DELETE from Hierarchical Schemas**
```sql
-- Delete from specific schema
DELETE FROM finance.accounting.temp_calculations
WHERE calculation_date < CURRENT_DATE;

-- Delete with schema context
SET SCHEMA 'inventory.warehouse';
DELETE FROM stock_movements
WHERE movement_date < CURRENT_DATE - 90
  AND movement_type = 'ADJUSTMENT'
  AND verified = TRUE;

-- Cross-schema DELETE
DELETE FROM company.americas.sales.leads
WHERE lead_source = 'EXPIRED_CAMPAIGN'
  AND created_date < (
      SELECT campaign_end_date
      FROM company.marketing.campaigns
      WHERE campaign_id = 'SUMMER2023'
  );
```

#### **DELETE via Database Links**
```sql
-- Delete from remote table
DELETE FROM archived_data@archive_link
WHERE archive_date < CURRENT_DATE - 2555;  -- 7 years

-- Delete with remote reference
DELETE FROM local_cache
WHERE record_id NOT IN (
    SELECT record_id
    FROM master_data@master_link
    WHERE status = 'ACTIVE'
);

-- Synchronized delete across links
DELETE FROM employee_cache
WHERE employee_id IN (
    SELECT employee_id
    FROM terminated_employees@hr_link
    WHERE termination_date < CURRENT_DATE - 365
);
```

### Performance Optimization for DELETE

#### **Batch DELETE Operations**
```sql
-- Delete in batches to avoid long-running transactions
DELETE FROM large_log_table
WHERE log_date < CURRENT_DATE - 365
ROWS 10000;  -- Delete maximum 10,000 rows

-- Iterative batch delete (in stored procedure)
WHILE (1=1) DO
BEGIN
    DELETE FROM audit_trail
    WHERE audit_date < :cutoff_date
    ROWS 5000;
    
    IF (ROW_COUNT = 0) THEN
        BREAK;
    
    -- Commit batch and continue
    COMMIT;
END

-- DELETE with index hints
DELETE FROM transaction_log
WHERE transaction_date < DATE '2023-01-01'
PLAN (TRANSACTION_LOG INDEX (IDX_TRANSACTION_DATE));
```

#### **DELETE with Locking Options**
```sql
-- Non-blocking DELETE
DELETE FROM queue_items
WHERE status = 'PROCESSED'
  AND processed_date < CURRENT_TIMESTAMP - INTERVAL '1 hour'
SKIP LOCKED;  -- Skip rows locked by other transactions

-- DELETE with specific plan
DELETE FROM large_table lt
WHERE lt.category = 'OBSOLETE'
  AND NOT EXISTS (
      SELECT 1
      FROM related_table rt
      WHERE rt.parent_id = lt.table_id
  )
PLAN (LT INDEX (IDX_CATEGORY));
```

---

## MERGE Statement

The MERGE statement provides advanced "upsert" functionality, combining INSERT, UPDATE, and DELETE operations in a single statement.

### Basic MERGE Syntax

#### **Simple MERGE Operations**
```sql
-- Basic MERGE with INSERT and UPDATE
MERGE INTO target_customers tc
USING source_customers sc ON tc.customer_id = sc.customer_id
WHEN MATCHED THEN
    UPDATE SET 
        customer_name = sc.customer_name,
        email = sc.email,
        last_updated = CURRENT_TIMESTAMP
WHEN NOT MATCHED THEN
    INSERT (customer_id, customer_name, email, created_date)
    VALUES (sc.customer_id, sc.customer_name, sc.email, CURRENT_DATE);

-- MERGE with conditional logic
MERGE INTO inventory i
USING daily_adjustments da ON i.product_id = da.product_id
WHEN MATCHED AND da.adjustment_type = 'INCREASE' THEN
    UPDATE SET 
        quantity = i.quantity + da.adjustment_quantity,
        last_updated = CURRENT_TIMESTAMP
WHEN MATCHED AND da.adjustment_type = 'DECREASE' THEN
    UPDATE SET 
        quantity = i.quantity - da.adjustment_quantity,
        last_updated = CURRENT_TIMESTAMP
WHEN NOT MATCHED AND da.adjustment_type = 'NEW_PRODUCT' THEN
    INSERT (product_id, quantity, last_updated)
    VALUES (da.product_id, da.adjustment_quantity, CURRENT_TIMESTAMP);
```

#### **MERGE with DELETE**
```sql
-- MERGE with all three operations: INSERT, UPDATE, DELETE
MERGE INTO customer_status cs
USING (
    SELECT 
        customer_id,
        CASE 
            WHEN total_orders = 0 THEN 'INACTIVE'
            WHEN total_orders >= 20 THEN 'VIP'
            WHEN total_orders >= 5 THEN 'REGULAR'
            ELSE 'OCCASIONAL'
        END as new_status,
        last_order_date
    FROM customer_summary
) summary ON cs.customer_id = summary.customer_id
WHEN MATCHED AND summary.last_order_date < CURRENT_DATE - 1095 THEN  -- 3 years
    DELETE
WHEN MATCHED AND summary.new_status != cs.status THEN
    UPDATE SET 
        status = summary.new_status,
        status_change_date = CURRENT_DATE
WHEN NOT MATCHED THEN
    INSERT (customer_id, status, status_change_date)
    VALUES (summary.customer_id, summary.new_status, CURRENT_DATE);
```

### Advanced MERGE Operations

#### **MERGE with Complex Source Queries**
```sql
-- MERGE using CTE as source
WITH product_analytics AS (
    SELECT 
        p.product_id,
        p.product_name,
        COALESCE(SUM(oi.quantity), 0) as total_sold,
        COALESCE(AVG(pr.rating), 0) as avg_rating,
        COUNT(DISTINCT o.customer_id) as unique_customers,
        MAX(o.order_date) as last_sale_date
    FROM products p
    LEFT JOIN order_items oi ON p.product_id = oi.product_id
    LEFT JOIN orders o ON oi.order_id = o.order_id
    LEFT JOIN product_reviews pr ON p.product_id = pr.product_id
    WHERE o.order_date >= CURRENT_DATE - 365 OR o.order_date IS NULL
    GROUP BY p.product_id, p.product_name
)
MERGE INTO product_performance pp
USING product_analytics pa ON pp.product_id = pa.product_id
WHEN MATCHED AND pa.last_sale_date IS NULL THEN
    UPDATE SET 
        status = 'NO_SALES',
        total_sold = 0,
        avg_rating = NULL,
        last_sale_date = NULL,
        analysis_date = CURRENT_DATE
WHEN MATCHED THEN
    UPDATE SET
        total_sold = pa.total_sold,
        avg_rating = pa.avg_rating,
        unique_customers = pa.unique_customers,
        last_sale_date = pa.last_sale_date,
        status = CASE 
            WHEN pa.total_sold >= 1000 THEN 'BEST_SELLER'
            WHEN pa.total_sold >= 100 THEN 'GOOD_SELLER'
            WHEN pa.total_sold >= 10 THEN 'MODERATE_SELLER'
            ELSE 'SLOW_SELLER'
        END,
        analysis_date = CURRENT_DATE
WHEN NOT MATCHED THEN
    INSERT (product_id, total_sold, avg_rating, unique_customers, 
            last_sale_date, status, analysis_date)
    VALUES (pa.product_id, pa.total_sold, pa.avg_rating, pa.unique_customers,
            pa.last_sale_date, 
            CASE 
                WHEN pa.total_sold >= 1000 THEN 'BEST_SELLER'
                WHEN pa.total_sold >= 100 THEN 'GOOD_SELLER'
                WHEN pa.total_sold >= 10 THEN 'MODERATE_SELLER'
                ELSE 'SLOW_SELLER'
            END,
            CURRENT_DATE);
```

#### **MERGE with WHEN NOT MATCHED BY SOURCE**
```sql
-- Handle records that exist in target but not in source
MERGE INTO current_prices cp
USING new_price_list npl ON cp.product_id = npl.product_id
WHEN MATCHED AND npl.price != cp.current_price THEN
    UPDATE SET 
        previous_price = cp.current_price,
        current_price = npl.price,
        price_change_date = CURRENT_DATE,
        change_reason = npl.change_reason
WHEN NOT MATCHED BY TARGET THEN
    INSERT (product_id, current_price, previous_price, price_change_date, change_reason)
    VALUES (npl.product_id, npl.price, NULL, CURRENT_DATE, npl.change_reason)
WHEN NOT MATCHED BY SOURCE THEN
    UPDATE SET 
        status = 'DISCONTINUED',
        discontinued_date = CURRENT_DATE;
```

### MERGE with RETURNING

#### **RETURNING MERGE Results**
```sql
-- Return information about MERGE operations
MERGE INTO customer_loyalty cl
USING (
    SELECT 
        customer_id,
        SUM(total_amount) as total_spent,
        COUNT(*) as order_count
    FROM orders
    WHERE order_date >= CURRENT_DATE - 365
    GROUP BY customer_id
) annual_summary ON cl.customer_id = annual_summary.customer_id
WHEN MATCHED THEN
    UPDATE SET 
        annual_spending = annual_summary.total_spent,
        annual_orders = annual_summary.order_count,
        loyalty_tier = CASE 
            WHEN annual_summary.total_spent >= 10000 THEN 'PLATINUM'
            WHEN annual_summary.total_spent >= 5000 THEN 'GOLD'
            WHEN annual_summary.total_spent >= 1000 THEN 'SILVER'
            ELSE 'BRONZE'
        END,
        last_calculation = CURRENT_DATE
WHEN NOT MATCHED THEN
    INSERT (customer_id, annual_spending, annual_orders, loyalty_tier, last_calculation)
    VALUES (annual_summary.customer_id, annual_summary.total_spent, 
            annual_summary.order_count,
            CASE 
                WHEN annual_summary.total_spent >= 10000 THEN 'PLATINUM'
                WHEN annual_summary.total_spent >= 5000 THEN 'GOLD'
                WHEN annual_summary.total_spent >= 1000 THEN 'SILVER'
                ELSE 'BRONZE'
            END,
            CURRENT_DATE)
RETURNING customer_id, loyalty_tier, annual_spending,
         CASE 
             WHEN INSERTING THEN 'NEW_CUSTOMER'
             WHEN UPDATING THEN 'UPDATED_CUSTOMER'
         END as operation_type;
```

### Schema-Qualified MERGE

#### **MERGE with Hierarchical Schemas**
```sql
-- MERGE across schemas
MERGE INTO finance.accounting.account_balances ab
USING (
    SELECT 
        account_id,
        SUM(CASE WHEN transaction_type = 'DEBIT' THEN amount ELSE 0 END) as total_debits,
        SUM(CASE WHEN transaction_type = 'CREDIT' THEN amount ELSE 0 END) as total_credits
    FROM finance.accounting.transactions
    WHERE transaction_date = CURRENT_DATE
    GROUP BY account_id
) daily_totals ON ab.account_id = daily_totals.account_id
WHEN MATCHED THEN
    UPDATE SET 
        current_balance = ab.opening_balance + daily_totals.total_credits - daily_totals.total_debits,
        total_debits = daily_totals.total_debits,
        total_credits = daily_totals.total_credits,
        last_update = CURRENT_TIMESTAMP
WHEN NOT MATCHED THEN
    INSERT (account_id, opening_balance, current_balance, total_debits, total_credits, last_update)
    VALUES (daily_totals.account_id, 0, 
            daily_totals.total_credits - daily_totals.total_debits,
            daily_totals.total_debits, daily_totals.total_credits, CURRENT_TIMESTAMP);
```

#### **MERGE via Database Links**
```sql
-- MERGE with remote data
MERGE INTO local_employee_cache lec
USING (
    SELECT employee_id, employee_name, department, salary, status
    FROM employees@hr_link
    WHERE last_modified >= CURRENT_DATE - 1
) remote_employees ON lec.employee_id = remote_employees.employee_id
WHEN MATCHED AND remote_employees.status = 'TERMINATED' THEN
    DELETE
WHEN MATCHED THEN
    UPDATE SET 
        employee_name = remote_employees.employee_name,
        department = remote_employees.department,
        salary = remote_employees.salary,
        status = remote_employees.status,
        cache_updated = CURRENT_TIMESTAMP
WHEN NOT MATCHED AND remote_employees.status = 'ACTIVE' THEN
    INSERT (employee_id, employee_name, department, salary, status, cache_created)
    VALUES (remote_employees.employee_id, remote_employees.employee_name,
            remote_employees.department, remote_employees.salary,
            remote_employees.status, CURRENT_TIMESTAMP);
```

---

## UPDATE OR INSERT Statement (ScratchBird-Specific)

The UPDATE OR INSERT statement is a ScratchBird/Firebird-specific upsert operation that's simpler than MERGE for basic scenarios.

### Basic UPDATE OR INSERT Syntax

#### **Simple UPDATE OR INSERT**
```sql
-- Basic upsert operation
UPDATE OR INSERT INTO customer_summary (customer_id, total_orders, total_spent)
VALUES (12345, 15, 2500.00)
MATCHING (customer_id);

-- Multiple column matching
UPDATE OR INSERT INTO product_pricing (product_id, store_id, price, effective_date)
VALUES (1001, 5, 89.99, CURRENT_DATE)
MATCHING (product_id, store_id);

-- UPDATE OR INSERT with all columns
UPDATE OR INSERT INTO user_preferences (user_id, theme, language, timezone, notifications)
VALUES (:user_id, :theme, :language, :timezone, :notifications)
MATCHING (user_id);
```

#### **UPDATE OR INSERT with Expressions**
```sql
-- Using calculated values
UPDATE OR INSERT INTO monthly_sales_summary (year, month, total_sales, transaction_count)
VALUES (
    EXTRACT(YEAR FROM :report_date),
    EXTRACT(MONTH FROM :report_date),
    (SELECT SUM(amount) FROM sales WHERE EXTRACT(MONTH FROM sale_date) = EXTRACT(MONTH FROM :report_date)),
    (SELECT COUNT(*) FROM sales WHERE EXTRACT(MONTH FROM sale_date) = EXTRACT(MONTH FROM :report_date))
)
MATCHING (year, month);

-- Using functions and defaults
UPDATE OR INSERT INTO session_tracking (user_id, last_login, login_count, session_duration)
VALUES (:user_id, CURRENT_TIMESTAMP, 1, 0)
MATCHING (user_id);
```

### UPDATE OR INSERT with RETURNING

#### **RETURNING Values from UPDATE OR INSERT**
```sql
-- Return whether row was updated or inserted
UPDATE OR INSERT INTO inventory_levels (product_id, location_id, quantity, last_updated)
VALUES (:product_id, :location_id, :new_quantity, CURRENT_TIMESTAMP)
MATCHING (product_id, location_id)
RETURNING product_id, quantity, 
         CASE 
             WHEN INSERTING THEN 'CREATED'
             WHEN UPDATING THEN 'UPDATED'
         END as operation;

-- Return calculated values
UPDATE OR INSERT INTO customer_metrics (customer_id, loyalty_points, tier_level)
VALUES (:customer_id, :points_earned, 
        CASE 
            WHEN :points_earned >= 1000 THEN 'GOLD'
            WHEN :points_earned >= 500 THEN 'SILVER'
            ELSE 'BRONZE'
        END)
MATCHING (customer_id)
RETURNING customer_id, loyalty_points, tier_level;
```

