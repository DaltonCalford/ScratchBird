# Tutorial: Data Warehouse Setup

Build a data warehouse for analytics workloads.

[Back to Getting Started](../index.md) | [Back to Documentation Index](../../index.md)

---

## What You'll Build

A data warehouse schema with:
- Dimensional model (star schema)
- Fact and dimension tables
- Aggregation tables
- Analytical queries

---

## Prerequisites

- ScratchBird installed and running
- Basic SQL knowledge
- Understanding of data warehouse concepts

---

## Setup

```bash
sb_isql -H localhost -P 3092 -U admin

sb_isql> CREATE DATABASE analytics;
sb_isql> \c analytics
```

---

## Step 1: Dimension Tables

### Date Dimension

```sql
CREATE TABLE dim_date (
    date_key INTEGER PRIMARY KEY,
    full_date DATE UNIQUE NOT NULL,
    year INTEGER NOT NULL,
    quarter INTEGER NOT NULL,
    month INTEGER NOT NULL,
    month_name VARCHAR(10) NOT NULL,
    week INTEGER NOT NULL,
    day_of_month INTEGER NOT NULL,
    day_of_week INTEGER NOT NULL,
    day_name VARCHAR(10) NOT NULL,
    is_weekend BOOLEAN NOT NULL,
    is_holiday BOOLEAN DEFAULT FALSE,
    fiscal_year INTEGER,
    fiscal_quarter INTEGER
);

-- Populate date dimension (5 years)
INSERT INTO dim_date
SELECT
    TO_CHAR(d, 'YYYYMMDD')::INTEGER AS date_key,
    d AS full_date,
    EXTRACT(YEAR FROM d) AS year,
    EXTRACT(QUARTER FROM d) AS quarter,
    EXTRACT(MONTH FROM d) AS month,
    TO_CHAR(d, 'Month') AS month_name,
    EXTRACT(WEEK FROM d) AS week,
    EXTRACT(DAY FROM d) AS day_of_month,
    EXTRACT(DOW FROM d) AS day_of_week,
    TO_CHAR(d, 'Day') AS day_name,
    EXTRACT(DOW FROM d) IN (0, 6) AS is_weekend,
    FALSE AS is_holiday,
    CASE WHEN EXTRACT(MONTH FROM d) >= 7
         THEN EXTRACT(YEAR FROM d) + 1
         ELSE EXTRACT(YEAR FROM d)
    END AS fiscal_year,
    CASE
        WHEN EXTRACT(MONTH FROM d) IN (7,8,9) THEN 1
        WHEN EXTRACT(MONTH FROM d) IN (10,11,12) THEN 2
        WHEN EXTRACT(MONTH FROM d) IN (1,2,3) THEN 3
        ELSE 4
    END AS fiscal_quarter
FROM generate_series('2020-01-01'::DATE, '2024-12-31'::DATE, '1 day'::INTERVAL) d;

CREATE INDEX idx_dim_date_full ON dim_date(full_date);
CREATE INDEX idx_dim_date_year_month ON dim_date(year, month);
```

### Customer Dimension

```sql
CREATE TABLE dim_customer (
    customer_key SERIAL PRIMARY KEY,
    customer_id VARCHAR(50) UNIQUE NOT NULL,  -- Natural key
    name VARCHAR(200) NOT NULL,
    email VARCHAR(255),
    segment VARCHAR(50),
    region VARCHAR(50),
    country VARCHAR(50),
    city VARCHAR(100),
    acquisition_date DATE,
    is_active BOOLEAN DEFAULT TRUE,
    -- SCD Type 2 fields
    effective_date DATE NOT NULL,
    expiry_date DATE DEFAULT '9999-12-31',
    is_current BOOLEAN DEFAULT TRUE
);

CREATE INDEX idx_dim_customer_id ON dim_customer(customer_id);
CREATE INDEX idx_dim_customer_current ON dim_customer(is_current) WHERE is_current = TRUE;
CREATE INDEX idx_dim_customer_segment ON dim_customer(segment);
```

### Product Dimension

```sql
CREATE TABLE dim_product (
    product_key SERIAL PRIMARY KEY,
    product_id VARCHAR(50) UNIQUE NOT NULL,
    sku VARCHAR(50) NOT NULL,
    name VARCHAR(200) NOT NULL,
    category VARCHAR(100),
    subcategory VARCHAR(100),
    brand VARCHAR(100),
    supplier VARCHAR(200),
    unit_cost DECIMAL(10,2),
    unit_price DECIMAL(10,2),
    is_active BOOLEAN DEFAULT TRUE,
    effective_date DATE NOT NULL,
    expiry_date DATE DEFAULT '9999-12-31',
    is_current BOOLEAN DEFAULT TRUE
);

CREATE INDEX idx_dim_product_id ON dim_product(product_id);
CREATE INDEX idx_dim_product_category ON dim_product(category);
CREATE INDEX idx_dim_product_brand ON dim_product(brand);
```

### Store/Location Dimension

```sql
CREATE TABLE dim_store (
    store_key SERIAL PRIMARY KEY,
    store_id VARCHAR(20) UNIQUE NOT NULL,
    store_name VARCHAR(100) NOT NULL,
    store_type VARCHAR(50),
    region VARCHAR(50),
    district VARCHAR(50),
    state VARCHAR(50),
    country VARCHAR(50),
    postal_code VARCHAR(20),
    latitude DECIMAL(10,6),
    longitude DECIMAL(10,6),
    open_date DATE,
    close_date DATE,
    is_active BOOLEAN DEFAULT TRUE
);

CREATE INDEX idx_dim_store_region ON dim_store(region);
CREATE INDEX idx_dim_store_type ON dim_store(store_type);
```

---

## Step 2: Fact Tables

### Sales Fact Table

```sql
CREATE TABLE fact_sales (
    sale_id BIGSERIAL PRIMARY KEY,
    date_key INTEGER NOT NULL REFERENCES dim_date(date_key),
    customer_key INTEGER REFERENCES dim_customer(customer_key),
    product_key INTEGER NOT NULL REFERENCES dim_product(product_key),
    store_key INTEGER REFERENCES dim_store(store_key),
    order_id VARCHAR(50),
    line_item INTEGER,
    quantity INTEGER NOT NULL,
    unit_price DECIMAL(10,2) NOT NULL,
    discount_amount DECIMAL(10,2) DEFAULT 0,
    tax_amount DECIMAL(10,2) DEFAULT 0,
    total_amount DECIMAL(10,2) NOT NULL,
    cost_amount DECIMAL(10,2),
    profit_amount DECIMAL(10,2)
);

-- Indexes for common query patterns
CREATE INDEX idx_fact_sales_date ON fact_sales(date_key);
CREATE INDEX idx_fact_sales_customer ON fact_sales(customer_key);
CREATE INDEX idx_fact_sales_product ON fact_sales(product_key);
CREATE INDEX idx_fact_sales_store ON fact_sales(store_key);

-- Composite indexes for common joins
CREATE INDEX idx_fact_sales_date_product ON fact_sales(date_key, product_key);
CREATE INDEX idx_fact_sales_date_store ON fact_sales(date_key, store_key);
```

### Inventory Fact Table (Snapshot)

```sql
CREATE TABLE fact_inventory (
    inventory_id BIGSERIAL PRIMARY KEY,
    date_key INTEGER NOT NULL REFERENCES dim_date(date_key),
    product_key INTEGER NOT NULL REFERENCES dim_product(product_key),
    store_key INTEGER NOT NULL REFERENCES dim_store(store_key),
    quantity_on_hand INTEGER NOT NULL,
    quantity_on_order INTEGER DEFAULT 0,
    quantity_reserved INTEGER DEFAULT 0,
    inventory_value DECIMAL(12,2),
    days_in_stock INTEGER
);

CREATE INDEX idx_fact_inventory_date ON fact_inventory(date_key);
CREATE INDEX idx_fact_inventory_product ON fact_inventory(product_key);
CREATE INDEX idx_fact_inventory_store ON fact_inventory(store_key);
```

---

## Step 3: Aggregation Tables

### Monthly Sales Summary

```sql
CREATE TABLE agg_sales_monthly (
    year INTEGER NOT NULL,
    month INTEGER NOT NULL,
    product_key INTEGER REFERENCES dim_product(product_key),
    store_key INTEGER REFERENCES dim_store(store_key),
    total_quantity INTEGER NOT NULL,
    total_revenue DECIMAL(14,2) NOT NULL,
    total_cost DECIMAL(14,2),
    total_profit DECIMAL(14,2),
    order_count INTEGER NOT NULL,
    avg_order_value DECIMAL(10,2),
    PRIMARY KEY (year, month, product_key, store_key)
);

CREATE INDEX idx_agg_monthly_product ON agg_sales_monthly(product_key);
CREATE INDEX idx_agg_monthly_store ON agg_sales_monthly(store_key);
```

### Customer Lifetime Value

```sql
CREATE TABLE agg_customer_ltv (
    customer_key INTEGER PRIMARY KEY REFERENCES dim_customer(customer_key),
    first_purchase_date DATE,
    last_purchase_date DATE,
    purchase_count INTEGER NOT NULL DEFAULT 0,
    total_revenue DECIMAL(14,2) NOT NULL DEFAULT 0,
    avg_order_value DECIMAL(10,2),
    days_since_last_purchase INTEGER,
    customer_tenure_days INTEGER,
    ltv_segment VARCHAR(20)
);
```

### Refresh Aggregations

```sql
-- Procedure to refresh monthly aggregations
CREATE OR REPLACE PROCEDURE refresh_monthly_agg(p_year INTEGER, p_month INTEGER)
LANGUAGE plpgsql
AS $$
BEGIN
    DELETE FROM agg_sales_monthly
    WHERE year = p_year AND month = p_month;

    INSERT INTO agg_sales_monthly
    SELECT
        d.year,
        d.month,
        f.product_key,
        f.store_key,
        SUM(f.quantity) AS total_quantity,
        SUM(f.total_amount) AS total_revenue,
        SUM(f.cost_amount) AS total_cost,
        SUM(f.profit_amount) AS total_profit,
        COUNT(DISTINCT f.order_id) AS order_count,
        AVG(f.total_amount) AS avg_order_value
    FROM fact_sales f
    JOIN dim_date d ON f.date_key = d.date_key
    WHERE d.year = p_year AND d.month = p_month
    GROUP BY d.year, d.month, f.product_key, f.store_key;
END;
$$;
```

---

## Step 4: Sample Data

```sql
-- Sample customers
INSERT INTO dim_customer (customer_id, name, email, segment, region, country, city, acquisition_date, effective_date)
VALUES
    ('CUST001', 'Acme Corp', 'orders@acme.com', 'Enterprise', 'North', 'USA', 'New York', '2021-01-15', '2021-01-15'),
    ('CUST002', 'Beta Inc', 'purchasing@beta.com', 'Mid-Market', 'South', 'USA', 'Atlanta', '2021-03-20', '2021-03-20'),
    ('CUST003', 'Gamma LLC', 'buy@gamma.com', 'SMB', 'West', 'USA', 'Denver', '2021-06-10', '2021-06-10');

-- Sample products
INSERT INTO dim_product (product_id, sku, name, category, subcategory, brand, unit_cost, unit_price, effective_date)
VALUES
    ('PROD001', 'LAPTOP-001', 'Business Laptop', 'Electronics', 'Computers', 'TechBrand', 600.00, 999.99, '2021-01-01'),
    ('PROD002', 'DESK-001', 'Standing Desk', 'Furniture', 'Desks', 'OfficePro', 200.00, 449.99, '2021-01-01'),
    ('PROD003', 'CHAIR-001', 'Ergonomic Chair', 'Furniture', 'Seating', 'ComfortPlus', 150.00, 349.99, '2021-01-01');

-- Sample stores
INSERT INTO dim_store (store_id, store_name, store_type, region, state, country, open_date)
VALUES
    ('STORE01', 'Downtown Flagship', 'Flagship', 'Northeast', 'NY', 'USA', '2015-01-01'),
    ('STORE02', 'Mall Location', 'Mall', 'Southeast', 'GA', 'USA', '2018-06-15'),
    ('STORE03', 'Online', 'E-commerce', 'National', NULL, 'USA', '2010-01-01');
```

---

## Step 5: Analytical Queries

### Sales by Time Period

```sql
-- Daily sales trend
SELECT
    d.full_date,
    SUM(f.total_amount) AS revenue,
    COUNT(DISTINCT f.order_id) AS orders
FROM fact_sales f
JOIN dim_date d ON f.date_key = d.date_key
WHERE d.year = 2024
GROUP BY d.full_date
ORDER BY d.full_date;

-- Monthly comparison YoY
SELECT
    d.month,
    d.month_name,
    SUM(CASE WHEN d.year = 2024 THEN f.total_amount ELSE 0 END) AS revenue_2024,
    SUM(CASE WHEN d.year = 2023 THEN f.total_amount ELSE 0 END) AS revenue_2023,
    ROUND(
        (SUM(CASE WHEN d.year = 2024 THEN f.total_amount ELSE 0 END) -
         SUM(CASE WHEN d.year = 2023 THEN f.total_amount ELSE 0 END)) /
        NULLIF(SUM(CASE WHEN d.year = 2023 THEN f.total_amount ELSE 0 END), 0) * 100,
    2) AS yoy_growth_pct
FROM fact_sales f
JOIN dim_date d ON f.date_key = d.date_key
WHERE d.year IN (2023, 2024)
GROUP BY d.month, d.month_name
ORDER BY d.month;
```

### Product Performance

```sql
-- Top products by revenue
SELECT
    p.name,
    p.category,
    SUM(f.quantity) AS units_sold,
    SUM(f.total_amount) AS revenue,
    SUM(f.profit_amount) AS profit,
    ROUND(SUM(f.profit_amount) / NULLIF(SUM(f.total_amount), 0) * 100, 2) AS margin_pct
FROM fact_sales f
JOIN dim_product p ON f.product_key = p.product_key
JOIN dim_date d ON f.date_key = d.date_key
WHERE d.year = 2024 AND p.is_current = TRUE
GROUP BY p.product_key, p.name, p.category
ORDER BY revenue DESC
LIMIT 20;
```

### Customer Segmentation

```sql
-- RFM Analysis (Recency, Frequency, Monetary)
WITH customer_metrics AS (
    SELECT
        c.customer_key,
        c.name,
        c.segment,
        MAX(d.full_date) AS last_purchase,
        COUNT(DISTINCT f.order_id) AS frequency,
        SUM(f.total_amount) AS monetary
    FROM fact_sales f
    JOIN dim_customer c ON f.customer_key = c.customer_key
    JOIN dim_date d ON f.date_key = d.date_key
    WHERE c.is_current = TRUE
    GROUP BY c.customer_key, c.name, c.segment
)
SELECT
    customer_key,
    name,
    segment,
    last_purchase,
    CURRENT_DATE - last_purchase AS recency_days,
    frequency,
    monetary,
    CASE
        WHEN monetary > 10000 AND frequency > 10 THEN 'Champion'
        WHEN monetary > 5000 THEN 'Loyal'
        WHEN CURRENT_DATE - last_purchase > 180 THEN 'At Risk'
        ELSE 'Regular'
    END AS rfm_segment
FROM customer_metrics
ORDER BY monetary DESC;
```

### Store Performance

```sql
-- Store comparison
SELECT
    s.store_name,
    s.store_type,
    s.region,
    COUNT(DISTINCT f.order_id) AS orders,
    SUM(f.total_amount) AS revenue,
    ROUND(AVG(f.total_amount), 2) AS avg_transaction,
    SUM(f.profit_amount) AS profit
FROM fact_sales f
JOIN dim_store s ON f.store_key = s.store_key
JOIN dim_date d ON f.date_key = d.date_key
WHERE d.year = 2024
GROUP BY s.store_key, s.store_name, s.store_type, s.region
ORDER BY revenue DESC;
```

---

## Step 6: Window Functions for Analytics

```sql
-- Running totals
SELECT
    d.full_date,
    SUM(f.total_amount) AS daily_revenue,
    SUM(SUM(f.total_amount)) OVER (
        ORDER BY d.full_date
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) AS cumulative_revenue
FROM fact_sales f
JOIN dim_date d ON f.date_key = d.date_key
WHERE d.year = 2024 AND d.month = 1
GROUP BY d.full_date
ORDER BY d.full_date;

-- Moving average
SELECT
    d.full_date,
    SUM(f.total_amount) AS daily_revenue,
    ROUND(AVG(SUM(f.total_amount)) OVER (
        ORDER BY d.full_date
        ROWS BETWEEN 6 PRECEDING AND CURRENT ROW
    ), 2) AS seven_day_avg
FROM fact_sales f
JOIN dim_date d ON f.date_key = d.date_key
WHERE d.year = 2024
GROUP BY d.full_date
ORDER BY d.full_date;

-- Rank products within category
SELECT
    p.category,
    p.name,
    SUM(f.total_amount) AS revenue,
    RANK() OVER (PARTITION BY p.category ORDER BY SUM(f.total_amount) DESC) AS category_rank
FROM fact_sales f
JOIN dim_product p ON f.product_key = p.product_key
GROUP BY p.category, p.name
ORDER BY p.category, category_rank;
```

---

## Step 7: Performance Optimization

### BRIN Indexes for Time-Series

```sql
-- BRIN index for large fact tables (efficient for time-ordered data)
CREATE INDEX idx_fact_sales_date_brin ON fact_sales USING BRIN (date_key);
```

### Materialized Views

```sql
-- Create materialized view for common aggregation
CREATE MATERIALIZED VIEW mv_daily_sales AS
SELECT
    d.date_key,
    d.full_date,
    d.year,
    d.month,
    d.week,
    SUM(f.quantity) AS total_quantity,
    SUM(f.total_amount) AS total_revenue,
    COUNT(DISTINCT f.order_id) AS order_count,
    COUNT(DISTINCT f.customer_key) AS customer_count
FROM fact_sales f
JOIN dim_date d ON f.date_key = d.date_key
GROUP BY d.date_key, d.full_date, d.year, d.month, d.week;

-- Index the materialized view
CREATE INDEX idx_mv_daily_date ON mv_daily_sales(date_key);
CREATE INDEX idx_mv_daily_year_month ON mv_daily_sales(year, month);

-- Refresh periodically
REFRESH MATERIALIZED VIEW mv_daily_sales;
```

---

## Summary

You've built a data warehouse with:

| Component | Tables |
|-----------|--------|
| Dimensions | dim_date, dim_customer, dim_product, dim_store |
| Facts | fact_sales, fact_inventory |
| Aggregates | agg_sales_monthly, agg_customer_ltv |
| Views | mv_daily_sales |

### Best Practices Applied

- Star schema design
- Surrogate keys
- SCD Type 2 for history tracking
- Pre-aggregated tables
- Appropriate indexing strategies

---

## Next Steps

1. Set up ETL pipelines for data loading
2. Configure [performance tuning](../../admin/performance-tuning.md)
3. Set up [monitoring](../../admin/monitoring.md)
4. Create dashboards connecting to these views
