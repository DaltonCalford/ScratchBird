/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		GinIndex.cpp  
 *	DESCRIPTION:	GIN (Generalized Inverted) index implementation for full-text search
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
 * 2025.07.22 - ScratchBird GIN Index Implementation
 */

#include "scratchbird.h"
#include "../jrd/GinIndex.h"
#include "../jrd/GinCompression.h"
#include "../jrd/GinPageManager.h"
#include "../jrd/GinQueryProcessor.h"
#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../jrd/constants.h"
#include "../jrd/val.h"
#include "../jrd/exe.h"
#include "../jrd/req.h"
#include "../jrd/btr.h"
#include "../common/gdsassert.h"
#include "../common/classes/array.h"
#include <cstring>
#include <algorithm>
#include <memory>

using namespace ScratchBird;
using namespace Jrd;

namespace Jrd {

//----------------------------
// Token Implementation
//----------------------------

Token::Token()
{
	length = 0;
	memset(data, 0, sizeof(data));
}

Token::Token(const UCHAR* text, USHORT len)
{
	fb_assert(text != nullptr);
	fb_assert(len <= GIN_MAX_TOKEN_LENGTH);
	
	length = (len > GIN_MAX_TOKEN_LENGTH) ? GIN_MAX_TOKEN_LENGTH : len;
	memcpy(data, text, length);
	
	// Null-terminate for safety
	if (length < GIN_MAX_TOKEN_LENGTH) {
		data[length] = 0;
	}
}

Token::Token(const char* text)
{
	fb_assert(text != nullptr);
	
	USHORT len = strlen(text);
	length = (len > GIN_MAX_TOKEN_LENGTH) ? GIN_MAX_TOKEN_LENGTH : len;
	memcpy(data, text, length);
	
	// Null-terminate for safety
	if (length < GIN_MAX_TOKEN_LENGTH) {
		data[length] = 0;
	}
}

Token::Token(const Token& other)
{
	length = other.length;
	memcpy(data, other.data, length);
	
	// Null-terminate for safety
	if (length < GIN_MAX_TOKEN_LENGTH) {
		data[length] = 0;
	}
}

Token& Token::operator=(const Token& other)
{
	if (this != &other) {
		length = other.length;
		memcpy(data, other.data, length);
		
		// Null-terminate for safety
		if (length < GIN_MAX_TOKEN_LENGTH) {
			data[length] = 0;
		}
	}
	return *this;
}

bool Token::operator==(const Token& other) const
{
	return (length == other.length) && 
		   (memcmp(data, other.data, length) == 0);
}

bool Token::operator!=(const Token& other) const
{
	return !(*this == other);
}

bool Token::operator<(const Token& other) const
{
	int cmp = memcmp(data, other.data, std::min(length, other.length));
	if (cmp != 0) {
		return cmp < 0;
	}
	return length < other.length;
}

ULONG Token::hash() const
{
	// Simple hash function for tokens
	ULONG hash = 5381;
	for (USHORT i = 0; i < length; i++) {
		hash = ((hash << 5) + hash) + data[i];
	}
	return hash;
}

const char* Token::c_str() const
{
	// Note: This assumes null termination was handled in constructors
	return reinterpret_cast<const char*>(data);
}

bool Token::isEmpty() const
{
	return length == 0;
}

void Token::clear()
{
	length = 0;
	memset(data, 0, sizeof(data));
}

//----------------------------
// PostingListEntry Implementation
//----------------------------

PostingListEntry::PostingListEntry()
{
	record_count = 0;
	compression_type = 0;
	compressed_size = 0;
	posting_data = nullptr;
}

PostingListEntry::PostingListEntry(const PostingList& list, UCHAR compression)
{
	record_count = list.getCount();
	compression_type = compression;
	compressed_size = 0;
	posting_data = nullptr;
	
	if (record_count > 0) {
		// Use compression algorithms from GinCompression
		CompressionType comp_type = static_cast<CompressionType>(compression);
		
		// Allocate buffer for compression
		ULONG buffer_size = GinCompression::COMPRESSION_BUFFER_SIZE;
		UCHAR* temp_buffer = (UCHAR*)FB_NEW_POOL(getDefaultMemoryPool()) UCHAR[buffer_size];
		
		try {
			// Compress the posting list
			CompressionStats stats;
			compressed_size = GinCompression::compress(list, comp_type, temp_buffer, buffer_size, &stats);
			
			if (compressed_size > 0 && compressed_size < buffer_size) {
				// Allocate exact size for compressed data
				posting_data = (UCHAR*)FB_NEW_POOL(getDefaultMemoryPool()) UCHAR[compressed_size];
				memcpy(posting_data, temp_buffer, compressed_size);
			} else {
				// Fall back to uncompressed storage if compression failed
				compression_type = COMPRESSION_NONE;
				compressed_size = record_count * sizeof(RecordNumber);
				posting_data = (UCHAR*)FB_NEW_POOL(getDefaultMemoryPool()) UCHAR[compressed_size];
				
				RecordNumber* records = reinterpret_cast<RecordNumber*>(posting_data);
				for (FB_SIZE_T i = 0; i < record_count; i++) {
					records[i] = list[i];
				}
			}
		}
		catch (...) {
			// Fall back to uncompressed storage on any error
			compression_type = COMPRESSION_NONE;
			compressed_size = record_count * sizeof(RecordNumber);
			posting_data = (UCHAR*)FB_NEW_POOL(getDefaultMemoryPool()) UCHAR[compressed_size];
			
			RecordNumber* records = reinterpret_cast<RecordNumber*>(posting_data);
			for (FB_SIZE_T i = 0; i < record_count; i++) {
				records[i] = list[i];
			}
		}
		
		// Clean up temporary buffer
		delete[] temp_buffer;
	}
}

PostingListEntry::~PostingListEntry()
{
	if (posting_data) {
		delete[] posting_data;
		posting_data = nullptr;
	}
}

PostingList PostingListEntry::decompress() const
{
	PostingList result;
	
	if (record_count > 0 && posting_data) {
		if (compression_type == COMPRESSION_NONE) {
			// Handle uncompressed data
			result.grow(record_count);
			RecordNumber* records = reinterpret_cast<RecordNumber*>(posting_data);
			for (ULONG i = 0; i < record_count; i++) {
				result.add(records[i]);
			}
		} else {
			// Handle compressed data
			try {
				CompressionType comp_type = static_cast<CompressionType>(compression_type);
				CompressionStats stats;
				
				result = GinCompression::decompress(posting_data, compressed_size, comp_type, &stats);
				
				// Verify decompressed record count matches expected
				if (result.getCount() != record_count) {
					// Decompression error - return empty list
					result.clear();
				}
			}
			catch (...) {
				// Decompression failed - return empty list
				result.clear();
			}
		}
	}
	
	return result;
}

//----------------------------
// GinTokenizer Implementation
//----------------------------

GinTokenizer::GinTokenizer(TokenizerType type)
	: m_type(type),
	  m_min_token_length(GIN_DEFAULT_MIN_TOKEN_LENGTH),
	  m_max_token_length(GIN_DEFAULT_MAX_TOKEN_LENGTH),
	  m_stop_words_enabled(false),
	  m_stemming_enabled(false)
{
}

GinTokenizer::~GinTokenizer()
{
}

TokenList GinTokenizer::tokenize(const UCHAR* text, USHORT length)
{
	fb_assert(text != nullptr);
	
	if (length == 0) {
		return TokenList();
	}
	
	switch (m_type) {
		case SIMPLE_TOKENIZER:
			return simpleTokenize(text, length);
		case STANDARD_TOKENIZER:
			return standardTokenize(text, length);
		case LANGUAGE_TOKENIZER:
			// For now, fallback to standard tokenizer
			return standardTokenize(text, length);
		default:
			return simpleTokenize(text, length);
	}
}

TokenList GinTokenizer::tokenize(const dsc* value)
{
	fb_assert(value != nullptr);
	
	if (value->dsc_address == nullptr || value->dsc_length == 0) {
		return TokenList();
	}
	
	// Handle different data types
	switch (value->dsc_dtype) {
		case dtype_text:
		case dtype_varying:
		{
			USHORT text_length = value->dsc_length;
			const UCHAR* text_data = value->dsc_address;
			
			// For varying strings, skip length prefix
			if (value->dsc_dtype == dtype_varying) {
				text_length = *reinterpret_cast<const USHORT*>(text_data);
				text_data += sizeof(USHORT);
			}
			
			return tokenize(text_data, text_length);
		}
		case dtype_blob:
			// TODO: Handle BLOB data in future enhancement
			return TokenList();
		default:
			// Convert other types to string representation
			// For now, return empty list - this should be enhanced later
			return TokenList();
	}
}

TokenList GinTokenizer::tokenize(const char* text)
{
	fb_assert(text != nullptr);
	
	USHORT length = strlen(text);
	return tokenize(reinterpret_cast<const UCHAR*>(text), length);
}

void GinTokenizer::setMinTokenLength(USHORT min_length)
{
	m_min_token_length = (min_length >= GIN_MIN_TOKEN_LENGTH) ? 
		min_length : GIN_MIN_TOKEN_LENGTH;
}

void GinTokenizer::setMaxTokenLength(USHORT max_length)
{
	m_max_token_length = (max_length <= GIN_MAX_TOKEN_LENGTH) ? 
		max_length : GIN_MAX_TOKEN_LENGTH;
}

void GinTokenizer::enableStopWords(bool enable)
{
	m_stop_words_enabled = enable;
}

void GinTokenizer::enableStemming(bool enable)
{
	m_stemming_enabled = enable;
}

TokenList GinTokenizer::simpleTokenize(const UCHAR* text, USHORT length)
{
	TokenList tokens;
	
	USHORT token_start = 0;
	bool in_token = false;
	
	for (USHORT i = 0; i <= length; i++) {
		bool is_token_char = (i < length) && isTokenChar(text[i]);
		
		if (is_token_char && !in_token) {
			// Start of new token
			token_start = i;
			in_token = true;
		}
		else if (!is_token_char && in_token) {
			// End of current token
			USHORT token_length = i - token_start;
			
			if (token_length >= m_min_token_length && token_length <= m_max_token_length) {
				Token token(&text[token_start], token_length);
				
				if (isValidToken(token)) {
					// Convert to lowercase for case-insensitive matching
					for (USHORT j = 0; j < token.length; j++) {
						if (token.data[j] >= 'A' && token.data[j] <= 'Z') {
							token.data[j] += 32; // Convert to lowercase
						}
					}
					
					tokens.add(token);
					
					// Prevent excessive token generation
					if (tokens.getCount() >= GIN_MAX_TOKENS_PER_DOCUMENT) {
						break;
					}
				}
			}
			
			in_token = false;
		}
	}
	
	// Apply post-processing filters
	if (m_stop_words_enabled) {
		applyStopWordFilter(tokens);
	}
	
	if (m_stemming_enabled) {
		applyStemmer(tokens);
	}
	
	return tokens;
}

TokenList GinTokenizer::standardTokenize(const UCHAR* text, USHORT length)
{
	// For now, standard tokenizer is the same as simple tokenizer
	// TODO: Implement Unicode-aware tokenization in Task 18
	return simpleTokenize(text, length);
}

void GinTokenizer::applyStopWordFilter(TokenList& tokens)
{
	// Remove stop words from token list
	for (FB_SIZE_T i = tokens.getCount(); i > 0; i--) {
		if (isStopWord(tokens[i - 1])) {
			tokens.remove(i - 1);
		}
	}
}

void GinTokenizer::applyStemmer(TokenList& tokens)
{
	// Apply stemming to each token
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		tokens[i] = stemToken(tokens[i]);
	}
}

bool GinTokenizer::isStopWord(const Token& token)
{
	// Basic English stop words
	static const char* stop_words[] = {
		"a", "an", "and", "are", "as", "at", "be", "by", "for", "from",
		"has", "he", "in", "is", "it", "its", "of", "on", "that", "the",
		"to", "was", "were", "will", "with", "the", nullptr
	};
	
	const char* token_str = token.c_str();
	
	for (int i = 0; stop_words[i] != nullptr; i++) {
		if (strcmp(token_str, stop_words[i]) == 0) {
			return true;
		}
	}
	
	return false;
}

Token GinTokenizer::stemToken(const Token& token)
{
	// Basic stemming - remove common English suffixes
	// This is a very simple implementation for demonstration
	// TODO: Implement proper Porter stemmer or similar in future enhancement
	
	Token stemmed = token;
	
	if (stemmed.length > 4) {
		// Remove "ing" suffix
		if (stemmed.length >= 3 && 
			stemmed.data[stemmed.length - 3] == 'i' &&
			stemmed.data[stemmed.length - 2] == 'n' &&
			stemmed.data[stemmed.length - 1] == 'g') {
			stemmed.length -= 3;
			stemmed.data[stemmed.length] = 0;
		}
		// Remove "ed" suffix
		else if (stemmed.length >= 2 &&
				 stemmed.data[stemmed.length - 2] == 'e' &&
				 stemmed.data[stemmed.length - 1] == 'd') {
			stemmed.length -= 2;
			stemmed.data[stemmed.length] = 0;
		}
		// Remove "s" suffix
		else if (stemmed.data[stemmed.length - 1] == 's') {
			stemmed.length -= 1;
			stemmed.data[stemmed.length] = 0;
		}
	}
	
	return stemmed;
}

bool GinTokenizer::isValidToken(const Token& token)
{
	// Check if token meets basic validity requirements
	return token.length >= m_min_token_length && 
		   token.length <= m_max_token_length && 
		   !token.isEmpty();
}

bool GinTokenizer::isTokenChar(UCHAR ch)
{
	// Define token characters (alphanumeric)
	return (ch >= 'a' && ch <= 'z') ||
		   (ch >= 'A' && ch <= 'Z') ||
		   (ch >= '0' && ch <= '9');
}

bool GinTokenizer::isWhitespace(UCHAR ch)
{
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

bool GinTokenizer::isPunctuation(UCHAR ch)
{
	return !isTokenChar(ch) && !isWhitespace(ch);
}

//----------------------------
// GinIndex Implementation
//----------------------------

GinIndex::GinIndex(thread_db* tdbb, Database* database, jrd_rel* relation, const index_desc* desc)
	: m_database(database),
	  m_relation(relation),
	  m_index_desc(desc),
	  m_index_id(desc->idx_id),
	  m_relation_id(relation->rel_id),
	  m_root_page(desc->idx_root),
	  m_tokenizer(nullptr),
	  m_page_manager(nullptr),
	  m_query_processor(nullptr),
	  m_stats_inserts(0),
	  m_stats_lookups(0),
	  m_stats_removes(0),
	  m_stats_tokens_processed(0),
	  m_total_tokens(0),
	  m_unique_tokens(0)
{
	fb_assert(tdbb != nullptr);
	fb_assert(database != nullptr);
	fb_assert(relation != nullptr);
	fb_assert(desc != nullptr);
	fb_assert(desc->idx_type == IDX_TYPE_GIN);
	
	// Initialize tokenizer with default settings
	m_tokenizer = FB_NEW_POOL(getDefaultMemoryPool()) GinTokenizer(GinTokenizer::STANDARD_TOKENIZER);
	
	// Initialize page manager
	m_page_manager = FB_NEW_POOL(getDefaultMemoryPool()) GinPageManager(tdbb, database, desc);
	
	// Initialize query processor
	m_query_processor = FB_NEW_POOL(getDefaultMemoryPool()) GinQueryProcessor(tdbb, this);
}

GinIndex::~GinIndex()
{
	// Clean up cache
	cleanupCache();
	
	// Delete tokenizer
	if (m_tokenizer) {
		delete m_tokenizer;
		m_tokenizer = nullptr;
	}
	
	// Delete page manager
	if (m_page_manager) {
		delete m_page_manager;
		m_page_manager = nullptr;
	}
	
	// Delete query processor
	if (m_query_processor) {
		delete m_query_processor;
		m_query_processor = nullptr;
	}
}

index_error_t GinIndex::insert(thread_db* tdbb, const dsc* key, RecordNumber record, jrd_tra* transaction)
{
	fb_assert(tdbb != nullptr);
	fb_assert(key != nullptr);
	
	try {
		m_stats_inserts++;
		
		// Extract tokens from the value
		TokenList tokens = extractTokens(key);
		
		if (tokens.isEmpty()) {
			return idx_e_ok; // Empty content, nothing to index
		}
		
		m_stats_tokens_processed += tokens.getCount();
		
		// Insert each token
		for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
			if (!insertToken(tdbb, tokens[i], record, transaction)) {
				return idx_e_duplicate; // Or another appropriate error
			}
		}
		
		updateStatistics();
		return idx_e_ok;
	}
	catch (...) {
		return idx_e_conversion; // Generic error for now
	}
}

bool GinIndex::lookup(thread_db* tdbb, const dsc* key, IndexRetrieval* retrieval)
{
	fb_assert(tdbb != nullptr);
	fb_assert(key != nullptr);
	fb_assert(retrieval != nullptr);
	
	try {
		m_stats_lookups++;
		
		// Extract tokens from search value
		TokenList search_tokens = extractTokens(key);
		
		if (search_tokens.isEmpty()) {
			return false; // No tokens to search for
		}
		
		// For lookup operation, we need to find records containing ALL tokens (AND logic)
		// This is simplified for now - full implementation will be in query processor
		
		// Find first token's posting list
		PostingListEntry* first_posting = findPostingList(tdbb, search_tokens[0]);
		if (!first_posting) {
			return false; // First token not found
		}
		
		// For single token search, return true if found
		if (search_tokens.getCount() == 1) {
			return true;
		}
		
		// For multiple tokens, we'd need to intersect posting lists
		// This is a simplified implementation - full version in Task 21
		return true;
	}
	catch (...) {
		return false;
	}
}

index_error_t GinIndex::remove(thread_db* tdbb, const dsc* key, RecordNumber record, jrd_tra* transaction)
{
	fb_assert(tdbb != nullptr);
	fb_assert(key != nullptr);
	
	try {
		m_stats_removes++;
		
		// Extract tokens from the value
		TokenList tokens = extractTokens(key);
		
		if (tokens.isEmpty()) {
			return idx_e_ok; // Nothing to remove
		}
		
		// Remove record from each token's posting list
		for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
			removeToken(tdbb, tokens[i], record, transaction);
		}
		
		updateStatistics();
		return idx_e_ok;
	}
	catch (...) {
		return idx_e_conversion; // Generic error for now
	}
}

bool GinIndex::searchTokens(thread_db* tdbb, const TokenList& search_tokens, RecordBitmap* result_bitmap)
{
	fb_assert(tdbb != nullptr);
	fb_assert(result_bitmap != nullptr);
	
	if (search_tokens.isEmpty()) {
		return false;
	}
	
	// This is a simplified implementation
	// Full implementation will be in GinQueryProcessor (Task 21)
	
	for (FB_SIZE_T i = 0; i < search_tokens.getCount(); i++) {
		PostingListEntry* posting = findPostingList(tdbb, search_tokens[i]);
		if (posting) {
			// TODO: Convert posting list to bitmap and intersect/union with result_bitmap
			// For now, just return true if any token is found
			return true;
		}
	}
	
	return false;
}

bool GinIndex::searchToken(thread_db* tdbb, const Token& token, RecordBitmap* result_bitmap)
{
	fb_assert(tdbb != nullptr);
	fb_assert(result_bitmap != nullptr);
	
	PostingListEntry* posting = findPostingList(tdbb, token);
	if (posting) {
		// TODO: Convert posting list to bitmap
		// For now, just return true if token is found
		return true;
	}
	
	return false;
}

bool GinIndex::containsQuery(thread_db* tdbb, const dsc* search_value, RecordBitmap* result_bitmap)
{
	fb_assert(tdbb != nullptr);
	fb_assert(search_value != nullptr);
	fb_assert(result_bitmap != nullptr);
	
	if (m_query_processor) {
		return m_query_processor->executeContainsQuery(search_value, result_bitmap);
	}
	
	// Fallback to simple implementation
	TokenList search_tokens = extractTokens(search_value);
	return searchTokens(tdbb, search_tokens, result_bitmap);
}

ULONG GinIndex::getTotalTokens() const
{
	return m_total_tokens;
}

ULONG GinIndex::getTotalPostings() const
{
	// Count total posting entries across all cached tokens
	ULONG total = 0;
	
	for (TokenPostingMap::const_iterator it = m_token_cache.begin();
		 it != m_token_cache.end(); ++it) {
		if (it->second) {
			total += it->second->getRecordCount();
		}
	}
	
	return total;
}

ULONG GinIndex::getUniqueTokens() const
{
	return m_unique_tokens;
}

double GinIndex::getAverageTokensPerRecord() const
{
	if (m_stats_inserts == 0) {
		return 0.0;
	}
	
	return static_cast<double>(m_stats_tokens_processed) / m_stats_inserts;
}

void GinIndex::setTokenizerOptions(USHORT min_length, USHORT max_length, bool stop_words, bool stemming)
{
	if (m_tokenizer) {
		m_tokenizer->setMinTokenLength(min_length);
		m_tokenizer->setMaxTokenLength(max_length);
		m_tokenizer->enableStopWords(stop_words);
		m_tokenizer->enableStemming(stemming);
	}
}

//----------------------------
// Private Implementation Methods
//----------------------------

TokenList GinIndex::extractTokens(const dsc* value)
{
	if (!m_tokenizer || !value) {
		return TokenList();
	}
	
	return m_tokenizer->tokenize(value);
}

bool GinIndex::insertToken(thread_db* tdbb, const Token& token, RecordNumber record, jrd_tra* transaction)
{
	fb_assert(tdbb != nullptr);
	
	if (!validateToken(token)) {
		return false;
	}
	
	// For now, this is a simplified implementation
	// Full implementation will involve page management in Task 20
	
	return addToPostingList(tdbb, token, record, transaction);
}

bool GinIndex::removeToken(thread_db* tdbb, const Token& token, RecordNumber record, jrd_tra* transaction)
{
	fb_assert(tdbb != nullptr);
	
	if (!validateToken(token)) {
		return false;
	}
	
	return removeFromPostingList(tdbb, token, record, transaction);
}

PostingListEntry* GinIndex::findPostingList(thread_db* tdbb, const Token& token)
{
	fb_assert(tdbb != nullptr);
	
	// Check cache first
	PostingListEntry* cached = getCachedPostingList(token);
	if (cached) {
		return cached;
	}
	
	// Load from disk storage using page manager
	if (m_page_manager) {
		ULONG page_number;
		USHORT offset;
		gin_token_entry* token_entry = m_page_manager->findTokenEntry(tdbb, token, page_number, offset);
		
		if (token_entry) {
			// Load compressed posting list data
			UCHAR* buffer = (UCHAR*)FB_NEW_POOL(getDefaultMemoryPool()) UCHAR[GinCompression::COMPRESSION_BUFFER_SIZE];
			ULONG actual_size;
			CompressionType compression;
			
			bool loaded = m_page_manager->loadPostingList(tdbb, token_entry->posting_page,
															  token_entry->posting_offset, buffer,
															  GinCompression::COMPRESSION_BUFFER_SIZE,
															  actual_size, compression);
			
			if (loaded) {
				// Create PostingListEntry from loaded data
				PostingListEntry* entry = FB_NEW_POOL(getDefaultMemoryPool()) PostingListEntry();
				entry->record_count = token_entry->record_count;
				entry->compression_type = compression;
				entry->compressed_size = actual_size;
				entry->posting_data = buffer;
				
				// Cache the loaded entry
				cachePostingList(token, entry);
				
				return entry;
			} else {
				delete[] buffer;
			}
		}
	}
	
	return nullptr;
}

bool GinIndex::addToPostingList(thread_db* tdbb, const Token& token, RecordNumber record, jrd_tra* transaction)
{
	fb_assert(tdbb != nullptr);
	
	// Check memory cache first
	PostingListEntry* cached = getCachedPostingList(token);
	PostingList posting_list;
	
	if (!cached) {
		// Try to load from disk first
		cached = findPostingList(tdbb, token);
	}
	
	if (!cached) {
		// Create new posting list
		posting_list.add(record);
		m_unique_tokens++;
	}
	else {
		// Add to existing posting list
		posting_list = cached->decompress();
		
		// Check for duplicates
		for (FB_SIZE_T i = 0; i < posting_list.getCount(); i++) {
			if (posting_list[i] == record) {
				return true; // Already exists, no need to add again
			}
		}
		
		posting_list.add(record);
	}
	
	// Choose appropriate compression algorithm
	CompressionType comp_type = GinCompression::chooseBestAlgorithm(posting_list);
	
	// Compress the posting list
	UCHAR* compressed_buffer = (UCHAR*)FB_NEW_POOL(getDefaultMemoryPool()) UCHAR[GinCompression::COMPRESSION_BUFFER_SIZE];
	CompressionStats stats;
	ULONG compressed_size = GinCompression::compress(posting_list, comp_type, compressed_buffer, 
													 GinCompression::COMPRESSION_BUFFER_SIZE, &stats);
	
	if (compressed_size == 0) {
		delete[] compressed_buffer;
		return false; // Compression failed
	}
	
	try {
		if (m_page_manager) {
			ULONG posting_page;
			USHORT posting_offset;
			
			// Allocate space for posting list
			bool space_allocated = m_page_manager->allocatePostingSpace(tdbb, compressed_size, 
																	 posting_page, posting_offset, transaction);
			
			if (space_allocated) {
				// Store compressed posting list
				bool stored = m_page_manager->storePostingList(tdbb, posting_page, posting_offset,
															   compressed_buffer, compressed_size,
															   token.hash(), posting_list.getCount(),
															   comp_type, transaction);
				
				if (stored) {
					// Insert or update token entry
					bool token_updated;
					if (cached) {
						token_updated = m_page_manager->updateTokenEntry(tdbb, token, posting_page, 
																 posting_offset, compressed_size,
																 posting_list.getCount(), comp_type, transaction);
					} else {
						token_updated = m_page_manager->insertTokenEntry(tdbb, token, posting_page,
																 posting_offset, compressed_size,
																 posting_list.getCount(), comp_type, transaction);
					}
					
					if (token_updated) {
						// Update memory cache
						if (cached) {
							delete cached;
						}
						
						PostingListEntry* new_entry = FB_NEW_POOL(getDefaultMemoryPool()) PostingListEntry();
						new_entry->record_count = posting_list.getCount();
						new_entry->compression_type = comp_type;
						new_entry->compressed_size = compressed_size;
						new_entry->posting_data = compressed_buffer;
						
						cachePostingList(token, new_entry);
						
						return true;
					}
				}
			}
		}
		
		// Storage failed, fall back to memory-only cache
		PostingListEntry* new_entry = FB_NEW_POOL(getDefaultMemoryPool()) PostingListEntry(posting_list, comp_type);
		if (cached) {
			delete cached;
		}
		cachePostingList(token, new_entry);
		
		delete[] compressed_buffer;
		return true;
	}
	catch (...) {
		delete[] compressed_buffer;
		return false;
	}
}

bool GinIndex::removeFromPostingList(thread_db* tdbb, const Token& token, RecordNumber record, jrd_tra* transaction)
{
	fb_assert(tdbb != nullptr);
	
	PostingListEntry* cached = getCachedPostingList(token);
	if (!cached) {
		return false; // Token not found
	}
	
	PostingList existing = cached->decompress();
	
	// Find and remove record
	for (FB_SIZE_T i = 0; i < existing.getCount(); i++) {
		if (existing[i] == record) {
			existing.remove(i);
			
			if (existing.isEmpty()) {
				// Remove token entirely if no more records
				// TODO: Implement proper token removal from cache
				delete cached;
				m_unique_tokens--;
			}
			else {
				// Choose appropriate compression algorithm for updated list
				CompressionType comp_type = GinCompression::chooseBestAlgorithm(existing);
				
				// Update cached entry
				delete cached;
				PostingListEntry* updated_entry = FB_NEW_POOL(getDefaultMemoryPool()) PostingListEntry(existing, comp_type);
				cachePostingList(token, updated_entry);
			}
			
			return true;
		}
	}
	
	return false; // Record not found in posting list
}

PostingList GinIndex::intersectPostingLists(const PostingList& list1, const PostingList& list2)
{
	PostingList result;
	
	FB_SIZE_T i = 0, j = 0;
	
	while (i < list1.getCount() && j < list2.getCount()) {
		if (list1[i] == list2[j]) {
			result.add(list1[i]);
			i++;
			j++;
		}
		else if (list1[i] < list2[j]) {
			i++;
		}
		else {
			j++;
		}
	}
	
	return result;
}

PostingList GinIndex::unionPostingLists(const PostingList& list1, const PostingList& list2)
{
	PostingList result;
	
	FB_SIZE_T i = 0, j = 0;
	
	while (i < list1.getCount() && j < list2.getCount()) {
		if (list1[i] == list2[j]) {
			result.add(list1[i]);
			i++;
			j++;
		}
		else if (list1[i] < list2[j]) {
			result.add(list1[i]);
			i++;
		}
		else {
			result.add(list2[j]);
			j++;
		}
	}
	
	// Add remaining elements
	while (i < list1.getCount()) {
		result.add(list1[i]);
		i++;
	}
	
	while (j < list2.getCount()) {
		result.add(list2[j]);
		j++;
	}
	
	return result;
}

PostingListEntry* GinIndex::getCachedPostingList(const Token& token)
{
	TokenPostingMap::iterator it = m_token_cache.locate(Pair<Token, PostingListEntry*>(token, nullptr));
	return (it != m_token_cache.end()) ? it->second : nullptr;
}

void GinIndex::cachePostingList(const Token& token, PostingListEntry* posting_list)
{
	fb_assert(posting_list != nullptr);
	
	// Remove existing entry if present
	TokenPostingMap::iterator it = m_token_cache.locate(Pair<Token, PostingListEntry*>(token, nullptr));
	if (it != m_token_cache.end()) {
		delete it->second;
		m_token_cache.remove(it);
	}
	
	// Add new entry
	m_token_cache.put(Pair<Token, PostingListEntry*>(token, posting_list));
}

void GinIndex::invalidateCache()
{
	cleanupCache();
}

void GinIndex::cleanupCache()
{
	// Delete all cached posting list entries
	for (TokenPostingMap::iterator it = m_token_cache.begin();
		 it != m_token_cache.end(); ++it) {
		delete it->second;
	}
	
	m_token_cache.clear();
}

ULONG GinIndex::findTokenPage(thread_db* tdbb, const Token& token)
{
	if (m_page_manager) {
		return m_page_manager->findTokenPage(tdbb, token);
	}
	return 0;
}

ULONG GinIndex::allocateTokenPage(thread_db* tdbb, jrd_tra* transaction)
{
	if (m_page_manager) {
		return m_page_manager->allocatePage(tdbb, GIN_PAGE_TOKEN, transaction);
	}
	return 0;
}

ULONG GinIndex::allocatePostingPage(thread_db* tdbb, jrd_tra* transaction)
{
	if (m_page_manager) {
		return m_page_manager->allocatePage(tdbb, GIN_PAGE_POSTING, transaction);
	}
	return 0;
}

void GinIndex::updateStatistics()
{
	m_total_tokens = m_stats_tokens_processed;
	m_unique_tokens = m_token_cache.count();
}

bool GinIndex::validateToken(const Token& token)
{
	return !token.isEmpty() && 
		   token.length >= GIN_MIN_TOKEN_LENGTH &&
		   token.length <= GIN_MAX_TOKEN_LENGTH;
}

ULONG GinIndex::calculatePostingListSize(const PostingList& list)
{
	// Use compression algorithm to estimate size
	CompressionType best_algorithm = GinCompression::chooseBestAlgorithm(list);
	return GinCompression::estimateCompressedSize(list, best_algorithm);
}

void GinIndex::deallocatePostingListEntry(PostingListEntry* entry)
{
	delete entry;
}

} // namespace Jrd