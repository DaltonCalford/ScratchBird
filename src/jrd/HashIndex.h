/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		HashIndex.h
 *	DESCRIPTION:	Hash index implementation for equality lookups
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
 * Contributor(s): ________________________________________.
 *
 * 2025.07.22 - ScratchBird Hash Index Implementation
 */

#ifndef JRD_HASH_INDEX_H
#define JRD_HASH_INDEX_H

#include "../jrd/IndexType.h"
#include "../jrd/constants.h"
#include "../jrd/ods.h"
#include "../common/classes/array.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/fb_pair.h"

namespace Jrd {

// Forward declarations
class Database;
class jrd_rel;
class index_desc;
class thread_db;
class jrd_tra;
class IndexRetrieval;
struct dsc;

//----------------------------
// Hash Index Constants
//----------------------------
const USHORT HASH_DEFAULT_BUCKETS = 64;			// Default number of buckets
const USHORT HASH_MAX_ENTRIES_PER_BUCKET = 32;		// Max entries per bucket before overflow
const USHORT HASH_BUCKET_DATA_SIZE = 2048;			// Default bucket data allocation size
const float HASH_MIN_LOAD_FACTOR = 0.25f;			// Minimum load factor before shrinking
const float HASH_MAX_LOAD_FACTOR = 0.75f;			// Maximum load factor before expanding

//----------------------------
// HashBucket - Individual hash bucket structure
//----------------------------
struct HashBucket
{
	USHORT hb_size;				// Size of this bucket in bytes
	USHORT hb_count;			// Number of keys in this bucket
	USHORT hb_free_space;		// Free space remaining in bucket
	USHORT hb_flags;			// Bucket flags
	UCHAR* hb_data;				// Pointer to key-value pairs data
};

//----------------------------
// HashKeyEntry - Individual key-value entry within a bucket
//----------------------------
struct HashKeyEntry
{
	USHORT key_length;			// Length of the key
	RecordNumber record_number;	// Record number this key points to
	UCHAR key_data[1];			// Variable length key data
};

//----------------------------
// HashIndex - Main hash index implementation class
//----------------------------
class HashIndex : public IndexType
{
public:
	// Constructor and destructor
	HashIndex(thread_db* tdbb, Database* database, jrd_rel* relation, 
			  const index_desc* desc);
	virtual ~HashIndex();

	// IndexType interface implementation
	virtual index_error_t initialize(thread_db* tdbb, Database* database, 
									 jrd_rel* relation, const index_desc* desc) override;
	virtual index_error_t insert(thread_db* tdbb, const dsc* key, 
								 RecordNumber record, jrd_tra* transaction) override;
	virtual bool lookup(thread_db* tdbb, const dsc* key, IndexRetrieval* retrieval) override;
	virtual index_error_t remove(thread_db* tdbb, const dsc* key, 
								 RecordNumber record, jrd_tra* transaction) override;
	virtual double calculateSelectivity(thread_db* tdbb, const dsc* key) override;
	virtual index_error_t getStatistics(thread_db* tdbb, IndexStatistics* stats) override;
	virtual index_error_t validate(thread_db* tdbb) override;
	virtual index_error_t rebuild(thread_db* tdbb, jrd_tra* transaction) override;
	virtual const char* getTypeName() const override { return IDX_TYPE_NAME_HASH; }
	virtual const char* getVersion() const override;
	virtual bool supportsDataType(int field_type) const override;
	virtual bool supportsIndexFlags(USHORT flags) const override;
	virtual USHORT getOptimalPageSize(USHORT avg_key_length, ULONG cardinality) const override;
	virtual ULONG estimateStorageSize(ULONG num_keys, USHORT avg_key_length) const override;

	// Hash index specific operations
	bool expand(thread_db* tdbb);			// Expand hash table when load factor too high
	bool shrink(thread_db* tdbb);			// Shrink hash table when load factor too low
	bool reorganize(thread_db* tdbb);		// Reorganize hash table for optimal performance

	// Statistics and introspection
	double getLoadFactor() const;
	ULONG getBucketCount() const;
	ULONG getKeyCount() const;
	ULONG getPageCount() const;

	// Hash function interface
	ULONG hash(const UCHAR* key_data, USHORT key_length) const;

private:
	// Private implementation methods
	ULONG findBucket(const dsc* key) const;
	HashBucket* getBucket(thread_db* tdbb, ULONG bucket_number);
	bool insertIntoBucket(thread_db* tdbb, HashBucket* bucket, 
						  const dsc* key, RecordNumber record);
	bool removeFromBucket(thread_db* tdbb, HashBucket* bucket, 
						  const dsc* key, RecordNumber record);
	bool findInBucket(const HashBucket* bucket, const dsc* key, 
					  RecordNumber* record) const;

	// Page management
	Ods::hash_page* getHashPage(thread_db* tdbb, ULONG page_number);
	ULONG allocateHashPage(thread_db* tdbb);
	void releaseHashPage(thread_db* tdbb, ULONG page_number);

	// Bucket management
	bool splitBucket(thread_db* tdbb, ULONG bucket_number);
	bool mergeBucket(thread_db* tdbb, ULONG bucket_number);
	ULONG calculateOptimalBucketCount() const;

	// Utility functions
	USHORT calculateKeySize(const dsc* key) const;
	bool compareKeys(const UCHAR* key1, USHORT len1, 
					 const UCHAR* key2, USHORT len2) const;
	void copyKeyData(UCHAR* dest, const dsc* source) const;

	// Hash algorithms
	ULONG hashCRC32(const UCHAR* key_data, USHORT key_length) const;
	ULONG hashMurmurHash3(const UCHAR* key_data, USHORT key_length) const;

protected:
	// Member variables
	Database* m_database;			// Database this index belongs to
	jrd_rel* m_relation;			// Relation this index is on
	const index_desc* m_index_desc;// Index descriptor
	UCHAR m_index_id;				// Index ID
	USHORT m_relation_id;			// Relation ID
	UCHAR m_hash_algorithm;			// Hash algorithm to use
	USHORT m_bucket_count;			// Current number of buckets
	USHORT m_bucket_size;			// Size of each bucket
	UCHAR m_target_load_factor;		// Target load factor percentage
	ULONG m_total_keys;				// Total number of keys in index
	ULONG m_root_page;				// Root page number for this hash index

	// Bucket storage for hash index
	HashBucket* m_buckets;				// Array of hash buckets
	
	// Page cache for frequently accessed hash pages
	typedef ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::NonPooled<ULONG, Ods::hash_page*>>> PageCache;
	PageCache m_page_cache;

	// Statistics
	mutable ULONG m_stats_lookups;		// Number of lookups performed
	mutable ULONG m_stats_inserts;		// Number of inserts performed
	mutable ULONG m_stats_removes;		// Number of removes performed
	mutable ULONG m_stats_collisions;	// Number of hash collisions encountered
	mutable ULONG m_stats_expansions;	// Number of times hash table was expanded
	mutable ULONG m_stats_shrinks;		// Number of times hash table was shrunk
};

//----------------------------
// HashIndexFactory - Factory for creating HashIndex instances
//----------------------------
class HashIndexFactory : public IndexTypeFactory
{
public:
	virtual IndexType* createIndex(thread_db* tdbb, Database* database,
								   jrd_rel* relation, const index_desc* desc) override
	{
		return new HashIndex(tdbb, database, relation, desc);
	}

	virtual const char* getTypeName() const override
	{
		return IDX_TYPE_NAME_HASH;
	}

	virtual int getTypeId() const override
	{
		return IDX_TYPE_HASH;
	}
};

} // namespace Jrd

#endif // JRD_HASH_INDEX_H