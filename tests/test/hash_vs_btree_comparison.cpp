/*
 *	PROGRAM:	ScratchBird Hash vs B-Tree Index Comparison
 *	MODULE:		hash_vs_btree_comparison.cpp
 *	DESCRIPTION:	Performance comparison between hash and B-tree indexes
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * All Rights Reserved.
 * 2025.07.22 - ScratchBird Hash vs B-Tree Comparison Tests
 */

#include "scratchbird.h"
#include "boost/test/unit_test.hpp"
#include "../jrd/HashIndex.h"
#include "../jrd/IndexTypeRegistry.h"
#include "../jrd/constants.h"
#include "../jrd/btr.h"
#include "../common/gdsassert.h"
#include <chrono>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <iostream>
#include <iomanip>

using namespace ScratchBird;
using namespace Jrd;
using namespace std::chrono;

BOOST_AUTO_TEST_SUITE(HashVsBTreeComparisonSuite)

namespace {
	// Performance test configuration
	const int COMPARISON_DATASET_SIZE = 5000;
	const int COMPARISON_ITERATIONS = 3;
	
	// Test data generator
	class ComparisonDataGenerator {
	public:
		static std::vector<std::string> generateSequentialKeys(int count) {
			std::vector<std::string> keys;
			keys.reserve(count);
			
			for (int i = 0; i < count; i++) {
				char buffer[32];
				sprintf(buffer, "seq_key_%08d", i);
				keys.emplace_back(buffer);
			}
			
			return keys;
		}
		
		static std::vector<std::string> generateRandomKeys(int count, int seed = 12345) {
			std::vector<std::string> keys;
			keys.reserve(count);
			
			std::mt19937 gen(seed);
			std::uniform_int_distribution<> dis(0, 61);
			const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
			
			for (int i = 0; i < count; i++) {
				std::string key;
				int keyLen = 8 + (gen() % 16); // 8-24 character keys
				key.reserve(keyLen);
				
				for (int j = 0; j < keyLen; j++) {
					key += chars[dis(gen)];
				}
				keys.push_back(key);
			}
			
			return keys;
		}
		
		static std::vector<RecordNumber> generateRecordNumbers(int count) {
			std::vector<RecordNumber> records;
			records.reserve(count);
			
			for (int i = 0; i < count; i++) {
				records.emplace_back(i + 1);
			}
			
			return records;
		}
	};
	
	// Performance measurement helper
	class PerformanceMeasurer {
	public:
		struct Result {
			double avg_time_us;
			double ops_per_second;
			double min_time_us;
			double max_time_us;
		};
		
		static Result measureOperations(std::function<void()> operation, int iterations) {
			std::vector<double> times;
			times.reserve(iterations);
			
			for (int i = 0; i < iterations; i++) {
				auto start = high_resolution_clock::now();
				operation();
				auto end = high_resolution_clock::now();
				
				double time_us = duration_cast<microseconds>(end - start).count();
				times.push_back(time_us);
			}
			
			Result result;
			result.avg_time_us = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
			result.min_time_us = *std::min_element(times.begin(), times.end());
			result.max_time_us = *std::max_element(times.begin(), times.end());
			result.ops_per_second = 1000000.0 / result.avg_time_us;
			
			return result;
		}
	};
	
	// Mock classes
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

BOOST_AUTO_TEST_SUITE(HashVsBTreeComparisonTests)

BOOST_AUTO_TEST_CASE(EqualityLookupPerformanceComparison)
{
	std::cout << "\\n=== Equality Lookup Performance Comparison ===" << std::endl;
	std::cout << std::fixed << std::setprecision(2);
	
	MockThreadDb tdbb;
	MockDatabase database;
	MockRelation relation;
	
	// Create hash index descriptor
	index_desc hash_desc;
	hash_desc.idx_type = IDX_TYPE_HASH;
	hash_desc.idx_flags = 0;
	hash_desc.idx_count = 1;
	hash_desc.idx_root = 0;
	hash_desc.idx_selectivity = 1.0f;
	hash_desc.idx_id = 1;
	
	// Create B-tree index descriptor  
	index_desc btree_desc;
	btree_desc.idx_type = IDX_TYPE_BTREE;
	btree_desc.idx_flags = 0;
	btree_desc.idx_count = 1;
	btree_desc.idx_root = 0;
	btree_desc.idx_selectivity = 1.0f;
	btree_desc.idx_id = 2;
	
	// Generate test data
	auto keys = ComparisonDataGenerator::generateRandomKeys(COMPARISON_DATASET_SIZE);
	auto records = ComparisonDataGenerator::generateRecordNumbers(COMPARISON_DATASET_SIZE);
	
	// Test with hash index
	HashIndex hashIndex(&tdbb, &database, &relation, &hash_desc);
	
	// Pre-populate hash index
	for (int i = 0; i < COMPARISON_DATASET_SIZE; i++) {
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)keys[i].c_str();
		key_desc.dsc_length = keys[i].length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		hashIndex.insert(&tdbb, &key_desc, records[i], nullptr);
	}
	
	// Measure hash index lookup performance
	auto hash_result = PerformanceMeasurer::measureOperations([&]() {
		for (int i = 0; i < COMPARISON_DATASET_SIZE; i++) {
			dsc key_desc;
			key_desc.dsc_address = (UCHAR*)keys[i].c_str();
			key_desc.dsc_length = keys[i].length();
			key_desc.dsc_dtype = dtype_text;
			key_desc.dsc_scale = 0;
			key_desc.dsc_sub_type = 0;
			
			IndexRetrieval retrieval;
			volatile bool found = hashIndex.lookup(&tdbb, &key_desc, &retrieval);
			(void)found; // Prevent optimization
		}
	}, COMPARISON_ITERATIONS);
	
	// Note: B-tree comparison would require a full B-tree implementation
	// For now, we'll report hash index performance and note B-tree expectations
	
	std::cout << "Hash Index Lookup Performance:" << std::endl;
	std::cout << "  Average time: " << (hash_result.avg_time_us / COMPARISON_DATASET_SIZE) << " μs per lookup" << std::endl;
	std::cout << "  Lookup rate: " << (hash_result.ops_per_second * COMPARISON_DATASET_SIZE) << " lookups/second" << std::endl;
	std::cout << "  Min time: " << (hash_result.min_time_us / COMPARISON_DATASET_SIZE) << " μs per lookup" << std::endl;
	std::cout << "  Max time: " << (hash_result.max_time_us / COMPARISON_DATASET_SIZE) << " μs per lookup" << std::endl;
	
	// Hash indexes should provide O(1) lookup performance
	double avg_lookup_time = hash_result.avg_time_us / COMPARISON_DATASET_SIZE;
	BOOST_TEST(avg_lookup_time < 10.0); // Should be very fast for equality lookups
	
	std::cout << "\\nExpected B-tree Performance:" << std::endl;
	std::cout << "  B-tree lookups: O(log n) = ~" << (int)(std::log2(COMPARISON_DATASET_SIZE)) << " comparisons" << std::endl;
	std::cout << "  Hash lookups: O(1) = ~1 hash + 1-2 comparisons" << std::endl;
	std::cout << "  Expected hash advantage: 3-5x faster for equality lookups" << std::endl;
}

BOOST_AUTO_TEST_CASE(InsertionPerformanceComparison)
{
	std::cout << "\\n=== Insertion Performance Comparison ===" << std::endl;
	
	MockThreadDb tdbb;
	MockDatabase database;
	MockRelation relation;
	
	index_desc hash_desc;
	hash_desc.idx_type = IDX_TYPE_HASH;
	hash_desc.idx_flags = 0;
	hash_desc.idx_count = 1;
	hash_desc.idx_root = 0;
	hash_desc.idx_selectivity = 1.0f;
	hash_desc.idx_id = 1;
	
	auto keys = ComparisonDataGenerator::generateRandomKeys(COMPARISON_DATASET_SIZE);
	auto records = ComparisonDataGenerator::generateRecordNumbers(COMPARISON_DATASET_SIZE);
	
	// Measure hash index insertion performance
	auto hash_result = PerformanceMeasurer::measureOperations([&]() {
		HashIndex hashIndex(&tdbb, &database, &relation, &hash_desc);
		
		for (int i = 0; i < COMPARISON_DATASET_SIZE; i++) {
			dsc key_desc;
			key_desc.dsc_address = (UCHAR*)keys[i].c_str();
			key_desc.dsc_length = keys[i].length();
			key_desc.dsc_dtype = dtype_text;
			key_desc.dsc_scale = 0;
			key_desc.dsc_sub_type = 0;
			
			volatile index_error_t result = hashIndex.insert(&tdbb, &key_desc, records[i], nullptr);
			(void)result; // Prevent optimization
		}
	}, COMPARISON_ITERATIONS);
	
	std::cout << "Hash Index Insertion Performance:" << std::endl;
	std::cout << "  Average time: " << (hash_result.avg_time_us / COMPARISON_DATASET_SIZE) << " μs per insert" << std::endl;
	std::cout << "  Insert rate: " << (hash_result.ops_per_second * COMPARISON_DATASET_SIZE) << " inserts/second" << std::endl;
	
	// Hash insertions should be reasonably fast
	double avg_insert_time = hash_result.avg_time_us / COMPARISON_DATASET_SIZE;
	BOOST_TEST(avg_insert_time < 50.0); // Should be fast for insertions
	
	std::cout << "\\nExpected B-tree Performance:" << std::endl;
	std::cout << "  B-tree inserts: O(log n) with potential page splits" << std::endl;
	std::cout << "  Hash inserts: O(1) with occasional bucket expansion" << std::endl;
	std::cout << "  Expected hash advantage: 2-3x faster for random insertions" << std::endl;
}

BOOST_AUTO_TEST_CASE(SequentialDataComparison)
{
	std::cout << "\\n=== Sequential Data Performance Comparison ===" << std::endl;
	
	MockThreadDb tdbb;
	MockDatabase database;
	MockRelation relation;
	
	index_desc hash_desc;
	hash_desc.idx_type = IDX_TYPE_HASH;
	hash_desc.idx_flags = 0;
	hash_desc.idx_count = 1;
	hash_desc.idx_root = 0;
	hash_desc.idx_selectivity = 1.0f;
	hash_desc.idx_id = 1;
	
	// Generate sequential keys (worst case for some hash functions)
	auto keys = ComparisonDataGenerator::generateSequentialKeys(COMPARISON_DATASET_SIZE);
	auto records = ComparisonDataGenerator::generateRecordNumbers(COMPARISON_DATASET_SIZE);
	
	HashIndex hashIndex(&tdbb, &database, &relation, &hash_desc);
	
	// Test insertion with sequential data
	auto start = high_resolution_clock::now();
	
	for (int i = 0; i < COMPARISON_DATASET_SIZE; i++) {
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
	auto insert_time = duration_cast<microseconds>(end - start);
	
	// Test lookup with sequential data
	start = high_resolution_clock::now();
	
	int found_count = 0;
	for (int i = 0; i < COMPARISON_DATASET_SIZE; i++) {
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
	
	end = high_resolution_clock::now();
	auto lookup_time = duration_cast<microseconds>(end - start);
	
	std::cout << "Sequential Data Results:" << std::endl;
	std::cout << "  Insert time: " << insert_time.count() << " μs total" << std::endl;
	std::cout << "  Lookup time: " << lookup_time.count() << " μs total" << std::endl;
	std::cout << "  Found keys: " << found_count << "/" << COMPARISON_DATASET_SIZE << std::endl;
	std::cout << "  Load factor: " << hashIndex.getLoadFactor() << std::endl;
	std::cout << "  Bucket count: " << hashIndex.getBucketCount() << std::endl;
	
	// Sequential data should still work well with hash indexes
	BOOST_TEST(found_count == COMPARISON_DATASET_SIZE);
	BOOST_TEST(hashIndex.getLoadFactor() <= 1.0);
	
	std::cout << "\\nNote: Hash indexes handle sequential data well due to good hash function distribution" << std::endl;
}

BOOST_AUTO_TEST_CASE(ScalabilityAnalysis)
{
	std::cout << "\\n=== Scalability Analysis ===" << std::endl;
	
	MockThreadDb tdbb;
	MockDatabase database;
	MockRelation relation;
	
	index_desc hash_desc;
	hash_desc.idx_type = IDX_TYPE_HASH;
	hash_desc.idx_flags = 0;
	hash_desc.idx_count = 1;
	hash_desc.idx_root = 0;
	hash_desc.idx_selectivity = 1.0f;
	hash_desc.idx_id = 1;
	
	std::vector<int> dataset_sizes = {1000, 2000, 4000, 8000};
	std::vector<double> lookup_times;
	
	for (int size : dataset_sizes) {
		auto keys = ComparisonDataGenerator::generateRandomKeys(size, 54321);
		auto records = ComparisonDataGenerator::generateRecordNumbers(size);
		
		HashIndex hashIndex(&tdbb, &database, &relation, &hash_desc);
		
		// Populate index
		for (int i = 0; i < size; i++) {
			dsc key_desc;
			key_desc.dsc_address = (UCHAR*)keys[i].c_str();
			key_desc.dsc_length = keys[i].length();
			key_desc.dsc_dtype = dtype_text;
			key_desc.dsc_scale = 0;
			key_desc.dsc_sub_type = 0;
			
			hashIndex.insert(&tdbb, &key_desc, records[i], nullptr);
		}
		
		// Measure lookup time for last 100 keys
		auto start = high_resolution_clock::now();
		
		for (int i = size - 100; i < size; i++) {
			dsc key_desc;
			key_desc.dsc_address = (UCHAR*)keys[i].c_str();
			key_desc.dsc_length = keys[i].length();
			key_desc.dsc_dtype = dtype_text;
			key_desc.dsc_scale = 0;
			key_desc.dsc_sub_type = 0;
			
			IndexRetrieval retrieval;
			volatile bool found = hashIndex.lookup(&tdbb, &key_desc, &retrieval);
			(void)found;
		}
		
		auto end = high_resolution_clock::now();
		double avg_lookup_time = duration_cast<microseconds>(end - start).count() / 100.0;
		
		lookup_times.push_back(avg_lookup_time);
		
		std::cout << "Dataset size: " << size << ", Avg lookup time: " << avg_lookup_time 
				  << " μs, Load factor: " << hashIndex.getLoadFactor() << std::endl;
	}
	
	// Analyze scalability - lookup times should remain relatively constant (O(1))
	double first_time = lookup_times[0];
	double last_time = lookup_times.back();
	double scalability_ratio = last_time / first_time;
	
	std::cout << "Scalability ratio (8K vs 1K): " << scalability_ratio << std::endl;
	
	// Hash index should scale well - lookup time shouldn't increase dramatically
	BOOST_TEST(scalability_ratio < 3.0); // Should not degrade more than 3x
	
	std::cout << "Expected B-tree scalability: O(log n) = " 
			  << (std::log2(dataset_sizes.back()) / std::log2(dataset_sizes.front())) 
			  << "x slower" << std::endl;
}

BOOST_AUTO_TEST_SUITE_END() // HashVsBTreeComparisonTests

BOOST_AUTO_TEST_SUITE(HashIndexUseCaseTests)

BOOST_AUTO_TEST_CASE(PrimaryKeyLookupSimulation)
{
	std::cout << "\\n=== Primary Key Lookup Simulation ===" << std::endl;
	
	// Simulate typical primary key lookups (integer-like keys)
	MockThreadDb tdbb;
	MockDatabase database;
	MockRelation relation;
	
	index_desc hash_desc;
	hash_desc.idx_type = IDX_TYPE_HASH;
	hash_desc.idx_flags = idx_unique; // Primary key is unique
	hash_desc.idx_count = 1;
	hash_desc.idx_root = 0;
	hash_desc.idx_selectivity = 1.0f;
	hash_desc.idx_id = 1;
	
	HashIndex hashIndex(&tdbb, &database, &relation, &hash_desc);
	
	// Generate primary key-like data
	const int num_records = 10000;
	std::vector<std::string> pk_keys;
	std::vector<RecordNumber> records;
	
	for (int i = 0; i < num_records; i++) {
		char buffer[16];
		sprintf(buffer, "%d", i + 100000); // ID starting from 100000
		pk_keys.emplace_back(buffer);
		records.emplace_back(i + 1);
	}
	
	// Insert all primary keys
	auto start = high_resolution_clock::now();
	
	for (int i = 0; i < num_records; i++) {
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)pk_keys[i].c_str();
		key_desc.dsc_length = pk_keys[i].length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		index_error_t result = hashIndex.insert(&tdbb, &key_desc, records[i], nullptr);
		BOOST_TEST(result == idx_e_ok);
	}
	
	auto end = high_resolution_clock::now();
	auto insert_time = duration_cast<milliseconds>(end - start);
	
	// Simulate random primary key lookups
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, num_records - 1);
	
	const int lookup_count = 1000;
	start = high_resolution_clock::now();
	
	int successful_lookups = 0;
	for (int i = 0; i < lookup_count; i++) {
		int random_index = dis(gen);
		
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)pk_keys[random_index].c_str();
		key_desc.dsc_length = pk_keys[random_index].length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		IndexRetrieval retrieval;
		if (hashIndex.lookup(&tdbb, &key_desc, &retrieval)) {
			successful_lookups++;
		}
	}
	
	end = high_resolution_clock::now();
	auto lookup_time = duration_cast<microseconds>(end - start);
	
	std::cout << "Primary Key Simulation Results:" << std::endl;
	std::cout << "  Records inserted: " << num_records << " in " << insert_time.count() << " ms" << std::endl;
	std::cout << "  Random lookups: " << successful_lookups << "/" << lookup_count 
			  << " in " << lookup_time.count() << " μs" << std::endl;
	std::cout << "  Average lookup time: " << (lookup_time.count() / lookup_count) << " μs" << std::endl;
	std::cout << "  Lookup rate: " << (lookup_count * 1000000 / lookup_time.count()) << " lookups/second" << std::endl;
	
	BOOST_TEST(successful_lookups == lookup_count);
	BOOST_TEST((lookup_time.count() / lookup_count) < 20); // Very fast PK lookups
}

BOOST_AUTO_TEST_CASE(HashJoinSimulation)
{
	std::cout << "\\n=== Hash Join Simulation ===" << std::endl;
	
	// Simulate hash join scenario (equality lookups for joins)
	MockThreadDb tdbb;
	MockDatabase database;
	MockRelation relation;
	
	index_desc hash_desc;
	hash_desc.idx_type = IDX_TYPE_HASH;
	hash_desc.idx_flags = 0;
	hash_desc.idx_count = 1;
	hash_desc.idx_root = 0;
	hash_desc.idx_selectivity = 0.1f; // Foreign key selectivity
	hash_desc.idx_id = 1;
	
	HashIndex hashIndex(&tdbb, &database, &relation, &hash_desc);
	
	// Create "dimension" table data (smaller table)
	const int dim_records = 1000;
	std::vector<std::string> dim_keys;
	
	for (int i = 0; i < dim_records; i++) {
		char buffer[16];
		sprintf(buffer, "DIM_%04d", i);
		dim_keys.emplace_back(buffer);
	}
	
	// Populate hash index with dimension keys
	for (int i = 0; i < dim_records; i++) {
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)dim_keys[i].c_str();
		key_desc.dsc_length = dim_keys[i].length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		RecordNumber record(i + 1);
		hashIndex.insert(&tdbb, &key_desc, record, nullptr);
	}
	
	// Simulate "fact" table lookups (larger table joining to dimension)
	const int fact_lookups = 10000;
	std::mt19937 gen(67890);
	std::uniform_int_distribution<> dis(0, dim_records - 1);
	
	auto start = high_resolution_clock::now();
	
	int join_matches = 0;
	for (int i = 0; i < fact_lookups; i++) {
		int dim_index = dis(gen);  // Random foreign key reference
		
		dsc key_desc;
		key_desc.dsc_address = (UCHAR*)dim_keys[dim_index].c_str();
		key_desc.dsc_length = dim_keys[dim_index].length();
		key_desc.dsc_dtype = dtype_text;
		key_desc.dsc_scale = 0;
		key_desc.dsc_sub_type = 0;
		
		IndexRetrieval retrieval;
		if (hashIndex.lookup(&tdbb, &key_desc, &retrieval)) {
			join_matches++;
		}
	}
	
	auto end = high_resolution_clock::now();
	auto join_time = duration_cast<milliseconds>(end - start);
	
	std::cout << "Hash Join Simulation Results:" << std::endl;
	std::cout << "  Dimension records: " << dim_records << std::endl;
	std::cout << "  Fact table lookups: " << fact_lookups << std::endl;
	std::cout << "  Join matches: " << join_matches << std::endl;
	std::cout << "  Join time: " << join_time.count() << " ms" << std::endl;
	std::cout << "  Join rate: " << (fact_lookups * 1000 / join_time.count()) << " joins/second" << std::endl;
	
	// Hash joins should be very fast
	BOOST_TEST(join_matches == fact_lookups); // All should match
	BOOST_TEST(join_time.count() < 100); // Should complete quickly
}

BOOST_AUTO_TEST_SUITE_END() // HashIndexUseCaseTests

BOOST_AUTO_TEST_SUITE_END() // HashVsBTreeComparisonSuite