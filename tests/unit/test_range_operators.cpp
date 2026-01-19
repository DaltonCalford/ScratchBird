/**
 * Unit tests for Range Operators
 * Task 15 Phase 3: Range Operators Implementation
 *
 * Tests PostgreSQL-compatible range operators:
 * - && (overlaps)
 * - @> (contains range/element)
 * - <@ (contained by)
 * - << (strictly left of)
 * - >> (strictly right of)
 * - -|- (adjacent)
 * - & (intersection)
 * - + (union)
 * - - (difference)
 */

#include "scratchbird/core/range.h"
#include <iostream>
#include <cassert>
#include <stdexcept>

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
// Overlaps Operator (&&)
// ============================================================================

void test_overlaps_operator()
{
    std::cout << "\n=== test_overlaps_operator ===" << std::endl;

    Range<int> r1(1, 10, true, false);   // [1,10)
    Range<int> r2(5, 15, true, false);   // [5,15)
    Range<int> r3(10, 20, true, false);  // [10,20)
    Range<int> r4(11, 20, true, false);  // [11,20)

    test_assert(r1 && r2, "[1,10) && [5,15) = true");
    test_assert(r2 && r1, "[5,15) && [1,10) = true");
    test_assert(!(r1 && r3), "[1,10) && [10,20) = false (adjacent)");
    test_assert(!(r1 && r4), "[1,10) && [11,20) = false");
}

// ============================================================================
// Contains Operators (@>)
// ============================================================================

void test_contains_range_operator()
{
    std::cout << "\n=== test_contains_range_operator ===" << std::endl;

    Range<int> r1(1, 10, true, false);  // [1,10)
    Range<int> r2(2, 8, true, false);   // [2,8)
    Range<int> r3(5, 15, true, false);  // [5,15)
    Range<int> r4(0, 5, true, false);   // [0,5)

    test_assert(r1.containsRange(r2), "[1,10) @> [2,8) = true");
    test_assert(!r1.containsRange(r3), "[1,10) @> [5,15) = false");
    test_assert(!r1.containsRange(r4), "[1,10) @> [0,5) = false");
    test_assert(r1.containsRange(r1), "[1,10) @> [1,10) = true (self)");
}

void test_contains_element_operator()
{
    std::cout << "\n=== test_contains_element_operator ===" << std::endl;

    Range<int> r(5, 10, true, false);  // [5,10)

    test_assert(r.containsElement(5), "[5,10) @> 5 = true");
    test_assert(r.containsElement(7), "[5,10) @> 7 = true");
    test_assert(r.containsElement(9), "[5,10) @> 9 = true");
    test_assert(!r.containsElement(10), "[5,10) @> 10 = false");
    test_assert(!r.containsElement(4), "[5,10) @> 4 = false");
}

// ============================================================================
// Contained By Operator (<@)
// ============================================================================

void test_contained_by_operator()
{
    std::cout << "\n=== test_contained_by_operator ===" << std::endl;

    Range<int> r1(1, 10, true, false);  // [1,10)
    Range<int> r2(2, 8, true, false);   // [2,8)
    Range<int> r3(5, 15, true, false);  // [5,15)

    test_assert(r2.containedBy(r1), "[2,8) <@ [1,10) = true");
    test_assert(!r3.containedBy(r1), "[5,15) <@ [1,10) = false");
    test_assert(!r1.containedBy(r2), "[1,10) <@ [2,8) = false");
    test_assert(r1.containedBy(r1), "[1,10) <@ [1,10) = true (self)");
}

// ============================================================================
// Strictly Left Of Operator (<<)
// ============================================================================

void test_strictly_left_of_operator()
{
    std::cout << "\n=== test_strictly_left_of_operator ===" << std::endl;

    Range<int> r1(1, 5, true, false);    // [1,5)
    Range<int> r2(5, 10, true, false);   // [5,10)
    Range<int> r3(10, 15, true, false);  // [10,15)

    test_assert(r1 << r3, "[1,5) << [10,15) = true");
    test_assert(!(r1 << r2), "[1,5) << [5,10) = false (adjacent)");
    test_assert(!(r2 << r3), "[5,10) << [10,15) = false (adjacent)");
    test_assert(!(r3 << r1), "[10,15) << [1,5) = false");
}

// ============================================================================
// Strictly Right Of Operator (>>)
// ============================================================================

void test_strictly_right_of_operator()
{
    std::cout << "\n=== test_strictly_right_of_operator ===" << std::endl;

    Range<int> r1(1, 5, true, false);    // [1,5)
    Range<int> r2(5, 10, true, false);   // [5,10)
    Range<int> r3(10, 15, true, false);  // [10,15)

    test_assert(r3 >> r1, "[10,15) >> [1,5) = true");
    test_assert(!(r2 >> r1), "[5,10) >> [1,5) = false (adjacent)");
    test_assert(!(r3 >> r2), "[10,15) >> [5,10) = false (adjacent)");
    test_assert(!(r1 >> r3), "[1,5) >> [10,15) = false");
}

// ============================================================================
// Adjacent Operator (-|-)
// ============================================================================

void test_adjacent_operator()
{
    std::cout << "\n=== test_adjacent_operator ===" << std::endl;

    Range<int> r1(1, 5, true, false);    // [1,5)
    Range<int> r2(5, 10, true, false);   // [5,10)
    Range<int> r3(5, 10, false, false);  // (5,10)
    Range<int> r4(10, 15, true, false);  // [10,15)
    Range<int> r5(11, 20, true, false);  // [11,20)

    test_assert(r1.isAdjacent(r2), "[1,5) -|- [5,10) = true");
    test_assert(r2.isAdjacent(r1), "[5,10) -|- [1,5) = true");
    test_assert(!r1.isAdjacent(r3), "[1,5) -|- (5,10) = false (gap)");
    test_assert(r2.isAdjacent(r4), "[5,10) -|- [10,15) = true");
    test_assert(!r2.isAdjacent(r5), "[5,10) -|- [11,20) = false (gap)");
}

// ============================================================================
// Intersection Operator (&)
// ============================================================================

void test_intersection_operator()
{
    std::cout << "\n=== test_intersection_operator ===" << std::endl;

    Range<int> r1(1, 10, true, false);  // [1,10)
    Range<int> r2(5, 15, true, false);  // [5,15)
    Range<int> r3(10, 20, true, false); // [10,20)

    // Overlapping ranges
    auto i1 = r1 & r2;
    test_assert(!i1.isEmpty(), "[1,10) & [5,15) is not empty");
    test_assert(i1.lower() == 5 && i1.upper() == 10, "[1,10) & [5,15) = [5,10)");

    // Adjacent ranges (no overlap)
    auto i2 = r1 & r3;
    test_assert(i2.isEmpty(), "[1,10) & [10,20) = empty");
}

// ============================================================================
// Union Operator (+)
// ============================================================================

void test_union_operator()
{
    std::cout << "\n=== test_union_operator ===" << std::endl;

    Range<int> r1(1, 5, true, false);   // [1,5)
    Range<int> r2(5, 10, true, false);  // [5,10)
    Range<int> r3(3, 7, true, false);   // [3,7)
    Range<int> r4(10, 15, true, false); // [10,15)

    // Adjacent ranges
    auto u1 = r1 + r2;
    test_assert(u1.lower() == 1 && u1.upper() == 10, "[1,5) + [5,10) = [1,10)");

    // Overlapping ranges
    auto u2 = r1 + r3;
    test_assert(u2.lower() == 1 && u2.upper() == 7, "[1,5) + [3,7) = [1,7)");

    // Disjoint ranges (should throw)
    bool threw = false;
    try {
        auto u3 = r1 + r4;
    } catch (const std::runtime_error& e) {
        threw = true;
    }
    test_assert(threw, "[1,5) + [10,15) throws (disjoint)");
}

// ============================================================================
// Difference Operator (-)
// ============================================================================

void test_difference_operator()
{
    std::cout << "\n=== test_difference_operator ===" << std::endl;

    Range<int> r1(1, 10, true, false);  // [1,10)
    Range<int> r2(5, 15, true, false);  // [5,15)
    Range<int> r3(3, 7, true, false);   // [3,7)
    Range<int> r4(20, 30, true, false); // [20,30)

    // Non-overlapping
    auto d1 = r1 - r4;
    test_assert(!d1.isEmpty(), "[1,10) - [20,30) is not empty");
    test_assert(d1.lower() == 1 && d1.upper() == 10, "[1,10) - [20,30) = [1,10)");

    // Contained range (simplified - would need multirange for full support)
    auto d2 = r1 - r3;
    test_assert(!d2.isEmpty(), "[1,10) - [3,7) is not empty (lower remainder)");
    test_assert(d2.lower() == 1 && d2.upper() == 3, "[1,10) - [3,7) = [1,3)");

    // Overlapping range (simplified - returns single remainder)
    auto d3 = r1 - r2;
    test_assert(!d3.isEmpty(), "[1,10) - [5,15) is not empty (lower remainder)");
    test_assert(d3.lower() == 1 && d3.upper() == 5, "[1,10) - [5,15) = [1,5)");
}

// ============================================================================
// Operator Combinations
// ============================================================================

void test_operator_combinations()
{
    std::cout << "\n=== test_operator_combinations ===" << std::endl;

    Range<int> r1(1, 10, true, false);   // [1,10)
    Range<int> r2(5, 15, true, false);   // [5,15)
    Range<int> r3(10, 20, true, false);  // [10,20)

    // Overlaps and intersection
    if (r1 && r2) {
        auto intersection = r1 & r2;
        test_assert(intersection.lower() == 5, "Intersection of overlapping ranges");
    }

    // Adjacent and union
    if (r2.isAdjacent(r3)) {
        auto union_result = r2 + r3;
        test_assert(union_result.lower() == 5 && union_result.upper() == 20,
                   "Union of adjacent ranges");
    }

    // Contains and contained by (inverse)
    if (r1.containsRange(r1)) {
        test_assert(r1.containedBy(r1), "Self-containment");
    }
}

// ============================================================================
// NumRange Operators
// ============================================================================

void test_numrange_operators()
{
    std::cout << "\n=== test_numrange_operators ===" << std::endl;

    NumRange r1(0.0, 5.0, true, false);  // [0.0,5.0)
    NumRange r2(3.0, 8.0, true, false);  // [3.0,8.0)
    NumRange r3(5.0, 10.0, true, false); // [5.0,10.0)

    test_assert(r1 && r2, "[0.0,5.0) && [3.0,8.0) = true");
    test_assert(r1.containsElement(2.5), "[0.0,5.0) @> 2.5 = true");
    test_assert(r1.isAdjacent(r3), "[0.0,5.0) -|- [5.0,10.0) = true");

    auto intersection = r1 & r2;
    test_assert(intersection.lower() == 3.0 && intersection.upper() == 5.0,
               "[0.0,5.0) & [3.0,8.0) = [3.0,5.0)");

    auto union_result = r2 + r3;
    test_assert(union_result.lower() == 3.0 && union_result.upper() == 10.0,
               "[3.0,8.0) + [5.0,10.0) = [3.0,10.0)");
}

// ============================================================================
// DateRange Operators
// ============================================================================

void test_daterange_operators()
{
    std::cout << "\n=== test_daterange_operators ===" << std::endl;

    int64_t date1 = 18000;
    int64_t date2 = 18100;
    int64_t date3 = 18050;
    int64_t date4 = 18150;

    DateRange r1(date1, date2, true, false);  // [18000,18100)
    DateRange r2(date3, date4, true, false);  // [18050,18150)
    DateRange r3(date2, date4, true, false);  // [18100,18150)

    test_assert(r1 && r2, "DateRanges overlap");
    test_assert(r1.containsElement(date3), "DateRange contains date");
    test_assert(r1.isAdjacent(r3), "DateRanges adjacent");

    auto intersection = r1 & r2;
    test_assert(intersection.lower() == date3 && intersection.upper() == date2,
               "DateRange intersection correct");
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===========================================\n";
    std::cout << "Range Operators Unit Tests\n";
    std::cout << "Task 15 Phase 3: Range Operators\n";
    std::cout << "===========================================\n";

    // Basic operators
    test_overlaps_operator();
    test_contains_range_operator();
    test_contains_element_operator();
    test_contained_by_operator();
    test_strictly_left_of_operator();
    test_strictly_right_of_operator();
    test_adjacent_operator();
    test_intersection_operator();
    test_union_operator();
    test_difference_operator();

    // Combinations
    test_operator_combinations();

    // Type-specific
    test_numrange_operators();
    test_daterange_operators();

    // Summary
    std::cout << "\n===========================================\n";
    std::cout << "Tests passed: " << g_tests_passed << "\n";
    std::cout << "Tests failed: " << g_tests_failed << "\n";
    std::cout << "===========================================\n";

    return g_tests_failed > 0 ? 1 : 0;
}
