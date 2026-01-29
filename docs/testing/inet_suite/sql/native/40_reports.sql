-- Native reports / sanity checks

USE sb_grind_native;

SELECT COUNT(*) AS customers_total FROM customers;
SELECT COUNT(*) AS orders_total FROM orders;
SELECT COUNT(*) AS items_total FROM order_items;
SELECT COUNT(*) AS events_total FROM event_log;
SELECT COUNT(*) AS samples_total FROM metric_samples;

SELECT order_status, COUNT(*) AS cnt
FROM orders
GROUP BY order_status
ORDER BY cnt DESC;

SELECT status, COUNT(*) AS cnt
FROM customers
GROUP BY status
ORDER BY cnt DESC;

SELECT * FROM v_order_summary ORDER BY order_date DESC LIMIT 10;

