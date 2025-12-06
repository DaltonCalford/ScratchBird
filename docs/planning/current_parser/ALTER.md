# ScratchBird ALTER Commands - Complete Reference

**Document Type:** Comparative Parser Audit
**Last Updated:** December 6, 2025
**Parser Version:** Alpha 1 Complete (1020/1020 tests passing)

This document provides a comprehensive audit of all ALTER commands implemented in the ScratchBird parser, documenting every syntax variation, option, and operation supported.

---

## Overview

The ScratchBird parser implements ALTER commands for five object types:

1. **ALTER SEQUENCE** - Modify sequence parameters
2. **ALTER TABLESPACE** - Modify tablespace properties
3. **ALTER TABLE** - Modify table structure and properties
4. **ALTER USER** - Modify user credentials and privileges
5. **ALTER TABLE (RLS)** - Row Level Security operations

All ALTER command parsing is dispatched from `parseStatement()` at lines 446-469 in `/home/dcalford/CliWork/ScratchBird/src/parser/parser.cpp`.

---

## 1. ALTER SEQUENCE

**Parser Function:** `parseAlterSequence()` (lines 5894-5996)
**AST Node:** `AlterSequenceStmt` (ast.h lines 1758-1802)

### Complete BNF Syntax

```bnf
ALTER SEQUENCE sequence_name
    [ INCREMENT BY increment_value ]
    [ MINVALUE minvalue | NO MINVALUE ]
    [ MAXVALUE maxvalue | NO MAXVALUE ]
    [ RESTART [ WITH restart_value ] ]
    [ CACHE cache_value ]
    [ CYCLE | NO CYCLE ]
```

### Alter Actions

#### 1.1 INCREMENT BY
**Syntax:** `INCREMENT BY <expression>`
**Purpose:** Change the increment value for the sequence
**Implementation:**
- Keyword: `KW_INCREMENT` followed by `KW_BY`
- Value: Expression (parsed via `parseExpression()`)
- AST Method: `setIncrementBy(Expression* expr)`

**Example:**
```sql
ALTER SEQUENCE order_seq INCREMENT BY 5;
```

#### 1.2 MINVALUE / NO MINVALUE
**Syntax:**
- `MINVALUE <expression>`
- `NO MINVALUE`

**Purpose:** Set or remove minimum value constraint
**Implementation:**
- `MINVALUE`: Keyword `KW_MINVALUE` + expression
- `NO MINVALUE`: Keywords `KW_NO` + `KW_MINVALUE`
- AST Methods:
  - `setMinValue(Expression* expr)`
  - `setNoMinValue(bool no_min)`

**Examples:**
```sql
ALTER SEQUENCE order_seq MINVALUE 1000;
ALTER SEQUENCE order_seq NO MINVALUE;
```

#### 1.3 MAXVALUE / NO MAXVALUE
**Syntax:**
- `MAXVALUE <expression>`
- `NO MAXVALUE`

**Purpose:** Set or remove maximum value constraint
**Implementation:**
- `MAXVALUE`: Keyword `KW_MAXVALUE` + expression
- `NO MAXVALUE`: Keywords `KW_NO` + `KW_MAXVALUE`
- AST Methods:
  - `setMaxValue(Expression* expr)`
  - `setNoMaxValue(bool no_max)`

**Examples:**
```sql
ALTER SEQUENCE order_seq MAXVALUE 999999;
ALTER SEQUENCE order_seq NO MAXVALUE;
```

#### 1.4 RESTART
**Syntax:**
- `RESTART`
- `RESTART WITH <expression>`

**Purpose:** Reset the sequence to start value or specific value
**Implementation:**
- Keyword: `KW_RESTART`
- Optional: `KW_WITH` + expression
- AST Method: `setRestart(Expression* expr)` (nullptr for RESTART without WITH)

**Examples:**
```sql
ALTER SEQUENCE order_seq RESTART;              -- Restart from start_value
ALTER SEQUENCE order_seq RESTART WITH 5000;    -- Restart from 5000
```

#### 1.5 CACHE
**Syntax:** `CACHE <expression>`
**Purpose:** Change the number of sequence values to preallocate in memory
**Implementation:**
- Keyword: `KW_CACHE`
- Value: Expression
- AST Method: `setCache(Expression* expr)`

**Example:**
```sql
ALTER SEQUENCE order_seq CACHE 50;
```

#### 1.6 CYCLE / NO CYCLE
**Syntax:**
- `CYCLE`
- `NO CYCLE`

**Purpose:** Enable or disable cycling when sequence reaches min/max value
**Implementation:**
- Keywords: `KW_CYCLE` or `KW_NO` + `KW_CYCLE`
- AST Method: `setCycle(bool cycle)`

**Examples:**
```sql
ALTER SEQUENCE order_seq CYCLE;
ALTER SEQUENCE order_seq NO CYCLE;
```

### Multiple Operations

Multiple ALTER operations can be combined in a single statement:

```sql
ALTER SEQUENCE order_seq
    INCREMENT BY 10
    MINVALUE 1000
    MAXVALUE 999999
    CACHE 20
    CYCLE;
```

### AST Structure

**AlterSequenceStmt members:**
- `StringPool::StringId name_` - Sequence name
- `Expression* increment_by_` - INCREMENT BY value
- `Expression* min_value_` - MINVALUE
- `Expression* max_value_` - MAXVALUE
- `Expression* restart_` - RESTART value (nullptr = restart from start_value)
- `Expression* cache_` - CACHE value
- `bool has_cycle_` - Whether CYCLE/NO CYCLE was specified
- `bool cycle_` - CYCLE value (true/false)
- `bool no_min_value_` - NO MINVALUE specified
- `bool no_max_value_` - NO MAXVALUE specified

---

## 2. ALTER TABLESPACE

**Parser Function:** `parseAlterTablespace()` (lines 6388-6520)
**AST Node:** `AlterTablespaceStmt` (ast.h lines 3014-3042)
**Alteration Types:** `TablespaceAlterationType` enum (ast.h lines 2993-2999)

### Complete BNF Syntax

```bnf
ALTER TABLESPACE tablespace_name
    { AUTOEXTEND { ON | OFF }
    | AUTOEXTEND_SIZE size_value
    | MAXSIZE { size_value | UNLIMITED }
    | RENAME TO new_name }
    [ ... ]
```

### Alter Actions

#### 2.1 AUTOEXTEND ON/OFF
**Syntax:** `AUTOEXTEND { ON | OFF }`
**Purpose:** Enable or disable automatic tablespace extension
**Implementation:**
- Keyword: `KW_AUTOEXTEND`
- Options: `KW_ON` or `KW_OFF`
- Alteration Type: `TablespaceAlterationType::SET_AUTOEXTEND`
- Structure Member: `bool autoextend_enabled`

**Examples:**
```sql
ALTER TABLESPACE data_ts AUTOEXTEND ON;
ALTER TABLESPACE data_ts AUTOEXTEND OFF;
```

#### 2.2 AUTOEXTEND_SIZE
**Syntax:** `AUTOEXTEND_SIZE <integer>`
**Purpose:** Set the size increment when tablespace auto-extends
**Implementation:**
- Keyword: `KW_AUTOEXTEND_SIZE`
- Value: `INTEGER_LITERAL`
- Alteration Type: `TablespaceAlterationType::SET_AUTOEXTEND_SIZE`
- Structure Member: `uint32_t size_value`

**Example:**
```sql
ALTER TABLESPACE data_ts AUTOEXTEND_SIZE 1048576;  -- 1MB
```

#### 2.3 MAXSIZE
**Syntax:** `MAXSIZE { <integer> | UNLIMITED }`
**Purpose:** Set maximum tablespace size or make it unlimited
**Implementation:**
- Keyword: `KW_MAXSIZE`
- Options:
  - `KW_UNLIMITED` - Sets size_value to 0 (special value for unlimited)
  - `INTEGER_LITERAL` - Actual size value
- Alteration Type: `TablespaceAlterationType::SET_MAXSIZE`
- Structure Member: `uint32_t size_value` (0 = unlimited)

**Examples:**
```sql
ALTER TABLESPACE data_ts MAXSIZE 10485760;  -- 10MB max
ALTER TABLESPACE data_ts MAXSIZE UNLIMITED;
```

#### 2.4 RENAME TO
**Syntax:** `RENAME TO new_name`
**Purpose:** Rename the tablespace
**Implementation:**
- Keywords: `KW_RENAME` + `KW_TO`
- Value: `IDENTIFIER` (new tablespace name)
- Alteration Type: `TablespaceAlterationType::RENAME_TO`
- Structure Member: `StringPool::StringId new_name`

**Example:**
```sql
ALTER TABLESPACE old_ts RENAME TO new_ts;
```

### Multiple Operations

Multiple alterations can be specified in a single statement:

```sql
ALTER TABLESPACE data_ts
    AUTOEXTEND ON
    AUTOEXTEND_SIZE 524288
    MAXSIZE 10485760;
```

### AST Structure

**TablespaceAlteration struct (ast.h lines 3001-3012):**
```cpp
struct TablespaceAlteration {
    TablespaceAlterationType type;
    bool autoextend_enabled;              // For SET_AUTOEXTEND
    uint32_t size_value;                  // For SET_AUTOEXTEND_SIZE and SET_MAXSIZE
    StringPool::StringId new_name;        // For RENAME_TO
};
```

**AlterTablespaceStmt members:**
- `StringPool::StringId tablespace_name_` - Tablespace to alter
- `std::vector<TablespaceAlteration> alterations_` - List of alterations

**Validation:**
- At least one alteration is required (enforced at line 6512)

---

## 3. ALTER TABLE

**Parser Function:** `parseAlterTable()` (lines 6522-6734)
**AST Nodes:**
- `AlterTableStmt` (ast.h lines 1566-1705)
- `AlterTableSetTablespaceStmt` (ast.h lines 3044-3079)
- `AlterTableRLSStmt` (ast.h lines 4543-4571)

### Complete BNF Syntax

```bnf
ALTER TABLE [ schema. ] table_name
    { SET TABLESPACE tablespace_name [ ONLINE ]
    | ADD COLUMN column_definition
    | DROP COLUMN [ IF EXISTS ] column_name [ CASCADE | RESTRICT ]
    | RENAME COLUMN old_name TO new_name
    | ALTER COLUMN column_name TYPE new_type
    | { ENABLE | DISABLE | FORCE | NO FORCE } ROW LEVEL SECURITY }
```

### Alter Actions

#### 3.1 SET TABLESPACE
**Syntax:** `SET TABLESPACE tablespace_name [ ONLINE ]`
**Purpose:** Move table to a different tablespace
**Implementation:**
- Keywords: `KW_SET` + `KW_TABLESPACE`
- Tablespace: `IDENTIFIER`
- Optional: `KW_ONLINE` flag
- AST Node: `AlterTableSetTablespaceStmt` (separate statement type)

**Examples:**
```sql
ALTER TABLE customers SET TABLESPACE ssd_ts;
ALTER TABLE customers SET TABLESPACE ssd_ts ONLINE;
```

**AST Structure (AlterTableSetTablespaceStmt):**
- `StringPool::StringId table_name_`
- `StringPool::StringId tablespace_name_`
- `bool online_` - Whether to perform move online

#### 3.2 ADD COLUMN
**Syntax:** `ADD COLUMN column_definition`
**Purpose:** Add a new column to the table
**Implementation:**
- Keywords: `KW_ADD` + `KW_COLUMN`
- Column Definition: Parsed via `parseColumnDef()`
- Alter Action: `AlterTableStmt::AlterAction::ADD_COLUMN`
- AST Method: `setColumnDef(ColumnDef* col_def)`

**Examples:**
```sql
ALTER TABLE customers ADD COLUMN email VARCHAR(255);
ALTER TABLE customers ADD COLUMN created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP;
ALTER TABLE customers ADD COLUMN age INTEGER CHECK (age >= 18);
```

**Column Definition Supports:**
- Data type with size/precision
- NOT NULL / NULL constraints
- DEFAULT expressions
- CHECK constraints
- UNIQUE constraints
- PRIMARY KEY constraint
- FOREIGN KEY references
- GENERATED columns (computed/identity)

#### 3.3 DROP COLUMN
**Syntax:** `DROP COLUMN [ IF EXISTS ] column_name [ CASCADE | RESTRICT ]`
**Purpose:** Remove a column from the table
**Implementation:**
- Keywords: `KW_DROP` + `KW_COLUMN`
- Optional: `IF EXISTS` clause
- Column Name: `IDENTIFIER`
- Drop Behavior: `CASCADE` or `RESTRICT` (default)
- Alter Action: `AlterTableStmt::AlterAction::DROP_COLUMN`
- AST Method: `setDropColumnName(StringPool::StringId, bool if_exists, DropBehavior)`

**Examples:**
```sql
ALTER TABLE customers DROP COLUMN middle_name;
ALTER TABLE customers DROP COLUMN IF EXISTS temp_column;
ALTER TABLE customers DROP COLUMN status CASCADE;
ALTER TABLE customers DROP COLUMN notes RESTRICT;
```

**Drop Behavior:**
- `RESTRICT` (default): Fail if column has dependencies
- `CASCADE`: Drop dependent objects (views, indexes, constraints, etc.)

**AST Enum:**
```cpp
enum class DropBehavior : uint8_t {
    RESTRICT,  // Fail if dependencies exist (default)
    CASCADE    // Drop dependent objects recursively
};
```

#### 3.4 RENAME COLUMN
**Syntax:** `RENAME COLUMN old_name TO new_name`
**Purpose:** Rename an existing column
**Implementation:**
- Keywords: `KW_RENAME` + `KW_COLUMN`
- Old Name: `IDENTIFIER`
- Keyword: `KW_TO`
- New Name: `IDENTIFIER`
- Alter Action: `AlterTableStmt::AlterAction::RENAME_COLUMN`
- AST Method: `setRenameColumn(StringPool::StringId old_name, StringPool::StringId new_name)`

**Example:**
```sql
ALTER TABLE customers RENAME COLUMN customer_id TO id;
ALTER TABLE products RENAME COLUMN description TO product_description;
```

#### 3.5 ALTER COLUMN TYPE
**Syntax:** `ALTER COLUMN column_name TYPE new_type`
**Purpose:** Change the data type of an existing column
**Implementation:**
- Keywords: `KW_ALTER` + `KW_COLUMN`
- Column Name: `IDENTIFIER`
- Keyword: `KW_TYPE`
- New Type: Parsed via `parseTypeName()`
- Alter Action: `AlterTableStmt::AlterAction::ALTER_COLUMN_TYPE`
- AST Method: `setAlterColumnType(StringPool::StringId col_name, TypeName* new_type)`

**Examples:**
```sql
ALTER TABLE customers ALTER COLUMN age TYPE BIGINT;
ALTER TABLE products ALTER COLUMN price TYPE DECIMAL(10,2);
ALTER TABLE users ALTER COLUMN username TYPE VARCHAR(100);
```

**Type Changes Supported:**
- Basic types: INTEGER, BIGINT, SMALLINT, FLOAT, DOUBLE, etc.
- Sized types: VARCHAR(n), CHAR(n), DECIMAL(p,s)
- All ScratchBird data types supported by `parseTypeName()`

#### 3.6 Row Level Security Operations
**Syntax:**
```bnf
{ ENABLE | DISABLE | FORCE | NO FORCE } ROW LEVEL SECURITY
```

**Purpose:** Enable, disable, or force row-level security policies on the table
**Implementation:**
- Dispatched to `parseAlterTableRLS()` (lines 9579-9640)
- Detected by checking for `ENABLE`, `DISABLE`, `FORCE`, or `NO` keywords
- AST Node: `AlterTableRLSStmt` (separate statement type)

**RLS Actions:**

##### 3.6.1 ENABLE ROW LEVEL SECURITY
**Syntax:** `ENABLE ROW LEVEL SECURITY`
**Purpose:** Enable RLS on the table (policies will be enforced for non-owners)
**RLS Action:** `AlterTableRLSStmt::RLSAction::ENABLE`

**Example:**
```sql
ALTER TABLE sensitive_data ENABLE ROW LEVEL SECURITY;
```

##### 3.6.2 DISABLE ROW LEVEL SECURITY
**Syntax:** `DISABLE ROW LEVEL SECURITY`
**Purpose:** Disable RLS on the table (policies will not be enforced)
**RLS Action:** `AlterTableRLSStmt::RLSAction::DISABLE`

**Example:**
```sql
ALTER TABLE sensitive_data DISABLE ROW LEVEL SECURITY;
```

##### 3.6.3 FORCE ROW LEVEL SECURITY
**Syntax:** `FORCE ROW LEVEL SECURITY`
**Purpose:** Force RLS policies to apply to table owner as well
**RLS Action:** `AlterTableRLSStmt::RLSAction::FORCE`

**Example:**
```sql
ALTER TABLE sensitive_data FORCE ROW LEVEL SECURITY;
```

##### 3.6.4 NO FORCE ROW LEVEL SECURITY
**Syntax:** `NO FORCE ROW LEVEL SECURITY`
**Purpose:** Disable forced RLS (owner bypasses policies)
**RLS Action:** `AlterTableRLSStmt::RLSAction::NO_FORCE`

**Example:**
```sql
ALTER TABLE sensitive_data NO FORCE ROW LEVEL SECURITY;
```

### AST Structures

#### AlterTableStmt (ast.h lines 1566-1705)

**AlterAction Enum:**
```cpp
enum class AlterAction : uint8_t {
    ADD_COLUMN,
    DROP_COLUMN,
    ALTER_COLUMN_TYPE,
    ALTER_COLUMN_SET_DEFAULT,      // Note: Not yet implemented in parser
    ALTER_COLUMN_DROP_DEFAULT,     // Note: Not yet implemented in parser
    RENAME_COLUMN,
    ADD_CONSTRAINT,                // Note: Not yet implemented in parser
    DROP_CONSTRAINT                // Note: Not yet implemented in parser
};
```

**Members:**
- `StringPool::StringId table_name_` - Table to alter
- `AlterAction action_` - Type of alteration
- `ColumnDef* column_def_` - For ADD_COLUMN
- `TypeName* new_type_` - For ALTER_COLUMN_TYPE
- `Expression* default_expr_` - For SET/DROP DEFAULT (future)
- `StringPool::StringId old_column_name_` - For DROP/RENAME/ALTER COLUMN
- `StringPool::StringId new_column_name_` - For RENAME COLUMN
- `StringPool::StringId constraint_name_` - For constraint operations (future)
- `bool if_exists_` - For DROP COLUMN IF EXISTS
- `DropBehavior drop_behavior_` - CASCADE or RESTRICT

#### AlterTableRLSStmt (ast.h lines 4543-4571)

**RLSAction Enum:**
```cpp
enum class RLSAction : uint8_t {
    ENABLE,      // ENABLE ROW LEVEL SECURITY
    DISABLE,     // DISABLE ROW LEVEL SECURITY
    FORCE,       // FORCE ROW LEVEL SECURITY
    NO_FORCE     // NO FORCE ROW LEVEL SECURITY
};
```

**Members:**
- `StringPool::StringId table_name_` - Table to alter
- `RLSAction action_` - RLS action to perform

### NOT YET IMPLEMENTED

The following ALTER TABLE operations are defined in the AST but not yet parsed:

1. **ALTER COLUMN SET DEFAULT**
   ```sql
   ALTER TABLE table_name ALTER COLUMN column_name SET DEFAULT expression;
   ```

2. **ALTER COLUMN DROP DEFAULT**
   ```sql
   ALTER TABLE table_name ALTER COLUMN column_name DROP DEFAULT;
   ```

3. **ADD CONSTRAINT**
   ```sql
   ALTER TABLE table_name ADD CONSTRAINT constraint_name constraint_definition;
   ```

4. **DROP CONSTRAINT**
   ```sql
   ALTER TABLE table_name DROP CONSTRAINT constraint_name [CASCADE | RESTRICT];
   ```

These operations are in the `AlterAction` enum but have no corresponding parser implementation.

---

## 4. ALTER USER

**Parser Function:** `parseAlterUser()` (lines 8414-8476)
**AST Node:** `AlterUserStmt` (ast.h lines 3984-4017)

### Complete BNF Syntax

```bnf
ALTER USER username
    [ WITH PASSWORD 'password' ]
    [ SUPERUSER | NOSUPERUSER ]
```

### Alter Actions

#### 4.1 WITH PASSWORD
**Syntax:** `[ WITH ] PASSWORD 'password_string'`
**Purpose:** Change the user's password
**Implementation:**
- Keywords: `KW_WITH` (optional) + `KW_PASSWORD`
- Value: `STRING_LITERAL`
- AST Members:
  - `StringPool::StringId password_`
  - `bool change_password_` (true if password provided)

**Example:**
```sql
ALTER USER john WITH PASSWORD 'new_secure_password';
ALTER USER jane PASSWORD 'another_password';  -- WITH is optional
```

#### 4.2 SUPERUSER / NOSUPERUSER
**Syntax:** `SUPERUSER | NOSUPERUSER`
**Purpose:** Grant or revoke superuser privileges
**Implementation:**
- Keywords: `KW_SUPERUSER` or `KW_NOSUPERUSER`
- AST Members:
  - `bool is_superuser_` (true for SUPERUSER, false for NOSUPERUSER)
  - `bool change_superuser_` (true if either keyword present)

**Examples:**
```sql
ALTER USER john SUPERUSER;
ALTER USER jane NOSUPERUSER;
```

### Combined Operations

Both password and superuser status can be changed in one statement:

```sql
ALTER USER admin WITH PASSWORD 'new_password' SUPERUSER;
ALTER USER guest PASSWORD 'guest_pass' NOSUPERUSER;
```

### AST Structure

**AlterUserStmt members:**
- `StringPool::StringId username_` - User to alter
- `StringPool::StringId password_` - New password (0 if not changing)
- `bool change_password_` - Whether to change password
- `bool is_superuser_` - New superuser status
- `bool change_superuser_` - Whether to change superuser status

**Change Tracking:**
The AST uses separate boolean flags (`change_password_`, `change_superuser_`) to distinguish between:
- Not specified in ALTER statement
- Explicitly set to a value

This allows partial updates where only password OR only superuser status is modified.

---

## 5. Dispatch Logic

**Location:** `parseStatement()` lines 446-469

### ALTER Command Dispatch

```cpp
else if (match(TokenType::KW_ALTER))
{
    if (check(TokenType::KW_TABLESPACE))
    {
        stmt = parseAlterTablespace();
    }
    else if (check(TokenType::KW_TABLE))
    {
        stmt = parseAlterTable(); // May dispatch to parseAlterTableRLS
    }
    else if (check(TokenType::KW_SEQUENCE))
    {
        stmt = parseAlterSequence();
    }
    else if (check(TokenType::KW_USER))
    {
        stmt = parseAlterUser();
    }
    else
    {
        error("Expected TABLESPACE, TABLE, SEQUENCE, or USER after ALTER");
        synchronize();
    }
}
```

### Decision Tree

```
ALTER
├── TABLESPACE → parseAlterTablespace()
├── TABLE → parseAlterTable()
│   ├── SET TABLESPACE → AlterTableSetTablespaceStmt
│   ├── ENABLE/DISABLE/FORCE/NO → parseAlterTableRLS()
│   ├── ADD COLUMN → AlterTableStmt (ADD_COLUMN)
│   ├── DROP COLUMN → AlterTableStmt (DROP_COLUMN)
│   ├── RENAME COLUMN → AlterTableStmt (RENAME_COLUMN)
│   └── ALTER COLUMN → AlterTableStmt (ALTER_COLUMN_TYPE)
├── SEQUENCE → parseAlterSequence()
└── USER → parseAlterUser()
```

---

## 6. Keywords Used

### ALTER SEQUENCE Keywords
- `KW_ALTER`, `KW_SEQUENCE`
- `KW_INCREMENT`, `KW_BY`
- `KW_MINVALUE`, `KW_MAXVALUE`
- `KW_NO`
- `KW_RESTART`, `KW_WITH`
- `KW_CACHE`
- `KW_CYCLE`

### ALTER TABLESPACE Keywords
- `KW_ALTER`, `KW_TABLESPACE`
- `KW_AUTOEXTEND`, `KW_ON`, `KW_OFF`
- `KW_AUTOEXTEND_SIZE`
- `KW_MAXSIZE`, `KW_UNLIMITED`
- `KW_RENAME`, `KW_TO`

### ALTER TABLE Keywords
- `KW_ALTER`, `KW_TABLE`
- `KW_SET`, `KW_TABLESPACE`, `KW_ONLINE`
- `KW_ADD`, `KW_COLUMN`
- `KW_DROP`, `KW_IF`, `KW_EXISTS`, `KW_CASCADE`, `KW_RESTRICT`
- `KW_RENAME`, `KW_TO`
- `KW_ALTER`, `KW_TYPE`
- `KW_ENABLE`, `KW_DISABLE`, `KW_FORCE`, `KW_NO`
- `KW_ROW`, `KW_LEVEL`, `KW_SECURITY`

### ALTER USER Keywords
- `KW_ALTER`, `KW_USER`
- `KW_WITH`, `KW_PASSWORD`
- `KW_SUPERUSER`, `KW_NOSUPERUSER`

---

## 7. Common Patterns

### Optional Clauses
- `IF EXISTS` in DROP COLUMN
- `WITH` in ALTER USER PASSWORD (optional)
- `ONLINE` in SET TABLESPACE
- `WITH` in RESTART WITH

### Dual Options
- `MINVALUE value | NO MINVALUE`
- `MAXVALUE value | NO MAXVALUE`
- `CYCLE | NO CYCLE`
- `SUPERUSER | NOSUPERUSER`
- `AUTOEXTEND ON | OFF`
- `CASCADE | RESTRICT`

### Value Types
- **Expressions:** Used for INCREMENT BY, MINVALUE, MAXVALUE, RESTART, CACHE
- **Integer Literals:** Used for AUTOEXTEND_SIZE, MAXSIZE
- **String Literals:** Used for PASSWORD
- **Identifiers:** Used for names (table, column, tablespace, user)
- **Type Names:** Used for ALTER COLUMN TYPE

---

## 8. Error Handling

All ALTER parsers follow consistent error handling:

1. **Consume required keywords** with error messages
2. **Check for expected tokens** before advancing
3. **Synchronize** on errors to continue parsing
4. **Return nullptr** on parse failure

Example error messages:
- "Expected SEQUENCE after ALTER"
- "Expected sequence name after ALTER SEQUENCE"
- "Expected BY after INCREMENT"
- "Expected ON or OFF after AUTOEXTEND"
- "Expected TO after RENAME"
- "Expected column name after DROP COLUMN"
- "Expected EXISTS after IF"
- "Expected TABLESPACE after SET"
- "Expected TYPE after column name"

---

## 9. Comparative Analysis

### What's Implemented

| Feature | Status | Notes |
|---------|--------|-------|
| ALTER SEQUENCE (all options) | ✅ Complete | All 6 options fully supported |
| ALTER TABLESPACE (all options) | ✅ Complete | All 4 options fully supported |
| ALTER TABLE ADD COLUMN | ✅ Complete | Full column definition support |
| ALTER TABLE DROP COLUMN | ✅ Complete | IF EXISTS, CASCADE/RESTRICT |
| ALTER TABLE RENAME COLUMN | ✅ Complete | Simple rename operation |
| ALTER TABLE ALTER COLUMN TYPE | ✅ Complete | Type changes supported |
| ALTER TABLE SET TABLESPACE | ✅ Complete | With ONLINE option |
| ALTER TABLE RLS | ✅ Complete | All 4 RLS actions |
| ALTER USER PASSWORD | ✅ Complete | WITH keyword optional |
| ALTER USER SUPERUSER | ✅ Complete | Grant/revoke superuser |

### What's NOT Implemented

| Feature | Status | Notes |
|---------|--------|-------|
| ALTER TABLE SET/DROP DEFAULT | ❌ Not Implemented | AST defined, parser missing |
| ALTER TABLE ADD CONSTRAINT | ❌ Not Implemented | AST defined, parser missing |
| ALTER TABLE DROP CONSTRAINT | ❌ Not Implemented | AST defined, parser missing |
| ALTER TABLE SET NOT NULL | ❌ Not Implemented | Not in AST or parser |
| ALTER TABLE DROP NOT NULL | ❌ Not Implemented | Not in AST or parser |
| ALTER TABLE RENAME TO | ❌ Not Implemented | Table rename not supported |
| ALTER DOMAIN | ❌ Not Implemented | No ALTER DOMAIN support |
| ALTER INDEX | ❌ Not Implemented | No ALTER INDEX support |
| ALTER DATABASE | ❌ Not Implemented | No ALTER DATABASE support |
| ALTER VIEW | ❌ Not Implemented | No ALTER VIEW support |
| ALTER SCHEMA | ❌ Not Implemented | No ALTER SCHEMA support |

### Multiple Actions Per Statement

- **ALTER SEQUENCE:** ✅ Supports multiple options in one statement
- **ALTER TABLESPACE:** ✅ Supports multiple alterations in one statement
- **ALTER TABLE:** ❌ Only one action per statement currently
- **ALTER USER:** ✅ Can change password and superuser in one statement

---

## 10. File Locations

### Parser Implementation
- **Main Dispatch:** `/home/dcalford/CliWork/ScratchBird/src/parser/parser.cpp` lines 446-469
- **parseAlterSequence():** Lines 5894-5996
- **parseAlterTablespace():** Lines 6388-6520
- **parseAlterTable():** Lines 6522-6734
- **parseAlterUser():** Lines 8414-8476
- **parseAlterTableRLS():** Lines 9579-9640

### AST Definitions
- **AlterSequenceStmt:** `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast.h` lines 1758-1802
- **AlterTablespaceStmt:** Lines 3014-3042
- **TablespaceAlterationType:** Lines 2993-2999
- **AlterTableStmt:** Lines 1566-1705
- **AlterTableSetTablespaceStmt:** Lines 3044-3079
- **AlterUserStmt:** Lines 3984-4017
- **AlterTableRLSStmt:** Lines 4543-4571

### Token Definitions
- **Keywords:** `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/token.h`
- **KW_ALTER:** Line 363

---

## 11. Complete Examples

### ALTER SEQUENCE Examples

```sql
-- Single operation
ALTER SEQUENCE order_id_seq INCREMENT BY 10;

-- Multiple operations
ALTER SEQUENCE order_id_seq
    INCREMENT BY 5
    MINVALUE 1000
    MAXVALUE 999999
    CACHE 50
    CYCLE;

-- Reset sequence
ALTER SEQUENCE order_id_seq RESTART WITH 1;

-- Remove limits
ALTER SEQUENCE order_id_seq NO MINVALUE NO MAXVALUE NO CYCLE;
```

### ALTER TABLESPACE Examples

```sql
-- Enable auto-extension
ALTER TABLESPACE data_ts AUTOEXTEND ON;

-- Set extension parameters
ALTER TABLESPACE data_ts
    AUTOEXTEND ON
    AUTOEXTEND_SIZE 1048576
    MAXSIZE 104857600;

-- Make unlimited
ALTER TABLESPACE data_ts MAXSIZE UNLIMITED;

-- Rename tablespace
ALTER TABLESPACE old_name RENAME TO new_name;
```

### ALTER TABLE Examples

```sql
-- Move to different tablespace
ALTER TABLE customers SET TABLESPACE ssd_ts;
ALTER TABLE large_table SET TABLESPACE archive_ts ONLINE;

-- Add columns
ALTER TABLE customers ADD COLUMN email VARCHAR(255);
ALTER TABLE customers ADD COLUMN created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP;

-- Drop columns
ALTER TABLE customers DROP COLUMN middle_name;
ALTER TABLE customers DROP COLUMN IF EXISTS temp_col CASCADE;

-- Rename column
ALTER TABLE customers RENAME COLUMN customer_id TO id;

-- Change column type
ALTER TABLE customers ALTER COLUMN age TYPE BIGINT;
ALTER TABLE products ALTER COLUMN price TYPE DECIMAL(10,2);

-- Enable Row Level Security
ALTER TABLE sensitive_data ENABLE ROW LEVEL SECURITY;
ALTER TABLE sensitive_data FORCE ROW LEVEL SECURITY;
ALTER TABLE sensitive_data DISABLE ROW LEVEL SECURITY;
ALTER TABLE sensitive_data NO FORCE ROW LEVEL SECURITY;
```

### ALTER USER Examples

```sql
-- Change password
ALTER USER john WITH PASSWORD 'new_secure_password';
ALTER USER jane PASSWORD 'another_password';

-- Change superuser status
ALTER USER admin SUPERUSER;
ALTER USER guest NOSUPERUSER;

-- Change both
ALTER USER john WITH PASSWORD 'new_pass' SUPERUSER;
```

---

## 12. Summary

The ScratchBird parser implements a comprehensive set of ALTER commands covering:

- **Sequences:** Full control over all sequence parameters
- **Tablespaces:** Auto-extension, sizing, and renaming
- **Tables:** Column operations (add/drop/rename/type change), tablespace movement, and RLS
- **Users:** Password and privilege management

The implementation is well-structured with:
- Clean separation between different ALTER types
- Consistent error handling
- Flexible AST structures supporting multiple operations
- Support for optional clauses and dual-option keywords

Future enhancements could include:
- ALTER TABLE constraint operations (ADD/DROP CONSTRAINT)
- ALTER TABLE default value operations (SET/DROP DEFAULT)
- ALTER TABLE NOT NULL operations
- ALTER TABLE RENAME TO (table rename)
- ALTER commands for other objects (INDEX, DOMAIN, DATABASE, VIEW, SCHEMA)
- Support for multiple actions in a single ALTER TABLE statement

---

**End of Document**
