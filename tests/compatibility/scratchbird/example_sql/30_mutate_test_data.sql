-- Chain step: update/delete cycle for regression visibility

UPDATE demo.orders
SET order_status = 'paid'
WHERE order_id = 1002;

DELETE FROM demo.orders
WHERE order_id = 1003;

SELECT order_id, customer_id, order_total, order_status
FROM demo.orders
WHERE customer_id = 1001
ORDER BY order_id;
