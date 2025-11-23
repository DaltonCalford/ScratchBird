#include "scratchbird/core/ts_config.h"
#include "scratchbird/core/ts_functions.h"
#include <cassert>
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
// TSConfig Tests
// ============================================================================

void test_config_registry()
{
    TSConfig* simple = TSConfig::get("simple");
    test_assert(simple != nullptr, "Get simple config");
    test_assert(simple->name() == "simple", "Simple config name");

    TSConfig* english = TSConfig::get("english");
    test_assert(english != nullptr, "Get english config");
    test_assert(english->name() == "english", "English config name");

    TSConfig* invalid = TSConfig::get("invalid");
    test_assert(invalid == nullptr, "Invalid config returns nullptr");
}

void test_simple_tokenization()
{
    SimpleConfig config;
    auto tokens = config.tokenize("The quick brown fox");

    test_assert(tokens.size() == 4, "Simple tokenize count");
    test_assert(tokens[0].word == "The", "Simple token 1");
    test_assert(tokens[1].word == "quick", "Simple token 2");
    test_assert(tokens[2].word == "brown", "Simple token 3");
    test_assert(tokens[3].word == "fox", "Simple token 4");
    test_assert(tokens[0].position == 1, "Simple token position 1");
    test_assert(tokens[1].position == 2, "Simple token position 2");
}

void test_simple_normalization()
{
    SimpleConfig config;
    std::string normalized = config.normalize("HELLO");
    test_assert(normalized == "hello", "Simple normalize lowercase");
}

void test_simple_no_stemming()
{
    SimpleConfig config;
    std::string stemmed = config.stem("running");
    test_assert(stemmed == "running", "Simple no stemming");
}

void test_simple_no_stop_words()
{
    SimpleConfig config;
    test_assert(!config.isStopWord("the"), "Simple no stop words");
}

// ============================================================================
// Porter Stemmer Tests
// ============================================================================

void test_porter_step1a()
{
    EnglishConfig config;

    test_assert(config.stem("caresses") == "caress", "Porter: caresses -> caress");
    test_assert(config.stem("ponies") == "poni", "Porter: ponies -> poni");
    test_assert(config.stem("ties") == "ti", "Porter: ties -> ti");
    test_assert(config.stem("caress") == "caress", "Porter: caress -> caress");
    test_assert(config.stem("cats") == "cat", "Porter: cats -> cat");
}

void test_porter_step1b()
{
    EnglishConfig config;

    test_assert(config.stem("agreed") == "agre", "Porter: agreed -> agre");
    test_assert(config.stem("plastered") == "plaster", "Porter: plastered -> plaster");
    test_assert(config.stem("motoring") == "motor", "Porter: motoring -> motor");
}

void test_porter_step1c()
{
    EnglishConfig config;

    test_assert(config.stem("happy") == "happi", "Porter: happy -> happi");
    test_assert(config.stem("sky") == "sky", "Porter: sky -> sky (no vowel before y)");
}

void test_porter_common_words()
{
    EnglishConfig config;

    test_assert(config.stem("running") == "run", "Porter: running -> run");
    test_assert(config.stem("flies") == "fli", "Porter: flies -> fli");
    test_assert(config.stem("died") == "di", "Porter: died -> di");
    test_assert(config.stem("agreed") == "agre", "Porter: agreed -> agre");
    test_assert(config.stem("disabled") == "disabl", "Porter: disabled -> disabl");
    test_assert(config.stem("sized") == "siz", "Porter: sized -> siz");
    test_assert(config.stem("meeting") == "meet", "Porter: meeting -> meet");
    test_assert(config.stem("stating") == "stat", "Porter: stating -> stat");
    test_assert(config.stem("siezing") == "siez", "Porter: siezing -> siez");
    test_assert(config.stem("itemization") == "item", "Porter: itemization -> item");
    test_assert(config.stem("sensational") == "sensat", "Porter: sensational -> sensat");
    test_assert(config.stem("traditional") == "tradit", "Porter: traditional -> tradit");
}

void test_english_stop_words()
{
    EnglishConfig config;

    test_assert(config.isStopWord("the"), "English stop word: the");
    test_assert(config.isStopWord("a"), "English stop word: a");
    test_assert(config.isStopWord("and"), "English stop word: and");
    test_assert(config.isStopWord("is"), "English stop word: is");
    test_assert(config.isStopWord("of"), "English stop word: of");

    test_assert(!config.isStopWord("hello"), "English non-stop word: hello");
    test_assert(!config.isStopWord("world"), "English non-stop word: world");
}

// ============================================================================
// to_tsvector Tests
// ============================================================================

void test_to_tsvector_simple()
{
    auto vec = to_tsvector("simple", "The quick brown fox");
    test_assert(vec.has_value(), "to_tsvector simple - has value");
    test_assert(vec->numLexemes() == 4, "to_tsvector simple - 4 lexemes");
    test_assert(vec->contains("the"), "to_tsvector simple contains 'the'");
    test_assert(vec->contains("quick"), "to_tsvector simple contains 'quick'");
}

void test_to_tsvector_english()
{
    auto vec = to_tsvector("english", "The quick brown fox");
    test_assert(vec.has_value(), "to_tsvector english - has value");

    // "The" should be filtered as stop word
    test_assert(!vec->contains("the"), "to_tsvector english filters 'the'");
    test_assert(vec->contains("quick"), "to_tsvector english contains 'quick'");
    test_assert(vec->contains("brown"), "to_tsvector english contains 'brown'");
    test_assert(vec->contains("fox"), "to_tsvector english contains 'fox'");
}

void test_to_tsvector_stemming()
{
    auto vec = to_tsvector("english", "I am running with cats");
    test_assert(vec.has_value(), "to_tsvector stemming - has value");

    // "running" -> "run", "cats" -> "cat"
    test_assert(vec->contains("run"), "to_tsvector stems 'running' -> 'run'");
    test_assert(vec->contains("cat"), "to_tsvector stems 'cats' -> 'cat'");

    // Stop words filtered
    test_assert(!vec->contains("am"), "to_tsvector filters 'am'");
    test_assert(!vec->contains("with"), "to_tsvector filters 'with'");
}

void test_to_tsvector_positions()
{
    auto vec = to_tsvector("english", "quick brown quick");
    test_assert(vec.has_value(), "to_tsvector positions - has value");

    const Lexeme* quick = vec->getLexeme("quick");
    test_assert(quick != nullptr, "to_tsvector get lexeme 'quick'");

    // "quick" appears at positions 1 and 3
    test_assert(quick->positions.size() == 2, "to_tsvector 'quick' has 2 positions");
    test_assert(quick->positions[0] == 1, "to_tsvector 'quick' position 1");
    test_assert(quick->positions[1] == 3, "to_tsvector 'quick' position 3");
}

// ============================================================================
// to_tsquery Tests
// ============================================================================

void test_to_tsquery_simple()
{
    auto q = to_tsquery("simple", "cat & dog");
    test_assert(q.has_value(), "to_tsquery simple - has value");
    test_assert(q->root()->type() == TSQueryNode::Type::AND, "to_tsquery simple - AND node");
}

void test_to_tsquery_stemming()
{
    auto q = to_tsquery("english", "running & cats");
    test_assert(q.has_value(), "to_tsquery stemming - has value");

    // Query should be stemmed: 'run' & 'cat'
    std::string str = q->toString();
    test_assert(str.find("run") != std::string::npos, "to_tsquery stems 'running' -> 'run'");
    test_assert(str.find("cat") != std::string::npos, "to_tsquery stems 'cats' -> 'cat'");
}

void test_to_tsquery_complex()
{
    auto q = to_tsquery("english", "(running | flies) & !died");
    test_assert(q.has_value(), "to_tsquery complex - has value");
    test_assert(q->isValid(), "to_tsquery complex - valid");
}

// ============================================================================
// plainto_tsquery Tests
// ============================================================================

void test_plainto_tsquery_simple()
{
    auto q = plainto_tsquery("simple", "cat dog bird");
    test_assert(q.has_value(), "plainto_tsquery simple - has value");

    // Should create: cat & dog & bird
    std::string str = q->toString();
    test_assert(str.find("cat") != std::string::npos, "plainto_tsquery contains 'cat'");
    test_assert(str.find("dog") != std::string::npos, "plainto_tsquery contains 'dog'");
    test_assert(str.find("bird") != std::string::npos, "plainto_tsquery contains 'bird'");
}

void test_plainto_tsquery_english()
{
    auto q = plainto_tsquery("english", "running with cats");
    test_assert(q.has_value(), "plainto_tsquery english - has value");

    // Should create: run & cat (with filtered, stems applied)
    std::string str = q->toString();
    test_assert(str.find("run") != std::string::npos, "plainto_tsquery stems 'running'");
    test_assert(str.find("cat") != std::string::npos, "plainto_tsquery stems 'cats'");
}

void test_plainto_matching()
{
    auto vec = to_tsvector("english", "The quick brown fox jumps");
    auto q = plainto_tsquery("english", "quick fox");

    test_assert(vec.has_value() && q.has_value(), "plainto matching - parse");
    test_assert(q->matches(*vec), "plainto matching - matches");
}

// ============================================================================
// phraseto_tsquery Tests
// ============================================================================

void test_phraseto_tsquery_simple()
{
    auto q = phraseto_tsquery("simple", "quick brown fox");
    test_assert(q.has_value(), "phraseto_tsquery simple - has value");

    // Should create phrase query with <1> operators
    std::string str = q->toString();
    test_assert(str.find("quick") != std::string::npos, "phraseto_tsquery contains 'quick'");
    test_assert(str.find("brown") != std::string::npos, "phraseto_tsquery contains 'brown'");
    test_assert(str.find("fox") != std::string::npos, "phraseto_tsquery contains 'fox'");
}

void test_phraseto_matching()
{
    auto vec = to_tsvector("english", "The quick brown fox");
    auto q = phraseto_tsquery("english", "quick brown");

    test_assert(vec.has_value() && q.has_value(), "phraseto matching - parse");
    test_assert(q->matches(*vec), "phraseto matching - adjacent words match");
}

void test_phraseto_non_matching()
{
    auto vec = to_tsvector("english", "The quick red brown fox");
    auto q = phraseto_tsquery("english", "quick brown");

    test_assert(vec.has_value() && q.has_value(), "phraseto non-matching - parse");
    // "quick" and "brown" are not adjacent (separated by "red")
    test_assert(!q->matches(*vec), "phraseto non-matching - non-adjacent words don't match");
}

// ============================================================================
// Integration Tests
// ============================================================================

void test_full_text_search()
{
    // Create document
    auto doc = to_tsvector("english",
        "PostgreSQL is a powerful, open source object-relational database system");

    // Create query
    auto q = to_tsquery("english", "postgresql & database");

    test_assert(doc.has_value() && q.has_value(), "Full text search - parse");
    test_assert(q->matches(*doc), "Full text search - matches");

    // Negative test
    auto q2 = to_tsquery("english", "mongodb & nosql");
    test_assert(q2.has_value(), "Full text search negative - parse");
    test_assert(!q2->matches(*doc), "Full text search - doesn't match");
}

void test_stemming_improves_recall()
{
    // Document with "running"
    auto doc = to_tsvector("english", "I am running every day");

    // Query with "run" should still match (stemming)
    auto q = to_tsquery("english", "run");

    test_assert(doc.has_value() && q.has_value(), "Stemming recall - parse");
    test_assert(q->matches(*doc), "Stemming improves recall: 'run' matches 'running'");
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "=== Testing TSConfig ===" << std::endl;
    test_config_registry();
    test_simple_tokenization();
    test_simple_normalization();
    test_simple_no_stemming();
    test_simple_no_stop_words();

    std::cout << "\n=== Testing Porter Stemmer ===" << std::endl;
    test_porter_step1a();
    test_porter_step1b();
    test_porter_step1c();
    test_porter_common_words();
    test_english_stop_words();

    std::cout << "\n=== Testing to_tsvector ===" << std::endl;
    test_to_tsvector_simple();
    test_to_tsvector_english();
    test_to_tsvector_stemming();
    test_to_tsvector_positions();

    std::cout << "\n=== Testing to_tsquery ===" << std::endl;
    test_to_tsquery_simple();
    test_to_tsquery_stemming();
    test_to_tsquery_complex();

    std::cout << "\n=== Testing plainto_tsquery ===" << std::endl;
    test_plainto_tsquery_simple();
    test_plainto_tsquery_english();
    test_plainto_matching();

    std::cout << "\n=== Testing phraseto_tsquery ===" << std::endl;
    test_phraseto_tsquery_simple();
    test_phraseto_matching();
    test_phraseto_non_matching();

    std::cout << "\n=== Integration Tests ===" << std::endl;
    test_full_text_search();
    test_stemming_improves_recall();

    std::cout << "\n=== All Phase 2 Tests Passed! ===" << std::endl;
    return;
}
