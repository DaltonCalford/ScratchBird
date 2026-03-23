-- Chain step: readback validation for seeded/test data
SET SCHEMA users.public;

SELECT COUNT(*) AS customer_count FROM customers;
SELECT COUNT(*) AS order_count FROM orders;

SELECT order_status, COUNT(*) AS status_count
FROM orders
GROUP BY order_status
ORDER BY order_status;

SELECT customer_id, SUM(order_total) AS total_spend
FROM orders
GROUP BY customer_id
ORDER BY customer_id;
