-- ScratchBird example database bootstrap + seed data
-- This script is executed during dynamic/static example DB initialization.

CREATE USER sb_admin WITH PASSWORD 'SbAdmin_Compat1!' SUPERUSER;
CREATE USER pg_admin WITH PASSWORD 'PgAdmin_Compat1!' SUPERUSER;
CREATE USER root WITH PASSWORD 'RootCompat_1!' SUPERUSER;
CREATE USER SYSDBA WITH PASSWORD 'SysDbaCompat_1!' SUPERUSER;

-- NOTE:
-- SUPERUSER accounts already have full database access.
-- Keep bootstrap free of GRANT statements until the semantic-bridge
-- closure path for SBLR3_GRANT is fully enabled in all test profiles.

CREATE TABLE customers (
    customer_id INTEGER PRIMARY KEY,
    customer_name VARCHAR(96) NOT NULL,
    customer_tier VARCHAR(16) NOT NULL,
    active BOOLEAN NOT NULL
);

CREATE TABLE orders (
    order_id INTEGER PRIMARY KEY,
    customer_id INTEGER NOT NULL,
    order_total DECIMAL(12,2) NOT NULL,
    order_status VARCHAR(24) NOT NULL
);

INSERT INTO customers (customer_id, customer_name, customer_tier, active) VALUES
    (1, 'Alice Ng', 'gold', TRUE),
    (2, 'Bruno Hale', 'silver', TRUE),
    (3, 'Carmen Ives', 'bronze', FALSE),
    (4, 'Diego Wu', 'gold', TRUE);

INSERT INTO orders (order_id, customer_id, order_total, order_status) VALUES
    (101, 1, 120.50, 'paid'),
    (102, 1, 75.00, 'pending'),
    (103, 2, 225.20, 'paid'),
    (104, 4, 19.99, 'shipped');
