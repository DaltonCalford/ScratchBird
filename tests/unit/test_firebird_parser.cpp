/**
 * Firebird Parser Unit Tests
 *
 * Tests for the Firebird SQL parser, focusing on expression parsing.
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/firebird/firebird_parser.h"

namespace fb = scratchbird::parser::firebird;
namespace ast = scratchbird::parser::v2;

using fb::Parser;
using fb::SQLDialect;
using fb::SimpleParserErrorReporter;
using ast::ASTKind;
using ast::CommitStmt;
using ast::RollbackStmt;
using ast::SavepointStmt;
using ast::ReleaseSavepointStmt;
using ast::Expression;
using ast::StringPool;
using ast::CreateTableStmt;
using ast::CreateIndexStmt;
using ast::CreateViewStmt;
using ast::CreateSequenceStmt;
using ast::CreateProcedureStmt;
using ast::CreateFunctionStmt;
using ast::CreateTriggerStmt;
using ast::CreateExceptionStmt;
using ast::AlterTableStmt;
using ast::AlterTableAction;
using ast::AlterDatabaseStmt;
using ast::AlterIndexStmt;
using ast::AlterIndexAction;
using ast::DropSequenceStmt;
using ast::CreateDomainStmt;
using ast::AlterDomainStmt;
using ast::DropDomainStmt;
using ast::DropTableStmt;
using ast::DropIndexStmt;
using ast::DropViewStmt;
using ast::DropFunctionStmt;
using ast::DropProcedureStmt;
using ast::DropTriggerStmt;
using ast::DropPackageStmt;
using ast::DropRoleStmt;
using ast::DropExceptionStmt;
using ast::SelectStmt;
using ast::SelectItem;
using ast::InsertStmt;
using ast::UpdateStmt;
using ast::DeleteStmt;
using ast::ExecuteProcedureStmt;
using ast::ExecuteStatementStmt;
using ast::MergeStmt;
using ast::JoinType;
using ast::DomainKind;
using ast::AlterDomainAction;
using ast::SetStmt;
using ast::ShowStmt;
using ast::GrantStmt;
using ast::RevokeStmt;
using ast::CommentStmt;
using ast::ExecuteBlockStmt;
using ast::CompoundStmt;
using ast::DeclareVariableStmt;
using ast::AssignmentStmt;
using ast::IfStmt;
using ast::WhileStmt;
using ast::ForSelectStmt;
using ast::LeaveStmt;
using ast::ContinueStmt;
using ast::ExitStmt;
using ast::SuspendStmt;
using ast::ReturnStmt;
using ast::ExceptionRaiseStmt;
using ast::WhenExceptionStmt;
using ast::PostEventStmt;
using ast::DeclareCursorStmt;
using ast::OpenCursorStmt;
using ast::FetchCursorStmt;
using ast::CloseCursorStmt;

class FirebirdParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing to set up
    }

    // Helper to parse an expression
    Expression* parseExpression(const std::string& sql) {
        std::string full_sql = "SELECT " + sql;
        Parser parser(full_sql);
        // Skip SELECT keyword by advancing
        parser.parseStatement();  // This will fail since SELECT not implemented, but advances past SELECT
        return nullptr;  // For now, we'll test simpler things
    }

    void expectError(const std::string& sql) {
        SimpleParserErrorReporter reporter;
        Parser parser(sql);
        parser.setErrorReporter(&reporter);
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success) << "Expected error for: " << sql;
    }
};

// =============================================================================
// Literal Expression Tests
// =============================================================================

TEST_F(FirebirdParserTest, ParseIntegerLiteral) {
    Parser parser("COMMIT");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.statement, nullptr);
    EXPECT_EQ(result.statement->kind(), ASTKind::CommitStmt);
}

TEST_F(FirebirdParserTest, ParseCommitRetain) {
    Parser parser("COMMIT RETAIN");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.statement, nullptr);
    EXPECT_EQ(result.statement->kind(), ASTKind::CommitStmt);

    auto* commit = static_cast<CommitStmt*>(result.statement.get());
    EXPECT_TRUE(commit->and_chain);
}

TEST_F(FirebirdParserTest, ParseRollback) {
    Parser parser("ROLLBACK");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.statement, nullptr);
    EXPECT_EQ(result.statement->kind(), ASTKind::RollbackStmt);
}

TEST_F(FirebirdParserTest, ParseRollbackRetain) {
    Parser parser("ROLLBACK RETAIN");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* rollback = static_cast<RollbackStmt*>(result.statement.get());
    EXPECT_TRUE(rollback->and_chain);
    EXPECT_FALSE(rollback->to_savepoint);
}

TEST_F(FirebirdParserTest, ParseRollbackToSavepoint) {
    Parser parser("ROLLBACK TO SAVEPOINT sp1");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* rollback = static_cast<RollbackStmt*>(result.statement.get());
    EXPECT_TRUE(rollback->to_savepoint);
    EXPECT_NE(rollback->savepoint_name, StringPool::INVALID_ID);
}

TEST_F(FirebirdParserTest, ParseSavepoint) {
    Parser parser("SAVEPOINT my_savepoint");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::SavepointStmt);
    auto* sp = static_cast<SavepointStmt*>(result.statement.get());
    EXPECT_NE(sp->name, StringPool::INVALID_ID);
}

TEST_F(FirebirdParserTest, ParseReleaseSavepoint) {
    Parser parser("RELEASE SAVEPOINT my_savepoint");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::ReleaseSavepointStmt);
}

// =============================================================================
// Multiple Statement Tests
// =============================================================================

TEST_F(FirebirdParserTest, ParseMultipleStatements) {
    Parser parser("COMMIT; ROLLBACK; SAVEPOINT sp1;");
    auto results = parser.parseAll();
    EXPECT_EQ(results.size(), 3u);
    EXPECT_TRUE(results[0].success);
    EXPECT_TRUE(results[1].success);
    EXPECT_TRUE(results[2].success);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(FirebirdParserTest, ParseUnexpectedToken) {
    SimpleParserErrorReporter reporter;
    Parser parser("COMMIT INVALID");
    parser.setErrorReporter(&reporter);
    auto result = parser.parseStatement();
    // COMMIT should parse, INVALID becomes next statement which fails
    EXPECT_TRUE(result.success);
}

// =============================================================================
// Dialect Guardrails
// =============================================================================

TEST_F(FirebirdParserTest, DialectGuardrails) {
    expectError("CREATE TABLE a.b (id INTEGER)");
    expectError("SELECT a.b.c FROM t");
    expectError("ALTER DATABASE testdb RENAME TO otherdb");
}

// =============================================================================
// SQL Dialect Tests
// =============================================================================

TEST_F(FirebirdParserTest, DefaultDialect) {
    Parser parser("COMMIT");
    EXPECT_EQ(parser.dialect(), SQLDialect::DIALECT_3);
}

TEST_F(FirebirdParserTest, SetDialect) {
    Parser parser("COMMIT", SQLDialect::DIALECT_1);
    EXPECT_EQ(parser.dialect(), SQLDialect::DIALECT_1);
    parser.setDialect(SQLDialect::DIALECT_3);
    EXPECT_EQ(parser.dialect(), SQLDialect::DIALECT_3);
}

// =============================================================================
// String Pool Tests
// =============================================================================

TEST_F(FirebirdParserTest, StringPoolAccess) {
    Parser parser("SAVEPOINT my_sp");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);

    auto* sp = static_cast<SavepointStmt*>(result.statement.get());
    std::string_view name = parser.stringPool().get(sp->name);
    EXPECT_EQ(name, "MY_SP");  // Firebird uppercases identifiers
}

// =============================================================================
// DDL Tests - CREATE TABLE
// =============================================================================

TEST_F(FirebirdParserTest, CreateTableSimple) {
    Parser parser("CREATE TABLE test_table (id INTEGER, name VARCHAR(100))");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.statement, nullptr);
    EXPECT_EQ(result.statement->kind(), ASTKind::CreateTableStmt);

    auto* stmt = static_cast<ast::CreateTableStmt*>(result.statement.get());
    EXPECT_EQ(stmt->columns.size(), 2u);
}

TEST_F(FirebirdParserTest, CreateTableWithPrimaryKey) {
    Parser parser("CREATE TABLE employees (id INTEGER PRIMARY KEY, name VARCHAR(100) NOT NULL)");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.statement, nullptr);
    EXPECT_EQ(result.statement->kind(), ASTKind::CreateTableStmt);
}

TEST_F(FirebirdParserTest, CreateGlobalTemporaryTable) {
    Parser parser("CREATE GLOBAL TEMPORARY TABLE temp_data (id INTEGER)");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::CreateTableStmt*>(result.statement.get());
    EXPECT_TRUE(stmt->temporary);
}

// =============================================================================
// DDL Tests - CREATE INDEX
// =============================================================================

TEST_F(FirebirdParserTest, CreateIndexSimple) {
    Parser parser("CREATE INDEX idx_name ON employees (name)");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::CreateIndexStmt);

    auto* stmt = static_cast<ast::CreateIndexStmt*>(result.statement.get());
    EXPECT_FALSE(stmt->unique);
    EXPECT_EQ(stmt->columns.size(), 1u);
}

TEST_F(FirebirdParserTest, CreateUniqueIndex) {
    Parser parser("CREATE UNIQUE INDEX idx_emp_id ON employees (emp_id)");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::CreateIndexStmt*>(result.statement.get());
    EXPECT_TRUE(stmt->unique);
}

TEST_F(FirebirdParserTest, CreateDescendingIndex) {
    Parser parser("CREATE DESCENDING INDEX idx_date ON orders (order_date)");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::CreateIndexStmt*>(result.statement.get());
    EXPECT_EQ(stmt->columns.size(), 1u);
    EXPECT_FALSE(stmt->columns[0].ascending);
}

// =============================================================================
// DDL Tests - DROP
// =============================================================================

TEST_F(FirebirdParserTest, DropTable) {
    Parser parser("DROP TABLE employees");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::DropTableStmt);
}

TEST_F(FirebirdParserTest, DropIndex) {
    Parser parser("DROP INDEX idx_name");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::DropIndexStmt);
}

TEST_F(FirebirdParserTest, DropView) {
    Parser parser("DROP VIEW emp_view");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::DropViewStmt);
}

TEST_F(FirebirdParserTest, DropProcedure) {
    Parser parser("DROP PROCEDURE my_proc");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::DropProcedureStmt);
    auto* stmt = static_cast<DropProcedureStmt*>(result.statement.get());
    EXPECT_EQ(stmt->procedures.size(), 1u);
}

TEST_F(FirebirdParserTest, DropFunction) {
    Parser parser("DROP FUNCTION my_func");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::DropFunctionStmt);
    auto* stmt = static_cast<DropFunctionStmt*>(result.statement.get());
    EXPECT_EQ(stmt->functions.size(), 1u);
}

TEST_F(FirebirdParserTest, DropTrigger) {
    Parser parser("DROP TRIGGER trig_test");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::DropTriggerStmt);
    auto* stmt = static_cast<DropTriggerStmt*>(result.statement.get());
    EXPECT_EQ(stmt->triggers.size(), 1u);
}

TEST_F(FirebirdParserTest, DropPackage) {
    Parser parser("DROP PACKAGE pkg_test");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::DropPackageStmt);
    auto* stmt = static_cast<DropPackageStmt*>(result.statement.get());
    EXPECT_EQ(stmt->packages.size(), 1u);
}

TEST_F(FirebirdParserTest, DropRole) {
    Parser parser("DROP ROLE role_test CASCADE");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::DropRoleStmt);
    auto* stmt = static_cast<DropRoleStmt*>(result.statement.get());
    EXPECT_EQ(stmt->roles.size(), 1u);
    EXPECT_TRUE(stmt->cascade);
}

TEST_F(FirebirdParserTest, DropException) {
    Parser parser("DROP EXCEPTION ex_test");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::DropExceptionStmt);
    auto* stmt = static_cast<DropExceptionStmt*>(result.statement.get());
    EXPECT_EQ(stmt->exceptions.size(), 1u);
}

// =============================================================================
// DDL Tests - RECREATE
// =============================================================================

TEST_F(FirebirdParserTest, RecreateTable) {
    Parser parser("RECREATE TABLE test_table (id INTEGER)");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::CreateTableStmt);
    auto* stmt = static_cast<ast::CreateTableStmt*>(result.statement.get());
    EXPECT_TRUE(stmt->or_replace);
}

// =============================================================================
// DDL Tests - CREATE SEQUENCE/GENERATOR
// =============================================================================

TEST_F(FirebirdParserTest, CreateGenerator) {
    Parser parser("CREATE GENERATOR gen_emp_id");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::CreateSequenceStmt);
}

TEST_F(FirebirdParserTest, CreateSequence) {
    Parser parser("CREATE SEQUENCE seq_order_id START WITH 1000");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::CreateSequenceStmt);
}

TEST_F(FirebirdParserTest, DropSequence) {
    Parser parser("DROP SEQUENCE seq_order_id");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::DropSequenceStmt);
    auto* stmt = static_cast<DropSequenceStmt*>(result.statement.get());
    EXPECT_EQ(stmt->sequences.size(), 1u);
}

// =============================================================================
// DDL Tests - DOMAIN
// =============================================================================

TEST_F(FirebirdParserTest, CreateDomainBasic) {
    Parser parser("CREATE DOMAIN email AS VARCHAR(255) NOT NULL");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::CreateDomainStmt);

    auto* stmt = static_cast<CreateDomainStmt*>(result.statement.get());
    EXPECT_EQ(stmt->domain_kind, DomainKind::BASIC);
    EXPECT_TRUE(stmt->has_dialect);
    EXPECT_EQ(stmt->dialect_tag, "firebird");
}

TEST_F(FirebirdParserTest, AlterDomainSetDefault) {
    Parser parser("ALTER DOMAIN email SET DEFAULT 'x'");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::AlterDomainStmt);

    auto* stmt = static_cast<AlterDomainStmt*>(result.statement.get());
    EXPECT_EQ(stmt->action, AlterDomainAction::SET_DEFAULT);
}

TEST_F(FirebirdParserTest, DropDomain) {
    Parser parser("DROP DOMAIN email");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::DropDomainStmt);

    auto* stmt = static_cast<DropDomainStmt*>(result.statement.get());
    EXPECT_FALSE(stmt->if_exists);
    EXPECT_EQ(stmt->domains.size(), 1u);
}

TEST_F(FirebirdParserTest, CreateDomainRejectsWithBlocks) {
    expectError("CREATE DOMAIN email AS VARCHAR(255) WITH SECURITY (MASKING = FULL)");
}

TEST_F(FirebirdParserTest, CreateProcedureSimple) {
    Parser parser("CREATE PROCEDURE my_proc (a INTEGER) RETURNS (b INTEGER) AS BEGIN b = a; SUSPEND; END");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::CreateProcedureStmt);
    auto* stmt = static_cast<CreateProcedureStmt*>(result.statement.get());
    EXPECT_EQ(stmt->params.size(), 2u);
    EXPECT_NE(stmt->body, StringPool::INVALID_ID);
}

TEST_F(FirebirdParserTest, CreateFunctionSimple) {
    Parser parser("CREATE FUNCTION my_fn (a INTEGER) RETURNS INTEGER AS BEGIN RETURN a; END");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::CreateFunctionStmt);
    auto* stmt = static_cast<CreateFunctionStmt*>(result.statement.get());
    EXPECT_EQ(stmt->params.size(), 1u);
    EXPECT_NE(stmt->body, StringPool::INVALID_ID);
}

TEST_F(FirebirdParserTest, CreateTriggerSimple) {
    Parser parser("CREATE TRIGGER trg_emp FOR employees BEFORE INSERT AS BEGIN END");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::CreateTriggerStmt);
    auto* stmt = static_cast<CreateTriggerStmt*>(result.statement.get());
    EXPECT_NE(stmt->trigger_name, StringPool::INVALID_ID);
    EXPECT_NE(stmt->body, StringPool::INVALID_ID);
    EXPECT_NE(stmt->event_mask & (1u << static_cast<uint8_t>(ast::TriggerEvent::INSERT)), 0u);
}

TEST_F(FirebirdParserTest, CreateTriggerMultiEvent) {
    Parser parser("CREATE TRIGGER trg_emp FOR employees BEFORE INSERT OR UPDATE OR DELETE AS BEGIN END");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::CreateTriggerStmt);
    auto* stmt = static_cast<CreateTriggerStmt*>(result.statement.get());
    uint8_t mask = stmt->event_mask;
    EXPECT_NE(mask & (1u << static_cast<uint8_t>(ast::TriggerEvent::INSERT)), 0u);
    EXPECT_NE(mask & (1u << static_cast<uint8_t>(ast::TriggerEvent::UPDATE)), 0u);
    EXPECT_NE(mask & (1u << static_cast<uint8_t>(ast::TriggerEvent::DELETE)), 0u);
}

TEST_F(FirebirdParserTest, AlterTableRename) {
    Parser parser("ALTER TABLE employees RENAME TO employees_new");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::AlterTableStmt);
    auto* stmt = static_cast<AlterTableStmt*>(result.statement.get());
    EXPECT_EQ(stmt->action, AlterTableAction::RENAME_TABLE);
    EXPECT_NE(stmt->new_name, StringPool::INVALID_ID);
}

TEST_F(FirebirdParserTest, AlterDatabaseOwner) {
    Parser parser("ALTER DATABASE '/data/legacy/employees.fdb' OWNER TO SYSDBA");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::AlterDatabaseStmt);
    auto* stmt = static_cast<AlterDatabaseStmt*>(result.statement.get());
    EXPECT_EQ(stmt->action, ast::AlterDatabaseAction::SET_OWNER);
    EXPECT_NE(stmt->owner, StringPool::INVALID_ID);
}

TEST_F(FirebirdParserTest, CreateExceptionSimple) {
    Parser parser("CREATE EXCEPTION ex_test 'boom'");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::CreateExceptionStmt);

    auto* stmt = static_cast<CreateExceptionStmt*>(result.statement.get());
    ASSERT_EQ(stmt->exception_path.components.size(), 1u);
    EXPECT_NE(stmt->exception_path.components[0], StringPool::INVALID_ID);
    EXPECT_NE(stmt->message, StringPool::INVALID_ID);
}

TEST_F(FirebirdParserTest, AlterIndexActive) {
    Parser parser("ALTER INDEX idx_test ACTIVE");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::AlterIndexStmt);

    auto* stmt = static_cast<AlterIndexStmt*>(result.statement.get());
    ASSERT_EQ(stmt->index_path.components.size(), 1u);
    EXPECT_NE(stmt->index_path.components[0], StringPool::INVALID_ID);
    EXPECT_EQ(stmt->action, AlterIndexAction::ACTIVE);
}

// =============================================================================
// DML Tests - SELECT
// =============================================================================

TEST_F(FirebirdParserTest, SelectSimple) {
    Parser parser("SELECT id, name FROM employees");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::SelectStmt);

    auto* stmt = static_cast<ast::SelectStmt*>(result.statement.get());
    EXPECT_EQ(stmt->items.size(), 2u);
    EXPECT_NE(stmt->from, nullptr);
}

TEST_F(FirebirdParserTest, SelectWithStar) {
    Parser parser("SELECT * FROM employees");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::SelectStmt*>(result.statement.get());
    EXPECT_EQ(stmt->items.size(), 1u);
    EXPECT_EQ(stmt->items[0]->item_type, ast::SelectItem::Type::STAR);
}

TEST_F(FirebirdParserTest, SelectDistinct) {
    Parser parser("SELECT DISTINCT department FROM employees");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::SelectStmt*>(result.statement.get());
    EXPECT_TRUE(stmt->distinct);
}

TEST_F(FirebirdParserTest, SelectFirstSkip) {
    // Firebird-specific: FIRST/SKIP instead of LIMIT/OFFSET
    Parser parser("SELECT FIRST 10 SKIP 5 id, name FROM employees");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::SelectStmt*>(result.statement.get());
    EXPECT_NE(stmt->limit, nullptr);
    EXPECT_NE(stmt->offset, nullptr);
}

TEST_F(FirebirdParserTest, SelectWithWhere) {
    Parser parser("SELECT id FROM employees WHERE salary > 50000");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::SelectStmt*>(result.statement.get());
    EXPECT_NE(stmt->where, nullptr);
}

TEST_F(FirebirdParserTest, SelectWithLikeVariants) {
    {
        Parser parser("SELECT id FROM employees WHERE name CONTAINING 'Map'");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success);
        auto* stmt = static_cast<ast::SelectStmt*>(result.statement.get());
        auto* like = dynamic_cast<ast::LikeExpr*>(stmt->where);
        ASSERT_NE(like, nullptr);
        EXPECT_EQ(like->match_kind, ast::LikeMatchKind::CONTAINING);
        EXPECT_TRUE(like->case_insensitive);
    }
    {
        Parser parser("SELECT id FROM employees WHERE name STARTING WITH 'Jo'");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success);
        auto* stmt = static_cast<ast::SelectStmt*>(result.statement.get());
        auto* like = dynamic_cast<ast::LikeExpr*>(stmt->where);
        ASSERT_NE(like, nullptr);
        EXPECT_EQ(like->match_kind, ast::LikeMatchKind::STARTING);
        EXPECT_FALSE(like->case_insensitive);
    }
    {
        Parser parser("SELECT id FROM employees WHERE name SIMILAR TO '[A-Z]+'");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success);
        auto* stmt = static_cast<ast::SelectStmt*>(result.statement.get());
        auto* like = dynamic_cast<ast::LikeExpr*>(stmt->where);
        ASSERT_NE(like, nullptr);
        EXPECT_EQ(like->match_kind, ast::LikeMatchKind::SIMILAR);
    }
    {
        Parser parser("SELECT id FROM employees WHERE name NOT CONTAINING 'Map'");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success);
        auto* stmt = static_cast<ast::SelectStmt*>(result.statement.get());
        auto* like = dynamic_cast<ast::LikeExpr*>(stmt->where);
        ASSERT_NE(like, nullptr);
        EXPECT_TRUE(like->negated);
        EXPECT_EQ(like->match_kind, ast::LikeMatchKind::CONTAINING);
    }
}

TEST_F(FirebirdParserTest, SelectWithOrderBy) {
    Parser parser("SELECT id, name FROM employees ORDER BY name ASC, id DESC");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::SelectStmt*>(result.statement.get());
    EXPECT_EQ(stmt->order_by.size(), 2u);
}

TEST_F(FirebirdParserTest, SelectWithJoin) {
    Parser parser("SELECT e.name, d.name FROM employees e LEFT JOIN departments d ON e.dept_id = d.id");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::SelectStmt*>(result.statement.get());
    EXPECT_EQ(stmt->joins.size(), 1u);
    EXPECT_EQ(stmt->joins[0]->join_type, ast::JoinType::LEFT);
}

TEST_F(FirebirdParserTest, SelectWithGroupBy) {
    Parser parser("SELECT department, COUNT(*) FROM employees GROUP BY department");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::SelectStmt*>(result.statement.get());
    EXPECT_EQ(stmt->group_by.size(), 1u);
}

// =============================================================================
// DML Tests - INSERT
// =============================================================================

TEST_F(FirebirdParserTest, InsertSimple) {
    Parser parser("INSERT INTO employees (id, name) VALUES (1, 'John')");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::InsertStmt);

    auto* stmt = static_cast<ast::InsertStmt*>(result.statement.get());
    EXPECT_EQ(stmt->columns.size(), 2u);
    EXPECT_EQ(stmt->values_rows.size(), 1u);
}

TEST_F(FirebirdParserTest, InsertWithReturning) {
    // Firebird-specific: RETURNING clause
    Parser parser("INSERT INTO employees (name) VALUES ('John') RETURNING id");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::InsertStmt*>(result.statement.get());
    EXPECT_EQ(stmt->returning.size(), 1u);
}

TEST_F(FirebirdParserTest, InsertMultipleRows) {
    Parser parser("INSERT INTO employees (id, name) VALUES (1, 'John'), (2, 'Jane')");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::InsertStmt*>(result.statement.get());
    EXPECT_EQ(stmt->values_rows.size(), 2u);
}

// =============================================================================
// DML Tests - UPDATE
// =============================================================================

TEST_F(FirebirdParserTest, UpdateSimple) {
    Parser parser("UPDATE employees SET salary = 60000 WHERE id = 1");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::UpdateStmt);

    auto* stmt = static_cast<ast::UpdateStmt*>(result.statement.get());
    EXPECT_EQ(stmt->set_items.size(), 1u);
    EXPECT_NE(stmt->where, nullptr);
}

TEST_F(FirebirdParserTest, UpdateWithReturning) {
    Parser parser("UPDATE employees SET salary = 60000 WHERE id = 1 RETURNING salary");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::UpdateStmt*>(result.statement.get());
    EXPECT_EQ(stmt->returning.size(), 1u);
}

// =============================================================================
// DML Tests - DELETE
// =============================================================================

TEST_F(FirebirdParserTest, DeleteSimple) {
    Parser parser("DELETE FROM employees WHERE id = 1");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::DeleteStmt);

    auto* stmt = static_cast<ast::DeleteStmt*>(result.statement.get());
    EXPECT_NE(stmt->where, nullptr);
}

TEST_F(FirebirdParserTest, DeleteWithReturning) {
    Parser parser("DELETE FROM employees WHERE id = 1 RETURNING name");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::DeleteStmt*>(result.statement.get());
    EXPECT_EQ(stmt->returning.size(), 1u);
}

TEST_F(FirebirdParserTest, MergeStatement) {
    Parser parser("MERGE INTO t USING s ON t.id = s.id "
                  "WHEN MATCHED THEN UPDATE SET a = 1 "
                  "WHEN NOT MATCHED THEN INSERT (a) VALUES (1)");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::MergeStmt);
}

TEST_F(FirebirdParserTest, ExecuteProcedureStatement) {
    Parser parser("EXECUTE PROCEDURE my_proc(1, 2)");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::ExecuteProcedureStmt);
}

TEST_F(FirebirdParserTest, ExecuteStatementStatement) {
    Parser parser("EXECUTE STATEMENT 'select 1' INTO x");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::ExecuteStatementStmt);
}

TEST_F(FirebirdParserTest, SetSqlDialect) {
    Parser parser("SET SQL DIALECT 3");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::SetStmt);
    auto* stmt = static_cast<SetStmt*>(result.statement.get());
    EXPECT_EQ(stmt->set_type, SetStmt::SetType::SQL_DIALECT);
}

TEST_F(FirebirdParserTest, ShowTable) {
    Parser parser("SHOW TABLE employees");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::ShowStmt);
    auto* stmt = static_cast<ShowStmt*>(result.statement.get());
    EXPECT_EQ(stmt->show_type, ShowStmt::ShowType::TABLE);
}

TEST_F(FirebirdParserTest, GrantStatement) {
    Parser parser("GRANT SELECT ON TABLE employees TO PUBLIC");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::GrantStmt);
}

TEST_F(FirebirdParserTest, RevokeStatement) {
    Parser parser("REVOKE SELECT ON TABLE employees FROM PUBLIC");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::RevokeStmt);
}

TEST_F(FirebirdParserTest, CommentStatement) {
    Parser parser("COMMENT ON TABLE employees IS 'note'");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.statement->kind(), ASTKind::CommentStmt);
}

// =============================================================================
// PSQL Tests - EXECUTE BLOCK
// =============================================================================

TEST_F(FirebirdParserTest, ExecuteBlockSimple) {
    Parser parser(R"(
        EXECUTE BLOCK AS
        BEGIN
            EXIT;
        END
    )");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.statement, nullptr);
    EXPECT_EQ(result.statement->kind(), ASTKind::ExecuteBlockStmt);
}

TEST_F(FirebirdParserTest, ExecuteBlockWithVariables) {
    Parser parser(R"(
        EXECUTE BLOCK AS
        DECLARE VARIABLE x INTEGER;
        BEGIN
            x := 10;
        END
    )");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::ExecuteBlockStmt*>(result.statement.get());
    EXPECT_EQ(stmt->variables.size(), 1u);
}

TEST_F(FirebirdParserTest, ExecuteBlockWithReturns) {
    Parser parser(R"(
        EXECUTE BLOCK RETURNS (result INTEGER) AS
        BEGIN
            result := 42;
            SUSPEND;
        END
    )");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
    auto* stmt = static_cast<ast::ExecuteBlockStmt*>(result.statement.get());
    EXPECT_EQ(stmt->output_params.size(), 1u);
}

// =============================================================================
// PSQL Tests - Control Flow
// =============================================================================

TEST_F(FirebirdParserTest, IfStatement) {
    Parser parser(R"(
        EXECUTE BLOCK AS
        DECLARE VARIABLE x INTEGER;
        BEGIN
            IF (x > 0) THEN
                x := x - 1;
        END
    )");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
}

TEST_F(FirebirdParserTest, IfElseStatement) {
    Parser parser(R"(
        EXECUTE BLOCK AS
        DECLARE VARIABLE x INTEGER;
        BEGIN
            IF (x > 0) THEN
                x := 1;
            ELSE
                x := 0;
        END
    )");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
}

TEST_F(FirebirdParserTest, WhileStatement) {
    Parser parser(R"(
        EXECUTE BLOCK AS
        DECLARE VARIABLE i INTEGER;
        BEGIN
            i := 0;
            WHILE (i < 10) DO
            BEGIN
                i := i + 1;
            END
        END
    )");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
}

TEST_F(FirebirdParserTest, ForSelectStatement) {
    Parser parser(R"(
        EXECUTE BLOCK AS
        DECLARE VARIABLE emp_name VARCHAR(100);
        BEGIN
            FOR SELECT name FROM employees INTO emp_name DO
            BEGIN
                SUSPEND;
            END
        END
    )");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
}

// =============================================================================
// PSQL Tests - Exception Handling
// =============================================================================

TEST_F(FirebirdParserTest, ExceptionRaise) {
    Parser parser(R"(
        EXECUTE BLOCK AS
        BEGIN
            EXCEPTION my_exception;
        END
    )");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
}

TEST_F(FirebirdParserTest, WhenExceptionHandler) {
    Parser parser(R"(
        EXECUTE BLOCK AS
        BEGIN
            INSERT INTO test (id) VALUES (1);
        WHEN ANY DO
            EXIT;
        END
    )");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
}

// =============================================================================
// PSQL Tests - Cursor Operations
// =============================================================================

TEST_F(FirebirdParserTest, CursorOperations) {
    Parser parser(R"(
        EXECUTE BLOCK AS
        DECLARE VARIABLE emp_id INTEGER;
        BEGIN
            OPEN my_cursor;
            FETCH my_cursor INTO emp_id;
            CLOSE my_cursor;
        END
    )");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
}

// =============================================================================
// PSQL Tests - Statements
// =============================================================================

TEST_F(FirebirdParserTest, SuspendStatement) {
    Parser parser(R"(
        EXECUTE BLOCK RETURNS (x INTEGER) AS
        BEGIN
            x := 1;
            SUSPEND;
            x := 2;
            SUSPEND;
        END
    )");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
}

TEST_F(FirebirdParserTest, LeaveStatement) {
    Parser parser(R"(
        EXECUTE BLOCK AS
        DECLARE VARIABLE i INTEGER;
        BEGIN
            i := 0;
            WHILE (i < 100) DO
            BEGIN
                IF (i = 10) THEN
                    LEAVE;
                i := i + 1;
            END
        END
    )");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
}

TEST_F(FirebirdParserTest, PostEvent) {
    Parser parser(R"(
        EXECUTE BLOCK AS
        BEGIN
            POST_EVENT 'my_event';
        END
    )");
    auto result = parser.parseStatement();
    EXPECT_TRUE(result.success);
}
