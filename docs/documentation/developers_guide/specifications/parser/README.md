# ScratchBird Parser Statement Specifications

This directory contains comprehensive specifications for all SQL statement types parsed by the ScratchBird v3.0 parser.

## DDL Statements

### CREATE Statements
| File | Description |
|------|-------------|
| [stmt_create_table.md](./stmt_create_table.md) | CREATE TABLE with columns, constraints, partitioning |
| [stmt_create_index.md](./stmt_create_index.md) | CREATE INDEX with 58+ index types |
| [stmt_create_view.md](./stmt_create_view.md) | CREATE VIEW and CREATE MATERIALIZED VIEW |
| [stmt_create_sequence.md](./stmt_create_sequence.md) | CREATE SEQUENCE generator |
| [stmt_create_schema.md](./stmt_create_schema.md) | CREATE SCHEMA namespace |
| [stmt_create_database.md](./stmt_create_database.md) | CREATE DATABASE instance |
| [stmt_create_function.md](./stmt_create_function.md) | CREATE FUNCTION/PROCEDURE |
| [stmt_create_trigger.md](./stmt_create_trigger.md) | CREATE TRIGGER for tables/database |

### ALTER Statements
| File | Description |
|------|-------------|
| [stmt_alter_table.md](./stmt_alter_table.md) | ALTER TABLE - columns, constraints, RLS |
| [stmt_alter_index.md](./stmt_alter_index.md) | ALTER INDEX - rebuild, options, enable/disable |
| [stmt_alter_sequence.md](./stmt_alter_sequence.md) | ALTER SEQUENCE - modify generator properties |
| [stmt_alter_schema.md](./stmt_alter_schema.md) | ALTER SCHEMA - rename, owner, path |
| [stmt_alter_database.md](./stmt_alter_database.md) | ALTER DATABASE - rename, owner, options |

### DROP Statements
| File | Description |
|------|-------------|
| [stmt_drop_table.md](./stmt_drop_table.md) | DROP TABLE |
| [stmt_drop_index.md](./stmt_drop_index.md) | DROP INDEX |
| [stmt_drop_view.md](./stmt_drop_view.md) | DROP VIEW |
| [stmt_drop_sequence.md](./stmt_drop_sequence.md) | DROP SEQUENCE |
| [stmt_drop_schema.md](./stmt_drop_schema.md) | DROP SCHEMA |
| [stmt_drop_database.md](./stmt_drop_database.md) | DROP DATABASE |
| [stmt_drop_function.md](./stmt_drop_function.md) | DROP FUNCTION/PROCEDURE/TRIGGER |
| [stmt_truncate_table.md](./stmt_truncate_table.md) | TRUNCATE TABLE |

## DML Statements

| File | Description |
|------|-------------|
| [stmt_select.md](./stmt_select.md) | SELECT with CTEs, window functions, set ops |
| [stmt_insert.md](./stmt_insert.md) | INSERT with VALUES, SELECT, ON CONFLICT |
| [stmt_update.md](./stmt_update.md) | UPDATE with FROM, WHERE, RETURNING |
| [stmt_delete.md](./stmt_delete.md) | DELETE with USING, WHERE, RETURNING |
| [stmt_merge.md](./stmt_merge.md) | MERGE (UPSERT) - SQL:2003 standard |

## Transaction & Session

| File | Description |
|------|-------------|
| [stmt_transaction.md](./stmt_transaction.md) | BEGIN, COMMIT, ROLLBACK, SAVEPOINT |

## Security Statements

| File | Description |
|------|-------------|
| [stmt_grant_revoke.md](./stmt_grant_revoke.md) | GRANT and REVOKE privileges |
| [stmt_rls.md](./stmt_rls.md) | Row-Level Security (CREATE/ALTER/DROP POLICY) |

## Utility Statements

| File | Description |
|------|-------------|
| [stmt_copy.md](./stmt_copy.md) | COPY - data import/export |
| [stmt_explain.md](./stmt_explain.md) | EXPLAIN - execution plans |
| [stmt_analyze.md](./stmt_analyze.md) | ANALYZE - statistics collection |

## Specification Template

Each specification follows the standardized template defined in [TEMPLATE.md](../TEMPLATE.md), including:

- **Metadata**: Subsystem, version, status
- **Coverage and Evidence Status**: Source anchors to implementation
- **Synopsis**: One-paragraph summary
- **Scope**: In-scope and out-of-scope items
- **Background**: Context and concepts
- **Specification**: EBNF grammar, AST structures, semantic rules
- **Interface Contracts**: Function signatures and contracts
- **Examples**: SQL usage examples
- **Invariants**: Must-always-hold properties
- **Error Handling**: Error codes and recovery
- **Related Specifications**: Cross-references
- **Changelog**: Version history

## ASTKind Reference

All statement types are identified by the `ASTKind` enum in `ast_v3.h`:

```cpp
// DDL
CreateTableStmt, CreateIndexStmt, CreateViewStmt, CreateSequenceStmt,
CreateSchemaStmt, CreateDatabaseStmt, CreateFunctionStmt, CreateProcedureStmt,
CreateTriggerStmt, AlterTableStmt, AlterIndexStmt, AlterSequenceStmt,
DropTableStmt, DropIndexStmt, DropViewStmt, DropSequenceStmt, ...

// DML
SelectStmt, InsertStmt, UpdateStmt, DeleteStmt, MergeStmt, CopyStmt

// Transaction
StartTransactionStmt, CommitStmt, RollbackStmt, SavepointStmt, 
PrepareTransactionStmt

// Security
GrantStmt, RevokeStmt, CreatePolicyStmt, AlterPolicyStmt, DropPolicyStmt

// Utility
ExplainStmt, AnalyzeStmt
```

## Parser Implementation

The parser implementation is in `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp`.

Key entry points:
- `parseStatement()` - Main statement dispatch
- `parseCreate()` - CREATE statement dispatch
- `parseAlter()` - ALTER statement dispatch
- `parseDrop()` - DROP statement dispatch
- `parseSelect()` - SELECT statement
- `parseInsert()` - INSERT statement
- `parseUpdate()` - UPDATE statement
- `parseDelete()` - DELETE statement
- `parseMerge()` - MERGE statement

## Version

Specifications version: 1.0.0  
Last updated: 2026-03-08
