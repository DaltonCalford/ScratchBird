-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DML - Window Functions and Analytical Queries
-- Description: Comprehensive window function testing
-- ============================================================================

-- Window Functions: Analytical queries and ranking
-- ROW_NUMBER, RANK, DENSE_RANK, NTILE
-- LAG, LEAD, FIRST_VALUE, LAST_VALUE, NTH_VALUE
-- Aggregate window functions, frames, partitions

-- Create test database
CREATE DATABASE test_window_functions_db;
USE test_window_functions_db;

-- ============================================================================
-- Section 1: ROW_NUMBER - Sequential Numbering
-- ============================================================================

CREATE TABLE test_sales (
    sale_id SERIAL PRIMARY KEY,
    salesperson VARCHAR(100),
    region VARCHAR(50),
    sale_date DATE,
    amount NUMERIC(10,2)
);

INSERT INTO test_sales (salesperson, region, sale_date, amount) VALUES
    ('Alice', 'North', '2024-01-15', 1000.00),
    ('Bob', 'North', '2024-01-20', 1500.00),
    ('Alice', 'North', '2024-02-10', 1200.00),
    ('Charlie', 'South', '2024-01-18', 2000.00),
    ('Diana', 'South', '2024-02-05', 1800.00),
    ('Bob', 'North', '2024-02-15', 1600.00),
    ('Charlie', 'South', '2024-02-20', 2200.00);

-- ROW_NUMBER - unique sequential number
SELECT
    salesperson,
    region,
    sale_date,
    amount,
    ROW_NUMBER() OVER (ORDER BY sale_date) AS overall_row_num
FROM test_sales
ORDER BY sale_date;

-- ROW_NUMBER with PARTITION BY
SELECT
    salesperson,
    region,
    sale_date,
    amount,
    ROW_NUMBER() OVER (PARTITION BY region ORDER BY sale_date) AS region_row_num
FROM test_sales
ORDER BY region, sale_date;

-- ROW_NUMBER for top N per group
SELECT *
FROM (
    SELECT
        salesperson,
        region,
        amount,
        ROW_NUMBER() OVER (PARTITION BY region ORDER BY amount DESC) AS rn
    FROM test_sales
) ranked
WHERE rn <= 2
ORDER BY region, rn;

-- ============================================================================
-- Section 2: RANK and DENSE_RANK - Ranking with Ties
-- ============================================================================

CREATE TABLE test_exam_scores (
    student_id INT,
    student_name VARCHAR(100),
    subject VARCHAR(50),
    score INT
);

INSERT INTO test_exam_scores VALUES
    (1, 'Alice', 'Math', 95),
    (2, 'Bob', 'Math', 90),
    (3, 'Charlie', 'Math', 90),
    (4, 'Diana', 'Math', 85),
    (5, 'Eve', 'Math', 95),
    (1, 'Alice', 'Science', 88),
    (2, 'Bob', 'Science', 92),
    (3, 'Charlie', 'Science', 85);

-- RANK - gaps in ranking for ties
SELECT
    student_name,
    subject,
    score,
    RANK() OVER (PARTITION BY subject ORDER BY score DESC) AS rank
FROM test_exam_scores
ORDER BY subject, rank;

-- DENSE_RANK - no gaps in ranking
SELECT
    student_name,
    subject,
    score,
    DENSE_RANK() OVER (PARTITION BY subject ORDER BY score DESC) AS dense_rank
FROM test_exam_scores
ORDER BY subject, dense_rank;

-- Compare RANK, DENSE_RANK, ROW_NUMBER
SELECT
    student_name,
    score,
    ROW_NUMBER() OVER (ORDER BY score DESC) AS row_num,
    RANK() OVER (ORDER BY score DESC) AS rank,
    DENSE_RANK() OVER (ORDER BY score DESC) AS dense_rank
FROM test_exam_scores
WHERE subject = 'Math'
ORDER BY score DESC;

-- ============================================================================
-- Section 3: NTILE - Dividing into Buckets
-- ============================================================================

-- NTILE - divide into quartiles
SELECT
    salesperson,
    amount,
    NTILE(4) OVER (ORDER BY amount) AS quartile
FROM test_sales
ORDER BY amount;

-- NTILE - divide into deciles
SELECT
    salesperson,
    amount,
    NTILE(10) OVER (ORDER BY amount) AS decile
FROM test_sales
ORDER BY amount;

-- NTILE with PARTITION BY
SELECT
    region,
    salesperson,
    amount,
    NTILE(2) OVER (PARTITION BY region ORDER BY amount) AS region_half
FROM test_sales
ORDER BY region, amount;

-- ============================================================================
-- Section 4: LAG and LEAD - Accessing Adjacent Rows
-- ============================================================================

CREATE TABLE test_stock_prices (
    ticker VARCHAR(10),
    price_date DATE,
    close_price NUMERIC(10,2)
);

INSERT INTO test_stock_prices VALUES
    ('AAPL', '2024-01-02', 150.00),
    ('AAPL', '2024-01-03', 152.00),
    ('AAPL', '2024-01-04', 151.00),
    ('AAPL', '2024-01-05', 153.00),
    ('GOOGL', '2024-01-02', 120.00),
    ('GOOGL', '2024-01-03', 122.00),
    ('GOOGL', '2024-01-04', 121.50);

-- LAG - access previous row
SELECT
    ticker,
    price_date,
    close_price,
    LAG(close_price) OVER (PARTITION BY ticker ORDER BY price_date) AS prev_close,
    close_price - LAG(close_price) OVER (PARTITION BY ticker ORDER BY price_date) AS daily_change
FROM test_stock_prices
ORDER BY ticker, price_date;

-- LAG with offset and default
SELECT
    ticker,
    price_date,
    close_price,
    LAG(close_price, 1, 0) OVER (PARTITION BY ticker ORDER BY price_date) AS prev_close,
    LAG(close_price, 2, 0) OVER (PARTITION BY ticker ORDER BY price_date) AS two_days_ago
FROM test_stock_prices
ORDER BY ticker, price_date;

-- LEAD - access next row
SELECT
    ticker,
    price_date,
    close_price,
    LEAD(close_price) OVER (PARTITION BY ticker ORDER BY price_date) AS next_close,
    LEAD(close_price) OVER (PARTITION BY ticker ORDER BY price_date) - close_price AS next_change
FROM test_stock_prices
ORDER BY ticker, price_date;

-- ============================================================================
-- Section 5: FIRST_VALUE and LAST_VALUE
-- ============================================================================

CREATE TABLE test_temperatures (
    city VARCHAR(50),
    measurement_date DATE,
    temperature NUMERIC(5,2)
);

INSERT INTO test_temperatures VALUES
    ('New York', '2024-01-01', 32.5),
    ('New York', '2024-01-02', 35.0),
    ('New York', '2024-01-03', 30.0),
    ('Los Angeles', '2024-01-01', 65.0),
    ('Los Angeles', '2024-01-02', 68.0),
    ('Los Angeles', '2024-01-03', 70.0);

-- FIRST_VALUE - first value in window
SELECT
    city,
    measurement_date,
    temperature,
    FIRST_VALUE(temperature) OVER (
        PARTITION BY city ORDER BY measurement_date
    ) AS first_temp_of_period
FROM test_temperatures
ORDER BY city, measurement_date;

-- LAST_VALUE - last value in window (need proper frame)
SELECT
    city,
    measurement_date,
    temperature,
    LAST_VALUE(temperature) OVER (
        PARTITION BY city
        ORDER BY measurement_date
        ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
    ) AS last_temp_of_period
FROM test_temperatures
ORDER BY city, measurement_date;

-- FIRST_VALUE and LAST_VALUE together
SELECT
    city,
    measurement_date,
    temperature,
    FIRST_VALUE(temperature) OVER w AS first_temp,
    LAST_VALUE(temperature) OVER w AS last_temp,
    temperature - FIRST_VALUE(temperature) OVER w AS change_from_first
FROM test_temperatures
WINDOW w AS (PARTITION BY city ORDER BY measurement_date ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING)
ORDER BY city, measurement_date;

-- ============================================================================
-- Section 6: NTH_VALUE - Accessing Specific Positions
-- ============================================================================

-- NTH_VALUE - get second highest salary per department
CREATE TABLE test_salaries (
    emp_name VARCHAR(100),
    department VARCHAR(50),
    salary NUMERIC(10,2)
);

INSERT INTO test_salaries VALUES
    ('Alice', 'Engineering', 90000),
    ('Bob', 'Engineering', 95000),
    ('Charlie', 'Engineering', 100000),
    ('Diana', 'Sales', 70000),
    ('Eve', 'Sales', 75000),
    ('Frank', 'Sales', 80000);

SELECT
    department,
    emp_name,
    salary,
    NTH_VALUE(salary, 2) OVER (
        PARTITION BY department
        ORDER BY salary DESC
        ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
    ) AS second_highest_salary
FROM test_salaries
ORDER BY department, salary DESC;

-- ============================================================================
-- Section 7: Aggregate Window Functions
-- ============================================================================

-- Running totals with SUM
SELECT
    salesperson,
    sale_date,
    amount,
    SUM(amount) OVER (
        PARTITION BY salesperson
        ORDER BY sale_date
    ) AS running_total
FROM test_sales
ORDER BY salesperson, sale_date;

-- Running average
SELECT
    salesperson,
    sale_date,
    amount,
    AVG(amount) OVER (
        PARTITION BY salesperson
        ORDER BY sale_date
    ) AS running_avg
FROM test_sales
ORDER BY salesperson, sale_date;

-- COUNT with window
SELECT
    region,
    salesperson,
    sale_date,
    amount,
    COUNT(*) OVER (
        PARTITION BY region
        ORDER BY sale_date
    ) AS sales_count_so_far
FROM test_sales
ORDER BY region, sale_date;

-- Multiple aggregates
SELECT
    salesperson,
    sale_date,
    amount,
    SUM(amount) OVER w AS running_total,
    AVG(amount) OVER w AS running_avg,
    MIN(amount) OVER w AS min_so_far,
    MAX(amount) OVER w AS max_so_far,
    COUNT(*) OVER w AS count_so_far
FROM test_sales
WINDOW w AS (PARTITION BY salesperson ORDER BY sale_date)
ORDER BY salesperson, sale_date;

-- ============================================================================
-- Section 8: Window Frames - ROWS vs RANGE
-- ============================================================================

CREATE TABLE test_daily_sales (
    sale_date DATE,
    daily_amount NUMERIC(10,2)
);

INSERT INTO test_daily_sales VALUES
    ('2024-01-01', 1000),
    ('2024-01-02', 1100),
    ('2024-01-03', 900),
    ('2024-01-04', 1200),
    ('2024-01-05', 1050),
    ('2024-01-06', 1150),
    ('2024-01-07', 1000);

-- ROWS frame - physical rows
SELECT
    sale_date,
    daily_amount,
    SUM(daily_amount) OVER (
        ORDER BY sale_date
        ROWS BETWEEN 2 PRECEDING AND CURRENT ROW
    ) AS moving_3day_total
FROM test_daily_sales
ORDER BY sale_date;

-- RANGE frame - logical range
SELECT
    sale_date,
    daily_amount,
    SUM(daily_amount) OVER (
        ORDER BY sale_date
        RANGE BETWEEN INTERVAL '2 days' PRECEDING AND CURRENT ROW
    ) AS range_3day_total
FROM test_daily_sales
ORDER BY sale_date;

-- ROWS BETWEEN examples
SELECT
    sale_date,
    daily_amount,
    -- Previous 2 rows
    AVG(daily_amount) OVER (
        ORDER BY sale_date
        ROWS BETWEEN 2 PRECEDING AND 1 PRECEDING
    ) AS avg_prev_2,
    -- Centered moving average
    AVG(daily_amount) OVER (
        ORDER BY sale_date
        ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING
    ) AS centered_avg,
    -- All rows up to current
    SUM(daily_amount) OVER (
        ORDER BY sale_date
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) AS cumulative_sum
FROM test_daily_sales
ORDER BY sale_date;

-- ============================================================================
-- Section 9: Named Windows (WINDOW Clause)
-- ============================================================================

-- Define window once, reuse multiple times
SELECT
    salesperson,
    region,
    sale_date,
    amount,
    ROW_NUMBER() OVER w AS row_num,
    RANK() OVER w AS rank,
    SUM(amount) OVER w AS running_total,
    AVG(amount) OVER w AS running_avg
FROM test_sales
WINDOW w AS (PARTITION BY region ORDER BY sale_date)
ORDER BY region, sale_date;

-- Multiple named windows
SELECT
    salesperson,
    sale_date,
    amount,
    SUM(amount) OVER by_date AS daily_running_total,
    AVG(amount) OVER by_person AS person_avg,
    RANK() OVER by_amount AS amount_rank
FROM test_sales
WINDOW
    by_date AS (ORDER BY sale_date),
    by_person AS (PARTITION BY salesperson),
    by_amount AS (ORDER BY amount DESC)
ORDER BY sale_date;

-- ============================================================================
-- Section 10: Cumulative and Moving Aggregates
-- ============================================================================

-- Cumulative sum
SELECT
    sale_date,
    salesperson,
    amount,
    SUM(amount) OVER (ORDER BY sale_date) AS cumulative_sales
FROM test_sales
ORDER BY sale_date;

-- Moving average (3-period)
SELECT
    sale_date,
    amount,
    AVG(amount) OVER (
        ORDER BY sale_date
        ROWS BETWEEN 2 PRECEDING AND CURRENT ROW
    ) AS moving_avg_3
FROM test_daily_sales
ORDER BY sale_date;

-- Year-to-date calculations
CREATE TABLE test_monthly_revenue (
    revenue_month DATE,
    revenue NUMERIC(10,2)
);

INSERT INTO test_monthly_revenue VALUES
    ('2024-01-01', 10000),
    ('2024-02-01', 12000),
    ('2024-03-01', 11000),
    ('2024-04-01', 13000),
    ('2024-05-01', 14000);

SELECT
    revenue_month,
    revenue,
    SUM(revenue) OVER (
        PARTITION BY EXTRACT(YEAR FROM revenue_month)
        ORDER BY revenue_month
    ) AS ytd_revenue
FROM test_monthly_revenue
ORDER BY revenue_month;

-- ============================================================================
-- Section 11: Percent of Total Calculations
-- ============================================================================

-- Percent of total sales
SELECT
    salesperson,
    region,
    SUM(amount) AS total_sales,
    SUM(SUM(amount)) OVER () AS grand_total,
    ROUND(100.0 * SUM(amount) / SUM(SUM(amount)) OVER (), 2) AS pct_of_total
FROM test_sales
GROUP BY salesperson, region
ORDER BY total_sales DESC;

-- Percent of group total
SELECT
    region,
    salesperson,
    SUM(amount) AS person_sales,
    SUM(SUM(amount)) OVER (PARTITION BY region) AS region_sales,
    ROUND(100.0 * SUM(amount) / SUM(SUM(amount)) OVER (PARTITION BY region), 2) AS pct_of_region
FROM test_sales
GROUP BY region, salesperson
ORDER BY region, person_sales DESC;

-- ============================================================================
-- Section 12: Percentile Calculations
-- ============================================================================

-- PERCENT_RANK and CUME_DIST
SELECT
    salesperson,
    amount,
    PERCENT_RANK() OVER (ORDER BY amount) AS percent_rank,
    CUME_DIST() OVER (ORDER BY amount) AS cumulative_dist,
    ROUND(100 * PERCENT_RANK() OVER (ORDER BY amount), 2) AS percentile
FROM test_sales
ORDER BY amount;

-- Percentile with PARTITION BY
SELECT
    region,
    salesperson,
    amount,
    PERCENT_RANK() OVER (PARTITION BY region ORDER BY amount) AS region_percent_rank,
    ROUND(100 * PERCENT_RANK() OVER (PARTITION BY region ORDER BY amount), 2) AS region_percentile
FROM test_sales
ORDER BY region, amount;

-- ============================================================================
-- Section 13: Window Functions with Filtering
-- ============================================================================

-- Filter results based on window function
SELECT *
FROM (
    SELECT
        salesperson,
        region,
        amount,
        RANK() OVER (PARTITION BY region ORDER BY amount DESC) AS rank_in_region
    FROM test_sales
) ranked
WHERE rank_in_region <= 2
ORDER BY region, rank_in_region;

-- Filtering with aggregates
SELECT *
FROM (
    SELECT
        salesperson,
        sale_date,
        amount,
        AVG(amount) OVER (PARTITION BY salesperson) AS person_avg
    FROM test_sales
) with_avg
WHERE amount > person_avg
ORDER BY salesperson, sale_date;

-- ============================================================================
-- Section 14: Window Functions vs GROUP BY
-- ============================================================================

-- GROUP BY (aggregates reduce rows)
SELECT
    region,
    COUNT(*) AS sale_count,
    SUM(amount) AS total_sales,
    AVG(amount) AS avg_sale
FROM test_sales
GROUP BY region
ORDER BY region;

-- Window functions (preserve all rows)
SELECT
    salesperson,
    region,
    sale_date,
    amount,
    COUNT(*) OVER (PARTITION BY region) AS region_sale_count,
    SUM(amount) OVER (PARTITION BY region) AS region_total_sales,
    AVG(amount) OVER (PARTITION BY region) AS region_avg_sale
FROM test_sales
ORDER BY region, sale_date;

-- ============================================================================
-- Section 15: Window Functions with Joins
-- ============================================================================

CREATE TABLE test_targets (
    region VARCHAR(50) PRIMARY KEY,
    monthly_target NUMERIC(10,2)
);

INSERT INTO test_targets VALUES
    ('North', 5000),
    ('South', 6000);

-- Window functions with joined data
SELECT
    s.region,
    s.salesperson,
    s.amount,
    t.monthly_target,
    SUM(s.amount) OVER (PARTITION BY s.region ORDER BY s.sale_date) AS region_cumulative,
    t.monthly_target - SUM(s.amount) OVER (PARTITION BY s.region ORDER BY s.sale_date) AS remaining_to_target
FROM test_sales s
INNER JOIN test_targets t ON s.region = t.region
ORDER BY s.region, s.sale_date;

-- ============================================================================
-- Section 16: Running Differences
-- ============================================================================

-- Calculate change from previous period
SELECT
    ticker,
    price_date,
    close_price,
    close_price - LAG(close_price) OVER (PARTITION BY ticker ORDER BY price_date) AS price_change,
    ROUND(
        100.0 * (close_price - LAG(close_price) OVER (PARTITION BY ticker ORDER BY price_date)) /
        LAG(close_price) OVER (PARTITION BY ticker ORDER BY price_date),
        2
    ) AS pct_change
FROM test_stock_prices
ORDER BY ticker, price_date;

-- ============================================================================
-- Section 17: Gap and Island Detection
-- ============================================================================

CREATE TABLE test_attendance (
    emp_id INT,
    attendance_date DATE
);

INSERT INTO test_attendance VALUES
    (1, '2024-01-01'),
    (1, '2024-01-02'),
    (1, '2024-01-03'),
    -- Gap here
    (1, '2024-01-05'),
    (1, '2024-01-06'),
    -- Gap here
    (1, '2024-01-08');

-- Detect consecutive sequences (islands)
WITH numbered AS (
    SELECT
        emp_id,
        attendance_date,
        ROW_NUMBER() OVER (PARTITION BY emp_id ORDER BY attendance_date) AS rn,
        attendance_date - (ROW_NUMBER() OVER (PARTITION BY emp_id ORDER BY attendance_date) * INTERVAL '1 day') AS island_id
    FROM test_attendance
)
SELECT
    emp_id,
    island_id,
    MIN(attendance_date) AS streak_start,
    MAX(attendance_date) AS streak_end,
    COUNT(*) AS streak_length
FROM numbered
GROUP BY emp_id, island_id
ORDER BY emp_id, streak_start;

-- ============================================================================
-- Section 18: Window Functions for Pivot-like Operations
-- ============================================================================

CREATE TABLE test_quarterly_sales (
    year INT,
    quarter INT,
    region VARCHAR(50),
    sales NUMERIC(10,2)
);

INSERT INTO test_quarterly_sales VALUES
    (2024, 1, 'North', 10000),
    (2024, 2, 'North', 12000),
    (2024, 3, 'North', 11000),
    (2024, 1, 'South', 15000),
    (2024, 2, 'South', 16000),
    (2024, 3, 'South', 17000);

-- Use window functions for comparison across quarters
SELECT
    year,
    region,
    quarter,
    sales,
    LAG(sales, 1) OVER w AS prev_quarter,
    sales - LAG(sales, 1) OVER w AS qoq_change,
    FIRST_VALUE(sales) OVER w AS q1_sales,
    sales - FIRST_VALUE(sales) OVER w AS change_from_q1
FROM test_quarterly_sales
WINDOW w AS (PARTITION BY year, region ORDER BY quarter)
ORDER BY year, region, quarter;

-- ============================================================================
-- Section 19: Window Function Performance Optimization
-- ============================================================================

-- Create index for partition and order columns
CREATE INDEX idx_sales_region_date ON test_sales(region, sale_date);

-- Reuse window definition for better performance
EXPLAIN (ANALYZE, BUFFERS)
SELECT
    region,
    salesperson,
    sale_date,
    amount,
    ROW_NUMBER() OVER w AS rn,
    RANK() OVER w AS rnk,
    SUM(amount) OVER w AS running_sum
FROM test_sales
WINDOW w AS (PARTITION BY region ORDER BY sale_date)
ORDER BY region, sale_date;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_window_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_window_best_practices VALUES
    (1, 'Window functions do not reduce rows (unlike GROUP BY)'),
    (2, 'ROW_NUMBER always unique, RANK/DENSE_RANK handle ties'),
    (3, 'LAG/LEAD access adjacent rows without self-join'),
    (4, 'FIRST_VALUE/LAST_VALUE need proper frame specification'),
    (5, 'ROWS frame counts physical rows, RANGE uses logical values'),
    (6, 'PARTITION BY creates independent windows'),
    (7, 'ORDER BY in window determines row ordering'),
    (8, 'Named windows (WINDOW clause) improve readability'),
    (9, 'Default frame: RANGE UNBOUNDED PRECEDING to CURRENT ROW'),
    (10, 'Use ROWS for precise control over frame'),
    (11, 'Index PARTITION BY and ORDER BY columns'),
    (12, 'Window functions execute after WHERE, GROUP BY, HAVING'),
    (13, 'Cannot filter on window functions in WHERE (use subquery)'),
    (14, 'Use window functions instead of correlated subqueries'),
    (15, 'NTILE divides into approximately equal buckets'),
    (16, 'PERCENT_RANK and CUME_DIST for percentile calculations'),
    (17, 'Combine multiple window functions in single query'),
    (18, 'Frame defaults differ between aggregate and ranking functions'),
    (19, 'UNBOUNDED FOLLOWING for complete partition access'),
    (20, 'Test window query performance with EXPLAIN');

SELECT id, guideline FROM test_window_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_window_best_practices;
DROP TABLE test_quarterly_sales;
DROP TABLE test_attendance;
DROP TABLE test_targets;
DROP TABLE test_monthly_revenue;
DROP TABLE test_daily_sales;
DROP TABLE test_salaries;
DROP TABLE test_temperatures;
DROP TABLE test_stock_prices;
DROP TABLE test_exam_scores;
DROP TABLE test_sales;

DROP DATABASE test_window_functions_db;

-- End of window functions tests
