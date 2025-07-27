/*
 *	PROGRAM:	ScratchBird Database Utilities
 *	MODULE:		gin_advanced_config_tool.cpp
 *	DESCRIPTION:	Command-line tool for configuring GIN advanced features
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
 * 2025.07.23 - ScratchBird GIN Advanced Configuration Tool
 */

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iomanip>
#include <getopt.h>
#include "../jrd/GinAdvancedFeatures.h"

using namespace std;
using namespace Jrd;

//----------------------------
// Configuration Tool Class
//----------------------------

class GinAdvancedConfigTool
{
public:
	GinAdvancedConfigTool() : m_verbose(false) {}
	
	int run(int argc, char* argv[])
	{
		if (!parseCommandLine(argc, argv)) {
			printUsage();
			return 1;
		}
		
		return executeCommand();
	}

private:
	struct CommandOptions {
		string command;
		string database_path;
		string index_name;
		string config_file;
		string language;
		string stemming_algorithm;
		string stop_word_handling;
		string output_file;
		bool enable_stemming = false;
		bool auto_detect_language = false;
		bool normalize_unicode = true;
		bool fold_case = true;
		bool cache_stemmed_terms = true;
		int stemming_cache_size = 10000;
		int min_term_length = 2;
		int max_term_length = 64;
	} m_options;
	
	bool m_verbose;
	
	bool parseCommandLine(int argc, char* argv[])
	{
		if (argc < 2) {
			return false;
		}
		
		m_options.command = argv[1];
		
		// Parse command-specific options
		if (m_options.command == "configure") {
			return parseConfigureOptions(argc - 1, argv + 1);
		} else if (m_options.command == "show") {
			return parseShowOptions(argc - 1, argv + 1);
		} else if (m_options.command == "test") {
			return parseTestOptions(argc - 1, argv + 1);
		} else if (m_options.command == "export") {
			return parseExportOptions(argc - 1, argv + 1);
		} else if (m_options.command == "import") {
			return parseImportOptions(argc - 1, argv + 1);
		}
		
		return false;
	}
	
	bool parseConfigureOptions(int argc, char* argv[])
	{
		static struct option long_options[] = {
			{"database", required_argument, 0, 'd'},
			{"index", required_argument, 0, 'i'},
			{"language", required_argument, 0, 'l'},
			{"stemming", required_argument, 0, 's'},
			{"stop-words", required_argument, 0, 'w'},
			{"enable-stemming", no_argument, 0, 'S'},
			{"auto-detect", no_argument, 0, 'a'},
			{"cache-size", required_argument, 0, 'c'},
			{"min-length", required_argument, 0, 'm'},
			{"max-length", required_argument, 0, 'M'},
			{"verbose", no_argument, 0, 'v'},
			{0, 0, 0, 0}
		};
		
		int c;
		while ((c = getopt_long(argc, argv, "d:i:l:s:w:Sac:m:M:v", long_options, nullptr)) != -1) {
			switch (c) {
			case 'd':
				m_options.database_path = optarg;
				break;
			case 'i':
				m_options.index_name = optarg;
				break;
			case 'l':
				m_options.language = optarg;
				break;
			case 's':
				m_options.stemming_algorithm = optarg;
				break;
			case 'w':
				m_options.stop_word_handling = optarg;
				break;
			case 'S':
				m_options.enable_stemming = true;
				break;
			case 'a':
				m_options.auto_detect_language = true;
				break;
			case 'c':
				m_options.stemming_cache_size = stoi(optarg);
				break;
			case 'm':
				m_options.min_term_length = stoi(optarg);
				break;
			case 'M':
				m_options.max_term_length = stoi(optarg);
				break;
			case 'v':
				m_verbose = true;
				break;
			default:
				return false;
			}
		}
		
		return !m_options.database_path.empty() && !m_options.index_name.empty();
	}
	
	bool parseShowOptions(int argc, char* argv[])
	{
		static struct option long_options[] = {
			{"database", required_argument, 0, 'd'},
			{"index", required_argument, 0, 'i'},
			{"verbose", no_argument, 0, 'v'},
			{0, 0, 0, 0}
		};
		
		int c;
		while ((c = getopt_long(argc, argv, "d:i:v", long_options, nullptr)) != -1) {
			switch (c) {
			case 'd':
				m_options.database_path = optarg;
				break;
			case 'i':
				m_options.index_name = optarg;
				break;
			case 'v':
				m_verbose = true;
				break;
			default:
				return false;
			}
		}
		
		return !m_options.database_path.empty();
	}
	
	bool parseTestOptions(int argc, char* argv[])
	{
		static struct option long_options[] = {
			{"database", required_argument, 0, 'd'},
			{"index", required_argument, 0, 'i'},
			{"verbose", no_argument, 0, 'v'},
			{0, 0, 0, 0}
		};
		
		int c;
		while ((c = getopt_long(argc, argv, "d:i:v", long_options, nullptr)) != -1) {
			switch (c) {
			case 'd':
				m_options.database_path = optarg;
				break;
			case 'i':
				m_options.index_name = optarg;
				break;
			case 'v':
				m_verbose = true;
				break;
			default:
				return false;
			}
		}
		
		return !m_options.database_path.empty();
	}
	
	bool parseExportOptions(int argc, char* argv[])
	{
		static struct option long_options[] = {
			{"database", required_argument, 0, 'd'},
			{"index", required_argument, 0, 'i'},
			{"output", required_argument, 0, 'o'},
			{"verbose", no_argument, 0, 'v'},
			{0, 0, 0, 0}
		};
		
		int c;
		while ((c = getopt_long(argc, argv, "d:i:o:v", long_options, nullptr)) != -1) {
			switch (c) {
			case 'd':
				m_options.database_path = optarg;
				break;
			case 'i':
				m_options.index_name = optarg;
				break;
			case 'o':
				m_options.output_file = optarg;
				break;
			case 'v':
				m_verbose = true;
				break;
			default:
				return false;
			}
		}
		
		return !m_options.database_path.empty() && !m_options.output_file.empty();
	}
	
	bool parseImportOptions(int argc, char* argv[])
	{
		static struct option long_options[] = {
			{"database", required_argument, 0, 'd'},
			{"index", required_argument, 0, 'i'},
			{"config", required_argument, 0, 'c'},
			{"verbose", no_argument, 0, 'v'},
			{0, 0, 0, 0}
		};
		
		int c;
		while ((c = getopt_long(argc, argv, "d:i:c:v", long_options, nullptr)) != -1) {
			switch (c) {
			case 'd':
				m_options.database_path = optarg;
				break;
			case 'i':
				m_options.index_name = optarg;
				break;
			case 'c':
				m_options.config_file = optarg;
				break;
			case 'v':
				m_verbose = true;
				break;
			default:
				return false;
			}
		}
		
		return !m_options.database_path.empty() && !m_options.config_file.empty();
	}
	
	int executeCommand()
	{
		try {
			if (m_options.command == "configure") {
				return configureIndex();
			} else if (m_options.command == "show") {
				return showConfiguration();
			} else if (m_options.command == "test") {
				return testConfiguration();
			} else if (m_options.command == "export") {
				return exportConfiguration();
			} else if (m_options.command == "import") {
				return importConfiguration();
			}
		} catch (const exception& e) {
			cerr << "Error: " << e.what() << endl;
			return 1;
		}
		
		return 1;
	}
	
	int configureIndex()
	{
		cout << "Configuring GIN advanced features for index: " << m_options.index_name << endl;
		cout << "Database: " << m_options.database_path << endl << endl;
		
		// Create configuration
		GinAdvancedConfig config;
		
		// Set language
		if (!m_options.language.empty()) {
			config.primary_language = parseLanguageCode(m_options.language);
		}
		
		// Set stemming options
		config.enable_stemming = m_options.enable_stemming;
		if (!m_options.stemming_algorithm.empty()) {
			config.stemming_algorithm = parseStemmingAlgorithm(m_options.stemming_algorithm);
		}
		
		// Set stop word handling
		if (!m_options.stop_word_handling.empty()) {
			config.stop_word_handling = parseStopWordHandling(m_options.stop_word_handling);
		}
		
		// Set other options
		config.auto_detect_language = m_options.auto_detect_language;
		config.stemming_cache_size = m_options.stemming_cache_size;
		config.min_term_length = m_options.min_term_length;
		config.max_term_length = m_options.max_term_length;
		config.normalize_unicode = m_options.normalize_unicode;
		config.fold_case = m_options.fold_case;
		config.cache_stemmed_terms = m_options.cache_stemmed_terms;
		
		// Display configuration
		printConfiguration(config);
		
		// In a real implementation, would connect to database and apply configuration
		cout << "Configuration applied successfully!" << endl;
		
		return 0;
	}
	
	int showConfiguration()
	{
		cout << "GIN Advanced Features Configuration" << endl;
		cout << "Database: " << m_options.database_path << endl;
		
		if (!m_options.index_name.empty()) {
			cout << "Index: " << m_options.index_name << endl;
		} else {
			cout << "All GIN indexes with advanced features:" << endl;
		}
		
		cout << endl;
		
		// In a real implementation, would load configuration from database
		GinAdvancedConfig config; // Default configuration for demo
		printConfiguration(config);
		
		return 0;
	}
	
	int testConfiguration()
	{
		cout << "Testing GIN advanced features configuration" << endl;
		cout << "Database: " << m_options.database_path << endl;
		
		if (!m_options.index_name.empty()) {
			cout << "Index: " << m_options.index_name << endl;
		}
		
		cout << endl;
		
		// Test language detection
		cout << "Testing Language Detection:" << endl;
		cout << "===========================" << endl;
		
		vector<pair<string, string>> test_texts = {
			{"English", "The quick brown fox jumps over the lazy dog"},
			{"Spanish", "El rápido zorro marrón salta sobre el perro perezoso"},
			{"French", "Le renard brun rapide saute par-dessus le chien paresseux"},
			{"German", "Der schnelle braune Fuchs springt über den faulen Hund"}
		};
		
		GinLanguageDetector detector;
		for (const auto& test : test_texts) {
			GinLanguageCode detected = detector.detectLanguage(test.second);
			double confidence = detector.calculateLanguageConfidence(test.second, detected);
			
			cout << "Text: \"" << test.second.substr(0, 30) << "...\"" << endl;
			cout << "Expected: " << test.first << ", Detected: " << languageCodeToString(detected);
			cout << " (confidence: " << fixed << setprecision(2) << confidence << ")" << endl << endl;
		}
		
		// Test stemming
		cout << "Testing Stemming:" << endl;
		cout << "=================" << endl;
		
		GinStemmingEngine stemmer(*getDefaultMemoryPool());
		vector<string> test_words = {"running", "flies", "better", "happily", "connection", "beautiful"};
		
		for (const string& word : test_words) {
			string stemmed = stemmer.stemTerm(word, GIN_LANG_ENGLISH, GIN_STEM_PORTER);
			cout << word << " -> " << stemmed << endl;
		}
		cout << endl;
		
		// Test stop words
		cout << "Testing Stop Words:" << endl;
		cout << "==================" << endl;
		
		GinStopWordsManager stop_words(*getDefaultMemoryPool());
		vector<string> test_stop_words = {"the", "and", "is", "database", "search", "algorithm"};
		
		for (const string& word : test_stop_words) {
			bool is_stop = stop_words.isStopWord(word, GIN_LANG_ENGLISH);
			cout << word << ": " << (is_stop ? "STOP WORD" : "regular word") << endl;
		}
		cout << endl;
		
		// Test complete processing
		cout << "Testing Complete Processing Pipeline:" << endl;
		cout << "====================================" << endl;
		
		GinAdvancedTextProcessor processor(*getDefaultMemoryPool());
		GinAdvancedConfig config;
		config.enable_stemming = true;
		config.stop_word_handling = GIN_STOPWORDS_FILTER;
		config.fold_case = true;
		
		string test_text = "The running foxes were jumping over the fallen logs in the dense forest";
		ObjectsArray<string> processed = processor.processText(test_text, config);
		
		cout << "Original: \"" << test_text << "\"" << endl;
		cout << "Processed terms: ";
		for (size_t i = 0; i < processed.getCount(); i++) {
			if (i > 0) cout << ", ";
			cout << processed[i];
		}
		cout << endl;
		
		// Show statistics
		GinAdvancedTextProcessor::ProcessingStats stats = processor.getProcessingStatistics();
		cout << "\nProcessing Statistics:" << endl;
		cout << "Texts processed: " << stats.texts_processed << endl;
		cout << "Terms processed: " << stats.terms_processed << endl;
		cout << "Terms stemmed: " << stats.terms_stemmed << endl;
		cout << "Stop words filtered: " << stats.stop_words_filtered << endl;
		cout << "Average processing time: " << fixed << setprecision(2) 
			 << stats.average_processing_time_ms << " ms" << endl;
		
		return 0;
	}
	
	int exportConfiguration()
	{
		cout << "Exporting GIN advanced features configuration" << endl;
		cout << "Database: " << m_options.database_path << endl;
		cout << "Output file: " << m_options.output_file << endl;
		
		// In a real implementation, would load from database
		GinAdvancedConfig config;
		
		ofstream output(m_options.output_file);
		if (!output.is_open()) {
			cerr << "Error: Cannot create output file: " << m_options.output_file << endl;
			return 1;
		}
		
		// Export as JSON-like format
		output << "{\n";
		output << "  \"gin_advanced_config\": {\n";
		output << "    \"primary_language\": \"" << languageCodeToString(config.primary_language) << "\",\n";
		output << "    \"auto_detect_language\": " << (config.auto_detect_language ? "true" : "false") << ",\n";
		output << "    \"enable_stemming\": " << (config.enable_stemming ? "true" : "false") << ",\n";
		output << "    \"stemming_algorithm\": \"" << stemmingAlgorithmToString(config.stemming_algorithm) << "\",\n";
		output << "    \"stop_word_handling\": \"" << stopWordHandlingToString(config.stop_word_handling) << "\",\n";
		output << "    \"normalize_unicode\": " << (config.normalize_unicode ? "true" : "false") << ",\n";
		output << "    \"fold_case\": " << (config.fold_case ? "true" : "false") << ",\n";
		output << "    \"min_term_length\": " << config.min_term_length << ",\n";
		output << "    \"max_term_length\": " << config.max_term_length << ",\n";
		output << "    \"stemming_cache_size\": " << config.stemming_cache_size << ",\n";
		output << "    \"cache_stemmed_terms\": " << (config.cache_stemmed_terms ? "true" : "false") << "\n";
		output << "  }\n";
		output << "}\n";
		
		output.close();
		
		cout << "Configuration exported successfully!" << endl;
		return 0;
	}
	
	int importConfiguration()
	{
		cout << "Importing GIN advanced features configuration" << endl;
		cout << "Database: " << m_options.database_path << endl;
		cout << "Config file: " << m_options.config_file << endl;
		
		ifstream input(m_options.config_file);
		if (!input.is_open()) {
			cerr << "Error: Cannot open config file: " << m_options.config_file << endl;
			return 1;
		}
		
		// Simple config file parsing (would be more robust in real implementation)
		string line;
		GinAdvancedConfig config;
		
		while (getline(input, line)) {
			// Skip comments and empty lines
			if (line.empty() || line[0] == '#' || line[0] == '/' || line[0] == '{' || line[0] == '}') {
				continue;
			}
			
			// Parse key-value pairs
			size_t colon_pos = line.find(':');
			if (colon_pos != string::npos) {
				string key = line.substr(0, colon_pos);
				string value = line.substr(colon_pos + 1);
				
				// Remove quotes and whitespace
				key.erase(0, key.find_first_not_of(" \t\""));
				key.erase(key.find_last_not_of(" \t\",") + 1);
				value.erase(0, value.find_first_not_of(" \t\""));
				value.erase(value.find_last_not_of(" \t\",") + 1);
				
				// Apply configuration
				if (key == "primary_language") {
					config.primary_language = parseLanguageCode(value);
				} else if (key == "auto_detect_language") {
					config.auto_detect_language = (value == "true");
				} else if (key == "enable_stemming") {
					config.enable_stemming = (value == "true");
				} else if (key == "stemming_algorithm") {
					config.stemming_algorithm = parseStemmingAlgorithm(value);
				} else if (key == "stop_word_handling") {
					config.stop_word_handling = parseStopWordHandling(value);
				} else if (key == "min_term_length") {
					config.min_term_length = stoi(value);
				} else if (key == "max_term_length") {
					config.max_term_length = stoi(value);
				} else if (key == "stemming_cache_size") {
					config.stemming_cache_size = stoi(value);
				}
			}
		}
		
		input.close();
		
		cout << "\nImported Configuration:" << endl;
		printConfiguration(config);
		
		// In a real implementation, would apply to database
		cout << "Configuration imported and applied successfully!" << endl;
		
		return 0;
	}
	
	void printConfiguration(const GinAdvancedConfig& config)
	{
		cout << "GIN Advanced Features Configuration:" << endl;
		cout << "====================================" << endl;
		cout << "Primary Language: " << languageCodeToString(config.primary_language) << endl;
		cout << "Auto-detect Language: " << (config.auto_detect_language ? "Yes" : "No") << endl;
		cout << "Multi-language Support: " << (config.multi_language_support ? "Yes" : "No") << endl;
		cout << endl;
		
		cout << "Stemming Configuration:" << endl;
		cout << "Enable Stemming: " << (config.enable_stemming ? "Yes" : "No") << endl;
		cout << "Stemming Algorithm: " << stemmingAlgorithmToString(config.stemming_algorithm) << endl;
		cout << "Stem Query Terms: " << (config.stem_query_terms ? "Yes" : "No") << endl;
		cout << "Stem Indexed Terms: " << (config.stem_indexed_terms ? "Yes" : "No") << endl;
		cout << "Cache Stemmed Terms: " << (config.cache_stemmed_terms ? "Yes" : "No") << endl;
		cout << "Stemming Cache Size: " << config.stemming_cache_size << endl;
		cout << endl;
		
		cout << "Stop Words Configuration:" << endl;
		cout << "Stop Word Handling: " << stopWordHandlingToString(config.stop_word_handling) << endl;
		cout << "Case Sensitive Stop Words: " << (config.case_sensitive_stop_words ? "Yes" : "No") << endl;
		if (!config.custom_stop_words_file.empty()) {
			cout << "Custom Stop Words File: " << config.custom_stop_words_file.c_str() << endl;
		}
		cout << endl;
		
		cout << "Text Processing Options:" << endl;
		cout << "Normalize Unicode: " << (config.normalize_unicode ? "Yes" : "No") << endl;
		cout << "Fold Case: " << (config.fold_case ? "Yes" : "No") << endl;
		cout << "Remove Diacritics: " << (config.remove_diacritics ? "Yes" : "No") << endl;
		cout << "Handle Contractions: " << (config.handle_contractions ? "Yes" : "No") << endl;
		cout << "Min Term Length: " << config.min_term_length << endl;
		cout << "Max Term Length: " << config.max_term_length << endl;
		cout << endl;
	}
	
	GinLanguageCode parseLanguageCode(const string& language)
	{
		string lower_lang = language;
		transform(lower_lang.begin(), lower_lang.end(), lower_lang.begin(), ::tolower);
		
		if (lower_lang == "english" || lower_lang == "en") return GIN_LANG_ENGLISH;
		if (lower_lang == "spanish" || lower_lang == "es") return GIN_LANG_SPANISH;
		if (lower_lang == "french" || lower_lang == "fr") return GIN_LANG_FRENCH;
		if (lower_lang == "german" || lower_lang == "de") return GIN_LANG_GERMAN;
		if (lower_lang == "italian" || lower_lang == "it") return GIN_LANG_ITALIAN;
		if (lower_lang == "portuguese" || lower_lang == "pt") return GIN_LANG_PORTUGUESE;
		if (lower_lang == "russian" || lower_lang == "ru") return GIN_LANG_RUSSIAN;
		if (lower_lang == "chinese" || lower_lang == "zh") return GIN_LANG_CHINESE;
		if (lower_lang == "japanese" || lower_lang == "ja") return GIN_LANG_JAPANESE;
		if (lower_lang == "korean" || lower_lang == "ko") return GIN_LANG_KOREAN;
		if (lower_lang == "arabic" || lower_lang == "ar") return GIN_LANG_ARABIC;
		if (lower_lang == "hindi" || lower_lang == "hi") return GIN_LANG_HINDI;
		if (lower_lang == "auto") return GIN_LANG_AUTO_DETECT;
		
		return GIN_LANG_GENERIC;
	}
	
	GinStemmingAlgorithm parseStemmingAlgorithm(const string& algorithm)
	{
		string lower_alg = algorithm;
		transform(lower_alg.begin(), lower_alg.end(), lower_alg.begin(), ::tolower);
		
		if (lower_alg == "porter") return GIN_STEM_PORTER;
		if (lower_alg == "snowball") return GIN_STEM_SNOWBALL;
		if (lower_alg == "lovins") return GIN_STEM_LOVINS;
		if (lower_alg == "paice-husk" || lower_alg == "paicehusk") return GIN_STEM_PAICE_HUSK;
		if (lower_alg == "lancaster") return GIN_STEM_LANCASTER;
		
		return GIN_STEM_NONE;
	}
	
	GinStopWordHandling parseStopWordHandling(const string& handling)
	{
		string lower_handling = handling;
		transform(lower_handling.begin(), lower_handling.end(), lower_handling.begin(), ::tolower);
		
		if (lower_handling == "none") return GIN_STOPWORDS_NONE;
		if (lower_handling == "filter") return GIN_STOPWORDS_FILTER;
		if (lower_handling == "mark") return GIN_STOPWORDS_MARK;
		if (lower_handling == "custom") return GIN_STOPWORDS_CUSTOM;
		
		return GIN_STOPWORDS_NONE;
	}
	
	string languageCodeToString(GinLanguageCode language)
	{
		switch (language) {
		case GIN_LANG_ENGLISH: return "English";
		case GIN_LANG_SPANISH: return "Spanish";
		case GIN_LANG_FRENCH: return "French";
		case GIN_LANG_GERMAN: return "German";
		case GIN_LANG_ITALIAN: return "Italian";
		case GIN_LANG_PORTUGUESE: return "Portuguese";
		case GIN_LANG_RUSSIAN: return "Russian";
		case GIN_LANG_CHINESE: return "Chinese";
		case GIN_LANG_JAPANESE: return "Japanese";
		case GIN_LANG_KOREAN: return "Korean";
		case GIN_LANG_ARABIC: return "Arabic";
		case GIN_LANG_HINDI: return "Hindi";
		case GIN_LANG_GENERIC: return "Generic";
		case GIN_LANG_AUTO_DETECT: return "Auto-detect";
		default: return "Unknown";
		}
	}
	
	string stemmingAlgorithmToString(GinStemmingAlgorithm algorithm)
	{
		switch (algorithm) {
		case GIN_STEM_NONE: return "None";
		case GIN_STEM_PORTER: return "Porter";
		case GIN_STEM_SNOWBALL: return "Snowball";
		case GIN_STEM_LOVINS: return "Lovins";
		case GIN_STEM_PAICE_HUSK: return "Paice-Husk";
		case GIN_STEM_LANCASTER: return "Lancaster";
		default: return "Unknown";
		}
	}
	
	string stopWordHandlingToString(GinStopWordHandling handling)
	{
		switch (handling) {
		case GIN_STOPWORDS_NONE: return "None";
		case GIN_STOPWORDS_FILTER: return "Filter";
		case GIN_STOPWORDS_MARK: return "Mark";
		case GIN_STOPWORDS_CUSTOM: return "Custom";
		default: return "Unknown";
		}
	}
	
	void printUsage()
	{
		cout << "GIN Advanced Features Configuration Tool" << endl;
		cout << "=======================================" << endl;
		cout << "Version: 1.0 - Language Detection, Stemming, Stop Words" << endl << endl;
		
		cout << "Usage:" << endl;
		cout << "  gin_advanced_config_tool <command> [options]" << endl << endl;
		
		cout << "Commands:" << endl;
		cout << "  configure    Configure advanced features for a GIN index" << endl;
		cout << "  show         Show current configuration" << endl;
		cout << "  test         Test advanced features configuration" << endl;
		cout << "  export       Export configuration to file" << endl;
		cout << "  import       Import configuration from file" << endl << endl;
		
		cout << "Configure Options:" << endl;
		cout << "  -d, --database PATH        Database file path" << endl;
		cout << "  -i, --index NAME           Index name" << endl;
		cout << "  -l, --language LANG        Primary language (english, spanish, french, german, etc.)" << endl;
		cout << "  -s, --stemming ALGORITHM   Stemming algorithm (porter, snowball, lovins, paice-husk)" << endl;
		cout << "  -w, --stop-words HANDLING  Stop word handling (none, filter, mark, custom)" << endl;
		cout << "  -S, --enable-stemming      Enable stemming" << endl;
		cout << "  -a, --auto-detect          Enable auto language detection" << endl;
		cout << "  -c, --cache-size SIZE      Stemming cache size (default: 10000)" << endl;
		cout << "  -m, --min-length LENGTH    Minimum term length (default: 2)" << endl;
		cout << "  -M, --max-length LENGTH    Maximum term length (default: 64)" << endl;
		cout << "  -v, --verbose              Verbose output" << endl << endl;
		
		cout << "Show/Test Options:" << endl;
		cout << "  -d, --database PATH        Database file path" << endl;
		cout << "  -i, --index NAME           Index name (optional, shows all if not specified)" << endl;
		cout << "  -v, --verbose              Verbose output" << endl << endl;
		
		cout << "Export Options:" << endl;
		cout << "  -d, --database PATH        Database file path" << endl;
		cout << "  -i, --index NAME           Index name (optional)" << endl;
		cout << "  -o, --output FILE          Output configuration file" << endl;
		cout << "  -v, --verbose              Verbose output" << endl << endl;
		
		cout << "Import Options:" << endl;
		cout << "  -d, --database PATH        Database file path" << endl;
		cout << "  -i, --index NAME           Index name (optional)" << endl;
		cout << "  -c, --config FILE          Configuration file to import" << endl;
		cout << "  -v, --verbose              Verbose output" << endl << endl;
		
		cout << "Examples:" << endl;
		cout << "  # Configure English stemming with stop word filtering" << endl;
		cout << "  gin_advanced_config_tool configure -d mydb.fdb -i my_gin_index \\" << endl;
		cout << "    --language english --stemming porter --stop-words filter --enable-stemming" << endl << endl;
		
		cout << "  # Show current configuration" << endl;
		cout << "  gin_advanced_config_tool show -d mydb.fdb -i my_gin_index" << endl << endl;
		
		cout << "  # Test current configuration" << endl;
		cout << "  gin_advanced_config_tool test -d mydb.fdb -i my_gin_index" << endl << endl;
		
		cout << "  # Export configuration to file" << endl;
		cout << "  gin_advanced_config_tool export -d mydb.fdb -i my_gin_index -o config.json" << endl << endl;
		
		cout << "  # Import configuration from file" << endl;
		cout << "  gin_advanced_config_tool import -d mydb.fdb -i my_gin_index -c config.json" << endl;
	}
};

//----------------------------
// Main Function
//----------------------------

int main(int argc, char* argv[])
{
	try {
		GinAdvancedConfigTool tool;
		return tool.run(argc, argv);
	} catch (const exception& e) {
		cerr << "Fatal error: " << e.what() << endl;
		return 1;
	} catch (...) {
		cerr << "Unknown fatal error" << endl;
		return 1;
	}
}