-- MySQL procedures for bulk data loading
-- NOTE: uses generate_series to keep each procedure a single statement.

USE sb_grind_mysql;

CREATE PROCEDURE sb_seed_customers(IN p_rows INT)
INSERT INTO customers (customer_uuid, email, full_name, status, created_at, last_login, is_active)
SELECT
    UUID(),
    CONCAT('user', g, '@example.com'),
    CONCAT('Customer ', g),
    CASE
        WHEN (g % 5) = 0 THEN 'SUSPENDED'
        WHEN (g % 4) = 0 THEN 'PENDING'
        WHEN (g % 3) = 0 THEN 'CLOSED'
        ELSE 'ACTIVE'
    END,
    CURRENT_TIMESTAMP,
    CURRENT_TIMESTAMP,
    (g % 10) <> 0
FROM generate_series(1, p_rows) AS g;

CREATE PROCEDURE sb_seed_orders(IN p_rows INT)
INSERT INTO orders (customer_id, order_uuid, order_total, order_status, order_date, created_at, metadata, notes)
SELECT
    (g % p_rows) + 1,
    UUID(),
    (RAND() * 5000),
    CASE
        WHEN (g % 4) = 0 THEN 'PENDING'
        WHEN (g % 3) = 0 THEN 'CLOSED'
        ELSE 'ACTIVE'
    END,
    CURRENT_DATE,
    CURRENT_TIMESTAMP,
    CONCAT('{"source":"inet_suite","batch":', (g % 20), '}'),
    CONCAT('Order ', g)
FROM generate_series(1, p_rows) AS g;

CREATE PROCEDURE sb_seed_order_items(IN p_rows INT)
INSERT INTO order_items (order_id, sku, qty, unit_price, line_total)
SELECT
    (g % p_rows) + 1,
    CONCAT('SKU-', g),
    (g % 5) + 1,
    (RAND() * 200),
    ((g % 5) + 1) * (RAND() * 200)
FROM generate_series(1, p_rows * 2) AS g;

CREATE PROCEDURE sb_seed_events(IN p_rows INT)
INSERT INTO event_log (event_type, event_ts, actor_id, payload)
SELECT
    CASE
        WHEN (g % 4) = 0 THEN 'LOGIN'
        WHEN (g % 3) = 0 THEN 'PURCHASE'
        WHEN (g % 2) = 0 THEN 'VIEW'
        ELSE 'SEARCH'
    END,
    CURRENT_TIMESTAMP,
    (g % p_rows) + 1,
    CONCAT('{"event_id":', g, ',"status":"ok"}')
FROM generate_series(1, p_rows) AS g;

CREATE PROCEDURE sb_seed_metrics(IN p_rows INT)
INSERT INTO metric_samples (sample_ts, cpu_pct, mem_pct, io_read, io_write)
SELECT
    CURRENT_TIMESTAMP,
    (RAND() * 100),
    (RAND() * 100),
    (RAND() * 1000000),
    (RAND() * 1000000)
FROM generate_series(1, p_rows) AS g;
