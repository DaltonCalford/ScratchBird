/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// Integration tests for Wave 1 parser coverage (V2)

#include <gtest/gtest.h>

#include "scratchbird/parser/parser_v2.h"

#include <string>
#include <vector>

TEST(Wave1Integration, ParseWave1Statements)
{
    struct TestCase {
        std::string sql;
        std::string label;
    };

    const std::vector<TestCase> cases = {
        {"SELECT ST_POINT(1.5, 2.5) FROM locations", "Spatial SQL - ST_POINT"},
        {"SELECT ARRAY[1, 2, 3] FROM table1", "Array literal"},
        {"SELECT * FROM logs WHERE message ~ 'ERROR'", "Regex operator ~"},
        {"SELECT INITCAP(name) FROM users", "Text function INITCAP"}
    };

    for (const auto& test_case : cases)
    {
        scratchbird::parser::v2::Parser parser(test_case.sql);
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "Parse failed for: " << test_case.label;
    }
}
