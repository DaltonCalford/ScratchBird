/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		HashBucketManager.cpp
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

#include "scratchbird.h"
#include "../jrd/HashBucketManager.h"
#include "../jrd/jrd.h"
#include "../jrd/Database.h"
#include "../jrd/constants.h"
#include "../common/gdsassert.h"
#include "../jrd/err_proto.h"
#include <cstring>
#include <algorithm>

using namespace Jrd;
using namespace ScratchBird;

//----------------------------
// HashBucketManager Implementation
//----------------------------

HashBucketManager::HashBucketManager(CollisionStrategy strategy)
	: m_collision_strategy(strategy),
	  m_max_chain_length(HASH_BUCKET_CHAIN_LIMIT),
	  m_max_probe_distance(32),
	  m_total_insertions(0),
	  m_total_removals(0),
	  m_total_lookups(0),
	  m_total_collisions(0),
	  m_chain_splits(0),
	  m_overflow_buckets_created(0),
	  m_probe_failures(0)
{
}

HashBucketManager::~HashBucketManager()
{
	// Cleanup any resources if needed
}

//----------------------------
// Primary Bucket Operations
//----------------------------

bool HashBucketManager::insertEntry(thread_db* tdbb, Ods::hash_page* page, ULONG bucket_offset,
									 const dsc* key, ULONG key_hash, RecordNumber record)
{
	if (!page || !key || bucket_offset >= DEFAULT_PAGE_SIZE)
		return false;

	try
	{
		m_total_insertions++;

		HashBucketHeader* bucket = reinterpret_cast<HashBucketHeader*>(
			reinterpret_cast<UCHAR*>(page) + bucket_offset);
		UCHAR* bucket_data = reinterpret_cast<UCHAR*>(bucket) + sizeof(HashBucketHeader);
		USHORT bucket_size = page->hsh_bucket_size;

		// Choose collision resolution strategy
		switch (m_collision_strategy)
		{
			case COLLISION_CHAINING:
				return insertWithChaining(tdbb, bucket, bucket_data, bucket_size, key, key_hash, record);
			
			case COLLISION_LINEAR_PROBING:
				return insertWithLinearProbing(tdbb, page, bucket_offset, key, key_hash, record);
			
			case COLLISION_QUADRATIC_PROBING:
				return insertWithQuadraticProbing(tdbb, page, bucket_offset, key, key_hash, record);
			
			default:
				return insertWithChaining(tdbb, bucket, bucket_data, bucket_size, key, key_hash, record);
		}
	}
	catch (const Exception&)
	{
		return false;
	}
}

bool HashBucketManager::removeEntry(thread_db* tdbb, Ods::hash_page* page, ULONG bucket_offset,
									 const dsc* key, ULONG key_hash, RecordNumber record)
{
	if (!page || !key || bucket_offset >= DEFAULT_PAGE_SIZE)
		return false;

	try
	{
		m_total_removals++;

		HashBucketHeader* bucket = reinterpret_cast<HashBucketHeader*>(
			reinterpret_cast<UCHAR*>(page) + bucket_offset);
		UCHAR* bucket_data = reinterpret_cast<UCHAR*>(bucket) + sizeof(HashBucketHeader);

		// Choose collision resolution strategy
		switch (m_collision_strategy)
		{
			case COLLISION_CHAINING:
				return removeWithChaining(tdbb, bucket, bucket_data, key, key_hash, record);
			
			case COLLISION_LINEAR_PROBING:
			case COLLISION_QUADRATIC_PROBING:
				return removeWithProbing(tdbb, page, bucket_offset, key, key_hash, record);
			
			default:
				return removeWithChaining(tdbb, bucket, bucket_data, key, key_hash, record);
		}
	}
	catch (const Exception&)
	{
		return false;
	}
}

bool HashBucketManager::findEntry(thread_db* tdbb, const Ods::hash_page* page, ULONG bucket_offset,
								  const dsc* key, ULONG key_hash, RecordNumber* record)
{
	if (!page || !key || !record || bucket_offset >= DEFAULT_PAGE_SIZE)
		return false;

	try
	{
		m_total_lookups++;

		const HashBucketHeader* bucket = reinterpret_cast<const HashBucketHeader*>(
			reinterpret_cast<const UCHAR*>(page) + bucket_offset);
		const UCHAR* bucket_data = reinterpret_cast<const UCHAR*>(bucket) + sizeof(HashBucketHeader);

		// Choose collision resolution strategy
		switch (m_collision_strategy)
		{
			case COLLISION_CHAINING:
				return findWithChaining(bucket, bucket_data, key, key_hash, record);
			
			case COLLISION_LINEAR_PROBING:
			case COLLISION_QUADRATIC_PROBING:
				return findWithProbing(tdbb, page, bucket_offset, key, key_hash, record);
			
			default:
				return findWithChaining(bucket, bucket_data, key, key_hash, record);
		}
	}
	catch (const Exception&)
	{
		return false;
	}
}

//----------------------------
// Bucket Management Operations
//----------------------------

bool HashBucketManager::initializeBucket(HashBucketHeader* bucket, USHORT bucket_size)
{
	if (!bucket || bucket_size < sizeof(HashBucketHeader))
		return false;

	try
	{
		bucket->bucket_flags = HashBucketHeader::BUCKET_ACTIVE;
		bucket->entry_count = 0;
		bucket->used_space = 0;
		bucket->free_space = bucket_size - sizeof(HashBucketHeader);
		bucket->overflow_page = 0;
		bucket->first_entry_offset = 0;
		bucket->last_entry_offset = 0;
		bucket->collision_count = 0;

		return true;
	}
	catch (const Exception&)
	{
		return false;
	}
}

//----------------------------
// Collision Resolution - Separate Chaining
//----------------------------

bool HashBucketManager::insertWithChaining(thread_db* tdbb, HashBucketHeader* bucket, 
											UCHAR* bucket_data, USHORT bucket_size,
											const dsc* key, ULONG key_hash, RecordNumber record)
{
	if (!bucket || !bucket_data || !key)
		return false;

	// Check if key already exists
	HashBucketEntry* existing_entry = findEntryInChain(bucket_data, bucket->first_entry_offset, 
													   key, key_hash);
	if (existing_entry && existing_entry->record_number == record)
		return true; // Already exists

	// Calculate space needed for new entry
	USHORT entry_size = calculateEntrySize(key);
	USHORT required_space = calculateRequiredSpace(key);

	// Check if we have space in current bucket
	if (!hasSpaceForEntry(bucket, required_space))
	{
		// Check chain length before creating overflow bucket
		if (bucket->collision_count >= m_max_chain_length)
		{
			ULONG overflow_page;
			if (!createOverflowBucket(tdbb, bucket, &overflow_page))
				return false;
		}
		else
		{
			m_total_collisions++;
			bucket->collision_count++;
		}
	}

	// Allocate space for the new entry
	USHORT entry_offset = allocateEntrySpace(bucket, bucket_data, bucket_size, entry_size);
	if (entry_offset == 0)
		return false;

	// Create the new entry
	HashBucketEntry* new_entry = reinterpret_cast<HashBucketEntry*>(bucket_data + entry_offset);
	new_entry->entry_flags = HashBucketEntry::ENTRY_VALID;
	new_entry->key_length = calculateKeySize(key);
	new_entry->key_hash = key_hash;
	new_entry->record_number = record;
	new_entry->next_entry_offset = bucket->first_entry_offset;
	
	// Copy key data
	copyKeyData(new_entry->key_data, key);

	// Update bucket header
	bucket->first_entry_offset = entry_offset;
	bucket->entry_count++;
	bucket->used_space += entry_size;
	bucket->free_space -= entry_size;

	return true;
}

bool HashBucketManager::removeWithChaining(thread_db* tdbb, HashBucketHeader* bucket, 
											UCHAR* bucket_data, const dsc* key, 
											ULONG key_hash, RecordNumber record)
{
	if (!bucket || !bucket_data || !key)
		return false;

	USHORT prev_offset = 0;
	USHORT current_offset = bucket->first_entry_offset;

	while (current_offset != 0)
	{
		HashBucketEntry* entry = reinterpret_cast<HashBucketEntry*>(bucket_data + current_offset);
		
		// Check if this is the entry we're looking for
		if (entry->key_hash == key_hash && 
			entry->record_number == record &&
			compareKeys(key, entry->key_data, entry->key_length))
		{
			// Mark entry as deleted
			entry->setDeleted();
			
			// Update chain pointers if needed
			if (prev_offset == 0)
			{
				// This was the first entry
				bucket->first_entry_offset = entry->next_entry_offset;
			}
			else
			{
				HashBucketEntry* prev_entry = reinterpret_cast<HashBucketEntry*>(
					bucket_data + prev_offset);
				prev_entry->next_entry_offset = entry->next_entry_offset;
			}

			// Update bucket statistics
			bucket->entry_count--;
			bucket->used_space -= entry->getTotalSize();
			bucket->free_space += entry->getTotalSize();

			return true;
		}

		prev_offset = current_offset;
		current_offset = entry->next_entry_offset;
	}

	return false; // Entry not found
}

bool HashBucketManager::findWithChaining(const HashBucketHeader* bucket, const UCHAR* bucket_data,
										  const dsc* key, ULONG key_hash, RecordNumber* record)
{
	if (!bucket || !bucket_data || !key || !record)
		return false;

	HashBucketEntry* entry = findEntryInChain(bucket_data, bucket->first_entry_offset, 
											   key, key_hash);
	if (entry && entry->isValid())
	{
		*record = entry->record_number;
		return true;
	}

	return false;
}

//----------------------------
// Collision Resolution - Open Addressing (Linear/Quadratic Probing)
//----------------------------

bool HashBucketManager::insertWithLinearProbing(thread_db* tdbb, Ods::hash_page* page, 
												 ULONG base_bucket, const dsc* key, 
												 ULONG key_hash, RecordNumber record)
{
	if (!page || !key)
		return false;

	ULONG bucket_count = page->hsh_bucket_count;
	USHORT bucket_size = page->hsh_bucket_size;

	for (ULONG attempt = 0; attempt < m_max_probe_distance; attempt++)
	{
		ULONG probe_bucket = linearProbe(base_bucket, attempt, bucket_count);
		ULONG bucket_offset = probe_bucket * (sizeof(HashBucketHeader) + bucket_size);

		if (bucket_offset >= DEFAULT_PAGE_SIZE)
			break;

		HashBucketHeader* bucket = reinterpret_cast<HashBucketHeader*>(
			reinterpret_cast<UCHAR*>(page) + bucket_offset);
		UCHAR* bucket_data = reinterpret_cast<UCHAR*>(bucket) + sizeof(HashBucketHeader);

		// Check if bucket is empty or has space
		if (!bucket->isActive() || bucket->entry_count == 0)
		{
			// Initialize bucket if needed
			if (!bucket->isActive())
			{
				if (!initializeBucket(bucket, bucket_size))
					continue;
			}

			// Insert directly into this bucket
			return insertWithChaining(tdbb, bucket, bucket_data, bucket_size, key, key_hash, record);
		}
		
		// Check if key already exists in this bucket
		RecordNumber existing_record;
		if (findWithChaining(bucket, bucket_data, key, key_hash, &existing_record))
		{
			if (existing_record == record)
				return true; // Already exists
		}

		// Try next probe position
		if (attempt > 0)
			m_total_collisions++;
	}

	// Could not find a position after max probing distance
	m_probe_failures++;
	return false;
}

bool HashBucketManager::insertWithQuadraticProbing(thread_db* tdbb, Ods::hash_page* page, 
													ULONG base_bucket, const dsc* key, 
													ULONG key_hash, RecordNumber record)
{
	if (!page || !key)
		return false;

	ULONG bucket_count = page->hsh_bucket_count;
	USHORT bucket_size = page->hsh_bucket_size;

	for (ULONG attempt = 0; attempt < m_max_probe_distance; attempt++)
	{
		ULONG probe_bucket = quadraticProbe(base_bucket, attempt, bucket_count);
		ULONG bucket_offset = probe_bucket * (sizeof(HashBucketHeader) + bucket_size);

		if (bucket_offset >= DEFAULT_PAGE_SIZE)
			break;

		HashBucketHeader* bucket = reinterpret_cast<HashBucketHeader*>(
			reinterpret_cast<UCHAR*>(page) + bucket_offset);
		UCHAR* bucket_data = reinterpret_cast<UCHAR*>(bucket) + sizeof(HashBucketHeader);

		// Check if bucket is empty or has space
		if (!bucket->isActive() || bucket->entry_count == 0)
		{
			// Initialize bucket if needed
			if (!bucket->isActive())
			{
				if (!initializeBucket(bucket, bucket_size))
					continue;
			}

			// Insert directly into this bucket
			return insertWithChaining(tdbb, bucket, bucket_data, bucket_size, key, key_hash, record);
		}
		
		// Try next probe position
		if (attempt > 0)
			m_total_collisions++;
	}

	// Could not find a position after max probing distance
	m_probe_failures++;
	return false;
}

bool HashBucketManager::removeWithProbing(thread_db* tdbb, Ods::hash_page* page, 
										   ULONG base_bucket, const dsc* key, 
										   ULONG key_hash, RecordNumber record)
{
	if (!page || !key)
		return false;

	ULONG bucket_count = page->hsh_bucket_count;
	USHORT bucket_size = page->hsh_bucket_size;

	// Try both linear and quadratic probing patterns
	for (ULONG attempt = 0; attempt < m_max_probe_distance; attempt++)
	{
		ULONG probe_bucket = (m_collision_strategy == COLLISION_LINEAR_PROBING) ?
							  linearProbe(base_bucket, attempt, bucket_count) :
							  quadraticProbe(base_bucket, attempt, bucket_count);
		
		ULONG bucket_offset = probe_bucket * (sizeof(HashBucketHeader) + bucket_size);

		if (bucket_offset >= DEFAULT_PAGE_SIZE)
			break;

		HashBucketHeader* bucket = reinterpret_cast<HashBucketHeader*>(
			reinterpret_cast<UCHAR*>(page) + bucket_offset);
		UCHAR* bucket_data = reinterpret_cast<UCHAR*>(bucket) + sizeof(HashBucketHeader);

		if (bucket->isActive() && removeWithChaining(tdbb, bucket, bucket_data, key, key_hash, record))
			return true;
	}

	return false;
}

bool HashBucketManager::findWithProbing(thread_db* tdbb, const Ods::hash_page* page, 
										 ULONG base_bucket, const dsc* key, 
										 ULONG key_hash, RecordNumber* record)
{
	if (!page || !key || !record)
		return false;

	ULONG bucket_count = page->hsh_bucket_count;
	USHORT bucket_size = page->hsh_bucket_size;

	// Try both linear and quadratic probing patterns
	for (ULONG attempt = 0; attempt < m_max_probe_distance; attempt++)
	{
		ULONG probe_bucket = (m_collision_strategy == COLLISION_LINEAR_PROBING) ?
							  linearProbe(base_bucket, attempt, bucket_count) :
							  quadraticProbe(base_bucket, attempt, bucket_count);
		
		ULONG bucket_offset = probe_bucket * (sizeof(HashBucketHeader) + bucket_size);

		if (bucket_offset >= DEFAULT_PAGE_SIZE)
			break;

		const HashBucketHeader* bucket = reinterpret_cast<const HashBucketHeader*>(
			reinterpret_cast<const UCHAR*>(page) + bucket_offset);
		const UCHAR* bucket_data = reinterpret_cast<const UCHAR*>(bucket) + sizeof(HashBucketHeader);

		if (bucket->isActive() && findWithChaining(bucket, bucket_data, key, key_hash, record))
			return true;
	}

	return false;
}

//----------------------------
// Utility Methods
//----------------------------

USHORT HashBucketManager::calculateEntrySize(const dsc* key)
{
	if (!key)
		return 0;
	
	return sizeof(HashBucketEntry) - 1 + key->dsc_length;
}

USHORT HashBucketManager::calculateRequiredSpace(const dsc* key)
{
	// Add some padding for alignment
	return calculateEntrySize(key) + 4;
}

bool HashBucketManager::hasSpaceForEntry(const HashBucketHeader* bucket, USHORT entry_size)
{
	return bucket && bucket->free_space >= entry_size;
}

bool HashBucketManager::compareKeys(const dsc* key1, const UCHAR* key2_data, USHORT key2_length)
{
	if (!key1 || !key2_data)
		return false;
	
	if (key1->dsc_length != key2_length)
		return false;
	
	return memcmp(key1->dsc_address, key2_data, key2_length) == 0;
}

USHORT HashBucketManager::calculateKeySize(const dsc* key)
{
	return key ? key->dsc_length : 0;
}

void HashBucketManager::copyKeyData(UCHAR* dest, const dsc* source)
{
	if (dest && source && source->dsc_address)
	{
		memcpy(dest, source->dsc_address, source->dsc_length);
	}
}

//----------------------------
// Private Helper Methods
//----------------------------

HashBucketEntry* HashBucketManager::findEntryInChain(const UCHAR* bucket_data, USHORT first_offset,
													  const dsc* key, ULONG key_hash)
{
	USHORT current_offset = first_offset;
	
	while (current_offset != 0)
	{
		HashBucketEntry* entry = reinterpret_cast<HashBucketEntry*>(
			const_cast<UCHAR*>(bucket_data) + current_offset);
		
		if (entry->isValid() && 
			entry->key_hash == key_hash && 
			compareKeys(key, entry->key_data, entry->key_length))
		{
			return entry;
		}
		
		current_offset = entry->next_entry_offset;
	}
	
	return nullptr;
}

USHORT HashBucketManager::allocateEntrySpace(HashBucketHeader* bucket, UCHAR* bucket_data, 
											  USHORT bucket_size, USHORT entry_size)
{
	// Simple allocation - find first free space
	// In a real implementation, we'd use a more sophisticated allocator
	if (bucket->free_space < entry_size)
		return 0;
	
	// Allocate at the end of used space
	USHORT offset = bucket->used_space + sizeof(HashBucketHeader);
	if (offset + entry_size > bucket_size)
		return 0;
	
	return offset;
}

//----------------------------
// Probing Methods
//----------------------------

ULONG HashBucketManager::linearProbe(ULONG base_bucket, ULONG attempt, ULONG bucket_count)
{
	return (base_bucket + attempt) % bucket_count;
}

ULONG HashBucketManager::quadraticProbe(ULONG base_bucket, ULONG attempt, ULONG bucket_count)
{
	return (base_bucket + attempt * attempt) % bucket_count;
}

ULONG HashBucketManager::doubleHash(ULONG base_bucket, ULONG key_hash, ULONG attempt, ULONG bucket_count)
{
	// Use a second hash function for double hashing
	ULONG second_hash = 7 - (key_hash % 7); // Simple second hash function
	return (base_bucket + attempt * second_hash) % bucket_count;
}

//----------------------------
// Statistics Methods
//----------------------------

void HashBucketManager::getBucketStatistics(const HashBucketHeader* bucket, 
											 ULONG* entry_count, ULONG* collision_count, 
											 double* load_factor, USHORT* chain_length)
{
	if (!bucket)
		return;
		
	if (entry_count)
		*entry_count = bucket->entry_count;
	if (collision_count)
		*collision_count = bucket->collision_count;
	if (load_factor)
		*load_factor = bucket->free_space > 0 ? 
					   static_cast<double>(bucket->used_space) / (bucket->used_space + bucket->free_space) : 
					   1.0;
	if (chain_length)
	{
		// Calculate chain length by traversing entries
		*chain_length = bucket->entry_count; // Simplified
	}
}

//----------------------------
// Overflow bucket management (placeholders for now)
//----------------------------

bool HashBucketManager::createOverflowBucket(thread_db* tdbb, HashBucketHeader* bucket, ULONG* overflow_page)
{
	// TODO: Implement overflow bucket creation
	m_overflow_buckets_created++;
	return false; // Not implemented yet
}

bool HashBucketManager::freeOverflowBucket(thread_db* tdbb, ULONG overflow_page)
{
	// TODO: Implement overflow bucket cleanup
	return false; // Not implemented yet
}

HashBucketHeader* HashBucketManager::getOverflowBucket(thread_db* tdbb, ULONG overflow_page)
{
	// TODO: Implement overflow bucket retrieval
	return nullptr; // Not implemented yet
}

//----------------------------
// HashBucketIterator Implementation
//----------------------------

HashBucketIterator::HashBucketIterator(const HashBucketHeader* bucket, const UCHAR* bucket_data)
	: m_bucket(bucket),
	  m_bucket_data(bucket_data),
	  m_current_offset(0),
	  m_current_entry(nullptr),
	  m_at_end(false)
{
	if (bucket)
	{
		m_current_offset = bucket->first_entry_offset;
		if (m_current_offset != 0)
		{
			m_current_entry = reinterpret_cast<const HashBucketEntry*>(
				bucket_data + m_current_offset);
		}
		else
		{
			m_at_end = true;
		}
	}
}

HashBucketIterator::~HashBucketIterator()
{
}

bool HashBucketIterator::hasNext() const
{
	return !m_at_end && m_current_entry != nullptr;
}

const HashBucketEntry* HashBucketIterator::next()
{
	if (m_at_end || !m_current_entry)
		return nullptr;

	const HashBucketEntry* result = m_current_entry;
	
	// Move to next entry
	m_current_offset = m_current_entry->next_entry_offset;
	if (m_current_offset != 0)
	{
		m_current_entry = reinterpret_cast<const HashBucketEntry*>(
			m_bucket_data + m_current_offset);
	}
	else
	{
		m_current_entry = nullptr;
		m_at_end = true;
	}
	
	return result;
}

void HashBucketIterator::reset()
{
	if (m_bucket)
	{
		m_current_offset = m_bucket->first_entry_offset;
		if (m_current_offset != 0)
		{
			m_current_entry = reinterpret_cast<const HashBucketEntry*>(
				m_bucket_data + m_current_offset);
			m_at_end = false;
		}
		else
		{
			m_current_entry = nullptr;
			m_at_end = true;
		}
	}
}

const HashBucketEntry* HashBucketIterator::getCurrentEntry() const
{
	return m_current_entry;
}

bool HashBucketIterator::isCurrentEntryValid() const
{
	return m_current_entry && m_current_entry->isValid();
}