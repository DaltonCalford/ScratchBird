# Tutorial: Web Application Backend

Build a database backend for a web application.

[Back to Getting Started](../index.md) | [Back to Documentation Index](../../index.md)

---

## What You'll Build

A complete database schema for a web application with:
- User authentication
- Product catalog
- Shopping cart
- Order management

---

## Prerequisites

- ScratchBird installed and running
- Basic SQL knowledge ([Basic SQL](../basic-sql.md))
- A database created for this tutorial

---

## Setup

```bash
# Connect to ScratchBird
sb_isql -H localhost -P 3092 -U admin

# Create database
sb_isql> CREATE DATABASE webshop;
sb_isql> \c webshop
```

---

## Step 1: User Management

### Users Table

```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    email VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    name VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP,
    is_active BOOLEAN DEFAULT TRUE,
    is_admin BOOLEAN DEFAULT FALSE
);

-- Index for email lookups (login)
CREATE INDEX idx_users_email ON users(email);

-- Index for active users
CREATE INDEX idx_users_active ON users(is_active) WHERE is_active = TRUE;
```

### User Sessions

```sql
CREATE TABLE sessions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
    token VARCHAR(255) UNIQUE NOT NULL,
    expires_at TIMESTAMP NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    ip_address VARCHAR(45),
    user_agent TEXT
);

CREATE INDEX idx_sessions_token ON sessions(token);
CREATE INDEX idx_sessions_user ON sessions(user_id);
CREATE INDEX idx_sessions_expires ON sessions(expires_at);
```

### Sample Users

```sql
-- Password hash would normally come from your app
INSERT INTO users (email, password_hash, name, is_admin)
VALUES
    ('admin@shop.com', '$argon2id$...', 'Admin User', TRUE),
    ('alice@example.com', '$argon2id$...', 'Alice Smith', FALSE),
    ('bob@example.com', '$argon2id$...', 'Bob Jones', FALSE);
```

---

## Step 2: Product Catalog

### Categories

```sql
CREATE TABLE categories (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    slug VARCHAR(100) UNIQUE NOT NULL,
    description TEXT,
    parent_id INTEGER REFERENCES categories(id),
    sort_order INTEGER DEFAULT 0,
    is_active BOOLEAN DEFAULT TRUE
);

CREATE INDEX idx_categories_parent ON categories(parent_id);
CREATE INDEX idx_categories_slug ON categories(slug);
```

### Products

```sql
CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    sku VARCHAR(50) UNIQUE NOT NULL,
    name VARCHAR(200) NOT NULL,
    slug VARCHAR(200) UNIQUE NOT NULL,
    description TEXT,
    price DECIMAL(10,2) NOT NULL,
    compare_at_price DECIMAL(10,2),  -- Original price for sales
    cost DECIMAL(10,2),               -- Cost price
    quantity INTEGER DEFAULT 0,
    category_id INTEGER REFERENCES categories(id),
    is_active BOOLEAN DEFAULT TRUE,
    is_featured BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_products_category ON products(category_id);
CREATE INDEX idx_products_sku ON products(sku);
CREATE INDEX idx_products_slug ON products(slug);
CREATE INDEX idx_products_active ON products(is_active) WHERE is_active = TRUE;
CREATE INDEX idx_products_featured ON products(is_featured) WHERE is_featured = TRUE;
```

### Product Images

```sql
CREATE TABLE product_images (
    id SERIAL PRIMARY KEY,
    product_id INTEGER REFERENCES products(id) ON DELETE CASCADE,
    url VARCHAR(500) NOT NULL,
    alt_text VARCHAR(200),
    sort_order INTEGER DEFAULT 0,
    is_primary BOOLEAN DEFAULT FALSE
);

CREATE INDEX idx_product_images_product ON product_images(product_id);
```

### Sample Products

```sql
-- Categories
INSERT INTO categories (name, slug, description) VALUES
    ('Electronics', 'electronics', 'Electronic devices and accessories'),
    ('Clothing', 'clothing', 'Apparel and fashion'),
    ('Home & Garden', 'home-garden', 'Home improvement and garden');

-- Products
INSERT INTO products (sku, name, slug, description, price, quantity, category_id, is_featured)
VALUES
    ('ELEC-001', 'Wireless Headphones', 'wireless-headphones',
     'Premium wireless headphones with noise cancellation', 99.99, 50, 1, TRUE),
    ('ELEC-002', 'Smart Watch', 'smart-watch',
     'Fitness tracking smart watch', 199.99, 30, 1, TRUE),
    ('CLTH-001', 'Cotton T-Shirt', 'cotton-tshirt',
     '100% organic cotton t-shirt', 24.99, 100, 2, FALSE),
    ('HOME-001', 'Garden Tool Set', 'garden-tool-set',
     '5-piece stainless steel garden tool set', 49.99, 25, 3, FALSE);
```

---

## Step 3: Shopping Cart

### Cart Table

```sql
CREATE TABLE carts (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id) ON DELETE SET NULL,
    session_id VARCHAR(100),  -- For guest carts
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT cart_user_or_session
        CHECK (user_id IS NOT NULL OR session_id IS NOT NULL)
);

CREATE INDEX idx_carts_user ON carts(user_id);
CREATE INDEX idx_carts_session ON carts(session_id);
```

### Cart Items

```sql
CREATE TABLE cart_items (
    id SERIAL PRIMARY KEY,
    cart_id INTEGER REFERENCES carts(id) ON DELETE CASCADE,
    product_id INTEGER REFERENCES products(id) ON DELETE CASCADE,
    quantity INTEGER NOT NULL CHECK (quantity > 0),
    price_at_add DECIMAL(10,2) NOT NULL,  -- Price when added
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(cart_id, product_id)
);

CREATE INDEX idx_cart_items_cart ON cart_items(cart_id);
```

### Cart Functions

```sql
-- View for cart totals
CREATE VIEW cart_totals AS
SELECT
    c.id AS cart_id,
    c.user_id,
    COUNT(ci.id) AS item_count,
    SUM(ci.quantity) AS total_quantity,
    SUM(ci.quantity * ci.price_at_add) AS subtotal
FROM carts c
LEFT JOIN cart_items ci ON c.id = ci.cart_id
GROUP BY c.id, c.user_id;
```

---

## Step 4: Orders

### Orders Table

```sql
CREATE TABLE orders (
    id SERIAL PRIMARY KEY,
    order_number VARCHAR(20) UNIQUE NOT NULL,
    user_id INTEGER REFERENCES users(id),
    status VARCHAR(20) DEFAULT 'pending',
    subtotal DECIMAL(10,2) NOT NULL,
    tax DECIMAL(10,2) DEFAULT 0,
    shipping DECIMAL(10,2) DEFAULT 0,
    total DECIMAL(10,2) NOT NULL,
    shipping_name VARCHAR(100),
    shipping_address TEXT,
    shipping_city VARCHAR(100),
    shipping_state VARCHAR(50),
    shipping_zip VARCHAR(20),
    shipping_country VARCHAR(50) DEFAULT 'US',
    billing_same_as_shipping BOOLEAN DEFAULT TRUE,
    notes TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    shipped_at TIMESTAMP,
    delivered_at TIMESTAMP,
    CONSTRAINT valid_status CHECK (
        status IN ('pending', 'confirmed', 'processing', 'shipped', 'delivered', 'cancelled', 'refunded')
    )
);

CREATE INDEX idx_orders_user ON orders(user_id);
CREATE INDEX idx_orders_status ON orders(status);
CREATE INDEX idx_orders_number ON orders(order_number);
CREATE INDEX idx_orders_created ON orders(created_at);
```

### Order Items

```sql
CREATE TABLE order_items (
    id SERIAL PRIMARY KEY,
    order_id INTEGER REFERENCES orders(id) ON DELETE CASCADE,
    product_id INTEGER REFERENCES products(id),
    product_name VARCHAR(200) NOT NULL,  -- Snapshot at order time
    product_sku VARCHAR(50) NOT NULL,
    quantity INTEGER NOT NULL,
    unit_price DECIMAL(10,2) NOT NULL,
    total_price DECIMAL(10,2) NOT NULL
);

CREATE INDEX idx_order_items_order ON order_items(order_id);
```

### Order Number Generation

```sql
-- Function to generate order number
CREATE OR REPLACE FUNCTION generate_order_number()
RETURNS VARCHAR(20) AS $$
DECLARE
    new_number VARCHAR(20);
BEGIN
    new_number := 'ORD-' || TO_CHAR(CURRENT_DATE, 'YYYYMMDD') || '-' ||
                  LPAD(nextval('order_number_seq')::TEXT, 5, '0');
    RETURN new_number;
END;
$$ LANGUAGE plpgsql;

CREATE SEQUENCE order_number_seq;
```

---

## Step 5: Useful Views

### Product Listing View

```sql
CREATE VIEW product_listing AS
SELECT
    p.id,
    p.sku,
    p.name,
    p.slug,
    p.description,
    p.price,
    p.compare_at_price,
    p.quantity,
    p.is_active,
    p.is_featured,
    c.name AS category_name,
    c.slug AS category_slug,
    (SELECT url FROM product_images pi
     WHERE pi.product_id = p.id AND pi.is_primary = TRUE
     LIMIT 1) AS primary_image
FROM products p
LEFT JOIN categories c ON p.category_id = c.id
WHERE p.is_active = TRUE;
```

### Order Summary View

```sql
CREATE VIEW order_summary AS
SELECT
    o.id,
    o.order_number,
    o.status,
    o.total,
    o.created_at,
    u.name AS customer_name,
    u.email AS customer_email,
    COUNT(oi.id) AS item_count
FROM orders o
LEFT JOIN users u ON o.user_id = u.id
LEFT JOIN order_items oi ON o.id = oi.order_id
GROUP BY o.id, o.order_number, o.status, o.total, o.created_at,
         u.name, u.email;
```

---

## Step 6: Common Queries

### Product Search

```sql
-- Full-text search (if GIN index exists)
SELECT * FROM product_listing
WHERE name ILIKE '%wireless%' OR description ILIKE '%wireless%'
ORDER BY is_featured DESC, name
LIMIT 20;
```

### User Order History

```sql
SELECT
    order_number,
    status,
    total,
    created_at,
    (SELECT COUNT(*) FROM order_items WHERE order_id = orders.id) AS items
FROM orders
WHERE user_id = 1
ORDER BY created_at DESC;
```

### Low Stock Alert

```sql
SELECT sku, name, quantity
FROM products
WHERE quantity < 10 AND is_active = TRUE
ORDER BY quantity ASC;
```

### Sales Report

```sql
SELECT
    DATE_TRUNC('day', created_at) AS date,
    COUNT(*) AS order_count,
    SUM(total) AS revenue
FROM orders
WHERE status NOT IN ('cancelled', 'refunded')
  AND created_at >= CURRENT_DATE - INTERVAL '30 days'
GROUP BY DATE_TRUNC('day', created_at)
ORDER BY date DESC;
```

---

## Step 7: Security Considerations

### Row-Level Security

```sql
-- Enable RLS
ALTER TABLE orders ENABLE ROW LEVEL SECURITY;

-- Users can only see their own orders
CREATE POLICY orders_user_policy ON orders
    FOR ALL
    TO app_user
    USING (user_id = current_user_id());
```

### Input Validation

Always use parameterized queries in your application:

```python
# Good - parameterized
cursor.execute("SELECT * FROM users WHERE email = %s", (email,))

# Bad - SQL injection vulnerable
cursor.execute(f"SELECT * FROM users WHERE email = '{email}'")
```

---

## Summary

You've created a complete e-commerce database schema with:

- User authentication and sessions
- Product catalog with categories
- Shopping cart functionality
- Order management
- Useful views and queries

### Files Created

| Table | Purpose |
|-------|---------|
| `users` | User accounts |
| `sessions` | Login sessions |
| `categories` | Product categories |
| `products` | Product catalog |
| `product_images` | Product images |
| `carts` | Shopping carts |
| `cart_items` | Cart contents |
| `orders` | Customer orders |
| `order_items` | Order line items |

---

## Next Steps

1. Connect your application framework
2. Add [authentication](../../admin/security.md)
3. Set up [backups](../../admin/backup-restore.md)
4. Configure [monitoring](../../admin/monitoring.md)
