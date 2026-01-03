#include <gtest/gtest.h>

#include "scratchbird/parser/parser_v2.h"

namespace {

bool parseOk(const std::string& sql)
{
    scratchbird::parser::v2::Parser parser(sql);
    auto result = parser.parseStatement();
    return result.success();
}

} // namespace

TEST(SubqueryParser, ParseScalarSubquery)
{
    ASSERT_TRUE(parseOk(
        "SELECT * FROM users WHERE salary > (SELECT AVG(salary) FROM employees)"
    ));
}

TEST(SubqueryParser, ParseExistsSubquery)
{
    ASSERT_TRUE(parseOk(
        "SELECT * FROM orders WHERE EXISTS (SELECT 1 FROM order_items WHERE order_id = orders.id)"
    ));
}

TEST(SubqueryParser, ParseInSubquery)
{
    ASSERT_TRUE(parseOk(
        "SELECT * FROM products WHERE category_id IN (SELECT id FROM categories WHERE active = 1)"
    ));
}

TEST(SubqueryParser, ParseNotInSubquery)
{
    ASSERT_TRUE(parseOk(
        "SELECT * FROM users WHERE id NOT IN (SELECT user_id FROM banned_users)"
    ));
}
