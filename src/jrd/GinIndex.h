/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		GinIndex.h  
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

#ifndef JRD_GIN_INDEX_H
#define JRD_GIN_INDEX_H

#include "../jrd/IndexType.h"
#include "../jrd/Database.h"
#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../jrd/constants.h" 
#include "../jrd/ods.h"
#include "../common/classes/array.h"
#include "../common/classes/GenericMap.h"
#include "../include/firebird/impl/dsc_pub.h"

// Forward declare advanced tokenizer classes
namespace Jrd {
	class AdvancedGinTokenizer;
	class UnicodeHelper;
	class LanguageProcessor;
	class NGramGenerator;
	class PhoneticProcessor;
}

namespace Jrd {

// Forward declarations
class Database;
class jrd_rel;
class GinPageManager;
class GinQueryProcessor;
class index_desc;
class thread_db;
class jrd_tra;
class IndexRetrieval;
class RecordBitmap;
struct dsc;

//----------------------------
// GIN Index Constants
//----------------------------
const USHORT GIN_MIN_TOKEN_LENGTH = 2;			// Minimum token length
const USHORT GIN_MAX_TOKEN_LENGTH = 64;		// Maximum token length  
const USHORT GIN_DEFAULT_MIN_TOKEN_LENGTH = 3;	// Default minimum token length
const USHORT GIN_DEFAULT_MAX_TOKEN_LENGTH = 32;	// Default maximum token length
const ULONG GIN_MAX_TOKENS_PER_DOCUMENT = 10000;	// Maximum tokens per indexed value
const USHORT GIN_TOKEN_PAGE_SIZE = 4096;		// Token page size
const USHORT GIN_POSTING_PAGE_SIZE = 4096;		// Posting page size

//----------------------------
// Token - Represents a single token extracted from text
//----------------------------
struct Token
{
	USHORT length;						// Token length in bytes
	UCHAR data[GIN_MAX_TOKEN_LENGTH];	// Token data
	
	Token();
	Token(const UCHAR* text, USHORT len);
	Token(const char* text);
	Token(const Token& other);
	
	Token& operator=(const Token& other);
	bool operator==(const Token& other) const;
	bool operator!=(const Token& other) const;
	bool operator<(const Token& other) const;
	
	ULONG hash() const;
	const char* c_str() const;
	bool isEmpty() const;
	void clear();
};

//----------------------------
// TokenList - Dynamic array of tokens
//----------------------------
typedef ScratchBird::HalfStaticArray<Token, 16> TokenList;

//----------------------------
// PostingList - List of record numbers containing a token
//----------------------------
typedef ScratchBird::HalfStaticArray<RecordNumber, 32> PostingList;

//----------------------------
// PostingListEntry - Entry in the posting list with compression info
//----------------------------
struct PostingListEntry
{
	ULONG record_count;					// Number of records in this entry
	UCHAR compression_type;				// Compression method used
	USHORT compressed_size;				// Size of compressed data
	UCHAR* posting_data;				// Compressed posting list data
	
	PostingListEntry();
	PostingListEntry(const PostingList& list, UCHAR compression = 0);
	~PostingListEntry();
	
	PostingListEntry(const PostingListEntry& other) = delete;
	PostingListEntry& operator=(const PostingListEntry& other) = delete;
	
	PostingList decompress() const;
	ULONG getCompressedSize() const { return compressed_size; }
	ULONG getRecordCount() const { return record_count; }
};

//----------------------------
// GinTokenizer - Text tokenization engine
//----------------------------
class GinTokenizer
{
public:
	enum TokenizerType {
		SIMPLE_TOKENIZER = 0,			// Basic whitespace/punctuation splitting
		STANDARD_TOKENIZER = 1,			// Unicode-aware with configurable options
		LANGUAGE_TOKENIZER = 2			// Language-specific processing (future)
	};
	
	GinTokenizer(TokenizerType type = STANDARD_TOKENIZER);
	~GinTokenizer();
	
	// Main tokenization interface
	TokenList tokenize(const UCHAR* text, USHORT length);
	TokenList tokenize(const dsc* value);
	TokenList tokenize(const char* text);
	
	// Configuration
	void setMinTokenLength(USHORT min_length);
	void setMaxTokenLength(USHORT max_length);
	void enableStopWords(bool enable);
	void enableStemming(bool enable);
	
	// Getters
	USHORT getMinTokenLength() const { return m_min_token_length; }
	USHORT getMaxTokenLength() const { return m_max_token_length; }
	bool isStopWordsEnabled() const { return m_stop_words_enabled; }
	bool isStemmingEnabled() const { return m_stemming_enabled; }
	
private:
	TokenizerType m_type;
	USHORT m_min_token_length;
	USHORT m_max_token_length;
	bool m_stop_words_enabled;
	bool m_stemming_enabled;
	
	// Tokenization methods
	TokenList simpleTokenize(const UCHAR* text, USHORT length);
	TokenList standardTokenize(const UCHAR* text, USHORT length);
	
	// Token processing
	void applyStopWordFilter(TokenList& tokens);
	void applyStemmer(TokenList& tokens);
	bool isStopWord(const Token& token);
	Token stemToken(const Token& token);
	bool isValidToken(const Token& token);
	
	// Character classification
	bool isTokenChar(UCHAR ch);
	bool isWhitespace(UCHAR ch);
	bool isPunctuation(UCHAR ch);
};

//----------------------------
// GinIndex - Main GIN index implementation class
//----------------------------
class GinIndex : public IndexType
{
public:
	GinIndex(thread_db* tdbb, Database* database, jrd_rel* relation, const index_desc* desc);
	virtual ~GinIndex();
	
	// Core IndexType interface
	virtual index_error_t insert(thread_db* tdbb, const dsc* key, 
								 RecordNumber record, jrd_tra* transaction) override;
	virtual bool lookup(thread_db* tdbb, const dsc* key, IndexRetrieval* retrieval) override;
	virtual index_error_t remove(thread_db* tdbb, const dsc* key, 
								 RecordNumber record, jrd_tra* transaction) override;
	virtual const char* getTypeName() const override { return IDX_TYPE_NAME_GIN; }
	
	// GIN-specific public interface
	bool searchTokens(thread_db* tdbb, const TokenList& search_tokens, RecordBitmap* result_bitmap);
	bool searchToken(thread_db* tdbb, const Token& token, RecordBitmap* result_bitmap);
	bool containsQuery(thread_db* tdbb, const dsc* search_value, RecordBitmap* result_bitmap);
	
	// Statistics and introspection
	ULONG getTotalTokens() const;
	ULONG getTotalPostings() const;
	ULONG getUniqueTokens() const;
	double getAverageTokensPerRecord() const;
	
	// Configuration
	void setTokenizerOptions(USHORT min_length, USHORT max_length, bool stop_words, bool stemming);
	GinTokenizer* getTokenizer() { return m_tokenizer; }
	GinPageManager* getPageManager() { return m_page_manager; }
	
	// Statistics and information
	ULONG getTotalDocuments() const { return m_stats_inserts; } // Simplified estimate
	
private:
	// Member variables
	Database* m_database;				// Database this index belongs to
	jrd_rel* m_relation;				// Relation this index is on
	const index_desc* m_index_desc;		// Index descriptor
	UCHAR m_index_id;					// Index ID
	USHORT m_relation_id;				// Relation ID
	ULONG m_root_page;					// Root page number for token tree
	
	// GIN-specific components
	GinTokenizer* m_tokenizer;			// Text tokenization engine
	GinPageManager* m_page_manager;		// Page management and storage system
	GinQueryProcessor* m_query_processor;// Query processing and execution engine
	
	// Token-to-posting mapping (in-memory cache)
	typedef GenericMap<Pair<Token, PostingListEntry*>> TokenPostingMap;
	TokenPostingMap m_token_cache;
	
	// Statistics (mutable for const methods)
	mutable ULONG m_stats_inserts;		// Number of insert operations
	mutable ULONG m_stats_lookups;		// Number of lookup operations  
	mutable ULONG m_stats_removes;		// Number of remove operations
	mutable ULONG m_stats_tokens_processed; // Total tokens processed
	mutable ULONG m_total_tokens;		// Total tokens in index
	mutable ULONG m_unique_tokens;		// Unique tokens in index
	
	// Private implementation methods
	
	// Token management
	TokenList extractTokens(const dsc* value);
	bool insertToken(thread_db* tdbb, const Token& token, RecordNumber record, jrd_tra* transaction);
	bool removeToken(thread_db* tdbb, const Token& token, RecordNumber record, jrd_tra* transaction);
	PostingListEntry* findPostingList(thread_db* tdbb, const Token& token);
	
	// Posting list operations
	bool addToPostingList(thread_db* tdbb, const Token& token, RecordNumber record, jrd_tra* transaction);
	bool removeFromPostingList(thread_db* tdbb, const Token& token, RecordNumber record, jrd_tra* transaction);
	PostingList intersectPostingLists(const PostingList& list1, const PostingList& list2);
	PostingList unionPostingLists(const PostingList& list1, const PostingList& list2);
	
	// Cache management
	PostingListEntry* getCachedPostingList(const Token& token);
	void cachePostingList(const Token& token, PostingListEntry* posting_list);
	void invalidateCache();
	void cleanupCache();
	
	// Page management (implemented with GinPageManager)
	ULONG findTokenPage(thread_db* tdbb, const Token& token);
	ULONG allocateTokenPage(thread_db* tdbb, jrd_tra* transaction);
	ULONG allocatePostingPage(thread_db* tdbb, jrd_tra* transaction);
	
	// Utility methods
	void updateStatistics();
	bool validateToken(const Token& token);
	ULONG calculatePostingListSize(const PostingList& list);
	
	// Memory management helpers
	void deallocatePostingListEntry(PostingListEntry* entry);
};

//----------------------------
// GinIndexFactory - Factory for creating GinIndex instances
//----------------------------
class GinIndexFactory : public IndexTypeFactory
{
public:
	virtual IndexType* createIndex(thread_db* tdbb, Database* database,
								   jrd_rel* relation, const index_desc* desc) override
	{
		return new GinIndex(tdbb, database, relation, desc);
	}
	
	virtual const char* getTypeName() const override 
	{
		return IDX_TYPE_NAME_GIN;
	}
	
	virtual int getTypeId() const override 
	{
		return IDX_TYPE_GIN;
	}
	
	virtual bool supportsOrdering() const override 
	{
		return false; // GIN indexes don't support ordering
	}
	
	virtual bool supportsUniqueness() const override 
	{
		return false; // GIN indexes don't enforce uniqueness
	}
	
	virtual bool supportsPartialKey() const override 
	{
		return true; // GIN indexes support token-based partial matching
	}
};

} // namespace Jrd

#endif // JRD_GIN_INDEX_H