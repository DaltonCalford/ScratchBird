-- PostgreSQL worker workload (run in parallel)

BEGIN;

UPDATE customers
SET last_login = CURRENT_TIMESTAMP
WHERE customer_id IN (
    SELECT customer_id
    FROM customers
    ORDER BY random()
    LIMIT 500
);

INSERT INTO event_log (event_id, event_type, event_ts, actor_id, payload)
SELECT
    nextval('seq_event'),
    'LOGIN',
    CURRENT_TIMESTAMP,
    customer_id,
    '{"worker":"pg","ok":true}'
FROM customers
ORDER BY random()
LIMIT 500;

COMMIT;

SELECT order_status, COUNT(*)
FROM orders
GROUP BY order_status;

SELECT * FROM v_order_summary ORDER BY order_date DESC LIMIT 5;

