/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		AdvancedGinTokenizer.cpp  
 *	DESCRIPTION:	Advanced GIN tokenizer with Unicode and language support
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

#include "scratchbird.h"
#include "../jrd/GinTokenizer.h"
#include "../jrd/GinIndex.h"
#include "../jrd/constants.h"
#include "../common/gdsassert.h"
#include <chrono>
#include <algorithm>
#include <cstring>

using namespace ScratchBird;
using namespace Jrd;
using namespace std::chrono;

namespace Jrd {

//----------------------------
// AdvancedGinTokenizer Implementation
//----------------------------

AdvancedGinTokenizer::AdvancedGinTokenizer(const TokenizerConfig& config)
	: GinTokenizer(config.type),
	  m_config(config),
	  m_language_processor(nullptr),
	  m_ngram_generator(nullptr),
	  m_phonetic_processor(nullptr),
	  m_cache_hits(0),
	  m_cache_misses(0)
{
	// Validate configuration
	m_config.validate();
	
	// Initialize language processor if needed
	if (m_config.enable_stop_words || m_config.enable_stemming || m_config.enable_language_rules) {
		m_language_processor = FB_NEW_POOL(getDefaultMemoryPool()) LanguageProcessor(m_config.language);
	}
	
	// Initialize N-gram generator if needed
	if (m_config.enable_ngrams) {
		m_ngram_generator = FB_NEW_POOL(getDefaultMemoryPool()) NGramGenerator(m_config.ngram_size);
	}
	
	// Initialize phonetic processor if needed
	if (m_config.enable_phonetic) {
		m_phonetic_processor = FB_NEW_POOL(getDefaultMemoryPool()) PhoneticProcessor();
	}
	
	// Initialize statistics
	resetStats();
}

AdvancedGinTokenizer::~AdvancedGinTokenizer()
{
	// Clean up components
	if (m_language_processor) {
		delete m_language_processor;
		m_language_processor = nullptr;
	}
	
	if (m_ngram_generator) {
		delete m_ngram_generator;
		m_ngram_generator = nullptr;
	}
	
	if (m_phonetic_processor) {
		delete m_phonetic_processor;
		m_phonetic_processor = nullptr;
	}
	
	// Clean up cache
	cleanupCache();
}

TokenList AdvancedGinTokenizer::tokenize(const UCHAR* text, USHORT length)
{
	fb_assert(text != nullptr);
	
	auto start_time = high_resolution_clock::now();
	
	if (length == 0) {
		m_last_stats.total_characters = 0;
		m_last_stats.total_tokens = 0;
		m_last_stats.processing_time_ms = 0.0;
		return TokenList();
	}
	
	// Check cache first if enabled
	if (m_config.enable_token_caching) {
		ScratchBird::string text_key(reinterpret_cast<const char*>(text), length);
		TokenList* cached = getCachedTokens(text_key);
		if (cached) {
			m_cache_hits++;
			auto end_time = high_resolution_clock::now();
			double processing_time = duration_cast<microseconds>(end_time - start_time).count() / 1000.0;
			updateStats(*cached, processing_time);
			return *cached;
		}
		m_cache_misses++;
	}
	
	TokenList tokens;
	
	// Choose tokenization method based on configuration
	if (m_config.normalization != UnicodeHelper::NORM_NONE || m_config.handle_diacritics) {
		tokens = unicodeTokenize(text, length);
	} else if (m_config.language != LanguageProcessor::LANG_NEUTRAL) {
		tokens = languageTokenize(text, length);
	} else {
		// Fall back to base class implementation
		tokens = GinTokenizer::tokenize(text, length);
	}
	
	// Apply processing pipeline
	applyNormalization(tokens);
	applyCaseProcessing(tokens);
	applyLengthFiltering(tokens);
	
	if (m_config.enable_stop_words) {
		applyStopWordFiltering(tokens);
	}
	
	if (m_config.enable_stemming) {
		applyStemming(tokens);
	}
	
	if (m_config.enable_ngrams) {
		applyNGramGeneration(tokens);
	}
	
	if (m_config.enable_phonetic) {
		applyPhonetic(tokens);
	}
	
	// Cache results if enabled
	if (m_config.enable_token_caching && m_token_cache.count() < m_config.max_token_cache_size) {
		ScratchBird::string text_key(reinterpret_cast<const char*>(text), length);
		cacheTokens(text_key, tokens);
	}
	
	// Update statistics
	auto end_time = high_resolution_clock::now();
	double processing_time = duration_cast<microseconds>(end_time - start_time).count() / 1000.0;
	updateStats(tokens, processing_time);
	
	return tokens;
}

TokenList AdvancedGinTokenizer::tokenize(const dsc* value)
{
	fb_assert(value != nullptr);
	
	// Delegate to base class for data type handling, then enhance
	TokenList base_tokens = GinTokenizer::tokenize(value);
	
	if (base_tokens.isEmpty()) {
		return base_tokens;
	}
	
	// Apply advanced processing to base tokens
	TokenList enhanced_tokens = base_tokens;
	
	applyNormalization(enhanced_tokens);
	applyCaseProcessing(enhanced_tokens);
	applyLengthFiltering(enhanced_tokens);
	
	if (m_config.enable_stop_words) {
		applyStopWordFiltering(enhanced_tokens);
	}
	
	if (m_config.enable_stemming) {
		applyStemming(enhanced_tokens);
	}
	
	if (m_config.enable_ngrams) {
		applyNGramGeneration(enhanced_tokens);
	}
	
	if (m_config.enable_phonetic) {
		applyPhonetic(enhanced_tokens);
	}
	
	return enhanced_tokens;
}

TokenList AdvancedGinTokenizer::tokenize(const char* text)
{
	fb_assert(text != nullptr);
	
	USHORT length = strlen(text);
	return tokenize(reinterpret_cast<const UCHAR*>(text), length);
}

TokenList AdvancedGinTokenizer::tokenizeWithUnicode(const UCHAR* utf8_text, USHORT length)
{
	return unicodeTokenize(utf8_text, length);
}

TokenList AdvancedGinTokenizer::tokenizeWithLanguage(const UCHAR* text, USHORT length, LanguageProcessor::LanguageCode language)
{
	// Temporarily change language setting
	LanguageProcessor::LanguageCode old_language = m_config.language;
	m_config.language = language;
	
	if (m_language_processor) {
		m_language_processor->setLanguage(language);
	}
	
	TokenList result = tokenize(text, length);
	
	// Restore original language setting
	m_config.language = old_language;
	if (m_language_processor) {
		m_language_processor->setLanguage(old_language);
	}
	
	return result;
}

TokenList AdvancedGinTokenizer::tokenizeWithOptions(const UCHAR* text, USHORT length, const TokenizerConfig& options)
{
	// Save current configuration
	TokenizerConfig old_config = m_config;
	
	// Apply new configuration
	setConfig(options);
	
	TokenList result = tokenize(text, length);
	
	// Restore original configuration
	setConfig(old_config);
	
	return result;
}

void AdvancedGinTokenizer::setConfig(const TokenizerConfig& config)
{
	m_config = config;
	m_config.validate();
	
	// Reinitialize components if needed
	if (m_config.enable_stop_words || m_config.enable_stemming || m_config.enable_language_rules) {
		if (!m_language_processor) {
			m_language_processor = FB_NEW_POOL(getDefaultMemoryPool()) LanguageProcessor(m_config.language);
		} else {
			m_language_processor->setLanguage(m_config.language);
		}
	} else {
		if (m_language_processor) {
			delete m_language_processor;
			m_language_processor = nullptr;
		}
	}
	
	if (m_config.enable_ngrams) {
		if (!m_ngram_generator) {
			m_ngram_generator = FB_NEW_POOL(getDefaultMemoryPool()) NGramGenerator(m_config.ngram_size);
		} else {
			m_ngram_generator->setNGramSize(m_config.ngram_size);
		}
	} else {
		if (m_ngram_generator) {
			delete m_ngram_generator;
			m_ngram_generator = nullptr;
		}
	}
	
	if (m_config.enable_phonetic) {
		if (!m_phonetic_processor) {
			m_phonetic_processor = FB_NEW_POOL(getDefaultMemoryPool()) PhoneticProcessor();
		}
	} else {
		if (m_phonetic_processor) {
			delete m_phonetic_processor;
			m_phonetic_processor = nullptr;
		}
	}
	
	// Update base tokenizer settings
	setMinTokenLength(m_config.min_token_length);
	setMaxTokenLength(m_config.max_token_length);
	enableStopWords(m_config.enable_stop_words);
	enableStemming(m_config.enable_stemming);
}

TokenList AdvancedGinTokenizer::normalizeTokens(const TokenList& tokens) const
{
	TokenList normalized;
	normalized.grow(tokens.getCount());
	
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		Token normalized_token = normalizeToken(tokens[i]);
		normalized.add(normalized_token);
	}
	
	return normalized;
}

TokenList AdvancedGinTokenizer::applyLanguageProcessing(const TokenList& tokens) const
{
	if (!m_language_processor) {
		return tokens;
	}
	
	return m_language_processor->applyLanguageSpecificRules(tokens);
}

TokenList AdvancedGinTokenizer::generateNGrams(const TokenList& tokens) const
{
	if (!m_ngram_generator) {
		return tokens;
	}
	
	return m_ngram_generator->generateNGrams(tokens);
}

TokenList AdvancedGinTokenizer::applyPhoneticEncoding(const TokenList& tokens) const
{
	if (!m_phonetic_processor) {
		return tokens;
	}
	
	TokenList phonetic;
	phonetic.grow(tokens.getCount());
	
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		ScratchBird::string phonetic_code = m_phonetic_processor->encode(tokens[i]);
		
		if (!phonetic_code.empty() && phonetic_code.length() <= GIN_MAX_TOKEN_LENGTH) {
			Token phonetic_token(reinterpret_cast<const UCHAR*>(phonetic_code.c_str()), phonetic_code.length());
			phonetic.add(phonetic_token);
		}
	}
	
	return phonetic;
}

void AdvancedGinTokenizer::resetStats()
{
	memset(&m_last_stats, 0, sizeof(m_last_stats));
	memset(&m_cumulative_stats, 0, sizeof(m_cumulative_stats));
	m_cache_hits = 0;
	m_cache_misses = 0;
}

TokenList AdvancedGinTokenizer::unicodeTokenize(const UCHAR* utf8_text, USHORT length)
{
	fb_assert(utf8_text != nullptr);
	
	// Validate UTF-8 sequence
	if (!UnicodeHelper::isValidUtf8Sequence(utf8_text, length)) {
		// Fall back to simple tokenization for invalid UTF-8
		return simpleTokenize(utf8_text, length);
	}
	
	// Split on Unicode word boundaries
	return splitOnBoundaries(utf8_text, length);
}

TokenList AdvancedGinTokenizer::languageTokenize(const UCHAR* text, USHORT length)
{
	// Start with Unicode tokenization
	TokenList tokens = unicodeTokenize(text, length);
	
	// Apply language-specific processing
	if (m_language_processor) {
		tokens = m_language_processor->applyLanguageSpecificRules(tokens);
	}
	
	return tokens;
}

void AdvancedGinTokenizer::applyNormalization(TokenList& tokens) const
{
	if (m_config.normalization == UnicodeHelper::NORM_NONE) {
		return;
	}
	
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		tokens[i] = normalizeToken(tokens[i]);
	}
}

void AdvancedGinTokenizer::applyCaseProcessing(TokenList& tokens) const
{
	if (!m_config.fold_case && !m_config.preserve_case) {
		return;
	}
	
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		if (m_config.fold_case) {
			// Convert to lowercase for case-insensitive matching
			for (USHORT j = 0; j < tokens[i].length; j++) {
				USHORT bytes_consumed;
				ULONG unicode_char = UnicodeHelper::getCharacter(&tokens[i].data[j], bytes_consumed);
				ULONG lower_char = UnicodeHelper::toLower(unicode_char);
				
				if (lower_char != unicode_char && bytes_consumed == 1) {
					// Simple ASCII case conversion
					tokens[i].data[j] = static_cast<UCHAR>(lower_char);
				}
			}
		}
	}
}

void AdvancedGinTokenizer::applyLengthFiltering(TokenList& tokens) const
{
	// Remove tokens that don't meet length requirements
	for (FB_SIZE_T i = tokens.getCount(); i > 0; i--) {
		const Token& token = tokens[i - 1];
		
		if (token.length < m_config.min_token_length || token.length > m_config.max_token_length) {
			tokens.remove(i - 1);
		}
	}
}

void AdvancedGinTokenizer::applyStopWordFiltering(TokenList& tokens) const
{
	if (!m_language_processor) {
		return;
	}
	
	ULONG removed_count = 0;
	
	for (FB_SIZE_T i = tokens.getCount(); i > 0; i--) {
		if (m_language_processor->isStopWord(tokens[i - 1])) {
			tokens.remove(i - 1);
			removed_count++;
		}
	}
	
	m_last_stats.stop_words_removed = removed_count;
}

void AdvancedGinTokenizer::applyStemming(TokenList& tokens) const
{
	if (!m_language_processor) {
		return;
	}
	
	ULONG stemmed_count = 0;
	
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		Token original = tokens[i];
		Token stemmed = m_language_processor->applyStemming(original);
		
		if (!(stemmed == original)) {
			tokens[i] = stemmed;
			stemmed_count++;
		}
	}
	
	m_last_stats.tokens_stemmed = stemmed_count;
}

void AdvancedGinTokenizer::applyNGramGeneration(TokenList& tokens) const
{
	if (!m_ngram_generator) {
		return;
	}
	
	TokenList ngram_tokens;
	
	// Generate character n-grams for each token
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		TokenList token_ngrams = m_ngram_generator->generateNGrams(tokens[i]);
		
		for (FB_SIZE_T j = 0; j < token_ngrams.getCount(); j++) {
			ngram_tokens.add(token_ngrams[j]);
		}
	}
	
	// Generate word n-grams
	TokenList word_ngrams = m_ngram_generator->generateNGrams(tokens);
	for (FB_SIZE_T i = 0; i < word_ngrams.getCount(); i++) {
		ngram_tokens.add(word_ngrams[i]);
	}
	
	// Add n-grams to original tokens
	for (FB_SIZE_T i = 0; i < ngram_tokens.getCount(); i++) {
		tokens.add(ngram_tokens[i]);
	}
}

void AdvancedGinTokenizer::applyPhonetic(TokenList& tokens) const
{
	if (!m_phonetic_processor) {
		return;
	}
	
	TokenList phonetic_tokens = applyPhoneticEncoding(tokens);
	
	// Add phonetic codes as additional tokens
	for (FB_SIZE_T i = 0; i < phonetic_tokens.getCount(); i++) {
		tokens.add(phonetic_tokens[i]);
	}
}

bool AdvancedGinTokenizer::isTokenBoundary(ULONG current_char, ULONG next_char) const
{
	// Determine if there should be a token boundary between two characters
	bool current_is_letter = UnicodeHelper::isLetterOrDigit(current_char);
	bool next_is_letter = UnicodeHelper::isLetterOrDigit(next_char);
	
	bool current_is_whitespace = UnicodeHelper::isWhitespace(current_char);
	bool next_is_whitespace = UnicodeHelper::isWhitespace(next_char);
	
	bool current_is_punct = UnicodeHelper::isPunctuation(current_char);
	bool next_is_punct = UnicodeHelper::isPunctuation(next_char);
	
	// Boundary conditions
	if (current_is_whitespace || next_is_whitespace) {
		return true;  // Whitespace always creates boundaries
	}
	
	if (current_is_letter && next_is_punct) {
		return true;  // Letter followed by punctuation
	}
	
	if (current_is_punct && next_is_letter) {
		return true;  // Punctuation followed by letter
	}
	
	if (current_is_punct && next_is_punct) {
		return true;  // Separate punctuation marks
	}
	
	return false;  // No boundary
}

TokenList AdvancedGinTokenizer::splitOnBoundaries(const UCHAR* utf8_text, USHORT length) const
{
	TokenList tokens;
	
	USHORT token_start = 0;
	bool in_token = false;
	USHORT i = 0;
	
	while (i < length) {
		USHORT bytes_consumed;
		ULONG current_char = UnicodeHelper::getCharacter(&utf8_text[i], bytes_consumed);
		
		if (bytes_consumed == 0) {
			i++;  // Skip invalid bytes
			continue;
		}
		
		ULONG next_char = 0;
		USHORT next_bytes_consumed = 0;
		if (i + bytes_consumed < length) {
			next_char = UnicodeHelper::getCharacter(&utf8_text[i + bytes_consumed], next_bytes_consumed);
		}
		
		bool is_token_char = UnicodeHelper::isLetterOrDigit(current_char);
		bool is_boundary = (next_bytes_consumed > 0) ? isTokenBoundary(current_char, next_char) : true;
		
		if (is_token_char && !in_token) {
			// Start of new token
			token_start = i;
			in_token = true;
		}
		else if (in_token && (is_boundary || !is_token_char)) {
			// End of current token
			USHORT token_length = i - token_start + bytes_consumed;
			
			if (token_length >= m_config.min_token_length && token_length <= m_config.max_token_length) {
				Token token(&utf8_text[token_start], token_length);
				tokens.add(token);
				
				if (tokens.getCount() >= m_config.max_tokens_per_document) {
					break;
				}
			}
			
			in_token = false;
		}
		
		i += bytes_consumed;
	}
	
	return tokens;
}

Token AdvancedGinTokenizer::normalizeToken(const Token& token) const
{
	if (m_config.normalization == UnicodeHelper::NORM_NONE) {
		return token;
	}
	
	ScratchBird::string normalized = UnicodeHelper::normalize(
		token.data, token.length, m_config.normalization);
	
	if (normalized.length() <= GIN_MAX_TOKEN_LENGTH) {
		return Token(reinterpret_cast<const UCHAR*>(normalized.c_str()), normalized.length());
	}
	
	return token;  // Return original if normalization makes it too long
}

TokenList* AdvancedGinTokenizer::getCachedTokens(const ScratchBird::string& text) const
{
	GenericMap<Pair<ScratchBird::string, TokenList>>::const_iterator it = 
		m_token_cache.locate(Pair<ScratchBird::string, TokenList>(text, TokenList()));
	
	if (it != m_token_cache.end()) {
		return const_cast<TokenList*>(&it->second);
	}
	
	return nullptr;
}

void AdvancedGinTokenizer::cacheTokens(const ScratchBird::string& text, const TokenList& tokens) const
{
	if (m_token_cache.count() >= m_config.max_token_cache_size) {
		// Remove oldest entries (simplified LRU)
		cleanupCache();
	}
	
	m_token_cache.put(Pair<ScratchBird::string, TokenList>(text, tokens));
}

void AdvancedGinTokenizer::cleanupCache()
{
	// Simple cleanup - remove all cached entries
	// TODO: Implement proper LRU eviction
	m_token_cache.clear();
}

void AdvancedGinTokenizer::updateStats(const TokenList& tokens, double processing_time) const
{
	m_last_stats.total_tokens = tokens.getCount();
	m_last_stats.processing_time_ms = processing_time;
	
	// Count unique tokens
	GenericMap<Pair<ScratchBird::string, bool>> unique_tokens;
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		ScratchBird::string token_str(tokens[i].c_str());
		unique_tokens.put(Pair<ScratchBird::string, bool>(token_str, true));
	}
	m_last_stats.unique_tokens = unique_tokens.count();
	
	// Update cumulative statistics
	incrementStats(m_cumulative_stats, tokens);
	m_cumulative_stats.processing_time_ms += processing_time;
}

void AdvancedGinTokenizer::incrementStats(TokenizationStats& stats, const TokenList& tokens) const
{
	stats.total_tokens += tokens.getCount();
	
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		stats.total_characters += tokens[i].length;
		
		// Count Unicode vs ASCII characters
		for (USHORT j = 0; j < tokens[i].length; j++) {
			if (tokens[i].data[j] > 127) {
				stats.unicode_characters++;
			} else {
				stats.ascii_characters++;
			}
		}
	}
}

} // namespace Jrd