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

#ifndef JRD_HASH_TABLE_SCAN_H
#define JRD_HASH_TABLE_SCAN_H

#include "RecordSource.h"
#include "../HashIndex.h"
#include "../HashBucketManager.h"
#include "../constants.h"
#include "../common/classes/array.h"

namespace Jrd {

// Forward declarations
class InversionNode;
class HashIndex;
struct index_desc;
struct temporary_key;

//----------------------------
// HashRetrievalState - State for hash index retrieval
//----------------------------
enum HashRetrievalState
{
	HASH_RETRIEVAL_NONE = 0,		// No retrieval active
	HASH_RETRIEVAL_EQUALITY = 1,	// Equality lookup (single key)
	HASH_RETRIEVAL_SET = 2,			// Set lookup (multiple keys)
	HASH_RETRIEVAL_SCAN = 3			// Full scan of hash index
};

//----------------------------
// HashScanDirection - Direction for hash index scans
//----------------------------
enum HashScanDirection
{
	HASH_SCAN_FORWARD = 0,			// Forward scan (normal order)
	HASH_SCAN_BACKWARD = 1			// Backward scan (reverse order)
};

//----------------------------
// HashTableScan - Record source for hash index access
//----------------------------
class HashTableScan final : public RecordStream
{
public:
	// Impure area for runtime state
	struct Impure
	{
		ULONG irsb_flags;					// Control flags
		HashRetrievalState retrieval_state;	// Current retrieval state
		HashScanDirection scan_direction;	// Scan direction
		ULONG current_bucket;				// Current bucket being scanned
		ULONG bucket_count;					// Total number of buckets
		RecordNumber current_record;		// Current record number
		temporary_key* search_key;			// Key being searched for
		HashBucketIterator* bucket_iterator; // Iterator for current bucket
		
		// Hash-specific navigation
		UCHAR* key_data;					// Current key data buffer
		USHORT key_length;					// Length of current key
		ULONG key_hash;						// Hash value of current key
		bool key_found;						// Whether key was found
		
		// Statistics
		ULONG buckets_scanned;				// Number of buckets examined
		ULONG collisions_encountered;		// Number of collisions encountered
		ULONG keys_examined;				// Number of keys examined
	};

	// Control flags
	static const ULONG irsb_first			= 0x01;	// First time flag
	static const ULONG irsb_open			= 0x02;	// Stream is open
	static const ULONG irsb_mustread		= 0x04;	// Must read record
	static const ULONG irsb_positioned		= 0x08;	// Stream is positioned
	static const ULONG irsb_crack			= 0x10;	// Crack is required
	static const ULONG irsb_eof				= 0x20; // End of file reached
	static const ULONG irsb_bof				= 0x40; // Beginning of file
	static const ULONG irsb_equality		= 0x80; // Equality lookup mode

public:
	// Constructor
	HashTableScan(CompilerScratch* csb, const ScratchBird::string& alias,
				  StreamType stream, jrd_rel* relation, 
				  InversionNode* index, HashIndex* hash_index,
				  USHORT key_length, double selectivity);

	// Destructor
	virtual ~HashTableScan();

public:
	// RecordSource interface implementation
	void open(thread_db* tdbb) const override;
	void close(thread_db* tdbb) const override;
	bool getRecord(thread_db* tdbb) const override;
	bool refetchRecord(thread_db* tdbb) const override;
	bool lockRecord(thread_db* tdbb) const override;

	void markRecursive() override;
	void invalidateRecords(Request* request) const override;

	void getLegacyPlan(ScratchBird::string& plan, unsigned level) const override;
	void internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned level, bool recurse) const override;

	void setInversion(InversionNode* inversion) override;

	// Hash-specific methods
	bool seekEqualKey(thread_db* tdbb, temporary_key* key) const;
	bool seekFirstKey(thread_db* tdbb) const;
	bool seekLastKey(thread_db* tdbb) const;
	bool seekNextKey(thread_db* tdbb) const;
	bool seekPriorKey(thread_db* tdbb) const;
	
	// Hash index navigation
	bool positionAtHash(thread_db* tdbb, ULONG hash_value) const;
	bool positionAtBucket(thread_db* tdbb, ULONG bucket_number) const;
	bool positionAtKey(thread_db* tdbb, const UCHAR* key_data, USHORT key_length) const;

	// Iterator interface
	bool hasMoreRecords(thread_db* tdbb) const;
	RecordNumber getCurrentRecordNumber() const;
	bool getCurrentKey(UCHAR* buffer, USHORT* length) const;

	// Statistics and introspection
	ULONG getBucketsScanned(thread_db* tdbb) const;
	ULONG getCollisionsEncountered(thread_db* tdbb) const;
	ULONG getKeysExamined(thread_db* tdbb) const;
	double getEfficiency(thread_db* tdbb) const;

private:
	// Internal implementation methods
	void internalOpen(thread_db* tdbb) const;
	bool internalGetRecord(thread_db* tdbb) const;
	void initializeRetrieval(thread_db* tdbb, HashRetrievalState state) const;
	void setupEqualityLookup(thread_db* tdbb, temporary_key* key) const;
	void setupFullScan(thread_db* tdbb, HashScanDirection direction) const;

	// Hash-specific retrieval methods
	bool retrieveEqualityMatch(thread_db* tdbb) const;
	bool retrieveNextInScan(thread_db* tdbb) const;
	bool retrievePreviousInScan(thread_db* tdbb) const;

	// Bucket navigation
	bool advanceToBucket(thread_db* tdbb, ULONG bucket_number) const;
	bool moveToNextBucket(thread_db* tdbb) const;
	bool moveToPreviousBucket(thread_db* tdbb) const;
	bool initializeBucketIterator(thread_db* tdbb, ULONG bucket_number) const;

	// Key and record handling
	bool processCurrentKey(thread_db* tdbb) const;
	bool fetchRecordByNumber(thread_db* tdbb, RecordNumber record_number) const;
	bool validateKeyMatch(thread_db* tdbb, const UCHAR* key_data, USHORT key_length) const;

	// Utility methods
	ULONG calculateHash(const UCHAR* key_data, USHORT key_length) const;
	bool compareKeys(const UCHAR* key1, USHORT len1, const UCHAR* key2, USHORT len2) const;
	void copyKeyData(thread_db* tdbb, const UCHAR* source, USHORT length) const;
	void updateStatistics(thread_db* tdbb) const;

private:
	// Member variables
	ScratchBird::string m_alias;		// Table alias
	jrd_rel* m_relation;				// Relation being scanned
	InversionNode* m_index;				// Index inversion node
	HashIndex* m_hash_index;			// Hash index implementation
	InversionNode* m_inversion;			// Inversion for this scan
	BoolExprNode* m_condition;			// Optional scan condition
	USHORT m_key_length;				// Maximum key length
	ULONG m_impure_offset;				// Offset to impure area
	HashBucketManager m_bucket_manager;	// Bucket management helper
	
	// Configuration
	bool m_allow_duplicates;			// Allow duplicate keys
	ULONG m_max_buckets_to_scan;		// Maximum buckets to scan in one operation
	double m_selectivity;				// Expected selectivity of this scan
	
	// Optimization hints
	bool m_prefer_equality;				// Optimize for equality lookups
	bool m_sequential_hint;				// Hint for sequential access pattern
};

} // namespace Jrd

#endif // JRD_HASH_TABLE_SCAN_H