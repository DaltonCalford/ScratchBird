/*
 *	PROGRAM:	ScratchBird Hash Index Performance Benchmarks
 *	MODULE:		hash_index_performance_benchmarks.cpp
 *	DESCRIPTION:	Performance testing and validation for hash indexes
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * All Rights Reserved.
 * 2025.07.22 - ScratchBird Hash Index Performance Benchmarks
 */

#include "scratchbird.h"
#include "boost/test/unit_test.hpp"
#include "../jrd/HashIndex.h"
#include "../jrd/IndexTypeRegistry.h"
#include "../jrd/constants.h"
#include "../jrd/btr.h"
#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../common/gdsassert.h"
#include <chrono>
#include <random>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <iostream>

using namespace ScratchBird;
using namespace Jrd;
using namespace std::chrono;

BOOST_AUTO_TEST_SUITE(HashIndexPerformanceSuite)

namespace {
	// Performance test configuration
	const int SMALL_DATASET = 1000;
	const int MEDIUM_DATASET = 10000; 
	const int LARGE_DATASET = 100000;
	const int ITERATIONS = 5;  // Number of test iterations for averaging
	
	// Test data generators
	class TestDataGenerator 
	{
	public:
		static std::vector<std::string> generateUniqueKeys(int count) 
		{
			std::vector<std::string> keys;
			keys.reserve(count);
			
			for (int i = 0; i < count; i++) {
				char buffer[64];
				sprintf(buffer, "key_%08d_%04x", i, rand() & 0xFFFF);
				keys.emplace_back(buffer);
			}
			
			return keys;
		}
		
		static std::vector<std::string> generateRandomKeys(int count) 
		{
			std::vector<std::string> keys;
			keys.reserve(count);
			
			std::mt19937 gen(12345);  // Fixed seed for reproducible results
			std::uniform_int_distribution<> dis(0, 61);  // a-z, A-Z, 0-9
			const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
			
			for (int i = 0; i < count; i++) {
				std::string key;
				int keyLen = 8 + (gen() % 24); // 8-32 character keys
				key.reserve(keyLen);
				
				for (int j = 0; j < keyLen; j++) {
					key += chars[dis(gen)];
				}
				keys.push_back(key);
			}
			
			return keys;
		}
		
		static std::vector<RecordNumber> generateRecordNumbers(int count) 
		{
			std::vector<RecordNumber> records;
			records.reserve(count);
			
			for (int i = 0; i < count; i++) {
				records.emplace_back(i + 1);  // Record numbers start from 1
			}
			
			return records;
		}
	};
	
	// Mock classes for testing
	class MockThreadDb : public thread_db 
	{
	public:
		MockThreadDb() = default;
	};
	
	class MockDatabase : public Database 
	{
	public:
		MockDatabase() = default;
	};
	
	class MockRelation : public jrd_rel 
	{
	public:
		MockRelation() {
			rel_id = 1;
		}
	};
	
	class MockTransaction : public jrd_tra 
	{
	public:
		MockTransaction() = default;
	};
}

BOOST_AUTO_TEST_SUITE(HashIndexPerformanceTests)

BOOST_AUTO_TEST_CASE(HashFunctionPerformanceTest)
{
	std::cout << "=== Hash Function Performance Test ===" << std::endl;
	
	auto keys = TestDataGenerator::generateRandomKeys(LARGE_DATASET);
	
	// Test CRC32 performance
	auto start = high_resolution_clock::now();
	
	for (int iter = 0; iter < ITERATIONS; iter++) {
		for (const auto& key : keys) {
			volatile ULONG hash = HashIndex::hashCRC32(
				reinterpret_cast<const UCHAR*>(key.c_str()), key.length());
			(void)hash; // Prevent optimization
		}
	}
	
	auto end = high_resolution_clock::now();
	auto crc32_time = duration_cast<microseconds>(end - start);
	
	// Test MurmurHash3 performance
	start = high_resolution_clock::now();
	
	for (int iter = 0; iter < ITERATIONS; iter++) {
		for (const auto& key : keys) {
			volatile ULONG hash = HashIndex::hashMurmurHash3(
				reinterpret_cast<const UCHAR*>(key.c_str()), key.length());
			(void)hash; // Prevent optimization
		}
	}
	
	end = high_resolution_clock::now();
	auto murmur_time = duration_cast<microseconds>(end - start);
	
	std::cout << "CRC32 Hash: " << crc32_time.count() << " μs for " 
			  << (LARGE_DATASET * ITERATIONS) << " keys" << std::endl;
	std::cout << "MurmurHash3: " << murmur_time.count() << " μs for " 
			  << (LARGE_DATASET * ITERATIONS) << " keys" << std::endl;
	
	double crc32_rate = (LARGE_DATASET * ITERATIONS * 1000000.0) / crc32_time.count();
	double murmur_rate = (LARGE_DATASET * ITERATIONS * 1000000.0) / murmur_time.count();
	
	std::cout << "CRC32 Rate: " << static_cast<int>(crc32_rate) << " hashes/second" << std::endl;
	std::cout << "MurmurHash3 Rate: " << static_cast<int>(murmur_rate) << " hashes/second" << std::endl;
	
	// Both should be fast enough (> 1M hashes/second)
	BOOST_TEST(crc32_rate > 1000000);
	BOOST_TEST(murmur_rate > 1000000);
}

BOOST_AUTO_TEST_CASE(HashIndexInsertionPerformanceTest)
{
	std::cout << "=== Hash Index Insertion Performance Test ===" << std::endl;
	
	MockThreadDb tdbb;
	MockDatabase database;
	MockRelation relation;
	
	// Create index descriptor for hash index
	index_desc desc;
	desc.idx_type = IDX_TYPE_HASH;
	desc.idx_flags = 0;
	desc.idx_count = 1;
	desc.idx_root = 0;
	desc.idx_selectivity = 1.0f;
	desc.idx_id = 1;
	
	std::vector<std::pair<int, double>> results; // dataset_size, ops_per_second
	
	for (int dataset_size : {SMALL_DATASET, MEDIUM_DATASET, LARGE_DATASET}) {
		auto keys = TestDataGenerator::generateUniqueKeys(dataset_size);
		auto records = TestDataGenerator::generateRecordNumbers(dataset_size);
		
		double total_time = 0.0;
		
		for (int iter = 0; iter < ITERATIONS; iter++) {
			HashIndex hashIndex(&tdbb, &database, &relation, &desc);
			
			auto start = high_resolution_clock::now();
			
			// Insert all keys
			for (int i = 0; i < dataset_size; i++) {
				dsc key_desc;
				key_desc.dsc_address = (UCHAR*)keys[i].c_str();
				key_desc.dsc_length = keys[i].length();
				key_desc.dsc_dtype = dtype_text;
				key_desc.dsc_scale = 0;
				key_desc.dsc_sub_type = 0;
				
				index_error_t result = hashIndex.insert(&tdbb, &key_desc, records[i], nullptr);
				BOOST_TEST(result == idx_e_ok);
			}
			
			auto end = high_resolution_clock::now();
			total_time += duration_cast<microseconds>(end - start).count();
		}
		
		double avg_time = total_time / ITERATIONS;
		double ops_per_second = (dataset_size * 1000000.0) / avg_time;
		
		results.emplace_back(dataset_size, ops_per_second);
		
		std::cout << "Dataset Size: " << dataset_size << ", Insertion Rate: " 
				  << static_cast<int>(ops_per_second) << " ops/second" << std::endl;
	}
	
	// Performance should scale reasonably (not degrade too much with size)
	BOOST_TEST(results[0].second > 50000);  // Small dataset: > 50K ops/sec
	BOOST_TEST(results[1].second > 20000);  // Medium dataset: > 20K ops/sec  
	BOOST_TEST(results[2].second > 5000);   // Large dataset: > 5K ops/sec
}

BOOST_AUTO_TEST_CASE(HashIndexLookupPerformanceTest)
{
	std::cout << "=== Hash Index Lookup Performance Test ===" << std::endl;
	
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
	
	const int dataset_size = MEDIUM_DATASET;
	auto keys = TestDataGenerator::generateUniqueKeys(dataset_size);
	auto records = TestDataGenerator::generateRecordNumbers(dataset_size);
	
	// Pre-populate hash index
	HashIndex hashIndex(&tdbb, &database, &relation, &desc);
	
	for (int i = 0; i < dataset_size; i++) {
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)keys[i].c_str();
		key_desc.dsc_length = keys[i].length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		hashIndex.insert(&tdbb, &key_desc, records[i], nullptr);
	}
	
	// Test lookup performance
	double total_time = 0.0;
	int successful_lookups = 0;
	
	for (int iter = 0; iter < ITERATIONS; iter++) {
		auto start = high_resolution_clock::now();
		
		// Lookup all keys
		for (int i = 0; i < dataset_size; i++) {
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
		
		auto end = high_resolution_clock::now();
		total_time += duration_cast<microseconds>(end - start).count();
	}
	
	double avg_time = total_time / ITERATIONS;
	double ops_per_second = (dataset_size * 1000000.0) / avg_time;
	double success_rate = (double)successful_lookups / (dataset_size * ITERATIONS);
	
	std::cout << "Lookup Rate: " << static_cast<int>(ops_per_second) << " ops/second" << std::endl;
	std::cout << "Success Rate: " << (success_rate * 100) << "%" << std::endl;
	
	// Hash indexes should provide fast lookups
	BOOST_TEST(ops_per_second > 100000);  // > 100K lookups/second
	BOOST_TEST(success_rate > 0.99);      // > 99% success rate
}

BOOST_AUTO_TEST_CASE(HashIndexDeletePerformanceTest)
{
	std::cout << "=== Hash Index Delete Performance Test ===" << std::endl;
	
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
	
	const int dataset_size = MEDIUM_DATASET;
	auto keys = TestDataGenerator::generateUniqueKeys(dataset_size);
	auto records = TestDataGenerator::generateRecordNumbers(dataset_size);
	
	double total_time = 0.0;
	
	for (int iter = 0; iter < ITERATIONS; iter++) {
		HashIndex hashIndex(&tdbb, &database, &relation, &desc);
		
		// Pre-populate index
		for (int i = 0; i < dataset_size; i++) {
			dsc key_desc;
			key_desc.dsc_address = (UCHAR*)keys[i].c_str();
			key_desc.dsc_length = keys[i].length();
			key_desc.dsc_dtype = dtype_text;
			key_desc.dsc_scale = 0;
			key_desc.dsc_sub_type = 0;
			
			hashIndex.insert(&tdbb, &key_desc, records[i], nullptr);
		}
		
		// Test deletion performance
		auto start = high_resolution_clock::now();
		
		// Delete all keys
		for (int i = 0; i < dataset_size; i++) {
			dsc key_desc;
			key_desc.dsc_address = (UCHAR*)keys[i].c_str();
			key_desc.dsc_length = keys[i].length();
			key_desc.dsc_dtype = dtype_text;
			key_desc.dsc_scale = 0;
			key_desc.dsc_sub_type = 0;
			
			index_error_t result = hashIndex.remove(&tdbb, &key_desc, records[i], nullptr);
			BOOST_TEST(result == idx_e_ok);
		}
		
		auto end = high_resolution_clock::now();
		total_time += duration_cast<microseconds>(end - start).count();
	}
	
	double avg_time = total_time / ITERATIONS;
	double ops_per_second = (dataset_size * 1000000.0) / avg_time;
	
	std::cout << "Delete Rate: " << static_cast<int>(ops_per_second) << " ops/second" << std::endl;
	
	// Delete performance should be reasonable
	BOOST_TEST(ops_per_second > 20000);  // > 20K deletes/second
}

BOOST_AUTO_TEST_CASE(HashIndexLoadFactorTest)
{
	std::cout << "=== Hash Index Load Factor Test ===" << std::endl;
	
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
	
	const int max_keys = 1000;
	auto keys = TestDataGenerator::generateUniqueKeys(max_keys);
	auto records = TestDataGenerator::generateRecordNumbers(max_keys);
	
	std::vector<std::pair<int, double>> load_factors; // key_count, load_factor
	
	// Insert keys and monitor load factor
	for (int i = 0; i < max_keys; i += 50) {
		// Insert batch of keys
		for (int j = 0; j < 50 && (i + j) < max_keys; j++) {
			dsc key_desc;
			key_desc.dsc_address = (UCHAR*)keys[i + j].c_str();
			key_desc.dsc_length = keys[i + j].length();
			key_desc.dsc_dtype = dtype_text;
			key_desc.dsc_scale = 0;
			key_desc.dsc_sub_type = 0;
			
			hashIndex.insert(&tdbb, &key_desc, records[i + j], nullptr);
		}
		
		double load_factor = hashIndex.getLoadFactor();
		load_factors.emplace_back(i + 50, load_factor);
		
		std::cout << "Keys: " << (i + 50) << ", Load Factor: " << load_factor 
				  << ", Buckets: " << hashIndex.getBucketCount() << std::endl;
	}
	
	// Load factor should remain reasonable (hash table should expand)
	for (const auto& lf : load_factors) {
		BOOST_TEST(lf.second <= 1.0);  // Load factor should not exceed 100%
	}
}

BOOST_AUTO_TEST_SUITE_END() // HashIndexPerformanceTests

BOOST_AUTO_TEST_SUITE(HashIndexValidationTests)

BOOST_AUTO_TEST_CASE(HashIndexCorrectnessTest)
{
	std::cout << "=== Hash Index Correctness Test ===" << std::endl;
	
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
	
	const int test_size = 1000;
	auto keys = TestDataGenerator::generateUniqueKeys(test_size);
	auto records = TestDataGenerator::generateRecordNumbers(test_size);
	
	// Insert all keys
	for (int i = 0; i < test_size; i++) {
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)keys[i].c_str();
		key_desc.dsc_length = keys[i].length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		index_error_t result = hashIndex.insert(&tdbb, &key_desc, records[i], nullptr);
		BOOST_TEST(result == idx_e_ok);
	}
	
	// Verify all keys can be found
	int found_count = 0;
	for (int i = 0; i < test_size; i++) {
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)keys[i].c_str();
		key_desc.dsc_length = keys[i].length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		IndexRetrieval retrieval;
		if (hashIndex.lookup(&tdbb, &key_desc, &retrieval)) {
			found_count++;
		}
	}
	
	std::cout << "Inserted: " << test_size << ", Found: " << found_count << std::endl;
	BOOST_TEST(found_count == test_size);
	
	// Delete half the keys
	int delete_count = test_size / 2;
	for (int i = 0; i < delete_count; i++) {
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)keys[i].c_str();
		key_desc.dsc_length = keys[i].length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		index_error_t result = hashIndex.remove(&tdbb, &key_desc, records[i], nullptr);
		BOOST_TEST(result == idx_e_ok);
	}
	
	// Verify deleted keys are not found and remaining keys are still found
	int deleted_not_found = 0;
	int remaining_found = 0;
	
	for (int i = 0; i < test_size; i++) {
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)keys[i].c_str();
		key_desc.dsc_length = keys[i].length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		IndexRetrieval retrieval;
		bool found = hashIndex.lookup(&tdbb, &key_desc, &retrieval);
		
		if (i < delete_count) {
			// Should be deleted
			if (!found) deleted_not_found++;
		} else {
			// Should still exist  
			if (found) remaining_found++;
		}
	}
	
	std::cout << "Deleted keys not found: " << deleted_not_found << "/" << delete_count << std::endl;
	std::cout << "Remaining keys found: " << remaining_found << "/" << (test_size - delete_count) << std::endl;
	
	BOOST_TEST(deleted_not_found == delete_count);
	BOOST_TEST(remaining_found == (test_size - delete_count));
}

BOOST_AUTO_TEST_CASE(HashIndexCollisionHandlingTest)
{
	std::cout << "=== Hash Index Collision Handling Test ===" << std::endl;
	
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
	
	// Create keys that are likely to cause collisions
	std::vector<std::string> collision_keys;
	for (int i = 0; i < 100; i++) {
		char buffer[16];
		sprintf(buffer, "col_%d", i);
		collision_keys.emplace_back(buffer);
	}
	
	auto records = TestDataGenerator::generateRecordNumbers(collision_keys.size());
	
	// Insert all keys (some will collide)
	int successful_inserts = 0;
	for (size_t i = 0; i < collision_keys.size(); i++) {
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)collision_keys[i].c_str();
		key_desc.dsc_length = collision_keys[i].length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		index_error_t result = hashIndex.insert(&tdbb, &key_desc, records[i], nullptr);
		if (result == idx_e_ok) {
			successful_inserts++;
		}
	}
	
	// Verify all keys can be retrieved correctly despite collisions
	int successful_lookups = 0;
	for (size_t i = 0; i < collision_keys.size(); i++) {
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)collision_keys[i].c_str();
		key_desc.dsc_length = collision_keys[i].length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		IndexRetrieval retrieval;
		if (hashIndex.lookup(&tdbb, &key_desc, &retrieval)) {
			successful_lookups++;
		}
	}
	
	std::cout << "Successful inserts: " << successful_inserts << "/" << collision_keys.size() << std::endl;
	std::cout << "Successful lookups: " << successful_lookups << "/" << collision_keys.size() << std::endl;
	
	// All operations should succeed despite collisions
	BOOST_TEST(successful_inserts == static_cast<int>(collision_keys.size()));
	BOOST_TEST(successful_lookups == static_cast<int>(collision_keys.size()));
}

BOOST_AUTO_TEST_SUITE_END() // HashIndexValidationTests

BOOST_AUTO_TEST_SUITE_END() // HashIndexPerformanceSuite