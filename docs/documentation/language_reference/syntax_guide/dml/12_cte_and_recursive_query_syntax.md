# Common Table Expressions (CTEs)

[Prev](./11_returning_and_output_surfaces.md) | [Next](./README.md) | [Topic README](./README.md) | [DML README](./README.md) | [Syntax Guide README](../README.md)

## Synopsis

CTEs (WITH clauses) provide a way to write auxiliary statements for use in larger queries. They improve readability and enable recursive queries.

## Syntax

```sql
WITH [ RECURSIVE ] cte_name [ ( column_name [, ...] ) ] AS ( cte_query ) [, ...]
SELECT ...

-- Recursive CTE
WITH RECURSIVE cte_name AS (
    -- Non-recursive term (anchor)
    cte_query
    
    UNION [ ALL ]
    
    -- Recursive term
    cte_query  -- References cte_name
)
SELECT ...
```

## Basic CTEs

### Simple CTE

```sql
WITH active_users AS (
    SELECT * FROM users WHERE status = 'active'
)
SELECT * FROM active_users WHERE created_at > '2024-01-01';
```

### Multiple CTEs

```sql
WITH 
    monthly_sales AS (
        SELECT 
            DATE_TRUNC('month', date) as month,
            SUM(amount) as total
        FROM sales
        GROUP BY 1
    ),
    top_months AS (
        SELECT * FROM monthly_sales 
        ORDER BY total DESC 
        LIMIT 3
    )
SELECT * FROM top_months;
```

### CTE with Column Aliases

```sql
WITH sales_stats (month, total, avg_daily) AS (
    SELECT 
        DATE_TRUNC('month', date),
        SUM(amount),
        AVG(amount)
    FROM sales
    GROUP BY 1
)
SELECT * FROM sales_stats;
```

## Recursive CTEs

### Hierarchical Data

```sql
-- Employee hierarchy
WITH RECURSIVE employee_hierarchy AS (
    -- Anchor: top-level employees
    SELECT id, name, manager_id, 0 as level
    FROM employees
    WHERE manager_id IS NULL
    
    UNION ALL
    
    -- Recursive: employees with managers
    SELECT e.id, e.name, e.manager_id, eh.level + 1
    FROM employees e
    JOIN employee_hierarchy eh ON e.manager_id = eh.id
)
SELECT * FROM employee_hierarchy ORDER BY level, name;
```

### Bill of Materials

```sql
-- Explode product components
WITH RECURSIVE components AS (
    -- Base components
    SELECT 
        product_id,
        component_id,
        quantity
    FROM product_components
    WHERE product_id = 1
    
    UNION ALL
    
    -- Sub-components
    SELECT 
        c.product_id,
        pc.component_id,
        c.quantity * pc.quantity
    FROM components c
    JOIN product_components pc ON c.component_id = pc.product_id
)
SELECT * FROM components;
```

### Path Traversal

```sql
-- Find all paths between nodes
WITH RECURSIVE paths AS (
    SELECT 
        from_node,
        to_node,
        ARRAY[from_node, to_node] as path,
        1 as length
    FROM edges
    WHERE from_node = 'A'
    
    UNION ALL
    
    SELECT 
        p.from_node,
        e.to_node,
        p.path || e.to_node,
        p.length + 1
    FROM paths p
    JOIN edges e ON p.to_node = e.from_node
    WHERE p.length < 10  -- Prevent infinite loops
        AND NOT e.to_node = ANY(p.path)  -- Avoid cycles
)
SELECT * FROM paths WHERE to_node = 'Z';
```

### Date Series

```sql
-- Generate date series
WITH RECURSIVE dates AS (
    SELECT '2024-01-01'::date as date
    
    UNION ALL
    
    SELECT date + 1
    FROM dates
    WHERE date < '2024-12-31'
)
SELECT * FROM dates;
```

## CTEs with DML

### INSERT with CTE

```sql
WITH new_user AS (
    INSERT INTO users (name, email) 
    VALUES ('John', 'john@example.com')
    RETURNING id
)
INSERT INTO user_profiles (user_id, bio)
SELECT id, 'New user' FROM new_user;
```

### UPDATE with CTE

```sql
WITH updated AS (
    UPDATE products 
    SET price = price * 0.9
    WHERE category = 'clearance'
    RETURNING id, name, price as new_price
)
INSERT INTO price_changes (product_id, old_price, new_price)
SELECT u.id, p.price, u.new_price
FROM updated u
JOIN products p ON u.id = p.id;
```

### DELETE with CTE

```sql
WITH deleted AS (
    DELETE FROM old_logs 
    WHERE created_at < NOW() - INTERVAL '1 year'
    RETURNING *
)
INSERT INTO archive_logs
SELECT * FROM deleted;
```

## Advanced Patterns

### Pagination with CTE

```sql
WITH ranked AS (
    SELECT 
        *,
        ROW_NUMBER() OVER (ORDER BY score DESC) as rn
    FROM players
)
SELECT * FROM ranked 
WHERE rn BETWEEN 21 AND 40;  -- Page 2, 20 per page
```

### Running Totals

```sql
WITH RECURSIVE running AS (
    SELECT 
        date,
        amount,
        amount as total
    FROM sales
    ORDER BY date
    LIMIT 1
    
    UNION ALL
    
    SELECT 
        s.date,
        s.amount,
        r.total + s.amount
    FROM running r
    JOIN sales s ON s.date > r.date
    ORDER BY s.date
    LIMIT 1
)
SELECT * FROM running;
```

### Cycle Detection

```sql
WITH RECURSIVE search_tree AS (
    SELECT 
        id,
        parent_id,
        1 as depth,
        ARRAY[id] as path,
        FALSE as cycle
    FROM tree
    WHERE parent_id IS NULL
    
    UNION ALL
    
    SELECT 
        t.id,
        t.parent_id,
        st.depth + 1,
        st.path || t.id,
        t.id = ANY(st.path)
    FROM tree t
    JOIN search_tree st ON t.parent_id = st.id
    WHERE NOT st.cycle
)
SELECT * FROM search_tree;
```

## Performance Tips

1. **Materialized CTEs**: Use `MATERIALIZED` to prevent inline expansion
   ```sql
   WITH cte AS MATERIALIZED (SELECT ...)
   ```

2. **Recursive Limits**: Always set a depth limit on recursive CTEs
   ```sql
   WHERE depth < 100
   ```

3. **Indexing**: Ensure recursive join columns are indexed

## See Also

- [SELECT](01_select_core_syntax.md)
- [INSERT](07_insert_syntax.md)
- [UPDATE](08_update_syntax.md)
- [DELETE](09_delete_syntax.md)
