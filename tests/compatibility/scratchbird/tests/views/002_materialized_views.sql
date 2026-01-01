-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Views - Materialized Views
-- Description: Comprehensive materialized view testing
-- ============================================================================

-- Materialized views store query results physically
-- Must be refreshed to update data
-- Provide better performance than regular views for expensive queries
-- Consume storage space
-- Can have indexes created on them

-- Create test database
CREATE DATABASE test_materialized_views_db;
USE test_materialized_views_db;

-- ============================================================================
-- Section 1: Basic Materialized View
-- ============================================================================

CREATE TABLE test_sales (
    sale_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    quantity INT,
    unit_price NUMERIC(10,2),
    sale_date DATE
);

INSERT INTO test_sales (product_name, quantity, unit_price, sale_date)
SELECT
    'Product ' || (i % 10 + 1),
    (RANDOM() * 100)::INT + 1,
    (RANDOM() * 100 + 10)::NUMERIC(10,2),
    CURRENT_DATE - (RANDOM() * 365)::INT
FROM generate_series(1, 1000) i;

-- Create materialized view
CREATE MATERIALIZED VIEW test_sales_summary AS
SELECT
    product_name,
    COUNT(*) AS sale_count,
    SUM(quantity) AS total_quantity,
    AVG(unit_price) AS avg_price,
    SUM(quantity * unit_price) AS total_revenue
FROM test_sales
GROUP BY product_name;

-- Query the materialized view
SELECT product_name, sale_count, total_quantity, total_revenue FROM test_sales_summary ORDER BY total_revenue DESC LIMIT 5;

-- ============================================================================
-- Section 2: Refreshing Materialized Views
-- ============================================================================

-- Add more sales data
INSERT INTO test_sales (product_name, quantity, unit_price, sale_date) VALUES
    ('Product 1', 50, 25.00, CURRENT_DATE);

-- Materialized view still shows old data
SELECT product_name, sale_count FROM test_sales_summary WHERE product_name = 'Product 1';

-- Refresh the materialized view
REFRESH MATERIALIZED VIEW test_sales_summary;

-- Now shows updated data
SELECT product_name, sale_count FROM test_sales_summary WHERE product_name = 'Product 1';

-- ============================================================================
-- Section 3: CONCURRENTLY Refresh
-- ============================================================================

CREATE MATERIALIZED VIEW test_daily_sales AS
SELECT
    sale_date,
    COUNT(*) AS num_sales,
    SUM(quantity * unit_price) AS daily_revenue
FROM test_sales
GROUP BY sale_date;

-- Create unique index (required for CONCURRENTLY refresh)
CREATE UNIQUE INDEX idx_daily_sales_date ON test_daily_sales(sale_date);

-- Refresh concurrently (doesn't lock the view)
REFRESH MATERIALIZED VIEW CONCURRENTLY test_daily_sales;

SELECT sale_date, num_sales, daily_revenue FROM test_daily_sales ORDER BY sale_date DESC LIMIT 5;

-- ============================================================================
-- Section 4: Materialized View with JOIN
-- ============================================================================

CREATE TABLE test_customers (
    customer_id SERIAL PRIMARY KEY,
    customer_name VARCHAR(200),
    city VARCHAR(100),
    country VARCHAR(100)
);

CREATE TABLE test_orders (
    order_id SERIAL PRIMARY KEY,
    customer_id INT REFERENCES test_customers(customer_id),
    order_date DATE,
    total_amount NUMERIC(12,2)
);

INSERT INTO test_customers (customer_name, city, country) VALUES
    ('Customer A', 'New York', 'USA'),
    ('Customer B', 'London', 'UK'),
    ('Customer C', 'Tokyo', 'Japan'),
    ('Customer D', 'Paris', 'France');

INSERT INTO test_orders (customer_id, order_date, total_amount)
SELECT
    (RANDOM() * 3 + 1)::INT,
    CURRENT_DATE - (RANDOM() * 180)::INT,
    (RANDOM() * 5000 + 100)::NUMERIC(12,2)
FROM generate_series(1, 500) i;

CREATE MATERIALIZED VIEW test_customer_order_summary AS
SELECT
    c.customer_id,
    c.customer_name,
    c.city,
    c.country,
    COUNT(o.order_id) AS order_count,
    SUM(o.total_amount) AS total_spent,
    AVG(o.total_amount) AS avg_order_value,
    MAX(o.order_date) AS last_order_date
FROM test_customers c
LEFT JOIN test_orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.customer_name, c.city, c.country;

SELECT customer_name, order_count, total_spent, avg_order_value FROM test_customer_order_summary ORDER BY total_spent DESC;

-- ============================================================================
-- Section 5: Indexes on Materialized Views
-- ============================================================================

-- Create indexes on materialized view for better query performance
CREATE INDEX idx_mv_customer_country ON test_customer_order_summary(country);
CREATE INDEX idx_mv_customer_total_spent ON test_customer_order_summary(total_spent DESC);

-- Query using index
SELECT customer_name, total_spent FROM test_customer_order_summary WHERE country = 'USA';

-- ============================================================================
-- Section 6: Materialized View with Window Functions
-- ============================================================================

CREATE MATERIALIZED VIEW test_product_rankings AS
SELECT
    product_name,
    SUM(quantity * unit_price) AS total_revenue,
    RANK() OVER (ORDER BY SUM(quantity * unit_price) DESC) AS revenue_rank,
    PERCENT_RANK() OVER (ORDER BY SUM(quantity * unit_price) DESC) AS revenue_percentile
FROM test_sales
GROUP BY product_name;

SELECT product_name, total_revenue, revenue_rank, revenue_percentile FROM test_product_rankings ORDER BY revenue_rank;

-- ============================================================================
-- Section 7: Materialized View with Complex Aggregations
-- ============================================================================

CREATE TABLE test_web_events (
    event_id SERIAL PRIMARY KEY,
    user_id INT,
    event_type VARCHAR(50),
    event_date DATE,
    event_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    page_url TEXT
);

INSERT INTO test_web_events (user_id, event_type, event_date, page_url)
SELECT
    (RANDOM() * 100 + 1)::INT,
    CASE (RANDOM() * 4)::INT
        WHEN 0 THEN 'page_view'
        WHEN 1 THEN 'click'
        WHEN 2 THEN 'form_submit'
        ELSE 'download'
    END,
    CURRENT_DATE - (RANDOM() * 30)::INT,
    '/page/' || (RANDOM() * 20 + 1)::INT
FROM generate_series(1, 5000) i;

CREATE MATERIALIZED VIEW test_user_engagement_metrics AS
SELECT
    user_id,
    COUNT(*) AS total_events,
    COUNT(DISTINCT event_date) AS active_days,
    COUNT(CASE WHEN event_type = 'page_view' THEN 1 END) AS page_views,
    COUNT(CASE WHEN event_type = 'click' THEN 1 END) AS clicks,
    COUNT(CASE WHEN event_type = 'form_submit' THEN 1 END) AS form_submits,
    COUNT(CASE WHEN event_type = 'download' THEN 1 END) AS downloads,
    MIN(event_date) AS first_activity,
    MAX(event_date) AS last_activity
FROM test_web_events
GROUP BY user_id;

SELECT user_id, total_events, active_days, page_views FROM test_user_engagement_metrics ORDER BY total_events DESC LIMIT 10;

-- ============================================================================
-- Section 8: Materialized View for Data Warehouse
-- ============================================================================

CREATE TABLE test_fact_sales (
    sale_id SERIAL PRIMARY KEY,
    date_key INT,
    product_key INT,
    customer_key INT,
    quantity INT,
    amount NUMERIC(12,2)
);

CREATE TABLE test_dim_date (
    date_key INT PRIMARY KEY,
    full_date DATE,
    year INT,
    quarter INT,
    month INT,
    week INT,
    day_of_week INT
);

-- Generate date dimension
INSERT INTO test_dim_date (date_key, full_date, year, quarter, month, week, day_of_week)
SELECT
    TO_CHAR(d, 'YYYYMMDD')::INT,
    d,
    EXTRACT(YEAR FROM d)::INT,
    EXTRACT(QUARTER FROM d)::INT,
    EXTRACT(MONTH FROM d)::INT,
    EXTRACT(WEEK FROM d)::INT,
    EXTRACT(DOW FROM d)::INT
FROM generate_series('2024-01-01'::DATE, '2024-12-31'::DATE, '1 day'::INTERVAL) d;

-- Generate fact data
INSERT INTO test_fact_sales (date_key, product_key, customer_key, quantity, amount)
SELECT
    TO_CHAR(CURRENT_DATE - (RANDOM() * 365)::INT, 'YYYYMMDD')::INT,
    (RANDOM() * 100 + 1)::INT,
    (RANDOM() * 1000 + 1)::INT,
    (RANDOM() * 50 + 1)::INT,
    (RANDOM() * 1000 + 10)::NUMERIC(12,2)
FROM generate_series(1, 10000) i;

CREATE MATERIALIZED VIEW test_sales_by_month AS
SELECT
    d.year,
    d.month,
    COUNT(f.sale_id) AS sale_count,
    SUM(f.quantity) AS total_quantity,
    SUM(f.amount) AS total_revenue,
    AVG(f.amount) AS avg_sale_amount
FROM test_fact_sales f
JOIN test_dim_date d ON f.date_key = d.date_key
GROUP BY d.year, d.month;

SELECT year, month, sale_count, total_revenue FROM test_sales_by_month ORDER BY year, month;

-- ============================================================================
-- Section 9: Materialized View with DISTINCT
-- ============================================================================

CREATE MATERIALIZED VIEW test_unique_products AS
SELECT DISTINCT product_name
FROM test_sales
ORDER BY product_name;

SELECT product_name FROM test_unique_products;

-- ============================================================================
-- Section 10: Materialized View Dependencies
-- ============================================================================

-- Create a view on top of a materialized view
CREATE VIEW test_top_products AS
SELECT product_name, total_revenue, revenue_rank
FROM test_product_rankings
WHERE revenue_rank <= 5;

SELECT product_name, total_revenue, revenue_rank FROM test_top_products;

-- ============================================================================
-- Section 11: Refresh Strategy Testing
-- ============================================================================

CREATE TABLE test_refresh_log (
    refresh_id SERIAL PRIMARY KEY,
    view_name VARCHAR(200),
    refresh_type VARCHAR(50),
    rows_before INT,
    rows_after INT,
    refresh_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Function to log refresh
CREATE FUNCTION log_mv_refresh(view_name TEXT, refresh_type TEXT) RETURNS VOID AS $$
DECLARE
    row_count INT;
BEGIN
    EXECUTE format('SELECT COUNT(*) FROM %I', view_name) INTO row_count;

    INSERT INTO test_refresh_log (view_name, refresh_type, rows_after)
    VALUES (view_name, refresh_type, row_count);
END;
$$ LANGUAGE plpgsql;

-- Refresh and log
REFRESH MATERIALIZED VIEW test_sales_summary;
PERFORM log_mv_refresh('test_sales_summary', 'full');

SELECT refresh_id, view_name, refresh_type, rows_after FROM test_refresh_log ORDER BY refresh_id;

-- ============================================================================
-- Section 12: Materialized View with UNION
-- ============================================================================

CREATE TABLE test_online_sales (
    sale_id SERIAL PRIMARY KEY,
    product VARCHAR(200),
    amount NUMERIC(12,2),
    sale_date DATE
);

CREATE TABLE test_retail_sales (
    sale_id SERIAL PRIMARY KEY,
    product VARCHAR(200),
    amount NUMERIC(12,2),
    sale_date DATE
);

INSERT INTO test_online_sales (product, amount, sale_date)
SELECT 'Product ' || i, (RANDOM() * 500 + 50)::NUMERIC(12,2), CURRENT_DATE - (RANDOM() * 90)::INT
FROM generate_series(1, 200) i;

INSERT INTO test_retail_sales (product, amount, sale_date)
SELECT 'Product ' || i, (RANDOM() * 300 + 30)::NUMERIC(12,2), CURRENT_DATE - (RANDOM() * 90)::INT
FROM generate_series(1, 150) i;

CREATE MATERIALIZED VIEW test_all_sales_summary AS
SELECT
    'Online' AS channel,
    COUNT(*) AS sale_count,
    SUM(amount) AS total_revenue
FROM test_online_sales
UNION ALL
SELECT
    'Retail' AS channel,
    COUNT(*) AS sale_count,
    SUM(amount) AS total_revenue
FROM test_retail_sales;

SELECT channel, sale_count, total_revenue FROM test_all_sales_summary;

-- ============================================================================
-- Section 13: Materialized View Performance Comparison
-- ============================================================================

-- Regular view for comparison
CREATE VIEW test_regular_customer_summary AS
SELECT
    c.customer_id,
    c.customer_name,
    COUNT(o.order_id) AS order_count,
    SUM(o.total_amount) AS total_spent
FROM test_customers c
LEFT JOIN test_orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.customer_name;

-- The materialized view test_customer_order_summary already exists

-- Query both (materialized should be faster on large datasets)
SELECT customer_name, order_count FROM test_regular_customer_summary ORDER BY order_count DESC LIMIT 3;
SELECT customer_name, order_count FROM test_customer_order_summary ORDER BY order_count DESC LIMIT 3;

-- ============================================================================
-- Section 14: DROP MATERIALIZED VIEW
-- ============================================================================

CREATE MATERIALIZED VIEW test_temp_mv AS
SELECT product_name, SUM(quantity) AS total_qty FROM test_sales GROUP BY product_name;

-- Verify it exists
SELECT matviewname FROM pg_matviews WHERE matviewname = 'test_temp_mv';

-- Drop it
DROP MATERIALIZED VIEW test_temp_mv;

-- Verify it's gone
SELECT matviewname FROM pg_matviews WHERE matviewname = 'test_temp_mv';

-- ============================================================================
-- Section 15: Materialized View Metadata
-- ============================================================================

-- Query materialized view information
SELECT
    schemaname,
    matviewname,
    matviewowner,
    ispopulated
FROM pg_matviews
WHERE schemaname = 'public'
ORDER BY matviewname
LIMIT 5;

-- ============================================================================
-- Section 16: Best Practices
-- ============================================================================

CREATE TABLE test_mv_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_mv_best_practices VALUES
    (1, 'Use materialized views for expensive queries that change infrequently'),
    (2, 'Materialized views store data physically and consume storage'),
    (3, 'Must explicitly REFRESH to update data'),
    (4, 'Use REFRESH CONCURRENTLY to avoid locking (requires unique index)'),
    (5, 'Create indexes on materialized views for better query performance'),
    (6, 'Consider refresh frequency based on data freshness requirements'),
    (7, 'Monitor storage space consumed by materialized views'),
    (8, 'Use for pre-computed aggregations in data warehouses'),
    (9, 'Can significantly improve query performance vs regular views'),
    (10, 'Schedule refreshes during off-peak hours for large MVs'),
    (11, 'Test refresh time to ensure it fits within maintenance window'),
    (12, 'Consider incremental refresh strategies for very large datasets'),
    (13, 'Document refresh schedule and dependencies'),
    (14, 'Drop unused materialized views to free storage'),
    (15, 'Monitor query performance before and after implementing MVs');

SELECT id, guideline FROM test_mv_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_mv_best_practices;
DROP VIEW test_regular_customer_summary;
DROP FUNCTION log_mv_refresh(TEXT, TEXT);
DROP TABLE test_refresh_log;
DROP VIEW test_top_products;
DROP MATERIALIZED VIEW test_unique_products;
DROP MATERIALIZED VIEW test_sales_by_month;
DROP TABLE test_dim_date;
DROP TABLE test_fact_sales;
DROP MATERIALIZED VIEW test_user_engagement_metrics;
DROP TABLE test_web_events;
DROP MATERIALIZED VIEW test_product_rankings;
DROP MATERIALIZED VIEW test_customer_order_summary;
DROP TABLE test_orders;
DROP TABLE test_customers;
DROP MATERIALIZED VIEW test_daily_sales;
DROP MATERIALIZED VIEW test_all_sales_summary;
DROP TABLE test_retail_sales;
DROP TABLE test_online_sales;
DROP MATERIALIZED VIEW test_sales_summary;
DROP TABLE test_sales;

DROP DATABASE test_materialized_views_db;

-- End of materialized view tests
