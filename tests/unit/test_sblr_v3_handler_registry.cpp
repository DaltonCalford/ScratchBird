#include <gtest/gtest.h>

#include "scratchbird/sblr/v3_handler_registry.h"

using namespace scratchbird::sblr::v3;

TEST(SBLRV3HandlerRegistry, RegisterAndLookup) {
    HandlerRegistry& registry = HandlerRegistry::instance();
    registry.clearForTests();

    EXPECT_EQ(registry.statementHandlerCount(), static_cast<std::size_t>(0));
    EXPECT_FALSE(registry.hasStatementHandler("OP_STMT_DML_SELECT"));

    registry.registerStatementHandler("OP_STMT_DML_SELECT");
    EXPECT_TRUE(registry.hasStatementHandler("OP_STMT_DML_SELECT"));
    EXPECT_EQ(registry.statementHandlerCount(), static_cast<std::size_t>(1));
}

TEST(SBLRV3HandlerRegistry, DuplicateRegistrationIsIdempotent) {
    HandlerRegistry& registry = HandlerRegistry::instance();
    registry.clearForTests();

    registry.registerStatementHandler("OP_STMT_DML_SELECT");
    registry.registerStatementHandler("OP_STMT_DML_SELECT");
    registry.registerStatementHandler("OP_STMT_DDL_CREATE_TABLE");

    EXPECT_EQ(registry.statementHandlerCount(), static_cast<std::size_t>(2));
    EXPECT_TRUE(registry.hasStatementHandler("OP_STMT_DML_SELECT"));
    EXPECT_TRUE(registry.hasStatementHandler("OP_STMT_DDL_CREATE_TABLE"));
}

