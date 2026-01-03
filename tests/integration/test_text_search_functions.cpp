#include <gtest/gtest.h>

#include "scratchbird/parser/parser_v2.h"

#include <string>
#include <vector>

namespace {

bool parseOk(const std::string& sql)
{
    scratchbird::parser::v2::Parser parser(sql);
    auto result = parser.parseStatement();
    return result.success();
}

} // namespace

TEST(TextSearchFunctions, ParseIlikeAndRegexOperators)
{
    const std::vector<std::string> sqls = {
        "SELECT * FROM users WHERE name ILIKE '%john%'",
        "SELECT * FROM users WHERE email ~ '^[a-z]+@example\\.com$'",
        "SELECT * FROM products WHERE name ~* 'laptop'",
        "SELECT * FROM logs WHERE message !~ 'ERROR'",
        "SELECT * FROM data WHERE value !~* 'test'"
    };

    for (const auto& sql : sqls)
    {
        ASSERT_TRUE(parseOk(sql)) << "Parse failed for: " << sql;
    }
}

TEST(TextSearchFunctions, ParseRegexpFunctions)
{
    const std::vector<std::string> sqls = {
        "SELECT REGEXP_MATCHES('foobarbaz', 'b..')",
        "SELECT REGEXP_MATCHES('test123test', '[0-9]+', 'g')",
        "SELECT REGEXP_REPLACE('Hello World', 'World', 'Universe')",
        "SELECT REGEXP_REPLACE('foo bar', '\\\\s+', '_', 'g')",
        "SELECT REGEXP_SPLIT_TO_ARRAY('a,b,c,d', ',')",
        "SELECT REGEXP_SPLIT_TO_TABLE('x:y:z', ':')"
    };

    for (const auto& sql : sqls)
    {
        ASSERT_TRUE(parseOk(sql)) << "Parse failed for: " << sql;
    }
}

TEST(TextSearchFunctions, ParseStringUtilities)
{
    const std::vector<std::string> sqls = {
        "SELECT STRPOS('hello world', 'world')",
        "SELECT POSITION('test' IN 'this is a test')",
        "SELECT SPLIT_PART('a|b|c', '|', 2)",
        "SELECT OVERLAY('hello' PLACING 'XX' FROM 2 FOR 3)",
        "SELECT INITCAP('hello world')",
        "SELECT ASCII('A')",
        "SELECT CHR(65)",
        "SELECT REPEAT('*', 5)",
        "SELECT REVERSE('hello')"
    };

    for (const auto& sql : sqls)
    {
        ASSERT_TRUE(parseOk(sql)) << "Parse failed for: " << sql;
    }
}
