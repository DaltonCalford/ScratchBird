/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		GinTokenizer.h  
 *	DESCRIPTION:	Advanced text tokenization engine with Unicode support for GIN indexes
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * 2025.07.22 - ScratchBird Advanced GIN Tokenizer Implementation
 */

#ifndef JRD_GIN_TOKENIZER_H
#define JRD_GIN_TOKENIZER_H

#include "../jrd/jrd.h"
#include "../jrd/constants.h"
#include "../common/classes/array.h"
#include "../common/classes/GenericMap.h"
#include "../include/firebird/impl/dsc_pub.h"
#include <vector>
#include <string>

namespace Jrd {

// Forward declarations
struct Token;
class TokenList;

//----------------------------
// Token Structure
//----------------------------
struct Token
{
    USHORT length;              // Token length in bytes
    UCHAR* data;                // Token data (UTF-8 encoded)
    ULONG hash;                 // Precomputed hash for performance
    USHORT position;            // Position in original text
    
    Token() : length(0), data(nullptr), hash(0), position(0) {}
    Token(const UCHAR* token_data, USHORT token_length, USHORT pos = 0);
    Token(const Token& other);
    ~Token();
    
    Token& operator=(const Token& other);
    bool operator==(const Token& other) const;
    bool operator<(const Token& other) const;
    
    ScratchBird::string toString() const;
    void clear();
};

//----------------------------
// TokenList Container
//----------------------------
class TokenList
{
public:
    TokenList();
    TokenList(const TokenList& other);
    ~TokenList();
    
    TokenList& operator=(const TokenList& other);
    
    // Container interface
    FB_SIZE_T getCount() const { return m_count; }
    bool isEmpty() const { return m_count == 0; }
    void clear();
    
    // Element access
    const Token& operator[](FB_SIZE_T index) const;
    Token& operator[](FB_SIZE_T index);
    
    // Modification
    void add(const Token& token);
    void add(const UCHAR* data, USHORT length, USHORT position = 0);
    void remove(FB_SIZE_T index);
    void reserve(FB_SIZE_T capacity);
    
    // Iteration support
    const Token* begin() const { return m_tokens; }
    const Token* end() const { return m_tokens + m_count; }
    Token* begin() { return m_tokens; }
    Token* end() { return m_tokens + m_count; }
    
private:
    Token* m_tokens;
    FB_SIZE_T m_count;
    FB_SIZE_T m_capacity;
    
    void grow();
    void copyFrom(const TokenList& other);
};

//----------------------------
// Unicode Character Classification
//----------------------------
class UnicodeHelper
{
public:
	// Unicode character categories
	enum CharCategory {
		CATEGORY_LETTER = 0x01,			// Letters (Lu, Ll, Lt, Lm, Lo)
		CATEGORY_DIGIT = 0x02,			// Numbers (Nd, Nl, No)
		CATEGORY_MARK = 0x04,			// Marks (Mn, Mc, Me)
		CATEGORY_PUNCTUATION = 0x08,	// Punctuation (Pc, Pd, Ps, Pe, Pi, Pf, Po)
		CATEGORY_SYMBOL = 0x10,			// Symbols (Sm, Sc, Sk, So)
		CATEGORY_SEPARATOR = 0x20,		// Separators (Zs, Zl, Zp)
		CATEGORY_OTHER = 0x40			// Other characters (Cc, Cf, Cs, Co, Cn)
	};
	
	// Unicode normalization forms
	enum NormalizationForm {
		NORM_NONE = 0,		// No normalization
		NORM_NFC = 1,		// Canonical decomposition + canonical composition
		NORM_NFD = 2,		// Canonical decomposition
		NORM_NFKC = 3,		// Compatibility decomposition + canonical composition
		NORM_NFKD = 4		// Compatibility decomposition
	};
	
	// Character classification methods
	static ULONG getCharacter(const UCHAR* utf8_text, USHORT& bytes_consumed);
	static CharCategory getCharacterCategory(ULONG unicode_char);
	static bool isLetter(ULONG unicode_char);
	static bool isDigit(ULONG unicode_char);
	static bool isLetterOrDigit(ULONG unicode_char);
	static bool isWhitespace(ULONG unicode_char);
	static bool isPunctuation(ULONG unicode_char);
	static bool isControl(ULONG unicode_char);
	
	// Case conversion
	static ULONG toLower(ULONG unicode_char);
	static ULONG toUpper(ULONG unicode_char);
	static ULONG toTitle(ULONG unicode_char);
	
	// Text normalization
	static ScratchBird::string normalize(const UCHAR* utf8_text, USHORT length, NormalizationForm form);
	static ScratchBird::string normalize(const ScratchBird::string& text, NormalizationForm form);
	
	// UTF-8 utilities
	static USHORT getUtf8ByteLength(ULONG unicode_char);
	static USHORT encodeUtf8(ULONG unicode_char, UCHAR* buffer);
	static bool isValidUtf8Sequence(const UCHAR* text, USHORT length);
	
private:
	// Unicode lookup tables (simplified for basic support)
	static bool initializeUnicodeTables();
	static CharCategory lookupCharacterCategory(ULONG unicode_char);
	static ULONG lookupCaseConversion(ULONG unicode_char, bool to_lower);
};

//----------------------------
// Language-Specific Text Processing
//----------------------------
class LanguageProcessor
{
public:
	enum LanguageCode {
		LANG_NEUTRAL = 0,		// Language neutral (default)
		LANG_ENGLISH = 1,		// English
		LANG_SPANISH = 2,		// Spanish
		LANG_FRENCH = 3,		// French
		LANG_GERMAN = 4,		// German
		LANG_ITALIAN = 5,		// Italian
		LANG_PORTUGUESE = 6,	// Portuguese
		LANG_DUTCH = 7,			// Dutch
		LANG_RUSSIAN = 8,		// Russian
		LANG_CHINESE = 9,		// Chinese
		LANG_JAPANESE = 10		// Japanese
	};
	
	LanguageProcessor(LanguageCode language = LANG_NEUTRAL);
	~LanguageProcessor();
	
	// Language-specific processing
	bool isStopWord(const Token& token) const;
	Token applyStemming(const Token& token) const;
	TokenList applyLanguageSpecificRules(const TokenList& tokens) const;
	
	// Configuration
	void setLanguage(LanguageCode language);
	LanguageCode getLanguage() const { return m_language; }
	
	// Stop word management
	void loadStopWords(const ScratchBird::string& stop_word_list);
	void addStopWord(const ScratchBird::string& word);
	void removeStopWord(const ScratchBird::string& word);
	ULONG getStopWordCount() const;
	
private:
	LanguageCode m_language;
	ScratchBird::GenericMap<ScratchBird::LeftPooledPair<ScratchBird::string, bool>> m_stop_words;
	
	// Language-specific processing methods
	void initializeStopWords();
	Token stemEnglish(const Token& token) const;
	Token stemSpanish(const Token& token) const;
	Token stemFrench(const Token& token) const;
	Token stemGerman(const Token& token) const;
	
	// Porter stemmer implementation for English
	ScratchBird::string porterStem(const ScratchBird::string& word) const;
	bool isVowel(const ScratchBird::string& word, USHORT position) const;
	USHORT getMeasure(const ScratchBird::string& word) const;
	bool containsVowel(const ScratchBird::string& word) const;
	bool endsWithDoubleConsonant(const ScratchBird::string& word) const;
	bool cvc(const ScratchBird::string& word, USHORT position) const;
};

//----------------------------
// Advanced Tokenizer Configuration
//----------------------------
struct TokenizerConfig
{
	// Tokenization options
	TokenizerType type;
	USHORT min_token_length;
	USHORT max_token_length;
	bool case_sensitive;
	bool preserve_case;
	
	// Unicode options
	UnicodeHelper::NormalizationForm normalization;
	bool handle_diacritics;
	bool fold_case;
	
	// Language processing
	LanguageProcessor::LanguageCode language;
	bool enable_stop_words;
	bool enable_stemming;
	bool enable_language_rules;
	
	// Advanced features
	bool enable_ngrams;
	USHORT ngram_size;
	bool enable_phonetic;
	bool enable_synonyms;
	
	// Performance settings
	ULONG max_tokens_per_document;
	ULONG max_token_cache_size;
	bool enable_token_caching;
	
	TokenizerConfig();
	void setDefaults();
	void validate();
};

//----------------------------
// N-Gram Token Generator
//----------------------------
class NGramGenerator
{
public:
	NGramGenerator(USHORT ngram_size = 3);
	~NGramGenerator();
	
	// N-gram generation
	TokenList generateNGrams(const Token& token) const;
	TokenList generateNGrams(const TokenList& tokens) const;
	
	// Configuration
	void setNGramSize(USHORT size);
	USHORT getNGramSize() const { return m_ngram_size; }
	
	// N-gram types
	enum NGramType {
		NGRAM_CHARACTER = 0,	// Character-based n-grams
		NGRAM_WORD = 1,			// Word-based n-grams
		NGRAM_MIXED = 2			// Mixed character/word n-grams
	};
	
	void setNGramType(NGramType type);
	NGramType getNGramType() const { return m_ngram_type; }
	
private:
	USHORT m_ngram_size;
	NGramType m_ngram_type;
	
	TokenList generateCharacterNGrams(const Token& token) const;
	TokenList generateWordNGrams(const TokenList& tokens) const;
};

//----------------------------
// Phonetic Matching
//----------------------------
class PhoneticProcessor
{
public:
	enum PhoneticAlgorithm {
		PHONETIC_SOUNDEX = 0,		// Traditional Soundex
		PHONETIC_METAPHONE = 1,		// Metaphone
		PHONETIC_DOUBLE_METAPHONE = 2, // Double Metaphone
		PHONETIC_NYSIIS = 3			// NYSIIS
	};
	
	PhoneticProcessor(PhoneticAlgorithm algorithm = PHONETIC_SOUNDEX);
	~PhoneticProcessor();
	
	// Phonetic encoding
	ScratchBird::string encode(const Token& token) const;
	ScratchBird::string encode(const ScratchBird::string& word) const;
	
	// Phonetic matching
	bool matches(const Token& token1, const Token& token2) const;
	double similarity(const Token& token1, const Token& token2) const;
	
	// Configuration
	void setAlgorithm(PhoneticAlgorithm algorithm);
	PhoneticAlgorithm getAlgorithm() const { return m_algorithm; }
	
private:
	PhoneticAlgorithm m_algorithm;
	
	// Algorithm implementations
	ScratchBird::string soundex(const ScratchBird::string& word) const;
	ScratchBird::string metaphone(const ScratchBird::string& word) const;
	ScratchBird::string doubleMetaphone(const ScratchBird::string& word) const;
	ScratchBird::string nysiis(const ScratchBird::string& word) const;
};

//----------------------------
// Enhanced GinTokenizer with Advanced Features
//----------------------------
class AdvancedGinTokenizer : public GinTokenizer
{
public:
	AdvancedGinTokenizer(const TokenizerConfig& config = TokenizerConfig());
	virtual ~AdvancedGinTokenizer();
	
	// Enhanced tokenization interface
	TokenList tokenize(const UCHAR* text, USHORT length) override;
	TokenList tokenize(const dsc* value) override;
	TokenList tokenize(const char* text) override;
	
	// Advanced tokenization methods
	TokenList tokenizeWithUnicode(const UCHAR* utf8_text, USHORT length);
	TokenList tokenizeWithLanguage(const UCHAR* text, USHORT length, LanguageProcessor::LanguageCode language);
	TokenList tokenizeWithOptions(const UCHAR* text, USHORT length, const TokenizerConfig& options);
	
	// Configuration management
	void setConfig(const TokenizerConfig& config);
	const TokenizerConfig& getConfig() const { return m_config; }
	
	// Component access
	UnicodeHelper* getUnicodeHelper() { return &m_unicode_helper; }
	LanguageProcessor* getLanguageProcessor() { return m_language_processor; }
	NGramGenerator* getNGramGenerator() { return m_ngram_generator; }
	PhoneticProcessor* getPhoneticProcessor() { return m_phonetic_processor; }
	
	// Advanced processing
	TokenList normalizeTokens(const TokenList& tokens) const;
	TokenList applyLanguageProcessing(const TokenList& tokens) const;
	TokenList generateNGrams(const TokenList& tokens) const;
	TokenList applyPhoneticEncoding(const TokenList& tokens) const;
	
	// Statistics and analysis
	struct TokenizationStats {
		ULONG total_characters;
		ULONG total_tokens;
		ULONG unique_tokens;
		ULONG filtered_tokens;
		ULONG unicode_characters;
		ULONG ascii_characters;
		ULONG stop_words_removed;
		ULONG tokens_stemmed;
		double processing_time_ms;
	};
	
	TokenizationStats getLastTokenizationStats() const { return m_last_stats; }
	void resetStats();
	
private:
	TokenizerConfig m_config;
	UnicodeHelper m_unicode_helper;
	LanguageProcessor* m_language_processor;
	NGramGenerator* m_ngram_generator;
	PhoneticProcessor* m_phonetic_processor;
	
	// Statistics
	mutable TokenizationStats m_last_stats;
	mutable TokenizationStats m_cumulative_stats;
	
	// Token cache for performance
	mutable ScratchBird::GenericMap<ScratchBird::LeftPooledPair<ScratchBird::string, TokenList>> m_token_cache;
	mutable ULONG m_cache_hits;
	mutable ULONG m_cache_misses;
	
	// Core tokenization methods
	TokenList unicodeTokenize(const UCHAR* utf8_text, USHORT length);
	TokenList languageTokenize(const UCHAR* text, USHORT length);
	
	// Processing pipeline
	void applyNormalization(TokenList& tokens) const;
	void applyCaseProcessing(TokenList& tokens) const;
	void applyLengthFiltering(TokenList& tokens) const;
	void applyStopWordFiltering(TokenList& tokens) const;
	void applyStemming(TokenList& tokens) const;
	void applyNGramGeneration(TokenList& tokens) const;
	void applyPhonetic(TokenList& tokens) const;
	
	// Unicode processing helpers
	bool isTokenBoundary(ULONG current_char, ULONG next_char) const;
	TokenList splitOnBoundaries(const UCHAR* utf8_text, USHORT length) const;
	Token normalizeToken(const Token& token) const;
	
	// Performance optimization
	TokenList* getCachedTokens(const ScratchBird::string& text) const;
	void cacheTokens(const ScratchBird::string& text, const TokenList& tokens) const;
	void cleanupCache();
	
	// Statistics helpers
	void updateStats(const TokenList& tokens, double processing_time) const;
	void incrementStats(TokenizationStats& stats, const TokenList& tokens) const;
};

//----------------------------
// Tokenizer Factory
//----------------------------
class TokenizerFactory
{
public:
	// Create tokenizers based on configuration
	static GinTokenizer* createTokenizer(const TokenizerConfig& config);
	static GinTokenizer* createSimpleTokenizer();
	static GinTokenizer* createStandardTokenizer();
	static GinTokenizer* createAdvancedTokenizer();
	static GinTokenizer* createLanguageTokenizer(LanguageProcessor::LanguageCode language);
	
	// Configuration presets
	static TokenizerConfig getDefaultConfig();
	static TokenizerConfig getPerformanceConfig();
	static TokenizerConfig getQualityConfig();
	static TokenizerConfig getLanguageConfig(LanguageProcessor::LanguageCode language);
	
	// Tokenizer capabilities
	static bool supportsUnicode(GinTokenizer::TokenizerType type);
	static bool supportsLanguage(GinTokenizer::TokenizerType type);
	static bool supportsAdvancedFeatures(GinTokenizer::TokenizerType type);
};

} // namespace Jrd

#endif // JRD_GIN_TOKENIZER_H