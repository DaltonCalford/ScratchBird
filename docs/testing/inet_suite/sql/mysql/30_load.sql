-- MySQL bulk load

USE sb_grind_mysql;

CALL sb_seed_customers(1000000);
CALL sb_seed_orders(1000000);
CALL sb_seed_order_items(1000000);
CALL sb_seed_events(1000000);

