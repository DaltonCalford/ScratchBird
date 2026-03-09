# CREATE PROCEDURE

[Prev](./03_drop_function.md) | [Next](./05_alter_procedure.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md)

## Synopsis

Creates a procedure - similar to a function but can use transaction control and doesn't return a value.

## Syntax

```sql
CREATE [ OR REPLACE ] PROCEDURE procedure_name ( [ argmode ] [ argname ] argtype [ { DEFAULT | = } default_expr ] [, ...] )
    { LANGUAGE lang_name
      | TRANSFORM { FOR TYPE type_name } [, ... ]
      | [ EXTERNAL ] SECURITY INVOKER | [ EXTERNAL ] SECURITY DEFINER
      | SET configuration_parameter { TO value | = value | FROM CURRENT }
      | AS 'definition'
      | AS 'obj_file', 'link_symbol'
    } ...
```

## Function vs Procedure

| Aspect | Function | Procedure |
|--------|----------|-----------|
| Returns value | Yes | No |
| Transaction control | No | Yes (COMMIT/ROLLBACK) |
| Can be called in SQL | Yes | No (CALL only) |
| Use case | Calculations, queries | Business logic, ETL |

## Examples

### Basic Procedure

```sql
-- Simple procedure
CREATE PROCEDURE update_user_status(user_id UUID, new_status TEXT)
LANGUAGE SQL AS $$
    UPDATE users SET status = new_status WHERE id = user_id;
$$;

-- Call it
CALL update_user_status('550e8400...', 'active');
```

### Procedure with Transaction Control

```sql
-- Multi-step process with commits
CREATE PROCEDURE process_batch_orders()
LANGUAGE plpgsql AS $$
DECLARE
    order_rec RECORD;
BEGIN
    FOR order_rec IN SELECT * FROM pending_orders LOOP
        BEGIN
            -- Process order
            INSERT INTO processed_orders SELECT * FROM orders WHERE id = order_rec.id;
            DELETE FROM pending_orders WHERE id = order_rec.id;
            
            -- Commit each order separately
            COMMIT;
        EXCEPTION WHEN OTHERS THEN
            -- Log error and continue
            INSERT INTO error_log (order_id, error) VALUES (order_rec.id, SQLERRM);
            ROLLBACK;
        END;
    END LOOP;
END;
$$;
```

### ETL Procedure

```sql
CREATE PROCEDURE etl_daily_sales()
LANGUAGE plpgsql AS $$
BEGIN
    -- Truncate and load pattern
    TRUNCATE TABLE staging_sales;
    
    -- Load from external source
    COPY staging_sales FROM '/data/daily_sales.csv' WITH CSV;
    
    -- Transform and insert
    INSERT INTO sales_fact (date, product_id, amount)
    SELECT date, product_id, SUM(amount)
    FROM staging_sales
    GROUP BY date, product_id;
    
    COMMIT;
END;
$$;
```

### INOUT Parameters

```sql
CREATE PROCEDURE get_and_update_counter(
    IN counter_name TEXT,
    INOUT current_value INTEGER
)
LANGUAGE plpgsql AS $$
BEGIN
    SELECT value INTO current_value FROM counters WHERE name = counter_name;
    UPDATE counters SET value = value + 1 WHERE name = counter_name;
END;
$$;

-- Usage:
-- CALL get_and_update_counter('page_views', 0);
```

## See Also

- [CREATE FUNCTION](01_create_function.md)
- [CALL Statement](../../dml/README.md)
