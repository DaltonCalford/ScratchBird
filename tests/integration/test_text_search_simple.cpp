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

TEST(TextSearchSimple, ParseIlikeExpression)
{
    const std::string sql = "SELECT * FROM users WHERE email ILIKE '%@gmail.com'";

    scratchbird::parser::v2::Parser parser(sql);
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
}
