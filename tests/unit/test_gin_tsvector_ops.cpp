#include "scratchbird/core/gin_tsvector_ops.h"
#include "scratchbird/core/ts_functions.h"
#include <cassert>
#include <iostream>
#include <algorithm>

using namespace scratchbird::core;

// Test helper
void test_assert(bool condition, const char* test_name)
{
    if (condition)
    {
        std::cout << "[PASS] " << test_name << std::endl;
    }
    else
    {
        std::cerr << "[FAIL] " << test_name << std::endl;
        exit(1);
    }
}

// Helper to convert byte vector to string
std::string bytesToString(const std::vector<uint8_t>& bytes)
{
    return std::string(bytes.begin(), bytes.end());
}

// Helper to check if a key exists in keys vector
bool containsKey(const std::vector<std::vector<uint8_t>>& keys, const std::string& key)
{
    for (const auto& k : keys)
    {
        if (bytesToString(k) == key)
            return true;
    }
    return false;
}

// ============================================================================
// extractKeys Tests
// ============================================================================

void test_extract_keys_simple()
{
    auto vec = TSVector::fromString("'cat':1 'dog':2 'bird':3");
    test_assert(vec.has_value(), "Extract keys simple - parse");

    auto keys = GINTSVectorOps::extractKeys(*vec);

    test_assert(keys.size() == 3, "Extract keys simple - 3 keys");
    test_assert(containsKey(keys, "cat"), "Extract keys simple - contains 'cat'");
    test_assert(containsKey(keys, "dog"), "Extract keys simple - contains 'dog'");
    test_assert(containsKey(keys, "bird"), "Extract keys simple - contains 'bird'");
}

void test_extract_keys_with_positions()
{
    // Positions and weights should be discarded
    auto vec = TSVector::fromString("'cat':1A,3B 'dog':2C");
    test_assert(vec.has_value(), "Extract keys with positions - parse");

    auto keys = GINTSVectorOps::extractKeys(*vec);

    test_assert(keys.size() == 2, "Extract keys with positions - 2 keys");
    test_assert(containsKey(keys, "cat"), "Extract keys with positions - contains 'cat'");
    test_assert(containsKey(keys, "dog"), "Extract keys with positions - contains 'dog'");
}

void test_extract_keys_empty()
{
    TSVector vec;  // Empty tsvector
    auto keys = GINTSVectorOps::extractKeys(vec);

    test_assert(keys.empty(), "Extract keys empty - no keys");
}

void test_extract_keys_binary()
{
    auto vec = TSVector::fromString("'quick':1 'brown':2 'fox':3");
    test_assert(vec.has_value(), "Extract keys binary - parse");

    // Serialize to binary
    auto binary = vec->toBinary();

    // Extract keys from binary
    auto keys = GINTSVectorOps::extractKeys(binary.data(), binary.size());

    test_assert(keys.size() == 3, "Extract keys binary - 3 keys");
    test_assert(containsKey(keys, "quick"), "Extract keys binary - contains 'quick'");
    test_assert(containsKey(keys, "brown"), "Extract keys binary - contains 'brown'");
    test_assert(containsKey(keys, "fox"), "Extract keys binary - contains 'fox'");
}

// ============================================================================
// extractQueryKeys Tests
// ============================================================================

void test_extract_query_keys_simple()
{
    auto q = TSQuery::fromString("cat");
    test_assert(q.has_value(), "Extract query keys simple - parse");

    auto keys = GINTSVectorOps::extractQueryKeys(*q);

    test_assert(keys.size() == 1, "Extract query keys simple - 1 key");
    test_assert(containsKey(keys, "cat"), "Extract query keys simple - contains 'cat'");
}

void test_extract_query_keys_and()
{
    auto q = TSQuery::fromString("cat & dog");
    test_assert(q.has_value(), "Extract query keys AND - parse");

    auto keys = GINTSVectorOps::extractQueryKeys(*q);

    test_assert(keys.size() == 2, "Extract query keys AND - 2 keys");
    test_assert(containsKey(keys, "cat"), "Extract query keys AND - contains 'cat'");
    test_assert(containsKey(keys, "dog"), "Extract query keys AND - contains 'dog'");
}

void test_extract_query_keys_or()
{
    auto q = TSQuery::fromString("cat | dog | bird");
    test_assert(q.has_value(), "Extract query keys OR - parse");

    auto keys = GINTSVectorOps::extractQueryKeys(*q);

    test_assert(keys.size() == 3, "Extract query keys OR - 3 keys");
    test_assert(containsKey(keys, "cat"), "Extract query keys OR - contains 'cat'");
    test_assert(containsKey(keys, "dog"), "Extract query keys OR - contains 'dog'");
    test_assert(containsKey(keys, "bird"), "Extract query keys OR - contains 'bird'");
}

void test_extract_query_keys_not()
{
    auto q = TSQuery::fromString("cat & !dog");
    test_assert(q.has_value(), "Extract query keys NOT - parse");

    auto keys = GINTSVectorOps::extractQueryKeys(*q);

    test_assert(keys.size() == 2, "Extract query keys NOT - 2 keys (cat and dog)");
    test_assert(containsKey(keys, "cat"), "Extract query keys NOT - contains 'cat'");
    test_assert(containsKey(keys, "dog"), "Extract query keys NOT - contains 'dog'");
}

void test_extract_query_keys_complex()
{
    auto q = TSQuery::fromString("(cat & dog) | (bird & fish)");
    test_assert(q.has_value(), "Extract query keys complex - parse");

    auto keys = GINTSVectorOps::extractQueryKeys(*q);

    test_assert(keys.size() == 4, "Extract query keys complex - 4 keys");
    test_assert(containsKey(keys, "cat"), "Extract query keys complex - contains 'cat'");
    test_assert(containsKey(keys, "dog"), "Extract query keys complex - contains 'dog'");
    test_assert(containsKey(keys, "bird"), "Extract query keys complex - contains 'bird'");
    test_assert(containsKey(keys, "fish"), "Extract query keys complex - contains 'fish'");
}

void test_extract_query_keys_phrase()
{
    auto q = TSQuery::fromString("quick <1> brown");
    test_assert(q.has_value(), "Extract query keys phrase - parse");

    auto keys = GINTSVectorOps::extractQueryKeys(*q);

    test_assert(keys.size() == 2, "Extract query keys phrase - 2 keys");
    test_assert(containsKey(keys, "quick"), "Extract query keys phrase - contains 'quick'");
    test_assert(containsKey(keys, "brown"), "Extract query keys phrase - contains 'brown'");
}

// ============================================================================
// consistent Tests
// ============================================================================

void test_consistent_simple_match()
{
    auto vec = TSVector::fromString("'cat':1 'dog':2 'bird':3");
    auto q = TSQuery::fromString("cat");

    test_assert(vec.has_value() && q.has_value(), "Consistent simple match - parse");
    test_assert(GINTSVectorOps::consistent(*vec, *q), "Consistent simple match - matches");
}

void test_consistent_simple_no_match()
{
    auto vec = TSVector::fromString("'cat':1 'dog':2");
    auto q = TSQuery::fromString("bird");

    test_assert(vec.has_value() && q.has_value(), "Consistent simple no match - parse");
    test_assert(!GINTSVectorOps::consistent(*vec, *q), "Consistent simple no match - doesn't match");
}

void test_consistent_and_match()
{
    auto vec = TSVector::fromString("'cat':1 'dog':2 'bird':3");
    auto q = TSQuery::fromString("cat & dog");

    test_assert(vec.has_value() && q.has_value(), "Consistent AND match - parse");
    test_assert(GINTSVectorOps::consistent(*vec, *q), "Consistent AND match - matches");
}

void test_consistent_and_no_match()
{
    auto vec = TSVector::fromString("'cat':1 'dog':2");
    auto q = TSQuery::fromString("cat & bird");

    test_assert(vec.has_value() && q.has_value(), "Consistent AND no match - parse");
    test_assert(!GINTSVectorOps::consistent(*vec, *q), "Consistent AND no match - doesn't match");
}

void test_consistent_or_match()
{
    auto vec = TSVector::fromString("'cat':1 'dog':2");
    auto q = TSQuery::fromString("bird | cat");

    test_assert(vec.has_value() && q.has_value(), "Consistent OR match - parse");
    test_assert(GINTSVectorOps::consistent(*vec, *q), "Consistent OR match - matches");
}

void test_consistent_or_no_match()
{
    auto vec = TSVector::fromString("'cat':1 'dog':2");
    auto q = TSQuery::fromString("bird | fish");

    test_assert(vec.has_value() && q.has_value(), "Consistent OR no match - parse");
    test_assert(!GINTSVectorOps::consistent(*vec, *q), "Consistent OR no match - doesn't match");
}

void test_consistent_not_match()
{
    auto vec = TSVector::fromString("'cat':1 'dog':2");
    auto q = TSQuery::fromString("cat & !bird");

    test_assert(vec.has_value() && q.has_value(), "Consistent NOT match - parse");
    test_assert(GINTSVectorOps::consistent(*vec, *q), "Consistent NOT match - matches");
}

void test_consistent_not_no_match()
{
    auto vec = TSVector::fromString("'cat':1 'bird':2");
    auto q = TSQuery::fromString("cat & !bird");

    test_assert(vec.has_value() && q.has_value(), "Consistent NOT no match - parse");
    test_assert(!GINTSVectorOps::consistent(*vec, *q), "Consistent NOT no match - doesn't match");
}

void test_consistent_phrase_match()
{
    auto vec = TSVector::fromString("'quick':1 'brown':2 'fox':3");
    auto q = TSQuery::fromString("quick <1> brown");

    test_assert(vec.has_value() && q.has_value(), "Consistent phrase match - parse");
    test_assert(GINTSVectorOps::consistent(*vec, *q), "Consistent phrase match - matches");
}

void test_consistent_phrase_no_match()
{
    auto vec = TSVector::fromString("'quick':1 'very':2 'brown':3");
    auto q = TSQuery::fromString("quick <1> brown");

    test_assert(vec.has_value() && q.has_value(), "Consistent phrase no match - parse");
    test_assert(!GINTSVectorOps::consistent(*vec, *q), "Consistent phrase no match - doesn't match");
}

// ============================================================================
// analyzeQuery Tests
// ============================================================================

void test_analyze_query_single()
{
    auto q = TSQuery::fromString("cat");
    test_assert(q.has_value(), "Analyze query single - parse");

    auto strategy = GINTSVectorOps::analyzeQuery(*q);
    test_assert(strategy == GINTSVectorOps::QueryStrategy::NEED_ALL,
               "Analyze query single - NEED_ALL");
}

void test_analyze_query_and()
{
    auto q = TSQuery::fromString("cat & dog");
    test_assert(q.has_value(), "Analyze query AND - parse");

    auto strategy = GINTSVectorOps::analyzeQuery(*q);
    test_assert(strategy == GINTSVectorOps::QueryStrategy::NEED_ALL,
               "Analyze query AND - NEED_ALL");
}

void test_analyze_query_or()
{
    auto q = TSQuery::fromString("cat | dog");
    test_assert(q.has_value(), "Analyze query OR - parse");

    auto strategy = GINTSVectorOps::analyzeQuery(*q);
    test_assert(strategy == GINTSVectorOps::QueryStrategy::NEED_ANY,
               "Analyze query OR - NEED_ANY");
}

void test_analyze_query_not()
{
    auto q = TSQuery::fromString("!cat");
    test_assert(q.has_value(), "Analyze query NOT - parse");

    auto strategy = GINTSVectorOps::analyzeQuery(*q);
    test_assert(strategy == GINTSVectorOps::QueryStrategy::NEED_RECHECK,
               "Analyze query NOT - NEED_RECHECK");
}

void test_analyze_query_phrase()
{
    auto q = TSQuery::fromString("quick <1> brown");
    test_assert(q.has_value(), "Analyze query phrase - parse");

    auto strategy = GINTSVectorOps::analyzeQuery(*q);
    test_assert(strategy == GINTSVectorOps::QueryStrategy::NEED_RECHECK,
               "Analyze query phrase - NEED_RECHECK");
}

void test_analyze_query_mixed()
{
    auto q = TSQuery::fromString("(cat & dog) | bird");
    test_assert(q.has_value(), "Analyze query mixed - parse");

    auto strategy = GINTSVectorOps::analyzeQuery(*q);
    // OR at top level means NEED_ANY (can use GIN OR strategy)
    test_assert(strategy == GINTSVectorOps::QueryStrategy::NEED_ANY,
               "Analyze query mixed - NEED_ANY (OR at top)");
}

// ============================================================================
// estimateSelectivity Tests
// ============================================================================

void test_estimate_selectivity_simple()
{
    auto q = TSQuery::fromString("cat");
    test_assert(q.has_value(), "Estimate selectivity simple - parse");

    GinIndex::Statistics stats;
    stats.num_keys = 1000;
    stats.num_tuples = 10000;
    stats.avg_tids_per_key = 10.0;  // Each key appears in 10 documents on average

    double selectivity = GINTSVectorOps::estimateSelectivity(*q, stats);

    std::cout << "  Selectivity (single term): " << selectivity << std::endl;
    test_assert(selectivity > 0.0 && selectivity <= 1.0,
               "Estimate selectivity simple - in range [0,1]");
}

void test_estimate_selectivity_and()
{
    auto q = TSQuery::fromString("cat & dog & bird");
    test_assert(q.has_value(), "Estimate selectivity AND - parse");

    GinIndex::Statistics stats;
    stats.num_keys = 1000;
    stats.num_tuples = 10000;
    stats.avg_tids_per_key = 10.0;

    double selectivity = GINTSVectorOps::estimateSelectivity(*q, stats);

    std::cout << "  Selectivity (AND query): " << selectivity << std::endl;
    test_assert(selectivity > 0.0 && selectivity <= 1.0,
               "Estimate selectivity AND - in range [0,1]");

    // AND should be more selective (lower) than single term
    auto q_single = TSQuery::fromString("cat");
    double sel_single = GINTSVectorOps::estimateSelectivity(*q_single, stats);
    test_assert(selectivity < sel_single,
               "Estimate selectivity AND - more selective than single term");
}

void test_estimate_selectivity_or()
{
    auto q = TSQuery::fromString("cat | dog | bird");
    test_assert(q.has_value(), "Estimate selectivity OR - parse");

    GinIndex::Statistics stats;
    stats.num_keys = 100;
    stats.num_tuples = 1000;
    stats.avg_tids_per_key = 100.0;  // Higher selectivity for each key

    double selectivity = GINTSVectorOps::estimateSelectivity(*q, stats);

    std::cout << "  Selectivity (OR query): " << selectivity << std::endl;
    test_assert(selectivity > 0.0 && selectivity <= 1.0,
               "Estimate selectivity OR - in range [0,1]");

    // OR should be less selective (higher) than single term
    auto q_single = TSQuery::fromString("cat");
    double sel_single = GINTSVectorOps::estimateSelectivity(*q_single, stats);
    std::cout << "  Selectivity (single): " << sel_single << std::endl;
    test_assert(selectivity > sel_single,
               "Estimate selectivity OR - less selective than single term");
}

// ============================================================================
// Integration Tests
// ============================================================================

void test_round_trip_extraction()
{
    // Test full round trip: TSVector -> keys -> query -> consistent check

    auto vec = to_tsvector("english", "The quick brown fox jumps over the lazy dog");
    test_assert(vec.has_value(), "Round trip - create vector");

    // Extract keys
    auto keys = GINTSVectorOps::extractKeys(*vec);
    test_assert(!keys.empty(), "Round trip - extract keys");

    // Create query
    auto q = to_tsquery("english", "quick & fox");
    test_assert(q.has_value(), "Round trip - create query");

    // Extract query keys
    auto query_keys = GINTSVectorOps::extractQueryKeys(*q);
    test_assert(query_keys.size() == 2, "Round trip - extract query keys");

    // Check consistent
    bool matches = GINTSVectorOps::consistent(*vec, *q);
    test_assert(matches, "Round trip - consistent check matches");
}

void test_gin_workflow()
{
    // Simulate typical GIN workflow
    std::cout << "  Simulating GIN index workflow..." << std::endl;

    // 1. Index documents
    auto doc1 = to_tsvector("english", "PostgreSQL is a powerful database");
    auto doc2 = to_tsvector("english", "MySQL is a fast database");
    auto doc3 = to_tsvector("english", "MongoDB is a NoSQL solution");

    // 2. Extract keys for indexing
    auto keys1 = GINTSVectorOps::extractKeys(*doc1);
    auto keys2 = GINTSVectorOps::extractKeys(*doc2);
    auto keys3 = GINTSVectorOps::extractKeys(*doc3);

    std::cout << "    Doc1 keys: " << keys1.size() << std::endl;
    std::cout << "    Doc2 keys: " << keys2.size() << std::endl;
    std::cout << "    Doc3 keys: " << keys3.size() << std::endl;

    // 3. Create query
    auto q = to_tsquery("english", "database & powerful");

    // 4. Extract query keys (for GIN lookup)
    auto query_keys = GINTSVectorOps::extractQueryKeys(*q);
    std::cout << "    Query keys: " << query_keys.size() << std::endl;

    // 5. GIN would return candidates containing query keys
    // Then we do consistent check on candidates
    bool match1 = GINTSVectorOps::consistent(*doc1, *q);  // Should match
    bool match2 = GINTSVectorOps::consistent(*doc2, *q);  // Should not match (no "powerful")
    bool match3 = GINTSVectorOps::consistent(*doc3, *q);  // Should not match (no "database")

    test_assert(match1, "GIN workflow - doc1 matches");
    test_assert(!match2, "GIN workflow - doc2 doesn't match");
    test_assert(!match3, "GIN workflow - doc3 doesn't match");
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "=== Testing extractKeys ===\n" << std::endl;
    test_extract_keys_simple();
    test_extract_keys_with_positions();
    test_extract_keys_empty();
    test_extract_keys_binary();

    std::cout << "\n=== Testing extractQueryKeys ===\n" << std::endl;
    test_extract_query_keys_simple();
    test_extract_query_keys_and();
    test_extract_query_keys_or();
    test_extract_query_keys_not();
    test_extract_query_keys_complex();
    test_extract_query_keys_phrase();

    std::cout << "\n=== Testing consistent ===\n" << std::endl;
    test_consistent_simple_match();
    test_consistent_simple_no_match();
    test_consistent_and_match();
    test_consistent_and_no_match();
    test_consistent_or_match();
    test_consistent_or_no_match();
    test_consistent_not_match();
    test_consistent_not_no_match();
    test_consistent_phrase_match();
    test_consistent_phrase_no_match();

    std::cout << "\n=== Testing analyzeQuery ===\n" << std::endl;
    test_analyze_query_single();
    test_analyze_query_and();
    test_analyze_query_or();
    test_analyze_query_not();
    test_analyze_query_phrase();
    test_analyze_query_mixed();

    std::cout << "\n=== Testing estimateSelectivity ===\n" << std::endl;
    test_estimate_selectivity_simple();
    test_estimate_selectivity_and();
    test_estimate_selectivity_or();

    std::cout << "\n=== Integration Tests ===\n" << std::endl;
    test_round_trip_extraction();
    test_gin_workflow();

    std::cout << "\n=== All GIN TSVector Ops Tests Passed! ===\n" << std::endl;
    return 0;
}
