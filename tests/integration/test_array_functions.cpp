/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
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

TEST(ArrayFunctions, ParseArrayStatements)
{
    const std::vector<std::string> sqls = {
        "SELECT ARRAY[1, 2, 3, 4, 5]",
        "SELECT ARRAY[ARRAY[1,2], ARRAY[3,4]]",
        "SELECT ARRAY[1 + 2, 3 * 4, 5]",
        "SELECT ARRAY[]",
        "SELECT ARRAY_APPEND(ARRAY[1,2], 3)",
        "SELECT ARRAY_PREPEND(0, ARRAY[1,2,3])",
        "SELECT ARRAY_CAT(ARRAY[1,2], ARRAY[3,4])",
        "SELECT ARRAY_LENGTH(ARRAY[1,2,3,4,5])",
        "SELECT UNNEST(ARRAY[1,2,3])",
        "SELECT ARRAY['foo', 'bar', 'baz']",
        R"(
            SELECT
                ARRAY[id, id * 2, id * 3] as multiples,
                ARRAY_LENGTH(ARRAY[1,2,3,4,5]) as len,
                ARRAY_APPEND(ARRAY[1,2], id) as with_id
            FROM users
            WHERE id > 0
        )"
    };

    for (const auto& sql : sqls)
    {
        ASSERT_TRUE(parseOk(sql)) << "Parse failed for: " << sql;
    }
}
