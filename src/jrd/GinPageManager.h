/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		GinPageManager.h  
 *	DESCRIPTION:	GIN page management and storage system
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
 * 2025.07.22 - ScratchBird GIN Page Management Implementation
 */

#ifndef JRD_GIN_PAGE_MANAGER_H
#define JRD_GIN_PAGE_MANAGER_H

#include "../jrd/jrd.h"
#include "../jrd/pag.h"
#include "../jrd/btr.h"
#include "../jrd/ods.h"
#include "../jrd/constants.h"
#include "../jrd/GinIndex.h"
#include "../jrd/GinCompression.h"
#include "../common/classes/array.h"

namespace Jrd {

// Forward declarations
class GinIndex;
class Database;
struct index_desc;

//----------------------------
// GIN Page Type Constants
//----------------------------
enum GinPageType {
	GIN_PAGE_ROOT = 0,			// Root page containing metadata and tree structure
	GIN_PAGE_TOKEN = 1,			// Token directory page (B+ tree node)
	GIN_PAGE_POSTING = 2,		// Posting list data page
	GIN_PAGE_OVERFLOW = 3,		// Overflow page for large posting lists
	GIN_PAGE_LEAF = 4,			// Leaf page containing token entries
	GIN_PAGE_BRANCH = 5			// Branch page for navigation
};

//----------------------------
// GIN Page Header Structure
//----------------------------
struct gin_page_header {
	pag			gin_header;				// Standard page header
	UCHAR		gin_page_type;			// GinPageType value
	UCHAR		gin_flags;				// Page flags
	USHORT		gin_free_space;			// Free space available on page
	USHORT		gin_item_count;			// Number of items on page
	ULONG		gin_level;				// Tree level (0 = leaf, higher = branch)
	ULONG		gin_prev_page;			// Previous page in sequence
	ULONG		gin_next_page;			// Next page in sequence
	ULONG		gin_parent_page;		// Parent page number
	ULONG		gin_reserved[2];		// Reserved for future use
	
	// Get usable space on page
	USHORT getUsableSpace() const {
		return getDefaultPageSize() - sizeof(gin_page_header) - gin_free_space;
	}
	
	// Get remaining space on page
	USHORT getRemainingSpace() const {
		return gin_free_space;
	}
};

//----------------------------
// GIN Root Page Structure
//----------------------------
struct gin_root_page {
	gin_page_header	root_header;		// Standard GIN page header
	ULONG			root_version;		// Index format version
	ULONG			root_flags;			// Index configuration flags
	ULONG			root_token_count;	// Total number of unique tokens
	ULONG			root_posting_count;	// Total number of postings
	ULONG			root_levels;		// Number of tree levels
	ULONG			root_first_token_page;	// First token page
	ULONG			root_first_posting_page;// First posting page
	ULONG			root_last_statistics_page;	// Statistics page
	USHORT			root_min_token_length;	// Minimum token length
	USHORT			root_max_token_length;	// Maximum token length
	CompressionType	root_compression_type;	// Default compression algorithm
	UCHAR			root_tokenizer_type;	// Tokenizer configuration
	UCHAR			root_language_code;		// Language for processing
	UCHAR			root_reserved[18];		// Reserved for alignment and future use
	
	// Index statistics
	struct {
		ULONG		total_inserts;		// Total insert operations
		ULONG		total_lookups;		// Total lookup operations  
		ULONG		total_removes;		// Total remove operations
		ULONG		cache_hits;			// Cache hit count
		ULONG		cache_misses;		// Cache miss count
		double		avg_posting_size;	// Average posting list size
		double		compression_ratio;	// Average compression ratio
		ULONG		last_maintenance;	// Timestamp of last maintenance
	} statistics;
};

//----------------------------
// GIN Token Entry Structure
//----------------------------
struct gin_token_entry {
	USHORT		token_length;			// Length of token data
	USHORT		token_flags;			// Token flags (frequency, etc.)
	ULONG		posting_page;			// Page containing posting list
	USHORT		posting_offset;			// Offset within posting page
	USHORT		posting_size;			// Size of compressed posting data
	ULONG		record_count;			// Number of records in posting list
	CompressionType compression_type;	// Compression algorithm used
	UCHAR		token_data[1];			// Variable-length token data
	
	// Get total entry size including token data
	USHORT getTotalSize() const {
		return sizeof(gin_token_entry) - 1 + token_length;
	}
	
	// Get token as string
	ScratchBird::string getTokenString() const {
		return ScratchBird::string(reinterpret_cast<const char*>(token_data), token_length);
	}
};

//----------------------------
// GIN Posting Page Structure
//----------------------------
struct gin_posting_page {
	gin_page_header	posting_header;		// Standard GIN page header
	ULONG			posting_flags;		// Posting page flags
	USHORT			posting_entry_count;// Number of posting entries
	USHORT			posting_data_size;	// Size of posting data area
	ULONG			posting_overflow;	// Overflow page if needed
	UCHAR			posting_reserved[8];// Reserved space
	
	// Variable-length posting data follows
	UCHAR			posting_data[1];
	
	// Get pointer to posting data area
	UCHAR* getPostingData() {
		return posting_data;
	}
	
	// Get available space for posting data
	USHORT getAvailableSpace() const {
		return getDefaultPageSize() - sizeof(gin_posting_page) + 1 - posting_data_size;
	}
};

//----------------------------
// GIN Posting Entry Structure
//----------------------------
struct gin_posting_entry {
	ULONG			entry_token_hash;	// Hash of token for verification
	USHORT			entry_data_offset;	// Offset to compressed data
	USHORT			entry_data_size;	// Size of compressed data
	ULONG			entry_record_count;	// Number of records
	CompressionType	entry_compression;	// Compression algorithm
	USHORT			entry_flags;		// Entry flags
	USHORT			entry_reserved;		// Reserved for alignment
	
	// Get pointer to compressed data
	UCHAR* getCompressedData(gin_posting_page* page) {
		return page->getPostingData() + entry_data_offset;
	}
};

//----------------------------
// GIN Page Management Class
//----------------------------
class GinPageManager
{
public:
	//----------------------------
	// Constructor and Destructor
	//----------------------------
	GinPageManager(thread_db* tdbb, Database* database, const index_desc* idx_desc);
	~GinPageManager();
	
	//----------------------------
	// Page Allocation and Management
	//----------------------------
	
	// Allocate new page of specified type
	ULONG allocatePage(thread_db* tdbb, GinPageType page_type, jrd_tra* transaction);
	
	// Release page back to free list
	void releasePage(thread_db* tdbb, ULONG page_number, jrd_tra* transaction);
	
	// Get page and ensure it's properly typed
	pag* getPage(thread_db* tdbb, ULONG page_number, GinPageType expected_type);
	
	// Mark page as dirty for write
	void markPageDirty(thread_db* tdbb, ULONG page_number);
	
	// Initialize new page with proper header
	void initializePage(thread_db* tdbb, ULONG page_number, GinPageType page_type);
	
	//----------------------------
	// Root Page Management
	//----------------------------
	
	// Initialize root page for new index
	void initializeRootPage(thread_db* tdbb, ULONG root_page, jrd_tra* transaction);
	
	// Update root page statistics
	void updateRootStatistics(thread_db* tdbb, ULONG root_page, const GinIndex::Statistics& stats);
	
	// Get root page metadata
	gin_root_page* getRootPage(thread_db* tdbb, ULONG root_page);
	
	//----------------------------
	// Token Page Management
	//----------------------------
	
	// Find page containing specific token
	ULONG findTokenPage(thread_db* tdbb, const Token& token);
	
	// Insert token entry on appropriate page
	bool insertTokenEntry(thread_db* tdbb, const Token& token, 
						  ULONG posting_page, USHORT posting_offset, 
						  USHORT posting_size, ULONG record_count,
						  CompressionType compression, jrd_tra* transaction);
	
	// Update existing token entry
	bool updateTokenEntry(thread_db* tdbb, const Token& token,
						  ULONG posting_page, USHORT posting_offset,
						  USHORT posting_size, ULONG record_count,
						  CompressionType compression, jrd_tra* transaction);
	
	// Remove token entry
	bool removeTokenEntry(thread_db* tdbb, const Token& token, jrd_tra* transaction);
	
	// Find token entry and return posting location
	gin_token_entry* findTokenEntry(thread_db* tdbb, const Token& token, 
									ULONG& page_number, USHORT& offset);
	
	//----------------------------
	// Posting Page Management
	//----------------------------
	
	// Allocate space for posting list
	bool allocatePostingSpace(thread_db* tdbb, USHORT data_size, 
							  ULONG& page_number, USHORT& offset, jrd_tra* transaction);
	
	// Store compressed posting list data
	bool storePostingList(thread_db* tdbb, ULONG page_number, USHORT offset,
						  const UCHAR* compressed_data, USHORT data_size,
						  ULONG token_hash, ULONG record_count,
						  CompressionType compression, jrd_tra* transaction);
	
	// Load compressed posting list data
	bool loadPostingList(thread_db* tdbb, ULONG page_number, USHORT offset,
						 UCHAR* buffer, USHORT buffer_size,
						 ULONG& actual_size, CompressionType& compression);
	
	// Update posting list in place
	bool updatePostingList(thread_db* tdbb, ULONG page_number, USHORT offset,
						   const UCHAR* compressed_data, USHORT data_size,
						   ULONG record_count, CompressionType compression, 
						   jrd_tra* transaction);
	
	// Remove posting list data
	bool removePostingList(thread_db* tdbb, ULONG page_number, USHORT offset,
						   jrd_tra* transaction);
	
	//----------------------------
	// Tree Navigation and Maintenance
	//----------------------------
	
	// Find leaf page for token insertion/lookup
	ULONG findLeafPage(thread_db* tdbb, const Token& token);
	
	// Split page when it becomes full
	ULONG splitPage(thread_db* tdbb, ULONG page_number, GinPageType page_type, 
					jrd_tra* transaction);
	
	// Merge pages when they become too empty
	bool mergePages(thread_db* tdbb, ULONG left_page, ULONG right_page,
					jrd_tra* transaction);
	
	// Rebalance tree structure
	void rebalanceTree(thread_db* tdbb, ULONG root_page, jrd_tra* transaction);
	
	//----------------------------
	// Space Management
	//----------------------------
	
	// Get free space on page
	USHORT getFreeSpace(thread_db* tdbb, ULONG page_number);
	
	// Compact page to reclaim fragmented space
	void compactPage(thread_db* tdbb, ULONG page_number);
	
	// Estimate space required for token entry
	USHORT estimateTokenEntrySize(const Token& token, USHORT posting_size);
	
	// Estimate space required for posting entry
	USHORT estimatePostingEntrySize(USHORT data_size);
	
	//----------------------------
	// Statistics and Monitoring
	//----------------------------
	
	// Collect page utilization statistics
	struct PageStatistics {
		ULONG total_pages;
		ULONG token_pages;
		ULONG posting_pages;
		ULONG overflow_pages;
		double avg_utilization;
		ULONG fragmented_pages;
		ULONG total_free_space;
		ULONG largest_free_block;
	};
	
	PageStatistics collectPageStatistics(thread_db* tdbb, ULONG root_page);
	
	// Analyze index structure health
	struct IndexHealthReport {
		ULONG tree_depth;
		double balance_factor;
		ULONG leaf_pages;
		ULONG internal_pages;
		double avg_posting_compression;
		ULONG oversized_postings;
		bool needs_rebalancing;
		bool needs_compaction;
	};
	
	IndexHealthReport analyzeIndexHealth(thread_db* tdbb, ULONG root_page);
	
	//----------------------------
	// Maintenance Operations
	//----------------------------
	
	// Perform full index maintenance
	void performMaintenance(thread_db* tdbb, ULONG root_page, 
							bool force_rebalance, jrd_tra* transaction);
	
	// Rebuild index structure
	void rebuildIndex(thread_db* tdbb, ULONG root_page, jrd_tra* transaction);
	
	// Vacuum unused pages
	ULONG vacuumUnusedPages(thread_db* tdbb, ULONG root_page, jrd_tra* transaction);
	
	//----------------------------
	// Configuration Constants
	//----------------------------
	
	static const USHORT MAX_TOKEN_ENTRY_SIZE = 512;		// Maximum token entry size
	static const USHORT MAX_POSTING_ENTRY_SIZE = 8192;	// Maximum posting entry size
	static const USHORT MIN_PAGE_UTILIZATION = 50;		// Minimum page utilization %
	static const USHORT TARGET_PAGE_UTILIZATION = 75;	// Target page utilization %
	static const USHORT PAGE_SPLIT_THRESHOLD = 90;		// Page split threshold %
	static const USHORT PAGE_MERGE_THRESHOLD = 25;		// Page merge threshold %
	static const ULONG MAX_TREE_DEPTH = 16;			// Maximum tree depth
	static const USHORT PAGE_COMPACTION_THRESHOLD = 50;// Compaction threshold %

private:
	//----------------------------
	// Private Member Variables
	//----------------------------
	Database* m_database;
	const index_desc* m_index_desc;
	ULONG m_root_page;
	
	// Page cache for frequently accessed pages
	GenericMap<Pair<ULONG, pag*>> m_page_cache;
	ULONG m_cache_hits;
	ULONG m_cache_misses;
	
	//----------------------------
	// Private Helper Methods
	//----------------------------
	
	// Page allocation helpers
	ULONG allocateNewPage(thread_db* tdbb, jrd_tra* transaction);
	void initializePageHeader(gin_page_header* header, GinPageType page_type);
	
	// Token page helpers
	gin_token_entry* findTokenEntryOnPage(pag* page, const Token& token);
	bool insertTokenEntryOnPage(thread_db* tdbb, ULONG page_number, 
								const gin_token_entry* entry, jrd_tra* transaction);
	void splitTokenPage(thread_db* tdbb, ULONG page_number, jrd_tra* transaction);
	
	// Posting page helpers
	gin_posting_entry* findPostingEntryOnPage(gin_posting_page* page, USHORT offset);
	bool insertPostingEntryOnPage(thread_db* tdbb, ULONG page_number,
								  const gin_posting_entry* entry,
								  const UCHAR* data, jrd_tra* transaction);
	
	// Tree navigation helpers
	ULONG navigateToLeaf(thread_db* tdbb, ULONG start_page, const Token& token);
	bool shouldSplitPage(thread_db* tdbb, ULONG page_number, USHORT entry_size);
	bool shouldMergePages(thread_db* tdbb, ULONG page_number);
	
	// Space management helpers
	USHORT calculateFragmentation(thread_db* tdbb, ULONG page_number);
	void defragmentPage(thread_db* tdbb, ULONG page_number);
	
	// Cache management
	pag* getCachedPage(ULONG page_number);
	void cachePage(ULONG page_number, pag* page);
	void invalidatePageCache();
	
	// Validation helpers
	bool validatePageStructure(thread_db* tdbb, ULONG page_number);
	bool validateTreeIntegrity(thread_db* tdbb, ULONG root_page);
};

//----------------------------
// GIN Page Iterator
//----------------------------
class GinPageIterator
{
public:
	GinPageIterator(thread_db* tdbb, GinPageManager* manager, ULONG start_page);
	~GinPageIterator();
	
	// Iterator interface
	bool next();
	bool hasNext() const;
	gin_token_entry* getCurrentTokenEntry();
	ULONG getCurrentPageNumber() const;
	
	// Filtering options
	void setTokenFilter(const Token& min_token, const Token& max_token);
	void setPageTypeFilter(GinPageType page_type);
	
private:
	thread_db* m_tdbb;
	GinPageManager* m_manager;
	ULONG m_current_page;
	USHORT m_current_entry;
	Token m_min_token;
	Token m_max_token;
	GinPageType m_page_filter;
	bool m_has_token_filter;
	bool m_has_page_filter;
};

} // namespace Jrd

#endif // JRD_GIN_PAGE_MANAGER_H