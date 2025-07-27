/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created for the ScratchBird Open Source 
 *  RDBMS project.
 *
 *  Copyright (c) 2025 ScratchBird Project
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 * 2025.07.22 - ScratchBird Hash Index Implementation - HashTableScan
 */

#include "scratchbird.h"
#include "HashTableScan.h"
#include "../jrd.h"
#include "../exe.h"
#include "../req.h"
#include "../btr.h"
#include "../Relation.h"
#include "../constants.h"
#include "../cch_proto.h"
#include "../cmp_proto.h"
#include "../evl_proto.h"
#include "../met_proto.h"
#include "../vio_proto.h"
#include "../rlck_proto.h"
#include "../RecordSourceNodes.h"
#include "../common/gdsassert.h"

using namespace ScratchBird;
using namespace Jrd;

//----------------------------
// HashTableScan Implementation
//----------------------------

HashTableScan::HashTableScan(CompilerScratch* csb, const string& alias,
							  StreamType stream, jrd_rel* relation,
							  InversionNode* index, HashIndex* hash_index,
							  USHORT key_length, double selectivity)
	: RecordStream(csb, stream),
	  m_alias(csb->csb_pool, alias),
	  m_relation(relation),
	  m_index(index),
	  m_hash_index(hash_index),
	  m_inversion(nullptr),
	  m_condition(nullptr),
	  m_key_length(key_length),
	  m_impure_offset(0),
	  m_bucket_manager(COLLISION_CHAINING),
	  m_allow_duplicates(true),
	  m_max_buckets_to_scan(1000),
	  m_selectivity(selectivity),
	  m_prefer_equality(true),
	  m_sequential_hint(false)
{
	fb_assert(m_hash_index);
	fb_assert(relation);

	// Calculate impure area size
	FB_SIZE_T size = sizeof(Impure);
	size += 2 * m_key_length;  // Space for key buffers
	size = FB_ALIGN(size, FB_ALIGNMENT);

	m_impure = csb->allocImpure(FB_ALIGNMENT, static_cast<ULONG>(size));
	m_cardinality = csb->csb_rpt[stream].csb_cardinality * selectivity;
}

HashTableScan::~HashTableScan()
{
	// Cleanup handled by destructors
}

//----------------------------
// RecordSource Interface Implementation
//----------------------------

void HashTableScan::open(thread_db* tdbb) const
{
	internalOpen(tdbb);
}

void HashTableScan::internalOpen(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	// Initialize impure area
	impure->irsb_flags = irsb_first | irsb_open;
	impure->retrieval_state = HASH_RETRIEVAL_NONE;
	impure->scan_direction = HASH_SCAN_FORWARD;
	impure->current_bucket = 0;
	impure->bucket_count = m_hash_index->getBucketCount();
	impure->current_record = RecordNumber(BOF_NUMBER);
	impure->search_key = nullptr;
	impure->bucket_iterator = nullptr;

	// Initialize key buffers
	impure->key_data = reinterpret_cast<UCHAR*>(impure) + sizeof(Impure);
	impure->key_length = 0;
	impure->key_hash = 0;
	impure->key_found = false;

	// Initialize statistics
	impure->buckets_scanned = 0;
	impure->collisions_encountered = 0;
	impure->keys_examined = 0;

	// Set up record parameter block
	record_param* const rpb = &request->req_rpb[m_stream];
	RLCK_reserve_relation(tdbb, request->req_transaction, m_relation, false);
	rpb->rpb_number.setValue(BOF_NUMBER);
}

void HashTableScan::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	invalidateRecords(request);

	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->irsb_flags & irsb_open)
	{
		impure->irsb_flags &= ~irsb_open;

		// Cleanup bucket iterator
		if (impure->bucket_iterator)
		{
			delete impure->bucket_iterator;
			impure->bucket_iterator = nullptr;
		}

		// Cleanup search key
		if (impure->search_key)
		{
			delete impure->search_key;
			impure->search_key = nullptr;
		}
	}
}

bool HashTableScan::getRecord(thread_db* tdbb) const
{
	if (--tdbb->tdbb_quantum < 0)
		JRD_reschedule(tdbb, true);

	Request* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!(impure->irsb_flags & irsb_open))
		return false;

	// Handle first call
	if (impure->irsb_flags & irsb_first)
	{
		impure->irsb_flags &= ~irsb_first;
		
		// Initialize retrieval based on inversion
		if (m_inversion)
		{
			// TODO: Analyze inversion to determine retrieval strategy
			// For now, assume equality lookup
			initializeRetrieval(tdbb, HASH_RETRIEVAL_EQUALITY);
		}
		else
		{
			// Full scan
			initializeRetrieval(tdbb, HASH_RETRIEVAL_SCAN);
		}
	}

	// Retrieve next record based on current state
	switch (impure->retrieval_state)
	{
		case HASH_RETRIEVAL_EQUALITY:
			return retrieveEqualityMatch(tdbb);
		
		case HASH_RETRIEVAL_SCAN:
			if (impure->scan_direction == HASH_SCAN_FORWARD)
				return retrieveNextInScan(tdbb);
			else
				return retrievePreviousInScan(tdbb);
		
		default:
			return false;
	}
}

bool HashTableScan::refetchRecord(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];

	if (rpb->rpb_number.isBof() || rpb->rpb_number.isEof())
		return false;

	return fetchRecordByNumber(tdbb, rpb->rpb_number);
}

bool HashTableScan::lockRecord(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];

	if (rpb->rpb_number.isBof() || rpb->rpb_number.isEof())
		return false;

	// Use standard record locking mechanism
	return VIO_get(tdbb, rpb, request->req_transaction, tdbb->getDefaultPool());
}

void HashTableScan::markRecursive()
{
	m_recursive = true;
}

void HashTableScan::invalidateRecords(Request* request) const
{
	record_param* const rpb = &request->req_rpb[m_stream];
	invalidateRecord(request, rpb);
}

//----------------------------
// Hash-Specific Methods
//----------------------------

bool HashTableScan::seekEqualKey(thread_db* tdbb, temporary_key* key) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!key || !key->key_data)
		return false;

	// Store search key
	if (impure->search_key)
		delete impure->search_key;
	
	impure->search_key = FB_NEW_POOL(*tdbb->getDefaultPool()) temporary_key;
	impure->search_key->key_length = key->key_length;
	impure->search_key->key_data = FB_NEW_POOL(*tdbb->getDefaultPool()) UCHAR[key->key_length];
	memcpy(impure->search_key->key_data, key->key_data, key->key_length);

	// Calculate hash and position
	impure->key_hash = calculateHash(key->key_data, key->key_length);
	ULONG target_bucket = impure->key_hash % impure->bucket_count;

	return positionAtBucket(tdbb, target_bucket);
}

bool HashTableScan::seekFirstKey(thread_db* tdbb) const
{
	return positionAtBucket(tdbb, 0);
}

bool HashTableScan::seekLastKey(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);
	
	return positionAtBucket(tdbb, impure->bucket_count - 1);
}

bool HashTableScan::seekNextKey(thread_db* tdbb) const
{
	return retrieveNextInScan(tdbb);
}

bool HashTableScan::seekPriorKey(thread_db* tdbb) const
{
	return retrievePreviousInScan(tdbb);
}

//----------------------------
// Private Implementation Methods
//----------------------------

void HashTableScan::initializeRetrieval(thread_db* tdbb, HashRetrievalState state) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->retrieval_state = state;
	impure->buckets_scanned = 0;
	impure->collisions_encountered = 0;
	impure->keys_examined = 0;

	switch (state)
	{
		case HASH_RETRIEVAL_EQUALITY:
			setupEqualityLookup(tdbb, impure->search_key);
			break;
		
		case HASH_RETRIEVAL_SCAN:
			setupFullScan(tdbb, HASH_SCAN_FORWARD);
			break;
		
		default:
			// Invalid state
			break;
	}
}

void HashTableScan::setupEqualityLookup(thread_db* tdbb, temporary_key* key) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!key)
		return;

	// Position at target bucket
	impure->key_hash = calculateHash(key->key_data, key->key_length);
	ULONG target_bucket = impure->key_hash % impure->bucket_count;
	
	impure->current_bucket = target_bucket;
	impure->irsb_flags |= irsb_equality;
}

void HashTableScan::setupFullScan(thread_db* tdbb, HashScanDirection direction) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	impure->scan_direction = direction;
	if (direction == HASH_SCAN_FORWARD)
		impure->current_bucket = 0;
	else
		impure->current_bucket = impure->bucket_count - 1;

	impure->irsb_flags &= ~irsb_equality;
}

bool HashTableScan::retrieveEqualityMatch(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (!impure->search_key)
		return false;

	// Initialize bucket iterator if needed
	if (!impure->bucket_iterator)
	{
		if (!initializeBucketIterator(tdbb, impure->current_bucket))
			return false;
	}

	// Search for matching key in current bucket
	while (impure->bucket_iterator && impure->bucket_iterator->hasNext())
	{
		const HashBucketEntry* entry = impure->bucket_iterator->next();
		if (!entry || !entry->isValid())
			continue;

		impure->keys_examined++;

		// Check if this entry matches our search key
		if (entry->key_hash == impure->key_hash &&
			entry->key_length == impure->search_key->key_length &&
			compareKeys(entry->key_data, entry->key_length,
					   impure->search_key->key_data, impure->search_key->key_length))
		{
			// Found matching key - fetch the record
			return fetchRecordByNumber(tdbb, entry->record_number);
		}
		else if (entry->key_hash == impure->key_hash)
		{
			// Hash collision
			impure->collisions_encountered++;
		}
	}

	// No more matches found
	return false;
}

bool HashTableScan::retrieveNextInScan(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	// Continue from current position
	while (impure->current_bucket < impure->bucket_count)
	{
		// Initialize bucket iterator if needed
		if (!impure->bucket_iterator)
		{
			if (!initializeBucketIterator(tdbb, impure->current_bucket))
			{
				moveToNextBucket(tdbb);
				continue;
			}
		}

		// Get next entry from current bucket
		if (impure->bucket_iterator->hasNext())
		{
			const HashBucketEntry* entry = impure->bucket_iterator->next();
			if (entry && entry->isValid())
			{
				impure->keys_examined++;
				return fetchRecordByNumber(tdbb, entry->record_number);
			}
		}
		else
		{
			// Move to next bucket
			moveToNextBucket(tdbb);
		}
	}

	// End of scan
	impure->irsb_flags |= irsb_eof;
	return false;
}

bool HashTableScan::retrievePreviousInScan(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	// Similar to retrieveNextInScan but in reverse direction
	// This is a simplified implementation
	return false; // TODO: Implement backward scanning
}

//----------------------------
// Bucket Navigation Methods
//----------------------------

bool HashTableScan::positionAtBucket(thread_db* tdbb, ULONG bucket_number) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	if (bucket_number >= impure->bucket_count)
		return false;

	impure->current_bucket = bucket_number;
	
	// Cleanup old iterator
	if (impure->bucket_iterator)
	{
		delete impure->bucket_iterator;
		impure->bucket_iterator = nullptr;
	}

	return true;
}

bool HashTableScan::moveToNextBucket(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	// Cleanup current iterator
	if (impure->bucket_iterator)
	{
		delete impure->bucket_iterator;
		impure->bucket_iterator = nullptr;
	}

	impure->current_bucket++;
	impure->buckets_scanned++;

	return impure->current_bucket < impure->bucket_count;
}

bool HashTableScan::initializeBucketIterator(thread_db* tdbb, ULONG bucket_number) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);

	// TODO: Get bucket from hash index and create iterator
	// This would require integration with the actual hash index implementation
	// For now, return false to indicate no iterator available
	return false;
}

//----------------------------
// Utility Methods
//----------------------------

ULONG HashTableScan::calculateHash(const UCHAR* key_data, USHORT key_length) const
{
	// Use the hash index's hash function
	return m_hash_index->hash(key_data, key_length);
}

bool HashTableScan::compareKeys(const UCHAR* key1, USHORT len1, 
								const UCHAR* key2, USHORT len2) const
{
	if (len1 != len2)
		return false;
	
	return memcmp(key1, key2, len1) == 0;
}

bool HashTableScan::fetchRecordByNumber(thread_db* tdbb, RecordNumber record_number) const
{
	Request* const request = tdbb->getRequest();
	record_param* const rpb = &request->req_rpb[m_stream];

	rpb->rpb_number = record_number;
	
	// Use standard VIO mechanism to fetch the record
	return VIO_get(tdbb, rpb, request->req_transaction, tdbb->getDefaultPool());
}

//----------------------------
// Plan and Statistics Methods
//----------------------------

void HashTableScan::getLegacyPlan(string& plan, unsigned level) const
{
	plan += printIndent(++level) + "Hash Index Scan";
	if (!m_alias.isEmpty())
		plan += " on " + m_alias;
}

void HashTableScan::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, 
									unsigned level, bool recurse) const
{
	planEntry.className = "HashTableScan";
	planEntry.lines = 1;
	planEntry.pixelsWidth = static_cast<int>(planEntry.className.length() * PLAN_CHAR_WIDTH + 0.5);
	
	if (!m_alias.isEmpty())
	{
		planEntry.description = "Hash scan on " + m_alias;
	}
	else
	{
		planEntry.description = "Hash index scan";
	}
}

void HashTableScan::setInversion(InversionNode* inversion)
{
	m_inversion = inversion;
}

//----------------------------
// Statistics Methods
//----------------------------

ULONG HashTableScan::getBucketsScanned(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	const Impure* const impure = request->getImpure<Impure>(m_impure);
	return impure->buckets_scanned;
}

ULONG HashTableScan::getCollisionsEncountered(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	const Impure* const impure = request->getImpure<Impure>(m_impure);
	return impure->collisions_encountered;
}

ULONG HashTableScan::getKeysExamined(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	const Impure* const impure = request->getImpure<Impure>(m_impure);
	return impure->keys_examined;
}

double HashTableScan::getEfficiency(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	const Impure* const impure = request->getImpure<Impure>(m_impure);

	if (impure->keys_examined == 0)
		return 1.0; // Perfect efficiency when no keys examined yet

	// Calculate efficiency as (useful lookups / total lookups)
	ULONG useful_lookups = impure->keys_examined - impure->collisions_encountered;
	return static_cast<double>(useful_lookups) / static_cast<double>(impure->keys_examined);
}