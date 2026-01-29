[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL Tables and Constraints

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## Overview

This document covers table DDL operations in MySQL emulation mode, including table creation, modification, and deletion. ScratchBird's MySQL parser supports MySQL 8.x-style table syntax with various column and table-level constraints.

**Important:** Due to bytecode format mismatches between the parser and executor, many table operations are currently in a stubbed or partial state. This documentation describes both the intended functionality and current limitations.

---

## CREATE TABLE

Creates a new table with columns, constraints, and table options.

### Syntax

```sql
CREATE [TEMPORARY] TABLE [IF NOT EXISTS] table_name (
    column_definition [, column_definition ...]
    [, table_constraint ...]
)
[table_option ...]
```

#### Column Definition

```sql
column_name data_type
    [NOT NULL | NULL]
    [DEFAULT default_value]
    [AUTO_INCREMENT]
    [UNIQUE [KEY]]
    [PRIMARY KEY]
    [COMMENT 'string']
    [COLUMN_FORMAT {FIXED | DYNAMIC | DEFAULT}]
    [STORAGE {DISK | MEMORY}]
    [reference_definition]
```

#### Table Constraint

```sql
[CONSTRAINT [constraint_name]]
    PRIMARY KEY (column_name [, column_name ...])
  | UNIQUE [INDEX | KEY] [index_name] (column_name [, column_name ...])
  | FOREIGN KEY [index_name] (column_name [, column_name ...])
        REFERENCES ref_table (ref_column [, ref_column ...])
        [ON DELETE {RESTRICT | CASCADE | SET NULL | NO ACTION | SET DEFAULT}]
        [ON UPDATE {RESTRICT | CASCADE | SET NULL | NO ACTION | SET DEFAULT}]
  | CHECK (expression)
```

#### Table Options

```sql
ENGINE = {InnoDB | MyISAM | MEMORY | ...}
AUTO_INCREMENT = value
CHARACTER SET = charset_name
COLLATE = collation_name
COMMENT = 'string'
ROW_FORMAT = {DEFAULT | DYNAMIC | FIXED | COMPRESSED | REDUNDANT | COMPACT}
... and more
```

### Examples

**Basic table creation:**
```sql
CREATE TABLE users (
    id INT PRIMARY KEY,
    email VARCHAR(255) UNIQUE,
    name VARCHAR(100) NOT NULL
);
```

**Table with AUTO_INCREMENT:**
```sql
CREATE TABLE customers (
    customer_id INT AUTO_INCREMENT PRIMARY KEY,
    first_name VARCHAR(50) NOT NULL,
    last_name VARCHAR(50) NOT NULL,
    email VARCHAR(100) UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

**Table with foreign key:**
```sql
CREATE TABLE orders (
    order_id INT AUTO_INCREMENT PRIMARY KEY,
    customer_id INT NOT NULL,
    order_date DATE NOT NULL,
    total DECIMAL(10, 2),
    FOREIGN KEY (customer_id) REFERENCES customers(customer_id)
        ON DELETE CASCADE
        ON UPDATE CASCADE
);
```

**Table with composite primary key:**
```sql
CREATE TABLE order_items (
    order_id INT,
    product_id INT,
    quantity INT NOT NULL,
    price DECIMAL(10, 2) NOT NULL,
    PRIMARY KEY (order_id, product_id),
    FOREIGN KEY (order_id) REFERENCES orders(order_id)
);
```

**Table with CHECK constraint:**
```sql
CREATE TABLE products (
    product_id INT PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    price DECIMAL(10, 2),
    stock INT,
    CHECK (price >= 0),
    CHECK (stock >= 0)
);
```

**Table with IF NOT EXISTS:**
```sql
CREATE TABLE IF NOT EXISTS sessions (
    session_id VARCHAR(255) PRIMARY KEY,
    user_id INT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP
);
```

**Table with multiple constraints:**
```sql
CREATE TABLE employees (
    employee_id INT AUTO_INCREMENT,
    department_id INT NOT NULL,
    email VARCHAR(100),
    salary DECIMAL(10, 2),
    hire_date DATE NOT NULL,
    PRIMARY KEY (employee_id),
    UNIQUE KEY (email),
    FOREIGN KEY (department_id) REFERENCES departments(dept_id),
    CHECK (salary > 0)
);
```

**Table with table options:**
```sql
CREATE TABLE logs (
    log_id BIGINT AUTO_INCREMENT PRIMARY KEY,
    message TEXT,
    created_at TIMESTAMP
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_unicode_ci
  COMMENT='Application logs';
```

**Table with UNSIGNED and ZEROFILL:**
```sql
CREATE TABLE counters (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    counter_value BIGINT UNSIGNED NOT NULL,
    display_value INT ZEROFILL
);
```

### Column Constraint Details

#### NOT NULL / NULL
Specifies whether a column can contain NULL values:
```sql
CREATE TABLE test (
    required_field VARCHAR(50) NOT NULL,
    optional_field VARCHAR(50) NULL
);
```

#### DEFAULT
Provides a default value when no value is specified during INSERT:
```sql
CREATE TABLE articles (
    id INT PRIMARY KEY,
    status VARCHAR(20) DEFAULT 'draft',
    views INT DEFAULT 0,
    published_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

#### AUTO_INCREMENT
Automatically generates sequential integer values:
```sql
CREATE TABLE auto_test (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(50)
);

-- Starting value can be set at table level:
CREATE TABLE auto_test2 (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(50)
) AUTO_INCREMENT = 1000;
```

#### UNIQUE
Ensures all values in a column are unique:
```sql
CREATE TABLE unique_test (
    id INT PRIMARY KEY,
    email VARCHAR(100) UNIQUE,
    username VARCHAR(50) UNIQUE KEY
);
```

#### PRIMARY KEY
Designates a column as the primary key:
```sql
CREATE TABLE pk_test (
    id INT PRIMARY KEY,
    name VARCHAR(50)
);
```

### Table Constraint Details

#### PRIMARY KEY Constraint
```sql
-- Single column:
CREATE TABLE t1 (
    id INT,
    PRIMARY KEY (id)
);

-- Multiple columns (composite):
CREATE TABLE t2 (
    part1 INT,
    part2 INT,
    PRIMARY KEY (part1, part2)
);
```

#### UNIQUE Constraint
```sql
CREATE TABLE unique_multi (
    id INT PRIMARY KEY,
    email VARCHAR(100),
    username VARCHAR(50),
    UNIQUE INDEX idx_email (email),
    UNIQUE KEY idx_username (username)
);
```

#### FOREIGN KEY Constraint
```sql
CREATE TABLE child (
    id INT PRIMARY KEY,
    parent_id INT,
    FOREIGN KEY (parent_id) REFERENCES parent(id)
);

-- With referential actions:
CREATE TABLE child_cascade (
    id INT PRIMARY KEY,
    parent_id INT,
    FOREIGN KEY (parent_id) REFERENCES parent(id)
        ON DELETE CASCADE
        ON UPDATE CASCADE
);
```

#### CHECK Constraint
```sql
CREATE TABLE constrained (
    id INT PRIMARY KEY,
    age INT CHECK (age >= 0 AND age <= 150),
    status VARCHAR(20) CHECK (status IN ('active', 'inactive', 'pending'))
);
```

### Notes

- The `TEMPORARY` keyword is parsed but not currently implemented (see Known Limitations)
- `IF NOT EXISTS` prevents errors when the table already exists
- Column and table constraints can be used interchangeably for single-column constraints
- Foreign key constraints are parsed but require compatible data types between referenced columns
- CHECK constraints are parsed but validation behavior may vary

### Cross-References

- See [Data Types](04_types_and_domains.md) for available column types
- See [ALTER TABLE](#alter-table) for modifying table structure
- See [DROP TABLE](#drop-table) for removing tables
- See [SHOW CREATE TABLE](10_session_show_set.md#show-create-table) to view table definition

---

## ALTER TABLE

Modifies an existing table structure. Currently only table renaming is supported.

### Syntax

```sql
ALTER TABLE table_name RENAME TO new_table_name
```

### Parameters

- `table_name`: Current name of the table
- `new_table_name`: New name for the table

### Examples

**Rename a table:**
```sql
ALTER TABLE users RENAME TO app_users;
```

**Rename with database qualification:**
```sql
ALTER TABLE mydb.old_name RENAME TO mydb.new_name;
```

### Notes

- Only the `RENAME TO` clause is currently implemented
- The table rename operation updates all catalog references
- Renaming a table does not affect data or existing foreign key relationships

### Limitations

Other common `ALTER TABLE` operations are not yet supported:
- Adding/dropping/modifying columns
- Adding/dropping constraints
- Changing table options
- Partitioning operations

### Cross-References

- See [CREATE TABLE](#create-table) for table creation
- See [SHOW CREATE TABLE](10_session_show_set.md#show-create-table) to verify table structure

---

## DROP TABLE

Removes one or more tables from the database.

### Syntax

```sql
DROP TABLE [IF EXISTS] table_name [, table_name ...]
```

### Parameters

- `IF EXISTS`: Prevents error if table does not exist
- `table_name`: Name of the table to drop

### Examples

**Drop a single table:**
```sql
DROP TABLE users;
```

**Drop with IF EXISTS:**
```sql
DROP TABLE IF EXISTS temporary_data;
```

**Drop multiple tables:**
```sql
DROP TABLE logs, sessions, temp_cache;
```

### Notes

- Dropping a table permanently deletes all data in the table
- Foreign key constraints referencing the table may prevent deletion
- This operation cannot be undone

### Safety Warning

**WARNING:** `DROP TABLE` permanently deletes all data. Always verify the table name and ensure you have backups before dropping tables in production.

### Current Status

**Implemented:** The MySQL parser handles DROP [TEMPORARY] TABLE with IF EXISTS, schema qualification, and CASCADE/RESTRICT flags.

### Cross-References

- See [CREATE TABLE](#create-table) for table creation
- See [TRUNCATE TABLE](#truncate-table) for deleting data while keeping structure

---

## TRUNCATE TABLE

Removes all rows from a table while preserving the table structure.

### Syntax

```sql
TRUNCATE [TABLE] table_name
```

### Parameters

- `table_name`: Name of the table to truncate

### Examples

**Truncate a table:**
```sql
TRUNCATE TABLE sessions;
```

**Without TABLE keyword:**
```sql
TRUNCATE logs;
```

### Notes

- `TRUNCATE` is faster than `DELETE FROM table` because it does not generate individual row deletion events
- Auto-increment counters are reset to their initial values
- Triggers are not fired for truncated rows
- Cannot be rolled back in some database systems (behavior varies)

### Differences from DELETE

| Feature | TRUNCATE TABLE | DELETE FROM table |
|---------|---------------|-------------------|
| Speed | Faster | Slower |
| WHERE clause | Not supported | Supported |
| Triggers | Not fired | Fired |
| Auto-increment | Reset | Preserved |
| Transaction log | Minimal | Full |

### Current Status

**Implemented:** The MySQL parser handles TRUNCATE [TABLE] with schema qualification.

### Cross-References

- See [DELETE](07_dml_modification.md#delete) for conditional row deletion
- See [DROP TABLE](#drop-table) for removing the table entirely

---

## Temporary Tables

### Overview

Temporary tables are session-specific tables that are automatically dropped when the session ends. They are useful for intermediate computations and staging data.

### Syntax

```sql
CREATE TEMPORARY TABLE table_name (
    column_definition [, ...]
)
[table_option ...]
```

### Examples

**Create temporary table:**
```sql
CREATE TEMPORARY TABLE temp_calculations (
    id INT,
    result DECIMAL(10, 2)
);
```

**Create temporary table from SELECT:**
```sql
CREATE TEMPORARY TABLE temp_users AS
SELECT * FROM users WHERE created_at > '2024-01-01';
```

### Behavior

- Temporary tables are visible only to the current session
- They are automatically dropped when the session closes
- Temporary tables can have the same name as permanent tables (they shadow permanent tables)
- Data in temporary tables is not persistent across sessions

### Current Status

**CRITICAL LIMITATION:** The `TEMPORARY` keyword is currently parsed but NOT implemented. Tables created with `CREATE TEMPORARY TABLE` will be created as permanent tables instead.

This is a **silent failure** where the syntax is accepted but the behavior is incorrect.

### Workaround

Until temporary table support is fully implemented:
1. Use unique table names to avoid conflicts
2. Manually drop tables when done: `DROP TABLE table_name;`
3. Consider using application-level temporary storage instead

### Cross-References

- See Known Limitations section below for implementation status
- See [CREATE TABLE](#create-table) for permanent table creation

---

## Known Limitations

### Stubbed Implementation

- **CREATE TABLE bytecode mismatch**: The parser generates bytecode that does not fully match the executor's expectations. While basic table creation works, complex constraints and options may not be properly processed.
  - Column constraint encoding may not match executor format
  - Table constraint encoding may have format mismatches
  - Some constraint combinations may not work as expected

### Critical Issues

- **TEMPORARY TABLES - Silent Failure**: The `TEMPORARY` keyword is parsed but completely ignored. Tables created with `CREATE TEMPORARY TABLE` are created as permanent tables instead. This is a critical issue because:
  - No error is raised
  - User expects temporary behavior but gets permanent tables
  - Can lead to data persistence issues
  - **Status:** Requires immediate fix or explicit rejection with error

- **Table Options - Unknown Status**: Table options like `ENGINE`, `CHARSET`, `COLLATE`, and `COMMENT` are parsed but their bytecode emission and catalog storage status is uncertain. They may or may not be preserved.

### Partial Implementation

- **ALTER TABLE - Rename Only**: Only the `RENAME TO` clause is implemented. All other ALTER TABLE operations (ADD COLUMN, DROP COLUMN, MODIFY COLUMN, etc.) are rejected by the parser.

### Missing Features

- **DROP TABLE**: Implemented in the parser.

- **TRUNCATE TABLE**: Implemented in the parser.

- **UNSIGNED and ZEROFILL type modifiers**: These are parsed and stored internally but NOT emitted to bytecode. Constraints are not enforced:
  ```sql
  CREATE TABLE test (
      id INT UNSIGNED,           -- Parsed but not enforced
      display INT ZEROFILL       -- Parsed but not enforced
  );
  ```
  - `UNSIGNED` should generate `CHECK (column >= 0)` constraints
  - `ZEROFILL` should add display formatting metadata

- **Advanced Index Types in CREATE TABLE**: Index types like `FULLTEXT`, `SPATIAL`, and index hints are parsed but may not be correctly emitted:
  ```sql
  CREATE TABLE articles (
      id INT PRIMARY KEY,
      content TEXT,
      FULLTEXT INDEX idx_content (content)  -- Parsed but may not work
  );
  ```

- **Foreign Key Options**: While basic foreign key constraints are supported, advanced options may not work:
  - `MATCH FULL`, `MATCH PARTIAL`, `MATCH SIMPLE`
  - Some referential action combinations

- **CREATE TABLE ... AS SELECT**: Not currently supported:
  ```sql
  CREATE TABLE new_table AS SELECT * FROM old_table;  -- Not supported
  ```

- **CREATE TABLE ... LIKE**: Not currently supported:
  ```sql
  CREATE TABLE new_table LIKE old_table;  -- Not supported
  ```

- **Partitioning**: All partitioning clauses are not supported:
  ```sql
  CREATE TABLE sales (
      id INT,
      sale_date DATE
  ) PARTITION BY RANGE (YEAR(sale_date)) ...;  -- Not supported
  ```

### Spec Deltas

- **INSERT modifiers**: Modifiers like `LOW_PRIORITY`, `HIGH_PRIORITY`, `DELAYED`, and `IGNORE` in CREATE TABLE context are not supported

- **Storage engine specifics**: ENGINE options are parsed but ScratchBird uses its own storage engine. Engine-specific features (MyISAM, InnoDB, MEMORY) do not apply

- **Character set/collation inheritance**: While these can be specified, the actual collation behavior may differ from MySQL

### Implementation Priority

According to `/docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md`:

**Alpha Blockers (Critical):**
- Fix TEMPORARY TABLE handling (reject with error or implement fully)
- Fix CREATE TABLE bytecode format to match executor

**Post-Alpha (High Priority):**
- Implement DROP TABLE
- Implement TRUNCATE TABLE
- Implement UNSIGNED constraints
- Full ALTER TABLE support

**Beta Target (Medium Priority):**
- Full temporary table implementation
- ZEROFILL formatting
- Advanced index types
- Partitioning support
