-- Firebird procedures for bulk data loading

SET TERM ^;

CREATE PROCEDURE sb_seed_customers(p_rows INT)
AS
DECLARE VARIABLE g INT;
BEGIN
    g = 1;
    WHILE (g <= :p_rows) DO
    BEGIN
        INSERT INTO customers (customer_uuid, email, full_name, status, created_at, last_login, is_active)
        VALUES (
            GEN_UUID(),
            'user' || g || '@example.com',
            'Customer ' || g,
            CASE
                WHEN (MOD(g, 5) = 0) THEN 'SUSPENDED'
                WHEN (MOD(g, 4) = 0) THEN 'PENDING'
                WHEN (MOD(g, 3) = 0) THEN 'CLOSED'
                ELSE 'ACTIVE'
            END,
            CURRENT_TIMESTAMP,
            CURRENT_TIMESTAMP,
            (MOD(g, 10) <> 0)
        );
        g = g + 1;
    END
END^

CREATE PROCEDURE sb_seed_orders(p_rows INT)
AS
DECLARE VARIABLE g INT;
BEGIN
    g = 1;
    WHILE (g <= :p_rows) DO
    BEGIN
        INSERT INTO orders (customer_id, order_uuid, order_total, order_status, order_date, created_at, metadata, notes)
        VALUES (
            MOD(g, :p_rows) + 1,
            GEN_UUID(),
            CAST(MOD(g * 37, 5000) AS NUMERIC(12,2)),
            CASE
                WHEN (MOD(g, 4) = 0) THEN 'PENDING'
                WHEN (MOD(g, 3) = 0) THEN 'CLOSED'
                ELSE 'ACTIVE'
            END,
            CURRENT_DATE,
            CURRENT_TIMESTAMP,
            '{"source":"inet_suite","batch":' || MOD(g, 20) || '}',
            'Order ' || g
        );
        g = g + 1;
    END
END^

CREATE PROCEDURE sb_seed_order_items(p_rows INT)
AS
DECLARE VARIABLE g INT;
DECLARE VARIABLE max_rows INT;
BEGIN
    g = 1;
    max_rows = :p_rows * 2;
    WHILE (g <= :max_rows) DO
    BEGIN
        INSERT INTO order_items (item_id, order_id, sku, qty, unit_price, line_total)
        VALUES (
            g,
            MOD(g, :p_rows) + 1,
            'SKU-' || g,
            MOD(g, 5) + 1,
            CAST(MOD(g * 17, 200) AS NUMERIC(12,2)),
            CAST((MOD(g, 5) + 1) * MOD(g * 17, 200) AS NUMERIC(12,2))
        );
        g = g + 1;
    END
END^

CREATE PROCEDURE sb_seed_events(p_rows INT)
AS
DECLARE VARIABLE g INT;
BEGIN
    g = 1;
    WHILE (g <= :p_rows) DO
    BEGIN
        INSERT INTO event_log (event_type, event_ts, actor_id, payload)
        VALUES (
            CASE
                WHEN (MOD(g, 4) = 0) THEN 'LOGIN'
                WHEN (MOD(g, 3) = 0) THEN 'PURCHASE'
                WHEN (MOD(g, 2) = 0) THEN 'VIEW'
                ELSE 'SEARCH'
            END,
            CURRENT_TIMESTAMP,
            MOD(g, :p_rows) + 1,
            '{"event_id":' || g || ',"status":"ok"}'
        );
        g = g + 1;
    END
END^

SET TERM ;^
