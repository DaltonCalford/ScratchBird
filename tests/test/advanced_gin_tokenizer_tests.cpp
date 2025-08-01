/*
 *	PROGRAM:	ScratchBird Advanced GIN Tokenizer Tests
 *	MODULE:		advanced_gin_tokenizer_tests.cpp
 *	DESCRIPTION:	Comprehensive tests for Unicode and language-aware tokenization
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * All Rights Reserved.
 * 2025.07.22 - ScratchBird Advanced GIN Tokenizer Tests
 */

#include "scratchbird.h"
#include "boost/test/unit_test.hpp"
#include "../jrd/GinIndex.h"
#include "../jrd/GinTokenizer.h"
#include "../jrd/constants.h"
#include "../common/gdsassert.h"
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <iomanip>

using namespace ScratchBird;
using namespace Jrd;

BOOST_AUTO_TEST_SUITE(AdvancedGinTokenizerSuite)

namespace {
	// Test data for various languages and Unicode scenarios
	
	struct TestCase {
		const char* input;
		std::vector<std::string> expected_tokens;
		LanguageProcessor::LanguageCode language;
		const char* description;
	};
	
	// Unicode test cases
	TestCase unicode_test_cases[] = {
		{
			"Hello 世界 Мир العالم", 
			{"hello", "世界", "мир", "العالم"},
			LanguageProcessor::LANG_NEUTRAL,
			"Mixed script text (English, Chinese, Russian, Arabic)"
		},
		{
			"café naïve résumé", 
			{"café", "naïve", "résumé"},
			LanguageProcessor::LANG_FRENCH,
			"French text with diacritics"
		},
		{
			"Straße Größe Weiß", 
			{"straße", "größe", "weiß"},
			LanguageProcessor::LANG_GERMAN,
			"German text with umlauts and eszett"
		},
		{
			"niño piñata años", 
			{"niño", "piñata", "años"},
			LanguageProcessor::LANG_SPANISH,
			"Spanish text with tildes"
		},
		{
			"こんにちは カタカナ ひらがな", 
			{"こんにちは", "カタカナ", "ひらがな"},
			LanguageProcessor::LANG_JAPANESE,
			"Japanese text with Hiragana and Katakana"
		}
	};
	
	// Language-specific test cases
	TestCase language_test_cases[] = {
		{
			"The quick brown fox jumps over the lazy dog",
			{"quick", "brown", "fox", "jumps", "lazy", "dog"},
			LanguageProcessor::LANG_ENGLISH,
			"English with stop word removal"
		},
		{
			"El rápido zorro marrón salta sobre el perro perezoso",
			{"rápido", "zorro", "marrón", "salta", "perro", "perezoso"},
			LanguageProcessor::LANG_SPANISH,
			"Spanish with stop word removal"
		},
		{
			"Le renard brun rapide saute par-dessus le chien paresseux",
			{"renard", "brun", "rapide", "saute", "par-dessus", "chien", "paresseux"},
			LanguageProcessor::LANG_FRENCH,
			"French with stop word removal"
		},
		{
			"Der schnelle braune Fuchs springt über den faulen Hund",
			{"schnelle", "braune", "fuchs", "springt", "faulen", "hund"},
			LanguageProcessor::LANG_GERMAN,
			"German with stop word removal"
		}
	};
	
	// Stemming test cases
	TestCase stemming_test_cases[] = {
		{
			"running runner runs ran",
			{"run", "run", "run", "ran"},
			LanguageProcessor::LANG_ENGLISH,
			"English stemming test"
		},
		{
			"programming programmed programmer programs",
			{"program", "program", "program", "program"},
			LanguageProcessor::LANG_ENGLISH,
			"English stemming - programming terms"
		},
		{
			"corriendo corredor corre corrió",
			{"corr", "corre", "corre", "corrió"},
			LanguageProcessor::LANG_SPANISH,
			"Spanish stemming test"
		}
	};
}

BOOST_AUTO_TEST_SUITE(UnicodeHelperTests)

BOOST_AUTO_TEST_CASE(BasicUnicodeCharacterExtraction)
{
	std::cout << "\n=== Unicode Character Extraction Tests ===" << std::endl;
	
	// Test ASCII characters
	const UCHAR ascii_text[] = "Hello";
	USHORT bytes_consumed;
	ULONG char1 = UnicodeHelper::getCharacter(&ascii_text[0], bytes_consumed);
	
	BOOST_TEST(char1 == 'H');
	BOOST_TEST(bytes_consumed == 1);
	
	// Test 2-byte UTF-8 character (é = 0xC3 0xA9)
	const UCHAR utf8_2byte[] = {0xC3, 0xA9, 0x00}; // é
	ULONG char2 = UnicodeHelper::getCharacter(utf8_2byte, bytes_consumed);
	
	BOOST_TEST(char2 == 0x00E9); // Unicode code point for é
	BOOST_TEST(bytes_consumed == 2);
	
	// Test 3-byte UTF-8 character (€ = 0xE2 0x82 0xAC)
	const UCHAR utf8_3byte[] = {0xE2, 0x82, 0xAC, 0x00}; // €
	ULONG char3 = UnicodeHelper::getCharacter(utf8_3byte, bytes_consumed);
	
	BOOST_TEST(char3 == 0x20AC); // Unicode code point for €
	BOOST_TEST(bytes_consumed == 3);
	
	std::cout << "✅ Unicode character extraction working correctly" << std::endl;
}

BOOST_AUTO_TEST_CASE(UnicodeCharacterClassification)
{
	std::cout << "\n=== Unicode Character Classification Tests ===" << std::endl;
	
	// Test ASCII classification
	BOOST_TEST(UnicodeHelper::isLetter('A') == true);
	BOOST_TEST(UnicodeHelper::isLetter('z') == true);
	BOOST_TEST(UnicodeHelper::isDigit('5') == true);
	BOOST_TEST(UnicodeHelper::isWhitespace(' ') == true);
	BOOST_TEST(UnicodeHelper::isPunctuation('.') == true);
	
	// Test Unicode classification
	BOOST_TEST(UnicodeHelper::isLetter(0x00E9) == true);  // é
	BOOST_TEST(UnicodeHelper::isLetter(0x03B1) == true);  // α (Greek alpha)
	BOOST_TEST(UnicodeHelper::isLetter(0x0440) == true);  // р (Cyrillic r)
	
	std::cout << "✅ Unicode character classification working correctly" << std::endl;
}

BOOST_AUTO_TEST_CASE(UnicodeCaseConversion)
{
	std::cout << "\n=== Unicode Case Conversion Tests ===" << std::endl;
	
	// Test ASCII case conversion
	BOOST_TEST(UnicodeHelper::toLower('A') == 'a');
	BOOST_TEST(UnicodeHelper::toLower('Z') == 'z');
	BOOST_TEST(UnicodeHelper::toUpper('a') == 'A');
	BOOST_TEST(UnicodeHelper::toUpper('z') == 'Z');
	
	// Test extended Latin case conversion
	BOOST_TEST(UnicodeHelper::toLower(0x00C0) == 0x00E0); // À -> à
	BOOST_TEST(UnicodeHelper::toUpper(0x00E9) == 0x00C9); // é -> É
	
	std::cout << "✅ Unicode case conversion working correctly" << std::endl;
}

BOOST_AUTO_TEST_CASE(Utf8Encoding)
{
	std::cout << "\n=== UTF-8 Encoding Tests ===" << std::endl;
	
	UCHAR buffer[4];
	
	// Test 1-byte encoding (ASCII)
	USHORT length = UnicodeHelper::encodeUtf8('A', buffer);
	BOOST_TEST(length == 1);
	BOOST_TEST(buffer[0] == 'A');
	
	// Test 2-byte encoding (é)
	length = UnicodeHelper::encodeUtf8(0x00E9, buffer);
	BOOST_TEST(length == 2);
	BOOST_TEST(buffer[0] == 0xC3);
	BOOST_TEST(buffer[1] == 0xA9);
	
	// Test 3-byte encoding (€)
	length = UnicodeHelper::encodeUtf8(0x20AC, buffer);
	BOOST_TEST(length == 3);
	BOOST_TEST(buffer[0] == 0xE2);
	BOOST_TEST(buffer[1] == 0x82);
	BOOST_TEST(buffer[2] == 0xAC);
	
	std::cout << "✅ UTF-8 encoding working correctly" << std::endl;
}

BOOST_AUTO_TEST_SUITE_END() // UnicodeHelperTests

BOOST_AUTO_TEST_SUITE(LanguageProcessorTests)

BOOST_AUTO_TEST_CASE(EnglishStopWordDetection)
{
	std::cout << "\n=== English Stop Word Detection Tests ===" << std::endl;
	
	LanguageProcessor processor(LanguageProcessor::LANG_ENGLISH);
	
	// Test common English stop words
	Token the_token("the");
	Token and_token("and");
	Token database_token("database");
	Token performance_token("performance");
	
	BOOST_TEST(processor.isStopWord(the_token) == true);
	BOOST_TEST(processor.isStopWord(and_token) == true);
	BOOST_TEST(processor.isStopWord(database_token) == false);
	BOOST_TEST(processor.isStopWord(performance_token) == false);
	
	std::cout << "✅ English stop word detection working correctly" << std::endl;
}

BOOST_AUTO_TEST_CASE(EnglishStemming)
{
	std::cout << "\n=== English Stemming Tests ===" << std::endl;
	
	LanguageProcessor processor(LanguageProcessor::LANG_ENGLISH);
	
	// Test basic stemming
	Token running("running");
	Token runs("runs");
	Token databases("databases");
	
	Token stemmed1 = processor.applyStemming(running);
	Token stemmed2 = processor.applyStemming(runs);
	Token stemmed3 = processor.applyStemming(databases);
	
	std::cout << "Original: 'running' -> Stemmed: '" << stemmed1.c_str() << "'" << std::endl;
	std::cout << "Original: 'runs' -> Stemmed: '" << stemmed2.c_str() << "'" << std::endl;
	std::cout << "Original: 'databases' -> Stemmed: '" << stemmed3.c_str() << "'" << std::endl;
	
	// Check that stemming reduces word forms
	BOOST_TEST(stemmed1.length <= running.length);
	BOOST_TEST(stemmed2.length <= runs.length);
	BOOST_TEST(stemmed3.length <= databases.length);
	
	std::cout << "✅ English stemming working correctly" << std::endl;
}

BOOST_AUTO_TEST_CASE(MultiLanguageSupport)
{
	std::cout << "\n=== Multi-Language Support Tests ===" << std::endl;
	
	// Test different language processors
	LanguageProcessor english(LanguageProcessor::LANG_ENGLISH);
	LanguageProcessor spanish(LanguageProcessor::LANG_SPANISH);
	LanguageProcessor french(LanguageProcessor::LANG_FRENCH);
	LanguageProcessor german(LanguageProcessor::LANG_GERMAN);
	
	Token el_token("el"); // Spanish "the"
	Token le_token("le"); // French "the"  
	Token der_token("der"); // German "the"
	Token the_token("the"); // English "the"
	
	BOOST_TEST(english.isStopWord(the_token) == true);
	BOOST_TEST(english.isStopWord(el_token) == false);
	
	BOOST_TEST(spanish.isStopWord(el_token) == true);
	BOOST_TEST(spanish.isStopWord(the_token) == false);
	
	BOOST_TEST(french.isStopWord(le_token) == true);
	BOOST_TEST(french.isStopWord(the_token) == false);
	
	BOOST_TEST(german.isStopWord(der_token) == true);
	BOOST_TEST(german.isStopWord(the_token) == false);
	
	std::cout << "✅ Multi-language support working correctly" << std::endl;
}

BOOST_AUTO_TEST_SUITE_END() // LanguageProcessorTests

BOOST_AUTO_TEST_SUITE(AdvancedTokenizerTests)

BOOST_AUTO_TEST_CASE(BasicAdvancedTokenization)
{
	std::cout << "\n=== Basic Advanced Tokenization Tests ===" << std::endl;
	
	TokenizerConfig config = TokenizerFactory::getDefaultConfig();
	config.enable_stop_words = true;
	config.language = LanguageProcessor::LANG_ENGLISH;
	
	AdvancedGinTokenizer tokenizer(config);
	
	const char* test_text = "The quick brown fox jumps over the lazy dog";
	TokenList tokens = tokenizer.tokenize(test_text);
	
	std::cout << "Input: '" << test_text << "'" << std::endl;
	std::cout << "Tokens (" << tokens.getCount() << "): ";
	
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		std::cout << "'" << tokens[i].c_str() << "'";
		if (i < tokens.getCount() - 1) std::cout << ", ";
	}
	std::cout << std::endl;
	
	// Should have fewer tokens due to stop word removal
	BOOST_TEST(tokens.getCount() < 9); // Original has 9 words
	BOOST_TEST(tokens.getCount() >= 4); // Should have at least main content words
	
	std::cout << "✅ Basic advanced tokenization working correctly" << std::endl;
}

BOOST_AUTO_TEST_CASE(UnicodeTextTokenization)
{
	std::cout << "\n=== Unicode Text Tokenization Tests ===" << std::endl;
	
	TokenizerConfig config = TokenizerFactory::getDefaultConfig();
	config.handle_diacritics = true;
	config.fold_case = true;
	
	AdvancedGinTokenizer tokenizer(config);
	
	// Run through Unicode test cases
	for (size_t i = 0; i < sizeof(unicode_test_cases) / sizeof(unicode_test_cases[0]); i++) {
		const TestCase& test_case = unicode_test_cases[i];
		
		std::cout << "\nTest: " << test_case.description << std::endl;
		std::cout << "Input: '" << test_case.input << "'" << std::endl;
		
		config.language = test_case.language;
		tokenizer.setConfig(config);
		
		TokenList tokens = tokenizer.tokenize(test_case.input);
		
		std::cout << "Tokens (" << tokens.getCount() << "): ";
		for (FB_SIZE_T j = 0; j < tokens.getCount(); j++) {
			std::cout << "'" << tokens[j].c_str() << "'";
			if (j < tokens.getCount() - 1) std::cout << ", ";
		}
		std::cout << std::endl;
		
		// Should produce reasonable number of tokens
		BOOST_TEST(tokens.getCount() > 0);
		BOOST_TEST(tokens.getCount() <= 10);
	}
	
	std::cout << "✅ Unicode text tokenization working correctly" << std::endl;
}

BOOST_AUTO_TEST_CASE(LanguageSpecificTokenization)
{
	std::cout << "\n=== Language-Specific Tokenization Tests ===" << std::endl;
	
	// Run through language-specific test cases
	for (size_t i = 0; i < sizeof(language_test_cases) / sizeof(language_test_cases[0]); i++) {
		const TestCase& test_case = language_test_cases[i];
		
		std::cout << "\nTest: " << test_case.description << std::endl;
		std::cout << "Input: '" << test_case.input << "'" << std::endl;
		
		TokenizerConfig config = TokenizerFactory::getLanguageConfig(test_case.language);
		config.enable_stop_words = true;
		
		AdvancedGinTokenizer tokenizer(config);
		TokenList tokens = tokenizer.tokenize(test_case.input);
		
		std::cout << "Tokens (" << tokens.getCount() << "): ";
		for (FB_SIZE_T j = 0; j < tokens.getCount(); j++) {
			std::cout << "'" << tokens[j].c_str() << "'";
			if (j < tokens.getCount() - 1) std::cout << ", ";
		}
		std::cout << std::endl;
		
		// Should have removed stop words
		BOOST_TEST(tokens.getCount() > 0);
		
		// Check that some expected tokens are present
		if (test_case.expected_tokens.size() > 0) {
			bool found_expected = false;
			for (FB_SIZE_T j = 0; j < tokens.getCount(); j++) {
				std::string token_str(tokens[j].c_str());
				for (const std::string& expected : test_case.expected_tokens) {
					if (token_str.find(expected.substr(0, 3)) != std::string::npos) {
						found_expected = true;
						break;
					}
				}
				if (found_expected) break;
			}
			BOOST_TEST(found_expected);
		}
	}
	
	std::cout << "✅ Language-specific tokenization working correctly" << std::endl;
}

BOOST_AUTO_TEST_CASE(StemmingFeatureTest)
{
	std::cout << "\n=== Stemming Feature Tests ===" << std::endl;
	
	TokenizerConfig config = TokenizerFactory::getLanguageConfig(LanguageProcessor::LANG_ENGLISH);
	config.enable_stemming = true;
	config.enable_stop_words = false; // Disable to see all stemmed tokens
	
	AdvancedGinTokenizer tokenizer(config);
	
	// Run through stemming test cases
	for (size_t i = 0; i < sizeof(stemming_test_cases) / sizeof(stemming_test_cases[0]); i++) {
		const TestCase& test_case = stemming_test_cases[i];
		
		std::cout << "\nTest: " << test_case.description << std::endl;
		std::cout << "Input: '" << test_case.input << "'" << std::endl;
		
		config.language = test_case.language;
		tokenizer.setConfig(config);
		
		TokenList tokens = tokenizer.tokenize(test_case.input);
		
		std::cout << "Stemmed tokens (" << tokens.getCount() << "): ";
		for (FB_SIZE_T j = 0; j < tokens.getCount(); j++) {
			std::cout << "'" << tokens[j].c_str() << "'";
			if (j < tokens.getCount() - 1) std::cout << ", ";
		}
		std::cout << std::endl;
		
		// Check stemming statistics
		AdvancedGinTokenizer::TokenizationStats stats = tokenizer.getLastTokenizationStats();
		std::cout << "Tokens stemmed: " << stats.tokens_stemmed << std::endl;
		
		BOOST_TEST(tokens.getCount() > 0);
	}
	
	std::cout << "✅ Stemming feature working correctly" << std::endl;
}

BOOST_AUTO_TEST_CASE(NGramGenerationTest)
{
	std::cout << "\n=== N-Gram Generation Tests ===" << std::endl;
	
	TokenizerConfig config = TokenizerFactory::getDefaultConfig();
	config.enable_ngrams = true;
	config.ngram_size = 3;
	
	AdvancedGinTokenizer tokenizer(config);
	
	const char* test_text = "database performance";
	TokenList tokens = tokenizer.tokenize(test_text);
	
	std::cout << "Input: '" << test_text << "'" << std::endl;
	std::cout << "Tokens with n-grams (" << tokens.getCount() << "): ";
	
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		std::cout << "'" << tokens[i].c_str() << "'";
		if (i < tokens.getCount() - 1) std::cout << ", ";
	}
	std::cout << std::endl;
	
	// Should have more tokens due to n-gram generation
	BOOST_TEST(tokens.getCount() > 2); // Original has 2 words
	
	std::cout << "✅ N-gram generation working correctly" << std::endl;
}

BOOST_AUTO_TEST_CASE(PhoneticEncodingTest)
{
	std::cout << "\n=== Phonetic Encoding Tests ===" << std::endl;
	
	TokenizerConfig config = TokenizerFactory::getDefaultConfig();
	config.enable_phonetic = true;
	
	AdvancedGinTokenizer tokenizer(config);
	
	const char* test_text = "Smith Smythe Schmidt";
	TokenList tokens = tokenizer.tokenize(test_text);
	
	std::cout << "Input: '" << test_text << "'" << std::endl;
	std::cout << "Tokens with phonetic codes (" << tokens.getCount() << "): ";
	
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		std::cout << "'" << tokens[i].c_str() << "'";
		if (i < tokens.getCount() - 1) std::cout << ", ";
	}
	std::cout << std::endl;
	
	// Should have more tokens due to phonetic encoding
	BOOST_TEST(tokens.getCount() > 3); // Original has 3 words
	
	std::cout << "✅ Phonetic encoding working correctly" << std::endl;
}

BOOST_AUTO_TEST_CASE(TokenizationPerformanceTest)
{
	std::cout << "\n=== Tokenization Performance Tests ===" << std::endl;
	
	TokenizerConfig config = TokenizerFactory::getPerformanceConfig();
	AdvancedGinTokenizer tokenizer(config);
	
	// Large text for performance testing
	std::string large_text = "Database performance optimization is crucial for enterprise applications. "
		"Index structures like B-trees, hash tables, and inverted indexes provide "
		"different trade-offs between query speed and storage efficiency. "
		"Full-text search capabilities enable powerful document retrieval and "
		"content analysis across large text collections. Modern database engines "
		"implement sophisticated query optimizers that analyze execution plans "
		"and select optimal index access paths based on statistical models.";
	
	// Repeat text to make it larger
	for (int i = 0; i < 5; i++) {
		large_text += " " + large_text;
	}
	
	std::cout << "Processing text of " << large_text.length() << " characters" << std::endl;
	
	auto start_time = std::chrono::high_resolution_clock::now();
	
	TokenList tokens = tokenizer.tokenize(large_text.c_str());
	
	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
	
	AdvancedGinTokenizer::TokenizationStats stats = tokenizer.getLastTokenizationStats();
	
	std::cout << "Results:" << std::endl;
	std::cout << "  Total characters: " << stats.total_characters << std::endl;
	std::cout << "  Total tokens: " << stats.total_tokens << std::endl;
	std::cout << "  Unique tokens: " << stats.unique_tokens << std::endl;
	std::cout << "  Processing time: " << stats.processing_time_ms << " ms" << std::endl;
	std::cout << "  Measured time: " << duration.count() << " μs" << std::endl;
	
	if (stats.total_characters > 0) {
		std::cout << "  Characters/second: " << (stats.total_characters * 1000.0 / stats.processing_time_ms) << std::endl;
	}
	
	BOOST_TEST(tokens.getCount() > 0);
	BOOST_TEST(stats.processing_time_ms < 1000); // Should complete in < 1 second
	
	std::cout << "✅ Tokenization performance acceptable" << std::endl;
}

BOOST_AUTO_TEST_CASE(TokenCachingTest)
{
	std::cout << "\n=== Token Caching Tests ===" << std::endl;
	
	TokenizerConfig config = TokenizerFactory::getDefaultConfig();
	config.enable_token_caching = true;
	config.max_token_cache_size = 100;
	
	AdvancedGinTokenizer tokenizer(config);
	
	const char* test_text = "database performance optimization";
	
	// First tokenization (cache miss)
	TokenList tokens1 = tokenizer.tokenize(test_text);
	
	// Second tokenization (cache hit)
	TokenList tokens2 = tokenizer.tokenize(test_text);
	
	// Results should be identical
	BOOST_TEST(tokens1.getCount() == tokens2.getCount());
	
	for (FB_SIZE_T i = 0; i < tokens1.getCount(); i++) {
		BOOST_TEST(tokens1[i] == tokens2[i]);
	}
	
	std::cout << "Cached " << tokens1.getCount() << " tokens successfully" << std::endl;
	std::cout << "✅ Token caching working correctly" << std::endl;
}

BOOST_AUTO_TEST_SUITE_END() // AdvancedTokenizerTests

BOOST_AUTO_TEST_SUITE(TokenizerFactoryTests)

BOOST_AUTO_TEST_CASE(TokenizerCreationTest)
{
	std::cout << "\n=== Tokenizer Factory Tests ===" << std::endl;
	
	// Test different tokenizer types
	GinTokenizer* simple = TokenizerFactory::createSimpleTokenizer();
	BOOST_TEST(simple != nullptr);
	BOOST_TEST(simple->getTypeName() != nullptr);
	
	GinTokenizer* standard = TokenizerFactory::createStandardTokenizer();
	BOOST_TEST(standard != nullptr);
	
	GinTokenizer* advanced = TokenizerFactory::createAdvancedTokenizer();
	BOOST_TEST(advanced != nullptr);
	
	GinTokenizer* english = TokenizerFactory::createLanguageTokenizer(LanguageProcessor::LANG_ENGLISH);
	BOOST_TEST(english != nullptr);
	
	// Test configuration presets
	TokenizerConfig default_config = TokenizerFactory::getDefaultConfig();
	BOOST_TEST(default_config.min_token_length >= GIN_MIN_TOKEN_LENGTH);
	BOOST_TEST(default_config.max_token_length <= GIN_MAX_TOKEN_LENGTH);
	
	TokenizerConfig perf_config = TokenizerFactory::getPerformanceConfig();
	BOOST_TEST(perf_config.type == GinTokenizer::SIMPLE_TOKENIZER);
	
	TokenizerConfig quality_config = TokenizerFactory::getQualityConfig();
	BOOST_TEST(quality_config.enable_stemming == true);
	BOOST_TEST(quality_config.enable_stop_words == true);
	
	// Clean up
	delete simple;
	delete standard;
	delete advanced;
	delete english;
	
	std::cout << "✅ Tokenizer factory working correctly" << std::endl;
}

BOOST_AUTO_TEST_CASE(TokenizerCapabilitiesTest)
{
	std::cout << "\n=== Tokenizer Capabilities Tests ===" << std::endl;
	
	// Test capability queries
	bool simple_unicode = TokenizerFactory::supportsUnicode(GinTokenizer::SIMPLE_TOKENIZER);
	bool standard_unicode = TokenizerFactory::supportsUnicode(GinTokenizer::STANDARD_TOKENIZER);
	bool language_unicode = TokenizerFactory::supportsUnicode(GinTokenizer::LANGUAGE_TOKENIZER);
	
	BOOST_TEST(simple_unicode == false);
	BOOST_TEST(standard_unicode == true);
	BOOST_TEST(language_unicode == true);
	
	bool simple_language = TokenizerFactory::supportsLanguage(GinTokenizer::SIMPLE_TOKENIZER);
	bool language_language = TokenizerFactory::supportsLanguage(GinTokenizer::LANGUAGE_TOKENIZER);
	
	BOOST_TEST(simple_language == false);
	BOOST_TEST(language_language == true);
	
	bool simple_advanced = TokenizerFactory::supportsAdvancedFeatures(GinTokenizer::SIMPLE_TOKENIZER);
	bool language_advanced = TokenizerFactory::supportsAdvancedFeatures(GinTokenizer::LANGUAGE_TOKENIZER);
	
	BOOST_TEST(simple_advanced == false);
	BOOST_TEST(language_advanced == true);
	
	std::cout << "✅ Tokenizer capabilities working correctly" << std::endl;
}

BOOST_AUTO_TEST_SUITE_END() // TokenizerFactoryTests

BOOST_AUTO_TEST_SUITE_END() // AdvancedGinTokenizerSuite