-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Aggregate and Window Functions
-- Description: Comprehensive aggregate and window function testing
-- ============================================================================

-- Aggregate functions: Operate on sets of rows, return single value
-- Window functions: Operate on sets of rows, return value for each row
-- Support PARTITION BY, ORDER BY, frame specifications

-- Create test database
CREATE DATABASE test_aggregate_window_db;
USE test_aggregate_window_db;

-- ============================================================================
-- Section 1: Basic Aggregate Functions
-- ============================================================================

CREATE TABLE test_sales (
    sale_id SERIAL PRIMARY KEY,
    product VARCHAR(100),
    category VARCHAR(100),
    amount NUMERIC(10,2),
    quantity INT,
    sale_date DATE
);

INSERT INTO test_sales (product, category, amount, quantity, sale_date) VALUES
    ('Widget A', 'Electronics', 29.99, 5, '2024-01-15'),
    ('Widget B', 'Electronics', 49.99, 3, '2024-01-20'),
    ('Gadget A', 'Electronics', 99.99, 2, '2024-02-10'),
    ('Book A', 'Books', 15.99, 10, '2024-01-18'),
    ('Book B', 'Books', 12.99, 8, '2024-02-05'),
    ('Toy A', 'Toys', 24.99, 4, '2024-01-25'),
    ('Toy B', 'Toys', 19.99, 6, '2024-02-15');

-- COUNT
SELECT COUNT(*) AS total_sales FROM test_sales;
SELECT COUNT(DISTINCT category) AS unique_categories FROM test_sales;

-- SUM
SELECT SUM(amount) AS total_revenue FROM test_sales;
SELECT SUM(quantity) AS total_units_sold FROM test_sales;

-- AVG
SELECT AVG(amount) AS average_sale_amount FROM test_sales;
SELECT AVG(quantity) AS average_quantity FROM test_sales;

-- MIN and MAX
SELECT MIN(amount) AS min_amount, MAX(amount) AS max_amount FROM test_sales;
SELECT MIN(sale_date) AS first_sale, MAX(sale_date) AS last_sale FROM test_sales;

-- ============================================================================
-- Section 2: GROUP BY with Aggregates
-- ============================================================================

SELECT
    category,
    COUNT(*) AS num_sales,
    SUM(amount) AS total_revenue,
    AVG(amount) AS avg_amount,
    MIN(amount) AS min_amount,
    MAX(amount) AS max_amount
FROM test_sales
GROUP BY category
ORDER BY category;

-- ============================================================================
-- Section 3: HAVING Clause
-- ============================================================================

SELECT
    category,
    SUM(amount) AS total_revenue
FROM test_sales
GROUP BY category
HAVING SUM(amount) > 100
ORDER BY total_revenue DESC;

-- ============================================================================
-- Section 4: GROUPING SETS
-- ============================================================================

SELECT
    category,
    product,
    SUM(amount) AS total_amount
FROM test_sales
GROUP BY GROUPING SETS (
    (category),
    (product),
    ()
)
ORDER BY category NULLS LAST, product NULLS LAST;

-- ============================================================================
-- Section 5: ROLLUP
-- ============================================================================

SELECT
    category,
    product,
    SUM(amount) AS total_amount
FROM test_sales
GROUP BY ROLLUP (category, product)
ORDER BY category NULLS LAST, product NULLS LAST;

-- ============================================================================
-- Section 6: CUBE
-- ============================================================================

SELECT
    category,
    EXTRACT(MONTH FROM sale_date) AS month,
    SUM(amount) AS total_amount
FROM test_sales
GROUP BY CUBE (category, EXTRACT(MONTH FROM sale_date))
ORDER BY category NULLS LAST, month NULLS LAST;

-- ============================================================================
-- Section 7: Custom Aggregate Function
-- ============================================================================

-- Create a custom aggregate to concatenate strings
CREATE AGGREGATE test_string_agg_custom(TEXT) (
    SFUNC = textcat,
    STYPE = TEXT,
    INITCOND = ''
);

SELECT test_string_agg_custom(product || ', ') AS all_products FROM test_sales;

-- ============================================================================
-- Section 8: Window Function - ROW_NUMBER
-- ============================================================================

SELECT
    sale_id,
    product,
    category,
    amount,
    ROW_NUMBER() OVER (ORDER BY amount DESC) AS row_num,
    ROW_NUMBER() OVER (PARTITION BY category ORDER BY amount DESC) AS category_row_num
FROM test_sales
ORDER BY amount DESC;

-- ============================================================================
-- Section 9: Window Function - RANK and DENSE_RANK
-- ============================================================================

SELECT
    sale_id,
    product,
    amount,
    RANK() OVER (ORDER BY amount DESC) AS rank,
    DENSE_RANK() OVER (ORDER BY amount DESC) AS dense_rank,
    RANK() OVER (PARTITION BY category ORDER BY amount DESC) AS category_rank
FROM test_sales
ORDER BY amount DESC;

-- ============================================================================
-- Section 10: Window Function - NTILE
-- ============================================================================

-- Divide sales into quartiles
SELECT
    sale_id,
    product,
    amount,
    NTILE(4) OVER (ORDER BY amount) AS quartile
FROM test_sales
ORDER BY amount;

-- ============================================================================
-- Section 11: Window Function - LAG and LEAD
-- ============================================================================

SELECT
    sale_id,
    product,
    sale_date,
    amount,
    LAG(amount) OVER (ORDER BY sale_date) AS previous_amount,
    LEAD(amount) OVER (ORDER BY sale_date) AS next_amount,
    amount - LAG(amount) OVER (ORDER BY sale_date) AS diff_from_previous
FROM test_sales
ORDER BY sale_date;

-- ============================================================================
-- Section 12: Window Function - FIRST_VALUE and LAST_VALUE
-- ============================================================================

SELECT
    sale_id,
    product,
    category,
    amount,
    FIRST_VALUE(product) OVER (PARTITION BY category ORDER BY amount DESC) AS highest_product,
    LAST_VALUE(product) OVER (
        PARTITION BY category
        ORDER BY amount DESC
        ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
    ) AS lowest_product
FROM test_sales
ORDER BY category, amount DESC;

-- ============================================================================
-- Section 13: Window Function - Running Totals
-- ============================================================================

SELECT
    sale_id,
    product,
    sale_date,
    amount,
    SUM(amount) OVER (ORDER BY sale_date) AS running_total,
    SUM(amount) OVER (PARTITION BY category ORDER BY sale_date) AS category_running_total
FROM test_sales
ORDER BY sale_date;

-- ============================================================================
-- Section 14: Window Function - Moving Averages
-- ============================================================================

SELECT
    sale_id,
    product,
    sale_date,
    amount,
    AVG(amount) OVER (
        ORDER BY sale_date
        ROWS BETWEEN 2 PRECEDING AND CURRENT ROW
    ) AS moving_avg_3
FROM test_sales
ORDER BY sale_date;

-- ============================================================================
-- Section 15: Frame Specifications - ROWS
-- ============================================================================

SELECT
    sale_id,
    product,
    amount,
    sale_date,
    SUM(amount) OVER (
        ORDER BY sale_date
        ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING
    ) AS sum_3_rows,
    AVG(amount) OVER (
        ORDER BY sale_date
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) AS cumulative_avg
FROM test_sales
ORDER BY sale_date;

-- ============================================================================
-- Section 16: Frame Specifications - RANGE
-- ============================================================================

SELECT
    sale_id,
    product,
    amount,
    sale_date,
    SUM(amount) OVER (
        ORDER BY sale_date
        RANGE BETWEEN INTERVAL '7 days' PRECEDING AND CURRENT ROW
    ) AS sum_last_7_days
FROM test_sales
ORDER BY sale_date;

-- ============================================================================
-- Section 17: Statistical Aggregate Functions
-- ============================================================================

SELECT
    category,
    COUNT(*) AS count,
    AVG(amount) AS mean,
    STDDEV(amount) AS stddev,
    VARIANCE(amount) AS variance,
    MIN(amount) AS min,
    MAX(amount) AS max
FROM test_sales
GROUP BY category
ORDER BY category;

-- ============================================================================
-- Section 18: Percentile Functions
-- ============================================================================

SELECT
    PERCENTILE_CONT(0.5) WITHIN GROUP (ORDER BY amount) AS median,
    PERCENTILE_CONT(0.25) WITHIN GROUP (ORDER BY amount) AS q1,
    PERCENTILE_CONT(0.75) WITHIN GROUP (ORDER BY amount) AS q3,
    PERCENTILE_CONT(0.9) WITHIN GROUP (ORDER BY amount) AS p90
FROM test_sales;

-- ============================================================================
-- Section 19: FILTER Clause with Aggregates
-- ============================================================================

SELECT
    category,
    COUNT(*) AS total_sales,
    COUNT(*) FILTER (WHERE amount > 30) AS high_value_sales,
    SUM(amount) FILTER (WHERE amount > 30) AS high_value_revenue,
    AVG(amount) FILTER (WHERE amount > 30) AS avg_high_value
FROM test_sales
GROUP BY category
ORDER BY category;

-- ============================================================================
-- Section 20: Array Aggregation
-- ============================================================================

SELECT
    category,
    ARRAY_AGG(product ORDER BY amount DESC) AS products,
    ARRAY_AGG(amount ORDER BY amount DESC) AS amounts
FROM test_sales
GROUP BY category
ORDER BY category;

-- ============================================================================
-- Section 21: JSON Aggregation
-- ============================================================================

SELECT
    category,
    JSON_AGG(
        JSON_BUILD_OBJECT(
            'product', product,
            'amount', amount,
            'quantity', quantity
        )
        ORDER BY amount DESC
    ) AS sales_json
FROM test_sales
GROUP BY category
ORDER BY category;

-- ============================================================================
-- Section 22: String Aggregation
-- ============================================================================

SELECT
    category,
    STRING_AGG(product, ', ' ORDER BY product) AS products_list,
    STRING_AGG(amount::TEXT, ' | ' ORDER BY amount DESC) AS amounts_list
FROM test_sales
GROUP BY category
ORDER BY category;

-- ============================================================================
-- Section 23: Boolean Aggregates
-- ============================================================================

SELECT
    category,
    BOOL_AND(amount > 20) AS all_above_20,
    BOOL_OR(amount > 50) AS any_above_50,
    EVERY(quantity > 0) AS all_positive_quantity
FROM test_sales
GROUP BY category
ORDER BY category;

-- ============================================================================
-- Section 24: Complex Window Function Query
-- ============================================================================

CREATE TABLE test_employees (
    employee_id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    department VARCHAR(100),
    salary NUMERIC(12,2),
    hire_date DATE
);

INSERT INTO test_employees (name, department, salary, hire_date) VALUES
    ('Alice', 'Engineering', 95000, '2020-01-15'),
    ('Bob', 'Engineering', 85000, '2021-03-20'),
    ('Charlie', 'Engineering', 75000, '2022-06-10'),
    ('Diana', 'Sales', 70000, '2020-05-01'),
    ('Eve', 'Sales', 65000, '2021-08-15'),
    ('Frank', 'HR', 60000, '2019-11-20'),
    ('Grace', 'HR', 58000, '2022-02-28');

SELECT
    employee_id,
    name,
    department,
    salary,
    hire_date,
    -- Ranking within department
    RANK() OVER (PARTITION BY department ORDER BY salary DESC) AS dept_salary_rank,
    -- Salary percentile
    PERCENT_RANK() OVER (ORDER BY salary) AS salary_percentile,
    -- Running average
    AVG(salary) OVER (
        PARTITION BY department
        ORDER BY hire_date
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) AS dept_running_avg,
    -- Difference from department average
    salary - AVG(salary) OVER (PARTITION BY department) AS diff_from_dept_avg,
    -- Cumulative headcount
    COUNT(*) OVER (
        PARTITION BY department
        ORDER BY hire_date
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) AS cumulative_headcount
FROM test_employees
ORDER BY department, hire_date;

-- ============================================================================
-- Section 25: Window Function with Multiple Partitions
-- ============================================================================

SELECT
    employee_id,
    name,
    department,
    salary,
    EXTRACT(YEAR FROM hire_date) AS hire_year,
    -- Rank within department
    RANK() OVER (PARTITION BY department ORDER BY salary DESC) AS dept_rank,
    -- Rank within hire year
    RANK() OVER (PARTITION BY EXTRACT(YEAR FROM hire_date) ORDER BY salary DESC) AS year_rank,
    -- Overall rank
    RANK() OVER (ORDER BY salary DESC) AS overall_rank
FROM test_employees
ORDER BY salary DESC;

-- ============================================================================
-- Section 26: Best Practices
-- ============================================================================

CREATE TABLE test_aggregate_window_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_aggregate_window_best_practices VALUES
    (1, 'Use COUNT(*) for counting all rows, COUNT(column) for non-NULL values'),
    (2, 'Use DISTINCT with aggregates to count unique values'),
    (3, 'Use FILTER clause to apply conditions to specific aggregates'),
    (4, 'Understand difference between RANK, DENSE_RANK, and ROW_NUMBER'),
    (5, 'Use PARTITION BY to apply window functions within groups'),
    (6, 'Specify ORDER BY in window functions for deterministic results'),
    (7, 'Use frame specifications (ROWS/RANGE) for moving calculations'),
    (8, 'ROWS is physical (count of rows), RANGE is logical (value range)'),
    (9, 'Use UNBOUNDED PRECEDING/FOLLOWING for full partition access'),
    (10, 'LAG/LEAD are useful for time-series analysis'),
    (11, 'Use NTILE for creating equal-sized buckets'),
    (12, 'String/Array/JSON aggregation is powerful for data shaping'),
    (13, 'GROUPING SETS, ROLLUP, CUBE for multi-dimensional aggregation'),
    (14, 'Percentile functions require WITHIN GROUP clause'),
    (15, 'Test window functions with edge cases and empty partitions');

SELECT id, guideline FROM test_aggregate_window_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_aggregate_window_best_practices;
DROP TABLE test_employees;
DROP AGGREGATE test_string_agg_custom(TEXT);
DROP TABLE test_sales;

DROP DATABASE test_aggregate_window_db;

-- End of aggregate and window function tests
