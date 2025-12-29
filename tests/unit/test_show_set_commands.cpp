/**
 * Tests for Server-Side SHOW and SET Commands
 *
 * This file tests the parser and bytecode generation for
 * Firebird ISQL-compatible SHOW and SET commands.
 *
 * SHOW Commands tested:
 * - SHOW TABLE, SHOW INDEX, SHOW TRIGGER, SHOW PROCEDURE
 * - SHOW FUNCTION, SHOW VIEW, SHOW DOMAIN, SHOW GENERATOR
 * - SHOW SCHEMA, SHOW ROLE, SHOW GRANTS, SHOW CHECKS
 * - SHOW COLLATIONS, SHOW COMMENTS, SHOW DEPENDENCIES
 * - SHOW PACKAGE, SHOW SYSTEM, SHOW SQL DIALECT
 * - SHOW VERSION, SHOW DATABASE
 *
 * SET Commands tested:
 * - SET SQL DIALECT n
 * - SET NAMES charset
 * - SET LOCAL_TIMEOUT n
 */

#include <gtest/gtest.h>

#include "scratchbird/parser/ast.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/opcodes.h"
#include <sstream>

using namespace scratchbird::parser;
using namespace scratchbird::sblr;

class ShowSetCommandsTest : public ::testing::Test
{
protected:
    // Helper to parse SQL and return the statement
    Statement* parseStatement(const std::string &sql)
    {
        lexer_ = std::make_unique<Lexer>(sql);
        arena_ = std::make_unique<ASTArena>();
        parser_ = std::make_unique<Parser>(*lexer_, *arena_);

        auto parse_result = parser_->parseStatement();

        if (!parse_result.success())
        {
            for (const auto &err : parse_result.errors())
            {
                last_error_ = err.message;
            }
            return nullptr;
        }

        return parse_result.statement();
    }

    // Helper to check if parsing succeeds
    bool parseSucceeds(const std::string &sql)
    {
        return parseStatement(sql) != nullptr;
    }

    // Helper to generate bytecode
    std::vector<uint8_t> generateBytecode(const std::string &sql)
    {
        auto stmt = parseStatement(sql);
        if (!stmt) return {};

        BytecodeGenerator generator(parser_->stringPool());
        auto result = generator.generate(stmt);

        if (!result.success())
        {
            for (const auto &err : result.errors())
            {
                last_error_ = err;
            }
            return {};
        }

        return result.bytecode();
    }

    // Helper to check if bytecode contains an extended opcode
    bool bytecodeContainsExtOpcode(const std::vector<uint8_t>& bc, ExtendedOpcode ext_op)
    {
        for (size_t i = 0; i + 2 < bc.size(); i++)
        {
            if (bc[i] == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE))
            {
                uint16_t op = sblr::readInt16(&bc[i + 1]);
                if (op == static_cast<uint16_t>(ext_op))
                {
                    return true;
                }
            }
        }
        return false;
    }

    std::string last_error_;

private:
    std::unique_ptr<Lexer> lexer_;
    std::unique_ptr<ASTArena> arena_;
    std::unique_ptr<Parser> parser_;
};

// =============================================================================
// SHOW TABLE Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowTableParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW TABLE users"));
    EXPECT_TRUE(parseSucceeds("SHOW TABLE my_table"));
    EXPECT_TRUE(parseSucceeds("SHOW TABLE \"CaseSensitiveTable\""));
}

TEST_F(ShowSetCommandsTest, ShowTableBytecode)
{
    auto bc = generateBytecode("SHOW TABLE users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_TABLE));
}

// =============================================================================
// SHOW INDEX Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowIndexParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW INDEX idx_users_name"));
    EXPECT_TRUE(parseSucceeds("SHOW INDEX pk_table"));
}

TEST_F(ShowSetCommandsTest, ShowIndexBytecode)
{
    auto bc = generateBytecode("SHOW INDEX idx_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_INDEX));
}

// =============================================================================
// SHOW TRIGGER Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowTriggerParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW TRIGGER trg_before_insert"));
    EXPECT_TRUE(parseSucceeds("SHOW TRIGGER my_trigger"));
}

TEST_F(ShowSetCommandsTest, ShowTriggerBytecode)
{
    auto bc = generateBytecode("SHOW TRIGGER trg_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_TRIGGER));
}

// =============================================================================
// SHOW PROCEDURE Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowProcedureParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW PROCEDURE sp_get_users"));
    EXPECT_TRUE(parseSucceeds("SHOW PROCEDURE my_proc"));
}

TEST_F(ShowSetCommandsTest, ShowProcedureBytecode)
{
    auto bc = generateBytecode("SHOW PROCEDURE sp_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_PROCEDURE));
}

// =============================================================================
// SHOW FUNCTION Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowFunctionParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW FUNCTION fn_calculate"));
    EXPECT_TRUE(parseSucceeds("SHOW FUNCTION my_func"));
}

TEST_F(ShowSetCommandsTest, ShowFunctionBytecode)
{
    auto bc = generateBytecode("SHOW FUNCTION fn_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_FUNCTION));
}

// =============================================================================
// SHOW VIEW Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowViewParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW VIEW v_active_users"));
    EXPECT_TRUE(parseSucceeds("SHOW VIEW my_view"));
}

TEST_F(ShowSetCommandsTest, ShowViewBytecode)
{
    auto bc = generateBytecode("SHOW VIEW v_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_VIEW));
}

// =============================================================================
// SHOW DOMAIN Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowDomainParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW DOMAIN d_email"));
    EXPECT_TRUE(parseSucceeds("SHOW DOMAIN my_domain"));
}

TEST_F(ShowSetCommandsTest, ShowDomainBytecode)
{
    auto bc = generateBytecode("SHOW DOMAIN d_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_DOMAIN));
}

// =============================================================================
// SHOW GENERATOR Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowGeneratorParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW GENERATOR gen_user_id"));
    EXPECT_TRUE(parseSucceeds("SHOW GENERATOR my_sequence"));
}

TEST_F(ShowSetCommandsTest, ShowGeneratorBytecode)
{
    auto bc = generateBytecode("SHOW GENERATOR gen_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_GENERATOR));
}

// =============================================================================
// SHOW SCHEMA Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowSchemaParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW SCHEMA"));
    EXPECT_TRUE(parseSucceeds("SHOW SCHEMA public"));
    EXPECT_TRUE(parseSucceeds("SHOW SCHEMA my_schema"));
}

TEST_F(ShowSetCommandsTest, ShowSchemaBytecode)
{
    auto bc = generateBytecode("SHOW SCHEMA");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_SCHEMA));
}

// =============================================================================
// SHOW ROLE Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowRoleParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW ROLE admin"));
    EXPECT_TRUE(parseSucceeds("SHOW ROLE my_role"));
}

TEST_F(ShowSetCommandsTest, ShowRoleBytecode)
{
    auto bc = generateBytecode("SHOW ROLE test_role");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_ROLE));
}

// =============================================================================
// SHOW GRANTS Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowGrantsParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW GRANTS"));
    EXPECT_TRUE(parseSucceeds("SHOW GRANTS FOR users"));
    EXPECT_TRUE(parseSucceeds("SHOW GRANTS FOR my_table"));
}

TEST_F(ShowSetCommandsTest, ShowGrantsBytecode)
{
    auto bc = generateBytecode("SHOW GRANTS");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_GRANTS));

    bc = generateBytecode("SHOW GRANTS FOR users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_GRANTS));
}

// =============================================================================
// SHOW CHECKS Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowChecksParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW CHECKS"));
    EXPECT_TRUE(parseSucceeds("SHOW CHECKS users"));
    EXPECT_TRUE(parseSucceeds("SHOW CHECKS my_table"));
}

TEST_F(ShowSetCommandsTest, ShowChecksBytecode)
{
    auto bc = generateBytecode("SHOW CHECKS users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_CHECKS));

    bc = generateBytecode("SHOW CHECKS");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_CHECKS));
}

// =============================================================================
// SHOW COLLATIONS Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowCollationsParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW COLLATIONS"));
    EXPECT_TRUE(parseSucceeds("SHOW COLLATIONS LIKE 'UTF%'"));
}

TEST_F(ShowSetCommandsTest, ShowCollationsBytecode)
{
    auto bc = generateBytecode("SHOW COLLATIONS");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_COLLATIONS));
}

// =============================================================================
// SHOW COMMENTS Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowCommentsParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW COMMENTS users"));
    EXPECT_TRUE(parseSucceeds("SHOW COMMENTS my_object"));
}

TEST_F(ShowSetCommandsTest, ShowCommentsBytecode)
{
    auto bc = generateBytecode("SHOW COMMENTS users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_COMMENTS));
}

// =============================================================================
// SHOW DEPENDENCIES Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowDependenciesParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW DEPENDENCIES users"));
    EXPECT_TRUE(parseSucceeds("SHOW DEPENDENCIES my_view"));
}

TEST_F(ShowSetCommandsTest, ShowDependenciesBytecode)
{
    auto bc = generateBytecode("SHOW DEPENDENCIES users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_DEPENDENCIES));
}

// =============================================================================
// SHOW PACKAGE Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowPackageParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW PACKAGE pkg_utils"));
    EXPECT_TRUE(parseSucceeds("SHOW PACKAGE my_package"));
}

TEST_F(ShowSetCommandsTest, ShowPackageBytecode)
{
    auto bc = generateBytecode("SHOW PACKAGE pkg_test");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_PACKAGE));
}

// =============================================================================
// SHOW SYSTEM Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowSystemParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW SYSTEM"));
}

TEST_F(ShowSetCommandsTest, ShowSystemBytecode)
{
    auto bc = generateBytecode("SHOW SYSTEM");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_SYSTEM));
}

// =============================================================================
// SHOW SQL DIALECT Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowSqlDialectParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW SQL DIALECT"));
}

TEST_F(ShowSetCommandsTest, ShowSqlDialectBytecode)
{
    auto bc = generateBytecode("SHOW SQL DIALECT");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_SQL_DIALECT));
}

// =============================================================================
// SHOW VERSION Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowVersionParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW VERSION"));
}

TEST_F(ShowSetCommandsTest, ShowVersionBytecode)
{
    auto bc = generateBytecode("SHOW VERSION");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_VERSION));
}

// =============================================================================
// SHOW DATABASE Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowDatabaseParsing)
{
    EXPECT_TRUE(parseSucceeds("SHOW DATABASE"));
}

TEST_F(ShowSetCommandsTest, ShowDatabaseBytecode)
{
    auto bc = generateBytecode("SHOW DATABASE");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_DATABASE));
}

// =============================================================================
// SET SQL DIALECT Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, SetSqlDialectParsing)
{
    EXPECT_TRUE(parseSucceeds("SET SQL DIALECT 1"));
    EXPECT_TRUE(parseSucceeds("SET SQL DIALECT 2"));
    EXPECT_TRUE(parseSucceeds("SET SQL DIALECT 3"));
}

TEST_F(ShowSetCommandsTest, SetSqlDialectASTValues)
{
    auto stmt = parseStatement("SET SQL DIALECT 3");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->kind(), ASTKind::SET_SQL_DIALECT);

    auto* set_stmt = static_cast<SetSqlDialectStmt*>(stmt);
    EXPECT_EQ(set_stmt->dialect(), 3);
}

TEST_F(ShowSetCommandsTest, SetSqlDialectBytecode)
{
    auto bc = generateBytecode("SET SQL DIALECT 3");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SET_SQL_DIALECT));
}

// =============================================================================
// SET NAMES Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, SetNamesParsing)
{
    EXPECT_TRUE(parseSucceeds("SET NAMES UTF8"));
    EXPECT_TRUE(parseSucceeds("SET NAMES ISO8859_1"));
    EXPECT_TRUE(parseSucceeds("SET NAMES WIN1252"));
}

TEST_F(ShowSetCommandsTest, SetNamesASTValues)
{
    auto stmt = parseStatement("SET NAMES UTF8");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->kind(), ASTKind::SET_NAMES);
}

TEST_F(ShowSetCommandsTest, SetNamesBytecode)
{
    auto bc = generateBytecode("SET NAMES UTF8");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SET_NAMES));
}

// =============================================================================
// SET LOCAL_TIMEOUT Command Tests
// =============================================================================

TEST_F(ShowSetCommandsTest, SetLocalTimeoutParsing)
{
    EXPECT_TRUE(parseSucceeds("SET LOCAL_TIMEOUT 30"));
    EXPECT_TRUE(parseSucceeds("SET LOCAL_TIMEOUT 0"));
    EXPECT_TRUE(parseSucceeds("SET LOCAL_TIMEOUT 3600"));
}

TEST_F(ShowSetCommandsTest, SetLocalTimeoutASTValues)
{
    auto stmt = parseStatement("SET LOCAL_TIMEOUT 60");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->kind(), ASTKind::SET_LOCAL_TIMEOUT);

    auto* timeout_stmt = static_cast<SetLocalTimeoutStmt*>(stmt);
    EXPECT_EQ(timeout_stmt->timeoutSeconds(), 60u);
}

TEST_F(ShowSetCommandsTest, SetLocalTimeoutBytecode)
{
    auto bc = generateBytecode("SET LOCAL_TIMEOUT 30");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SET_LOCAL_TIMEOUT));
}

// =============================================================================
// Original SHOW Commands (verify they still work)
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowTablesStillWorks)
{
    EXPECT_TRUE(parseSucceeds("SHOW TABLES"));
    auto bc = generateBytecode("SHOW TABLES");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_TABLES));
}

TEST_F(ShowSetCommandsTest, ShowDatabasesStillWorks)
{
    EXPECT_TRUE(parseSucceeds("SHOW DATABASES"));
    auto bc = generateBytecode("SHOW DATABASES");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_DATABASES));
}

TEST_F(ShowSetCommandsTest, ShowColumnsStillWorks)
{
    EXPECT_TRUE(parseSucceeds("SHOW COLUMNS FROM users"));
    auto bc = generateBytecode("SHOW COLUMNS FROM users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_COLUMNS));
}

TEST_F(ShowSetCommandsTest, ShowIndexesStillWorks)
{
    EXPECT_TRUE(parseSucceeds("SHOW INDEXES FROM users"));
    auto bc = generateBytecode("SHOW INDEXES FROM users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_INDEXES));
}

TEST_F(ShowSetCommandsTest, ShowCreateTableStillWorks)
{
    EXPECT_TRUE(parseSucceeds("SHOW CREATE TABLE users"));
    auto bc = generateBytecode("SHOW CREATE TABLE users");
    ASSERT_FALSE(bc.empty());
    EXPECT_TRUE(bytecodeContainsExtOpcode(bc, ExtendedOpcode::EXT_SHOW_CREATE_TABLE));
}

// =============================================================================
// Edge Cases and Error Handling
// =============================================================================

TEST_F(ShowSetCommandsTest, ShowTableAllowsOptionalObjectName)
{
    // SHOW TABLE without object name shows all tables (Firebird ISQL behavior)
    EXPECT_TRUE(parseSucceeds("SHOW TABLE"));
    EXPECT_TRUE(parseSucceeds("SHOW TABLE users"));
}

TEST_F(ShowSetCommandsTest, ShowIndexAllowsOptionalObjectName)
{
    // SHOW INDEX without object name shows all indexes (Firebird ISQL behavior)
    EXPECT_TRUE(parseSucceeds("SHOW INDEX"));
    EXPECT_TRUE(parseSucceeds("SHOW INDEX idx_users_email"));
}

TEST_F(ShowSetCommandsTest, SetSqlDialectRequiresNumber)
{
    EXPECT_FALSE(parseSucceeds("SET SQL DIALECT"));
    EXPECT_FALSE(parseSucceeds("SET SQL DIALECT abc"));
}

TEST_F(ShowSetCommandsTest, SetNamesRequiresCharset)
{
    EXPECT_FALSE(parseSucceeds("SET NAMES"));
}

TEST_F(ShowSetCommandsTest, SetLocalTimeoutRequiresNumber)
{
    EXPECT_FALSE(parseSucceeds("SET LOCAL_TIMEOUT"));
    EXPECT_FALSE(parseSucceeds("SET LOCAL_TIMEOUT abc"));
}
