/**
 * Unit tests for Range Types
 * Task 15: Range Types Implementation
 *
 * Tests PostgreSQL-compatible range types:
 * - Generic Range<T> template
 * - Int4Range, Int8Range, NumRange
 * - Range operations: contains, overlaps, union, intersection
 * - Range functions: lower, upper, isempty, etc.
 * - Range operators: &&, @>, <@, <<, >>
 */

#include "scratchbird/core/range.h"
#include "scratchbird/core/range_functions.h"
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
// Construction Tests
// ============================================================================

void test_empty_range()
{
    std::cout << "\n=== test_empty_range ===" << std::endl;

    Range<int> r;
    test_assert(r.isEmpty(), "Default constructor creates empty range");
    test_assert(r.toString() == "empty", "Empty range string representation");
}

void test_basic_construction()
{
    std::cout << "\n=== test_basic_construction ===" << std::endl;

    // [1,10)
    Range<int> r1(1, 10, true, false);
    test_assert(!r1.isEmpty(), "Range [1,10) is not empty");
    test_assert(r1.lower().has_value() && *r1.lower() == 1, "Lower bound is 1");
    test_assert(r1.upper().has_value() && *r1.upper() == 10, "Upper bound is 10");
    test_assert(r1.isLowerInclusive(), "Lower bound is inclusive");
    test_assert(!r1.isUpperInclusive(), "Upper bound is exclusive");
}

void test_unbounded_ranges()
{
    std::cout << "\n=== test_unbounded_ranges ===" << std::endl;

    // (,10] - unbounded lower
    Range<int> r1(std::nullopt, 10, false, true);
    test_assert(!r1.isEmpty(), "Unbounded lower range is not empty");
    test_assert(!r1.isLowerBounded(), "Lower is unbounded");
    test_assert(r1.isUpperBounded(), "Upper is bounded");

    // [5,) - unbounded upper
    Range<int> r2(5, std::nullopt, true, false);
    test_assert(!r2.isEmpty(), "Unbounded upper range is not empty");
    test_assert(r2.isLowerBounded(), "Lower is bounded");
    test_assert(!r2.isUpperBounded(), "Upper is unbounded");

    // (,) - completely unbounded
    Range<int> r3(std::nullopt, std::nullopt, false, false);
    test_assert(!r3.isEmpty(), "Completely unbounded range is not empty");
    test_assert(!r3.isLowerBounded(), "Lower is unbounded");
    test_assert(!r3.isUpperBounded(), "Upper is unbounded");
}

void test_degenerate_ranges()
{
    std::cout << "\n=== test_degenerate_ranges ===" << std::endl;

    // [5,5] - single point
    Range<int> r1(5, 5, true, true);
    test_assert(!r1.isEmpty(), "[5,5] is not empty");
    test_assert(r1.contains(5), "[5,5] contains 5");

    // (5,5) - empty
    Range<int> r2(5, 5, false, false);
    test_assert(r2.isEmpty(), "(5,5) is empty");

    // [5,5) - empty
    Range<int> r3(5, 5, true, false);
    test_assert(r3.isEmpty(), "[5,5) is empty");

    // (5,5] - empty
    Range<int> r4(5, 5, false, true);
    test_assert(r4.isEmpty(), "(5,5] is empty");
}

void test_invalid_ranges()
{
    std::cout << "\n=== test_invalid_ranges ===" << std::endl;

    // [10,5] - lower > upper, should be empty
    Range<int> r1(10, 5, true, true);
    test_assert(r1.isEmpty(), "[10,5] is empty (invalid)");
}

// ============================================================================
// Contains Tests
// ============================================================================

void test_contains_value()
{
    std::cout << "\n=== test_contains_value ===" << std::endl;

    Range<int> r(5, 10, true, false);  // [5,10)

    test_assert(r.contains(5), "[5,10) contains 5");
    test_assert(r.contains(7), "[5,10) contains 7");
    test_assert(r.contains(9), "[5,10) contains 9");
    test_assert(!r.contains(10), "[5,10) does not contain 10");
    test_assert(!r.contains(4), "[5,10) does not contain 4");
    test_assert(!r.contains(11), "[5,10) does not contain 11");
}

void test_contains_range()
{
    std::cout << "\n=== test_contains_range ===" << std::endl;

    Range<int> r1(1, 10, true, false);  // [1,10)
    Range<int> r2(2, 8, true, false);   // [2,8)
    Range<int> r3(5, 15, true, false);  // [5,15)
    Range<int> r4(0, 5, true, false);   // [0,5)

    test_assert(r1.contains(r2), "[1,10) contains [2,8)");
    test_assert(!r1.contains(r3), "[1,10) does not contain [5,15)");
    test_assert(!r1.contains(r4), "[1,10) does not contain [0,5)");
    test_assert(r1.contains(r1), "[1,10) contains itself");
}

// ============================================================================
// Overlaps Tests
// ============================================================================

void test_overlaps()
{
    std::cout << "\n=== test_overlaps ===" << std::endl;

    Range<int> r1(1, 10, true, false);   // [1,10)
    Range<int> r2(5, 15, true, false);   // [5,15)
    Range<int> r3(10, 20, true, false);  // [10,20)
    Range<int> r4(11, 20, true, false);  // [11,20)

    test_assert(r1.overlaps(r2), "[1,10) overlaps [5,15)");
    test_assert(r2.overlaps(r1), "[5,15) overlaps [1,10)");
    test_assert(!r1.overlaps(r3), "[1,10) does not overlap [10,20) (adjacent)");
    test_assert(!r1.overlaps(r4), "[1,10) does not overlap [11,20)");
}

void test_is_left_of()
{
    std::cout << "\n=== test_is_left_of ===" << std::endl;

    Range<int> r1(1, 5, true, false);    // [1,5)
    Range<int> r2(5, 10, true, false);   // [5,10)
    Range<int> r3(10, 15, true, false);  // [10,15)

    test_assert(r1.isLeftOf(r3), "[1,5) is left of [10,15)");
    test_assert(!r1.isLeftOf(r2), "[1,5) is not left of [5,10) (adjacent)");
    test_assert(!r2.isLeftOf(r3), "[5,10) is not left of [10,15) (adjacent)");
}

void test_is_right_of()
{
    std::cout << "\n=== test_is_right_of ===" << std::endl;

    Range<int> r1(1, 5, true, false);    // [1,5)
    Range<int> r2(5, 10, true, false);   // [5,10)
    Range<int> r3(10, 15, true, false);  // [10,15)

    test_assert(r3.isRightOf(r1), "[10,15) is right of [1,5)");
    test_assert(!r2.isRightOf(r1), "[5,10) is not right of [1,5) (adjacent)");
}

void test_is_adjacent()
{
    std::cout << "\n=== test_is_adjacent ===" << std::endl;

    Range<int> r1(1, 5, true, false);    // [1,5)
    Range<int> r2(5, 10, true, false);   // [5,10)
    Range<int> r3(5, 10, false, false);  // (5,10)
    Range<int> r4(10, 15, true, false);  // [10,15)

    test_assert(r1.isAdjacentTo(r2), "[1,5) is adjacent to [5,10)");
    test_assert(r2.isAdjacentTo(r1), "[5,10) is adjacent to [1,5)");
    test_assert(!r1.isAdjacentTo(r3), "[1,5) is not adjacent to (5,10) (gap)");
    test_assert(r2.isAdjacentTo(r4), "[5,10) is adjacent to [10,15)");
}

// ============================================================================
// Union and Intersection Tests
// ============================================================================

void test_union()
{
    std::cout << "\n=== test_union ===" << std::endl;

    Range<int> r1(1, 5, true, false);   // [1,5)
    Range<int> r2(5, 10, true, false);  // [5,10)
    Range<int> r3(3, 7, true, false);   // [3,7)
    Range<int> r4(10, 15, true, false); // [10,15)

    // Adjacent ranges
    auto u1 = r1.rangeUnion(r2);
    test_assert(u1.has_value(), "Union of adjacent ranges exists");
    if (u1)
    {
        test_assert(u1->lower() == 1 && u1->upper() == 10, "Union [1,5) + [5,10) = [1,10)");
    }

    // Overlapping ranges
    auto u2 = r1.rangeUnion(r3);
    test_assert(u2.has_value(), "Union of overlapping ranges exists");
    if (u2)
    {
        test_assert(u2->lower() == 1 && u2->upper() == 7, "Union [1,5) + [3,7) = [1,7)");
    }

    // Disjoint ranges
    auto u3 = r1.rangeUnion(r4);
    test_assert(!u3.has_value(), "Union of disjoint ranges does not exist");
}

void test_intersection()
{
    std::cout << "\n=== test_intersection ===" << std::endl;

    Range<int> r1(1, 10, true, false);  // [1,10)
    Range<int> r2(5, 15, true, false);  // [5,15)
    Range<int> r3(10, 20, true, false); // [10,20)

    // Overlapping ranges
    auto i1 = r1.intersection(r2);
    test_assert(!i1.isEmpty(), "Intersection of overlapping ranges is not empty");
    test_assert(i1.lower() == 5 && i1.upper() == 10, "Intersection [1,10) ∩ [5,15) = [5,10)");

    // Adjacent ranges (no overlap)
    auto i2 = r1.intersection(r3);
    test_assert(i2.isEmpty(), "Intersection of adjacent ranges is empty");
}

// ============================================================================
// Range Functions Tests
// ============================================================================

void test_range_functions()
{
    std::cout << "\n=== test_range_functions ===" << std::endl;

    Range<int> r1(5, 10, true, false);  // [5,10)
    Range<int> r2(std::nullopt, 10, false, true);  // (,10]
    Range<int> r3;  // empty

    // lower/upper
    test_assert(range_lower(r1).has_value() && *range_lower(r1) == 5, "lower([5,10)) = 5");
    test_assert(range_upper(r1).has_value() && *range_upper(r1) == 10, "upper([5,10)) = 10");

    // isempty
    test_assert(!range_isempty(r1), "isempty([5,10)) = false");
    test_assert(range_isempty(r3), "isempty(empty) = true");

    // lower_inc/upper_inc
    test_assert(range_lower_inc(r1), "lower_inc([5,10)) = true");
    test_assert(!range_upper_inc(r1), "upper_inc([5,10)) = false");

    // lower_inf/upper_inf
    test_assert(!range_lower_inf(r1), "lower_inf([5,10)) = false");
    test_assert(!range_upper_inf(r1), "upper_inf([5,10)) = false");
    test_assert(range_lower_inf(r2), "lower_inf((,10]) = true");
    test_assert(!range_upper_inf(r2), "upper_inf((,10]) = false");
}

void test_range_merge()
{
    std::cout << "\n=== test_range_merge ===" << std::endl;

    Range<int> r1(1, 5, true, false);   // [1,5)
    Range<int> r2(3, 10, true, false);  // [3,10)
    Range<int> r3(15, 20, true, false); // [15,20)

    // Overlapping ranges
    auto m1 = range_merge(r1, r2);
    test_assert(!m1.isEmpty(), "Merge of overlapping ranges is not empty");
    test_assert(m1.lower() == 1 && m1.upper() == 10, "Merge [1,5) + [3,10) = [1,10)");

    // Disjoint ranges
    auto m2 = range_merge(r1, r3);
    test_assert(!m2.isEmpty(), "Merge of disjoint ranges is not empty");
    test_assert(m2.lower() == 1 && m2.upper() == 20, "Merge [1,5) + [15,20) = [1,20)");
}

// ============================================================================
// Type-Specific Tests
// ============================================================================

void test_int4range()
{
    std::cout << "\n=== test_int4range ===" << std::endl;

    Int4Range r(1, 100, true, false);
    test_assert(!r.isEmpty(), "Int4Range [1,100) is not empty");
    test_assert(r.contains(50), "Int4Range contains 50");
    test_assert(r.toString() == "[1,100)", "Int4Range string representation");
}

void test_int8range()
{
    std::cout << "\n=== test_int8range ===" << std::endl;

    Int8Range r(1000000000LL, 2000000000LL, true, false);
    test_assert(!r.isEmpty(), "Int8Range is not empty");
    test_assert(r.contains(1500000000LL), "Int8Range contains value");
}

void test_numrange()
{
    std::cout << "\n=== test_numrange ===" << std::endl;

    NumRange r(0.0, 1.0, true, true);  // [0.0, 1.0]
    test_assert(!r.isEmpty(), "NumRange [0.0,1.0] is not empty");
    test_assert(r.contains(0.5), "NumRange contains 0.5");
    test_assert(r.contains(0.0), "NumRange contains 0.0");
    test_assert(r.contains(1.0), "NumRange contains 1.0");
    test_assert(!r.contains(-0.1), "NumRange does not contain -0.1");
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===========================================" << std::endl;
    std::cout << "Range Types Unit Tests" << std::endl;
    std::cout << "Task 15: Range Types Implementation" << std::endl;
    std::cout << "===========================================" << std::endl;

    // Construction tests
    test_empty_range();
    test_basic_construction();
    test_unbounded_ranges();
    test_degenerate_ranges();
    test_invalid_ranges();

    // Contains tests
    test_contains_value();
    test_contains_range();

    // Relationship tests
    test_overlaps();
    test_is_left_of();
    test_is_right_of();
    test_is_adjacent();

    // Operations tests
    test_union();
    test_intersection();

    // Functions tests
    test_range_functions();
    test_range_merge();

    // Type-specific tests
    test_int4range();
    test_int8range();
    test_numrange();

    // Summary
    std::cout << "\n===========================================" << std::endl;
    std::cout << "Tests passed: " << g_tests_passed << std::endl;
    std::cout << "Tests failed: " << g_tests_failed << std::endl;
    std::cout << "===========================================" << std::endl;

    return g_tests_failed > 0 ? 1 : 0;
}
