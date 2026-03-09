# Specification: Semantic Binding Flow

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | parser / semantic-analyzer |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | Generated from source analysis |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:308` - ASTNode base class
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:323` - Statement base class
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:103` - ASTKind enum (statement types)
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/schema_path_v3.h:78` - SchemaPath with StringPool IDs
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/v3_compiler.cpp:9` - Compiler binding flow
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/v3_emitter.cpp` - Emitter implementation
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_parser_capability_contract.cpp` - Catalog binding tests

## Synopsis

This specification defines how parsed AST statements (containing unresolved SchemaPath references with StringPool IDs) bind to catalog objects (identified by UUIDs). The binding flow occurs in three phases: parsing (unresolved AST), semantic analysis (path to UUID resolution), and emission (bound IR generation).

## Scope

### In Scope

- Unresolved AST node structure (SchemaPath with StringPool IDs)
- Statement type enumeration (ASTKind)
- Visitor pattern for AST traversal
- Compiler integration (parse → emit flow)
- Container encoding for bound statements

### Out of Scope

- Catalog lookup implementation (in catalog subsystem)
- UUID generation and management
- Permission checking during binding
- Query optimization post-binding

## Background

The ScratchBird parser produces an "unresolved" AST where:
- Object references are SchemaPath structures with StringPool StringIds
- Types are TypeName structures with optional schema paths
- Column references may have table path qualifiers

Semantic binding resolves these StringId references to UUIDs by:
1. Looking up schema paths in the catalog namespace
2. Resolving table aliases in the current query scope
3. Binding column names to table column UUIDs
4. Validating type references

## Specification

### Data Structures

#### ASTNode Base Class

```cpp
// From include/scratchbird/parser/ast_v3.h:308
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual ASTKind kind() const = 0;

    SourceSpan span;  // Source location for error reporting

protected:
    ASTNode() = default;
    explicit ASTNode(SourceSpan s) : span(s) {}
};
```

#### Statement Base Class

```cpp
// From include/scratchbird/parser/ast_v3.h:323
class Statement : public ASTNode {
public:
    virtual void accept(ASTVisitor& visitor) = 0;

protected:
    Statement() = default;
    explicit Statement(SourceSpan s) : ASTNode(s) {}
};
```

#### ASTKind Enum (Statement Types)

```cpp
// From include/scratchbird/parser/ast_v3.h:103
enum class ASTKind : uint16_t {
    // DDL Statements
    CreateTableStmt, CreateIndexStmt, CreateViewStmt, CreateSequenceStmt,
    AlterSequenceStmt, CreateSchemaStmt, DropSchemaStmt, AlterSchemaStmt,
    CreateDatabaseStmt, CreateTablespaceStmt, AlterTablespaceStmt,
    // ... (60+ statement kinds)

    // DML Statements
    SelectStmt, InsertStmt, UpdateStmt, DeleteStmt, CopyStmt, MergeStmt,

    // Transaction Statements
    StartTransactionStmt, PrepareTransactionStmt, CommitStmt, RollbackStmt,
    SavepointStmt, ReleaseSavepointStmt,

    // Session Statements
    SetStmt, AlterSystemStmt, ResetStmt, ShowStmt, ExplainStmt, AnalyzeStmt,

    // PSQL Statements
    ExecuteBlockStmt, CompoundStmt, DeclareVariableStmt, AssignmentStmt,
    IfStmt, WhileStmt, ForSelectStmt, LoopStmt, LeaveStmt, ContinueStmt,

    // Expressions
    LiteralExpr, ColumnRefExpr, ParameterExpr, BinaryExpr, UnaryExpr,
    FunctionCallExpr, CastExpr, CaseExpr, SubqueryExpr, ExistsExpr,
    // ... (30+ expression kinds)
};
```

#### CreateTableStmt (Example DDL)

```cpp
// From include/scratchbird/parser/ast_v3.h:701
class CreateTableStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateTableStmt; }
    void accept(ASTVisitor& visitor) override;

    // Options
    bool or_replace = false;
    bool if_not_exists = false;
    TempTableType temp_type = TempTableType::NONE;
    TempOnCommitAction on_commit = TempOnCommitAction::NONE;
    bool unlogged = false;

    // Table path (UNRESOLVED - StringPool IDs)
    SchemaPath table_path;

    // Column definitions
    std::vector<ColumnDef*> columns;

    // Table constraints
    std::vector<TableConstraint*> constraints;

    // Storage options
    SchemaPath tablespace;
    bool has_tablespace = false;

    // Inheritance (PostgreSQL-style) - UNRESOLVED paths
    std::vector<SchemaPath> inherits;

    // Partitioning
    bool is_partitioned = false;
    StringPool::StringId partition_by = StringPool::INVALID_ID;
    std::vector<StringPool::StringId> partition_columns;

    // CREATE TABLE AS SELECT
    SelectStmt* as_query = nullptr;
};
```

#### SelectStmt (Example DML)

```cpp
// From include/scratchbird/parser/ast_v3.h (referenced in parser_v3.h:286)
class SelectStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::SelectStmt; }
    void accept(ASTVisitor& visitor) override;

    // WITH clause (common table expressions)
    WithClause* with_clause = nullptr;

    // SELECT list
    std::vector<SelectItem*> select_list;

    // FROM clause - UNRESOLVED table references
    FromClause* from_clause = nullptr;

    // WHERE clause
    Expression* where_clause = nullptr;

    // GROUP BY
    GroupByClause* group_by = nullptr;

    // HAVING
    Expression* having_clause = nullptr;

    // ORDER BY
    std::vector<OrderByItem*> order_by;

    // LIMIT/OFFSET
    Expression* limit = nullptr;
    Expression* offset = nullptr;

    // Set operations (UNION, INTERSECT, EXCEPT)
    SetOperation* set_op = nullptr;
};
```

#### ColumnRefExpr (Example Expression)

```cpp
// From include/scratchbird/parser/ast_v3.h (implied by ColumnRef)
class ColumnRefExpr : public Expression {
public:
    ASTKind kind() const override { return ASTKind::ColumnRefExpr; }
    void accept(ASTVisitor& visitor) override;

    // Table qualifier (UNRESOLVED path)
    SchemaPath table_path;
    bool has_table_qualifier = false;

    // Column name (StringPool ID - resolved during binding)
    StringPool::StringId column_name = StringPool::INVALID_ID;
};
```

### Binding Flow Architecture

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│   SQL Source    │────▶│      Lexer       │────▶│     Tokens      │
└─────────────────┘     └──────────────────┘     └────────┬────────┘
                                                         │
                              ┌──────────────────────────┘
                              ▼
                    ┌──────────────────┐
                    │      Parser      │
                    │  (parser_v3.cpp) │
                    └────────┬─────────┘
                             │ Unresolved AST
                             │ (SchemaPath with StringIds)
                             ▼
                    ┌──────────────────┐
                    │  Semantic Binder │
                    │ (catalog lookup) │
                    └────────┬─────────┘
                             │ Bound AST
                             │ (UUIDs resolved)
                             ▼
                    ┌──────────────────┐
                    │     Emitter      │
                    │ (v3_emitter.cpp) │
                    └────────┬─────────┘
                             │ SBLR Container
                             ▼
                    ┌──────────────────┐
                    │      encode      │
                    │  (v3_container)  │
                    └────────┬─────────┘
                             │ Bytecode
                             ▼
                    ┌──────────────────┐
                    │  Execution/Store │
                    └──────────────────┘
```

### Interface Contracts

#### Function: `Compiler::compile()`

```cpp
// Source: src/parser/v3_compiler.cpp:9
CompileResult Compiler::compile(std::string_view sql);
```

**Preconditions:**
- Valid SQL source string
- Parser and emitter subsystems initialized

**Postconditions:**
- Returns CompileResult with bytecode or error
- On success, result.bytecode contains encoded container
- On failure, result.error contains description

**Algorithm:**
```
Input:  SQL source string
Output: CompileResult (bytecode or error)

1. Create parser with SQL input
   Parser parser(sql)

2. Parse statement
   ParseResult parse_result = parser.parseStatement()

3. Check parse success
   if !parse_result.success():
       result.ok = false
       result.error = parse_result.errors().front().message
       return result

4. Create emitter with string pool
   V3Emitter emitter(parser.stringPool())

5. Create container for output
   Container container

6. Emit statement to container
   if !emitter.emitStatementToContainer(parse_result.statement(), 
                                         container, err):
       result.ok = false
       result.error = err
       return result

7. Encode container to bytecode
   vector<uint8_t> encoded
   if !encodeContainer(container, encoded, err):
       result.ok = false
       result.error = err
       return result

8. Return success
   result.ok = true
   result.bytecode = encoded
   return result
```

#### AST Visitor Pattern

```cpp
// From include/scratchbird/parser/ast_v3.h:92
class ASTVisitor {
public:
    // DDL statements
    virtual void visit(CreateTableStmt& stmt) = 0;
    virtual void visit(CreateIndexStmt& stmt) = 0;
    virtual void visit(CreateViewStmt& stmt) = 0;
    // ... (all statement types)

    // DML statements
    virtual void visit(SelectStmt& stmt) = 0;
    virtual void visit(InsertStmt& stmt) = 0;
    virtual void visit(UpdateStmt& stmt) = 0;
    virtual void visit(DeleteStmt& stmt) = 0;

    // Expressions
    virtual void visit(LiteralExpr& expr) = 0;
    virtual void visit(ColumnRefExpr& expr) = 0;
    virtual void visit(BinaryExpr& expr) = 0;
    // ... (all expression types)
};
```

### Binding Resolution Process

#### Phase 1: Schema Path Resolution

```cpp
// Pseudocode for schema path binding
CatalogObjectId resolveSchemaPath(SchemaPath& path, CatalogManager& catalog) {
    // Path types require different resolution strategies
    switch (path.type) {
        case PathType::UNQUALIFIED:
            // Use search path if no_search_path is false
            if (path.no_search_path) {
                return catalog.lookupInCurrentSchema(path.components);
            } else {
                return catalog.lookupInSearchPath(path.components);
            }
            
        case PathType::CURRENT:
            // Resolve relative to current schema context
            return catalog.lookupRelativeToCurrent(path.components);
            
        case PathType::PARENT:
            // Resolve relative to parent schema context
            return catalog.lookupRelativeToParent(path.components);
            
        case PathType::ABSOLUTE:
            // Resolve from root namespace
            return catalog.lookupAbsolute(path.components);
    }
}
```

#### Phase 2: Table Reference Binding

```cpp
// Pseudocode for table reference binding
BoundTableRef bindTableRef(TableRef& ref, BindingContext& ctx) {
    BoundTableRef bound;
    
    // Resolve the schema path to catalog object
    bound.object_id = resolveSchemaPath(ref.path, ctx.catalog);
    
    // Validate it's a table/view type
    if (!ctx.catalog.isTableOrView(bound.object_id)) {
        throw BindingError("Expected table or view");
    }
    
    // Get column information
    bound.columns = ctx.catalog.getColumns(bound.object_id);
    
    // Register alias in current scope if present
    if (ref.has_alias) {
        StringPool::StringId alias_name = ref.alias;
        ctx.registerAlias(alias_name, bound);
    } else {
        // Register using the base name as implicit alias
        StringPool::StringId base_name = ref.path.objectName();
        ctx.registerAlias(base_name, bound);
    }
    
    return bound;
}
```

#### Phase 3: Column Reference Binding

```cpp
// Pseudocode for column reference binding
BoundColumnRef bindColumnRef(ColumnRefExpr& expr, BindingContext& ctx) {
    BoundColumnRef bound;
    
    if (expr.has_table_qualifier) {
        // Qualified column: table.column or schema.table.column
        
        // First resolve table path
        CatalogObjectId table_id = resolveSchemaPath(expr.table_path, ctx.catalog);
        
        // Then resolve column within that table
        bound.column_id = ctx.catalog.lookupColumn(table_id, expr.column_name);
        bound.table_id = table_id;
        
    } else {
        // Unqualified column: search in scope
        
        // Check current scope (FROM clause items)
        for (const auto& scope_entry : ctx.currentScope()) {
            CatalogObjectId col_id = ctx.catalog.findColumn(
                scope_entry.table_id, 
                expr.column_name
            );
            if (col_id.isValid()) {
                if (bound.column_id.isValid()) {
                    throw BindingError("Ambiguous column reference");
                }
                bound.column_id = col_id;
                bound.table_id = scope_entry.table_id;
            }
        }
        
        if (!bound.column_id.isValid()) {
            throw BindingError("Column not found: " + 
                ctx.stringPool.get(expr.column_name));
        }
    }
    
    return bound;
}
```

### Binding Context State Machine

```
┌─────────────────┐
│  Initial State  │
│  (no bindings)  │
└────────┬────────┘
         │ Parse DDL/DML statement
         ▼
┌─────────────────┐
│  Schema Level   │
│ Resolve schema  │
│ paths to UUIDs  │
└────────┬────────┘
         │ Enter DML scope
         ▼
┌─────────────────┐
│   FROM Scope    │
│ Bind table refs │
│ Register aliases│
└────────┬────────┘
         │ Process SELECT/WHERE
         ▼
┌─────────────────┐
│  Column Scope   │
│ Bind column refs│
│ Validate types  │
└────────┬────────┘
         │ Complete binding
         ▼
┌─────────────────┐
│  Bound State    │
│ All UUIDs known │
└─────────────────┘
```

### Type Reference Binding

```cpp
// From include/scratchbird/parser/ast_v3.h:351
struct TypeName {
    StringPool::StringId name = StringPool::INVALID_ID;
    bool has_schema_path = false;
    SchemaPath schema_path;  // For qualified types (schema.type)
    // ... other fields
};

// Binding process:
CatalogObjectId bindTypeName(TypeName& type, CatalogManager& catalog) {
    if (type.has_schema_path) {
        // Qualified type: schema.type or .type
        return catalog.lookupType(type.schema_path, type.name);
    } else {
        // Unqualified type: search in type search path
        return catalog.lookupTypeByName(type.name);
    }
}
```

### Foreign Key Reference Binding

```cpp
// From include/scratchbird/parser/ast_v3.h:405
struct ColumnConstraint {
    ConstraintType type;
    // For REFERENCES (foreign key)
    SchemaPath ref_table;  // UNRESOLVED - needs binding
    std::vector<StringPool::StringId> ref_columns;
    ForeignKeyAction on_delete = ForeignKeyAction::NO_ACTION;
    ForeignKeyAction on_update = ForeignKeyAction::NO_ACTION;
    // ...
};

// Binding during CREATE TABLE:
void bindForeignKeyConstraint(ColumnConstraint& constraint, 
                               CatalogManager& catalog,
                               SchemaId current_schema) {
    // Resolve referenced table
    CatalogObjectId ref_table_id = resolveSchemaPath(
        constraint.ref_table, 
        catalog
    );
    
    // Validate referenced columns exist
    for (auto col_name : constraint.ref_columns) {
        CatalogObjectId col_id = catalog.lookupColumn(ref_table_id, col_name);
        if (!col_id.isValid()) {
            throw BindingError("Referenced column not found");
        }
    }
    
    // Store binding
    constraint.bound_ref_table_id = ref_table_id;
}
```

## Invariants

1. **StringId Validity Invariant**: All StringPool::StringId values in AST must be valid (not INVALID_ID = 0) after parsing
   - Verification: Parser validation before returning AST

2. **Span Coverage Invariant**: Every AST node has a valid SourceSpan covering its source text
   - Verification: All parse methods set node->span before returning

3. **Hierarchy Invariant**: Parent nodes own child nodes (arena-allocated)
   - Verification: ASTArena manages all node lifetimes

4. **Binding Completeness Invariant**: After semantic binding, all SchemaPath references are resolved to UUIDs
   - Verification: Binding visitor asserts all paths resolved

5. **Type Consistency Invariant**: Bound AST maintains type consistency across references
   - Verification: Type checking pass after binding

## Error Handling

| Error Condition | Phase | Recovery |
|-----------------|-------|----------|
| Schema not found | Binding | Report missing object |
| Table not found | Binding | Report unknown table |
| Column not found | Binding | Report unknown column |
| Ambiguous column | Binding | Report ambiguity, suggest qualification |
| Type mismatch | Type Check | Report type error |
| Permission denied | Authorization | Report access error |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_catalog_parser_capability_contract.cpp` | Catalog binding contracts |
| `tests/unit/test_emulated_parser_boundary_contracts.cpp` | Parser boundary contracts |
| `tests/v3/parser/test_type_and_literal_spec.md` | Type resolution |

## Related Specifications

- [V3 Canonical Grammar](./v3_canonical_grammar.md) - Parser output format
- [Path Resolution Logic](./path_resolution_logic.md) - Path syntax and parsing

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Unresolved AST | AST with StringPool ID references |
| Bound AST | AST with UUID references to catalog |
| SchemaPath | Hierarchical object reference structure |
| StringPool | Interned string storage for identifiers |
| Binding Context | Scope tracking during semantic analysis |

### References

- ScratchBird PARSER_V3_IMPLEMENTATION_PLAN.md Section 9
- ScratchBird SBIR_SPECIFICATION.md (Intermediate Representation)

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | Source Analysis |
