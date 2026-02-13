/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Firebird Parser Window Specification Tests
 *
 * Validates parsing of OVER() window specifications in Firebird syntax.
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/firebird/firebird_parser.h"

namespace fb = scratchbird::parser::firebird;
namespace ast = scratchbird::parser::v3;

using fb::Parser;
using ast::FunctionCallExpr;
using ast::SelectItem;
using ast::SelectStmt;

TEST(FirebirdParserWindowTest, ParsesWindowSpecification) {
    Parser parser(
        "SELECT SUM(amount) OVER (PARTITION BY customer_id ORDER BY order_date "
        "ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) FROM orders");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.statement, nullptr);
    ASSERT_EQ(result.statement->kind(), ast::ASTKind::SelectStmt);

    auto* stmt = static_cast<SelectStmt*>(result.statement.get());
    ASSERT_EQ(stmt->items.size(), 1u);
    auto* item = stmt->items[0];
    ASSERT_EQ(item->item_type, SelectItem::Type::EXPRESSION);

    auto* func = dynamic_cast<FunctionCallExpr*>(item->expr);
    ASSERT_NE(func, nullptr);
    EXPECT_TRUE(func->is_window);
    ASSERT_NE(func->window, nullptr);

    auto* spec = func->window;
    EXPECT_EQ(spec->partition_by.size(), 1u);
    EXPECT_EQ(spec->order_by.size(), 1u);
    EXPECT_TRUE(spec->has_frame);
    EXPECT_EQ(spec->frame_type, ast::FrameType::ROWS);
    EXPECT_EQ(spec->frame_start, ast::FrameBoundType::VALUE_PRECEDING);
    EXPECT_NE(spec->frame_start_value, nullptr);
    EXPECT_EQ(spec->frame_end, ast::FrameBoundType::CURRENT_ROW);
    EXPECT_EQ(spec->frame_end_value, nullptr);
}
