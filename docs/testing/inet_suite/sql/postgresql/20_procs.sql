-- PostgreSQL procedures for bulk data loading

CREATE PROCEDURE sb_seed_customers(IN p_rows INT)
LANGUAGE plpgsql
AS $$
BEGIN
    INSERT INTO customers (customer_id, customer_uuid, email, full_name, status, created_at, last_login, is_active)
    SELECT
        nextval('seq_customer'),
        UUID(),
        'user' || g || '@example.com',
        'Customer ' || g,
        CASE
            WHEN (g % 5) = 0 THEN 'SUSPENDED'
            WHEN (g % 4) = 0 THEN 'PENDING'
            WHEN (g % 3) = 0 THEN 'CLOSED'
            ELSE 'ACTIVE'
        END,
        CURRENT_TIMESTAMP - (g || ' minutes')::INTERVAL,
        CURRENT_TIMESTAMP - ((g % 120) || ' minutes')::INTERVAL,
        (g % 10) <> 0
    FROM generate_series(1, p_rows) AS g;
END;
$$;

CREATE PROCEDURE sb_seed_orders(IN p_rows INT)
LANGUAGE plpgsql
AS $$
BEGIN
    INSERT INTO orders (order_id, customer_id, order_uuid, order_total, order_status, order_date, created_at, metadata, notes)
    SELECT
        nextval('seq_order'),
        (g % p_rows) + 1,
        UUID(),
        (random() * 5000)::NUMERIC(12,2),
        CASE
            WHEN (g % 4) = 0 THEN 'PENDING'
            WHEN (g % 3) = 0 THEN 'CLOSED'
            ELSE 'ACTIVE'
        END,
        CURRENT_DATE - (g % 365),
        CURRENT_TIMESTAMP - (g || ' seconds')::INTERVAL,
        '{"source":"inet_suite","batch":' || (g % 20) || '}',
        'Order ' || g
    FROM generate_series(1, p_rows) AS g;
END;
$$;

CREATE PROCEDURE sb_seed_order_items(IN p_rows INT)
LANGUAGE plpgsql
AS $$
BEGIN
    INSERT INTO order_items (item_id, order_id, sku, qty, unit_price, line_total)
    SELECT
        g,
        (g % p_rows) + 1,
        'SKU-' || g,
        (g % 5) + 1,
        (random() * 200)::NUMERIC(12,2),
        ((g % 5) + 1) * (random() * 200)::NUMERIC(12,2)
    FROM generate_series(1, p_rows * 2) AS g;
END;
$$;

CREATE PROCEDURE sb_seed_events(IN p_rows INT)
LANGUAGE plpgsql
AS $$
BEGIN
    INSERT INTO event_log (event_id, event_type, event_ts, actor_id, payload)
    SELECT
        nextval('seq_event'),
        CASE
            WHEN (g % 4) = 0 THEN 'LOGIN'
            WHEN (g % 3) = 0 THEN 'PURCHASE'
            WHEN (g % 2) = 0 THEN 'VIEW'
            ELSE 'SEARCH'
        END,
        CURRENT_TIMESTAMP - (g || ' seconds')::INTERVAL,
        (g % p_rows) + 1,
        '{"event_id":' || g || ',"status":"ok"}'
    FROM generate_series(1, p_rows) AS g;
END;
$$;

CREATE PROCEDURE sb_seed_metrics(IN p_rows INT)
LANGUAGE plpgsql
AS $$
BEGIN
    INSERT INTO metric_samples (sample_id, sample_ts, cpu_pct, mem_pct, io_read, io_write)
    SELECT
        g,
        CURRENT_TIMESTAMP - (g || ' seconds')::INTERVAL,
        random() * 100,
        random() * 100,
        (random() * 1000000)::BIGINT,
        (random() * 1000000)::BIGINT
    FROM generate_series(1, p_rows) AS g;
END;
$$;
