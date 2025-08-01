/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		GinAdvancedFeatures.cpp
 *	DESCRIPTION:	Advanced GIN index features implementation
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
 * 2025.07.23 - ScratchBird Advanced GIN Features Implementation
 */

#include "firebird.h"
#include "GinAdvancedFeatures.h"
#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../jrd/met.h"
#include "../common/StatusArg.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <ctime>
#include <cstring>
#include <cctype>

using namespace Jrd;
using namespace ScratchBird;

// Static initialization
GenericMap<USHORT, GinAdvancedTextProcessor*> GinAdvancedFeaturesManager::s_processors(*getDefaultMemoryPool());
bool GinAdvancedFeaturesManager::s_initialized = false;

//----------------------------
// GinLanguageDetector Implementation
//----------------------------

GinLanguageDetector::GinLanguageDetector()
	: m_language_profiles(*getDefaultMemoryPool()),
	  m_detection_threshold(0.7),
	  m_pool(*getDefaultMemoryPool())
{
	initializeLanguageProfiles();
}

GinLanguageDetector::~GinLanguageDetector()
{
}

GinLanguageCode GinLanguageDetector::detectLanguage(const string& text)
{
	if (text.length() < 10) {
		return GIN_LANG_GENERIC; // Too short for reliable detection
	}
	
	GinLanguageCode best_language = GIN_LANG_ENGLISH;
	double best_score = 0.0;
	
	// Test against all enabled language profiles
	for (size_t i = 0; i < m_language_profiles.getCount(); i++) {
		const LanguageProfile& profile = m_language_profiles[i];
		if (!profile.enabled) continue;
		
		double score = 0.0;
		score += calculateTrigramScore(text, profile) * 0.4;
		score += calculateCharacterScore(text, profile) * 0.3;
		score += calculateWordScore(text, profile) * 0.3;
		
		if (score > best_score && score >= profile.detection_threshold) {
			best_score = score;
			best_language = profile.language_code;
		}
	}
	
	return (best_score >= m_detection_threshold) ? best_language : GIN_LANG_GENERIC;
}

double GinLanguageDetector::calculateLanguageConfidence(const string& text, GinLanguageCode language)
{
	for (size_t i = 0; i < m_language_profiles.getCount(); i++) {
		const LanguageProfile& profile = m_language_profiles[i];
		if (profile.language_code == language) {
			double score = 0.0;
			score += calculateTrigramScore(text, profile) * 0.4;
			score += calculateCharacterScore(text, profile) * 0.3;
			score += calculateWordScore(text, profile) * 0.3;
			return score;
		}
	}
	return 0.0;
}

ObjectsArray<GinLanguageCode> GinLanguageDetector::detectMultipleLanguages(const string& text)
{
	ObjectsArray<GinLanguageCode> languages(m_pool);
	
	// Simple implementation - split text and detect language of each segment
	// More sophisticated implementation would use sentence/paragraph boundaries
	size_t segment_size = text.length() / 4; // Test 4 segments
	if (segment_size < 20) segment_size = text.length();
	
	for (size_t i = 0; i < text.length(); i += segment_size) {
		size_t end = std::min(i + segment_size, text.length());
		string segment = text.substr(i, end - i);
		
		GinLanguageCode lang = detectLanguage(segment);
		if (lang != GIN_LANG_GENERIC) {
			// Check if language already detected
			bool found = false;
			for (size_t j = 0; j < languages.getCount(); j++) {
				if (languages[j] == lang) {
					found = true;
					break;
				}
			}
			if (!found) {
				languages.add(lang);
			}
		}
	}
	
	return languages;
}

bool GinLanguageDetector::isMultiLanguageText(const string& text)
{
	ObjectsArray<GinLanguageCode> languages = detectMultipleLanguages(text);
	return languages.getCount() > 1;
}

string GinLanguageDetector::normalizeTextForLanguage(const string& text, GinLanguageCode language)
{
	string normalized = text;
	
	// Language-specific normalization
	switch (language) {
	case GIN_LANG_GERMAN:
		// Convert German umlauts
		// ä -> ae, ö -> oe, ü -> ue, ß -> ss
		// Simplified implementation
		break;
		
	case GIN_LANG_FRENCH:
		// Remove French accents for normalization
		// More sophisticated implementation would preserve semantic differences
		break;
		
	case GIN_LANG_RUSSIAN:
		// Cyrillic normalization
		break;
		
	case GIN_LANG_CHINESE:
	case GIN_LANG_JAPANESE:
		// CJK normalization - complex topic, simplified here
		break;
		
	default:
		// Generic normalization - convert to lowercase
		std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
		break;
	}
	
	return normalized;
}

bool GinLanguageDetector::requiresSpecialTokenization(GinLanguageCode language)
{
	switch (language) {
	case GIN_LANG_CHINESE:
	case GIN_LANG_JAPANESE:
	case GIN_LANG_KOREAN:
		return true; // Require segmentation algorithms
	default:
		return false;
	}
}

void GinLanguageDetector::setDetectionThreshold(double threshold)
{
	m_detection_threshold = threshold;
}

void GinLanguageDetector::enableLanguage(GinLanguageCode language, bool enabled)
{
	for (size_t i = 0; i < m_language_profiles.getCount(); i++) {
		if (m_language_profiles[i].language_code == language) {
			m_language_profiles[i].enabled = enabled;
			break;
		}
	}
}

void GinLanguageDetector::initializeLanguageProfiles()
{
	// Initialize basic language profiles
	// In a full implementation, these would be loaded from external files
	
	// English profile
	LanguageProfile english_profile(m_pool);
	english_profile.language_code = GIN_LANG_ENGLISH;
	english_profile.detection_threshold = 0.6;
	english_profile.common_words.add("the");
	english_profile.common_words.add("and");
	english_profile.common_words.add("that");
	english_profile.common_words.add("have");
	english_profile.common_words.add("for");
	m_language_profiles.add(english_profile);
	
	// Spanish profile
	LanguageProfile spanish_profile(m_pool);
	spanish_profile.language_code = GIN_LANG_SPANISH;
	spanish_profile.detection_threshold = 0.6;
	spanish_profile.common_words.add("que");
	spanish_profile.common_words.add("de");
	spanish_profile.common_words.add("la");
	spanish_profile.common_words.add("el");
	spanish_profile.common_words.add("en");
	m_language_profiles.add(spanish_profile);
	
	// French profile
	LanguageProfile french_profile(m_pool);
	french_profile.language_code = GIN_LANG_FRENCH;
	french_profile.detection_threshold = 0.6;
	french_profile.common_words.add("de");
	french_profile.common_words.add("le");
	french_profile.common_words.add("et");
	french_profile.common_words.add("à");
	french_profile.common_words.add("un");
	m_language_profiles.add(french_profile);
	
	// German profile
	LanguageProfile german_profile(m_pool);
	german_profile.language_code = GIN_LANG_GERMAN;
	german_profile.detection_threshold = 0.6;
	german_profile.common_words.add("der");
	german_profile.common_words.add("die");
	german_profile.common_words.add("und");
	german_profile.common_words.add("in");
	german_profile.common_words.add("den");
	m_language_profiles.add(german_profile);
}

double GinLanguageDetector::calculateTrigramScore(const string& text, const LanguageProfile& profile)
{
	ObjectsArray<string> trigrams = extractTrigrams(text);
	if (trigrams.isEmpty()) return 0.0;
	
	double score = 0.0;
	ULONG matches = 0;
	
	for (size_t i = 0; i < trigrams.getCount(); i++) {
		if (profile.trigram_frequencies.exist(trigrams[i])) {
			score += profile.trigram_frequencies[trigrams[i]];
			matches++;
		}
	}
	
	return matches > 0 ? score / matches : 0.0;
}

double GinLanguageDetector::calculateCharacterScore(const string& text, const LanguageProfile& profile)
{
	if (text.empty()) return 0.0;
	
	double score = 0.0;
	ULONG char_count = 0;
	
	for (size_t i = 0; i < text.length(); i++) {
		string char_str(1, text[i]);
		if (profile.character_frequencies.exist(char_str)) {
			score += profile.character_frequencies[char_str];
			char_count++;
		}
	}
	
	return char_count > 0 ? score / char_count : 0.0;
}

double GinLanguageDetector::calculateWordScore(const string& text, const LanguageProfile& profile)
{
	// Simple word splitting for scoring
	ObjectsArray<string> words(m_pool);
	string current_word;
	
	for (size_t i = 0; i < text.length(); i++) {
		if (std::isalpha(text[i])) {
			current_word += std::tolower(text[i]);
		} else {
			if (!current_word.empty()) {
				words.add(current_word);
				current_word.clear();
			}
		}
	}
	if (!current_word.empty()) {
		words.add(current_word);
	}
	
	if (words.isEmpty()) return 0.0;
	
	ULONG matches = 0;
	for (size_t i = 0; i < words.getCount(); i++) {
		for (size_t j = 0; j < profile.common_words.getCount(); j++) {
			if (words[i] == profile.common_words[j]) {
				matches++;
				break;
			}
		}
	}
	
	return static_cast<double>(matches) / words.getCount();
}

ObjectsArray<string> GinLanguageDetector::extractTrigrams(const string& text)
{
	ObjectsArray<string> trigrams(m_pool);
	
	if (text.length() < 3) return trigrams;
	
	for (size_t i = 0; i <= text.length() - 3; i++) {
		string trigram = text.substr(i, 3);
		// Convert to lowercase for consistency
		std::transform(trigram.begin(), trigram.end(), trigram.begin(), ::tolower);
		trigrams.add(trigram);
	}
	
	return trigrams;
}

//----------------------------
// GinStemmingEngine Implementation
//----------------------------

GinStemmingEngine::GinStemmingEngine(MemoryPool& pool)
	: m_pool(pool), m_stemming_cache(pool), m_stemming_rules(pool),
	  m_cache_size_limit(10000), m_cache_hits(0), m_cache_misses(0)
{
	initializeStemmingRules();
}

GinStemmingEngine::~GinStemmingEngine()
{
}

string GinStemmingEngine::stemTerm(const string& term, GinLanguageCode language, 
								   GinStemmingAlgorithm algorithm)
{
	if (term.length() < 3) {
		return term; // Too short to stem
	}
	
	// Check cache first
	string cache_key = createCacheKey(term, language, algorithm);
	if (m_stemming_cache.exist(cache_key)) {
		m_cache_hits++;
		StemmingCacheEntry& entry = m_stemming_cache[cache_key];
		entry.access_count++;
		entry.last_access = time(nullptr);
		return entry.stemmed_term;
	}
	
	m_cache_misses++;
	
	// Perform stemming based on algorithm
	string stemmed_term;
	switch (algorithm) {
	case GIN_STEM_PORTER:
		stemmed_term = porterStem(term, language);
		break;
	case GIN_STEM_SNOWBALL:
		stemmed_term = snowballStem(term, language);
		break;
	case GIN_STEM_LOVINS:
		stemmed_term = lovinstem(term, language);
		break;
	case GIN_STEM_PAICE_HUSK:
		stemmed_term = paiceHuskStem(term, language);
		break;
	default:
		stemmed_term = term; // No stemming
		break;
	}
	
	// Add to cache
	addToCache(term, stemmed_term, language, algorithm);
	
	return stemmed_term;
}

ObjectsArray<string> GinStemmingEngine::stemTerms(const ObjectsArray<string>& terms,
												  GinLanguageCode language, GinStemmingAlgorithm algorithm)
{
	ObjectsArray<string> stemmed_terms(m_pool);
	
	for (size_t i = 0; i < terms.getCount(); i++) {
		string stemmed = stemTerm(terms[i], language, algorithm);
		stemmed_terms.add(stemmed);
	}
	
	return stemmed_terms;
}

string GinStemmingEngine::porterStem(const string& term, GinLanguageCode language)
{
	// Simplified Porter stemmer implementation
	// Full implementation would have complete rule sets for each language
	
	string word = term;
	std::transform(word.begin(), word.end(), word.begin(), ::tolower);
	
	if (word.length() < 3) return word;
	
	// Step 1a: Remove plural suffixes
	if (word.length() > 4 && word.substr(word.length() - 4) == "sses") {
		word = word.substr(0, word.length() - 2); // sses -> ss
	}
	else if (word.length() > 3 && word.substr(word.length() - 3) == "ies") {
		word = word.substr(0, word.length() - 2); // ies -> i
	}
	else if (word.length() > 1 && word[word.length() - 1] == 's' && 
			 word[word.length() - 2] != 's') {
		word = word.substr(0, word.length() - 1); // s -> (empty)
	}
	
	// Step 1b: Remove -ed, -ing endings
	if (word.length() > 3 && word.substr(word.length() - 3) == "eed") {
		// Only remove if measure > 0 (simplified check)
		if (word.length() > 4) {
			word = word.substr(0, word.length() - 1); // eed -> ee
		}
	}
	else if (word.length() > 2 && word.substr(word.length() - 2) == "ed") {
		// Check if stem contains vowel (simplified)
		string stem = word.substr(0, word.length() - 2);
		if (stem.find_first_of("aeiou") != string::npos) {
			word = stem;
		}
	}
	else if (word.length() > 3 && word.substr(word.length() - 3) == "ing") {
		// Check if stem contains vowel (simplified)
		string stem = word.substr(0, word.length() - 3);
		if (stem.find_first_of("aeiou") != string::npos) {
			word = stem;
		}
	}
	
	// Additional steps would follow in a complete implementation
	
	return word;
}

string GinStemmingEngine::snowballStem(const string& term, GinLanguageCode language)
{
	// Snowball stemmer is an improvement over Porter
	// This is a placeholder implementation
	return porterStem(term, language);
}

string GinStemmingEngine::lovinstem(const string& term, GinLanguageCode language)
{
	// Lovins stemmer implementation placeholder
	return porterStem(term, language);
}

string GinStemmingEngine::paiceHuskStem(const string& term, GinLanguageCode language)
{
	// Paice-Husk stemmer implementation placeholder
	return porterStem(term, language);
}

bool GinStemmingEngine::isStemmingSupported(GinLanguageCode language, GinStemmingAlgorithm algorithm)
{
	// Check if we have rules for this language/algorithm combination
	for (size_t i = 0; i < m_stemming_rules.getCount(); i++) {
		if (m_stemming_rules[i].language == language && 
			m_stemming_rules[i].algorithm == algorithm) {
			return true;
		}
	}
	return false;
}

void GinStemmingEngine::configureStemming(const GinAdvancedConfig& config)
{
	m_cache_size_limit = config.stemming_cache_size;
	
	// Load additional rules if needed
	if (config.enable_stemming) {
		loadStemmingRulesForLanguage(config.primary_language, config.stemming_algorithm);
	}
}

void GinStemmingEngine::clearStemmingCache()
{
	m_stemming_cache.clear();
	m_cache_hits = 0;
	m_cache_misses = 0;
}

ULONG GinStemmingEngine::getCacheSize() const
{
	return m_stemming_cache.count();
}

double GinStemmingEngine::getCacheHitRatio() const
{
	ULONG total_requests = m_cache_hits + m_cache_misses;
	return total_requests > 0 ? static_cast<double>(m_cache_hits) / total_requests : 0.0;
}

void GinStemmingEngine::initializeStemmingRules()
{
	// Initialize basic stemming rules for English
	LanguageStemmingRules english_rules(m_pool);
	english_rules.language = GIN_LANG_ENGLISH;
	english_rules.algorithm = GIN_STEM_PORTER;
	
	// Add basic vowels and consonants
	english_rules.vowels.add("a");
	english_rules.vowels.add("e");
	english_rules.vowels.add("i");
	english_rules.vowels.add("o");
	english_rules.vowels.add("u");
	
	// Add some basic stemming rules
	StemmingRule rule1(m_pool);
	rule1.suffix = "ing";
	rule1.replacement = "";
	rule1.min_word_length = 4;
	rule1.remove_only = true;
	english_rules.rules.add(rule1);
	
	StemmingRule rule2(m_pool);
	rule2.suffix = "ed";
	rule2.replacement = "";
	rule2.min_word_length = 3;
	rule2.remove_only = true;
	english_rules.rules.add(rule2);
	
	m_stemming_rules.add(english_rules);
}

string GinStemmingEngine::createCacheKey(const string& term, GinLanguageCode language, 
										 GinStemmingAlgorithm algorithm)
{
	return term + "_" + std::to_string(static_cast<int>(language)) + "_" + 
		   std::to_string(static_cast<int>(algorithm));
}

void GinStemmingEngine::addToCache(const string& original, const string& stemmed,
								   GinLanguageCode language, GinStemmingAlgorithm algorithm)
{
	if (m_stemming_cache.count() >= m_cache_size_limit) {
		evictOldCacheEntries();
	}
	
	string cache_key = createCacheKey(original, language, algorithm);
	StemmingCacheEntry entry(m_pool);
	entry.original_term = original;
	entry.stemmed_term = stemmed;
	entry.language = language;
	entry.algorithm = algorithm;
	entry.access_count = 1;
	entry.last_access = time(nullptr);
	
	m_stemming_cache.put(cache_key, entry);
}

void GinStemmingEngine::evictOldCacheEntries()
{
	// Simple LRU eviction - remove 10% of oldest entries
	ULONG entries_to_remove = m_stemming_cache.count() / 10;
	if (entries_to_remove == 0) entries_to_remove = 1;
	
	// In a full implementation, would maintain a proper LRU structure
	// For now, just clear the cache when it gets too large
	if (m_stemming_cache.count() > m_cache_size_limit * 1.2) {
		clearStemmingCache();
	}
}

//----------------------------
// GinStopWordsManager Implementation
//----------------------------

GinStopWordsManager::GinStopWordsManager(MemoryPool& pool)
	: m_pool(pool), m_stop_word_sets(pool), 
	  m_handling_mode(GIN_STOPWORDS_NONE), m_case_sensitive(false)
{
	initializeDefaultStopWords();
}

GinStopWordsManager::~GinStopWordsManager()
{
}

bool GinStopWordsManager::isStopWord(const string& term, GinLanguageCode language)
{
	StopWordSet* word_set = getStopWordSet(language);
	if (!word_set) return false;
	
	string normalized_term = normalizeStopWord(term);
	return word_set->stop_words.find(normalized_term) != word_set->stop_words.end();
}

ObjectsArray<string> GinStopWordsManager::filterStopWords(const ObjectsArray<string>& terms,
														  GinLanguageCode language)
{
	ObjectsArray<string> filtered_terms(m_pool);
	
	for (size_t i = 0; i < terms.getCount(); i++) {
		if (!isStopWord(terms[i], language)) {
			filtered_terms.add(terms[i]);
		}
	}
	
	return filtered_terms;
}

ObjectsArray<string> GinStopWordsManager::markStopWords(const ObjectsArray<string>& terms,
													   GinLanguageCode language)
{
	ObjectsArray<string> marked_terms(m_pool);
	
	for (size_t i = 0; i < terms.getCount(); i++) {
		if (isStopWord(terms[i], language)) {
			marked_terms.add("STOPWORD:" + terms[i]);
		} else {
			marked_terms.add(terms[i]);
		}
	}
	
	return marked_terms;
}

void GinStopWordsManager::loadStopWordsForLanguage(GinLanguageCode language)
{
	StopWordSet* word_set = getStopWordSet(language);
	if (!word_set) {
		// Create new stop word set
		StopWordSet new_set;
		new_set.language = language;
		new_set.case_sensitive = m_case_sensitive;
		new_set.last_updated = time(nullptr);
		m_stop_word_sets.add(new_set);
		word_set = &m_stop_word_sets[m_stop_word_sets.getCount() - 1];
	}
	
	// Load language-specific stop words
	switch (language) {
	case GIN_LANG_ENGLISH:
		word_set->stop_words.insert("a");
		word_set->stop_words.insert("an");
		word_set->stop_words.insert("and");
		word_set->stop_words.insert("are");
		word_set->stop_words.insert("as");
		word_set->stop_words.insert("at");
		word_set->stop_words.insert("be");
		word_set->stop_words.insert("by");
		word_set->stop_words.insert("for");
		word_set->stop_words.insert("from");
		word_set->stop_words.insert("in");
		word_set->stop_words.insert("is");
		word_set->stop_words.insert("it");
		word_set->stop_words.insert("of");
		word_set->stop_words.insert("on");
		word_set->stop_words.insert("that");
		word_set->stop_words.insert("the");
		word_set->stop_words.insert("to");
		word_set->stop_words.insert("was");
		word_set->stop_words.insert("with");
		break;
		
	case GIN_LANG_SPANISH:
		word_set->stop_words.insert("el");
		word_set->stop_words.insert("la");
		word_set->stop_words.insert("de");
		word_set->stop_words.insert("que");
		word_set->stop_words.insert("y");
		word_set->stop_words.insert("a");
		word_set->stop_words.insert("en");
		word_set->stop_words.insert("un");
		word_set->stop_words.insert("es");
		word_set->stop_words.insert("se");
		word_set->stop_words.insert("no");
		word_set->stop_words.insert("te");
		word_set->stop_words.insert("lo");
		word_set->stop_words.insert("le");
		word_set->stop_words.insert("da");
		word_set->stop_words.insert("su");
		word_set->stop_words.insert("por");
		word_set->stop_words.insert("son");
		word_set->stop_words.insert("con");
		word_set->stop_words.insert("para");
		break;
		
	case GIN_LANG_FRENCH:
		word_set->stop_words.insert("le");
		word_set->stop_words.insert("de");
		word_set->stop_words.insert("et");
		word_set->stop_words.insert("à");
		word_set->stop_words.insert("un");
		word_set->stop_words.insert("il");
		word_set->stop_words.insert("être");
		word_set->stop_words.insert("et");
		word_set->stop_words.insert("en");
		word_set->stop_words.insert("avoir");
		word_set->stop_words.insert("que");
		word_set->stop_words.insert("pour");
		word_set->stop_words.insert("dans");
		word_set->stop_words.insert("ce");
		word_set->stop_words.insert("son");
		word_set->stop_words.insert("une");
		word_set->stop_words.insert("sur");
		word_set->stop_words.insert("avec");
		word_set->stop_words.insert("ne");
		word_set->stop_words.insert("se");
		break;
		
	case GIN_LANG_GERMAN:
		word_set->stop_words.insert("der");
		word_set->stop_words.insert("die");
		word_set->stop_words.insert("und");
		word_set->stop_words.insert("in");
		word_set->stop_words.insert("den");
		word_set->stop_words.insert("von");
		word_set->stop_words.insert("zu");
		word_set->stop_words.insert("das");
		word_set->stop_words.insert("mit");
		word_set->stop_words.insert("sich");
		word_set->stop_words.insert("des");
		word_set->stop_words.insert("auf");
		word_set->stop_words.insert("für");
		word_set->stop_words.insert("ist");
		word_set->stop_words.insert("im");
		word_set->stop_words.insert("dem");
		word_set->stop_words.insert("nicht");
		word_set->stop_words.insert("ein");
		word_set->stop_words.insert("eine");
		word_set->stop_words.insert("als");
		break;
		
	default:
		// No specific stop words for this language
		break;
	}
}

void GinStopWordsManager::setStopWordHandling(GinStopWordHandling handling)
{
	m_handling_mode = handling;
}

void GinStopWordsManager::setCaseSensitive(bool case_sensitive)
{
	m_case_sensitive = case_sensitive;
	
	// Update all existing stop word sets
	for (size_t i = 0; i < m_stop_word_sets.getCount(); i++) {
		m_stop_word_sets[i].case_sensitive = case_sensitive;
	}
}

ULONG GinStopWordsManager::getStopWordCount(GinLanguageCode language) const
{
	const StopWordSet* word_set = const_cast<GinStopWordsManager*>(this)->getStopWordSet(language);
	return word_set ? word_set->stop_words.size() : 0;
}

void GinStopWordsManager::initializeDefaultStopWords()
{
	// Initialize with English stop words by default
	loadStopWordsForLanguage(GIN_LANG_ENGLISH);
}

GinStopWordsManager::StopWordSet* GinStopWordsManager::getStopWordSet(GinLanguageCode language)
{
	for (size_t i = 0; i < m_stop_word_sets.getCount(); i++) {
		if (m_stop_word_sets[i].language == language) {
			return &m_stop_word_sets[i];
		}
	}
	return nullptr;
}

string GinStopWordsManager::normalizeStopWord(const string& word)
{
	if (m_case_sensitive) {
		return word;
	} else {
		string normalized = word;
		std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
		return normalized;
	}
}

//----------------------------
// GinAdvancedTextProcessor Implementation
//----------------------------

GinAdvancedTextProcessor::GinAdvancedTextProcessor(MemoryPool& pool)
	: m_pool(pool), m_language_detector(), m_stemming_engine(pool), 
	  m_stop_words_manager(pool), m_config(), m_stats()
{
}

GinAdvancedTextProcessor::~GinAdvancedTextProcessor()
{
}

ObjectsArray<string> GinAdvancedTextProcessor::processText(const string& text,
														   const GinAdvancedConfig& config)
{
	clock_t start_time = clock();
	
	// Update configuration if changed
	if (memcmp(&m_config, &config, sizeof(GinAdvancedConfig)) != 0) {
		configure(config);
	}
	
	// Detect language
	GinLanguageCode detected_language = config.primary_language;
	if (config.auto_detect_language) {
		detected_language = m_language_detector.detectLanguage(text);
		if (detected_language == GIN_LANG_GENERIC) {
			detected_language = config.primary_language; // Fallback
		}
	}
	
	// Tokenize the text
	ObjectsArray<string> terms = tokenizeText(text);
	if (terms.isEmpty()) {
		return terms;
	}
	
	// Normalize terms
	terms = normalizeTerms(terms, detected_language);
	
	// Apply stop word filtering
	ULONG original_count = terms.getCount();
	if (config.stop_word_handling == GIN_STOPWORDS_FILTER) {
		terms = filterStopWords(terms, detected_language);
	}
	ULONG filtered_count = original_count - terms.getCount();
	
	// Apply stemming
	ULONG stemmed_count = 0;
	if (config.enable_stemming && config.stem_indexed_terms) {
		terms = stemTerms(terms, detected_language);
		stemmed_count = terms.getCount();
	}
	
	// Update statistics
	clock_t end_time = clock();
	double processing_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
	updateStatistics(original_count, stemmed_count, filtered_count, processing_time);
	
	return terms;
}

ObjectsArray<string> GinAdvancedTextProcessor::processQuery(const string& query,
															const GinAdvancedConfig& config)
{
	// Query processing is similar to text processing but may have different settings
	GinAdvancedConfig query_config = config;
	query_config.stem_indexed_terms = config.stem_query_terms;
	
	return processText(query, query_config);
}

void GinAdvancedTextProcessor::configure(const GinAdvancedConfig& config)
{
	m_config = config;
	
	// Configure components
	m_stemming_engine.configureStemming(config);
	m_stop_words_manager.setStopWordHandling(config.stop_word_handling);
	m_stop_words_manager.setCaseSensitive(config.case_sensitive_stop_words);
	
	// Load language-specific data
	m_stop_words_manager.loadStopWordsForLanguage(config.primary_language);
}

GinAdvancedTextProcessor::ProcessingStats GinAdvancedTextProcessor::getProcessingStatistics() const
{
	return m_stats;
}

void GinAdvancedTextProcessor::resetStatistics()
{
	m_stats = ProcessingStats();
}

ObjectsArray<string> GinAdvancedTextProcessor::tokenizeText(const string& text)
{
	ObjectsArray<string> tokens(m_pool);
	string current_token;
	
	for (size_t i = 0; i < text.length(); i++) {
		char c = text[i];
		
		if (std::isalnum(c) || c == '_') {
			current_token += c;
		} else {
			if (!current_token.empty()) {
				if (current_token.length() >= m_config.min_term_length &&
					current_token.length() <= m_config.max_term_length) {
					tokens.add(current_token);
				}
				current_token.clear();
			}
		}
	}
	
	// Add the last token
	if (!current_token.empty()) {
		if (current_token.length() >= m_config.min_term_length &&
			current_token.length() <= m_config.max_term_length) {
			tokens.add(current_token);
		}
	}
	
	return tokens;
}

ObjectsArray<string> GinAdvancedTextProcessor::normalizeTerms(const ObjectsArray<string>& terms,
															  GinLanguageCode language)
{
	ObjectsArray<string> normalized_terms(m_pool);
	
	for (size_t i = 0; i < terms.getCount(); i++) {
		string normalized = terms[i];
		
		if (m_config.fold_case) {
			std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
		}
		
		if (m_config.normalize_unicode) {
			// Unicode normalization would go here
			// This is a complex topic requiring Unicode libraries
		}
		
		if (m_config.remove_diacritics) {
			// Diacritic removal would go here
		}
		
		// Language-specific normalization
		normalized = m_language_detector.normalizeTextForLanguage(normalized, language);
		
		normalized_terms.add(normalized);
	}
	
	return normalized_terms;
}

ObjectsArray<string> GinAdvancedTextProcessor::stemTerms(const ObjectsArray<string>& terms,
														 GinLanguageCode language)
{
	return m_stemming_engine.stemTerms(terms, language, m_config.stemming_algorithm);
}

ObjectsArray<string> GinAdvancedTextProcessor::filterStopWords(const ObjectsArray<string>& terms,
															   GinLanguageCode language)
{
	return m_stop_words_manager.filterStopWords(terms, language);
}

void GinAdvancedTextProcessor::updateStatistics(ULONG terms_count, ULONG stemmed_count,
												 ULONG filtered_count, double processing_time)
{
	m_stats.texts_processed++;
	m_stats.terms_processed += terms_count;
	m_stats.terms_stemmed += stemmed_count;
	m_stats.stop_words_filtered += filtered_count;
	
	// Update average processing time
	double total_time = m_stats.average_processing_time_ms * (m_stats.texts_processed - 1);
	m_stats.average_processing_time_ms = (total_time + processing_time) / m_stats.texts_processed;
}

//----------------------------
// GinAdvancedFeaturesManager Implementation
//----------------------------

void GinAdvancedFeaturesManager::initialize()
{
	if (s_initialized) return;
	
	s_initialized = true;
}

void GinAdvancedFeaturesManager::shutdown()
{
	if (!s_initialized) return;
	
	// Clean up all processors
	for (auto it = s_processors.begin(); it != s_processors.end(); ++it) {
		delete it->second;
	}
	s_processors.clear();
	
	s_initialized = false;
}

bool GinAdvancedFeaturesManager::setAdvancedConfig(thread_db* tdbb, USHORT index_id,
												   const GinAdvancedConfig& config)
{
	if (!s_initialized) return false;
	
	// Create or update processor for this index
	createProcessor(tdbb, index_id, config);
	
	return true;
}

GinAdvancedConfig GinAdvancedFeaturesManager::getAdvancedConfig(thread_db* tdbb, USHORT index_id)
{
	GinAdvancedTextProcessor* processor = getProcessor(tdbb, index_id);
	if (processor) {
		// Return a copy of the current configuration
		// In a full implementation, would store config separately
		return GinAdvancedConfig();
	}
	
	return GinAdvancedConfig(); // Default configuration
}

ObjectsArray<string> GinAdvancedFeaturesManager::processTextForIndexing(thread_db* tdbb, 
																		USHORT index_id, 
																		const string& text)
{
	GinAdvancedTextProcessor* processor = getProcessor(tdbb, index_id);
	if (processor) {
		GinAdvancedConfig config = getAdvancedConfig(tdbb, index_id);
		return processor->processText(text, config);
	}
	
	// Fallback to basic tokenization
	ObjectsArray<string> tokens(*tdbb->getDefaultPool());
	// Basic tokenization logic here
	return tokens;
}

ObjectsArray<string> GinAdvancedFeaturesManager::processQueryText(thread_db* tdbb,
																  USHORT index_id,
																  const string& query)
{
	GinAdvancedTextProcessor* processor = getProcessor(tdbb, index_id);
	if (processor) {
		GinAdvancedConfig config = getAdvancedConfig(tdbb, index_id);
		return processor->processQuery(query, config);
	}
	
	// Fallback to basic tokenization
	ObjectsArray<string> tokens(*tdbb->getDefaultPool());
	// Basic tokenization logic here
	return tokens;
}

GinAdvancedTextProcessor* GinAdvancedFeaturesManager::getProcessor(thread_db* tdbb, USHORT index_id)
{
	if (s_processors.exist(index_id)) {
		return s_processors[index_id];
	}
	return nullptr;
}

void GinAdvancedFeaturesManager::createProcessor(thread_db* tdbb, USHORT index_id,
												 const GinAdvancedConfig& config)
{
	// Remove existing processor if present
	if (s_processors.exist(index_id)) {
		delete s_processors[index_id];
	}
	
	// Create new processor
	GinAdvancedTextProcessor* processor = new GinAdvancedTextProcessor(*tdbb->getDefaultPool());
	processor->configure(config);
	
	s_processors.put(index_id, processor);
}

void GinAdvancedFeaturesManager::destroyProcessor(USHORT index_id)
{
	if (s_processors.exist(index_id)) {
		delete s_processors[index_id];
		s_processors.remove(index_id);
	}
}