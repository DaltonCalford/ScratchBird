/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/types.h"
#include "scratchbird/core/typed_value.h"
#include <iostream>
#include "gtest/gtest.h"

using namespace scratchbird::core;


TEST(MoneyTypeTest, Comprehensive) {

    std::cout << "Testing MONEY Type Implementation...\n\n";

    // Test 1: Basic MONEY creation and retrieval
    std::cout << "Test 1: Basic MONEY operations\n";
    auto money1 = TypedValue::makeMoney(12345); // $123.45
    ASSERT_EQ(money1.type(), DataType::MONEY);
    ASSERT_EQ(money1.toInt64(), 12345);
    std::cout << "  MONEY value (12345 units): " << money1.toString() << "\n";
    ASSERT_EQ(money1.toString(), "1.2345");
    std::cout << "  ✓ Basic MONEY operations passed\n\n";

    // Test 2: Zero value
    std::cout << "Test 2: Zero value\n";
    auto money_zero = TypedValue::makeMoney(0);
    ASSERT_EQ(money_zero.toInt64(), 0);
    std::cout << "  MONEY zero: " << money_zero.toString() << "\n";
    ASSERT_EQ(money_zero.toString(), "0.0000");
    std::cout << "  ✓ Zero value passed\n\n";

    // Test 3: Negative values
    std::cout << "Test 3: Negative values\n";
    auto money_negative = TypedValue::makeMoney(-5000); // -$50.00
    ASSERT_EQ(money_negative.toInt64(), -5000);
    std::cout << "  MONEY negative (-5000 units): " << money_negative.toString() << "\n";
    ASSERT_EQ(money_negative.toString(), "-0.5000");
    std::cout << "  ✓ Negative values passed\n\n";

    // Test 4: Large values
    std::cout << "Test 4: Large values\n";
    auto money_large = TypedValue::makeMoney(1234567890); // $12,345,678.90
    ASSERT_EQ(money_large.toInt64(), 1234567890);
    std::cout << "  MONEY large (1234567890 units): " << money_large.toString() << "\n";
    ASSERT_EQ(money_large.toString(), "123456.7890");
    std::cout << "  ✓ Large values passed\n\n";

    // Test 5: Penny values (cents)
    std::cout << "Test 5: Penny values\n";
    auto money_pennies = TypedValue::makeMoney(99); // $0.99
    ASSERT_EQ(money_pennies.toInt64(), 99);
    std::cout << "  MONEY pennies (99 units): " << money_pennies.toString() << "\n";
    ASSERT_EQ(money_pennies.toString(), "0.0099");
    std::cout << "  ✓ Penny values passed\n\n";

    // Test 6: Single penny
    std::cout << "Test 6: Single penny\n";
    auto money_penny = TypedValue::makeMoney(1); // $0.01
    ASSERT_EQ(money_penny.toInt64(), 1);
    std::cout << "  MONEY penny (1 unit): " << money_penny.toString() << "\n";
    ASSERT_EQ(money_penny.toString(), "0.0001");
    std::cout << "  ✓ Single penny passed\n\n";

    // Test 7: TypeSystem utilities
    std::cout << "Test 7: TypeSystem utilities\n";
    ASSERT_STREQ(TypeSystem::getTypeName(DataType::MONEY), "MONEY");
    std::cout << "  ✓ getTypeName() passed\n";
    std::cout << "  ✓ All TypeSystem utilities passed\n\n";

    // Test 8: Type mismatch error handling
    std::cout << "Test 8: Type mismatch error handling\n";
    try {
        money1.getInt64(); // Should throw (money is not INT64)
        ASSERT_TRUE(false && "Should have thrown exception");
    } catch (const std::runtime_error& e) {
        std::cout << "  ✓ Type mismatch correctly throws exception\n";
    }
    std::cout << "  ✓ Error handling passed\n\n";

    // Test 9: Common currency amounts
    std::cout << "Test 9: Common currency amounts\n";
    struct TestCase {
        int64_t cents;
        const char* expected;
    };

    TestCase test_cases[] = {
        {100, "0.0100"},
        {1000, "0.1000"},
        {10000, "1.0000"},
        {100000, "10.0000"},
        {1000000, "100.0000"},
        {50, "0.0050"},
        {25, "0.0025"},
        {10, "0.0010"},
        {5, "0.0005"},
        {-100, "-0.0100"},
        {-1234, "-0.1234"}
    };

    for (const auto& tc : test_cases) {
        auto val = TypedValue::makeMoney(tc.cents);
        std::string result = val.toString();
        if (result != tc.expected) {
            std::cout << "  ✗ FAILED: " << tc.cents << " cents -> got '" << result
                      << "', expected '" << tc.expected << "'\n";
            FAIL(); return;
        }
    }
    std::cout << "  ✓ All common currency amounts passed\n\n";

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "MONEY type is fully functional.\n";
    std::cout << "========================================\n";
}
