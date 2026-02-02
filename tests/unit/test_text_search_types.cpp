/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/tsvector.h"
#include "scratchbird/core/tsquery.h"
#include "gtest/gtest.h"
#include <iostream>

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

// ============================================================================
// TSVector Tests
// ============================================================================

void test_lexeme_construction()
{
    Lexeme lex("cat", {1, 3, 5}, {'A', 'B', 'C'});

    test_assert(lex.word == "cat", "Lexeme word");
    test_assert(lex.positions.size() == 3, "Lexeme positions size");
    test_assert(lex.positions[0] == 1, "Lexeme position 1");
    test_assert(lex.positions[1] == 3, "Lexeme position 2");
    test_assert(lex.positions[2] == 5, "Lexeme position 3");
    test_assert(lex.weights.size() == 3, "Lexeme weights size");
    test_assert(lex.getWeight(0) == 'A', "Lexeme weight 1");
    test_assert(lex.getWeight(1) == 'B', "Lexeme weight 2");
    test_assert(lex.getWeight(2) == 'C', "Lexeme weight 3");
    test_assert(lex.isValid(), "Lexeme valid");
}

void test_tsvector_parse_simple()
{
    auto vec = TSVector::fromString("'cat':1 'dog':2");
    test_assert(vec.has_value(), "TSVector parse simple - has value");
    test_assert(vec->numLexemes() == 2, "TSVector parse simple - num lexemes");
    test_assert(vec->contains("cat"), "TSVector contains cat");
    test_assert(vec->contains("dog"), "TSVector contains dog");
    test_assert(!vec->contains("rat"), "TSVector doesn't contain rat");
}

void test_tsvector_parse_with_positions()
{
    auto vec = TSVector::fromString("'quick':1,4 'brown':2 'fox':3");
    test_assert(vec.has_value(), "TSVector parse positions - has value");
    test_assert(vec->numLexemes() == 3, "TSVector parse positions - num lexemes");

    const Lexeme* quick = vec->getLexeme("quick");
    test_assert(quick != nullptr, "TSVector get lexeme quick");
    test_assert(quick->positions.size() == 2, "TSVector quick positions size");
    test_assert(quick->positions[0] == 1, "TSVector quick position 1");
    test_assert(quick->positions[1] == 4, "TSVector quick position 2");
}

void test_tsvector_parse_with_weights()
{
    auto vec = TSVector::fromString("'cat':1A,3B 'dog':2C");
    test_assert(vec.has_value(), "TSVector parse weights - has value");

    const Lexeme* cat = vec->getLexeme("cat");
    test_assert(cat != nullptr, "TSVector get lexeme cat");
    test_assert(cat->getWeight(0) == 'A', "TSVector cat weight 1");
    test_assert(cat->getWeight(1) == 'B', "TSVector cat weight 2");

    const Lexeme* dog = vec->getLexeme("dog");
    test_assert(dog != nullptr, "TSVector get lexeme dog");
    test_assert(dog->getWeight(0) == 'C', "TSVector dog weight");
}

void test_tsvector_to_string()
{
    auto vec = TSVector::fromString("'cat':1,3A 'dog':2B");
    test_assert(vec.has_value(), "TSVector to_string - parse");

    std::string str = vec->toString();
    test_assert(str == "'cat':1,3A 'dog':2B", "TSVector to_string");
}

void test_tsvector_concat()
{
    auto vec1 = TSVector::fromString("'cat':1 'dog':2");
    auto vec2 = TSVector::fromString("'bird':3 'cat':5");

    test_assert(vec1.has_value() && vec2.has_value(), "TSVector concat - parse");

    TSVector merged = vec1->concat(*vec2);
    test_assert(merged.numLexemes() == 3, "TSVector concat - num lexemes");
    test_assert(merged.contains("cat"), "TSVector concat contains cat");
    test_assert(merged.contains("dog"), "TSVector concat contains dog");
    test_assert(merged.contains("bird"), "TSVector concat contains bird");

    // Check that cat positions were merged
    const Lexeme* cat = merged.getLexeme("cat");
    test_assert(cat != nullptr, "TSVector concat - get cat");
    test_assert(cat->positions.size() == 2, "TSVector concat - cat positions merged");
}

void test_tsvector_binary_serialization()
{
    auto vec1 = TSVector::fromString("'cat':1,3A 'dog':2B 'rat':5");
    test_assert(vec1.has_value(), "TSVector binary - parse");

    std::vector<uint8_t> binary = vec1->toBinary();
    test_assert(binary.size() > 0, "TSVector binary - serialized");

    auto vec2 = TSVector::fromBinary(binary);
    test_assert(vec2.has_value(), "TSVector binary - deserialized");

    test_assert(*vec1 == *vec2, "TSVector binary - round trip");
}

// ============================================================================
// TSQuery Tests
// ============================================================================

void test_tsquery_parse_simple()
{
    auto q = TSQuery::fromString("cat");
    test_assert(q.has_value(), "TSQuery parse simple - has value");
    test_assert(!q->empty(), "TSQuery parse simple - not empty");
}

void test_tsquery_parse_and()
{
    auto q = TSQuery::fromString("cat & dog");
    test_assert(q.has_value(), "TSQuery parse AND - has value");

    const TSQueryNode* root = q->root();
    test_assert(root != nullptr, "TSQuery parse AND - root exists");
    test_assert(root->type() == TSQueryNode::Type::AND, "TSQuery parse AND - is AND");
}

void test_tsquery_parse_or()
{
    auto q = TSQuery::fromString("cat | dog");
    test_assert(q.has_value(), "TSQuery parse OR - has value");

    const TSQueryNode* root = q->root();
    test_assert(root != nullptr, "TSQuery parse OR - root exists");
    test_assert(root->type() == TSQueryNode::Type::OR, "TSQuery parse OR - is OR");
}

void test_tsquery_parse_not()
{
    auto q = TSQuery::fromString("!cat");
    test_assert(q.has_value(), "TSQuery parse NOT - has value");

    const TSQueryNode* root = q->root();
    test_assert(root != nullptr, "TSQuery parse NOT - root exists");
    test_assert(root->type() == TSQueryNode::Type::NOT, "TSQuery parse NOT - is NOT");
}

void test_tsquery_parse_complex()
{
    auto q = TSQuery::fromString("(cat | dog) & !rat");
    test_assert(q.has_value(), "TSQuery parse complex - has value");
    test_assert(q->isValid(), "TSQuery parse complex - is valid");
}

void test_tsquery_parse_quoted()
{
    auto q = TSQuery::fromString("'hello world' & test");
    test_assert(q.has_value(), "TSQuery parse quoted - has value");
}

void test_tsquery_to_string()
{
    auto q = TSQuery::fromString("cat & dog");
    test_assert(q.has_value(), "TSQuery to_string - parse");

    std::string str = q->toString();
    test_assert(str.find("cat") != std::string::npos, "TSQuery to_string contains cat");
    test_assert(str.find("dog") != std::string::npos, "TSQuery to_string contains dog");
}

void test_tsquery_binary_serialization()
{
    auto q1 = TSQuery::fromString("(cat | dog) & !rat");
    test_assert(q1.has_value(), "TSQuery binary - parse");

    std::vector<uint8_t> binary = q1->toBinary();
    test_assert(binary.size() > 0, "TSQuery binary - serialized");

    auto q2 = TSQuery::fromBinary(binary);
    test_assert(q2.has_value(), "TSQuery binary - deserialized");

    test_assert(*q1 == *q2, "TSQuery binary - round trip");
}

// ============================================================================
// Matching Tests
// ============================================================================

void test_match_single_term()
{
    auto vec = TSVector::fromString("'cat':1 'dog':2 'bird':3");
    auto q = TSQuery::fromString("cat");

    test_assert(vec.has_value() && q.has_value(), "Match single - parse");
    test_assert(q->matches(*vec), "Match single - cat matches");

    auto q2 = TSQuery::fromString("fish");
    test_assert(q2.has_value(), "Match single - parse fish");
    test_assert(!q2->matches(*vec), "Match single - fish doesn't match");
}

void test_match_and()
{
    auto vec = TSVector::fromString("'cat':1 'dog':2 'bird':3");
    auto q1 = TSQuery::fromString("cat & dog");
    test_assert(q1.has_value(), "Match AND - parse cat & dog");
    test_assert(q1->matches(*vec), "Match AND - cat & dog matches");

    auto q2 = TSQuery::fromString("cat & fish");
    test_assert(q2.has_value(), "Match AND - parse cat & fish");
    test_assert(!q2->matches(*vec), "Match AND - cat & fish doesn't match");
}

void test_match_or()
{
    auto vec = TSVector::fromString("'cat':1 'dog':2");
    auto q1 = TSQuery::fromString("cat | fish");
    test_assert(q1.has_value(), "Match OR - parse cat | fish");
    test_assert(q1->matches(*vec), "Match OR - cat | fish matches");

    auto q2 = TSQuery::fromString("fish | rat");
    test_assert(q2.has_value(), "Match OR - parse fish | rat");
    test_assert(!q2->matches(*vec), "Match OR - fish | rat doesn't match");
}

void test_match_not()
{
    auto vec = TSVector::fromString("'cat':1 'dog':2");
    auto q1 = TSQuery::fromString("cat & !fish");
    test_assert(q1.has_value(), "Match NOT - parse cat & !fish");
    test_assert(q1->matches(*vec), "Match NOT - cat & !fish matches");

    auto q2 = TSQuery::fromString("cat & !dog");
    test_assert(q2.has_value(), "Match NOT - parse cat & !dog");
    test_assert(!q2->matches(*vec), "Match NOT - cat & !dog doesn't match");
}

void test_match_phrase()
{
    auto vec = TSVector::fromString("'quick':1 'brown':2 'fox':3");
    auto q1 = TSQuery::fromString("quick <2> fox");
    test_assert(q1.has_value(), "Match phrase - parse quick <2> fox");
    test_assert(q1->matches(*vec), "Match phrase - quick <2> fox matches");

    auto q2 = TSQuery::fromString("quick <1> fox");
    test_assert(q2.has_value(), "Match phrase - parse quick <1> fox");
    test_assert(!q2->matches(*vec), "Match phrase - quick <1> fox doesn't match (distance too small)");
}

void test_match_complex()
{
    auto vec = TSVector::fromString("'cat':1 'dog':2 'bird':3 'fish':4");
    auto q = TSQuery::fromString("(cat | dog) & bird & !rat");
    test_assert(q.has_value(), "Match complex - parse");
    test_assert(q->matches(*vec), "Match complex - matches");
}

// ============================================================================
// Main
// ============================================================================


TEST(TextSearchTypesTest, Comprehensive) {

    std::cout << "=== Testing TSVector ===" << std::endl;
    test_lexeme_construction();
    test_tsvector_parse_simple();
    test_tsvector_parse_with_positions();
    test_tsvector_parse_with_weights();
    test_tsvector_to_string();
    test_tsvector_concat();
    test_tsvector_binary_serialization();

    std::cout << "\n=== Testing TSQuery ===" << std::endl;
    test_tsquery_parse_simple();
    test_tsquery_parse_and();
    test_tsquery_parse_or();
    test_tsquery_parse_not();
    test_tsquery_parse_complex();
    test_tsquery_parse_quoted();
    test_tsquery_to_string();
    test_tsquery_binary_serialization();

    std::cout << "\n=== Testing Matching ===" << std::endl;
    test_match_single_term();
    test_match_and();
    test_match_or();
    test_match_not();
    test_match_phrase();
    test_match_complex();

    std::cout << "\n=== All Tests Passed! ===" << std::endl;
    return;

}
