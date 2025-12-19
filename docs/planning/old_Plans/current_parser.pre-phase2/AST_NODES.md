# ScratchBird AST Node Reference

**Last Updated:** December 4, 2025

This document describes all Abstract Syntax Tree (AST) node types used by the ScratchBird parser.

---

## ASTKind Enum

All AST nodes have a `kind()` method returning one of these values:

### DDL Statement Kinds

| Kind | Description | Class |
|------|-------------|-------|
| `CREATE_TABLE` | CREATE TABLE statement | `CreateTableStmt` |
| `ALTER_TABLE` | ALTER TABLE statement | `AlterTableStmt` |
| `DROP_TABLE` | DROP TABLE statement | `DropTableStmt` |
| `TRUNCATE_TABLE` | TRUNCATE TABLE statement | `TruncateTableStmt` |
| `CREATE_INDEX` | CREATE INDEX statement | `CreateIndexStmt` |
| `DROP_INDEX` | DROP INDEX statement | `DropIndexStmt` |
| `CREATE_SEQUENCE` | CREATE SEQUENCE statement | `CreateSequenceStmt` |
| `ALTER_SEQUENCE` | ALTER SEQUENCE statement | `AlterSequenceStmt` |
| `DROP_SEQUENCE` | DROP SEQUENCE statement | `DropSequenceStmt` |
| `CREATE_VIEW` | CREATE VIEW statement | `CreateViewStmt` |
| `DROP_VIEW` | DROP VIEW statement | `DropViewStmt` |
| `REFRESH_MATERIALIZED_VIEW` | REFRESH MATERIALIZED VIEW | `RefreshMaterializedViewStmt` |
| `CREATE_TABLESPACE` | CREATE TABLESPACE statement | `CreateTablespaceStmt` |
| `ALTER_TABLESPACE` | ALTER TABLESPACE statement | `AlterTablespaceStmt` |
| `DROP_TABLESPACE` | DROP TABLESPACE statement | `DropTablespaceStmt` |
| `ATTACH_TABLESPACE` | ATTACH TABLESPACE statement | `AttachTablespaceStmt` |
| `DETACH_TABLESPACE` | DETACH TABLESPACE statement | `DetachTablespaceStmt` |
| `ALTER_TABLE_SET_TABLESPACE` | ALTER TABLE SET TABLESPACE | `AlterTableSetTablespaceStmt` |
| `CREATE_TRIGGER` | CREATE TRIGGER statement | `CreateTriggerStmt` |
| `DROP_TRIGGER` | DROP TRIGGER statement | `DropTriggerStmt` |

### DML Statement Kinds

| Kind | Description | Class |
|------|-------------|-------|
| `SELECT` | SELECT statement | `SelectStmt` |
| `SET_OPERATION` | UNION/INTERSECT/EXCEPT | `SetOperationStmt` |
| `INSERT` | INSERT statement | `InsertStmt` |
| `UPDATE` | UPDATE statement | `UpdateStmt` |
| `DELETE_STMT` | DELETE statement | `DeleteStmt` |
| `MERGE` | MERGE statement | `MergeStmt` |
| `ANALYZE` | ANALYZE statement | `AnalyzeStmt` |
| `EXPLAIN` | EXPLAIN statement | `ExplainStmt` |

### TCL Statement Kinds

| Kind | Description | Class |
|------|-------------|-------|
| `START_TRANSACTION` | START TRANSACTION | `StartTransactionStmt` |
| `SET_TRANSACTION` | SET TRANSACTION | `SetTransactionStmt` |
| `COMMIT` | COMMIT | `CommitStmt` |
| `ROLLBACK` | ROLLBACK | `RollbackStmt` |

### DCL Statement Kinds

| Kind | Description | Class |
|------|-------------|-------|
| `CREATE_USER` | CREATE USER | `CreateUserStmt` |
| `ALTER_USER` | ALTER USER | `AlterUserStmt` |
| `DROP_USER` | DROP USER | `DropUserStmt` |
| `CREATE_ROLE` | CREATE ROLE | `CreateRoleStmt` |
| `DROP_ROLE` | DROP ROLE | `DropRoleStmt` |
| `CREATE_GROUP` | CREATE GROUP | `CreateGroupStmt` |
| `DROP_GROUP` | DROP GROUP | `DropGroupStmt` |
| `GRANT_PRIVILEGE` | GRANT privileges | `GrantPrivilegeStmt` |
| `REVOKE_PRIVILEGE` | REVOKE privileges | `RevokePrivilegeStmt` |
| `GRANT_ROLE` | GRANT role TO user | `GrantRoleStmt` |
| `REVOKE_ROLE` | REVOKE role FROM user | `RevokeRoleStmt` |
| `SET_ROLE` | SET ROLE | `SetRoleStmt` |
| `SET_SESSION_AUTH` | SET SESSION AUTHORIZATION | `SetSessionAuthStmt` |
| `SET_CONSTRAINTS` | SET CONSTRAINTS | `SetConstraintsStmt` |
| `CREATE_POLICY` | CREATE POLICY (RLS) | `CreatePolicyStmt` |
| `DROP_POLICY` | DROP POLICY | `DropPolicyStmt` |
| `ALTER_TABLE_RLS` | ALTER TABLE ENABLE/DISABLE RLS | `AlterTableRLSStmt` |

### Stored Procedure Kinds

| Kind | Description | Class |
|------|-------------|-------|
| `CREATE_FUNCTION` | CREATE FUNCTION | `CreateFunctionStmt` |
| `CREATE_PROCEDURE` | CREATE PROCEDURE | `CreateProcedureStmt` |
| `BLOCK` | BEGIN...END block | `BlockStmt` |
| `VAR_DECLARATION` | Variable declaration | `VarDeclarationStmt` |
| `ASSIGNMENT` | Variable assignment | `AssignmentStmt` |
| `IF_STMT` | IF statement | `IfStmt` |
| `LOOP_STMT` | LOOP statement | `LoopStmt` |
| `WHILE_STMT` | WHILE statement | `WhileStmt` |
| `EXIT_STMT` | EXIT statement | `ExitStmt` |
| `RETURN_STMT` | RETURN statement | `ReturnStmt` |
| `RAISE_STMT` | RAISE statement | `RaiseStmt` |

### Expression Kinds

| Kind | Description | Class |
|------|-------------|-------|
| `LITERAL` | Literal value | `LiteralExpr` |
| `IDENTIFIER` | Column/table identifier | `IdentifierExpr` |
| `BINARY_OP` | Binary operation | `BinaryOpExpr` |
| `CAST` | CAST expression | `CastExpr` |
| `FUNCTION_CALL` | Function call | `FunctionCallExpr` |
| `AGGREGATE_FUNC` | Aggregate function | `AggregateExpr` |
| `WINDOW_FUNC` | Window function | `WindowFuncExpr` |
| `WINDOW_SPEC` | OVER clause | `WindowSpec` |
| `JSON_FUNC` | JSON operation | `JSONFuncExpr` |
| `COALESCE` | COALESCE expression | `CoalesceExpr` |
| `NULLIF` | NULLIF expression | `NullIfExpr` |
| `CASE` | CASE expression | `CaseExpr` |
| `GROUPING` | GROUPING function | `GroupingExpr` |
| `SUBQUERY` | Subquery expression | `SubqueryExpr` |
| `SEQUENCE_FUNCTION` | NEXTVAL/CURRVAL/SETVAL | `SequenceFunctionExpr` |
| `EXTRACT` | EXTRACT expression | `ExtractExpr` |
| `ARRAY_LITERAL` | ARRAY literal | `ArrayLiteral` |

### Utility Kinds

| Kind | Description | Class |
|------|-------------|-------|
| `SHOW` | SHOW command | `ShowStmt` |
| `DESCRIBE` | DESCRIBE command | `DescribeStmt` |
| `SWEEP` | SWEEP DATABASE | `SweepStmt` |
| `TYPE_NAME` | Type specification | `TypeName` |
| `COLUMN_DEF` | Column definition | `ColumnDef` |
| `TABLE_CONSTRAINT` | Table constraint | `TableConstraint` |
| `SELECT_LIST` | Select list | `SelectList` |
| `WHERE_CLAUSE` | WHERE clause | `WhereClause` |

---

## Expression Classes

### LiteralExpr

Represents literal values.

```cpp
class LiteralExpr : public Expression {
    enum Type { INTEGER, FLOAT, STRING, NULL_LITERAL, RANGE };
    Type literalType() const;
    int64_t intValue() const;        // For INTEGER
    double floatValue() const;       // For FLOAT
    StringId stringValue() const;    // For STRING
    StringId rangeValue() const;     // For RANGE
};
```

### IdentifierExpr

Represents column or table references.

```cpp
class IdentifierExpr : public Expression {
    StringId name() const;           // Column/table name
    StringId qualifier() const;      // Table qualifier (for table.column)
    bool isQualified() const;        // Has qualifier?
};
```

### BinaryOpExpr

Represents binary operations.

```cpp
class BinaryOpExpr : public Expression {
    BinaryOp op() const;
    const Expression* left() const;
    const Expression* right() const;
};

enum class BinaryOp {
    ADD, SUBTRACT, MULTIPLY, DIVIDE, MODULO,  // Arithmetic
    EQ, NE, LT, GT, LE, GE,                   // Comparison
    AND, OR,                                   // Logical
    LIKE, ILIKE,                              // Pattern matching
    IN, NOT_IN,                               // Membership
    ARRAY_OVERLAP, ARRAY_CONTAINS, ARRAY_CONTAINED_BY,  // Array
    REGEX_MATCH, REGEX_MATCH_CI, REGEX_NOT_MATCH, REGEX_NOT_MATCH_CI,  // Regex
    RANGE_STRICTLY_LEFT, RANGE_STRICTLY_RIGHT, RANGE_ADJACENT  // Range
};
```

### CastExpr

Represents CAST and TRY_CAST.

```cpp
class CastExpr : public Expression {
    const Expression* expr() const;
    TypeInfo targetType() const;
    bool isTryCast() const;          // TRY_CAST returns NULL on failure
};
```

### FunctionCallExpr

Represents function calls.

```cpp
class FunctionCallExpr : public Expression {
    StringId name() const;
    const std::vector<Expression*>& args() const;
};
```

### AggregateExpr

Represents aggregate functions.

```cpp
class AggregateExpr : public Expression {
    AggregateFunc func() const;
    const Expression* arg() const;   // NULL for COUNT(*)
    bool distinct() const;           // COUNT(DISTINCT x)
};

enum class AggregateFunc {
    COUNT, SUM, AVG, MIN, MAX, ARRAY_AGG
};
```

### WindowFuncExpr

Represents window functions.

```cpp
class WindowFuncExpr : public Expression {
    WindowFunction func() const;
    const Expression* arg() const;
    const WindowSpec* spec() const;
    int64_t offset() const;          // For LAG/LEAD
    const Expression* defaultValue() const;
};

enum class WindowFunction {
    ROW_NUMBER, RANK, DENSE_RANK,
    LAG, LEAD,
    FIRST_VALUE, LAST_VALUE, NTH_VALUE,
    CUME_DIST, PERCENT_RANK
};
```

### WindowSpec

Represents OVER clause.

```cpp
class WindowSpec {
    const std::vector<Expression*>& partitionBy() const;
    const std::vector<OrderByItem*>& orderBy() const;
    FrameMode frameMode() const;
    FrameBound frameStart() const;
    FrameBound frameEnd() const;
};

enum class FrameMode { ROWS, RANGE, GROUPS };
```

### CaseExpr

Represents CASE expressions.

```cpp
class CaseExpr : public Expression {
    bool isSimpleCase() const;
    const Expression* caseOperand() const;  // For simple CASE
    const std::vector<WhenClause>& whenClauses() const;
    const Expression* elseResult() const;
};

struct WhenClause {
    Expression* condition;
    Expression* result;
};
```

### SubqueryExpr

Represents subqueries.

```cpp
class SubqueryExpr : public Expression {
    SubqueryType type() const;
    const SelectStmt* query() const;
};

enum class SubqueryType {
    SCALAR,      // (SELECT x FROM ...)
    EXISTS,      // EXISTS (SELECT ...)
    IN,          // x IN (SELECT ...)
    NOT_IN,      // x NOT IN (SELECT ...)
    ARRAY        // ARRAY(SELECT ...)
};
```

---

## Statement Classes

### SelectStmt

```cpp
class SelectStmt : public Statement {
    bool distinct() const;
    const SelectList* selectList() const;
    const FromClause* from() const;
    const Expression* where() const;
    const GroupByClause* groupBy() const;
    const Expression* having() const;
    const std::vector<OrderByItem*>& orderBy() const;
    const Expression* limit() const;
    const Expression* offset() const;
    const WithClause* withClause() const;
    bool forUpdate() const;
    bool forShare() const;
};
```

### InsertStmt

```cpp
class InsertStmt : public Statement {
    StringId tableName() const;
    StringId schemaName() const;
    const std::vector<StringId>& columns() const;
    const std::vector<std::vector<Expression*>>& values() const;
    const SelectStmt* selectStmt() const;  // INSERT ... SELECT
    const std::vector<Expression*>& returning() const;
};
```

### UpdateStmt

```cpp
class UpdateStmt : public Statement {
    StringId tableName() const;
    StringId schemaName() const;
    const std::vector<Assignment>& assignments() const;
    const Expression* where() const;
    const FromClause* from() const;  // UPDATE ... FROM
    const std::vector<Expression*>& returning() const;
};
```

### DeleteStmt

```cpp
class DeleteStmt : public Statement {
    StringId tableName() const;
    StringId schemaName() const;
    const Expression* where() const;
    const FromClause* using_() const;  // DELETE ... USING
    const std::vector<Expression*>& returning() const;
};
```

### CreateTableStmt

```cpp
class CreateTableStmt : public Statement {
    StringId tableName() const;
    StringId schemaName() const;
    bool ifNotExists() const;
    const std::vector<ColumnDef*>& columns() const;
    const std::vector<TableConstraint*>& constraints() const;
    StringId tablespace() const;
    StringId charset() const;
    StringId collation() const;
};
```

### StartTransactionStmt

```cpp
class StartTransactionStmt : public Statement {
    TransactionMode mode() const;
    IsolationLevel isolation() const;
    int32_t lockTimeout() const;
    bool wait() const;
    const std::vector<TableReservation>& reservations() const;
};

enum class TransactionMode { READ_WRITE, READ_ONLY };
enum class IsolationLevel { READ_COMMITTED, SNAPSHOT, SNAPSHOT_TABLE_STABILITY };

struct TableReservation {
    StringId tableName;
    TableLockMode lockMode;
    bool forWrite;
};
```

---

## Clause Classes

### FromClause

```cpp
class FromClause {
    const TableRef* baseTable() const;
    const std::vector<JoinClause*>& joins() const;
};
```

### TableRef

```cpp
class TableRef {
    enum Type { TABLE, SUBQUERY, TABLE_FUNCTION };
    Type type() const;
    StringId tableName() const;
    StringId schemaName() const;
    StringId alias() const;
    const SelectStmt* subquery() const;
};
```

### JoinClause

```cpp
class JoinClause {
    JoinType joinType() const;
    const TableRef* table() const;
    const Expression* condition() const;      // ON condition
    const std::vector<StringId>& using_() const;  // USING columns
    bool natural() const;
};

enum class JoinType {
    INNER, LEFT, RIGHT, FULL, CROSS
};
```

### GroupByClause

```cpp
class GroupByClause {
    GroupingType type() const;
    const std::vector<Expression*>& expressions() const;
    const std::vector<std::vector<Expression*>>& groupingSets() const;
};

enum class GroupingType {
    STANDARD, ROLLUP, CUBE, GROUPING_SETS
};
```

### WithClause

```cpp
class WithClause {
    bool recursive() const;
    const std::vector<CTE*>& ctes() const;
};

struct CTE {
    StringId name;
    std::vector<StringId> columns;
    SelectStmt* query;
};
```

### OrderByItem

```cpp
class OrderByItem {
    const Expression* expr() const;
    SortOrder order() const;
    NullsOrder nullsOrder() const;
};

enum class SortOrder { ASC, DESC };
enum class NullsOrder { FIRST, LAST, DEFAULT };
```

---

## Constraint Classes

### ColumnDef

```cpp
class ColumnDef {
    StringId name() const;
    TypeInfo type() const;
    bool nullable() const;
    const Expression* defaultValue() const;
    bool isPrimaryKey() const;
    bool isUnique() const;
    const ForeignKeyConstraint* foreignKey() const;
    const Expression* checkExpr() const;
    bool isGenerated() const;
    const Expression* generatedExpr() const;
    bool isIdentity() const;
    IdentityType identityType() const;
};
```

### ForeignKeyConstraint

```cpp
class ForeignKeyConstraint : public TableConstraint {
    const std::vector<StringId>& columns() const;
    StringId referencedTable() const;
    const std::vector<StringId>& referencedColumns() const;
    FKAction onDelete() const;
    FKAction onUpdate() const;
    bool deferrable() const;
    bool initiallyDeferred() const;
};

enum class FKAction {
    NO_ACTION, RESTRICT, CASCADE, SET_NULL, SET_DEFAULT
};
```

### PrimaryKeyConstraint

```cpp
class PrimaryKeyConstraint : public TableConstraint {
    const std::vector<StringId>& columns() const;
};
```

### UniqueConstraint

```cpp
class UniqueConstraint : public TableConstraint {
    const std::vector<StringId>& columns() const;
};
```

### CheckTableConstraint

```cpp
class CheckTableConstraint : public TableConstraint {
    const Expression* expr() const;
};
```

---

## Memory Management

All AST nodes are allocated from an `ASTAllocator` arena. This provides:
- Fast allocation (bump pointer)
- Automatic deallocation (free entire arena at once)
- No individual `delete` calls needed

```cpp
class ASTAllocator {
    template<typename T, typename... Args>
    T* alloc(Args&&... args);
};
```
