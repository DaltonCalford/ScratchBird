/*
 *	PROGRAM:	ScratchBird Hash Index Validation Tests  
 *	MODULE:		hash_index_validation_tests.cpp
 *	DESCRIPTION:	Validation and correctness tests for hash indexes
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * All Rights Reserved.
 * 2025.07.22 - ScratchBird Hash Index Validation Tests
 */

#include "scratchbird.h" 
#include "boost/test/unit_test.hpp"
#include "../jrd/HashIndex.h"
#include "../jrd/IndexTypeRegistry.h"
#include "../jrd/constants.h"
#include "../jrd/btr.h"
#include "../common/gdsassert.h"
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>

using namespace ScratchBird;
using namespace Jrd;

BOOST_AUTO_TEST_SUITE(HashIndexValidationSuite)

namespace {
	// Mock classes for testing
	class MockThreadDb : public thread_db {
	public:
		MockThreadDb() = default;
	};
	
	class MockDatabase : public Database {
	public:
		MockDatabase() = default;
	};
	
	class MockRelation : public jrd_rel {
	public:
		MockRelation() {
			rel_id = 1;
		}
	};
}

BOOST_AUTO_TEST_SUITE(HashIndexBasicValidationTests)

BOOST_AUTO_TEST_CASE(HashIndexCreationTest)
{
	MockThreadDb tdbb;
	MockDatabase database;
	MockRelation relation;
	
	index_desc desc;
	desc.idx_type = IDX_TYPE_HASH;
	desc.idx_flags = 0;
	desc.idx_count = 1;
	desc.idx_root = 0;
	desc.idx_selectivity = 1.0f;
	desc.idx_id = 1;
	
	// Test hash index creation
	HashIndex hashIndex(&tdbb, &database, &relation, &desc);
	
	// Verify initial state
	BOOST_TEST(hashIndex.getBucketCount() == HASH_DEFAULT_BUCKETS);
	BOOST_TEST(hashIndex.getKeyCount() == 0);
	BOOST_TEST(hashIndex.getLoadFactor() == 0.0);
}

BOOST_AUTO_TEST_CASE(HashIndexInsertLookupTest) 
{
	MockThreadDb tdbb;
	MockDatabase database;
	MockRelation relation;
	
	index_desc desc;
	desc.idx_type = IDX_TYPE_HASH;
	desc.idx_flags = 0;
	desc.idx_count = 1;
	desc.idx_root = 0;
	desc.idx_selectivity = 1.0f;
	desc.idx_id = 1;
	
	HashIndex hashIndex(&tdbb, &database, &relation, &desc);
	
	// Test data
	const char* test_key = "test_key_123";
	RecordNumber test_record(42);
	
	// Create key descriptor
	dsc key_desc;
	key_desc.dsc_address = (UCHAR*)test_key;
	key_desc.dsc_length = strlen(test_key);
	key_desc.dsc_dtype = dtype_text;
	key_desc.dsc_scale = 0;
	key_desc.dsc_sub_type = 0;
	
	// Test insertion
	index_error_t insert_result = hashIndex.insert(&tdbb, &key_desc, test_record, nullptr);
	BOOST_TEST(insert_result == idx_e_ok);
	BOOST_TEST(hashIndex.getKeyCount() == 1);
	
	// Test lookup
	IndexRetrieval retrieval;
	bool lookup_result = hashIndex.lookup(&tdbb, &key_desc, &retrieval);
	BOOST_TEST(lookup_result == true);
}

BOOST_AUTO_TEST_CASE(HashIndexRemoveTest)
{
	MockThreadDb tdbb;
	MockDatabase database;
	MockRelation relation;
	
	index_desc desc;
	desc.idx_type = IDX_TYPE_HASH;
	desc.idx_flags = 0;
	desc.idx_count = 1;
	desc.idx_root = 0;
	desc.idx_selectivity = 1.0f;
	desc.idx_id = 1;
	
	HashIndex hashIndex(&tdbb, &database, &relation, &desc);
	
	// Insert test data
	const char* test_key = "remove_test_key";
	RecordNumber test_record(99);
	
	dsc key_desc;
	key_desc.dsc_address = (UCHAR*)test_key;
	key_desc.dsc_length = strlen(test_key);
	key_desc.dsc_dtype = dtype_text;
	key_desc.dsc_scale = 0;
	key_desc.dsc_sub_type = 0;
	
	// Insert key
	index_error_t insert_result = hashIndex.insert(&tdbb, &key_desc, test_record, nullptr);
	BOOST_TEST(insert_result == idx_e_ok);
	BOOST_TEST(hashIndex.getKeyCount() == 1);
	
	// Verify key exists
	IndexRetrieval retrieval;
	bool lookup_result = hashIndex.lookup(&tdbb, &key_desc, &retrieval);
	BOOST_TEST(lookup_result == true);
	
	// Remove key
	index_error_t remove_result = hashIndex.remove(&tdbb, &key_desc, test_record, nullptr);
	BOOST_TEST(remove_result == idx_e_ok);
	BOOST_TEST(hashIndex.getKeyCount() == 0);
	
	// Verify key no longer exists
	lookup_result = hashIndex.lookup(&tdbb, &key_desc, &retrieval);
	BOOST_TEST(lookup_result == false);
}

BOOST_AUTO_TEST_CASE(HashIndexMultipleKeysTest)
{
	MockThreadDb tdbb;
	MockDatabase database;
	MockRelation relation;
	
	index_desc desc;
	desc.idx_type = IDX_TYPE_HASH;
	desc.idx_flags = 0;
	desc.idx_count = 1;
	desc.idx_root = 0;
	desc.idx_selectivity = 1.0f;
	desc.idx_id = 1;
	
	HashIndex hashIndex(&tdbb, &database, &relation, &desc);
	
	// Test with multiple keys
	const int num_keys = 50;
	std::vector<std::string> keys;
	std::vector<RecordNumber> records;
	
	// Generate test data
	for (int i = 0; i < num_keys; i++) {
		char buffer[32];
		sprintf(buffer, "multi_key_%d", i);
		keys.emplace_back(buffer);
		records.emplace_back(i + 100);
	}
	
	// Insert all keys
	int successful_inserts = 0;
	for (int i = 0; i < num_keys; i++) {
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)keys[i].c_str();
		key_desc.dsc_length = keys[i].length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		index_error_t result = hashIndex.insert(&tdbb, &key_desc, records[i], nullptr);
		if (result == idx_e_ok) {
			successful_inserts++;
		}
	}
	
	BOOST_TEST(successful_inserts == num_keys);
	BOOST_TEST(hashIndex.getKeyCount() == static_cast<ULONG>(num_keys));
	
	// Verify all keys can be found
	int successful_lookups = 0;
	for (int i = 0; i < num_keys; i++) {
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)keys[i].c_str();
		key_desc.dsc_length = keys[i].length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		IndexRetrieval retrieval;
		if (hashIndex.lookup(&tdbb, &key_desc, &retrieval)) {
			successful_lookups++;
		}
	}
	
	BOOST_TEST(successful_lookups == num_keys);
}

BOOST_AUTO_TEST_CASE(HashIndexDuplicateKeyTest)
{
	MockThreadDb tdbb;
	MockDatabase database; 
	MockRelation relation;
	
	index_desc desc;
	desc.idx_type = IDX_TYPE_HASH;
	desc.idx_flags = 0; // Not unique
	desc.idx_count = 1;
	desc.idx_root = 0;
	desc.idx_selectivity = 1.0f;
	desc.idx_id = 1;
	
	HashIndex hashIndex(&tdbb, &database, &relation, &desc);
	
	// Test inserting same key with different records
	const char* test_key = "duplicate_key";
	RecordNumber record1(100);
	RecordNumber record2(200);
	
	dsc key_desc;
	key_desc.dsc_address = (UCHAR*)test_key;
	key_desc.dsc_length = strlen(test_key);
	key_desc.dsc_dtype = dtype_text;
	key_desc.dsc_scale = 0;
	key_desc.dsc_sub_type = 0;
	
	// Insert first key-record pair
	index_error_t result1 = hashIndex.insert(&tdbb, &key_desc, record1, nullptr);
	BOOST_TEST(result1 == idx_e_ok);
	
	// Insert same key with different record
	index_error_t result2 = hashIndex.insert(&tdbb, &key_desc, record2, nullptr);
	BOOST_TEST(result2 == idx_e_ok); // Should allow duplicates for non-unique index
	
	// Key should be found
	IndexRetrieval retrieval;
	bool lookup_result = hashIndex.lookup(&tdbb, &key_desc, &retrieval);
	BOOST_TEST(lookup_result == true);
}

BOOST_AUTO_TEST_CASE(HashIndexExpandShrinkTest)
{
	MockThreadDb tdbb;
	MockDatabase database;
	MockRelation relation;
	
	index_desc desc;
	desc.idx_type = IDX_TYPE_HASH;
	desc.idx_flags = 0;
	desc.idx_count = 1;
	desc.idx_root = 0;
	desc.idx_selectivity = 1.0f;
	desc.idx_id = 1;
	
	HashIndex hashIndex(&tdbb, &database, &relation, &desc);
	
	ULONG initial_buckets = hashIndex.getBucketCount();
	
	// Test expansion
	bool expand_result = hashIndex.expand(&tdbb);
	BOOST_TEST(expand_result == true);
	BOOST_TEST(hashIndex.getBucketCount() == (initial_buckets * 2));
	
	// Test shrinking
	bool shrink_result = hashIndex.shrink(&tdbb);
	BOOST_TEST(shrink_result == true);
	BOOST_TEST(hashIndex.getBucketCount() == initial_buckets);
	
	// Test that we can't shrink below minimum
	while (hashIndex.getBucketCount() > HASH_DEFAULT_BUCKETS) {
		hashIndex.shrink(&tdbb);
	}
	
	ULONG min_buckets = hashIndex.getBucketCount();
	hashIndex.shrink(&tdbb);
	BOOST_TEST(hashIndex.getBucketCount() == min_buckets); // Shouldn't shrink further
}

BOOST_AUTO_TEST_SUITE_END() // HashIndexBasicValidationTests

BOOST_AUTO_TEST_SUITE(HashIndexStressTests)

BOOST_AUTO_TEST_CASE(HashIndexStressInsertTest)
{
	MockThreadDb tdbb;
	MockDatabase database;
	MockRelation relation;
	
	index_desc desc;
	desc.idx_type = IDX_TYPE_HASH;
	desc.idx_flags = 0;
	desc.idx_count = 1;
	desc.idx_root = 0;
	desc.idx_selectivity = 1.0f;
	desc.idx_id = 1;
	
	HashIndex hashIndex(&tdbb, &database, &relation, &desc);
	
	// Stress test with many inserts
	const int stress_count = 1000;
	int successful_operations = 0;
	
	// Insert many keys to test bucket expansion
	for (int i = 0; i < stress_count; i++) {
		char key_buffer[64];
		sprintf(key_buffer, "stress_key_%06d", i);
		
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)key_buffer;
		key_desc.dsc_length = strlen(key_buffer);
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		RecordNumber record(i + 1000);
		
		index_error_t result = hashIndex.insert(&tdbb, &key_desc, record, nullptr);
		if (result == idx_e_ok) {
			successful_operations++;
		}
	}
	
	BOOST_TEST(successful_operations == stress_count);
	BOOST_TEST(hashIndex.getKeyCount() == static_cast<ULONG>(stress_count));
	
	// Verify load factor is reasonable (hash table should have expanded)
	double load_factor = hashIndex.getLoadFactor();
	BOOST_TEST(load_factor <= 1.0);  // Should not exceed 100%
	BOOST_TEST(load_factor >= 0.1);  // Should not be too low (indicating over-expansion)
}

BOOST_AUTO_TEST_CASE(HashIndexMixedOperationsTest)
{
	MockThreadDb tdbb;
	MockDatabase database;
	MockRelation relation;
	
	index_desc desc;
	desc.idx_type = IDX_TYPE_HASH;
	desc.idx_flags = 0;
	desc.idx_count = 1;
	desc.idx_root = 0;
	desc.idx_selectivity = 1.0f;
	desc.idx_id = 1;
	
	HashIndex hashIndex(&tdbb, &database, &relation, &desc);
	
	// Mixed operations: insert, lookup, delete in various patterns
	const int operation_count = 200;
	std::vector<std::string> active_keys;
	
	for (int i = 0; i < operation_count; i++) {
		char key_buffer[32];
		sprintf(key_buffer, "mixed_key_%d", i);
		
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)key_buffer;
		key_desc.dsc_length = strlen(key_buffer);
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		RecordNumber record(i + 2000);
		
		// Insert every key
		index_error_t insert_result = hashIndex.insert(&tdbb, &key_desc, record, nullptr);
		BOOST_TEST(insert_result == idx_e_ok);
		active_keys.emplace_back(key_buffer);
		
		// Every 10th operation, delete some older keys
		if (i % 10 == 9 && active_keys.size() > 5) {
			// Delete the oldest key
			std::string old_key = active_keys.front();
			active_keys.erase(active_keys.begin());
			
			dsc old_key_desc;
			old_key_desc.dsc_address = (UCHAR*)old_key.c_str();
			old_key_desc.dsc_length = old_key.length();
			old_key_desc.dsc_dtype = dtype_text;
			old_key_desc.dsc_scale = 0;
			old_key_desc.dsc_sub_type = 0;
			
			RecordNumber old_record(2000 + (i - active_keys.size()));
			index_error_t remove_result = hashIndex.remove(&tdbb, &old_key_desc, old_record, nullptr);
			BOOST_TEST(remove_result == idx_e_ok);
		}
		
		// Every 5th operation, do lookups on existing keys
		if (i % 5 == 4 && !active_keys.empty()) {
			for (const std::string& lookup_key : active_keys) {
				dsc lookup_desc;
				lookup_desc.dsc_address = (UCHAR*)lookup_key.c_str();
				lookup_desc.dsc_length = lookup_key.length();
				lookup_desc.dsc_dtype = dtype_text;
				lookup_desc.dsc_scale = 0;
				lookup_desc.dsc_sub_type = 0;
				
				IndexRetrieval retrieval;
				bool found = hashIndex.lookup(&tdbb, &lookup_desc, &retrieval);
				BOOST_TEST(found == true); // All active keys should be found
			}
		}
	}
	
	// Final verification: all remaining active keys should be findable
	int final_found_count = 0;
	for (const std::string& key : active_keys) {
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)key.c_str();
		key_desc.dsc_length = key.length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		IndexRetrieval retrieval;
		if (hashIndex.lookup(&tdbb, &key_desc, &retrieval)) {
			final_found_count++;
		}
	}
	
	BOOST_TEST(final_found_count == static_cast<int>(active_keys.size()));
}

BOOST_AUTO_TEST_SUITE_END() // HashIndexStressTests

BOOST_AUTO_TEST_SUITE_END() // HashIndexValidationSuite