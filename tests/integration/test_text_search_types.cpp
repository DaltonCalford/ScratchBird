/**
 * Integration tests for full-text search type system integration
 * Task 14 Phase 5: SQL Integration - Type System Testing
 *
 * Tests the TypedValue integration for TSVector and TSQuery:
 * - makeTSVector/getTSVector
 * - makeTSQuery/getTSQuery
 * - toString for both types
 */

#include "scratchbird/core/types.h"
#include "scratchbird/core/tsvector.h"
#include "scratchbird/core/tsquery.h"
#include "scratchbird/core/ts_functions.h"
#include "scratchbird/core/ts_operations.h"
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
// Test Functions
// ============================================================================

void test_tsvector_value_creation()
{
    std::cout << "\n=== test_tsvector_value_creation ===" << std::endl;

    auto vec = TSVector::fromString("'cat':1 'dog':2");
    test_assert(vec.has_value(), "TSVector parses successfully");

    if (vec.has_value())
    {
        // Test value creation with copy
        TypedValue val = TypedValue::makeTSVector(*vec);
        test_assert(val.type() == DataType::TSVECTOR, "Value type is TSVECTOR");

        // Test retrieval
        auto retrieved = val.getTSVector();
        test_assert(retrieved != nullptr, "getTSVector returns non-null");
        test_assert(retrieved->numLexemes() == 2, "Retrieved vector has 2 lexemes");
        test_assert(retrieved->toString() == "'cat':1 'dog':2", "Retrieved vector matches");
    }
}

void test_tsvector_value_shared_ptr()
{
    std::cout << "\n=== test_tsvector_value_shared_ptr ===" << std::endl;

    auto vec_ptr = std::make_shared<TSVector>();
    auto vec = TSVector::fromString("'bird':1");
    if (vec.has_value())
    {
        *vec_ptr = std::move(*vec);
    }

    TypedValue val = TypedValue::makeTSVector(vec_ptr);
    test_assert(val.type() == DataType::TSVECTOR, "Value type is TSVECTOR");

    auto retrieved = val.getTSVector();
    test_assert(retrieved != nullptr, "getTSVector returns non-null");
    test_assert(retrieved->numLexemes() == 1, "Retrieved vector has 1 lexeme");
}

void test_tsvector_value_tostring()
{
    std::cout << "\n=== test_tsvector_value_tostring ===" << std::endl;

    auto vec = TSVector::fromString("'hello':1 'world':2");
    if (vec.has_value())
    {
        TypedValue val = TypedValue::makeTSVector(*vec);
        std::string str = val.toString();

        test_assert(str == "'hello':1 'world':2", "TypedValue::toString() works for TSVECTOR");
    }
}

void test_tsquery_value_creation()
{
    std::cout << "\n=== test_tsquery_value_creation ===" << std::endl;

    auto query = TSQuery::fromString("cat & dog");
    test_assert(query.has_value(), "TSQuery parses successfully");

    if (query.has_value())
    {
        // Test value creation (uses binary copy since TSQuery is non-copyable)
        TypedValue val = TypedValue::makeTSQuery(*query);
        test_assert(val.type() == DataType::TSQUERY, "Value type is TSQUERY");

        // Test retrieval
        auto retrieved = val.getTSQuery();
        test_assert(retrieved != nullptr, "getTSQuery returns non-null");
        test_assert(retrieved->toString() == "('cat' & 'dog')", "Retrieved query matches");
    }
}

void test_tsquery_value_shared_ptr()
{
    std::cout << "\n=== test_tsquery_value_shared_ptr ===" << std::endl;

    auto query = TSQuery::fromString("cat | dog");
    if (query.has_value())
    {
        auto query_ptr = std::make_shared<TSQuery>(std::move(*query));

        TypedValue val = TypedValue::makeTSQuery(query_ptr);
        test_assert(val.type() == DataType::TSQUERY, "Value type is TSQUERY");

        auto retrieved = val.getTSQuery();
        test_assert(retrieved != nullptr, "getTSQuery returns non-null");
        test_assert(retrieved->toString() == "('cat' | 'dog')", "Retrieved query matches");
    }
}

void test_tsquery_value_tostring()
{
    std::cout << "\n=== test_tsquery_value_tostring ===" << std::endl;

    auto query = TSQuery::fromString("hello & world");
    if (query.has_value())
    {
        TypedValue val = TypedValue::makeTSQuery(*query);
        std::string str = val.toString();

        test_assert(str == "('hello' & 'world')", "TypedValue::toString() works for TSQUERY");
    }
}

void test_full_workflow()
{
    std::cout << "\n=== test_full_workflow ===" << std::endl;

    // Create tsvector using to_tsvector function
    auto vec_opt = to_tsvector("english", "The quick brown cat jumped");
    test_assert(vec_opt.has_value(), "to_tsvector returns value");

    if (vec_opt.has_value())
    {
        TypedValue vec_val = TypedValue::makeTSVector(*vec_opt);

        // Create tsquery using to_tsquery function
        auto query_opt = to_tsquery("english", "cat & jump");
        test_assert(query_opt.has_value(), "to_tsquery returns value");

        if (query_opt.has_value())
        {
            TypedValue query_val = TypedValue::makeTSQuery(*query_opt);

            // Test match operation
            auto vec = vec_val.getTSVector();
            auto query = query_val.getTSQuery();

            bool match = ts_match(*vec, *query);
            test_assert(match == true, "Match operation returns true for matching document");

            // Test rank operation
            double rank = ts_rank(*vec, *query);
            test_assert(rank > 0.0, "Rank operation returns positive value");
        }
    }
}

void test_text_match_workflow()
{
    std::cout << "\n=== test_text_match_workflow ===" << std::endl;

    // Test text @@ tsquery pattern
    auto query_opt = to_tsquery("simple", "cat & dog");
    test_assert(query_opt.has_value(), "Query created");

    if (query_opt.has_value())
    {
        TypedValue query_val = TypedValue::makeTSQuery(*query_opt);
        auto query = query_val.getTSQuery();

        // Test match with text
        bool match = ts_match_text("I have a cat and a dog", *query);
        test_assert(match == true, "Text match returns true");

        bool no_match = ts_match_text("I have a bird", *query);
        test_assert(no_match == false, "Text non-match returns false");
    }
}

void test_multiple_value_operations()
{
    std::cout << "\n=== test_multiple_value_operations ===" << std::endl;

    // Create multiple values
    auto vec1_opt = to_tsvector("simple", "cat dog");
    auto vec2_opt = to_tsvector("simple", "bird fish");
    auto query_opt = to_tsquery("simple", "cat");

    test_assert(vec1_opt.has_value(), "Vec1 created");
    test_assert(vec2_opt.has_value(), "Vec2 created");
    test_assert(query_opt.has_value(), "Query created");

    if (vec1_opt && vec2_opt && query_opt)
    {
        TypedValue val1 = TypedValue::makeTSVector(*vec1_opt);
        TypedValue val2 = TypedValue::makeTSVector(*vec2_opt);
        TypedValue query_val = TypedValue::makeTSQuery(*query_opt);

        test_assert(val1.type() == DataType::TSVECTOR, "Val1 type correct");
        test_assert(val2.type() == DataType::TSVECTOR, "Val2 type correct");
        test_assert(query_val.type() == DataType::TSQUERY, "Query val type correct");

        // Test matches
        auto vec1 = val1.getTSVector();
        auto vec2 = val2.getTSVector();
        auto query = query_val.getTSQuery();

        bool match1 = ts_match(*vec1, *query);
        bool match2 = ts_match(*vec2, *query);

        test_assert(match1 == true, "Vec1 matches (contains cat)");
        test_assert(match2 == false, "Vec2 doesn't match (no cat)");
    }
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===========================================" << std::endl;
    std::cout << "Text Search Type System Integration Tests" << std::endl;
    std::cout << "Task 14 Phase 5: SQL Integration" << std::endl;
    std::cout << "===========================================" << std::endl;

    // Run tests
    test_tsvector_value_creation();
    test_tsvector_value_shared_ptr();
    test_tsvector_value_tostring();
    test_tsquery_value_creation();
    test_tsquery_value_shared_ptr();
    test_tsquery_value_tostring();
    test_full_workflow();
    test_text_match_workflow();
    test_multiple_value_operations();

    // Summary
    std::cout << "\n===========================================" << std::endl;
    std::cout << "Tests passed: " << g_tests_passed << std::endl;
    std::cout << "Tests failed: " << g_tests_failed << std::endl;
    std::cout << "===========================================" << std::endl;

    return g_tests_failed > 0 ? 1 : 0;
}
