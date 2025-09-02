### DDL: Views

**What it is**

Views are virtual tables defined by SELECT queries that present data from one or more underlying tables in a specific way. They don't store data themselves but provide a saved query that can be referenced like a table. ScratchBird supports standard views, updatable views with CHECK OPTION, and serves as the foundation for materialized views.

**Why it matters**

- **Abstraction**: Hide complex queries and schema details from applications
- **Security**: Restrict access to specific columns or rows
- **Simplification**: Present denormalized data from normalized tables
- **Compatibility**: Maintain stable interfaces when underlying tables change
- **Reusability**: Share common query logic across applications

**How to use it**

Create views to encapsulate complex queries, implement row-level security, or provide stable APIs for applications. Use WITH CHECK OPTION for updatable views to maintain data integrity. Consider materialized views for expensive queries that don't need real-time data.

## CREATE VIEW

### Basic Syntax

```sql
CREATE [OR REPLACE] VIEW view_name [(column_list)]
AS select_statement
[WITH [CASCADED|LOCAL] CHECK OPTION];
```

### Simple Views

```sql
-- Basic view
CREATE VIEW active_users AS
SELECT id, username, email, created_at
FROM users
WHERE is_active = TRUE;

-- View with column aliases
CREATE VIEW customer_summary AS
SELECT 
    c.id AS customer_id,
    c.name AS customer_name,
    COUNT(o.id) AS order_count,
    SUM(o.total) AS total_spent
FROM customers c
LEFT JOIN orders o ON c.id = o.customer_id
GROUP BY c.id, c.name;

-- View with explicit column list
CREATE VIEW product_info (
    product_code,
    product_name,
    category,
    current_price,
    in_stock
) AS
SELECT 
    sku,
    name,
    category_name,
    price,
    quantity > 0
FROM products p
JOIN categories c ON p.category_id = c.id;
```

### Complex Views

```sql
-- View with CTEs
CREATE VIEW monthly_sales_summary AS
WITH monthly_totals AS (
    SELECT 
        DATE_TRUNC('month', order_date) AS month,
        SUM(total) AS revenue,
        COUNT(*) AS order_count
    FROM orders
    WHERE status = 'completed'
    GROUP BY DATE_TRUNC('month', order_date)
)
SELECT 
    month,
    revenue,
    order_count,
    revenue / order_count AS avg_order_value,
    LAG(revenue) OVER (ORDER BY month) AS prev_month_revenue,
    revenue - LAG(revenue) OVER (ORDER BY month) AS revenue_change
FROM monthly_totals;

-- View with UNION
CREATE VIEW all_contacts AS
SELECT 'customer' AS contact_type, id, name, email FROM customers
UNION ALL
SELECT 'supplier' AS contact_type, id, name, email FROM suppliers
UNION ALL
SELECT 'employee' AS contact_type, id, 
       first_name || ' ' || last_name, email FROM employees;

-- Hierarchical view
CREATE VIEW category_tree AS
WITH RECURSIVE cat_hierarchy AS (
    -- Root categories
    SELECT 
        id,
        name,
        parent_id,
        name AS path,
        0 AS level
    FROM categories
    WHERE parent_id IS NULL
    
    UNION ALL
    
    -- Subcategories
    SELECT 
        c.id,
        c.name,
        c.parent_id,
        h.path || ' > ' || c.name,
        h.level + 1
    FROM categories c
    JOIN cat_hierarchy h ON c.parent_id = h.id
)
SELECT * FROM cat_hierarchy
ORDER BY path;
```

## Updatable Views

Views can be updatable if they meet certain conditions:

### Simple Updatable Views

```sql
-- Updatable view (single table, no aggregates)
CREATE VIEW active_products AS
SELECT id, name, price, category_id
FROM products
WHERE is_active = TRUE;

-- Updates through view
UPDATE active_products SET price = price * 1.10 WHERE category_id = 5;
INSERT INTO active_products (name, price, category_id) VALUES ('New Item', 29.99, 5);
DELETE FROM active_products WHERE id = 100;
```

### WITH CHECK OPTION

Ensures modifications through the view satisfy the view's WHERE clause:

```sql
-- WITH CHECK OPTION prevents violations
CREATE VIEW premium_customers AS
SELECT * FROM customers
WHERE total_purchases > 10000
WITH CHECK OPTION;

-- This INSERT would fail (CHECK OPTION violation)
-- INSERT INTO premium_customers (name, total_purchases) 
-- VALUES ('New Customer', 500);  -- Error: violates CHECK OPTION

-- LOCAL vs CASCADED
-- LOCAL: Check only this view's WHERE clause
CREATE VIEW local_view AS
SELECT * FROM base_view
WHERE condition
WITH LOCAL CHECK OPTION;

-- CASCADED: Check this view and all underlying views (default)
CREATE VIEW cascaded_view AS
SELECT * FROM base_view
WHERE condition
WITH CASCADED CHECK OPTION;
```

### Complex Updatable Views

```sql
-- View with computed columns (some updatable)
CREATE VIEW employee_details AS
SELECT 
    id,
    first_name,
    last_name,
    first_name || ' ' || last_name AS full_name,  -- Not updatable
    salary,
    salary * 0.15 AS tax_amount,  -- Not updatable
    department_id
FROM employees;

-- Can update base columns
UPDATE employee_details 
SET salary = 75000 
WHERE id = 123;

-- Cannot update computed columns
-- UPDATE employee_details 
-- SET full_name = 'John Smith'  -- Error: cannot update computed column
-- WHERE id = 123;
```

## View Security

### Row-Level Security with Views

```sql
-- Create base table
CREATE TABLE documents (
    id INTEGER PRIMARY KEY,
    title VARCHAR(200),
    content TEXT,
    owner_id INTEGER,
    department_id INTEGER,
    is_public BOOLEAN DEFAULT FALSE
);

-- Department-specific view
CREATE VIEW department_documents AS
SELECT * FROM documents
WHERE department_id = current_setting('app.current_department')::INTEGER
   OR is_public = TRUE;

-- User-specific view
CREATE VIEW my_documents AS
SELECT * FROM documents
WHERE owner_id = current_setting('app.current_user')::INTEGER;

-- Grant access to views, not tables
REVOKE ALL ON documents FROM PUBLIC;
GRANT SELECT ON department_documents TO department_users;
GRANT ALL ON my_documents TO authenticated_users;
```

### Column-Level Security

```sql
-- Hide sensitive columns
CREATE VIEW public_employees AS
SELECT 
    id,
    first_name,
    last_name,
    department_id,
    hire_date
    -- Exclude: salary, ssn, performance_rating
FROM employees;

-- Different views for different roles
CREATE VIEW hr_employees AS
SELECT 
    id,
    first_name,
    last_name,
    department_id,
    hire_date,
    salary,
    performance_rating
    -- Still exclude: ssn
FROM employees;

-- Full access view
CREATE VIEW admin_employees AS
SELECT * FROM employees;

-- Grant appropriate access
GRANT SELECT ON public_employees TO PUBLIC;
GRANT SELECT ON hr_employees TO hr_role;
GRANT ALL ON admin_employees TO admin_role;
```

## ALTER VIEW

Modify existing views:

### Rename View

```sql
-- Rename view
ALTER VIEW old_view_name RENAME TO new_view_name;

-- Example
ALTER VIEW temp_summary RENAME TO quarterly_summary;
```

### Change Owner

```sql
-- Change view owner
ALTER VIEW customer_summary OWNER TO analytics_team;
```

### Alter Column

```sql
-- Rename view column (if supported)
ALTER VIEW product_info RENAME COLUMN product_code TO sku;

-- Change column default (for updatable views)
ALTER VIEW active_users ALTER COLUMN status SET DEFAULT 'active';
```

## DROP VIEW

Remove views:

```sql
-- Basic drop
DROP VIEW old_view;

-- Drop if exists
DROP VIEW IF EXISTS temporary_view;

-- Drop cascade (drops dependent objects)
DROP VIEW parent_view CASCADE;

-- Drop restrict (fail if dependencies exist)
DROP VIEW important_view RESTRICT;

-- Drop multiple views
DROP VIEW view1, view2, view3;
```

## RECREATE VIEW

Replace existing view (shorthand for DROP + CREATE):

```sql
-- Recreate with new definition
RECREATE VIEW customer_summary AS
SELECT 
    c.id AS customer_id,
    c.name AS customer_name,
    c.email,  -- Added email
    COUNT(o.id) AS order_count,
    SUM(o.total) AS total_spent,
    MAX(o.order_date) AS last_order_date  -- Added last order
FROM customers c
LEFT JOIN orders o ON c.id = o.customer_id
GROUP BY c.id, c.name, c.email;
```

## View Dependencies

### Managing Dependencies

```sql
-- Find view dependencies
SELECT 
    dependent_ns.nspname AS dependent_schema,
    dependent_view.relname AS dependent_view,
    source_ns.nspname AS source_schema,
    source_table.relname AS source_table
FROM pg_depend 
JOIN pg_rewrite ON pg_depend.objid = pg_rewrite.oid 
JOIN pg_class AS dependent_view ON pg_rewrite.ev_class = dependent_view.oid 
JOIN pg_class AS source_table ON pg_depend.refobjid = source_table.oid 
JOIN pg_namespace dependent_ns ON dependent_view.relnamespace = dependent_ns.oid
JOIN pg_namespace source_ns ON source_table.relnamespace = source_ns.oid
WHERE dependent_view.relkind = 'v'
  AND source_table.relname = 'your_table_name';

-- Find views depending on a column
SELECT DISTINCT 
    n.nspname AS schema_name,
    c.relname AS view_name
FROM pg_depend d
JOIN pg_rewrite r ON r.oid = d.objid
JOIN pg_class c ON c.oid = r.ev_class
JOIN pg_namespace n ON n.oid = c.relnamespace
JOIN pg_attribute a ON a.attrelid = d.refobjid AND a.attnum = d.refobjsubid
WHERE c.relkind = 'v'
  AND a.attname = 'column_name';
```

### Handling Schema Changes

```sql
-- Strategy 1: Drop and recreate
BEGIN;
DROP VIEW dependent_view CASCADE;
ALTER TABLE base_table ALTER COLUMN col TYPE NEW_TYPE;
CREATE VIEW dependent_view AS ...;
COMMIT;

-- Strategy 2: Create new, switch, drop old
BEGIN;
CREATE VIEW customer_summary_new AS ...;
DROP VIEW customer_summary;
ALTER VIEW customer_summary_new RENAME TO customer_summary;
COMMIT;
```

## Performance Considerations

### View Performance

```sql
-- Views don't store data - query is executed each time
-- Bad: Complex view in subquery
SELECT * FROM (
    SELECT * FROM complex_aggregation_view
) sub WHERE category = 'Electronics';

-- Good: Push conditions down
CREATE VIEW efficient_view AS
SELECT * FROM large_table;

-- Optimizer can push this predicate down
SELECT * FROM efficient_view WHERE category = 'Electronics';
```

### Indexed Views (Materialized)

```sql
-- Consider materialized views for expensive queries
-- Regular view (always current, potentially slow)
CREATE VIEW real_time_summary AS
SELECT 
    product_id,
    COUNT(*) as review_count,
    AVG(rating) as avg_rating
FROM reviews
GROUP BY product_id;

-- Materialized view (cached results, needs refresh)
CREATE MATERIALIZED VIEW cached_summary AS
SELECT 
    product_id,
    COUNT(*) as review_count,
    AVG(rating) as avg_rating
FROM reviews
GROUP BY product_id;

-- Create indexes on materialized view
CREATE INDEX idx_cached_summary_product ON cached_summary(product_id);
CREATE INDEX idx_cached_summary_rating ON cached_summary(avg_rating);
```

## View Patterns and Best Practices

### API Layer Pattern

```sql
-- Version 1 of API
CREATE SCHEMA api_v1;

CREATE VIEW api_v1.customers AS
SELECT 
    id AS customer_id,
    name AS customer_name,
    email
FROM customers
WHERE is_active = TRUE;

-- Version 2 with backward compatibility
CREATE SCHEMA api_v2;

CREATE VIEW api_v2.customers AS
SELECT 
    id AS customer_id,
    name AS customer_name,
    email,
    phone,  -- New field
    created_at  -- New field
FROM customers
WHERE is_active = TRUE;

-- Applications use versioned schema
-- App v1: SET search_path TO api_v1;
-- App v2: SET search_path TO api_v2;
```

### Denormalization Pattern

```sql
-- Normalized tables
CREATE TABLE authors (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100)
);

CREATE TABLE books (
    id INTEGER PRIMARY KEY,
    title VARCHAR(200),
    author_id INTEGER REFERENCES authors(id)
);

CREATE TABLE book_categories (
    book_id INTEGER REFERENCES books(id),
    category_id INTEGER REFERENCES categories(id)
);

-- Denormalized view for easier querying
CREATE VIEW book_catalog AS
SELECT 
    b.id AS book_id,
    b.title,
    a.name AS author_name,
    STRING_AGG(c.name, ', ' ORDER BY c.name) AS categories
FROM books b
JOIN authors a ON b.author_id = a.id
LEFT JOIN book_categories bc ON b.id = bc.book_id
LEFT JOIN categories c ON bc.category_id = c.id
GROUP BY b.id, b.title, a.name;
```

### Audit View Pattern

```sql
-- Current and historical data
CREATE VIEW customer_history AS
SELECT 
    id,
    name,
    email,
    modified_at,
    modified_by,
    'current' AS status
FROM customers
UNION ALL
SELECT 
    id,
    name,
    email,
    modified_at,
    modified_by,
    'historical' AS status
FROM customers_audit;

-- Point-in-time view
CREATE VIEW customers_as_of AS
SELECT DISTINCT ON (id)
    id,
    name,
    email
FROM customer_history
WHERE modified_at <= current_setting('app.as_of_date')::TIMESTAMP
ORDER BY id, modified_at DESC;
```

## Complex View Examples

### Dashboard View

```sql
CREATE VIEW executive_dashboard AS
WITH 
revenue_metrics AS (
    SELECT 
        DATE_TRUNC('month', order_date) AS month,
        SUM(total) AS revenue,
        COUNT(DISTINCT customer_id) AS unique_customers
    FROM orders
    WHERE status = 'completed'
    GROUP BY DATE_TRUNC('month', order_date)
),
product_metrics AS (
    SELECT 
        DATE_TRUNC('month', o.order_date) AS month,
        p.category_id,
        SUM(oi.quantity) AS units_sold,
        SUM(oi.quantity * oi.price) AS category_revenue
    FROM order_items oi
    JOIN orders o ON oi.order_id = o.id
    JOIN products p ON oi.product_id = p.id
    WHERE o.status = 'completed'
    GROUP BY DATE_TRUNC('month', o.order_date), p.category_id
),
customer_metrics AS (
    SELECT 
        DATE_TRUNC('month', created_at) AS month,
        COUNT(*) AS new_customers
    FROM customers
    GROUP BY DATE_TRUNC('month', created_at)
)
SELECT 
    r.month,
    r.revenue,
    r.unique_customers,
    c.new_customers,
    r.revenue / NULLIF(r.unique_customers, 0) AS avg_customer_value,
    pm.category_revenue AS top_category_revenue
FROM revenue_metrics r
LEFT JOIN customer_metrics c ON r.month = c.month
LEFT JOIN LATERAL (
    SELECT category_revenue 
    FROM product_metrics 
    WHERE month = r.month 
    ORDER BY category_revenue DESC 
    LIMIT 1
) pm ON true
ORDER BY r.month DESC;
```

### Recursive Org Chart View

```sql
CREATE VIEW organization_chart AS
WITH RECURSIVE org_tree AS (
    -- CEO/Top level
    SELECT 
        e.id,
        e.first_name || ' ' || e.last_name AS name,
        e.title,
        e.manager_id,
        0 AS level,
        ARRAY[e.id] AS path,
        e.first_name || ' ' || e.last_name AS hierarchy
    FROM employees e
    WHERE e.manager_id IS NULL
    
    UNION ALL
    
    -- Subordinates
    SELECT 
        e.id,
        e.first_name || ' ' || e.last_name AS name,
        e.title,
        e.manager_id,
        t.level + 1,
        t.path || e.id,
        t.hierarchy || ' > ' || e.first_name || ' ' || e.last_name
    FROM employees e
    JOIN org_tree t ON e.manager_id = t.id
    WHERE NOT e.id = ANY(t.path)  -- Prevent cycles
)
SELECT 
    id,
    REPEAT('  ', level) || name AS indented_name,
    title,
    level,
    hierarchy
FROM org_tree
ORDER BY path;
```

## Implementation Details

**Parser Implementation** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_view`: Handles CREATE/ALTER/RECREATE VIEW
- Captures view name, column list, and SELECT body
- Parses WITH CHECK OPTION (LOCAL/CASCADED)
- Handles OR REPLACE modifier

**AST Structure** (`include/scratchbird/engine/ast.h`):
```cpp
struct DdlViewAst {
    std::string name;
    std::string columns;  // Optional column list
    std::string body;     // AS (SELECT ...)
    bool check_option{false};
    std::string check_type;  // LOCAL|CASCADED
    std::string action;  // CREATE|ALTER|RECREATE
};
```

**Code Anchors**:
- View parser: `src/engine/parser_ddl.cpp` (parse_ddl_view)
- AST definition: `include/scratchbird/engine/ast.h` (DdlViewAst)
- CHECK OPTION parsing: Captures LOCAL/CASCADED modifiers

## See also

- [SELECT Queries](./sql-select.md) - View query definitions
- [Tables](./ddl-tables.md) - Underlying table structures
- [Materialized Views](./ddl-materialized-views.md) - Cached view results
- [Schemas](./ddl-schemas.md) - Organizing views in schemas
- [Roles & Grants](./ddl-roles-users-grants.md) - View permissions