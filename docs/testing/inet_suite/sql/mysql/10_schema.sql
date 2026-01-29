-- MySQL schema (domains/sequences not supported in MySQL parser)

USE sb_grind_mysql;

CREATE TABLE customers (
    customer_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    customer_uuid UUID,
    email VARCHAR(255),
    full_name VARCHAR(200) NOT NULL,
    status VARCHAR(20),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP,
    is_active BOOLEAN DEFAULT true
);

CREATE TABLE orders (
    order_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    customer_id BIGINT NOT NULL,
    order_uuid UUID,
    order_total DECIMAL(12,2),
    order_status VARCHAR(20),
    order_date DATE DEFAULT CURRENT_DATE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    metadata JSON,
    notes TEXT
);

CREATE TABLE order_items (
    item_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    order_id BIGINT NOT NULL,
    sku VARCHAR(64) NOT NULL,
    qty INT NOT NULL,
    unit_price DECIMAL(12,2),
    line_total DECIMAL(12,2)
);

CREATE TABLE event_log (
    event_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    event_type VARCHAR(64) NOT NULL,
    event_ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    actor_id BIGINT,
    payload JSON
);

CREATE TABLE metric_samples (
    sample_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    sample_ts TIMESTAMP NOT NULL,
    cpu_pct FLOAT,
    mem_pct FLOAT,
    io_read BIGINT,
    io_write BIGINT
);

CREATE INDEX idx_customers_email ON customers (email);
CREATE INDEX idx_customers_status ON customers (status);
CREATE INDEX idx_orders_customer ON orders (customer_id);
CREATE INDEX idx_orders_status ON orders (order_status);
CREATE INDEX idx_orders_date ON orders (order_date);
CREATE INDEX idx_event_type ON event_log (event_type);
CREATE INDEX idx_metric_ts ON metric_samples (sample_ts);

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

