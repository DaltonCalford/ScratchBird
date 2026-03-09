# Specification: CREATE TABLE Statement

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | parser |
| **Spec Version** | 1.0.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:701`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:2775`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_ddl.cpp`

## Synopsis

The CREATE TABLE statement creates a new table in the database with specified columns, constraints, and storage options. Supports temporary tables, table inheritance, partitioning, and CREATE TABLE AS SELECT (CTAS).

## Scope

### In Scope

- Column definitions with data types and constraints
- Table-level constraints (PRIMARY KEY, UNIQUE, FOREIGN KEY, CHECK, EXCLUDE)
- Temporary tables (SESSION, GLOBAL, TRANSACTION-scoped)
- Table inheritance (PostgreSQL-style)
- Table partitioning (RANGE, LIST, HASH)
- CREATE TABLE AS SELECT
- Tablespace specification
- ON COMMIT actions for temporary tables

### Out of Scope

- Column-level statistics (see ANALYZE specification)
- Physical storage parameters (see storage engine specs)

## Background

CREATE TABLE is the fundamental DDL operation for defining relational schema structures. The ScratchBird parser normalizes various SQL dialect syntaxes into a canonical AST representation that captures all table attributes for semantic analysis and catalog storage.

## Specification

### EBNF Grammar

```ebnf
create_table_stmt ::= 
    [ "CREATE" [ "OR" "REPLACE" ] | "RECREATE" ]
    [ "GLOBAL" | "LOCAL" ] [ "TEMPORARY" | "TEMP" ]
    [ "UNLOGGED" ]
    "TABLE" [ "IF" "NOT" "EXISTS" ]
    schema_path
    [ "(" ( column_def | table_constraint ) ("," ( column_def | table_constraint ) )* ")" ]
    [ table_options ]
    [ "AS" ( select_stmt | "WITH" ... ) ]

column_def ::=
    identifier type_name
    [ "CHARACTER" "SET" identifier ]
    [ column_constraint ... ]

column_constraint ::=
    [ "CONSTRAINT" identifier ]
    ( "NOT" "NULL"
    | "NULL"
    | "UNIQUE"
    | "PRIMARY" "KEY"
    | "CHECK" "(" expression ")"
    | "DEFAULT" expression
    | "REFERENCES" schema_path [ "(" identifier ("," identifier )* ")" ]
      [ "ON" "DELETE" referential_action ]
      [ "ON" "UPDATE" referential_action ]
    | "COLLATE" identifier
    | "GENERATED" ( "ALWAYS" | "BY" "DEFAULT" ) "AS" "IDENTITY" [ sequence_options ]
    | "GENERATED" "ALWAYS" "AS" "(" expression ")" [ "STORED" | "VIRTUAL" ]
    )
    [ constraint_deferrable ]

table_constraint ::=
    [ "CONSTRAINT" identifier ]
    ( "PRIMARY" "KEY" identifier_list
    | "UNIQUE" identifier_list
    | "FOREIGN" "KEY" identifier_list "REFERENCES" schema_path identifier_list
      [ "ON" "DELETE" referential_action ] [ "ON" "UPDATE" referential_action ]
    | "CHECK" "(" expression ")"
    | "EXCLUDE" [ "USING" identifier ] exclude_element ("," exclude_element )*
      [ "WHERE" "(" expression ")" ]
    )
    [ constraint_deferrable ]

table_options ::=
    [ "ON" "COMMIT" ( "DELETE" "ROWS" | "PRESERVE" "ROWS" | "DROP" ) ]
    [ "TABLESPACE" schema_path ]
    [ "INHERITS" "(" schema_path ("," schema_path )* ")" ]
    [ "PARTITION" "BY" ( "RANGE" | "LIST" | "HASH" ) "(" identifier_list ")" ]

referential_action ::= "NO" "ACTION" | "RESTRICT" | "CASCADE" | "SET" "NULL" | "SET" "DEFAULT"
constraint_deferrable ::= [ "NOT" ] "DEFERRABLE" [ "INITIALLY" ( "DEFERRED" | "IMMEDIATE" ) ]
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:701
class CreateTableStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateTableStmt; }
    
    // Options
    bool or_replace = false;
    bool if_not_exists = false;
    TempTableType temp_type = TempTableType::NONE;  // NONE, SESSION, TRANSACTION, GLOBAL
    TempOnCommitAction on_commit = TempOnCommitAction::NONE;
    bool unlogged = false;

    // Table path
    SchemaPath table_path;

    // Column definitions
    std::vector<ColumnDef*> columns;

    // Table constraints
    std::vector<TableConstraint*> constraints;

    // Storage options
    SchemaPath tablespace;
    bool has_tablespace = false;

    // Inheritance (PostgreSQL-style)
    std::vector<SchemaPath> inherits;

    // Partitioning
    bool is_partitioned = false;
    StringPool::StringId partition_by = StringPool::INVALID_ID;  // RANGE, LIST, HASH
    std::vector<StringPool::StringId> partition_columns;

    // MySQL CREATE TABLE ... LIKE <source>
    bool has_like_source = false;
    SchemaPath like_source;

    // CREATE TABLE AS SELECT
    SelectStmt* as_query = nullptr;
};

// Column definition
struct ColumnDef : public ASTNode {
    ASTKind kind() const override { return ASTKind::ColumnDef; }
    
    StringPool::StringId name = StringPool::INVALID_ID;
    TypeName type;
    StringPool::StringId charset = StringPool::INVALID_ID;
    std::vector<ColumnConstraint> constraints;

    // Computed column
    bool is_computed = false;
    Expression* computed_expr = nullptr;
    bool computed_stored = false;  // STORED vs VIRTUAL
};

// Table-level constraint
struct TableConstraint : public ASTNode {
    ASTKind kind() const override { return ASTKind::TableConstraint; }
    
    TableConstraintType type;  // PRIMARY_KEY, UNIQUE, FOREIGN_KEY, CHECK, EXCLUDE
    StringPool::StringId name = StringPool::INVALID_ID;
    std::vector<StringPool::StringId> columns;
    Expression* check_expr = nullptr;
    SchemaPath ref_table;
    std::vector<StringPool::StringId> ref_columns;
    ForeignKeyAction on_delete = ForeignKeyAction::NO_ACTION;
    ForeignKeyAction on_update = ForeignKeyAction::NO_ACTION;
    std::vector<Expression*> exclude_expressions;
    std::vector<StringPool::StringId> exclude_operators;
    Expression* exclude_where = nullptr;
    StringPool::StringId index_method = StringPool::INVALID_ID;
    bool deferrable = false;
    bool initially_deferred = false;
};
```

### Semantic Binding Rules

1. **Table Path Resolution**
   - If single identifier: resolve in current schema
   - If schema-qualified: resolve schema first, then table name
   - Error if table exists (unless IF NOT EXISTS or OR REPLACE)

2. **Column Definition Validation**
   - Type name must resolve to valid type in type registry
   - Computed columns must have valid expressions
   - Identity columns must have valid sequence options

3. **Constraint Validation**
   - Foreign key references must resolve to existing tables/columns
   - Check constraints must have valid boolean expressions
   - Named constraints must be unique within table

4. **Temporary Table Rules**
   - SESSION temp tables persist for connection lifetime
   - TRANSACTION temp tables truncate at transaction end
   - GLOBAL temp tables share structure, private data per session

### Interface Contracts

#### Function: `parseCreateTable()`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:2775
CreateTableStmt* Parser::parseCreateTable(bool or_replace, TempTableType temp_type);
```

**Preconditions:**
- Parser positioned after CREATE [OR REPLACE] [TEMP] TABLE keywords
- Valid table name follows

**Postconditions:**
- Returns populated CreateTableStmt AST node
- Parser positioned at statement end or semicolon

**Error Handling:**
- PRS_0504: Missing table name
- PRS_0504: Invalid column definition
- PRS_0504: Missing closing parenthesis

## Examples

```sql
-- Basic table
CREATE TABLE users (
    id INT PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    username VARCHAR(50) NOT NULL UNIQUE,
    email VARCHAR(100) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- With foreign key
CREATE TABLE orders (
    order_id INT PRIMARY KEY,
    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    total DECIMAL(10,2) CHECK (total >= 0),
    CONSTRAINT valid_total CHECK (total >= 0)
);

-- Temporary table
CREATE GLOBAL TEMPORARY TABLE temp_results (
    session_id VARCHAR(50),
    result_data TEXT
) ON COMMIT PRESERVE ROWS;

-- Partitioned table
CREATE TABLE events (
    event_id BIGINT,
    event_time TIMESTAMP,
    payload JSON
) PARTITION BY RANGE (event_time);

-- CTAS
CREATE TABLE active_users AS
SELECT * FROM users WHERE last_login > CURRENT_DATE - INTERVAL '30 days';
```

## Invariants

1. **Column Name Uniqueness**: All column names within a table must be unique
2. **Constraint Name Uniqueness**: Named constraints must be unique within a table
3. **Type Validity**: All column types must be resolvable to catalog types
4. **Temporary Table Restrictions**: Cannot create indexes on temp tables before data insertion

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `PRS_0504` | Missing table name | Provide table name |
| `PRS_0504` | Duplicate column name | Rename or remove duplicate |
| `PRS_0504` | Invalid data type | Use valid type name |
| `PRS_0504` | Missing constraint name for named constraint | Add CONSTRAINT keyword |

## Related Specifications

- [stmt_create_index.md](./stmt_create_index.md) - Index creation for tables
- [stmt_alter_table.md](./stmt_alter_table.md) - Table modification
- [stmt_drop_table.md](./stmt_drop_table.md) - Table removal
- [semantic_binding_flow.md](./semantic_binding_flow.md) - Name resolution

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
