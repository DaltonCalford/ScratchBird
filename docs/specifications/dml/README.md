# DML (Data Manipulation Language) Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains Data Manipulation Language (DML) specifications for querying and modifying data in ScratchBird databases.

## Overview

DML statements operate on table data, including SELECT queries, INSERT, UPDATE, DELETE, and MERGE operations. ScratchBird supports advanced DML features like CTEs, window functions, recursive queries, and XML/JSON table functions.

## Specifications in this Directory

### Overview

- **[04_DML_STATEMENTS_OVERVIEW.md](04_DML_STATEMENTS_OVERVIEW.md)** - Overview of all DML operations and syntax

### Core DML Operations

- **[DML_SELECT.md](DML_SELECT.md)** - SELECT queries
  - Basic SELECT
  - JOINs (INNER, LEFT, RIGHT, FULL, CROSS)
  - Subqueries (scalar, correlated, EXISTS)
  - CTEs (WITH clauses, recursive)
  - Window functions
  - Set operations (UNION, INTERSECT, EXCEPT)
  - GROUP BY, HAVING, ORDER BY, LIMIT/OFFSET

- **[DML_INSERT.md](DML_INSERT.md)** - INSERT operations
  - Single-row insert
  - Multi-row insert
  - INSERT ... SELECT
  - INSERT ... ON CONFLICT (upsert)
  - RETURNING clause

- **[DML_UPDATE.md](DML_UPDATE.md)** - UPDATE operations
  - Single-table update
  - Multi-table update
  - UPDATE ... FROM (PostgreSQL-style)
  - Correlated update
  - RETURNING clause

- **[DML_DELETE.md](DML_DELETE.md)** - DELETE operations
  - Basic DELETE
  - DELETE ... USING (PostgreSQL-style)
  - Correlated delete
  - RETURNING clause

- **[DML_MERGE.md](DML_MERGE.md)** - MERGE operations (upsert)
  - MERGE INTO ... USING
  - WHEN MATCHED / WHEN NOT MATCHED
  - Complex merge logic

- **[DML_COPY.md](DML_COPY.md)** - COPY bulk load/export
  - COPY FROM / TO
  - CSV/TEXT/BINARY formats
  - STDIN/STDOUT streaming

### Advanced DML

- **[DML_XML_JSON_TABLES.md](DML_XML_JSON_TABLES.md)** - XML and JSON table functions
  - XMLTABLE()
  - JSON_TABLE()
  - Extract tabular data from XML/JSON documents

## DML Statement Examples

### SELECT with CTEs and Window Functions
```sql
WITH monthly_sales AS (
    SELECT
        product_id,
        DATE_TRUNC('month', order_date) AS month,
        SUM(amount) AS total_sales
    FROM orders
    GROUP BY product_id, month
)
SELECT
    product_id,
    month,
    total_sales,
    SUM(total_sales) OVER (PARTITION BY product_id ORDER BY month) AS running_total,
    LAG(total_sales, 1) OVER (PARTITION BY product_id ORDER BY month) AS prev_month_sales
FROM monthly_sales
ORDER BY product_id, month;
```

### INSERT with ON CONFLICT (Upsert)
```sql
INSERT INTO user_stats (user_id, login_count, last_login)
VALUES (123, 1, NOW())
ON CONFLICT (user_id) DO UPDATE SET
    login_count = user_stats.login_count + 1,
    last_login = EXCLUDED.last_login;
```

### UPDATE with FROM clause
```sql
UPDATE orders o
SET total_price = (
    SELECT SUM(quantity * unit_price)
    FROM order_items
    WHERE order_id = o.order_id
)
FROM customers c
WHERE o.customer_id = c.customer_id
  AND c.status = 'active';
```

### MERGE (Upsert)
```sql
MERGE INTO inventory i
USING (
    SELECT product_id, SUM(quantity) AS qty_sold
    FROM order_items
    WHERE order_date = CURRENT_DATE
    GROUP BY product_id
) AS daily_sales
ON i.product_id = daily_sales.product_id
WHEN MATCHED THEN
    UPDATE SET quantity = i.quantity - daily_sales.qty_sold
WHEN NOT MATCHED THEN
    INSERT (product_id, quantity) VALUES (daily_sales.product_id, -daily_sales.qty_sold);
```

### JSON_TABLE
```sql
SELECT jt.*
FROM orders,
     JSON_TABLE(
         order_data,
         '$.items[*]' COLUMNS(
             item_id INTEGER PATH '$.id',
             item_name VARCHAR(100) PATH '$.name',
             quantity INTEGER PATH '$.qty',
             price NUMERIC(10,2) PATH '$.price'
         )
     ) AS jt;
```

## MGA & MVCC Integration

All DML operations respect Multi-Generational Architecture:

- **SELECT** - Reads consistent snapshot based on transaction isolation level
- **INSERT** - Creates new record version with current transaction ID
- **UPDATE** - Creates new version, old version retained for concurrent readers
- **DELETE** - Marks record as deleted, actual removal deferred to garbage collection

See [Transaction System](../transaction/) for details on MGA transaction management.

## Query Optimization

All SELECT queries go through the query optimizer:

1. **Parse** - SQL text → AST
2. **Analyze** - Semantic analysis and validation
3. **Optimize** - Cost-based query optimization
4. **Generate** - SBLR bytecode generation
5. **Execute** - Bytecode interpretation

See [Query Optimization](../query/) for optimizer details.

## Multi-Dialect Support

ScratchBird supports DML from multiple dialects:

- **PostgreSQL** - Full PostgreSQL DML syntax
- **MySQL** - MySQL-specific syntax (LIMIT, INSERT IGNORE, etc.)
- **Firebird** - Firebird DML syntax (ROWS, FIRST/SKIP, etc.)
- **MSSQL** - SQL Server syntax (TOP, OUTPUT, etc.) (post-gold)

## Related Specifications

- [Parser](../parser/) - DML statement parsing
- [Query Optimization](../query/) - Query plan generation
- [Transaction System](../transaction/) - MVCC and isolation levels
- [SBLR Bytecode](../sblr/) - Execution bytecode
- [Indexes](../indexes/) - Index usage in queries
- [DDL Operations](../ddl/) - Schema definitions

## Critical Reading

Before working on DML implementation:

1. **MUST READ:** [../../../MGA_RULES.md](../../../MGA_RULES.md) - MGA architecture rules
2. **MUST READ:** [../../../IMPLEMENTATION_STANDARDS.md](../../../IMPLEMENTATION_STANDARDS.md) - Implementation standards
3. **READ FIRST:** [04_DML_STATEMENTS_OVERVIEW.md](04_DML_STATEMENTS_OVERVIEW.md) - DML overview
4. **READ:** [../transaction/TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md) - MGA snapshot visibility

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
