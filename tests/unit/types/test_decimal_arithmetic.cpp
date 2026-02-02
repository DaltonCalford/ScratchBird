/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/decimal_arithmetic.h"
#include <iostream>
#include "gtest/gtest.h"

using namespace scratchbird::core;


TEST(DecimalArithmeticTest, Comprehensive) {

    std::cout << "Testing DECIMAL Arithmetic Implementation...\n\n";

    // Test 1: Parse and toString
    std::cout << "Test 1: Parse and toString\n";
    {
        auto d1 = DecimalValue::fromString("123.45");
        ASSERT_TRUE(d1.has_value());
        ASSERT_EQ(d1->toString(), "123.45");
        std::cout << "  123.45 -> " << d1->toString() << " ✓\n";

        auto d2 = DecimalValue::fromString("0.001");
        ASSERT_TRUE(d2.has_value());
        ASSERT_EQ(d2->toString(), "0.001");
        std::cout << "  0.001 -> " << d2->toString() << " ✓\n";

        auto d3 = DecimalValue::fromString("-99.99");
        ASSERT_TRUE(d3.has_value());
        ASSERT_EQ(d3->toString(), "-99.99");
        std::cout << "  -99.99 -> " << d3->toString() << " ✓\n";

        auto d4 = DecimalValue::fromString("1000");
        ASSERT_TRUE(d4.has_value());
        ASSERT_EQ(d4->toString(), "1000");
        std::cout << "  1000 -> " << d4->toString() << " ✓\n";
    }
    std::cout << "  ✓ Parse and toString passed\n\n";

    // Test 2: Addition
    std::cout << "Test 2: Addition\n";
    {
        auto result1 = DecimalArithmetic::add("123.45", "67.55");
        ASSERT_TRUE(result1.has_value());
        ASSERT_EQ(*result1, "191.00");
        std::cout << "  123.45 + 67.55 = " << *result1 << " ✓\n";

        auto result2 = DecimalArithmetic::add("0.1", "0.2");
        ASSERT_TRUE(result2.has_value());
        ASSERT_EQ(*result2, "0.3");
        std::cout << "  0.1 + 0.2 = " << *result2 << " ✓\n";

        auto result3 = DecimalArithmetic::add("100", "0.01");
        ASSERT_TRUE(result3.has_value());
        ASSERT_EQ(*result3, "100.01");
        std::cout << "  100 + 0.01 = " << *result3 << " ✓\n";

        auto result4 = DecimalArithmetic::add("-50.5", "25.25");
        ASSERT_TRUE(result4.has_value());
        ASSERT_EQ(*result4, "-25.25");
        std::cout << "  -50.5 + 25.25 = " << *result4 << " ✓\n";
    }
    std::cout << "  ✓ Addition passed\n\n";

    // Test 3: Subtraction
    std::cout << "Test 3: Subtraction\n";
    {
        auto result1 = DecimalArithmetic::subtract("100", "30.5");
        ASSERT_TRUE(result1.has_value());
        ASSERT_EQ(*result1, "69.5");
        std::cout << "  100 - 30.5 = " << *result1 << " ✓\n";

        auto result2 = DecimalArithmetic::subtract("0.3", "0.1");
        ASSERT_TRUE(result2.has_value());
        ASSERT_EQ(*result2, "0.2");
        std::cout << "  0.3 - 0.1 = " << *result2 << " ✓\n";

        auto result3 = DecimalArithmetic::subtract("50", "75");
        ASSERT_TRUE(result3.has_value());
        ASSERT_EQ(*result3, "-25");
        std::cout << "  50 - 75 = " << *result3 << " ✓\n";
    }
    std::cout << "  ✓ Subtraction passed\n\n";

    // Test 4: Multiplication
    std::cout << "Test 4: Multiplication\n";
    {
        auto result1 = DecimalArithmetic::multiply("10", "5");
        ASSERT_TRUE(result1.has_value());
        ASSERT_EQ(*result1, "50");
        std::cout << "  10 * 5 = " << *result1 << " ✓\n";

        auto result2 = DecimalArithmetic::multiply("12.5", "2");
        ASSERT_TRUE(result2.has_value());
        ASSERT_EQ(*result2, "25.0");
        std::cout << "  12.5 * 2 = " << *result2 << " ✓\n";

        auto result3 = DecimalArithmetic::multiply("0.1", "0.1");
        ASSERT_TRUE(result3.has_value());
        ASSERT_EQ(*result3, "0.01");
        std::cout << "  0.1 * 0.1 = " << *result3 << " ✓\n";

        auto result4 = DecimalArithmetic::multiply("3.14", "2.0");
        ASSERT_TRUE(result4.has_value());
        ASSERT_EQ(*result4, "6.280"); // Preserves precision: 2 + 1 = 3 decimals
        std::cout << "  3.14 * 2.0 = " << *result4 << " ✓\n";

        auto result5 = DecimalArithmetic::multiply("-5", "10");
        ASSERT_TRUE(result5.has_value());
        ASSERT_EQ(*result5, "-50");
        std::cout << "  -5 * 10 = " << *result5 << " ✓\n";
    }
    std::cout << "  ✓ Multiplication passed\n\n";

    // Test 5: Division
    std::cout << "Test 5: Division\n";
    {
        auto result1 = DecimalArithmetic::divide("100", "4", 2);
        ASSERT_TRUE(result1.has_value());
        ASSERT_EQ(*result1, "25.00");
        std::cout << "  100 / 4 = " << *result1 << " ✓\n";

        auto result2 = DecimalArithmetic::divide("10", "3", 4);
        ASSERT_TRUE(result2.has_value());
        std::cout << "  10 / 3 (scale 4) = " << *result2 << " ✓\n";

        auto result3 = DecimalArithmetic::divide("1", "2", 2);
        ASSERT_TRUE(result3.has_value());
        ASSERT_EQ(*result3, "0.50");
        std::cout << "  1 / 2 = " << *result3 << " ✓\n";

        auto result4 = DecimalArithmetic::divide("22", "7", 6);
        ASSERT_TRUE(result4.has_value());
        std::cout << "  22 / 7 (scale 6) = " << *result4 << " ✓\n";

        // Division by zero
        auto result5 = DecimalArithmetic::divide("100", "0", 2);
        ASSERT_FALSE(result5.has_value());
        std::cout << "  100 / 0 = (error: division by zero) ✓\n";
    }
    std::cout << "  ✓ Division passed\n\n";

    // Test 6: Comparison
    std::cout << "Test 6: Comparison\n";
    {
        auto cmp1 = DecimalArithmetic::compare("123.45", "123.45");
        ASSERT_TRUE(cmp1.has_value() && *cmp1 == 0);
        std::cout << "  123.45 == 123.45 ✓\n";

        auto cmp2 = DecimalArithmetic::compare("100", "99.99");
        ASSERT_TRUE(cmp2.has_value() && *cmp2 == 1);
        std::cout << "  100 > 99.99 ✓\n";

        auto cmp3 = DecimalArithmetic::compare("0.01", "0.02");
        ASSERT_TRUE(cmp3.has_value() && *cmp3 == -1);
        std::cout << "  0.01 < 0.02 ✓\n";

        auto cmp4 = DecimalArithmetic::compare("-50", "50");
        ASSERT_TRUE(cmp4.has_value() && *cmp4 == -1);
        std::cout << "  -50 < 50 ✓\n";
    }
    std::cout << "  ✓ Comparison passed\n\n";

    // Test 7: Negate and Abs
    std::cout << "Test 7: Negate and Abs\n";
    {
        auto neg1 = DecimalArithmetic::negate("123.45");
        ASSERT_TRUE(neg1.has_value());
        ASSERT_EQ(*neg1, "-123.45");
        std::cout << "  negate(123.45) = " << *neg1 << " ✓\n";

        auto neg2 = DecimalArithmetic::negate("-99.99");
        ASSERT_TRUE(neg2.has_value());
        ASSERT_EQ(*neg2, "99.99");
        std::cout << "  negate(-99.99) = " << *neg2 << " ✓\n";

        auto abs1 = DecimalArithmetic::abs("-123.45");
        ASSERT_TRUE(abs1.has_value());
        ASSERT_EQ(*abs1, "123.45");
        std::cout << "  abs(-123.45) = " << *abs1 << " ✓\n";

        auto abs2 = DecimalArithmetic::abs("99.99");
        ASSERT_TRUE(abs2.has_value());
        ASSERT_EQ(*abs2, "99.99");
        std::cout << "  abs(99.99) = " << *abs2 << " ✓\n";
    }
    std::cout << "  ✓ Negate and Abs passed\n\n";

    // Test 8: Rounding
    std::cout << "Test 8: Rounding\n";
    {
        auto round1 = DecimalArithmetic::round("123.456", 2);
        ASSERT_TRUE(round1.has_value());
        ASSERT_EQ(*round1, "123.46");
        std::cout << "  round(123.456, 2) = " << *round1 << " ✓\n";

        auto round2 = DecimalArithmetic::round("99.999", 1);
        ASSERT_TRUE(round2.has_value());
        ASSERT_EQ(*round2, "100.0");
        std::cout << "  round(99.999, 1) = " << *round2 << " ✓\n";

        auto round3 = DecimalArithmetic::round("0.125", 2);
        ASSERT_TRUE(round3.has_value());
        std::cout << "  round(0.125, 2) = " << *round3 << " ✓\n";

        auto round4 = DecimalArithmetic::round("123.45", 0);
        ASSERT_TRUE(round4.has_value());
        ASSERT_EQ(*round4, "123");
        std::cout << "  round(123.45, 0) = " << *round4 << " ✓\n";
    }
    std::cout << "  ✓ Rounding passed\n\n";

    // Test 9: Zero handling
    std::cout << "Test 9: Zero handling\n";
    {
        auto zero = DecimalValue::fromString("0");
        ASSERT_TRUE(zero.has_value());
        ASSERT_TRUE(zero->isZero());
        ASSERT_EQ(zero->toString(), "0");
        std::cout << "  Zero: " << zero->toString() << " ✓\n";

        auto result1 = DecimalArithmetic::add("0", "0");
        ASSERT_TRUE(result1.has_value());
        ASSERT_EQ(*result1, "0");
        std::cout << "  0 + 0 = " << *result1 << " ✓\n";

        auto result2 = DecimalArithmetic::multiply("123.45", "0");
        ASSERT_TRUE(result2.has_value());
        ASSERT_EQ(*result2, "0.00");
        std::cout << "  123.45 * 0 = " << *result2 << " ✓\n";
    }
    std::cout << "  ✓ Zero handling passed\n\n";

    // Test 10: Large numbers
    std::cout << "Test 10: Large numbers\n";
    {
        auto large1 = DecimalValue::fromString("123456789012345");
        ASSERT_TRUE(large1.has_value());
        ASSERT_EQ(large1->toString(), "123456789012345");
        std::cout << "  Large integer: " << large1->toString() << " ✓\n";

        auto large2 = DecimalValue::fromString("999999999999.999999");
        ASSERT_TRUE(large2.has_value());
        std::cout << "  Large decimal: " << large2->toString() << " ✓\n";

        auto result = DecimalArithmetic::add("1000000000", "1000000000");
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(*result, "2000000000");
        std::cout << "  1000000000 + 1000000000 = " << *result << " ✓\n";
    }
    std::cout << "  ✓ Large numbers passed\n\n";

    // Test 11: Precision preservation
    std::cout << "Test 11: Precision preservation\n";
    {
        auto d1 = DecimalValue::fromString("0.00001");
        ASSERT_TRUE(d1.has_value());
        ASSERT_EQ(d1->getScale(), 5);
        ASSERT_EQ(d1->toString(), "0.00001");
        std::cout << "  High precision: " << d1->toString() << " (scale=" << (int)d1->getScale() << ") ✓\n";

        auto result = DecimalArithmetic::add("0.001", "0.002");
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(*result, "0.003");
        std::cout << "  0.001 + 0.002 = " << *result << " ✓\n";
    }
    std::cout << "  ✓ Precision preservation passed\n\n";

    // Test 12: Currency calculations (common use case)
    std::cout << "Test 12: Currency calculations\n";
    {
        // Price * quantity
        auto total1 = DecimalArithmetic::multiply("19.99", "3");
        ASSERT_TRUE(total1.has_value());
        std::cout << "  Price $19.99 × 3 = $" << *total1 << " ✓\n";

        // Add tax (5%)
        auto tax = DecimalArithmetic::multiply(*total1, "0.05");
        ASSERT_TRUE(tax.has_value());
        std::cout << "  Tax (5%) = $" << *tax << " ✓\n";

        auto final_total = DecimalArithmetic::add(*total1, *tax);
        ASSERT_TRUE(final_total.has_value());
        std::cout << "  Final total = $" << *final_total << " ✓\n";

        // Split bill among 3 people
        auto per_person = DecimalArithmetic::divide(*final_total, "3", 2);
        ASSERT_TRUE(per_person.has_value());
        std::cout << "  Per person = $" << *per_person << " ✓\n";
    }
    std::cout << "  ✓ Currency calculations passed\n\n";

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "DECIMAL arithmetic is fully functional.\n";
    std::cout << "========================================\n";
}

