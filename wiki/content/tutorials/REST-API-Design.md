# REST API Design

**Last Updated:** 2026-01-28

---

## Overview

This guide covers best practices for designing REST APIs backed by ScratchBird. You'll learn how to structure your database schema, write efficient queries, handle transactions, and implement security patterns.

**What you'll learn:**
- Database schema design for REST APIs
- Query optimization strategies
- Transaction patterns
- Security implementation
- Pagination and filtering

---

## Part 1: Schema Design Principles

### Resource-Oriented Tables

Design tables that map cleanly to REST resources:

```sql
-- Good: Clear resource mapping
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    email VARCHAR(100) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE posts (
    id SERIAL PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id),
    title VARCHAR(200) NOT NULL,
    content TEXT,
    status VARCHAR(20) DEFAULT 'draft',
    published_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### Consistent Naming Conventions

| Convention | Example | Notes |
|------------|---------|-------|
| Table names | `users`, `blog_posts` | Plural, snake_case |
| Primary keys | `id` | Simple, consistent |
| Foreign keys | `user_id`, `post_id` | `{table}_id` pattern |
| Timestamps | `created_at`, `updated_at` | Standard audit fields |
| Status fields | `status`, `state` | Use CHECK constraints |

### Use Appropriate Data Types

```sql
-- IDs: Use SERIAL or BIGSERIAL
id SERIAL PRIMARY KEY,
id BIGSERIAL PRIMARY KEY,  -- For high-volume tables

-- Text: Choose appropriate length
username VARCHAR(50),       -- Known max length
email VARCHAR(255),         -- Standard email max
content TEXT,               -- Unlimited text

-- Numbers: Match your domain
price DECIMAL(10, 2),       -- Currency
quantity INTEGER,           -- Whole numbers
rating DECIMAL(2, 1),       -- 0.0 to 9.9

-- Dates: Use appropriate precision
birth_date DATE,            -- Date only
created_at TIMESTAMP,       -- Date and time
expires_at TIMESTAMPTZ,     -- With timezone
```

### Add Constraints for Data Integrity

```sql
CREATE TABLE orders (
    id SERIAL PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id),
    status VARCHAR(20) NOT NULL DEFAULT 'pending'
        CHECK (status IN ('pending', 'confirmed', 'shipped', 'delivered', 'cancelled')),
    total DECIMAL(10, 2) NOT NULL CHECK (total >= 0),
    item_count INTEGER NOT NULL CHECK (item_count > 0),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    -- Composite constraints
    CONSTRAINT valid_order CHECK (
        (status = 'cancelled') OR (total > 0)
    )
);
```

---

## Part 2: Index Strategy

### Index Primary Access Patterns

```sql
-- Foreign key indexes (for joins)
CREATE INDEX idx_posts_user_id ON posts(user_id);
CREATE INDEX idx_comments_post_id ON comments(post_id);

-- Status/filter indexes
CREATE INDEX idx_posts_status ON posts(status);
CREATE INDEX idx_orders_status ON orders(status);

-- Date range queries
CREATE INDEX idx_posts_published_at ON posts(published_at);
CREATE INDEX idx_orders_created_at ON orders(created_at);
```

### Composite Indexes for Common Queries

```sql
-- For: GET /users/:id/posts?status=published
CREATE INDEX idx_posts_user_status ON posts(user_id, status);

-- For: GET /posts?status=published&order=date
CREATE INDEX idx_posts_status_published ON posts(status, published_at DESC);

-- For: GET /orders?user_id=X&status=Y
CREATE INDEX idx_orders_user_status ON orders(user_id, status);
```

### Partial Indexes for Filtered Queries

```sql
-- Only index active records
CREATE INDEX idx_users_active ON users(email)
    WHERE status = 'active';

-- Only index published posts
CREATE INDEX idx_posts_published ON posts(published_at DESC)
    WHERE status = 'published';

-- Only index open orders
CREATE INDEX idx_orders_open ON orders(created_at)
    WHERE status NOT IN ('delivered', 'cancelled');
```

---

## Part 3: Query Patterns

### List Resources (GET /resources)

```sql
-- Basic list with pagination
SELECT id, title, status, created_at
FROM posts
WHERE user_id = $1
ORDER BY created_at DESC
LIMIT $2 OFFSET $3;

-- With total count for pagination headers
WITH filtered AS (
    SELECT *
    FROM posts
    WHERE user_id = $1
)
SELECT
    (SELECT COUNT(*) FROM filtered) AS total_count,
    f.*
FROM filtered f
ORDER BY created_at DESC
LIMIT $2 OFFSET $3;
```

### Get Single Resource (GET /resources/:id)

```sql
-- With related data
SELECT
    p.id,
    p.title,
    p.content,
    p.status,
    p.created_at,
    u.username AS author_username,
    u.id AS author_id,
    (SELECT COUNT(*) FROM comments WHERE post_id = p.id) AS comment_count
FROM posts p
JOIN users u ON p.user_id = u.id
WHERE p.id = $1;
```

### Create Resource (POST /resources)

```sql
-- Insert with returning
INSERT INTO posts (user_id, title, content, status)
VALUES ($1, $2, $3, $4)
RETURNING *;

-- Or return specific fields
INSERT INTO posts (user_id, title, content, status)
VALUES ($1, $2, $3, $4)
RETURNING id, title, status, created_at;
```

### Update Resource (PUT/PATCH /resources/:id)

```sql
-- Full update (PUT)
UPDATE posts
SET title = $1,
    content = $2,
    status = $3,
    updated_at = CURRENT_TIMESTAMP
WHERE id = $4
RETURNING *;

-- Partial update (PATCH) - build dynamically
UPDATE posts
SET title = COALESCE($1, title),
    content = COALESCE($2, content),
    updated_at = CURRENT_TIMESTAMP
WHERE id = $3
RETURNING *;
```

### Delete Resource (DELETE /resources/:id)

```sql
-- Soft delete (recommended)
UPDATE posts
SET status = 'deleted',
    deleted_at = CURRENT_TIMESTAMP
WHERE id = $1
RETURNING id;

-- Hard delete
DELETE FROM posts
WHERE id = $1
RETURNING id;
```

---

## Part 4: Filtering and Search

### Query Parameter Filtering

Map query parameters to WHERE clauses:

```
GET /posts?status=published&user_id=5&created_after=2026-01-01
```

```sql
SELECT *
FROM posts
WHERE 1=1
    AND ($1::text IS NULL OR status = $1)
    AND ($2::int IS NULL OR user_id = $2)
    AND ($3::date IS NULL OR created_at >= $3)
ORDER BY created_at DESC
LIMIT $4 OFFSET $5;
```

### Text Search

```sql
-- Simple LIKE search
SELECT *
FROM posts
WHERE title ILIKE '%' || $1 || '%'
   OR content ILIKE '%' || $1 || '%'
ORDER BY created_at DESC;

-- Full-text search (if available)
SELECT *
FROM posts
WHERE to_tsvector('english', title || ' ' || content)
    @@ plainto_tsquery('english', $1)
ORDER BY ts_rank(to_tsvector('english', title || ' ' || content),
                 plainto_tsquery('english', $1)) DESC;
```

### Range Queries

```sql
-- Date range
SELECT *
FROM orders
WHERE created_at BETWEEN $1 AND $2;

-- Numeric range
SELECT *
FROM products
WHERE price >= $1 AND price <= $2;
```

---

## Part 5: Pagination Strategies

### Offset-Based Pagination

Simple but can be slow for large offsets:

```sql
-- Page 3, 20 items per page
SELECT *
FROM posts
WHERE status = 'published'
ORDER BY created_at DESC
LIMIT 20 OFFSET 40;  -- (page - 1) * limit
```

Response headers:
```
X-Total-Count: 1234
X-Page: 3
X-Per-Page: 20
X-Total-Pages: 62
```

### Cursor-Based Pagination

Better performance for large datasets:

```sql
-- First page
SELECT id, title, created_at
FROM posts
WHERE status = 'published'
ORDER BY created_at DESC, id DESC
LIMIT 21;  -- Fetch one extra to detect more pages

-- Next page (using cursor from last item)
SELECT id, title, created_at
FROM posts
WHERE status = 'published'
    AND (created_at, id) < ($1, $2)  -- Cursor values
ORDER BY created_at DESC, id DESC
LIMIT 21;
```

Response format:
```json
{
  "data": [...],
  "pagination": {
    "has_next": true,
    "cursor": "eyJjcmVhdGVkX2F0IjoiMjAyNi0wMS0xOCIsImlkIjo0Mn0="
  }
}
```

### Keyset Pagination

For sorted results with unique ordering:

```sql
-- Using a compound sort key
SELECT *
FROM posts
WHERE (published_at, id) > ($1, $2)
ORDER BY published_at, id
LIMIT 20;
```

---

## Part 6: Transaction Patterns

### Single-Request Transactions

Most operations should be single statements:

```sql
-- Atomic insert with defaults
INSERT INTO orders (user_id, status, total)
VALUES ($1, 'pending', 0)
RETURNING *;
```

### Multi-Statement Transactions

For operations spanning multiple tables:

```python
# Python example
async def create_order_with_items(user_id, items):
    async with pool.acquire() as conn:
        async with conn.transaction():
            # Create order
            order = await conn.fetchrow("""
                INSERT INTO orders (user_id, status)
                VALUES ($1, 'pending')
                RETURNING *
            """, user_id)

            # Add items
            total = 0
            for item in items:
                await conn.execute("""
                    INSERT INTO order_items (order_id, product_id, quantity, price)
                    VALUES ($1, $2, $3, $4)
                """, order['id'], item['product_id'], item['quantity'], item['price'])
                total += item['quantity'] * item['price']

            # Update total
            await conn.execute("""
                UPDATE orders SET total = $1 WHERE id = $2
            """, total, order['id'])

            return order
```

### Optimistic Locking

Prevent lost updates in concurrent scenarios:

```sql
-- Add version column
ALTER TABLE posts ADD COLUMN version INTEGER DEFAULT 1;

-- Update with version check
UPDATE posts
SET title = $1,
    content = $2,
    version = version + 1,
    updated_at = CURRENT_TIMESTAMP
WHERE id = $3 AND version = $4
RETURNING *;

-- If no rows returned, version mismatch (409 Conflict)
```

---

## Part 7: Security Patterns

### Parameterized Queries

Always use parameterized queries to prevent SQL injection:

```python
# WRONG - SQL injection vulnerability
query = f"SELECT * FROM users WHERE id = {user_id}"

# CORRECT - Parameterized query
query = "SELECT * FROM users WHERE id = $1"
result = await conn.fetch(query, user_id)
```

### Row-Level Security

Implement tenant isolation:

```sql
-- Add tenant column
ALTER TABLE posts ADD COLUMN tenant_id INTEGER NOT NULL;

-- Create index
CREATE INDEX idx_posts_tenant ON posts(tenant_id);

-- Always filter by tenant in queries
SELECT *
FROM posts
WHERE tenant_id = $1  -- Current user's tenant
    AND id = $2;
```

### Column-Level Security

Hide sensitive data:

```sql
-- Create a view for API responses
CREATE VIEW users_public AS
SELECT id, username, created_at
FROM users;

-- Use view in API queries
SELECT * FROM users_public WHERE id = $1;
```

### Rate Limiting Table

Track API usage:

```sql
CREATE TABLE api_rate_limits (
    id SERIAL PRIMARY KEY,
    client_id VARCHAR(100) NOT NULL,
    endpoint VARCHAR(200) NOT NULL,
    request_count INTEGER DEFAULT 1,
    window_start TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (client_id, endpoint, window_start)
);

-- Check and increment
INSERT INTO api_rate_limits (client_id, endpoint, window_start)
VALUES ($1, $2, date_trunc('minute', CURRENT_TIMESTAMP))
ON CONFLICT (client_id, endpoint, window_start)
DO UPDATE SET request_count = api_rate_limits.request_count + 1
RETURNING request_count;
```

---

## Part 8: Error Handling

### Database Error to HTTP Status Mapping

| Database Error | HTTP Status | Response |
|----------------|-------------|----------|
| Unique violation | 409 Conflict | Resource already exists |
| Foreign key violation | 400 Bad Request | Referenced resource not found |
| Check constraint | 400 Bad Request | Invalid field value |
| Not null violation | 400 Bad Request | Required field missing |
| No rows affected | 404 Not Found | Resource not found |

### Error Response Format

```json
{
  "error": {
    "code": "RESOURCE_NOT_FOUND",
    "message": "Post with ID 123 not found",
    "details": {
      "resource": "post",
      "id": 123
    }
  }
}
```

### Validation Before Database

Validate input before hitting the database:

```python
from pydantic import BaseModel, validator

class CreatePost(BaseModel):
    title: str
    content: str
    status: str = "draft"

    @validator('title')
    def title_not_empty(cls, v):
        if not v.strip():
            raise ValueError('Title cannot be empty')
        if len(v) > 200:
            raise ValueError('Title too long (max 200 chars)')
        return v

    @validator('status')
    def valid_status(cls, v):
        valid = ['draft', 'published', 'archived']
        if v not in valid:
            raise ValueError(f'Status must be one of: {valid}')
        return v
```

---

## Part 9: Performance Tips

### Batch Operations

Insert multiple rows efficiently:

```sql
-- Single multi-row insert
INSERT INTO tags (name, post_id)
VALUES
    ($1, $4),
    ($2, $4),
    ($3, $4);

-- Using UNNEST for arrays
INSERT INTO tags (name, post_id)
SELECT unnest($1::text[]), $2;
```

### Avoid N+1 Queries

```sql
-- BAD: N+1 queries
-- 1. SELECT * FROM posts LIMIT 10
-- 2-11. SELECT * FROM users WHERE id = ? (for each post)

-- GOOD: Single query with JOIN
SELECT p.*, u.username AS author_username
FROM posts p
JOIN users u ON p.user_id = u.id
LIMIT 10;

-- Or batch load related data
SELECT * FROM users WHERE id = ANY($1);  -- Array of user IDs
```

### Use EXPLAIN for Query Analysis

```sql
EXPLAIN ANALYZE
SELECT p.*, u.username
FROM posts p
JOIN users u ON p.user_id = u.id
WHERE p.status = 'published'
ORDER BY p.created_at DESC
LIMIT 20;
```

Look for:
- Sequential scans on large tables (add index)
- High row estimates vs actual (update statistics)
- Nested loops with many iterations (consider join strategy)

### Connection Pooling

Always use connection pooling:

```python
# Python with asyncpg
pool = await asyncpg.create_pool(
    host='localhost',
    port=5432,
    database='myapp',
    user='admin',
    password='secret',
    min_size=5,
    max_size=20
)
```

---

## Part 10: API Design Checklist

### Resource Design

- [ ] Tables map to REST resources
- [ ] Consistent naming conventions
- [ ] Appropriate data types
- [ ] Constraints for data integrity
- [ ] Audit timestamps (created_at, updated_at)

### Performance

- [ ] Indexes on foreign keys
- [ ] Indexes on filter/sort columns
- [ ] Composite indexes for common queries
- [ ] Connection pooling configured
- [ ] Pagination implemented

### Security

- [ ] Parameterized queries only
- [ ] Input validation before database
- [ ] Tenant/user isolation
- [ ] Sensitive data filtered from responses
- [ ] Rate limiting in place

### Error Handling

- [ ] Database errors mapped to HTTP status
- [ ] Consistent error response format
- [ ] Validation errors are descriptive
- [ ] Not found vs forbidden distinction

---

## Example: Complete Resource Implementation

### Schema

```sql
CREATE TABLE articles (
    id SERIAL PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id),
    slug VARCHAR(100) NOT NULL UNIQUE,
    title VARCHAR(200) NOT NULL,
    content TEXT,
    status VARCHAR(20) DEFAULT 'draft'
        CHECK (status IN ('draft', 'published', 'archived')),
    published_at TIMESTAMP,
    view_count INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_articles_user ON articles(user_id);
CREATE INDEX idx_articles_status ON articles(status);
CREATE INDEX idx_articles_slug ON articles(slug);
CREATE INDEX idx_articles_published ON articles(published_at DESC)
    WHERE status = 'published';
```

### API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/articles` | List articles |
| GET | `/articles/:slug` | Get article by slug |
| POST | `/articles` | Create article |
| PATCH | `/articles/:slug` | Update article |
| DELETE | `/articles/:slug` | Delete article |
| POST | `/articles/:slug/publish` | Publish article |

### Query Examples

```sql
-- List published articles
SELECT a.id, a.slug, a.title, a.published_at,
       u.username AS author
FROM articles a
JOIN users u ON a.user_id = u.id
WHERE a.status = 'published'
ORDER BY a.published_at DESC
LIMIT $1 OFFSET $2;

-- Get by slug
SELECT a.*, u.username AS author
FROM articles a
JOIN users u ON a.user_id = u.id
WHERE a.slug = $1;

-- Create
INSERT INTO articles (user_id, slug, title, content)
VALUES ($1, $2, $3, $4)
RETURNING *;

-- Publish
UPDATE articles
SET status = 'published',
    published_at = CURRENT_TIMESTAMP,
    updated_at = CURRENT_TIMESTAMP
WHERE slug = $1 AND user_id = $2
RETURNING *;

-- Increment view count (fire-and-forget)
UPDATE articles
SET view_count = view_count + 1
WHERE slug = $1;
```

---

## See Also

- [Python Flask Tutorial](Web-App-Python-Flask.md)
- [Node.js Express Tutorial](Web-App-NodeJS-Express.md)
- [Basic SQL Guide](../getting-started/basic-sql.md)
- [Data Types Reference](../reference/Data-Types.md)

