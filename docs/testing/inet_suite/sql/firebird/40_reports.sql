-- Firebird reports

SELECT COUNT(*) AS customers_total FROM customers;
SELECT COUNT(*) AS orders_total FROM orders;
SELECT COUNT(*) AS items_total FROM order_items;
SELECT COUNT(*) AS events_total FROM event_log;

SELECT order_status, COUNT(*) AS cnt
FROM orders
GROUP BY order_status
ORDER BY cnt DESC;

SELECT status, COUNT(*) AS cnt
FROM customers
GROUP BY status
ORDER BY cnt DESC;

SELECT FIRST 10 * FROM v_order_summary ORDER BY order_date DESC;
