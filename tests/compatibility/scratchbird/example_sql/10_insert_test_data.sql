-- Chain step: deterministic test-data insert

DELETE FROM demo.orders WHERE order_id BETWEEN 1001 AND 1003;
DELETE FROM demo.customers WHERE customer_id = 1001;

INSERT INTO demo.customers (customer_id, customer_name, customer_tier, active)
VALUES (1001, 'Test Customer', 'qa', TRUE);

INSERT INTO demo.orders (order_id, customer_id, order_total, order_status) VALUES
    (1001, 1001, 12.34, 'paid'),
    (1002, 1001, 56.78, 'pending'),
    (1003, 1001, 90.12, 'paid');

SELECT customer_id, customer_name, customer_tier, active
FROM demo.customers
WHERE customer_id = 1001;
