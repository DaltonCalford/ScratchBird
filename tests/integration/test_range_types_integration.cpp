/**
 * Integration tests for Range Types with TypedValue
 * Task 15: Range Types Implementation
 *
 * Tests TypedValue integration:
 * - Factory methods (makeInt4Range, makeInt8Range, makeNumRange)
 * - Accessor methods (getInt4Range, getInt8Range, getNumRange)
 * - toString() support
 * - Full workflow: create → store → retrieve → use
 */

#include "scratchbird/core/types.h"
#include "scratchbird/core/range.h"
#include <gtest/gtest.h>

using namespace scratchbird::core;

// ============================================================================
// TypedValue Factory Tests
// ============================================================================

TEST(RangeTypesIntegration, Int4RangeFactory)
{
    Int4Range r(1, 100, true, false);  // [1,100)
    TypedValue v = TypedValue::makeInt4Range(r);

    EXPECT_EQ(v.type(), DataType::INT4RANGE);
    EXPECT_FALSE(v.isNull());
}

TEST(RangeTypesIntegration, Int8RangeFactory)
{
    Int8Range r(1000000000LL, 2000000000LL, true, false);
    TypedValue v = TypedValue::makeInt8Range(r);

    EXPECT_EQ(v.type(), DataType::INT8RANGE);
    EXPECT_FALSE(v.isNull());
}

TEST(RangeTypesIntegration, NumRangeFactory)
{
    NumRange r(0.0, 1.0, true, true);  // [0.0, 1.0]
    TypedValue v = TypedValue::makeNumRange(r);

    EXPECT_EQ(v.type(), DataType::NUMRANGE);
    EXPECT_FALSE(v.isNull());
}

// ============================================================================
// TypedValue Accessor Tests
// ============================================================================

TEST(RangeTypesIntegration, Int4RangeAccessor)
{
    Int4Range r(1, 10, true, false);  // [1,10)
    TypedValue v = TypedValue::makeInt4Range(r);

    Int4Range retrieved = v.getInt4Range();
    EXPECT_FALSE(retrieved.isEmpty());
    EXPECT_TRUE(retrieved.contains(5));
    EXPECT_FALSE(retrieved.contains(10));
}

TEST(RangeTypesIntegration, Int8RangeAccessor)
{
    Int8Range r(100LL, 200LL, true, false);
    TypedValue v = TypedValue::makeInt8Range(r);

    Int8Range retrieved = v.getInt8Range();
    EXPECT_TRUE(retrieved.contains(150LL));
    EXPECT_FALSE(retrieved.contains(200LL));
}

TEST(RangeTypesIntegration, NumRangeAccessor)
{
    NumRange r(0.0, 1.0, true, true);  // [0.0, 1.0]
    TypedValue v = TypedValue::makeNumRange(r);

    NumRange retrieved = v.getNumRange();
    EXPECT_TRUE(retrieved.contains(0.5));
    EXPECT_TRUE(retrieved.contains(0.0));
    EXPECT_TRUE(retrieved.contains(1.0));
    EXPECT_FALSE(retrieved.contains(1.1));
}

// ============================================================================
// ToString Tests
// ============================================================================

TEST(RangeTypesIntegration, Int4RangeToString)
{
    Int4Range r(1, 100, true, false);  // [1,100)
    TypedValue v = TypedValue::makeInt4Range(r);

    std::string str = v.toString();
    EXPECT_EQ(str, "[1,100)");
}

TEST(RangeTypesIntegration, Int8RangeToString)
{
    Int8Range r(1000LL, 2000LL, false, true);  // (1000,2000]
    TypedValue v = TypedValue::makeInt8Range(r);

    std::string str = v.toString();
    EXPECT_EQ(str, "(1000,2000]");
}

TEST(RangeTypesIntegration, NumRangeToString)
{
    NumRange r(0.0, 1.0, true, true);  // [0.0,1.0]
    TypedValue v = TypedValue::makeNumRange(r);

    std::string str = v.toString();
    EXPECT_EQ(str, "[0,1]");
}

TEST(RangeTypesIntegration, EmptyRangeToString)
{
    Int4Range r;  // Empty range
    TypedValue v = TypedValue::makeInt4Range(r);

    std::string str = v.toString();
    EXPECT_EQ(str, "empty");
}

// ============================================================================
// Full Workflow Tests
// ============================================================================

TEST(RangeTypesIntegration, Int4RangeWorkflow)
{
    // Create range
    Int4Range r1(1, 10, true, false);  // [1,10)
    Int4Range r2(5, 15, true, false);  // [5,15)

    // Store in TypedValue
    TypedValue v1 = TypedValue::makeInt4Range(r1);
    TypedValue v2 = TypedValue::makeInt4Range(r2);

    // Retrieve
    Int4Range retrieved1 = v1.getInt4Range();
    Int4Range retrieved2 = v2.getInt4Range();

    // Use range operations
    EXPECT_TRUE(retrieved1.overlaps(retrieved2));

    auto intersection = retrieved1.intersection(retrieved2);
    EXPECT_FALSE(intersection.isEmpty());
    EXPECT_EQ(intersection.lower(), std::make_optional(5));
    EXPECT_EQ(intersection.upper(), std::make_optional(10));
}

TEST(RangeTypesIntegration, NumRangeOperations)
{
    // Create overlapping ranges
    NumRange r1(0.0, 5.0, true, false);  // [0.0,5.0)
    NumRange r2(3.0, 8.0, true, false);  // [3.0,8.0)

    TypedValue v1 = TypedValue::makeNumRange(r1);
    TypedValue v2 = TypedValue::makeNumRange(r2);

    // Retrieve and check intersection
    NumRange range1 = v1.getNumRange();
    NumRange range2 = v2.getNumRange();

    auto intersection = range1.intersection(range2);
    EXPECT_FALSE(intersection.isEmpty());
    EXPECT_TRUE(intersection.contains(4.0));
    EXPECT_FALSE(intersection.contains(2.0));
    EXPECT_FALSE(intersection.contains(6.0));
}

TEST(RangeTypesIntegration, RangeContainment)
{
    Int4Range outer(1, 100, true, false);   // [1,100)
    Int4Range inner(10, 20, true, false);   // [10,20)

    TypedValue v_outer = TypedValue::makeInt4Range(outer);
    TypedValue v_inner = TypedValue::makeInt4Range(inner);

    Int4Range r_outer = v_outer.getInt4Range();
    Int4Range r_inner = v_inner.getInt4Range();

    EXPECT_TRUE(r_outer.contains(r_inner));
    EXPECT_FALSE(r_inner.contains(r_outer));
}
