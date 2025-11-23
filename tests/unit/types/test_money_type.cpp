#include "scratchbird/core/types.h"
#include <iostream>
#include "gtest/gtest.h"

using namespace scratchbird::core;


TEST(MoneyTypeTest, Comprehensive) {

    std::cout << "Testing MONEY Type Implementation...\n\n";

    // Test 1: Basic MONEY creation and retrieval
    std::cout << "Test 1: Basic MONEY operations\n";
    auto money1 = TypedValue::makeMoney(12345); // $123.45
    ASSERT_EQ(money1.type(), DataType::MONEY);
    ASSERT_EQ(money1.getMoney(), 12345);
    std::cout << "  MONEY value (12345 cents): " << money1.toString() << "\n";
    ASSERT_EQ(money1.toString(), "$123.45");
    std::cout << "  ✓ Basic MONEY operations passed\n\n";

    // Test 2: Zero value
    std::cout << "Test 2: Zero value\n";
    auto money_zero = TypedValue::makeMoney(0);
    ASSERT_EQ(money_zero.getMoney(), 0);
    std::cout << "  MONEY zero: " << money_zero.toString() << "\n";
    ASSERT_EQ(money_zero.toString(), "$0.00");
    std::cout << "  ✓ Zero value passed\n\n";

    // Test 3: Negative values
    std::cout << "Test 3: Negative values\n";
    auto money_negative = TypedValue::makeMoney(-5000); // -$50.00
    ASSERT_EQ(money_negative.getMoney(), -5000);
    std::cout << "  MONEY negative (-5000 cents): " << money_negative.toString() << "\n";
    ASSERT_EQ(money_negative.toString(), "-$50.00");
    std::cout << "  ✓ Negative values passed\n\n";

    // Test 4: Large values
    std::cout << "Test 4: Large values\n";
    auto money_large = TypedValue::makeMoney(1234567890); // $12,345,678.90
    ASSERT_EQ(money_large.getMoney(), 1234567890);
    std::cout << "  MONEY large (1234567890 cents): " << money_large.toString() << "\n";
    ASSERT_EQ(money_large.toString(), "$12345678.90");
    std::cout << "  ✓ Large values passed\n\n";

    // Test 5: Penny values (cents)
    std::cout << "Test 5: Penny values\n";
    auto money_pennies = TypedValue::makeMoney(99); // $0.99
    ASSERT_EQ(money_pennies.getMoney(), 99);
    std::cout << "  MONEY pennies (99 cents): " << money_pennies.toString() << "\n";
    ASSERT_EQ(money_pennies.toString(), "$0.99");
    std::cout << "  ✓ Penny values passed\n\n";

    // Test 6: Single penny
    std::cout << "Test 6: Single penny\n";
    auto money_penny = TypedValue::makeMoney(1); // $0.01
    ASSERT_EQ(money_penny.getMoney(), 1);
    std::cout << "  MONEY penny (1 cent): " << money_penny.toString() << "\n";
    ASSERT_EQ(money_penny.toString(), "$0.01");
    std::cout << "  ✓ Single penny passed\n\n";

    // Test 7: TypeSystem utilities
    std::cout << "Test 7: TypeSystem utilities\n";
    ASSERT_TRUE(TypeSystem::isNumeric(DataType::MONEY));
    std::cout << "  ✓ isNumeric() passed\n";

    ASSERT_TRUE(TypeSystem::isFixedLength(DataType::MONEY));
    std::cout << "  ✓ isFixedLength() passed\n";

    ASSERT_EQ(TypeSystem::getFixedSize(DataType::MONEY), 8);
    std::cout << "  ✓ getFixedSize() = 8 bytes passed\n";

    ASSERT_EQ(TypeSystem::getTypeName(DataType::MONEY), "MONEY");
    std::cout << "  ✓ getTypeName() passed\n";

    ASSERT_EQ(TypeSystem::parseTypeName("MONEY"), DataType::MONEY);
    ASSERT_EQ(TypeSystem::parseTypeName("money"), DataType::MONEY);
    std::cout << "  ✓ parseTypeName() passed\n";
    std::cout << "  ✓ All TypeSystem utilities passed\n\n";

    // Test 8: Type mismatch error handling
    std::cout << "Test 8: Type mismatch error handling\n";
    auto int_val = TypedValue::makeInt64(12345);
    try {
        int_val.getMoney(); // Should throw
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
        {100, "$1.00"},
        {1000, "$10.00"},
        {10000, "$100.00"},
        {100000, "$1000.00"},
        {1000000, "$10000.00"},
        {50, "$0.50"},
        {25, "$0.25"},
        {10, "$0.10"},
        {5, "$0.05"},
        {-100, "-$1.00"},
        {-1234, "-$12.34"}
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

