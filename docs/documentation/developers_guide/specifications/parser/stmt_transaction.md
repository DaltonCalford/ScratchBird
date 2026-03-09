# Specification: Transaction Statements

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

- Source anchors: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:3934-4030`
- Source anchors: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:13941-14350`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_transaction.cpp`

## Synopsis

Transaction statements control transaction boundaries, isolation levels, and commit/rollback behavior. Supports SQL standard and Firebird legacy transaction options.

## Specification

### EBNF Grammar

```ebnf
-- START TRANSACTION / BEGIN
start_transaction_stmt ::=
    ( "BEGIN" | "START" "TRANSACTION" )
    [ transaction_mode ... ]

transaction_mode ::=
    "ISOLATION" "LEVEL" ( "READ" "UNCOMMITTED" | "READ" "COMMITTED" |
                          "REPEATABLE" "READ" | "SERIALIZABLE" )
  | "READ" ( "WRITE" | "ONLY" )
  | "DEFERRABLE" | "NOT" "DEFERRABLE"
  | "WAIT" [ integer ] | "NO" "WAIT"
  | "LOCK" "TIMEOUT" integer
  | "READ" "COMMITTED" ( "READ" "CONSISTENCY" | "RECORD" "VERSION" | "NO" "RECORD" "VERSION" )
  | "RESERVING" table_reservation ("," table_reservation )*

table_reservation ::=
    identifier "FOR" [ ( "SHARED" | "PROTECTED" ) ]
    ( "READ" | "WRITE" )

-- COMMIT
commit_stmt ::=
    "COMMIT" [ "WORK" ] [ "TRANSACTION" identifier ]
    [ "AND" ( "CHAIN" | "NO" "CHAIN" ) ]
    [ "NO" "RELEASE" | "RELEASE" ]
    [ "PREPARED" string ]

-- ROLLBACK
rollback_stmt ::=
    "ROLLBACK" [ "WORK" ] [ "TRANSACTION" identifier ]
    [ "AND" ( "CHAIN" | "NO" "CHAIN" ) ]
    [ "NO" "RELEASE" | "RELEASE" ]
    [ "TO" [ "SAVEPOINT" ] identifier ]

-- SAVEPOINT
savepoint_stmt ::= "SAVEPOINT" identifier

-- RELEASE SAVEPOINT
release_savepoint_stmt ::= "RELEASE" [ "SAVEPOINT" ] identifier [ "AND" "CHAIN" | "AND" "NO" "CHAIN" ]

-- PREPARE TRANSACTION (2PC)
prepare_transaction_stmt ::= "PREPARE" "TRANSACTION" string
```

### AST Node Structures

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:3934
class StartTransactionStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::StartTransactionStmt; }
    
    bool has_isolation_level = false;
    IsolationLevel isolation_level = IsolationLevel::READ_COMMITTED;

    bool has_access_mode = false;
    TransactionAccess access_mode = TransactionAccess::READ_WRITE;

    bool has_read_committed_mode = false;
    ReadCommittedMode read_committed_mode = ReadCommittedMode::DEFAULT;

    bool deferrable = false;
    bool not_deferrable = false;

    // Firebird legacy options
    bool has_wait_mode = false;
    TransactionWaitMode wait_mode = TransactionWaitMode::WAIT;
    bool has_lock_timeout = false;
    uint32_t lock_timeout_seconds = 0;
    std::vector<TableReservation> table_reservations;

    // ScratchBird extensions
    bool has_autocommit = false;
    AutocommitMode autocommit_mode = AutocommitMode::UNCHANGED;
    TransactionConflictAction conflict_action = TransactionConflictAction::DEFAULT;
    bool has_conflict_error_code = false;
    int32_t conflict_error_code = 0;
};

// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:3984
class CommitStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CommitStmt; }
    
    bool and_chain = false;
    bool and_no_chain = false;
    bool release = false;
    bool no_release = false;
    StringPool::StringId prepared_gid = StringPool::INVALID_ID;
};

// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:4002
class RollbackStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::RollbackStmt; }
    
    bool and_chain = false;
    bool and_no_chain = false;
    bool release = false;
    bool no_release = false;
    bool to_savepoint = false;
    StringPool::StringId savepoint_name = StringPool::INVALID_ID;
};

// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:4018
class SavepointStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::SavepointStmt; }
    
    StringPool::StringId name = StringPool::INVALID_ID;
};

// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:4026
class ReleaseSavepointStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::ReleaseSavepointStmt; }
    
    StringPool::StringId name = StringPool::INVALID_ID;
};

// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:3973
class PrepareTransactionStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::PrepareTransactionStmt; }
    
    StringPool::StringId gid = StringPool::INVALID_ID;
};

// Enums
enum class IsolationLevel : uint8_t {
    READ_UNCOMMITTED, READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE
};

enum class TransactionAccess : uint8_t {
    READ_WRITE, READ_ONLY
};

enum class TransactionWaitMode : uint8_t {
    NO_WAIT = 0, WAIT = 1
};

enum class ReadCommittedMode : uint8_t {
    DEFAULT = 0, READ_CONSISTENCY = 1, RECORD_VERSION = 2, NO_RECORD_VERSION = 3
};
```

## Examples

```sql
-- Begin transaction
BEGIN;

-- Begin with isolation level
START TRANSACTION ISOLATION LEVEL SERIALIZABLE READ ONLY;

-- Firebird style
START TRANSACTION READ COMMITTED RECORD VERSION WAIT LOCK TIMEOUT 10;

-- Commit
COMMIT;

-- Commit and chain (start new transaction)
COMMIT AND CHAIN;

-- Commit with release (disconnect)
COMMIT RELEASE;

-- Rollback
ROLLBACK;

-- Rollback to savepoint
ROLLBACK TO SAVEPOINT sp1;

-- Savepoint
SAVEPOINT before_update;

-- Release savepoint
RELEASE SAVEPOINT before_update;

-- Two-phase commit prepare
PREPARE TRANSACTION 'tx-gid-12345';
```

## Related Specifications

- [stmt_grant_revoke.md](./stmt_grant_revoke.md) - Privilege management
- [stmt_rls.md](./stmt_rls.md) - Row-level security

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
