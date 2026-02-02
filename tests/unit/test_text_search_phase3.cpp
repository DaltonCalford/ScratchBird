/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/ts_operations.h"
#include "scratchbird/core/ts_functions.h"
#include <cassert>
#include <iostream>
#include <cmath>

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

// Helper to check if two doubles are approximately equal
bool approx_equal(double a, double b, double epsilon = 0.0001)
{
    return std::abs(a - b) < epsilon;
}

// ============================================================================
// ts_match Operator Tests
// ============================================================================

void test_match_simple()
{
    auto doc = TSVector::fromString("'cat':1 'dog':2 'bird':3");
    auto q = TSQuery::fromString("cat");

    test_assert(doc.has_value() && q.has_value(), "Match simple - parse");
    test_assert(ts_match(*doc, *q), "Match simple - cat matches");
}

void test_match_and()
{
    auto doc = TSVector::fromString("'cat':1 'dog':2 'bird':3");
    auto q = TSQuery::fromString("cat & dog");

    test_assert(doc.has_value() && q.has_value(), "Match AND - parse");
    test_assert(ts_match(*doc, *q), "Match AND - cat & dog matches");
}

void test_match_and_fail()
{
    auto doc = TSVector::fromString("'cat':1 'dog':2");
    auto q = TSQuery::fromString("cat & bird");

    test_assert(doc.has_value() && q.has_value(), "Match AND fail - parse");
    test_assert(!ts_match(*doc, *q), "Match AND fail - cat & bird doesn't match");
}

void test_match_or()
{
    auto doc = TSVector::fromString("'cat':1 'dog':2");
    auto q = TSQuery::fromString("bird | cat");

    test_assert(doc.has_value() && q.has_value(), "Match OR - parse");
    test_assert(ts_match(*doc, *q), "Match OR - bird | cat matches (cat present)");
}

void test_match_or_fail()
{
    auto doc = TSVector::fromString("'cat':1 'dog':2");
    auto q = TSQuery::fromString("bird | fish");

    test_assert(doc.has_value() && q.has_value(), "Match OR fail - parse");
    test_assert(!ts_match(*doc, *q), "Match OR fail - bird | fish doesn't match");
}

void test_match_not()
{
    auto doc = TSVector::fromString("'cat':1 'dog':2");
    auto q = TSQuery::fromString("cat & !bird");

    test_assert(doc.has_value() && q.has_value(), "Match NOT - parse");
    test_assert(ts_match(*doc, *q), "Match NOT - cat & !bird matches");
}

void test_match_not_fail()
{
    auto doc = TSVector::fromString("'cat':1 'dog':2");
    auto q = TSQuery::fromString("!cat");

    test_assert(doc.has_value() && q.has_value(), "Match NOT fail - parse");
    test_assert(!ts_match(*doc, *q), "Match NOT fail - !cat doesn't match (cat present)");
}

void test_match_complex()
{
    auto doc = TSVector::fromString("'quick':1 'brown':2 'fox':3 'jumps':4");
    auto q = TSQuery::fromString("(quick | slow) & fox & !lazy");

    test_assert(doc.has_value() && q.has_value(), "Match complex - parse");
    test_assert(ts_match(*doc, *q), "Match complex - (quick | slow) & fox & !lazy matches");
}

void test_match_phrase()
{
    auto doc = TSVector::fromString("'quick':1 'brown':2 'fox':3");
    auto q = TSQuery::fromString("quick <1> brown");

    test_assert(doc.has_value() && q.has_value(), "Match phrase - parse");
    test_assert(ts_match(*doc, *q), "Match phrase - quick <1> brown matches (adjacent)");
}

void test_match_phrase_fail()
{
    auto doc = TSVector::fromString("'quick':1 'very':2 'brown':3");
    auto q = TSQuery::fromString("quick <1> brown");

    test_assert(doc.has_value() && q.has_value(), "Match phrase fail - parse");
    test_assert(!ts_match(*doc, *q), "Match phrase fail - quick <1> brown doesn't match (not adjacent)");
}

// ============================================================================
// ts_match_text Operator Tests
// ============================================================================

void test_match_text_simple()
{
    auto q = TSQuery::fromString("cat");

    test_assert(q.has_value(), "Match text simple - parse");
    test_assert(ts_match_text("I have a cat", *q), "Match text simple - matches");
}

void test_match_text_case_insensitive()
{
    auto q = TSQuery::fromString("cat");

    test_assert(q.has_value(), "Match text case - parse");
    test_assert(ts_match_text("I have a CAT", *q), "Match text case - CAT matches cat");
}

void test_match_text_multiple_words()
{
    auto q = TSQuery::fromString("cat & dog");

    test_assert(q.has_value(), "Match text multiple - parse");
    test_assert(ts_match_text("I have a cat and a dog", *q), "Match text multiple - matches");
}

void test_match_text_with_config()
{
    auto q = to_tsquery("english", "running");

    test_assert(q.has_value(), "Match text config - parse");
    test_assert(ts_match_text("english", "I am running", *q), "Match text config - running matches (stemmed)");
    test_assert(ts_match_text("english", "I run every day", *q), "Match text config - run matches running (stemmed)");
}

// ============================================================================
// ts_rank Function Tests
// ============================================================================

void test_rank_simple()
{
    auto doc = to_tsvector("simple", "cat dog bird");
    auto q = to_tsquery("simple", "cat");

    test_assert(doc.has_value() && q.has_value(), "Rank simple - parse");

    double rank = ts_rank(*doc, *q);
    test_assert(rank > 0.0, "Rank simple - non-zero rank");
    std::cout << "  Rank: " << rank << std::endl;
}

void test_rank_no_match()
{
    auto doc = to_tsvector("simple", "cat dog");
    auto q = to_tsquery("simple", "bird");

    test_assert(doc.has_value() && q.has_value(), "Rank no match - parse");

    double rank = ts_rank(*doc, *q);
    test_assert(approx_equal(rank, 0.0), "Rank no match - zero rank");
}

void test_rank_multiple_matches()
{
    auto doc = to_tsvector("simple", "cat dog bird");
    auto q = to_tsquery("simple", "cat & dog");

    test_assert(doc.has_value() && q.has_value(), "Rank multiple - parse");

    double rank = ts_rank(*doc, *q);
    test_assert(rank > 0.0, "Rank multiple - non-zero rank");
    std::cout << "  Rank: " << rank << std::endl;
}

void test_rank_comparison()
{
    // Document with more occurrences should rank higher
    auto doc1 = to_tsvector("simple", "cat dog");
    auto doc2 = to_tsvector("simple", "cat cat cat dog");
    auto q = to_tsquery("simple", "cat");

    test_assert(doc1.has_value() && doc2.has_value() && q.has_value(), "Rank comparison - parse");

    double rank1 = ts_rank(*doc1, *q);
    double rank2 = ts_rank(*doc2, *q);

    test_assert(rank2 > rank1, "Rank comparison - more occurrences = higher rank");
    std::cout << "  Rank1 (1 cat): " << rank1 << ", Rank2 (3 cats): " << rank2 << std::endl;
}

void test_rank_normalization()
{
    auto doc = to_tsvector("simple", "cat dog bird fish");
    auto q = to_tsquery("simple", "cat");

    test_assert(doc.has_value() && q.has_value(), "Rank normalization - parse");

    double rank_no_norm = ts_rank(*doc, *q, 0);
    double rank_norm = ts_rank(*doc, *q, 1);

    test_assert(rank_norm < rank_no_norm, "Rank normalization - normalized rank is lower");
    std::cout << "  Rank (no norm): " << rank_no_norm << ", Rank (norm): " << rank_norm << std::endl;
}

void test_rank_weighted()
{
    // Create document with different weights
    auto doc = TSVector::fromString("'cat':1A 'dog':2B 'bird':3C 'fish':4D");
    auto q1 = to_tsquery("simple", "cat");  // Weight A (1.0)
    auto q2 = to_tsquery("simple", "fish"); // Weight D (0.1)

    test_assert(doc.has_value() && q1.has_value() && q2.has_value(), "Rank weighted - parse");

    double rank_high = ts_rank(*doc, *q1); // cat with weight A
    double rank_low = ts_rank(*doc, *q2);  // fish with weight D

    test_assert(rank_high > rank_low, "Rank weighted - higher weight = higher rank");
    std::cout << "  Rank (A): " << rank_high << ", Rank (D): " << rank_low << std::endl;
}

void test_rank_custom_weights()
{
    auto doc = TSVector::fromString("'cat':1A 'dog':2B");
    auto q = to_tsquery("simple", "cat | dog");

    test_assert(doc.has_value() && q.has_value(), "Rank custom weights - parse");

    // Equal weights
    float equal_weights[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    double rank_equal = ts_rank_weighted(equal_weights, *doc, *q);

    // Default weights (D=0.1, C=0.2, B=0.4, A=1.0)
    float default_weights[4] = {0.1f, 0.2f, 0.4f, 1.0f};
    double rank_default = ts_rank_weighted(default_weights, *doc, *q);

    test_assert(rank_equal > 0.0 && rank_default > 0.0, "Rank custom weights - non-zero");
    std::cout << "  Rank (equal): " << rank_equal << ", Rank (default): " << rank_default << std::endl;
}

// ============================================================================
// ts_rank_cd Cover Density Tests
// ============================================================================

void test_rank_cd_simple()
{
    auto doc = to_tsvector("simple", "cat dog bird");
    auto q = to_tsquery("simple", "cat");

    test_assert(doc.has_value() && q.has_value(), "Rank CD simple - parse");

    double rank = ts_rank_cd(*doc, *q);
    test_assert(rank > 0.0, "Rank CD simple - non-zero rank");
    std::cout << "  Rank CD: " << rank << std::endl;
}

void test_rank_cd_no_match()
{
    auto doc = to_tsvector("simple", "cat dog");
    auto q = to_tsquery("simple", "bird");

    test_assert(doc.has_value() && q.has_value(), "Rank CD no match - parse");

    double rank = ts_rank_cd(*doc, *q);
    test_assert(approx_equal(rank, 0.0), "Rank CD no match - zero rank");
}

void test_rank_cd_tight_vs_sparse()
{
    // Tight: query terms close together
    auto doc1 = to_tsvector("simple", "the cat and dog are here");

    // Sparse: query terms far apart
    auto doc2 = to_tsvector("simple", "the cat is here and the dog is there");

    auto q = to_tsquery("simple", "cat & dog");

    test_assert(doc1.has_value() && doc2.has_value() && q.has_value(), "Rank CD tight vs sparse - parse");

    double rank_tight = ts_rank_cd(*doc1, *q);
    double rank_sparse = ts_rank_cd(*doc2, *q);

    test_assert(rank_tight > rank_sparse, "Rank CD tight vs sparse - tight is higher");
    std::cout << "  Rank CD (tight): " << rank_tight << ", Rank CD (sparse): " << rank_sparse << std::endl;
}

void test_rank_cd_normalization()
{
    auto doc = to_tsvector("simple", "cat dog bird fish");
    auto q = to_tsquery("simple", "cat");

    test_assert(doc.has_value() && q.has_value(), "Rank CD normalization - parse");

    double rank_no_norm = ts_rank_cd(*doc, *q, 0);
    double rank_norm = ts_rank_cd(*doc, *q, 1);

    test_assert(rank_norm < rank_no_norm, "Rank CD normalization - normalized rank is lower");
    std::cout << "  Rank CD (no norm): " << rank_no_norm << ", Rank CD (norm): " << rank_norm << std::endl;
}

// ============================================================================
// Integration Tests
// ============================================================================

void test_full_text_search_workflow()
{
    // Simulate a typical full-text search workflow

    // 1. Index documents
    auto doc1 = to_tsvector("english", "PostgreSQL is a powerful open source database");
    auto doc2 = to_tsvector("english", "MySQL is a popular database system");
    auto doc3 = to_tsvector("english", "MongoDB is a NoSQL database for big data");

    // 2. Create query
    auto q = to_tsquery("english", "database & powerful");

    test_assert(doc1.has_value() && doc2.has_value() && doc3.has_value() && q.has_value(),
               "Full workflow - parse documents and query");

    // 3. Match documents
    bool match1 = ts_match(*doc1, *q);
    bool match2 = ts_match(*doc2, *q);
    bool match3 = ts_match(*doc3, *q);

    test_assert(match1, "Full workflow - doc1 matches");
    test_assert(!match2, "Full workflow - doc2 doesn't match (no 'powerful')");
    test_assert(!match3, "Full workflow - doc3 doesn't match (no 'powerful')");

    // 4. Rank matching documents
    if (match1)
    {
        double rank1 = ts_rank(*doc1, *q);
        std::cout << "  Doc1 rank: " << rank1 << std::endl;
        test_assert(rank1 > 0.0, "Full workflow - doc1 has rank");
    }
}

void test_search_with_stemming()
{
    // Test that stemming improves recall

    auto doc = to_tsvector("english", "I am running with cats every day");
    auto q1 = to_tsquery("english", "run & cat");

    test_assert(doc.has_value() && q1.has_value(), "Search stemming - parse");

    // Should match because "running" -> "run", "cats" -> "cat"
    test_assert(ts_match(*doc, *q1), "Search stemming - stemmed terms match");

    // Rank should be positive
    double rank = ts_rank(*doc, *q1);
    test_assert(rank > 0.0, "Search stemming - positive rank");
    std::cout << "  Stemmed match rank: " << rank << std::endl;
}

void test_ranking_order()
{
    // Test that ranking correctly orders search results

    auto doc1 = to_tsvector("english", "cat");
    auto doc2 = to_tsvector("english", "cat cat cat");
    auto doc3 = to_tsvector("english", "cat dog bird");

    auto q = to_tsquery("english", "cat");

    test_assert(doc1.has_value() && doc2.has_value() && doc3.has_value() && q.has_value(),
               "Ranking order - parse");

    double rank1 = ts_rank(*doc1, *q);
    double rank2 = ts_rank(*doc2, *q);
    double rank3 = ts_rank(*doc3, *q);

    std::cout << "  Rank1 (1 cat): " << rank1 << std::endl;
    std::cout << "  Rank2 (3 cats): " << rank2 << std::endl;
    std::cout << "  Rank3 (1 cat + other words): " << rank3 << std::endl;

    // Doc2 has most occurrences
    test_assert(rank2 > rank1, "Ranking order - multiple occurrences rank higher");
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "=== Testing ts_match Operator ===\n" << std::endl;
    test_match_simple();
    test_match_and();
    test_match_and_fail();
    test_match_or();
    test_match_or_fail();
    test_match_not();
    test_match_not_fail();
    test_match_complex();
    test_match_phrase();
    test_match_phrase_fail();

    std::cout << "\n=== Testing ts_match_text Operator ===\n" << std::endl;
    test_match_text_simple();
    test_match_text_case_insensitive();
    test_match_text_multiple_words();
    test_match_text_with_config();

    std::cout << "\n=== Testing ts_rank Function ===\n" << std::endl;
    test_rank_simple();
    test_rank_no_match();
    test_rank_multiple_matches();
    test_rank_comparison();
    test_rank_normalization();
    test_rank_weighted();
    test_rank_custom_weights();

    std::cout << "\n=== Testing ts_rank_cd Function ===\n" << std::endl;
    test_rank_cd_simple();
    test_rank_cd_no_match();
    test_rank_cd_tight_vs_sparse();
    test_rank_cd_normalization();

    std::cout << "\n=== Integration Tests ===\n" << std::endl;
    test_full_text_search_workflow();
    test_search_with_stemming();
    test_ranking_order();

    std::cout << "\n=== All Phase 3 Tests Passed! ===\n" << std::endl;
    return;
}
