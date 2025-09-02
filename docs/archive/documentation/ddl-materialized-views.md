### DDL: Materialized Views

**What it is**

Materialized views are database objects that store the results of a query physically on disk, unlike regular views which are virtual. They combine the simplicity of views with the performance of tables by pre-computing and storing complex query results. Materialized views can be refreshed periodically to update their data and can have indexes for optimal query performance.

**Why it matters**

- **Performance**: Pre-computed results eliminate repeated expensive calculations
- **Resource Optimization**: Reduce CPU and I/O for complex aggregations
- **Data Warehousing**: Essential for OLAP and reporting workloads
- **Caching**: Store results of slow queries or external data access
- **Consistency**: Provide point-in-time snapshots of data

**How to use it**

Create a materialized view with a SELECT statement, optionally build indexes on it, and refresh it periodically to update the data. Query materialized views like regular tables. Choose between complete refresh (rebuild entirely) or incremental refresh (apply changes only) based on your requirements.

## CREATE MATERIALIZED VIEW

### Basic Syntax

```sql
-- Simple materialized view
CREATE MATERIALIZED VIEW mv_name AS
SELECT columns FROM table
WHERE conditions;

-- With storage parameters
CREATE MATERIALIZED VIEW sales_summary
TABLESPACE fast_ssd
AS
SELECT 
    date_trunc('month', sale_date) AS month,
    product_category,
    SUM(quantity) AS total_quantity,
    SUM(amount) AS total_sales,
    AVG(amount) AS avg_sale
FROM sales
GROUP BY date_trunc('month', sale_date), product_category;

-- With no data (structure only)
CREATE MATERIALIZED VIEW customer_stats
WITH NO DATA
AS
SELECT 
    customer_id,
    COUNT(*) AS order_count,
    SUM(total) AS lifetime_value,
    MAX(order_date) AS last_order
FROM orders
GROUP BY customer_id;
```

### Complex Aggregations

```sql
-- Multi-level aggregation
CREATE MATERIALIZED VIEW regional_performance AS
SELECT 
    r.region_name,
    c.country,
    p.product_line,
    date_trunc('quarter', s.sale_date) AS quarter,
    COUNT(DISTINCT s.customer_id) AS unique_customers,
    COUNT(*) AS transaction_count,
    SUM(s.amount) AS revenue,
    SUM(s.quantity * p.unit_cost) AS cost,
    SUM(s.amount) - SUM(s.quantity * p.unit_cost) AS profit
FROM sales s
JOIN customers c ON s.customer_id = c.id
JOIN regions r ON c.region_id = r.id
JOIN products p ON s.product_id = p.id
WHERE s.sale_date >= '2020-01-01'
GROUP BY r.region_name, c.country, p.product_line, 
         date_trunc('quarter', s.sale_date);

-- Window functions
CREATE MATERIALIZED VIEW customer_rankings AS
SELECT 
    customer_id,
    total_purchases,
    RANK() OVER (ORDER BY total_purchases DESC) AS purchase_rank,
    NTILE(10) OVER (ORDER BY total_purchases DESC) AS decile,
    total_purchases - LAG(total_purchases) 
        OVER (ORDER BY total_purchases DESC) AS gap_to_next
FROM (
    SELECT 
        customer_id,
        SUM(amount) AS total_purchases
    FROM orders
    GROUP BY customer_id
) customer_totals;

-- Recursive CTE
CREATE MATERIALIZED VIEW org_hierarchy AS
WITH RECURSIVE org_tree AS (
    -- Anchor: top-level managers
    SELECT 
        employee_id,
        name,
        manager_id,
        1 AS level,
        name AS path
    FROM employees
    WHERE manager_id IS NULL
    
    UNION ALL
    
    -- Recursive: subordinates
    SELECT 
        e.employee_id,
        e.name,
        e.manager_id,
        ot.level + 1,
        ot.path || ' > ' || e.name
    FROM employees e
    JOIN org_tree ot ON e.manager_id = ot.employee_id
)
SELECT * FROM org_tree;
```

## REFRESH MATERIALIZED VIEW

### Complete Refresh

```sql
-- Basic refresh (locks view during refresh)
REFRESH MATERIALIZED VIEW sales_summary;

-- Concurrent refresh (allows queries during refresh)
REFRESH MATERIALIZED VIEW CONCURRENTLY sales_summary;

-- Conditional refresh
DO $$
BEGIN
    IF (SELECT MAX(last_updated) FROM source_table) > 
       (SELECT MAX(last_refreshed) FROM mv_metadata 
        WHERE mv_name = 'sales_summary') THEN
        REFRESH MATERIALIZED VIEW sales_summary;
    END IF;
END $$;
```

### Incremental Refresh Patterns

```sql
-- Incremental refresh using staging
CREATE MATERIALIZED VIEW daily_sales AS
SELECT 
    sale_date,
    SUM(amount) AS total_sales,
    COUNT(*) AS transaction_count
FROM sales
GROUP BY sale_date;

-- Refresh only new data
CREATE OR REPLACE PROCEDURE refresh_daily_sales_incremental()
AS
BEGIN
    -- Get last processed date
    DECLARE last_date DATE;
    SELECT MAX(sale_date) INTO last_date FROM daily_sales;
    
    -- Insert new aggregated data
    INSERT INTO daily_sales
    SELECT 
        sale_date,
        SUM(amount) AS total_sales,
        COUNT(*) AS transaction_count
    FROM sales
    WHERE sale_date > last_date
    GROUP BY sale_date;
END;
```

### Automated Refresh

```sql
-- Schedule periodic refresh
CREATE OR REPLACE PROCEDURE auto_refresh_materialized_views()
AS
BEGIN
    -- Refresh frequently accessed views
    REFRESH MATERIALIZED VIEW CONCURRENTLY sales_summary;
    REFRESH MATERIALIZED VIEW CONCURRENTLY customer_stats;
    
    -- Log refresh
    INSERT INTO refresh_log (mv_name, refreshed_at)
    VALUES 
        ('sales_summary', CURRENT_TIMESTAMP),
        ('customer_stats', CURRENT_TIMESTAMP);
END;

-- Schedule with cron or scheduler
-- Run every hour: 0 * * * *
CALL auto_refresh_materialized_views();
```

## ALTER MATERIALIZED VIEW

```sql
-- Rename materialized view
ALTER MATERIALIZED VIEW old_name RENAME TO new_name;

-- Change owner
ALTER MATERIALIZED VIEW sales_summary OWNER TO analytics_team;

-- Move to different tablespace
ALTER MATERIALIZED VIEW large_summary SET TABLESPACE archive_storage;

-- Change storage parameters
ALTER MATERIALIZED VIEW sales_summary 
    SET (fillfactor = 90);

-- Add column (requires rebuild)
-- Not directly supported - must recreate

-- Cluster on index
ALTER MATERIALIZED VIEW sales_summary CLUSTER ON idx_month;
```

## DROP MATERIALIZED VIEW

```sql
-- Drop materialized view
DROP MATERIALIZED VIEW sales_summary;

-- Drop if exists
DROP MATERIALIZED VIEW IF EXISTS old_summary;

-- Drop with dependencies
DROP MATERIALIZED VIEW customer_stats CASCADE;

-- Drop multiple views
DROP MATERIALIZED VIEW mv1, mv2, mv3;
```

## Indexing Materialized Views

```sql
-- Create indexes after materialization
CREATE MATERIALIZED VIEW product_sales AS
SELECT 
    product_id,
    product_name,
    SUM(quantity) AS total_sold,
    SUM(amount) AS revenue
FROM sales s
JOIN products p ON s.product_id = p.id
GROUP BY product_id, product_name;

-- Add indexes for query performance
CREATE INDEX idx_product_sales_revenue 
    ON product_sales(revenue DESC);
CREATE INDEX idx_product_sales_product 
    ON product_sales(product_id);

-- Unique index for concurrent refresh
CREATE UNIQUE INDEX idx_product_sales_pk 
    ON product_sales(product_id);
```

## Design Patterns

### Data Mart Pattern

```sql
-- Fact table materialized view
CREATE MATERIALIZED VIEW fact_sales AS
SELECT 
    s.sale_id,
    s.sale_date,
    d.date_key,
    c.customer_key,
    p.product_key,
    l.location_key,
    s.quantity,
    s.amount,
    s.discount,
    s.tax,
    s.amount - s.discount + s.tax AS total
FROM sales s
JOIN dim_date d ON s.sale_date = d.date
JOIN dim_customer c ON s.customer_id = c.customer_id
JOIN dim_product p ON s.product_id = p.product_id
JOIN dim_location l ON s.store_id = l.location_id;

-- Aggregate materialized views
CREATE MATERIALIZED VIEW agg_sales_daily AS
SELECT 
    date_key,
    COUNT(*) AS transaction_count,
    SUM(quantity) AS units_sold,
    SUM(total) AS revenue,
    AVG(total) AS avg_transaction
FROM fact_sales
GROUP BY date_key;

CREATE MATERIALIZED VIEW agg_sales_product AS
SELECT 
    product_key,
    date_trunc('month', sale_date) AS month,
    SUM(quantity) AS units_sold,
    SUM(total) AS revenue
FROM fact_sales
GROUP BY product_key, date_trunc('month', sale_date);
```

### Snapshot Pattern

```sql
-- Daily snapshot
CREATE MATERIALIZED VIEW inventory_snapshot_20240115 AS
SELECT 
    product_id,
    warehouse_id,
    quantity_on_hand,
    quantity_reserved,
    quantity_available,
    last_updated,
    CURRENT_DATE AS snapshot_date
FROM inventory
WHERE CURRENT_DATE = '2024-01-15';

-- Historical snapshots
CREATE TABLE inventory_history AS
SELECT * FROM inventory_snapshot_20240115
UNION ALL
SELECT * FROM inventory_snapshot_20240114
UNION ALL
SELECT * FROM inventory_snapshot_20240113;
```

### Cache Pattern

```sql
-- Cache expensive query
CREATE MATERIALIZED VIEW expensive_report_cache AS
SELECT 
    c.customer_segment,
    p.product_category,
    COUNT(DISTINCT o.customer_id) AS unique_customers,
    COUNT(DISTINCT o.order_id) AS order_count,
    SUM(oi.quantity * oi.unit_price) AS revenue,
    AVG(oi.quantity * oi.unit_price) AS avg_order_value,
    PERCENTILE_CONT(0.5) WITHIN GROUP (ORDER BY oi.quantity * oi.unit_price) AS median_order
FROM orders o
JOIN order_items oi ON o.order_id = oi.order_id
JOIN customers c ON o.customer_id = c.customer_id
JOIN products p ON oi.product_id = p.product_id
WHERE o.order_date >= CURRENT_DATE - INTERVAL '1 year'
GROUP BY c.customer_segment, p.product_category;

-- Query cache instead of base tables
SELECT * FROM expensive_report_cache
WHERE customer_segment = 'Premium';
```

## Performance Optimization

### Query Rewrite

```sql
-- Original slow query
SELECT 
    product_category,
    SUM(amount) AS total_sales
FROM sales s
JOIN products p ON s.product_id = p.id
WHERE s.sale_date >= '2024-01-01'
GROUP BY product_category;

-- Rewritten to use materialized view
SELECT 
    product_category,
    SUM(total_sales) AS total_sales
FROM sales_summary
WHERE month >= '2024-01-01'
GROUP BY product_category;
```

### Refresh Strategies

```sql
-- Off-peak refresh
CREATE PROCEDURE refresh_during_maintenance()
AS
BEGIN
    -- Check if in maintenance window
    IF EXTRACT(HOUR FROM CURRENT_TIME) BETWEEN 2 AND 4 THEN
        REFRESH MATERIALIZED VIEW CONCURRENTLY large_summary;
    END IF;
END;

-- Incremental refresh with logging
CREATE PROCEDURE smart_refresh(mv_name TEXT)
AS
DECLARE
    start_time TIMESTAMP;
    row_count BIGINT;
BEGIN
    start_time := CURRENT_TIMESTAMP;
    
    EXECUTE 'REFRESH MATERIALIZED VIEW CONCURRENTLY ' || mv_name;
    
    GET DIAGNOSTICS row_count = ROW_COUNT;
    
    INSERT INTO refresh_history (
        mv_name, 
        refresh_start, 
        refresh_end, 
        rows_affected
    ) VALUES (
        mv_name,
        start_time,
        CURRENT_TIMESTAMP,
        row_count
    );
END;
```

## Monitoring

```sql
-- Track materialized view usage
CREATE VIEW mv_statistics AS
SELECT 
    schemaname,
    matviewname,
    pg_size_pretty(pg_total_relation_size(schemaname||'.'||matviewname)) AS size,
    n_tup_ins AS rows_inserted,
    n_tup_upd AS rows_updated,
    n_tup_del AS rows_deleted,
    last_vacuum,
    last_analyze
FROM pg_stat_user_tables
WHERE schemaname || '.' || tablename IN (
    SELECT schemaname || '.' || matviewname 
    FROM pg_matviews
);

-- Check refresh status
CREATE TABLE mv_refresh_status (
    mv_name TEXT PRIMARY KEY,
    last_refresh TIMESTAMP,
    refresh_duration INTERVAL,
    status TEXT
);

-- Monitor refresh performance
SELECT 
    mv_name,
    last_refresh,
    refresh_duration,
    CASE 
        WHEN last_refresh < CURRENT_TIMESTAMP - INTERVAL '1 day' 
        THEN 'STALE'
        ELSE 'FRESH'
    END AS freshness
FROM mv_refresh_status
ORDER BY last_refresh;
```

## Best Practices

1. **Choose Appropriate Refresh Strategy**
   - Complete refresh for small datasets
   - Incremental for large, append-only data
   - Concurrent for high-availability requirements

2. **Index Strategically**
   - Create indexes based on query patterns
   - Include unique index for concurrent refresh
   - Balance index maintenance cost

3. **Monitor Staleness**
   - Track last refresh time
   - Alert on stale data
   - Document refresh schedules

4. **Storage Considerations**
   - Use appropriate tablespace
   - Monitor space usage
   - Plan for growth

5. **Query Optimization**
   - Ensure optimizer uses materialized views
   - Update statistics regularly
   - Test performance improvements

## Implementation Details

**Parser** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_materialized_view`: CREATE MATERIALIZED VIEW
- Handles WITH clause options
- Parses refresh specifications

**Storage** (`src/engine/storage_manager.cpp`):
- Physical storage management
- Refresh execution
- Concurrent refresh locking

**Optimizer**:
- Query rewrite rules
- Cost-based selection
- Freshness checking

**Code Anchors**:
- Materialized view parser: `src/engine/parser_ddl.cpp` (parse_ddl_materialized_view)
- Refresh logic: `src/engine/mv_refresh.cpp`
- Storage management: `src/engine/storage_manager.cpp`
- AST definitions: `include/scratchbird/engine/ast.h`

## See also

- [Views](./ddl-views.md) - Regular views
- [Tables](./ddl-tables.md) - Physical storage
- [Indexes](./ddl-indexes.md) - Indexing strategies
- [SELECT](./sql-select.md) - Query construction
- [Performance](./explain-analyze.md) - Query optimization