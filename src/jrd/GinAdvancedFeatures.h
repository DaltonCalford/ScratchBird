/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		GinAdvancedFeatures.h
 *	DESCRIPTION:	Advanced GIN index features: stemming, stop words, language support
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

#ifndef JRD_GIN_ADVANCED_FEATURES_H
#define JRD_GIN_ADVANCED_FEATURES_H

#include "firebird.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/array.h"
#include "../common/classes/GenericMap.h"
#include "../jrd/jrd.h"
#include <vector>
#include <set>
#include <map>

namespace Jrd {

// Forward declarations
class thread_db;
class jrd_tra;

//----------------------------
// Language Support Constants
//----------------------------

enum GinLanguageCode {
	GIN_LANG_ENGLISH = 0,
	GIN_LANG_SPANISH = 1,
	GIN_LANG_FRENCH = 2,
	GIN_LANG_GERMAN = 3,
	GIN_LANG_ITALIAN = 4,
	GIN_LANG_PORTUGUESE = 5,
	GIN_LANG_RUSSIAN = 6,
	GIN_LANG_CHINESE = 7,
	GIN_LANG_JAPANESE = 8,
	GIN_LANG_KOREAN = 9,
	GIN_LANG_ARABIC = 10,
	GIN_LANG_HINDI = 11,
	GIN_LANG_GENERIC = 99,
	GIN_LANG_AUTO_DETECT = 255
};

enum GinStemmingAlgorithm {
	GIN_STEM_NONE = 0,
	GIN_STEM_PORTER = 1,
	GIN_STEM_SNOWBALL = 2,
	GIN_STEM_LOVINS = 3,
	GIN_STEM_PAICE_HUSK = 4,
	GIN_STEM_LANCASTER = 5
};

enum GinStopWordHandling {
	GIN_STOPWORDS_NONE = 0,		// Don't filter stop words
	GIN_STOPWORDS_FILTER = 1,	// Remove stop words from indexing
	GIN_STOPWORDS_MARK = 2,		// Mark stop words but keep them
	GIN_STOPWORDS_CUSTOM = 3	// Use custom stop word list
};

//----------------------------
// GIN Advanced Configuration
//----------------------------
struct GinAdvancedConfig
{
	// Language configuration
	GinLanguageCode primary_language;
	bool auto_detect_language;
	bool multi_language_support;
	
	// Stemming configuration
	GinStemmingAlgorithm stemming_algorithm;
	bool enable_stemming;
	bool stem_query_terms;
	bool stem_indexed_terms;
	
	// Stop words configuration
	GinStopWordHandling stop_word_handling;
	ScratchBird::string custom_stop_words_file;
	bool case_sensitive_stop_words;
	
	// Text processing options
	bool normalize_unicode;
	bool fold_case;
	bool remove_diacritics;
	bool handle_contractions;
	
	// Performance options
	ULONG max_term_length;
	ULONG min_term_length;
	bool cache_stemmed_terms;
	ULONG stemming_cache_size;
	
	GinAdvancedConfig()
		: primary_language(GIN_LANG_ENGLISH), auto_detect_language(false),
		  multi_language_support(false), stemming_algorithm(GIN_STEM_PORTER),
		  enable_stemming(false), stem_query_terms(true), stem_indexed_terms(true),
		  stop_word_handling(GIN_STOPWORDS_NONE), case_sensitive_stop_words(false),
		  normalize_unicode(true), fold_case(true), remove_diacritics(false),
		  handle_contractions(true), max_term_length(64), min_term_length(2),
		  cache_stemmed_terms(true), stemming_cache_size(10000)
	{
	}
};

//----------------------------
// Language Detection Engine
//----------------------------
class GinLanguageDetector
{
public:
	GinLanguageDetector();
	~GinLanguageDetector();
	
	// Language detection methods
	GinLanguageCode detectLanguage(const ScratchBird::string& text);
	double calculateLanguageConfidence(const ScratchBird::string& text, GinLanguageCode language);
	
	// Multi-language support
	ScratchBird::ObjectsArray<GinLanguageCode> detectMultipleLanguages(const ScratchBird::string& text);
	bool isMultiLanguageText(const ScratchBird::string& text);
	
	// Language-specific processing
	ScratchBird::string normalizeTextForLanguage(const ScratchBird::string& text, GinLanguageCode language);
	bool requiresSpecialTokenization(GinLanguageCode language);
	
	// Configuration
	void setDetectionThreshold(double threshold);
	void enableLanguage(GinLanguageCode language, bool enabled);
	
private:
	// Language detection data
	struct LanguageProfile {
		GinLanguageCode language_code;
		ScratchBird::GenericMap<ScratchBird::string, double> trigram_frequencies;
		ScratchBird::GenericMap<ScratchBird::string, double> character_frequencies;
		ScratchBird::ObjectsArray<ScratchBird::string> common_words;
		double detection_threshold;
		bool enabled;
		
		LanguageProfile(MemoryPool& pool)
			: language_code(GIN_LANG_ENGLISH), trigram_frequencies(pool),
			  character_frequencies(pool), common_words(pool),
			  detection_threshold(0.7), enabled(true)
		{
		}
	};
	
	ScratchBird::ObjectsArray<LanguageProfile> m_language_profiles;
	double m_detection_threshold;
	MemoryPool& m_pool;
	
	// Internal methods
	void initializeLanguageProfiles();
	void loadLanguageProfile(GinLanguageCode language);
	double calculateTrigramScore(const ScratchBird::string& text, const LanguageProfile& profile);
	double calculateCharacterScore(const ScratchBird::string& text, const LanguageProfile& profile);
	double calculateWordScore(const ScratchBird::string& text, const LanguageProfile& profile);
	ScratchBird::ObjectsArray<ScratchBird::string> extractTrigrams(const ScratchBird::string& text);
};

//----------------------------
// Stemming Engine
//----------------------------
class GinStemmingEngine
{
public:
	GinStemmingEngine(MemoryPool& pool);
	~GinStemmingEngine();
	
	// Stemming operations
	ScratchBird::string stemTerm(const ScratchBird::string& term, GinLanguageCode language, 
								 GinStemmingAlgorithm algorithm);
	ScratchBird::ObjectsArray<ScratchBird::string> stemTerms(
		const ScratchBird::ObjectsArray<ScratchBird::string>& terms,
		GinLanguageCode language, GinStemmingAlgorithm algorithm);
	
	// Cache management
	void clearStemmingCache();
	void preloadCommonStems(GinLanguageCode language);
	ULONG getCacheSize() const;
	double getCacheHitRatio() const;
	
	// Algorithm-specific stemming
	ScratchBird::string porterStem(const ScratchBird::string& term, GinLanguageCode language);
	ScratchBird::string snowballStem(const ScratchBird::string& term, GinLanguageCode language);
	ScratchBird::string lovinstem(const ScratchBird::string& term, GinLanguageCode language);
	ScratchBird::string paiceHuskStem(const ScratchBird::string& term, GinLanguageCode language);
	
	// Validation and configuration
	bool isStemmingSupported(GinLanguageCode language, GinStemmingAlgorithm algorithm);
	void configureStemming(const GinAdvancedConfig& config);
	
private:
	struct StemmingRule {
		ScratchBird::string suffix;
		ScratchBird::string replacement;
		USHORT min_word_length;
		bool remove_only;
		
		StemmingRule(MemoryPool& pool)
			: suffix(pool), replacement(pool), min_word_length(3), remove_only(false)
		{
		}
	};
	
	struct LanguageStemmingRules {
		GinLanguageCode language;
		GinStemmingAlgorithm algorithm;
		ScratchBird::ObjectsArray<StemmingRule> rules;
		ScratchBird::ObjectsArray<ScratchBird::string> vowels;
		ScratchBird::ObjectsArray<ScratchBird::string> consonants;
		
		LanguageStemmingRules(MemoryPool& pool)
			: language(GIN_LANG_ENGLISH), algorithm(GIN_STEM_PORTER),
			  rules(pool), vowels(pool), consonants(pool)
		{
		}
	};
	
	// Stemming cache
	struct StemmingCacheEntry {
		ScratchBird::string original_term;
		ScratchBird::string stemmed_term;
		GinLanguageCode language;
		GinStemmingAlgorithm algorithm;
		ULONG access_count;
		time_t last_access;
		
		StemmingCacheEntry(MemoryPool& pool)
			: original_term(pool), stemmed_term(pool), language(GIN_LANG_ENGLISH),
			  algorithm(GIN_STEM_PORTER), access_count(0), last_access(0)
		{
		}
	};
	
	MemoryPool& m_pool;
	ScratchBird::GenericMap<ScratchBird::string, StemmingCacheEntry> m_stemming_cache;
	ScratchBird::ObjectsArray<LanguageStemmingRules> m_stemming_rules;
	ULONG m_cache_size_limit;
	ULONG m_cache_hits;
	ULONG m_cache_misses;
	
	// Internal methods
	void initializeStemmingRules();
	void loadStemmingRulesForLanguage(GinLanguageCode language, GinStemmingAlgorithm algorithm);
	ScratchBird::string applyStemmingrules(const ScratchBird::string& term, 
										   const LanguageStemmingRules& rules);
	bool isVowel(char c, const LanguageStemmingRules& rules);
	bool isConsonant(char c, const LanguageStemmingRules& rules);
	ScratchBird::string createCacheKey(const ScratchBird::string& term, 
									   GinLanguageCode language, GinStemmingAlgorithm algorithm);
	void addToCache(const ScratchBird::string& original, const ScratchBird::string& stemmed,
					GinLanguageCode language, GinStemmingAlgorithm algorithm);
	void evictOldCacheEntries();
};

//----------------------------
// Stop Words Manager
//----------------------------
class GinStopWordsManager
{
public:
	GinStopWordsManager(MemoryPool& pool);
	~GinStopWordsManager();
	
	// Stop word operations
	bool isStopWord(const ScratchBird::string& term, GinLanguageCode language);
	ScratchBird::ObjectsArray<ScratchBird::string> filterStopWords(
		const ScratchBird::ObjectsArray<ScratchBird::string>& terms, GinLanguageCode language);
	ScratchBird::ObjectsArray<ScratchBird::string> markStopWords(
		const ScratchBird::ObjectsArray<ScratchBird::string>& terms, GinLanguageCode language);
	
	// Stop word list management
	void loadStopWordsForLanguage(GinLanguageCode language);
	void loadCustomStopWords(const ScratchBird::string& filename);
	void addStopWord(const ScratchBird::string& word, GinLanguageCode language);
	void removeStopWord(const ScratchBird::string& word, GinLanguageCode language);
	
	// Configuration
	void setStopWordHandling(GinStopWordHandling handling);
	void setCaseSensitive(bool case_sensitive);
	ULONG getStopWordCount(GinLanguageCode language) const;
	
	// Validation
	bool validateStopWordFile(const ScratchBird::string& filename);
	
private:
	struct StopWordSet {
		GinLanguageCode language;
		std::set<ScratchBird::string> stop_words;
		bool case_sensitive;
		time_t last_updated;
		
		StopWordSet()
			: language(GIN_LANG_ENGLISH), case_sensitive(false), last_updated(0)
		{
		}
	};
	
	MemoryPool& m_pool;
	ScratchBird::ObjectsArray<StopWordSet> m_stop_word_sets;
	GinStopWordHandling m_handling_mode;
	bool m_case_sensitive;
	
	// Internal methods
	void initializeDefaultStopWords();
	StopWordSet* getStopWordSet(GinLanguageCode language);
	ScratchBird::string normalizeStopWord(const ScratchBird::string& word);
	void loadStopWordsFromFile(const ScratchBird::string& filename, GinLanguageCode language);
};

//----------------------------
// Advanced Text Processor
//----------------------------
class GinAdvancedTextProcessor
{
public:
	GinAdvancedTextProcessor(MemoryPool& pool);
	~GinAdvancedTextProcessor();
	
	// Main processing interface
	ScratchBird::ObjectsArray<ScratchBird::string> processText(const ScratchBird::string& text,
															   const GinAdvancedConfig& config);
	ScratchBird::ObjectsArray<ScratchBird::string> processQuery(const ScratchBird::string& query,
																const GinAdvancedConfig& config);
	
	// Component access
	GinLanguageDetector& getLanguageDetector() { return m_language_detector; }
	GinStemmingEngine& getStemmingEngine() { return m_stemming_engine; }
	GinStopWordsManager& getStopWordsManager() { return m_stop_words_manager; }
	
	// Configuration
	void configure(const GinAdvancedConfig& config);
	void reconfigure(const GinAdvancedConfig& config);
	
	// Performance monitoring
	struct ProcessingStats {
		ULONG texts_processed;
		ULONG terms_processed;
		ULONG terms_stemmed;
		ULONG stop_words_filtered;
		ULONG languages_detected;
		double average_processing_time_ms;
		
		ProcessingStats()
			: texts_processed(0), terms_processed(0), terms_stemmed(0),
			  stop_words_filtered(0), languages_detected(0), average_processing_time_ms(0.0)
		{
		}
	};
	
	ProcessingStats getProcessingStatistics() const;
	void resetStatistics();
	
private:
	MemoryPool& m_pool;
	GinLanguageDetector m_language_detector;
	GinStemmingEngine m_stemming_engine;
	GinStopWordsManager m_stop_words_manager;
	GinAdvancedConfig m_config;
	ProcessingStats m_stats;
	
	// Internal processing methods
	ScratchBird::ObjectsArray<ScratchBird::string> tokenizeText(const ScratchBird::string& text);
	ScratchBird::ObjectsArray<ScratchBird::string> normalizeTerms(
		const ScratchBird::ObjectsArray<ScratchBird::string>& terms, GinLanguageCode language);
	ScratchBird::ObjectsArray<ScratchBird::string> stemTerms(
		const ScratchBird::ObjectsArray<ScratchBird::string>& terms, GinLanguageCode language);
	ScratchBird::ObjectsArray<ScratchBird::string> filterStopWords(
		const ScratchBird::ObjectsArray<ScratchBird::string>& terms, GinLanguageCode language);
	
	void updateStatistics(ULONG terms_count, ULONG stemmed_count, ULONG filtered_count, 
						  double processing_time);
};

//----------------------------
// GIN Advanced Features Integration
//----------------------------
class GinAdvancedFeaturesManager
{
public:
	static void initialize();
	static void shutdown();
	
	// Configuration management
	static bool setAdvancedConfig(thread_db* tdbb, USHORT index_id, const GinAdvancedConfig& config);
	static GinAdvancedConfig getAdvancedConfig(thread_db* tdbb, USHORT index_id);
	static void removeAdvancedConfig(thread_db* tdbb, USHORT index_id);
	
	// Text processing interface
	static ScratchBird::ObjectsArray<ScratchBird::string> processTextForIndexing(
		thread_db* tdbb, USHORT index_id, const ScratchBird::string& text);
	static ScratchBird::ObjectsArray<ScratchBird::string> processQueryText(
		thread_db* tdbb, USHORT index_id, const ScratchBird::string& query);
	
	// System catalog integration
	static bool storeAdvancedConfigInCatalog(thread_db* tdbb, jrd_tra* transaction,
											 USHORT index_id, const GinAdvancedConfig& config);
	static bool loadAdvancedConfigFromCatalog(thread_db* tdbb, USHORT index_id,
											  GinAdvancedConfig& config);
	
	// Performance monitoring
	static void reportProcessingStatistics(thread_db* tdbb, USHORT index_id);
	static void clearProcessingStatistics(thread_db* tdbb, USHORT index_id);
	
private:
	static ScratchBird::GenericMap<USHORT, GinAdvancedTextProcessor*> s_processors;
	static bool s_initialized;
	
	static GinAdvancedTextProcessor* getProcessor(thread_db* tdbb, USHORT index_id);
	static void createProcessor(thread_db* tdbb, USHORT index_id, const GinAdvancedConfig& config);
	static void destroyProcessor(USHORT index_id);
};

} // namespace Jrd

#endif // JRD_GIN_ADVANCED_FEATURES_H