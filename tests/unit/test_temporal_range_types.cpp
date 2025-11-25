/**
 * Unit tests for Temporal Range Types
 * Task 15 Phase 2: Temporal Range Types Implementation
 *
 * Tests PostgreSQL-compatible temporal range types:
 * - DateRange, TSRange, TSTZRange
 * - All inherit Range<int64_t> functionality
 * - TypedValue integration
 */

#include "scratchbird/core/range.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/typed_value.h"
#include <iostream>
#include <cassert>

using namespace scratchbird::core;

// Test harness
int g_tests_passed = 0;
int g_tests_failed = 0;

#define test_assert(condition, message)                                                            \
    do                                                                                             \
    {                                                                                              \
        if (condition)                                                                             \
        {                                                                                          \
            std::cout << "[PASS] " << message << std::endl;                                       \
            g_tests_passed++;                                                                      \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            std::cout << "[FAIL] " << message << std::endl;                                       \
            g_tests_failed++;                                                                      \
        }                                                                                          \
    } while (0)

// ============================================================================
// DateRange Tests
// ============================================================================

void test_daterange_construction()
{
    std::cout << "\n=== test_daterange_construction ===" << std::endl;

    // Date values as days since epoch (simplified)
    int64_t date1 = 18000;  // Some date
    int64_t date2 = 18365;  // 365 days later

    DateRange r(date1, date2, true, false);  // [date1, date2)
    test_assert(!r.isEmpty(), "DateRange [date1,date2) is not empty");
    test_assert(r.lower() == date1, "DateRange lower bound is correct");
    test_assert(r.upper() == date2, "DateRange upper bound is correct");
}

void test_daterange_operations()
{
    std::cout << "\n=== test_daterange_operations ===" << std::endl;

    int64_t date1 = 18000;
    int64_t date2 = 18100;
    int64_t date3 = 18050;
    int64_t date4 = 18150;

    DateRange r1(date1, date2, true, false);  // [18000,18100)
    DateRange r2(date3, date4, true, false);  // [18050,18150)

    test_assert(r1.overlaps(r2), "DateRanges overlap");

    auto intersection = r1.intersection(r2);
    test_assert(!intersection.isEmpty(), "Intersection is not empty");
    test_assert(intersection.lower() == date3, "Intersection lower is 18050");
    test_assert(intersection.upper() == date2, "Intersection upper is 18100");
}

void test_daterange_typedvalue()
{
    std::cout << "\n=== test_daterange_typedvalue ===" << std::endl;

    int64_t date1 = 18000;
    int64_t date2 = 18100;

    DateRange r(date1, date2, true, false);
    TypedValue v = TypedValue::makeDateRange(r);

    test_assert(v.type() == DataType::DATERANGE, "TypedValue type is DATERANGE");

    const Range<int64_t>& retrieved = v.getDateRange<int64_t>();
    test_assert(retrieved.lower() == date1, "Retrieved lower bound correct");
    test_assert(retrieved.upper() == date2, "Retrieved upper bound correct");

    std::string str = v.toString();
    test_assert(str == "[18000,18100)", "DateRange toString correct");
}

// ============================================================================
// TSRange Tests
// ============================================================================

void test_tsrange_construction()
{
    std::cout << "\n=== test_tsrange_construction ===" << std::endl;

    // Timestamp values as microseconds since epoch
    int64_t ts1 = 1700000000000000LL;  // Some timestamp
    int64_t ts2 = 1700100000000000LL;  // Later timestamp

    TSRange r(ts1, ts2, true, false);  // [ts1, ts2)
    test_assert(!r.isEmpty(), "TSRange [ts1,ts2) is not empty");
    test_assert(r.contains(ts1), "TSRange contains ts1");
    test_assert(!r.contains(ts2), "TSRange does not contain ts2 (exclusive)");
}

void test_tsrange_operations()
{
    std::cout << "\n=== test_tsrange_operations ===" << std::endl;

    int64_t ts1 = 1700000000000000LL;
    int64_t ts2 = 1700100000000000LL;
    int64_t ts3 = 1700050000000000LL;
    int64_t ts4 = 1700150000000000LL;

    TSRange r1(ts1, ts2, true, false);  // [ts1,ts2)
    TSRange r2(ts3, ts4, true, false);  // [ts3,ts4)

    test_assert(r1.overlaps(r2), "TSRanges overlap");
    test_assert(r1.contains(ts3), "r1 contains ts3");
    test_assert(!r2.contains(ts1), "r2 does not fully contain ts1 range");
}

void test_tsrange_typedvalue()
{
    std::cout << "\n=== test_tsrange_typedvalue ===" << std::endl;

    int64_t ts1 = 1700000000000000LL;
    int64_t ts2 = 1700100000000000LL;

    TSRange r(ts1, ts2, true, true);  // [ts1,ts2]
    TypedValue v = TypedValue::makeTSRange(r);

    test_assert(v.type() == DataType::TSRANGE, "TypedValue type is TSRANGE");

    const Range<int64_t>& retrieved = v.getTSRange<int64_t>();
    test_assert(retrieved.lower() == ts1, "Retrieved lower bound correct");
    test_assert(retrieved.upper() == ts2, "Retrieved upper bound correct");
    test_assert(retrieved.isLowerInclusive(), "Lower bound is inclusive");
    test_assert(retrieved.isUpperInclusive(), "Upper bound is inclusive");
}

// ============================================================================
// TSTZRange Tests
// ============================================================================

void test_tstzrange_construction()
{
    std::cout << "\n=== test_tstzrange_construction ===" << std::endl;

    // Timestamp with timezone values
    int64_t tstz1 = 1700000000000000LL;
    int64_t tstz2 = 1700100000000000LL;

    TSTZRange r(tstz1, tstz2, true, false);
    test_assert(!r.isEmpty(), "TSTZRange is not empty");
    test_assert(r.lower() == tstz1, "TSTZRange lower bound correct");
}

void test_tstzrange_operations()
{
    std::cout << "\n=== test_tstzrange_operations ===" << std::endl;

    int64_t tstz1 = 1700000000000000LL;
    int64_t tstz2 = 1700100000000000LL;
    int64_t tstz3 = 1700200000000000LL;

    TSTZRange r1(tstz1, tstz2, true, false);
    TSTZRange r2(tstz2, tstz3, true, false);

    test_assert(r1.isAdjacentTo(r2), "TSTZRanges are adjacent");

    auto union_result = r1.rangeUnion(r2);
    test_assert(union_result.has_value(), "Union of adjacent ranges exists");
    if (union_result) {
        test_assert(union_result->lower() == tstz1, "Union lower is tstz1");
        test_assert(union_result->upper() == tstz3, "Union upper is tstz3");
    }
}

void test_tstzrange_typedvalue()
{
    std::cout << "\n=== test_tstzrange_typedvalue ===" << std::endl;

    int64_t tstz1 = 1700000000000000LL;
    int64_t tstz2 = 1700100000000000LL;

    TSTZRange r(tstz1, tstz2, false, true);  // (tstz1,tstz2]
    TypedValue v = TypedValue::makeTSTZRange(r);

    test_assert(v.type() == DataType::TSTZRANGE, "TypedValue type is TSTZRANGE");

    const Range<int64_t>& retrieved = v.getTSTZRange<int64_t>();
    test_assert(!retrieved.isLowerInclusive(), "Lower bound is exclusive");
    test_assert(retrieved.isUpperInclusive(), "Upper bound is inclusive");

    std::string str = v.toString();
    test_assert(str.find("(") != std::string::npos, "toString shows exclusive lower");
    test_assert(str.find("]") != std::string::npos, "toString shows inclusive upper");
}

// ============================================================================
// Unbounded Temporal Ranges
// ============================================================================

void test_unbounded_temporal_ranges()
{
    std::cout << "\n=== test_unbounded_temporal_ranges ===" << std::endl;

    int64_t date1 = 18000;

    // Unbounded upper date range: [date1,)
    DateRange r1(date1, std::nullopt, true, false);
    test_assert(!r1.isEmpty(), "Unbounded upper DateRange is not empty");
    test_assert(r1.isLowerBounded(), "Lower is bounded");
    test_assert(!r1.isUpperBounded(), "Upper is unbounded");
    test_assert(r1.contains(date1 + 1000000), "Contains future dates");

    // Unbounded lower timestamp range: (,ts1]
    int64_t ts1 = 1700000000000000LL;
    TSRange r2(std::nullopt, ts1, false, true);
    test_assert(!r2.isEmpty(), "Unbounded lower TSRange is not empty");
    test_assert(!r2.isLowerBounded(), "Lower is unbounded");
    test_assert(r2.isUpperBounded(), "Upper is bounded");
    test_assert(r2.contains(ts1 - 1000000), "Contains past timestamps");
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===========================================\n";
    std::cout << "Temporal Range Types Unit Tests\n";
    std::cout << "Task 15 Phase 2: Temporal Range Types\n";
    std::cout << "===========================================\n";

    // DateRange tests
    test_daterange_construction();
    test_daterange_operations();
    test_daterange_typedvalue();

    // TSRange tests
    test_tsrange_construction();
    test_tsrange_operations();
    test_tsrange_typedvalue();

    // TSTZRange tests
    test_tstzrange_construction();
    test_tstzrange_operations();
    test_tstzrange_typedvalue();

    // Unbounded tests
    test_unbounded_temporal_ranges();

    // Summary
    std::cout << "\n===========================================\n";
    std::cout << "Tests passed: " << g_tests_passed << "\n";
    std::cout << "Tests failed: " << g_tests_failed << "\n";
    std::cout << "===========================================\n";

    return g_tests_failed > 0 ? 1 : 0;
}
