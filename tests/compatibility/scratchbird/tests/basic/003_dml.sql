-- ScratchBird Native Test: DML Operations
-- Test ID: basic_003
-- Description: INSERT, UPDATE, DELETE, SELECT operations

-- Create database
CREATE DATABASE test_dml;
\c test_dml;

-- Create test table
CREATE TABLE products (
    product_id INTEGER PRIMARY KEY,
    product_name VARCHAR(100) NOT NULL,
    category VARCHAR(50),
    price DECIMAL(10, 2),
    stock_quantity INTEGER,
    last_updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Test INSERT - Single row
INSERT INTO products (product_id, product_name, category, price, stock_quantity)
VALUES (1, 'Laptop', 'Electronics', 1200.00, 50);

SELECT * FROM products;

-- Test INSERT - Multiple rows
INSERT INTO products (product_id, product_name, category, price, stock_quantity) VALUES
    (2, 'Mouse', 'Electronics', 25.00, 200),
    (3, 'Keyboard', 'Electronics', 75.00, 150),
    (4, 'Monitor', 'Electronics', 350.00, 75),
    (5, 'Desk Chair', 'Furniture', 250.00, 30);

SELECT * FROM products ORDER BY product_id;

-- Test UPDATE - Single row
UPDATE products SET price = 1150.00, stock_quantity = 45 WHERE product_id = 1;

SELECT product_id, product_name, price, stock_quantity FROM products WHERE product_id = 1;

-- Test UPDATE - Multiple rows with WHERE clause
UPDATE products SET price = price * 1.10 WHERE category = 'Electronics';

SELECT product_id, product_name, category, price FROM products ORDER BY product_id;

-- Test UPDATE - Without WHERE clause (update all rows)
UPDATE products SET last_updated = '2025-12-31 12:00:00';

SELECT COUNT(*) AS updated_count FROM products WHERE last_updated = '2025-12-31 12:00:00';

-- Test DELETE - Single row
DELETE FROM products WHERE product_id = 5;

SELECT COUNT(*) AS remaining_count FROM products;

-- Test DELETE - Multiple rows with WHERE clause
DELETE FROM products WHERE price < 50.00;

SELECT COUNT(*) AS remaining_count FROM products;
SELECT product_id, product_name, price FROM products ORDER BY product_id;

-- Test SELECT with various clauses
INSERT INTO products (product_id, product_name, category, price, stock_quantity) VALUES
    (6, 'Tablet', 'Electronics', 450.00, 80),
    (7, 'Headphones', 'Electronics', 120.00, 120),
    (8, 'Webcam', 'Electronics', 85.00, 60),
    (9, 'USB Cable', 'Accessories', 12.00, 500),
    (10, 'Power Bank', 'Accessories', 35.00, 200);

-- Test SELECT with WHERE
SELECT product_name, price FROM products WHERE category = 'Electronics' ORDER BY price DESC;

-- Test SELECT with LIMIT
SELECT product_name, price FROM products ORDER BY price DESC LIMIT 3;

-- Test SELECT with OFFSET
SELECT product_name, price FROM products ORDER BY price ASC LIMIT 3 OFFSET 2;

-- Test SELECT with aggregate functions
SELECT category, COUNT(*) AS product_count, AVG(price) AS avg_price, SUM(stock_quantity) AS total_stock
FROM products
GROUP BY category
ORDER BY category;

-- Test SELECT with HAVING
SELECT category, AVG(price) AS avg_price
FROM products
GROUP BY category
HAVING AVG(price) > 100
ORDER BY avg_price DESC;

-- Test SELECT DISTINCT
SELECT DISTINCT category FROM products ORDER BY category;

-- Test SELECT with calculated columns
SELECT product_name, price, stock_quantity, (price * stock_quantity) AS total_value
FROM products
ORDER BY total_value DESC;

-- Test UPSERT (INSERT ON CONFLICT)
INSERT INTO products (product_id, product_name, category, price, stock_quantity)
VALUES (1, 'Laptop Pro', 'Electronics', 1500.00, 40)
ON CONFLICT (product_id) DO UPDATE
    SET product_name = EXCLUDED.product_name,
        price = EXCLUDED.price,
        stock_quantity = EXCLUDED.stock_quantity;

SELECT product_id, product_name, price, stock_quantity FROM products WHERE product_id = 1;

-- Test DELETE with subquery
DELETE FROM products WHERE price < (SELECT AVG(price) FROM products);

SELECT product_name, price FROM products ORDER BY price DESC;

-- Test TRUNCATE TABLE
TRUNCATE TABLE products;
SELECT COUNT(*) FROM products;

-- Cleanup
DROP TABLE products;
DROP DATABASE test_dml;
