#include <gtest/gtest.h>

#include <cstring>

#include "scratchbird/odbc/odbc_handles.h"

using scratchbird::odbc::OdbcEnvironment;
using scratchbird::odbc::SQL_C_CHAR;
using scratchbird::odbc::SQL_UNKNOWN_TYPE;
using scratchbird::odbc::SQL_VARCHAR;
using scratchbird::odbc::SQL_SUCCESS;

TEST(OdbcTypeInfoTest, ReturnsVarcharInfo) {
    OdbcEnvironment env;
    auto* conn = env.createConnection();
    ASSERT_NE(conn, nullptr);
    auto* stmt = conn->createStatement();
    ASSERT_NE(stmt, nullptr);

    ASSERT_EQ(conn->getTypeInfo(SQL_VARCHAR, stmt), SQL_SUCCESS);

    scratchbird::odbc::SQLSMALLINT col_count = 0;
    EXPECT_EQ(stmt->numResultCols(&col_count), SQL_SUCCESS);
    EXPECT_EQ(col_count, 19);

    ASSERT_EQ(stmt->fetch(), SQL_SUCCESS);

    char type_name[64] = {};
    scratchbird::odbc::SQLLEN out_len = 0;
    EXPECT_EQ(stmt->getData(1, SQL_C_CHAR, type_name, sizeof(type_name), &out_len), SQL_SUCCESS);
    EXPECT_STREQ(type_name, "VARCHAR");
}

TEST(OdbcTypeInfoTest, ReturnsAllTypes) {
    OdbcEnvironment env;
    auto* conn = env.createConnection();
    ASSERT_NE(conn, nullptr);
    auto* stmt = conn->createStatement();
    ASSERT_NE(stmt, nullptr);

    ASSERT_EQ(conn->getTypeInfo(SQL_UNKNOWN_TYPE, stmt), SQL_SUCCESS);
    EXPECT_EQ(stmt->fetch(), SQL_SUCCESS);
}
