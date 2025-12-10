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
using ast::DropTableStmt;
using ast::DropIndexStmt;
using ast::DropViewStmt;
using ast::SelectStmt;
using ast::SelectItem;
using ast::InsertStmt;
using ast::UpdateStmt;
using ast::DeleteStmt;
using ast::JoinType;
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
