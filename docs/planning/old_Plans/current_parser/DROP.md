# DROP Commands - Comprehensive Reference

This document provides a complete audit of all DROP commands implemented in the ScratchBird parser.

## Overview

The ScratchBird parser supports DROP commands for the following object types:
- TABLE
- INDEX
- VIEW
- SEQUENCE
- TABLESPACE
- TRIGGER
- USER
- ROLE
- GROUP
- POLICY

## Parser Dispatch (lines 471-519)

Located in `parseStatement()`, the DROP keyword dispatcher checks the following token after DROP:

```cpp
if (match(TokenType::KW_DROP))
{
    if (check(TokenType::KW_TABLE))          -> parseDropTable()
    else if (check(TokenType::KW_INDEX))     -> parseDropIndex()
    else if (check(TokenType::KW_TABLESPACE)) -> parseDropTablespace()
    else if (check(TokenType::KW_SEQUENCE))  -> parseDropSequence()
    else if (check(TokenType::KW_VIEW))      -> parseDropView()
    else if (check(TokenType::KW_USER))      -> parseDropUser()
    else if (check(TokenType::KW_ROLE))      -> parseDropRole()
    else if (check(TokenType::KW_GROUP))     -> parseDropGroup()
    else if (check(TokenType::KW_POLICY))    -> parseDropPolicy()
    else if (check(TokenType::KW_TRIGGER))   -> parseDropTrigger()
}
```

**Note**: All DROP commands are fully dispatched including DROP TRIGGER (lines 517-519).

---

## 1. DROP TABLE

**Implementation**: `parseDropTable()` at line 5654

### BNF Syntax
```bnf
<drop_table_stmt> ::= DROP TABLE [ IF EXISTS ] <table_name> [ <drop_behavior> ]

<drop_behavior> ::= CASCADE | RESTRICT
```

### Description
Drops a table from the database. The IF EXISTS clause prevents errors if the table doesn't exist. CASCADE removes dependent objects (views, foreign keys, etc.), while RESTRICT (default) fails if dependencies exist.

### Components
- **IF EXISTS clause**: Supported (lines 5666-5675)
  - Keywords: `IF`, `EXISTS`
  - Prevents error if table does not exist

- **CASCADE/RESTRICT options**: Supported (lines 5687-5696)
  - Keywords: `CASCADE`, `RESTRICT`
  - Default: `RESTRICT`
  - Enum: `DropTableStmt::DropBehavior`
    - `RESTRICT`: Fail if dependencies exist (default)
    - `CASCADE`: Drop dependent objects recursively

### AST Node
- **Class**: `DropTableStmt` (ast.h line 1457)
- **Members**:
  - `table_name_`: StringPool::StringId
  - `if_exists_`: bool
  - `drop_behavior_`: DropBehavior enum (RESTRICT or CASCADE)

### Keywords Used
- `DROP` (TokenType::KW_DROP)
- `TABLE` (TokenType::KW_TABLE)
- `IF` (TokenType::KW_IF)
- `EXISTS` (TokenType::KW_EXISTS)
- `CASCADE` (TokenType::KW_CASCADE)
- `RESTRICT` (TokenType::KW_RESTRICT)

### Examples
```sql
-- Basic drop
DROP TABLE users;

-- Drop with IF EXISTS
DROP TABLE IF EXISTS users;

-- Drop with CASCADE
DROP TABLE users CASCADE;

-- Drop with RESTRICT (explicit)
DROP TABLE users RESTRICT;

-- Combined
DROP TABLE IF EXISTS users CASCADE;
```

---

## 2. DROP INDEX

**Implementation**: `parseDropIndex()` at line 5702

### BNF Syntax
```bnf
<drop_index_stmt> ::= DROP INDEX [ IF EXISTS ] <index_name> [ <drop_behavior> ]

<drop_behavior> ::= CASCADE | RESTRICT
```

### Description
Drops an index from the database. The IF EXISTS clause prevents errors if the index doesn't exist. CASCADE removes objects that depend on the index, while RESTRICT (default) fails if dependencies exist.

### Components
- **IF EXISTS clause**: Supported (lines 5714-5723)
  - Keywords: `IF`, `EXISTS`
  - Prevents error if index does not exist

- **CASCADE/RESTRICT options**: Supported (lines 5735-5744)
  - Keywords: `CASCADE`, `RESTRICT`
  - Default: `RESTRICT`
  - Enum: `DropIndexStmt::DropBehavior`
    - `RESTRICT`: Fail if dependencies exist (default)
    - `CASCADE`: Drop dependent objects recursively

### AST Node
- **Class**: `DropIndexStmt` (ast.h line 1495)
- **Members**:
  - `index_name_`: StringPool::StringId
  - `if_exists_`: bool
  - `drop_behavior_`: DropBehavior enum (RESTRICT or CASCADE)

### Keywords Used
- `DROP` (TokenType::KW_DROP)
- `INDEX` (TokenType::KW_INDEX)
- `IF` (TokenType::KW_IF)
- `EXISTS` (TokenType::KW_EXISTS)
- `CASCADE` (TokenType::KW_CASCADE)
- `RESTRICT` (TokenType::KW_RESTRICT)

### Examples
```sql
-- Basic drop
DROP INDEX idx_users_email;

-- Drop with IF EXISTS
DROP INDEX IF EXISTS idx_users_email;

-- Drop with CASCADE
DROP INDEX idx_users_email CASCADE;

-- Drop with RESTRICT
DROP INDEX idx_users_email RESTRICT;

-- Combined
DROP INDEX IF EXISTS idx_users_email CASCADE;
```

---

## 3. DROP SEQUENCE

**Implementation**: `parseDropSequence()` at line 5998

### BNF Syntax
```bnf
<drop_sequence_stmt> ::= DROP SEQUENCE [ IF EXISTS ] <sequence_name> [ <drop_behavior> ]

<drop_behavior> ::= CASCADE | RESTRICT
```

### Description
Drops a sequence from the database. The IF EXISTS clause prevents errors if the sequence doesn't exist. CASCADE removes objects that depend on the sequence (e.g., column defaults), while RESTRICT fails if dependencies exist.

### Components
- **IF EXISTS clause**: Supported (lines 6010-6019)
  - Keywords: `IF`, `EXISTS`
  - Prevents error if sequence does not exist

- **CASCADE/RESTRICT options**: Supported (lines 6032-6041)
  - Keywords: `CASCADE`, `RESTRICT`
  - Note: Implementation uses a boolean `cascade` flag instead of enum
  - Default: `false` (RESTRICT behavior)

### AST Node
- **Class**: `DropSequenceStmt` (ast.h line 1808)
- **Members**:
  - `name_`: StringPool::StringId
  - `if_exists_`: bool
  - `cascade_`: bool (true = CASCADE, false = RESTRICT)

### Keywords Used
- `DROP` (TokenType::KW_DROP)
- `SEQUENCE` (TokenType::KW_SEQUENCE)
- `IF` (TokenType::KW_IF)
- `EXISTS` (TokenType::KW_EXISTS)
- `CASCADE` (TokenType::KW_CASCADE)
- `RESTRICT` (TokenType::KW_RESTRICT)

### Examples
```sql
-- Basic drop
DROP SEQUENCE seq_order_id;

-- Drop with IF EXISTS
DROP SEQUENCE IF EXISTS seq_order_id;

-- Drop with CASCADE
DROP SEQUENCE seq_order_id CASCADE;

-- Drop with RESTRICT
DROP SEQUENCE seq_order_id RESTRICT;

-- Combined
DROP SEQUENCE IF EXISTS seq_order_id CASCADE;
```

---

## 4. DROP VIEW

**Implementation**: `parseDropView()` at line 6183

### BNF Syntax
```bnf
<drop_view_stmt> ::= DROP VIEW [ IF EXISTS ] <view_name> [ <drop_behavior> ]

<drop_behavior> ::= CASCADE | RESTRICT
```

### Description
Drops a view from the database. The IF EXISTS clause prevents errors if the view doesn't exist. CASCADE removes objects that depend on the view, while RESTRICT fails if dependencies exist.

### Components
- **IF EXISTS clause**: Supported (lines 6194-6204)
  - Keywords: `IF`, `EXISTS`
  - Prevents error if view does not exist

- **CASCADE/RESTRICT options**: Supported (lines 6217-6226)
  - Keywords: `CASCADE`, `RESTRICT`
  - Note: Implementation uses a boolean `cascade` flag instead of enum
  - Default: `false` (RESTRICT behavior)

### AST Node
- **Class**: `DropViewStmt` (ast.h line 1879)
- **Members**:
  - `name_`: StringPool::StringId
  - `if_exists_`: bool
  - `cascade_`: bool (true = CASCADE, false = RESTRICT)

### Keywords Used
- `DROP` (TokenType::KW_DROP)
- `VIEW` (TokenType::KW_VIEW)
- `IF` (TokenType::KW_IF)
- `EXISTS` (TokenType::KW_EXISTS)
- `CASCADE` (TokenType::KW_CASCADE)
- `RESTRICT` (TokenType::KW_RESTRICT)

### Examples
```sql
-- Basic drop
DROP VIEW active_users;

-- Drop with IF EXISTS
DROP VIEW IF EXISTS active_users;

-- Drop with CASCADE
DROP VIEW active_users CASCADE;

-- Drop with RESTRICT
DROP VIEW active_users RESTRICT;

-- Combined
DROP VIEW IF EXISTS active_users CASCADE;
```

---

## 5. DROP TABLESPACE

**Implementation**: `parseDropTablespace()` at line 6285

### BNF Syntax
```bnf
<drop_tablespace_stmt> ::= DROP TABLESPACE <tablespace_name> [ FORCE ]
```

### Description
Drops a tablespace from the database. The FORCE option allows dropping a tablespace even if it contains objects (which would normally prevent the drop).

### Components
- **IF EXISTS clause**: NOT SUPPORTED
  - No IF EXISTS support in this implementation

- **FORCE option**: Supported (lines 6306-6311)
  - Keyword: `FORCE`
  - Allows dropping tablespace with existing objects
  - Default: `false`

- **CASCADE/RESTRICT options**: NOT SUPPORTED
  - Uses FORCE instead of CASCADE/RESTRICT pattern

### AST Node
- **Class**: `DropTablespaceStmt` (ast.h line 2912)
- **Members**:
  - `tablespace_name_`: StringPool::StringId
  - `force_`: bool

### Keywords Used
- `DROP` (TokenType::KW_DROP)
- `TABLESPACE` (TokenType::KW_TABLESPACE)
- `FORCE` (TokenType::KW_FORCE)

### Examples
```sql
-- Basic drop
DROP TABLESPACE ts_archive;

-- Drop with FORCE
DROP TABLESPACE ts_archive FORCE;
```

### Notes
- Unlike other DROP commands, this does not support IF EXISTS
- Uses FORCE instead of CASCADE/RESTRICT pattern
- FORCE allows dropping tablespace even with existing objects

---

## 6. DROP TRIGGER

**Implementation**: `parseDropTrigger()` at line 1874

**STATUS**: Implemented but DISABLED in dispatch (lines 510-514)

### BNF Syntax
```bnf
<drop_trigger_stmt> ::= DROP TRIGGER [ IF EXISTS ] <trigger_name>
```

### Description
Drops a trigger from the database. The IF EXISTS clause prevents errors if the trigger doesn't exist.

### Components
- **IF EXISTS clause**: Commented as "future support" (lines 1886-1888)
  - Variable `if_exists` declared but always set to `false`
  - Comment: "For now, we'll skip IF EXISTS support"
  - Keywords: `IF`, `EXISTS` (not currently checked)

- **CASCADE/RESTRICT options**: NOT SUPPORTED
  - No CASCADE or RESTRICT support

### AST Node
- **Class**: `DropTriggerStmt` (ast.h line 3503)
- **Members**:
  - `trigger_name_`: StringPool::StringId
  - `if_exists_`: bool (currently always false)

### Keywords Used
- `DROP` (TokenType::KW_DROP)
- `TRIGGER` (TokenType::KW_TRIGGER)
- `IF` (TokenType::KW_IF) - planned but not implemented
- `EXISTS` (TokenType::KW_EXISTS) - planned but not implemented

### Examples
```sql
-- Basic drop (currently only supported syntax)
DROP TRIGGER log_insert;

-- IF EXISTS (not yet implemented)
-- DROP TRIGGER IF EXISTS log_insert;
```

### Notes
- Function is implemented but not called from dispatch (commented out)
- IF EXISTS support is planned but not yet implemented (lines 1886-1888)
- No CASCADE/RESTRICT support
- Comment indicates "Agent C" will add trigger support

---

## 7. DROP USER

**Implementation**: `parseDropUser()` at line 8478

### BNF Syntax
```bnf
<drop_user_stmt> ::= DROP USER [ IF EXISTS ] <username> [ <drop_behavior> ]

<drop_behavior> ::= CASCADE | RESTRICT
```

### Description
Drops a user from the database. The IF EXISTS clause prevents errors if the user doesn't exist. CASCADE transfers or drops objects owned by the user, while RESTRICT (default) fails if the user owns objects.

### Components
- **IF EXISTS clause**: Supported (lines 8489-8499)
  - Keywords: `IF`, `EXISTS`
  - Prevents error if user does not exist

- **CASCADE/RESTRICT options**: Supported (lines 8512-8521)
  - Keywords: `CASCADE`, `RESTRICT`
  - Default: `RESTRICT`
  - Enum: `DropUserStmt::DropBehavior`
    - `RESTRICT`: Fail if user owns objects
    - `CASCADE`: Drop user and transfer/drop owned objects

### AST Node
- **Class**: `DropUserStmt` (ast.h line 4019)
- **Members**:
  - `username_`: StringPool::StringId
  - `if_exists_`: bool
  - `drop_behavior_`: DropBehavior enum (RESTRICT or CASCADE)

### Keywords Used
- `DROP` (TokenType::KW_DROP)
- `USER` (TokenType::KW_USER)
- `IF` (TokenType::KW_IF)
- `EXISTS` (TokenType::KW_EXISTS)
- `CASCADE` (TokenType::KW_CASCADE)
- `RESTRICT` (TokenType::KW_RESTRICT)

### Examples
```sql
-- Basic drop
DROP USER alice;

-- Drop with IF EXISTS
DROP USER IF EXISTS alice;

-- Drop with CASCADE
DROP USER alice CASCADE;

-- Drop with RESTRICT
DROP USER alice RESTRICT;

-- Combined
DROP USER IF EXISTS alice CASCADE;
```

---

## 8. DROP ROLE

**Implementation**: `parseDropRole()` at line 8553

### BNF Syntax
```bnf
<drop_role_stmt> ::= DROP ROLE [ IF EXISTS ] <rolename> [ <drop_behavior> ]

<drop_behavior> ::= CASCADE | RESTRICT
```

### Description
Drops a role from the database. The IF EXISTS clause prevents errors if the role doesn't exist. CASCADE removes the role and revokes it from all users, while RESTRICT (default) fails if the role is granted to any users.

### Components
- **IF EXISTS clause**: Supported (lines 8564-8574)
  - Keywords: `IF`, `EXISTS`
  - Prevents error if role does not exist

- **CASCADE/RESTRICT options**: Supported (lines 8587-8596)
  - Keywords: `CASCADE`, `RESTRICT`
  - Default: `RESTRICT`
  - Enum: `DropRoleStmt::DropBehavior`
    - `RESTRICT`: Fail if role is granted to users
    - `CASCADE`: Drop role and revoke from all users

### AST Node
- **Class**: `DropRoleStmt` (ast.h line 4071)
- **Members**:
  - `rolename_`: StringPool::StringId
  - `if_exists_`: bool
  - `drop_behavior_`: DropBehavior enum (RESTRICT or CASCADE)

### Keywords Used
- `DROP` (TokenType::KW_DROP)
- `ROLE` (TokenType::KW_ROLE)
- `IF` (TokenType::KW_IF)
- `EXISTS` (TokenType::KW_EXISTS)
- `CASCADE` (TokenType::KW_CASCADE)
- `RESTRICT` (TokenType::KW_RESTRICT)

### Examples
```sql
-- Basic drop
DROP ROLE developer;

-- Drop with IF EXISTS
DROP ROLE IF EXISTS developer;

-- Drop with CASCADE
DROP ROLE developer CASCADE;

-- Drop with RESTRICT
DROP ROLE developer RESTRICT;

-- Combined
DROP ROLE IF EXISTS developer CASCADE;
```

---

## 9. DROP GROUP

**Implementation**: `parseDropGroup()` at line 8628

### BNF Syntax
```bnf
<drop_group_stmt> ::= DROP GROUP [ IF EXISTS ] <groupname> [ <drop_behavior> ]

<drop_behavior> ::= CASCADE | RESTRICT
```

### Description
Drops a group from the database. The IF EXISTS clause prevents errors if the group doesn't exist. CASCADE removes the group and its memberships, while RESTRICT (default) fails if the group has members.

### Components
- **IF EXISTS clause**: Supported (lines 8639-8649)
  - Keywords: `IF`, `EXISTS`
  - Prevents error if group does not exist

- **CASCADE/RESTRICT options**: Supported (lines 8662-8671)
  - Keywords: `CASCADE`, `RESTRICT`
  - Default: `RESTRICT`
  - Enum: `DropGroupStmt::DropBehavior`
    - `RESTRICT`: Fail if group has members
    - `CASCADE`: Drop group and remove all memberships

### AST Node
- **Class**: `DropGroupStmt` (ast.h line 4123)
- **Members**:
  - `groupname_`: StringPool::StringId
  - `if_exists_`: bool
  - `drop_behavior_`: DropBehavior enum (RESTRICT or CASCADE)

### Keywords Used
- `DROP` (TokenType::KW_DROP)
- `GROUP` (TokenType::KW_GROUP)
- `IF` (TokenType::KW_IF)
- `EXISTS` (TokenType::KW_EXISTS)
- `CASCADE` (TokenType::KW_CASCADE)
- `RESTRICT` (TokenType::KW_RESTRICT)

### Examples
```sql
-- Basic drop
DROP GROUP admins;

-- Drop with IF EXISTS
DROP GROUP IF EXISTS admins;

-- Drop with CASCADE
DROP GROUP admins CASCADE;

-- Drop with RESTRICT
DROP GROUP admins RESTRICT;

-- Combined
DROP GROUP IF EXISTS admins CASCADE;
```

---

## 10. DROP POLICY

**Implementation**: `parseDropPolicy()` at line 9515

### BNF Syntax
```bnf
<drop_policy_stmt> ::= DROP POLICY [ IF EXISTS ] <policy_name> ON <table_name> [ <drop_behavior> ]

<drop_behavior> ::= CASCADE | RESTRICT
```

### Description
Drops a Row Level Security (RLS) policy from a table. The IF EXISTS clause prevents errors if the policy doesn't exist. The ON clause is mandatory to specify which table the policy is on. CASCADE removes the policy and potentially dependent objects, while RESTRICT (default) fails if dependencies exist.

### Components
- **IF EXISTS clause**: Supported (lines 9526-9536)
  - Keywords: `IF`, `EXISTS`
  - Prevents error if policy does not exist

- **ON clause**: REQUIRED (lines 9548-9562)
  - Keyword: `ON`
  - Must specify table name after policy name
  - Format: `DROP POLICY policy_name ON table_name`

- **CASCADE/RESTRICT options**: Supported (lines 9564-9573)
  - Keywords: `CASCADE`, `RESTRICT`
  - Default: `RESTRICT`
  - Enum: `DropPolicyStmt::DropBehavior`
    - `RESTRICT`: Fail if dependencies exist
    - `CASCADE`: Drop policy and dependent objects

### AST Node
- **Class**: `DropPolicyStmt` (ast.h line 4502)
- **Members**:
  - `policy_name_`: StringPool::StringId
  - `table_name_`: StringPool::StringId
  - `if_exists_`: bool
  - `drop_behavior_`: DropBehavior enum (RESTRICT or CASCADE)

### Keywords Used
- `DROP` (TokenType::KW_DROP)
- `POLICY` (TokenType::KW_POLICY)
- `IF` (TokenType::KW_IF)
- `EXISTS` (TokenType::KW_EXISTS)
- `ON` (TokenType::KW_ON)
- `CASCADE` (TokenType::KW_CASCADE)
- `RESTRICT` (TokenType::KW_RESTRICT)

### Examples
```sql
-- Basic drop
DROP POLICY user_isolation ON users;

-- Drop with IF EXISTS
DROP POLICY IF EXISTS user_isolation ON users;

-- Drop with CASCADE
DROP POLICY user_isolation ON users CASCADE;

-- Drop with RESTRICT
DROP POLICY user_isolation ON users RESTRICT;

-- Combined
DROP POLICY IF EXISTS user_isolation ON users CASCADE;
```

### Notes
- Unique among DROP commands in requiring an ON clause
- Part of Row Level Security (RLS) feature set (Security Phase 3.4)
- Policy name alone is not sufficient; table name must also be specified

---

## Summary Table

| Command | IF EXISTS | CASCADE/RESTRICT | Other Options | AST Representation |
|---------|-----------|------------------|---------------|-------------------|
| DROP TABLE | Yes | Yes (enum) | - | DropBehavior enum |
| DROP INDEX | Yes | Yes (enum) | - | DropBehavior enum |
| DROP SEQUENCE | Yes | Yes (bool) | - | cascade bool |
| DROP VIEW | Yes | Yes (bool) | - | cascade bool |
| DROP TABLESPACE | **No** | **No** | FORCE | force bool |
| DROP TRIGGER | Planned* | **No** | - | if_exists bool (unused) |
| DROP USER | Yes | Yes (enum) | - | DropBehavior enum |
| DROP ROLE | Yes | Yes (enum) | - | DropBehavior enum |
| DROP GROUP | Yes | Yes (enum) | - | DropBehavior enum |
| DROP POLICY | Yes | Yes (enum) | ON clause (required) | DropBehavior enum |

*Planned but not yet implemented

## Implementation Patterns

### Pattern 1: Enum-based DropBehavior (Most Common)
Used by: TABLE, INDEX, USER, ROLE, GROUP, POLICY

```cpp
enum class DropBehavior : uint8_t {
    RESTRICT,  // Fail if dependencies exist (default)
    CASCADE    // Drop dependent objects recursively
};
```

Members:
- `StringPool::StringId name_`
- `bool if_exists_`
- `DropBehavior drop_behavior_`

### Pattern 2: Boolean cascade flag
Used by: SEQUENCE, VIEW

Members:
- `StringPool::StringId name_`
- `bool if_exists_`
- `bool cascade_`  // true = CASCADE, false = RESTRICT

### Pattern 3: Alternative option (FORCE)
Used by: TABLESPACE

Members:
- `StringPool::StringId tablespace_name_`
- `bool force_`  // No if_exists support

### Pattern 4: Minimal implementation
Used by: TRIGGER

Members:
- `StringPool::StringId trigger_name_`
- `bool if_exists_`  // Currently always false

## Inconsistencies and Notes

1. **DropBehavior Representation**:
   - TABLE, INDEX, USER, ROLE, GROUP, POLICY use DropBehavior enum
   - SEQUENCE, VIEW use boolean cascade flag
   - TABLESPACE uses FORCE instead of CASCADE/RESTRICT
   - Recommendation: Standardize on enum for consistency

2. **IF EXISTS Support**:
   - Most commands support IF EXISTS
   - DROP TABLESPACE does not support IF EXISTS
   - DROP TRIGGER has IF EXISTS planned but not implemented

3. **Dispatch Status**:
   - DROP TRIGGER is implemented but commented out in dispatch
   - Comment references "Agent C" for future activation

4. **Special Cases**:
   - DROP POLICY is unique in requiring an ON clause with table name
   - DROP TABLESPACE uses FORCE instead of CASCADE/RESTRICT pattern

5. **Default Behavior**:
   - When CASCADE/RESTRICT is supported, RESTRICT is always the default
   - This is the safe behavior that prevents accidental cascading deletes

## Token Requirements

All DROP commands use these core tokens:
- `KW_DROP` (line 338)
- `KW_IF` (line 413)
- `KW_EXISTS` (line 389)
- `KW_CASCADE` (line 336)
- `KW_RESTRICT` (line 337)

Object-specific tokens:
- `KW_TABLE` (line 79)
- `KW_INDEX` (line 80)
- `KW_VIEW` (line 353)
- `KW_SEQUENCE` (line 342)
- `KW_TABLESPACE` (line 326)
- `KW_TRIGGER` (line 392)
- `KW_USER` (line 429)
- `KW_ROLE` (line 430)
- `KW_GROUP` (line 114)
- `KW_POLICY` (line 445)
- `KW_FORCE` (line 335)

## File Locations

- Parser implementations: `/home/dcalford/CliWork/ScratchBird/src/parser/parser.cpp`
- AST definitions: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast.h`
- Token definitions: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/token.h`

## Comparative Analysis Notes

This comprehensive documentation is intended for comparative audit purposes. Key observations:

1. **Consistency**: Most DROP commands follow similar patterns with IF EXISTS and CASCADE/RESTRICT support
2. **Exceptions**: TABLESPACE and TRIGGER deviate from the standard pattern
3. **Completeness**: Security-related drops (USER, ROLE, GROUP, POLICY) are fully implemented
4. **Firebird Compatibility**: The TRIGGER implementation appears to be targeting Firebird compatibility (commented reference to "Agent C")

---

Document generated: 2025-12-06
Parser version: ScratchBird ALPHA Phase 1
Based on: `/home/dcalford/CliWork/ScratchBird/src/parser/parser.cpp`
