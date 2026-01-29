-- Native schema: domains, sequences, tables, indexes, views

USE sb_grind_native;

-- Domains
CREATE DOMAIN dom_email AS VARCHAR(255)
    CHECK (POSITION('@' IN VALUE) > 1);

CREATE DOMAIN dom_money AS NUMERIC(12,2)
    DEFAULT 0
    CHECK (VALUE >= 0);

CREATE DOMAIN dom_status AS VARCHAR(20)
    DEFAULT 'NEW'
    CHECK (VALUE IN ('NEW','PENDING','ACTIVE','SUSPENDED','CLOSED'));

-- Sequences
CREATE SEQUENCE seq_customer START WITH 1 INCREMENT BY 1;
CREATE SEQUENCE seq_order START WITH 1 INCREMENT BY 1;
CREATE SEQUENCE seq_event START WITH 1 INCREMENT BY 1;

-- Tables
CREATE TABLE customers (
    customer_id BIGINT DEFAULT nextval('seq_customer') PRIMARY KEY,
    customer_uuid UUID DEFAULT UUID(),
    email dom_email,
    full_name VARCHAR(200) NOT NULL,
    status dom_status,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP,
    is_active BOOLEAN DEFAULT true
);

CREATE TABLE orders (
    order_id BIGINT DEFAULT nextval('seq_order') PRIMARY KEY,
    customer_id BIGINT NOT NULL,
    order_uuid UUID DEFAULT UUID(),
    order_total dom_money,
    order_status dom_status,
    order_date DATE DEFAULT CURRENT_DATE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    metadata JSON,
    notes TEXT
);

CREATE TABLE order_items (
    item_id BIGINT PRIMARY KEY,
    order_id BIGINT NOT NULL,
    sku VARCHAR(64) NOT NULL,
    qty INT NOT NULL,
    unit_price dom_money,
    line_total dom_money
);

CREATE TABLE event_log (
    event_id BIGINT DEFAULT nextval('seq_event') PRIMARY KEY,
    event_type VARCHAR(64) NOT NULL,
    event_ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    actor_id BIGINT,
    payload JSON
);

CREATE TABLE metric_samples (
    sample_id BIGINT PRIMARY KEY,
    sample_ts TIMESTAMP NOT NULL,
    cpu_pct FLOAT,
    mem_pct FLOAT,
    io_read BIGINT,
    io_write BIGINT
);

CREATE TABLE blob_store (
    blob_id BIGINT PRIMARY KEY,
    blob_name VARCHAR(128),
    blob_data BLOB
);

-- Indexes
CREATE INDEX idx_customers_email ON customers (email);
CREATE INDEX idx_customers_status ON customers (status);
CREATE INDEX idx_orders_customer ON orders (customer_id);
CREATE INDEX idx_orders_status ON orders (order_status);
CREATE INDEX idx_orders_date ON orders (order_date);
CREATE INDEX idx_event_type ON event_log (event_type);
CREATE INDEX idx_metric_ts ON metric_samples (sample_ts);

-- Views
CREATE VIEW v_order_summary AS
SELECT
    o.order_id,
    o.customer_id,
    o.order_total,
    o.order_status,
    o.order_date,
    c.email,
    c.full_name
FROM orders o
JOIN customers c ON c.customer_id = o.customer_id;

