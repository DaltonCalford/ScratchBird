# Phase 18: Permissions and Access Control

## Objective
Implement permission system for database objects.

## Prerequisites
- Phase 17 complete (authentication)

## Tasks

### 18.1 Permission Types
```cpp
enum Permission {
    SELECT,
    INSERT,
    UPDATE,
    DELETE,
    CREATE,
    DROP,
    ALL
};
```

### 18.2 GRANT/REVOKE
```sql
GRANT SELECT, INSERT ON table TO user;
REVOKE UPDATE ON table FROM user;
GRANT ALL ON DATABASE dbname TO user;
```

### 18.3 Permission Storage
```sql
SDB$PERMISSIONS (
    object_type,
    object_id,
    grantee,
    permission,
    grantor,
    grant_option
)
```

### 18.4 Permission Checking
- Check before every operation
- Hierarchical permissions (database > schema > table)
- Owner has implicit permissions

### 18.5 Roles
```sql
CREATE ROLE role_name;
GRANT role_name TO user;
GRANT permissions TO role_name;
```

## Files to Create/Modify
- `include/scratchbird/engine/permissions.h`
- `src/engine/permission_manager.cpp`

## Validation Tests
```cpp
// Setup users
execute("CREATE USER owner");
execute("CREATE USER reader");
execute("CREATE USER writer");

// Create table as owner
set_user("owner");
execute("CREATE TABLE data (id INTEGER, value TEXT)");

// Grant permissions
execute("GRANT SELECT ON data TO reader");
execute("GRANT INSERT, UPDATE ON data TO writer");

// Test reader
set_user("reader");
auto result = execute("SELECT * FROM data");  // OK
result = execute("INSERT INTO data VALUES (1, 'test')");
assert(result.status == StatusCode::PermissionDenied);

// Test writer
set_user("writer");
result = execute("INSERT INTO data VALUES (1, 'test')");  // OK
result = execute("DROP TABLE data");
assert(result.status == StatusCode::PermissionDenied);

// Test roles
execute("CREATE ROLE analysts");
execute("GRANT SELECT ON ALL TABLES TO analysts");
execute("GRANT analysts TO reader");
```

## Exit Criteria
- Permissions enforced on all operations
- GRANT/REVOKE work correctly
- Roles simplify permission management