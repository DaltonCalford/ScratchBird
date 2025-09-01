# Phase 21: Advanced Features

## Objective
Implement advanced SQL features and optimizations.

## Prerequisites
- Phase 20 complete (backup/restore)

## Tasks

### 21.1 Window Functions
```sql
ROW_NUMBER() OVER (PARTITION BY ... ORDER BY ...)
RANK() OVER (...)
DENSE_RANK() OVER (...)
SUM() OVER (...)
LAG(column, offset) OVER (...)
LEAD(column, offset) OVER (...)
```

### 21.2 Common Table Expressions
```sql
WITH RECURSIVE cte AS (
    SELECT ...
    UNION ALL
    SELECT ... FROM cte WHERE ...
)
SELECT * FROM cte;
```

### 21.3 Triggers
```sql
CREATE TRIGGER trigger_name
BEFORE INSERT ON table
FOR EACH ROW
EXECUTE FUNCTION function_name();
```

### 21.4 Stored Procedures
```sql
CREATE PROCEDURE proc_name(param1 TYPE, param2 TYPE)
LANGUAGE SQL
AS $$
    -- procedure body
$$;
```

### 21.5 Views
```sql
CREATE VIEW view_name AS SELECT ...;
CREATE MATERIALIZED VIEW mv_name AS SELECT ...;
REFRESH MATERIALIZED VIEW mv_name;
```

## Files to Create/Modify
- `src/engine/window_functions.cpp`
- `src/engine/cte.cpp`
- `src/engine/triggers.cpp`
- `src/engine/procedures.cpp`
- `src/engine/views.cpp`

## Validation Tests
```cpp
// Window functions
auto result = execute(
    "SELECT id, ROW_NUMBER() OVER (ORDER BY id) as rn FROM users"
);

// CTE
result = execute(
    "WITH top_users AS (SELECT * FROM users LIMIT 10) "
    "SELECT * FROM top_users"
);

// Triggers
execute("CREATE TRIGGER audit_trigger ...");
execute("INSERT INTO users VALUES (...)");
// Verify trigger fired

// Views
execute("CREATE VIEW active_users AS SELECT * FROM users WHERE active = true");
result = execute("SELECT * FROM active_users");
```

## Exit Criteria
- Window functions calculate correctly
- CTEs work including recursive
- Triggers fire on events
- Views abstract underlying tables