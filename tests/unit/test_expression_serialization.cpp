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
#include <memory>
#include <vector>
#include "scratchbird/core/expression.h"
#include "scratchbird/core/expression_serializer.h"
#include "scratchbird/sblr/opcodes.h"

using scratchbird::core::Expression;
using scratchbird::core::ExpressionSerializer;
using scratchbird::core::LiteralExpr;
using scratchbird::core::IdentifierExpr;
using scratchbird::core::AlterElementExpr;
using scratchbird::core::ExtractExpr;
using scratchbird::sblr::ExtractField;

TEST(ExpressionSerializationTest, AlterElementRoundTrip)
{
    auto arg = std::make_unique<LiteralExpr>(LiteralExpr::LiteralType::INTEGER);
    arg->setIntValue(2);

    auto source = std::make_unique<IdentifierExpr>("items");

    auto new_value = std::make_unique<LiteralExpr>(LiteralExpr::LiteralType::INTEGER);
    new_value->setIntValue(99);

    std::vector<std::unique_ptr<Expression>> args;
    args.push_back(std::move(arg));

    auto expr = std::make_unique<AlterElementExpr>(
        static_cast<uint8_t>(ExtractField::ELEMENT),
        "ELEMENT",
        std::move(args),
        std::move(source),
        std::move(new_value));

    std::vector<uint8_t> data = ExpressionSerializer::serialize(expr.get());
    auto round_trip = ExpressionSerializer::deserialize(data.data(), data.size());

    auto* alter = dynamic_cast<AlterElementExpr*>(round_trip.get());
    ASSERT_NE(alter, nullptr);
    EXPECT_EQ(alter->fieldId(), expr->fieldId());
    EXPECT_EQ(alter->fieldName(), "ELEMENT");
    ASSERT_EQ(alter->args().size(), 1u);

    auto* arg0 = dynamic_cast<LiteralExpr*>(alter->args()[0].get());
    ASSERT_NE(arg0, nullptr);
    EXPECT_EQ(arg0->intValue(), 2);

    auto* source_expr = dynamic_cast<IdentifierExpr*>(alter->source());
    ASSERT_NE(source_expr, nullptr);
    EXPECT_EQ(source_expr->name(), "items");

    auto* new_value_expr = dynamic_cast<LiteralExpr*>(alter->newValue());
    ASSERT_NE(new_value_expr, nullptr);
    EXPECT_EQ(new_value_expr->intValue(), 99);
}
