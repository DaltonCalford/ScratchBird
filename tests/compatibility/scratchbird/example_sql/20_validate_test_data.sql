-- Chain step: readback validation for seeded/test data

SELECT COUNT(*) AS customer_count FROM demo.customers;
SELECT COUNT(*) AS order_count FROM demo.orders;

SELECT order_status, COUNT(*) AS status_count
FROM demo.orders
GROUP BY order_status
ORDER BY order_status;

SELECT customer_id, SUM(order_total) AS total_spend
FROM demo.orders
GROUP BY customer_id
ORDER BY customer_id;
