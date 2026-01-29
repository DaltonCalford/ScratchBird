-- Firebird worker workload (run in parallel)

SET TRANSACTION READ WRITE;

UPDATE customers
SET last_login = CURRENT_TIMESTAMP
WHERE customer_id IN (
    SELECT FIRST 500 customer_id
    FROM customers
    ORDER BY customer_id
);

INSERT INTO event_log (event_id, event_type, event_ts, actor_id, payload)
SELECT
    NEXT VALUE FOR seq_event,
    'LOGIN',
    CURRENT_TIMESTAMP,
    src.customer_id,
    '{"worker":"fb","ok":true}'
FROM (
    SELECT FIRST 500 customer_id
    FROM customers
    ORDER BY customer_id
) src;

COMMIT;

SELECT order_status, COUNT(*)
FROM orders
GROUP BY order_status;

SELECT FIRST 5 * FROM v_order_summary ORDER BY order_date DESC;
