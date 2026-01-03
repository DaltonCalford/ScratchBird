#include <gtest/gtest.h>

#include "scratchbird/parser/parser_v2.h"

TEST(TextSearchSimple, ParseIlikeExpression)
{
    const std::string sql = "SELECT * FROM users WHERE email ILIKE '%@gmail.com'";

    scratchbird::parser::v2::Parser parser(sql);
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
}
