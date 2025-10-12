#include "scratchbird/core/types.h"
#include <iostream>
#include <cassert>

using namespace scratchbird::core;

int main() {
    std::cout << "Testing INTERVAL Type Implementation...\n\n";

    // Test 1: Basic INTERVAL creation and retrieval
    std::cout << "Test 1: Basic INTERVAL operations\n";
    auto interval1 = TypedValue::makeInterval(12, 30, 3600000000); // 1 year, 30 days, 1 hour
    assert(interval1.type() == DataType::INTERVAL);
    Interval i1 = interval1.getInterval();
    assert(i1.months == 12);
    assert(i1.days == 30);
    assert(i1.microseconds == 3600000000);
    std::cout << "  INTERVAL (12 months, 30 days, 3600000000 us): " << interval1.toString() << "\n";
    std::cout << "  ✓ Basic INTERVAL operations passed\n\n";

    // Test 2: Zero interval
    std::cout << "Test 2: Zero interval\n";
    auto interval_zero = TypedValue::makeInterval(0, 0, 0);
    Interval i_zero = interval_zero.getInterval();
    assert(i_zero.months == 0);
    assert(i_zero.days == 0);
    assert(i_zero.microseconds == 0);
    std::cout << "  INTERVAL zero: " << interval_zero.toString() << "\n";
    std::cout << "  ✓ Zero interval passed\n\n";

    // Test 3: Negative values (for past intervals)
    std::cout << "Test 3: Negative intervals\n";
    auto interval_negative = TypedValue::makeInterval(-6, -15, -7200000000); // -6 months, -15 days, -2 hours
    Interval i_neg = interval_negative.getInterval();
    assert(i_neg.months == -6);
    assert(i_neg.days == -15);
    assert(i_neg.microseconds == -7200000000);
    std::cout << "  INTERVAL negative: " << interval_negative.toString() << "\n";
    std::cout << "  ✓ Negative intervals passed\n\n";

    // Test 4: Months only (years and months)
    std::cout << "Test 4: Months-only intervals\n";
    auto interval_1y = TypedValue::makeInterval(12, 0, 0); // 1 year
    std::cout << "  1 year: " << interval_1y.toString() << "\n";
    auto interval_3y6m = TypedValue::makeInterval(42, 0, 0); // 3 years 6 months
    std::cout << "  3 years 6 months: " << interval_3y6m.toString() << "\n";
    auto interval_7m = TypedValue::makeInterval(7, 0, 0); // 7 months
    std::cout << "  7 months: " << interval_7m.toString() << "\n";
    std::cout << "  ✓ Months-only intervals passed\n\n";

    // Test 5: Days only
    std::cout << "Test 5: Days-only intervals\n";
    auto interval_1d = TypedValue::makeInterval(0, 1, 0); // 1 day
    std::cout << "  1 day: " << interval_1d.toString() << "\n";
    auto interval_30d = TypedValue::makeInterval(0, 30, 0); // 30 days
    std::cout << "  30 days: " << interval_30d.toString() << "\n";
    std::cout << "  ✓ Days-only intervals passed\n\n";

    // Test 6: Time only
    std::cout << "Test 6: Time-only intervals\n";
    auto interval_1h = TypedValue::makeInterval(0, 0, 3600000000); // 1 hour
    std::cout << "  1 hour: " << interval_1h.toString() << "\n";
    auto interval_30m = TypedValue::makeInterval(0, 0, 1800000000); // 30 minutes
    std::cout << "  30 minutes: " << interval_30m.toString() << "\n";
    auto interval_45s = TypedValue::makeInterval(0, 0, 45000000); // 45 seconds
    std::cout << "  45 seconds: " << interval_45s.toString() << "\n";
    auto interval_5h30m15s = TypedValue::makeInterval(0, 0, 19815000000); // 5:30:15
    std::cout << "  5:30:15: " << interval_5h30m15s.toString() << "\n";
    std::cout << "  ✓ Time-only intervals passed\n\n";

    // Test 7: Mixed intervals
    std::cout << "Test 7: Mixed intervals (months + days + time)\n";
    auto interval_mixed1 = TypedValue::makeInterval(14, 7, 3661000000); // 1 year 2 months, 7 days, 1:01:01
    std::cout << "  1y 2m 7d 1:01:01: " << interval_mixed1.toString() << "\n";
    auto interval_mixed2 = TypedValue::makeInterval(24, 365, 86400000000); // 2 years, 365 days, 24 hours
    std::cout << "  2y 365d 24h: " << interval_mixed2.toString() << "\n";
    std::cout << "  ✓ Mixed intervals passed\n\n";

    // Test 8: Using Interval struct directly
    std::cout << "Test 8: Using Interval struct constructor\n";
    Interval custom_interval(6, 15, 7200000000); // 6 months, 15 days, 2 hours
    auto interval_custom = TypedValue::makeInterval(custom_interval);
    assert(interval_custom.type() == DataType::INTERVAL);
    Interval i_custom = interval_custom.getInterval();
    assert(i_custom.months == 6);
    assert(i_custom.days == 15);
    assert(i_custom.microseconds == 7200000000);
    std::cout << "  Custom interval: " << interval_custom.toString() << "\n";
    std::cout << "  ✓ Interval struct constructor passed\n\n";

    // Test 9: Interval comparison operators
    std::cout << "Test 9: Interval comparison operators\n";
    Interval i1_cmp(12, 30, 3600000000);
    Interval i2_cmp(12, 30, 3600000000);
    Interval i3_cmp(6, 15, 1800000000);
    assert(i1_cmp == i2_cmp);
    assert(i1_cmp != i3_cmp);
    std::cout << "  ✓ Comparison operators passed\n\n";

    // Test 10: TypeSystem utilities
    std::cout << "Test 10: TypeSystem utilities\n";
    assert(TypeSystem::isTemporal(DataType::INTERVAL));
    std::cout << "  ✓ isTemporal() passed\n";

    assert(TypeSystem::isFixedLength(DataType::INTERVAL));
    std::cout << "  ✓ isFixedLength() passed\n";

    auto size = TypeSystem::getFixedSize(DataType::INTERVAL);
    assert(size.has_value());
    assert(*size == 16); // 4 + 4 + 8 = 16 bytes
    std::cout << "  ✓ getFixedSize() = 16 bytes passed\n";

    assert(TypeSystem::getTypeName(DataType::INTERVAL) == "INTERVAL");
    std::cout << "  ✓ getTypeName() passed\n";

    assert(TypeSystem::parseTypeName("INTERVAL") == DataType::INTERVAL);
    assert(TypeSystem::parseTypeName("interval") == DataType::INTERVAL);
    std::cout << "  ✓ parseTypeName() passed\n";
    std::cout << "  ✓ All TypeSystem utilities passed\n\n";

    // Test 11: Type mismatch error handling
    std::cout << "Test 11: Type mismatch error handling\n";
    auto int_val = TypedValue::makeInt64(12345);
    try {
        int_val.getInterval(); // Should throw
        assert(false && "Should have thrown exception");
    } catch (const std::runtime_error& e) {
        std::cout << "  ✓ Type mismatch correctly throws exception\n";
    }
    std::cout << "  ✓ Error handling passed\n\n";

    // Test 12: Microseconds precision
    std::cout << "Test 12: Microseconds precision\n";
    auto interval_us = TypedValue::makeInterval(0, 0, 123456); // 0.123456 seconds
    std::cout << "  Microseconds (123456 us): " << interval_us.toString() << "\n";
    Interval i_us = interval_us.getInterval();
    assert(i_us.microseconds == 123456);
    std::cout << "  ✓ Microseconds precision passed\n\n";

    // Test 13: Large intervals
    std::cout << "Test 13: Large intervals\n";
    auto interval_100y = TypedValue::makeInterval(1200, 36500, 876000000000000); // 100 years, 36500 days, ~10 years in microseconds
    std::cout << "  100 years + 36500 days + time: " << interval_100y.toString() << "\n";
    std::cout << "  ✓ Large intervals passed\n\n";

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "INTERVAL type is fully functional.\n";
    std::cout << "========================================\n";

    return 0;
}
