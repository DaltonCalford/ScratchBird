/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		HashBucketManager.h
 *	DESCRIPTION:	Hash bucket management with collision handling
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
 * 2025.07.22 - ScratchBird Hash Index Implementation - Bucket Manager
 */

#ifndef JRD_HASH_BUCKET_MANAGER_H
#define JRD_HASH_BUCKET_MANAGER_H

#include "../jrd/constants.h"
#include "../jrd/ods.h"
#include "../jrd/RecordNumber.h"
#include "../common/classes/array.h"

namespace Jrd {

// Forward declarations
class thread_db;
struct dsc;

//----------------------------
// Collision Resolution Strategy
//----------------------------
enum CollisionStrategy
{
	COLLISION_CHAINING = 0,		// Separate chaining with linked lists
	COLLISION_LINEAR_PROBING = 1,	// Open addressing with linear probing
	COLLISION_QUADRATIC_PROBING = 2,	// Open addressing with quadratic probing
	COLLISION_DOUBLE_HASHING = 3	// Open addressing with double hashing
};

//----------------------------
// HashBucketEntry - Individual entry within a bucket
//----------------------------
struct HashBucketEntry
{
	USHORT entry_flags;			// Entry flags (deleted, overflow, etc.)
	USHORT key_length;			// Length of the key in bytes
	ULONG key_hash;				// Cached hash value for quick comparison
	RecordNumber record_number;	// Record number this entry points to
	USHORT next_entry_offset;	// Offset to next entry (for chaining)
	UCHAR key_data[1];			// Variable length key data

	// Entry flags
	static const USHORT ENTRY_VALID = 0x0001;		// Entry contains valid data
	static const USHORT ENTRY_DELETED = 0x0002;	// Entry has been deleted (tombstone)
	static const USHORT ENTRY_OVERFLOW = 0x0004;	// Entry continues in overflow bucket
	static const USHORT ENTRY_COMPRESSED = 0x0008;	// Key data is compressed

	// Helper methods
	bool isValid() const { return (entry_flags & ENTRY_VALID) != 0; }
	bool isDeleted() const { return (entry_flags & ENTRY_DELETED) != 0; }
	bool hasOverflow() const { return (entry_flags & ENTRY_OVERFLOW) != 0; }
	bool isCompressed() const { return (entry_flags & ENTRY_COMPRESSED) != 0; }

	void setValid() { entry_flags |= ENTRY_VALID; }
	void setDeleted() { entry_flags |= ENTRY_DELETED; entry_flags &= ~ENTRY_VALID; }
	void setOverflow() { entry_flags |= ENTRY_OVERFLOW; }
	void setCompressed() { entry_flags |= ENTRY_COMPRESSED; }

	USHORT getTotalSize() const { return sizeof(HashBucketEntry) - 1 + key_length; }
};

//----------------------------
// HashBucket - Enhanced bucket structure with collision handling
//----------------------------
struct HashBucketHeader
{
	USHORT bucket_flags;		// Bucket flags
	USHORT entry_count;			// Number of entries in this bucket
	USHORT used_space;			// Space used by entries in bytes
	USHORT free_space;			// Free space remaining in bytes
	ULONG overflow_page;		// Page number of overflow bucket (0 if none)
	USHORT first_entry_offset;	// Offset to first entry in bucket
	USHORT last_entry_offset;	// Offset to last entry in bucket
	USHORT collision_count;		// Number of collisions in this bucket

	// Bucket flags
	static const USHORT BUCKET_ACTIVE = 0x0001;		// Bucket is active and in use
	static const USHORT BUCKET_OVERFLOW = 0x0002;	// This is an overflow bucket
	static const USHORT BUCKET_COMPRESSED = 0x0004;	// Bucket data is compressed
	static const USHORT BUCKET_FULL = 0x0008;		// Bucket is at capacity
	static const USHORT BUCKET_SPLITTING = 0x0010;	// Bucket is being split

	// Helper methods
	bool isActive() const { return (bucket_flags & BUCKET_ACTIVE) != 0; }
	bool isOverflow() const { return (bucket_flags & BUCKET_OVERFLOW) != 0; }
	bool isCompressed() const { return (bucket_flags & BUCKET_COMPRESSED) != 0; }
	bool isFull() const { return (bucket_flags & BUCKET_FULL) != 0; }
	bool isSplitting() const { return (bucket_flags & BUCKET_SPLITTING) != 0; }

	void setActive() { bucket_flags |= BUCKET_ACTIVE; }
	void setOverflow() { bucket_flags |= BUCKET_OVERFLOW; }
	void setCompressed() { bucket_flags |= BUCKET_COMPRESSED; }
	void setFull() { bucket_flags |= BUCKET_FULL; }
	void setSplitting() { bucket_flags |= BUCKET_SPLITTING; }
	void clearFull() { bucket_flags &= ~BUCKET_FULL; }
	void clearSplitting() { bucket_flags &= ~BUCKET_SPLITTING; }
};

//----------------------------
// HashBucketManager - Manages hash buckets and collision resolution
//----------------------------
class HashBucketManager
{
public:
	// Constructor and destructor
	HashBucketManager(CollisionStrategy strategy = COLLISION_CHAINING);
	~HashBucketManager();

	// Primary bucket operations
	bool insertEntry(thread_db* tdbb, Ods::hash_page* page, ULONG bucket_offset,
					 const dsc* key, ULONG key_hash, RecordNumber record);
	bool removeEntry(thread_db* tdbb, Ods::hash_page* page, ULONG bucket_offset,
					 const dsc* key, ULONG key_hash, RecordNumber record);
	bool findEntry(thread_db* tdbb, const Ods::hash_page* page, ULONG bucket_offset,
				   const dsc* key, ULONG key_hash, RecordNumber* record);

	// Bucket management operations
	bool initializeBucket(HashBucketHeader* bucket, USHORT bucket_size);
	bool splitBucket(thread_db* tdbb, Ods::hash_page* page, ULONG bucket_offset,
					 Ods::hash_page* new_page, ULONG new_bucket_offset, ULONG new_hash_mask);
	bool mergeBuckets(thread_db* tdbb, Ods::hash_page* page1, ULONG bucket1_offset,
					  Ods::hash_page* page2, ULONG bucket2_offset);
	bool compactBucket(thread_db* tdbb, Ods::hash_page* page, ULONG bucket_offset);

	// Collision resolution methods
	bool insertWithChaining(thread_db* tdbb, HashBucketHeader* bucket, UCHAR* bucket_data,
							USHORT bucket_size, const dsc* key, ULONG key_hash, RecordNumber record);
	bool insertWithLinearProbing(thread_db* tdbb, Ods::hash_page* page, ULONG base_bucket,
								 const dsc* key, ULONG key_hash, RecordNumber record);
	bool insertWithQuadraticProbing(thread_db* tdbb, Ods::hash_page* page, ULONG base_bucket,
									const dsc* key, ULONG key_hash, RecordNumber record);

	bool removeWithChaining(thread_db* tdbb, HashBucketHeader* bucket, UCHAR* bucket_data,
							const dsc* key, ULONG key_hash, RecordNumber record);
	bool removeWithProbing(thread_db* tdbb, Ods::hash_page* page, ULONG base_bucket,
						   const dsc* key, ULONG key_hash, RecordNumber record);

	bool findWithChaining(const HashBucketHeader* bucket, const UCHAR* bucket_data,
						  const dsc* key, ULONG key_hash, RecordNumber* record);
	bool findWithProbing(thread_db* tdbb, const Ods::hash_page* page, ULONG base_bucket,
						 const dsc* key, ULONG key_hash, RecordNumber* record);

	// Overflow bucket management
	bool createOverflowBucket(thread_db* tdbb, HashBucketHeader* bucket, ULONG* overflow_page);
	bool freeOverflowBucket(thread_db* tdbb, ULONG overflow_page);
	HashBucketHeader* getOverflowBucket(thread_db* tdbb, ULONG overflow_page);

	// Utility methods
	USHORT calculateEntrySize(const dsc* key);
	USHORT calculateRequiredSpace(const dsc* key);
	bool hasSpaceForEntry(const HashBucketHeader* bucket, USHORT entry_size);
	bool compareKeys(const dsc* key1, const UCHAR* key2_data, USHORT key2_length);
	void copyKeyData(UCHAR* dest, const dsc* source);

	// Statistics and introspection
	void getBucketStatistics(const HashBucketHeader* bucket, 
							 ULONG* entry_count, ULONG* collision_count, 
							 double* load_factor, USHORT* chain_length);
	ULONG calculateTotalCollisions(thread_db* tdbb, const Ods::hash_page* page);
	double calculateAverageChainLength(thread_db* tdbb, const Ods::hash_page* page);

	// Configuration methods
	void setCollisionStrategy(CollisionStrategy strategy) { m_collision_strategy = strategy; }
	CollisionStrategy getCollisionStrategy() const { return m_collision_strategy; }
	void setMaxChainLength(USHORT max_length) { m_max_chain_length = max_length; }
	USHORT getMaxChainLength() const { return m_max_chain_length; }

private:
	// Private helper methods
	HashBucketEntry* findEntryInChain(const UCHAR* bucket_data, USHORT first_offset,
									  const dsc* key, ULONG key_hash);
	USHORT getNextEntryOffset(const UCHAR* bucket_data, USHORT current_offset);
	void setNextEntryOffset(UCHAR* bucket_data, USHORT current_offset, USHORT next_offset);
	
	USHORT allocateEntrySpace(HashBucketHeader* bucket, UCHAR* bucket_data, 
							  USHORT bucket_size, USHORT entry_size);
	void deallocateEntrySpace(HashBucketHeader* bucket, UCHAR* bucket_data, 
							  USHORT entry_offset, USHORT entry_size);

	// Probing methods for open addressing
	ULONG linearProbe(ULONG base_bucket, ULONG attempt, ULONG bucket_count);
	ULONG quadraticProbe(ULONG base_bucket, ULONG attempt, ULONG bucket_count);
	ULONG doubleHash(ULONG base_bucket, ULONG key_hash, ULONG attempt, ULONG bucket_count);

	// Bucket validation and repair
	bool validateBucketStructure(const HashBucketHeader* bucket, const UCHAR* bucket_data, 
								 USHORT bucket_size);
	bool repairBucketChain(thread_db* tdbb, HashBucketHeader* bucket, UCHAR* bucket_data, 
						   USHORT bucket_size);

	// Member variables
	CollisionStrategy m_collision_strategy;	// Current collision resolution strategy
	USHORT m_max_chain_length;				// Maximum allowed chain length before overflow
	USHORT m_max_probe_distance;			// Maximum probing distance for open addressing
	
	// Statistics
	mutable ULONG m_total_insertions;		// Total insertions performed
	mutable ULONG m_total_removals;			// Total removals performed
	mutable ULONG m_total_lookups;			// Total lookups performed
	mutable ULONG m_total_collisions;		// Total collisions encountered
	mutable ULONG m_chain_splits;			// Number of chain splits performed
	mutable ULONG m_overflow_buckets_created;	// Number of overflow buckets created
	mutable ULONG m_probe_failures;			// Number of probing failures (table full)
};

//----------------------------
// HashBucketIterator - Iterator for traversing bucket entries
//----------------------------
class HashBucketIterator
{
public:
	HashBucketIterator(const HashBucketHeader* bucket, const UCHAR* bucket_data);
	~HashBucketIterator();

	// Iterator interface
	bool hasNext() const;
	const HashBucketEntry* next();
	void reset();

	// Current entry access
	const HashBucketEntry* getCurrentEntry() const;
	bool isCurrentEntryValid() const;

private:
	const HashBucketHeader* m_bucket;
	const UCHAR* m_bucket_data;
	USHORT m_current_offset;
	const HashBucketEntry* m_current_entry;
	bool m_at_end;
};

} // namespace Jrd

#endif // JRD_HASH_BUCKET_MANAGER_H