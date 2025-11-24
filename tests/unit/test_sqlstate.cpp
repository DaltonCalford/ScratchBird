#include <gtest/gtest.h>
#include "scratchbird/core/sqlstate.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <cstring>

using namespace scratchbird::core;

class SQLStateTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test SQLSTATE mapping for success
TEST_F(SQLStateTest, SuccessMapping) {
    const char* sqlstate = statusToSQLState(Status::OK);
    EXPECT_STREQ(sqlstate, "00000");
}

// Test SQLSTATE mapping for constraint violations
TEST_F(SQLStateTest, ConstraintViolations) {
    EXPECT_STREQ(statusToSQLState(Status::NOT_NULL_VIOLATION), "23502");
    EXPECT_STREQ(statusToSQLState(Status::FOREIGN_KEY_VIOLATION), "23503");
    EXPECT_STREQ(statusToSQLState(Status::UNIQUE_VIOLATION), "23505");
    EXPECT_STREQ(statusToSQLState(Status::CHECK_VIOLATION), "23514");
}

// Test SQLSTATE mapping for data exceptions
TEST_F(SQLStateTest, DataExceptions) {
    EXPECT_STREQ(statusToSQLState(Status::DIVISION_BY_ZERO), "22012");
    EXPECT_STREQ(statusToSQLState(Status::NUMERIC_VALUE_OUT_OF_RANGE), "22003");
    EXPECT_STREQ(statusToSQLState(Status::STRING_DATA_RIGHT_TRUNCATION), "22001");
    EXPECT_STREQ(statusToSQLState(Status::DATETIME_FIELD_OVERFLOW), "22008");
}

// Test SQLSTATE mapping for syntax errors
TEST_F(SQLStateTest, SyntaxErrors) {
    EXPECT_STREQ(statusToSQLState(Status::SYNTAX_ERROR), "42601");
    EXPECT_STREQ(statusToSQLState(Status::UNDEFINED_TABLE), "42P01");
    EXPECT_STREQ(statusToSQLState(Status::UNDEFINED_COLUMN), "42703");
    EXPECT_STREQ(statusToSQLState(Status::DUPLICATE_TABLE), "42P07");
}

// Test SQLSTATE mapping for transaction errors
TEST_F(SQLStateTest, TransactionErrors) {
    EXPECT_STREQ(statusToSQLState(Status::DEADLOCK), "40P01");
    EXPECT_STREQ(statusToSQLState(Status::SERIALIZATION_FAILURE), "40001");
    EXPECT_STREQ(statusToSQLState(Status::INVALID_TRANSACTION_STATE), "25000");
}

// Test SQLSTATE mapping for resource errors
TEST_F(SQLStateTest, ResourceErrors) {
    EXPECT_STREQ(statusToSQLState(Status::OOM), "53200");
    EXPECT_STREQ(statusToSQLState(Status::DISK_FULL), "53100");
    EXPECT_STREQ(statusToSQLState(Status::TOO_MANY_CONNECTIONS), "53300");
}

// Test SQLSTATE mapping for PL/pgSQL errors
TEST_F(SQLStateTest, PLpgSQLErrors) {
    EXPECT_STREQ(statusToSQLState(Status::NO_DATA_FOUND), "P0002");
    EXPECT_STREQ(statusToSQLState(Status::TOO_MANY_ROWS), "P0003");
}

// Test SQLSTATE mapping for cursor errors
TEST_F(SQLStateTest, CursorErrors) {
    EXPECT_STREQ(statusToSQLState(Status::INVALID_CURSOR_STATE), "24000");
    EXPECT_STREQ(statusToSQLState(Status::INVALID_CURSOR_NAME), "34000");
}

// Test SQLSTATE mapping for internal errors
TEST_F(SQLStateTest, InternalErrors) {
    EXPECT_STREQ(statusToSQLState(Status::INTERNAL_ERROR), "XX000");
    EXPECT_STREQ(statusToSQLState(Status::DATA_CORRUPTED), "XX001");
    EXPECT_STREQ(statusToSQLState(Status::INDEX_CORRUPTED), "XX002");
}

// Test SQLSTATE class descriptions
TEST_F(SQLStateTest, ClassDescriptions) {
    EXPECT_EQ(getSQLStateClass("00000"), "Successful Completion");
    EXPECT_EQ(getSQLStateClass("22012"), "Data Exception");
    EXPECT_EQ(getSQLStateClass("23502"), "Integrity Constraint Violation");
    EXPECT_EQ(getSQLStateClass("40P01"), "Transaction Rollback");
    EXPECT_EQ(getSQLStateClass("42601"), "Syntax Error or Access Rule Violation");
    EXPECT_EQ(getSQLStateClass("53200"), "Insufficient Resources");
    EXPECT_EQ(getSQLStateClass("XX001"), "Internal Error");
}

// Test ErrorContext integration
TEST_F(SQLStateTest, ErrorContextIntegration) {
    ErrorContext ctx;
    ctx.set(Status::FOREIGN_KEY_VIOLATION, "Foreign key constraint violated", __FILE__, __LINE__, __func__);

    EXPECT_EQ(ctx.code, Status::FOREIGN_KEY_VIOLATION);
    EXPECT_STREQ(ctx.sqlstate, "23503");
    EXPECT_EQ(ctx.message, "Foreign key constraint violated");
}

// Test ErrorContext manual SQLSTATE override
TEST_F(SQLStateTest, ErrorContextManualOverride) {
    ErrorContext ctx;
    ctx.set(Status::INTERNAL_ERROR, "Custom error", __FILE__, __LINE__, __func__);

    // Initially should map to XX000
    EXPECT_STREQ(ctx.sqlstate, "XX000");

    // Override with custom SQLSTATE
    ctx.setSQLState("42P01");
    EXPECT_STREQ(ctx.sqlstate, "42P01");
}

// Test default mapping for unknown status
TEST_F(SQLStateTest, UnknownStatusMapping) {
    // Cast an arbitrary value to Status to simulate unknown status
    Status unknown = static_cast<Status>(99999);
    const char* sqlstate = statusToSQLState(unknown);
    EXPECT_STREQ(sqlstate, "XX000"); // Should default to internal error
}
