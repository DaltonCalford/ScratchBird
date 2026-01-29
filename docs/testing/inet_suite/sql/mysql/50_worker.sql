-- MySQL worker workload (run in parallel)

USE sb_grind_mysql;

BEGIN;

UPDATE customers
SET last_login = CURRENT_TIMESTAMP
WHERE customer_id IN (
    SELECT customer_id
    FROM customers
    ORDER BY RAND()
    LIMIT 500
);

INSERT INTO event_log (event_type, event_ts, actor_id, payload)
SELECT
    'LOGIN',
    CURRENT_TIMESTAMP,
    customer_id,
    '{"worker":"mysql","ok":true}'
FROM customers
ORDER BY RAND()
LIMIT 500;

COMMIT;

SELECT order_status, COUNT(*)
FROM orders
GROUP BY order_status;

SELECT * FROM v_order_summary ORDER BY order_date DESC LIMIT 5;

