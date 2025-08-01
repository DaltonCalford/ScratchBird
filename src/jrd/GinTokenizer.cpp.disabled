/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		GinTokenizer.cpp  
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

#include "scratchbird.h"
#include "../jrd/GinTokenizer.h"
#include "../jrd/GinIndex.h"
#include "../jrd/constants.h"
#include "../common/gdsassert.h"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <cctype>

using namespace ScratchBird;
using namespace Jrd;
using namespace std::chrono;

namespace Jrd {

//----------------------------
// Token Implementation
//----------------------------

Token::Token(const UCHAR* token_data, USHORT token_length, USHORT pos)
    : length(token_length), data(nullptr), hash(0), position(pos)
{
    if (token_data && token_length > 0) {
        data = (UCHAR*)malloc(token_length + 1);
        if (data) {
            memcpy(data, token_data, token_length);
            data[token_length] = 0; // null terminate
            
            // Calculate hash
            hash = 0;
            for (USHORT i = 0; i < token_length; i++) {
                hash = hash * 31 + data[i];
            }
        }
    }
}

Token::Token(const Token& other)
    : length(other.length), data(nullptr), hash(other.hash), position(other.position)
{
    if (other.data && other.length > 0) {
        data = (UCHAR*)malloc(length + 1);
        if (data) {
            memcpy(data, other.data, length);
            data[length] = 0;
        }
    }
}

Token::~Token()
{
    if (data) {
        free(data);
        data = nullptr;
    }
}

Token& Token::operator=(const Token& other)
{
    if (this != &other) {
        if (data) {
            free(data);
            data = nullptr;
        }
        
        length = other.length;
        hash = other.hash;
        position = other.position;
        
        if (other.data && other.length > 0) {
            data = (UCHAR*)malloc(length + 1);
            if (data) {
                memcpy(data, other.data, length);
                data[length] = 0;
            }
        }
    }
    return *this;
}

bool Token::operator==(const Token& other) const
{
    if (length != other.length || hash != other.hash) {
        return false;
    }
    if (!data && !other.data) {
        return true;
    }
    if (!data || !other.data) {
        return false;
    }
    return memcmp(data, other.data, length) == 0;
}

bool Token::operator<(const Token& other) const
{
    if (!data && !other.data) return false;
    if (!data) return true;
    if (!other.data) return false;
    
    int cmp = memcmp(data, other.data, std::min(length, other.length));
    if (cmp != 0) return cmp < 0;
    return length < other.length;
}

ScratchBird::string Token::toString() const
{
    if (data && length > 0) {
        return ScratchBird::string(reinterpret_cast<const char*>(data), length);
    }
    return ScratchBird::string();
}

void Token::clear()
{
    if (data) {
        free(data);
        data = nullptr;
    }
    length = 0;
    hash = 0;
    position = 0;
}

//----------------------------
// TokenList Implementation
//----------------------------

TokenList::TokenList()
    : m_tokens(nullptr), m_count(0), m_capacity(0)
{
}

TokenList::TokenList(const TokenList& other)
    : m_tokens(nullptr), m_count(0), m_capacity(0)
{
    copyFrom(other);
}

TokenList::~TokenList()
{
    clear();
}

TokenList& TokenList::operator=(const TokenList& other)
{
    if (this != &other) {
        clear();
        copyFrom(other);
    }
    return *this;
}

void TokenList::clear()
{
    if (m_tokens) {
        for (FB_SIZE_T i = 0; i < m_count; i++) {
            m_tokens[i].clear();
        }
        free(m_tokens);
        m_tokens = nullptr;
    }
    m_count = 0;
    m_capacity = 0;
}

const Token& TokenList::operator[](FB_SIZE_T index) const
{
    fb_assert(index < m_count);
    return m_tokens[index];
}

Token& TokenList::operator[](FB_SIZE_T index)
{
    fb_assert(index < m_count);
    return m_tokens[index];
}

void TokenList::add(const Token& token)
{
    if (m_count >= m_capacity) {
        grow();
    }
    if (m_count < m_capacity) {
        m_tokens[m_count] = token;
        m_count++;
    }
}

void TokenList::add(const UCHAR* data, USHORT length, USHORT position)
{
    Token token(data, length, position);
    add(token);
}

void TokenList::remove(FB_SIZE_T index)
{
    if (index < m_count) {
        m_tokens[index].clear();
        for (FB_SIZE_T i = index; i < m_count - 1; i++) {
            m_tokens[i] = m_tokens[i + 1];
        }
        m_count--;
    }
}

void TokenList::reserve(FB_SIZE_T capacity)
{
    if (capacity > m_capacity) {
        Token* new_tokens = (Token*)realloc(m_tokens, capacity * sizeof(Token));
        if (new_tokens) {
            // Initialize new tokens
            for (FB_SIZE_T i = m_capacity; i < capacity; i++) {
                new(&new_tokens[i]) Token();
            }
            m_tokens = new_tokens;
            m_capacity = capacity;
        }
    }
}

void TokenList::grow()
{
    FB_SIZE_T new_capacity = m_capacity == 0 ? 8 : m_capacity * 2;
    reserve(new_capacity);
}

void TokenList::copyFrom(const TokenList& other)
{
    if (other.m_count > 0) {
        reserve(other.m_count);
        for (FB_SIZE_T i = 0; i < other.m_count; i++) {
            add(other.m_tokens[i]);
        }
    }
}

//----------------------------
// Unicode Helper Implementation
//----------------------------

ULONG UnicodeHelper::getCharacter(const UCHAR* utf8_text, USHORT& bytes_consumed)
{
	fb_assert(utf8_text != nullptr);
	
	bytes_consumed = 0;
	ULONG unicode_char = 0;
	
	UCHAR first_byte = utf8_text[0];
	bytes_consumed = 1;
	
	if ((first_byte & 0x80) == 0) {
		// ASCII character (0xxxxxxx)
		unicode_char = first_byte;
	}
	else if ((first_byte & 0xE0) == 0xC0) {
		// 2-byte sequence (110xxxxx 10xxxxxx)
		if ((utf8_text[1] & 0xC0) == 0x80) {
			unicode_char = ((first_byte & 0x1F) << 6) | (utf8_text[1] & 0x3F);
			bytes_consumed = 2;
		}
	}
	else if ((first_byte & 0xF0) == 0xE0) {
		// 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
		if ((utf8_text[1] & 0xC0) == 0x80 && (utf8_text[2] & 0xC0) == 0x80) {
			unicode_char = ((first_byte & 0x0F) << 12) | 
						   ((utf8_text[1] & 0x3F) << 6) | 
						   (utf8_text[2] & 0x3F);
			bytes_consumed = 3;
		}
	}
	else if ((first_byte & 0xF8) == 0xF0) {
		// 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
		if ((utf8_text[1] & 0xC0) == 0x80 && (utf8_text[2] & 0xC0) == 0x80 && (utf8_text[3] & 0xC0) == 0x80) {
			unicode_char = ((first_byte & 0x07) << 18) | 
						   ((utf8_text[1] & 0x3F) << 12) | 
						   ((utf8_text[2] & 0x3F) << 6) | 
						   (utf8_text[3] & 0x3F);
			bytes_consumed = 4;
		}
	}
	
	// Return replacement character for invalid sequences
	if (bytes_consumed == 1 && (first_byte & 0x80) != 0) {
		unicode_char = 0xFFFD; // Unicode replacement character
	}
	
	return unicode_char;
}

UnicodeHelper::CharCategory UnicodeHelper::getCharacterCategory(ULONG unicode_char)
{
	// Simplified character classification for basic Unicode support
	// Full implementation would use Unicode character database
	
	if (unicode_char <= 0x7F) {
		// ASCII range
		if ((unicode_char >= 'A' && unicode_char <= 'Z') ||
			(unicode_char >= 'a' && unicode_char <= 'z')) {
			return CATEGORY_LETTER;
		}
		else if (unicode_char >= '0' && unicode_char <= '9') {
			return CATEGORY_DIGIT;
		}
		else if (unicode_char == ' ' || unicode_char == '\t' || 
				 unicode_char == '\n' || unicode_char == '\r') {
			return CATEGORY_SEPARATOR;
		}
		else if (unicode_char < 32 || unicode_char == 127) {
			return CATEGORY_OTHER;
		}
		else {
			return CATEGORY_PUNCTUATION;
		}
	}
	
	// Extended ranges for common Unicode blocks
	if (unicode_char >= 0x00C0 && unicode_char <= 0x024F) {
		// Latin Extended-A and Extended-B
		return CATEGORY_LETTER;
	}
	else if (unicode_char >= 0x0370 && unicode_char <= 0x03FF) {
		// Greek and Coptic
		return CATEGORY_LETTER;
	}
	else if (unicode_char >= 0x0400 && unicode_char <= 0x04FF) {
		// Cyrillic
		return CATEGORY_LETTER;
	}
	else if (unicode_char >= 0x2000 && unicode_char <= 0x206F) {
		// General Punctuation
		return CATEGORY_PUNCTUATION;
	}
	else if (unicode_char >= 0x2070 && unicode_char <= 0x209F) {
		// Superscripts and Subscripts
		return CATEGORY_DIGIT;
	}
	
	// Default to letter for most other characters
	return CATEGORY_LETTER;
}

bool UnicodeHelper::isLetter(ULONG unicode_char)
{
	CharCategory category = getCharacterCategory(unicode_char);
	return (category & CATEGORY_LETTER) != 0;
}

bool UnicodeHelper::isDigit(ULONG unicode_char)
{
	CharCategory category = getCharacterCategory(unicode_char);
	return (category & CATEGORY_DIGIT) != 0;
}

bool UnicodeHelper::isLetterOrDigit(ULONG unicode_char)
{
	CharCategory category = getCharacterCategory(unicode_char);
	return (category & (CATEGORY_LETTER | CATEGORY_DIGIT)) != 0;
}

bool UnicodeHelper::isWhitespace(ULONG unicode_char)
{
	CharCategory category = getCharacterCategory(unicode_char);
	return (category & CATEGORY_SEPARATOR) != 0;
}

bool UnicodeHelper::isPunctuation(ULONG unicode_char)
{
	CharCategory category = getCharacterCategory(unicode_char);
	return (category & CATEGORY_PUNCTUATION) != 0;
}

bool UnicodeHelper::isControl(ULONG unicode_char)
{
	return unicode_char < 32 || unicode_char == 127 ||
		   (unicode_char >= 0x80 && unicode_char <= 0x9F);
}

ULONG UnicodeHelper::toLower(ULONG unicode_char)
{
	// Basic ASCII case conversion
	if (unicode_char >= 'A' && unicode_char <= 'Z') {
		return unicode_char + 32;
	}
	
	// Extended Latin case conversion (simplified)
	if (unicode_char >= 0x00C0 && unicode_char <= 0x00DF) {
		if (unicode_char != 0x00D7) { // Multiplication sign
			return unicode_char + 32;
		}
	}
	
	return unicode_char;
}

ULONG UnicodeHelper::toUpper(ULONG unicode_char)
{
	// Basic ASCII case conversion
	if (unicode_char >= 'a' && unicode_char <= 'z') {
		return unicode_char - 32;
	}
	
	// Extended Latin case conversion (simplified)
	if (unicode_char >= 0x00E0 && unicode_char <= 0x00FF) {
		if (unicode_char != 0x00F7) { // Division sign
			return unicode_char - 32;
		}
	}
	
	return unicode_char;
}

ULONG UnicodeHelper::toTitle(ULONG unicode_char)
{
	// For most characters, title case is the same as uppercase
	return toUpper(unicode_char);
}

ScratchBird::string UnicodeHelper::normalize(const UCHAR* utf8_text, USHORT length, NormalizationForm form)
{
	// Simplified normalization implementation
	// Full implementation would use ICU library or similar
	
	ScratchBird::string result;
	result.reserve(length);
	
	for (USHORT i = 0; i < length; ) {
		USHORT bytes_consumed;
		ULONG unicode_char = getCharacter(&utf8_text[i], bytes_consumed);
		
		if (bytes_consumed > 0) {
			// Apply normalization (simplified - just copy for now)
			UCHAR utf8_buffer[4];
			USHORT utf8_length = encodeUtf8(unicode_char, utf8_buffer);
			result.append(reinterpret_cast<char*>(utf8_buffer), utf8_length);
			i += bytes_consumed;
		} else {
			i++; // Skip invalid bytes
		}
	}
	
	return result;
}

ScratchBird::string UnicodeHelper::normalize(const ScratchBird::string& text, NormalizationForm form)
{
	return normalize(reinterpret_cast<const UCHAR*>(text.c_str()), text.length(), form);
}

USHORT UnicodeHelper::getUtf8ByteLength(ULONG unicode_char)
{
	if (unicode_char <= 0x7F) return 1;
	else if (unicode_char <= 0x7FF) return 2;
	else if (unicode_char <= 0xFFFF) return 3;
	else if (unicode_char <= 0x10FFFF) return 4;
	else return 0; // Invalid character
}

USHORT UnicodeHelper::encodeUtf8(ULONG unicode_char, UCHAR* buffer)
{
	fb_assert(buffer != nullptr);
	
	if (unicode_char <= 0x7F) {
		buffer[0] = static_cast<UCHAR>(unicode_char);
		return 1;
	}
	else if (unicode_char <= 0x7FF) {
		buffer[0] = 0xC0 | static_cast<UCHAR>(unicode_char >> 6);
		buffer[1] = 0x80 | static_cast<UCHAR>(unicode_char & 0x3F);
		return 2;
	}
	else if (unicode_char <= 0xFFFF) {
		buffer[0] = 0xE0 | static_cast<UCHAR>(unicode_char >> 12);
		buffer[1] = 0x80 | static_cast<UCHAR>((unicode_char >> 6) & 0x3F);
		buffer[2] = 0x80 | static_cast<UCHAR>(unicode_char & 0x3F);
		return 3;
	}
	else if (unicode_char <= 0x10FFFF) {
		buffer[0] = 0xF0 | static_cast<UCHAR>(unicode_char >> 18);
		buffer[1] = 0x80 | static_cast<UCHAR>((unicode_char >> 12) & 0x3F);
		buffer[2] = 0x80 | static_cast<UCHAR>((unicode_char >> 6) & 0x3F);
		buffer[3] = 0x80 | static_cast<UCHAR>(unicode_char & 0x3F);
		return 4;
	}
	else {
		return 0; // Invalid character
	}
}

bool UnicodeHelper::isValidUtf8Sequence(const UCHAR* text, USHORT length)
{
	for (USHORT i = 0; i < length; ) {
		USHORT bytes_consumed;
		ULONG unicode_char = getCharacter(&text[i], bytes_consumed);
		
		if (bytes_consumed == 0 || i + bytes_consumed > length) {
			return false;
		}
		
		i += bytes_consumed;
	}
	
	return true;
}

//----------------------------
// Language Processor Implementation
//----------------------------

LanguageProcessor::LanguageProcessor(LanguageCode language)
	: m_language(language)
{
	initializeStopWords();
}

LanguageProcessor::~LanguageProcessor()
{
}

bool LanguageProcessor::isStopWord(const Token& token) const
{
	ScratchBird::string word(token.c_str());
	
	// Convert to lowercase for comparison
	std::transform(word.begin(), word.end(), word.begin(), 
		[](char c) { return std::tolower(c); });
	
	GenericMap<Pair<ScratchBird::string, bool>>::const_iterator it = 
		m_stop_words.locate(Pair<ScratchBird::string, bool>(word, true));
	
	return it != m_stop_words.end();
}

Token LanguageProcessor::applyStemming(const Token& token) const
{
	switch (m_language) {
		case LANG_ENGLISH:
			return stemEnglish(token);
		case LANG_SPANISH:
			return stemSpanish(token);
		case LANG_FRENCH:
			return stemFrench(token);
		case LANG_GERMAN:
			return stemGerman(token);
		default:
			return token; // No stemming for unknown languages
	}
}

TokenList LanguageProcessor::applyLanguageSpecificRules(const TokenList& tokens) const
{
	TokenList result = tokens;
	
	// Apply language-specific processing rules
	switch (m_language) {
		case LANG_GERMAN:
			// German compound word handling (simplified)
			// TODO: Implement compound word splitting
			break;
		case LANG_CHINESE:
		case LANG_JAPANESE:
			// CJK language processing (simplified)
			// TODO: Implement proper CJK tokenization
			break;
		default:
			break;
	}
	
	return result;
}

void LanguageProcessor::setLanguage(LanguageCode language)
{
	if (m_language != language) {
		m_language = language;
		m_stop_words.clear();
		initializeStopWords();
	}
}

void LanguageProcessor::loadStopWords(const ScratchBird::string& stop_word_list)
{
	// Parse stop word list (comma-separated)
	size_t start = 0;
	size_t end = stop_word_list.find(',');
	
	while (end != ScratchBird::string::npos) {
		ScratchBird::string word = stop_word_list.substr(start, end - start);
		// Trim whitespace
		word.erase(0, word.find_first_not_of(" \t"));
		word.erase(word.find_last_not_of(" \t") + 1);
		
		if (!word.empty()) {
			addStopWord(word);
		}
		
		start = end + 1;
		end = stop_word_list.find(',', start);
	}
	
	// Handle last word
	if (start < stop_word_list.length()) {
		ScratchBird::string word = stop_word_list.substr(start);
		word.erase(0, word.find_first_not_of(" \t"));
		word.erase(word.find_last_not_of(" \t") + 1);
		
		if (!word.empty()) {
			addStopWord(word);
		}
	}
}

void LanguageProcessor::addStopWord(const ScratchBird::string& word)
{
	ScratchBird::string lowercase_word = word;
	std::transform(lowercase_word.begin(), lowercase_word.end(), lowercase_word.begin(), 
		[](char c) { return std::tolower(c); });
	
	m_stop_words.put(Pair<ScratchBird::string, bool>(lowercase_word, true));
}

void LanguageProcessor::removeStopWord(const ScratchBird::string& word)
{
	ScratchBird::string lowercase_word = word;
	std::transform(lowercase_word.begin(), lowercase_word.end(), lowercase_word.begin(), 
		[](char c) { return std::tolower(c); });
	
	GenericMap<Pair<ScratchBird::string, bool>>::iterator it = 
		m_stop_words.locate(Pair<ScratchBird::string, bool>(lowercase_word, true));
	
	if (it != m_stop_words.end()) {
		m_stop_words.remove(it);
	}
}

ULONG LanguageProcessor::getStopWordCount() const
{
	return m_stop_words.count();
}

void LanguageProcessor::initializeStopWords()
{
	m_stop_words.clear();
	
	switch (m_language) {
		case LANG_ENGLISH:
		{
			const char* english_stop_words[] = {
				"a", "an", "and", "are", "as", "at", "be", "by", "for", "from",
				"has", "he", "in", "is", "it", "its", "of", "on", "that", "the",
				"to", "was", "were", "will", "with", "i", "you", "we", "they",
				"this", "these", "those", "but", "or", "not", "can", "could",
				"would", "should", "have", "had", "do", "does", "did", "been",
				nullptr
			};
			
			for (int i = 0; english_stop_words[i] != nullptr; i++) {
				addStopWord(english_stop_words[i]);
			}
			break;
		}
		case LANG_SPANISH:
		{
			const char* spanish_stop_words[] = {
				"el", "la", "de", "que", "y", "a", "en", "un", "ser", "se",
				"no", "te", "lo", "le", "da", "su", "por", "son", "con", "para",
				"al", "del", "los", "las", "una", "como", "pero", "sus", "ya",
				nullptr
			};
			
			for (int i = 0; spanish_stop_words[i] != nullptr; i++) {
				addStopWord(spanish_stop_words[i]);
			}
			break;
		}
		case LANG_FRENCH:
		{
			const char* french_stop_words[] = {
				"le", "de", "et", "à", "un", "il", "être", "et", "en", "avoir",
				"que", "pour", "dans", "ce", "son", "une", "sur", "avec", "ne",
				"se", "pas", "tout", "plus", "par", "grand", "en", "mais", "du",
				nullptr
			};
			
			for (int i = 0; french_stop_words[i] != nullptr; i++) {
				addStopWord(french_stop_words[i]);
			}
			break;
		}
		case LANG_GERMAN:
		{
			const char* german_stop_words[] = {
				"der", "die", "und", "in", "den", "von", "zu", "das", "mit", "sich",
				"des", "auf", "für", "ist", "im", "dem", "nicht", "ein", "eine", "als",
				"auch", "es", "an", "werden", "aus", "er", "hat", "dass", "sie", "nach",
				nullptr
			};
			
			for (int i = 0; german_stop_words[i] != nullptr; i++) {
				addStopWord(german_stop_words[i]);
			}
			break;
		}
		default:
			// No language-specific stop words for neutral or unknown languages
			break;
	}
}

Token LanguageProcessor::stemEnglish(const Token& token) const
{
	ScratchBird::string word(token.c_str());
	ScratchBird::string stemmed = porterStem(word);
	
	// Convert back to Token
	if (stemmed.length() <= GIN_MAX_TOKEN_LENGTH) {
		return Token(reinterpret_cast<const UCHAR*>(stemmed.c_str()), stemmed.length());
	}
	
	return token;
}

Token LanguageProcessor::stemSpanish(const Token& token) const
{
	// Simplified Spanish stemming
	Token result = token;
	
	if (result.length > 4) {
		// Remove common Spanish suffixes
		const char* suffixes[] = { "mente", "ción", "ante", "ado", "ida", nullptr };
		
		for (int i = 0; suffixes[i] != nullptr; i++) {
			size_t suffix_len = strlen(suffixes[i]);
			if (result.length >= suffix_len) {
				if (memcmp(&result.data[result.length - suffix_len], suffixes[i], suffix_len) == 0) {
					result.length -= suffix_len;
					result.data[result.length] = 0;
					break;
				}
			}
		}
	}
	
	return result;
}

Token LanguageProcessor::stemFrench(const Token& token) const
{
	// Simplified French stemming
	Token result = token;
	
	if (result.length > 4) {
		// Remove common French suffixes
		const char* suffixes[] = { "ment", "tion", "able", "ique", nullptr };
		
		for (int i = 0; suffixes[i] != nullptr; i++) {
			size_t suffix_len = strlen(suffixes[i]);
			if (result.length >= suffix_len) {
				if (memcmp(&result.data[result.length - suffix_len], suffixes[i], suffix_len) == 0) {
					result.length -= suffix_len;
					result.data[result.length] = 0;
					break;
				}
			}
		}
	}
	
	return result;
}

Token LanguageProcessor::stemGerman(const Token& token) const
{
	// Simplified German stemming
	Token result = token;
	
	if (result.length > 4) {
		// Remove common German suffixes
		const char* suffixes[] = { "heit", "keit", "ung", "end", nullptr };
		
		for (int i = 0; suffixes[i] != nullptr; i++) {
			size_t suffix_len = strlen(suffixes[i]);
			if (result.length >= suffix_len) {
				if (memcmp(&result.data[result.length - suffix_len], suffixes[i], suffix_len) == 0) {
					result.length -= suffix_len;
					result.data[result.length] = 0;
					break;
				}
			}
		}
	}
	
	return result;
}

ScratchBird::string LanguageProcessor::porterStem(const ScratchBird::string& word) const
{
	// Simplified Porter stemmer implementation
	ScratchBird::string result = word;
	
	// Convert to lowercase
	std::transform(result.begin(), result.end(), result.begin(), 
		[](char c) { return std::tolower(c); });
	
	if (result.length() <= 2) {
		return result;
	}
	
	// Step 1a
	if (result.length() >= 4 && result.substr(result.length() - 4) == "sses") {
		result = result.substr(0, result.length() - 2); // sses -> ss
	}
	else if (result.length() >= 3 && result.substr(result.length() - 3) == "ies") {
		result = result.substr(0, result.length() - 2); // ies -> i
	}
	else if (result.length() >= 2 && result.back() == 's' && result[result.length() - 2] != 's') {
		result.pop_back(); // s -> (empty)
	}
	
	// Step 1b (simplified)
	if (result.length() >= 3 && result.substr(result.length() - 3) == "ing") {
		if (containsVowel(result.substr(0, result.length() - 3))) {
			result = result.substr(0, result.length() - 3);
		}
	}
	else if (result.length() >= 2 && result.substr(result.length() - 2) == "ed") {
		if (containsVowel(result.substr(0, result.length() - 2))) {
			result = result.substr(0, result.length() - 2);
		}
	}
	
	return result;
}

bool LanguageProcessor::isVowel(const ScratchBird::string& word, USHORT position) const
{
	if (position >= word.length()) return false;
	
	char c = std::tolower(word[position]);
	
	if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') {
		return true;
	}
	
	if (c == 'y' && position > 0) {
		return !isVowel(word, position - 1);
	}
	
	return false;
}

USHORT LanguageProcessor::getMeasure(const ScratchBird::string& word) const
{
	USHORT measure = 0;
	bool prev_was_vowel = false;
	
	for (USHORT i = 0; i < word.length(); i++) {
		bool is_vowel = isVowel(word, i);
		
		if (!is_vowel && prev_was_vowel) {
			measure++;
		}
		
		prev_was_vowel = is_vowel;
	}
	
	return measure;
}

bool LanguageProcessor::containsVowel(const ScratchBird::string& word) const
{
	for (USHORT i = 0; i < word.length(); i++) {
		if (isVowel(word, i)) {
			return true;
		}
	}
	
	return false;
}

bool LanguageProcessor::endsWithDoubleConsonant(const ScratchBird::string& word) const
{
	if (word.length() < 2) return false;
	
	char last = word.back();
	char second_last = word[word.length() - 2];
	
	return (last == second_last) && !isVowel(word, word.length() - 1) && !isVowel(word, word.length() - 2);
}

bool LanguageProcessor::cvc(const ScratchBird::string& word, USHORT position) const
{
	if (position < 2 || position >= word.length()) return false;
	
	if (!isVowel(word, position - 2) && isVowel(word, position - 1) && !isVowel(word, position)) {
		char last = std::tolower(word[position]);
		return (last != 'w' && last != 'x' && last != 'y');
	}
	
	return false;
}

//----------------------------
// TokenizerConfig Implementation
//----------------------------

TokenizerConfig::TokenizerConfig()
{
	setDefaults();
}

void TokenizerConfig::setDefaults()
{
	type = GinTokenizer::STANDARD_TOKENIZER;
	min_token_length = GIN_DEFAULT_MIN_TOKEN_LENGTH;
	max_token_length = GIN_DEFAULT_MAX_TOKEN_LENGTH;
	case_sensitive = false;
	preserve_case = false;
	
	normalization = UnicodeHelper::NORM_NFC;
	handle_diacritics = true;
	fold_case = true;
	
	language = LanguageProcessor::LANG_NEUTRAL;
	enable_stop_words = false;
	enable_stemming = false;
	enable_language_rules = false;
	
	enable_ngrams = false;
	ngram_size = 3;
	enable_phonetic = false;
	enable_synonyms = false;
	
	max_tokens_per_document = GIN_MAX_TOKENS_PER_DOCUMENT;
	max_token_cache_size = 1000;
	enable_token_caching = true;
}

void TokenizerConfig::validate()
{
	if (min_token_length < GIN_MIN_TOKEN_LENGTH) {
		min_token_length = GIN_MIN_TOKEN_LENGTH;
	}
	
	if (max_token_length > GIN_MAX_TOKEN_LENGTH) {
		max_token_length = GIN_MAX_TOKEN_LENGTH;
	}
	
	if (min_token_length > max_token_length) {
		max_token_length = min_token_length;
	}
	
	if (ngram_size < 2) {
		ngram_size = 2;
	}
	
	if (ngram_size > 10) {
		ngram_size = 10;
	}
}

//----------------------------
// NGramGenerator Implementation
//----------------------------

NGramGenerator::NGramGenerator(USHORT ngram_size)
	: m_ngram_size(ngram_size), m_ngram_type(NGRAM_CHARACTER)
{
}

NGramGenerator::~NGramGenerator()
{
}

TokenList NGramGenerator::generateNGrams(const Token& token) const
{
	switch (m_ngram_type) {
		case NGRAM_CHARACTER:
		case NGRAM_MIXED:
			return generateCharacterNGrams(token);
		default:
			return TokenList();
	}
}

TokenList NGramGenerator::generateNGrams(const TokenList& tokens) const
{
	switch (m_ngram_type) {
		case NGRAM_WORD:
		case NGRAM_MIXED:
			return generateWordNGrams(tokens);
		default:
			return TokenList();
	}
}

void NGramGenerator::setNGramSize(USHORT size)
{
	m_ngram_size = (size >= 2) ? size : 2;
}

void NGramGenerator::setNGramType(NGramType type)
{
	m_ngram_type = type;
}

TokenList NGramGenerator::generateCharacterNGrams(const Token& token) const
{
	TokenList ngrams;
	
	if (token.length < m_ngram_size) {
		return ngrams;
	}
	
	for (USHORT i = 0; i <= token.length - m_ngram_size; i++) {
		Token ngram(&token.data[i], m_ngram_size);
		ngrams.add(ngram);
	}
	
	return ngrams;
}

TokenList NGramGenerator::generateWordNGrams(const TokenList& tokens) const
{
	TokenList ngrams;
	
	if (tokens.getCount() < m_ngram_size) {
		return ngrams;
	}
	
	for (FB_SIZE_T i = 0; i <= tokens.getCount() - m_ngram_size; i++) {
		// Create combined token from n consecutive words
		ScratchBird::string combined;
		
		for (USHORT j = 0; j < m_ngram_size; j++) {
			if (j > 0) combined += " ";
			combined += tokens[i + j].c_str();
		}
		
		if (combined.length() <= GIN_MAX_TOKEN_LENGTH) {
			Token ngram(reinterpret_cast<const UCHAR*>(combined.c_str()), combined.length());
			ngrams.add(ngram);
		}
	}
	
	return ngrams;
}

//----------------------------
// PhoneticProcessor Implementation
//----------------------------

PhoneticProcessor::PhoneticProcessor(PhoneticAlgorithm algorithm)
	: m_algorithm(algorithm)
{
}

PhoneticProcessor::~PhoneticProcessor()
{
}

ScratchBird::string PhoneticProcessor::encode(const Token& token) const
{
	ScratchBird::string word(token.c_str());
	return encode(word);
}

ScratchBird::string PhoneticProcessor::encode(const ScratchBird::string& word) const
{
	switch (m_algorithm) {
		case PHONETIC_SOUNDEX:
			return soundex(word);
		case PHONETIC_METAPHONE:
			return metaphone(word);
		case PHONETIC_DOUBLE_METAPHONE:
			return doubleMetaphone(word);
		case PHONETIC_NYSIIS:
			return nysiis(word);
		default:
			return word;
	}
}

bool PhoneticProcessor::matches(const Token& token1, const Token& token2) const
{
	ScratchBird::string code1 = encode(token1);
	ScratchBird::string code2 = encode(token2);
	
	return code1 == code2;
}

double PhoneticProcessor::similarity(const Token& token1, const Token& token2) const
{
	ScratchBird::string code1 = encode(token1);
	ScratchBird::string code2 = encode(token2);
	
	if (code1 == code2) return 1.0;
	if (code1.empty() || code2.empty()) return 0.0;
	
	// Simple Levenshtein distance-based similarity
	size_t len1 = code1.length();
	size_t len2 = code2.length();
	size_t max_len = std::max(len1, len2);
	
	if (max_len == 0) return 1.0;
	
	// Simplified distance calculation
	size_t differences = 0;
	size_t min_len = std::min(len1, len2);
	
	for (size_t i = 0; i < min_len; i++) {
		if (code1[i] != code2[i]) {
			differences++;
		}
	}
	
	differences += (max_len - min_len);
	
	return 1.0 - (static_cast<double>(differences) / max_len);
}

void PhoneticProcessor::setAlgorithm(PhoneticAlgorithm algorithm)
{
	m_algorithm = algorithm;
}

ScratchBird::string PhoneticProcessor::soundex(const ScratchBird::string& word) const
{
	if (word.empty()) return "";
	
	ScratchBird::string result;
	result.reserve(4);
	
	// First character (uppercase)
	result += std::toupper(word[0]);
	
	// Soundex mapping
	const char* soundex_map = "01230120022455012623010202";
	
	for (size_t i = 1; i < word.length() && result.length() < 4; i++) {
		char c = std::toupper(word[i]);
		
		if (c >= 'A' && c <= 'Z') {
			char code = soundex_map[c - 'A'];
			
			if (code != '0' && (result.length() == 1 || result.back() != code)) {
				result += code;
			}
		}
	}
	
	// Pad with zeros
	while (result.length() < 4) {
		result += '0';
	}
	
	return result;
}

ScratchBird::string PhoneticProcessor::metaphone(const ScratchBird::string& word) const
{
	// Simplified Metaphone implementation
	if (word.empty()) return "";
	
	ScratchBird::string upper_word = word;
	std::transform(upper_word.begin(), upper_word.end(), upper_word.begin(), 
		[](char c) { return std::toupper(c); });
	
	ScratchBird::string result;
	result.reserve(upper_word.length());
	
	for (size_t i = 0; i < upper_word.length(); i++) {
		char c = upper_word[i];
		
		switch (c) {
			case 'A': case 'E': case 'I': case 'O': case 'U':
				if (i == 0) result += c;
				break;
			case 'B':
				result += 'B';
				break;
			case 'C':
				if (i + 1 < upper_word.length() && upper_word[i + 1] == 'H') {
					result += 'X';
					i++; // Skip H
				} else {
					result += 'K';
				}
				break;
			case 'D':
				result += 'T';
				break;
			case 'F':
				result += 'F';
				break;
			case 'G':
				result += 'K';
				break;
			case 'H':
				// Skip H unless at beginning
				if (i == 0) result += 'H';
				break;
			case 'J':
				result += 'J';
				break;
			case 'K':
				result += 'K';
				break;
			case 'L':
				result += 'L';
				break;
			case 'M':
				result += 'M';
				break;
			case 'N':
				result += 'N';
				break;
			case 'P':
				if (i + 1 < upper_word.length() && upper_word[i + 1] == 'H') {
					result += 'F';
					i++; // Skip H
				} else {
					result += 'P';
				}
				break;
			case 'Q':
				result += 'K';
				break;
			case 'R':
				result += 'R';
				break;
			case 'S':
				result += 'S';
				break;
			case 'T':
				if (i + 1 < upper_word.length() && upper_word[i + 1] == 'H') {
					result += '0';
					i++; // Skip H
				} else {
					result += 'T';
				}
				break;
			case 'V':
				result += 'F';
				break;
			case 'W': case 'Y':
				if (i == 0 || (i > 0 && strchr("AEIOU", upper_word[i - 1]) == nullptr)) {
					result += c;
				}
				break;
			case 'X':
				result += "KS";
				break;
			case 'Z':
				result += 'S';
				break;
			default:
				// Skip other characters
				break;
		}
	}
	
	return result;
}

ScratchBird::string PhoneticProcessor::doubleMetaphone(const ScratchBird::string& word) const
{
	// For now, use simplified metaphone
	// TODO: Implement full Double Metaphone algorithm
	return metaphone(word);
}

ScratchBird::string PhoneticProcessor::nysiis(const ScratchBird::string& word) const
{
	// Simplified NYSIIS implementation
	if (word.empty()) return "";
	
	ScratchBird::string result = word;
	std::transform(result.begin(), result.end(), result.begin(), 
		[](char c) { return std::toupper(c); });
	
	// Basic NYSIIS transformations (simplified)
	// TODO: Implement full NYSIIS algorithm
	
	return result;
}

//----------------------------
// TokenizerFactory Implementation
//----------------------------

GinTokenizer* TokenizerFactory::createTokenizer(const TokenizerConfig& config)
{
	switch (config.type) {
		case GinTokenizer::SIMPLE_TOKENIZER:
			return createSimpleTokenizer();
		case GinTokenizer::STANDARD_TOKENIZER:
			return createStandardTokenizer();
		case GinTokenizer::LANGUAGE_TOKENIZER:
		default:
			return createAdvancedTokenizer();
	}
}

GinTokenizer* TokenizerFactory::createSimpleTokenizer()
{
	return FB_NEW_POOL(getDefaultMemoryPool()) GinTokenizer(GinTokenizer::SIMPLE_TOKENIZER);
}

GinTokenizer* TokenizerFactory::createStandardTokenizer()
{
	return FB_NEW_POOL(getDefaultMemoryPool()) GinTokenizer(GinTokenizer::STANDARD_TOKENIZER);
}

GinTokenizer* TokenizerFactory::createAdvancedTokenizer()
{
	TokenizerConfig config;
	config.type = GinTokenizer::STANDARD_TOKENIZER;
	config.enable_stop_words = true;
	config.enable_stemming = true;
	config.handle_diacritics = true;
	
	return FB_NEW_POOL(getDefaultMemoryPool()) AdvancedGinTokenizer(config);
}

GinTokenizer* TokenizerFactory::createLanguageTokenizer(LanguageProcessor::LanguageCode language)
{
	TokenizerConfig config = getLanguageConfig(language);
	return FB_NEW_POOL(getDefaultMemoryPool()) AdvancedGinTokenizer(config);
}

TokenizerConfig TokenizerFactory::getDefaultConfig()
{
	return TokenizerConfig();
}

TokenizerConfig TokenizerFactory::getPerformanceConfig()
{
	TokenizerConfig config;
	config.type = GinTokenizer::SIMPLE_TOKENIZER;
	config.enable_stop_words = false;
	config.enable_stemming = false;
	config.enable_token_caching = true;
	config.max_token_cache_size = 5000;
	
	return config;
}

TokenizerConfig TokenizerFactory::getQualityConfig()
{
	TokenizerConfig config;
	config.type = GinTokenizer::LANGUAGE_TOKENIZER;
	config.enable_stop_words = true;
	config.enable_stemming = true;
	config.enable_language_rules = true;
	config.handle_diacritics = true;
	config.fold_case = true;
	config.normalization = UnicodeHelper::NORM_NFC;
	
	return config;
}

TokenizerConfig TokenizerFactory::getLanguageConfig(LanguageProcessor::LanguageCode language)
{
	TokenizerConfig config = getQualityConfig();
	config.language = language;
	
	// Language-specific optimizations
	switch (language) {
		case LanguageProcessor::LANG_ENGLISH:
			config.enable_stemming = true;
			config.enable_stop_words = true;
			break;
		case LanguageProcessor::LANG_GERMAN:
			config.enable_language_rules = true; // For compound words
			break;
		case LanguageProcessor::LANG_CHINESE:
		case LanguageProcessor::LANG_JAPANESE:
			config.enable_ngrams = true;
			config.ngram_size = 2;
			break;
		default:
			break;
	}
	
	return config;
}

bool TokenizerFactory::supportsUnicode(GinTokenizer::TokenizerType type)
{
	return type == GinTokenizer::STANDARD_TOKENIZER || type == GinTokenizer::LANGUAGE_TOKENIZER;
}

bool TokenizerFactory::supportsLanguage(GinTokenizer::TokenizerType type)
{
	return type == GinTokenizer::LANGUAGE_TOKENIZER;
}

bool TokenizerFactory::supportsAdvancedFeatures(GinTokenizer::TokenizerType type)
{
	return type == GinTokenizer::LANGUAGE_TOKENIZER;
}

} // namespace Jrd