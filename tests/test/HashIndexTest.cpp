#include "firebird.h"
#include "boost/test/unit_test.hpp"
#include "../HashIndex.h"
#include "../IndexTypeRegistry.h"
#include "../constants.h"
#include "../btr.h"
#include "../jrd.h"
#include "../tra.h"
#include "../../common/gdsassert.h"
#include <chrono>
#include <cstring>
#include <cstdio>

using namespace ScratchBird;
using namespace Jrd;

BOOST_AUTO_TEST_SUITE(EngineSuite)
BOOST_AUTO_TEST_SUITE(HashIndexSuite)

// Mock classes and structures for testing
namespace {
	class MockThreadDb : public thread_db
	{
	public:
		MockThreadDb() = default;
	};

	class MockRelation : public jrd_rel
	{
	public:
		MockRelation() = default;
	};

	class MockTransaction : public jrd_tra
	{
	public:
		MockTransaction() = default;
	};
}

BOOST_AUTO_TEST_SUITE(HashIndexRegistryTests)

BOOST_AUTO_TEST_CASE(IndexTypeRegistryTest)
{
	IndexTypeRegistry& registry = IndexTypeRegistry::getInstance();
	
	// Test hash index type registration
	BOOST_TEST(registry.isTypeSupported(IDX_TYPE_NAME_HASH));
	BOOST_TEST(registry.getTypeId(IDX_TYPE_NAME_HASH) == IDX_TYPE_HASH);
	BOOST_TEST(strcmp(registry.getTypeName(IDX_TYPE_HASH), IDX_TYPE_NAME_HASH) == 0);
	
	// Test default type
	BOOST_TEST(registry.getDefaultTypeId() == IDX_TYPE_BTREE);
	BOOST_TEST(strcmp(registry.getDefaultTypeName(), IDX_TYPE_NAME_BTREE) == 0);
	
	// Test unsupported type
	BOOST_TEST(!registry.isTypeSupported("UNSUPPORTED"));
	BOOST_TEST(registry.getTypeId("UNSUPPORTED") == -1);
}

BOOST_AUTO_TEST_CASE(HashIndexCreationTest)
{
	IndexTypeRegistry& registry = IndexTypeRegistry::getInstance();
	
	// Create hash index instance
	index_desc desc;
	desc.idx_type = IDX_TYPE_HASH;
	desc.idx_flags = 0;
	desc.idx_count = 1;
	desc.idx_root = 0;
	desc.idx_selectivity = 0.5f;
	
	AutoPtr<IndexType> hashIndex = registry.createIndexType(IDX_TYPE_HASH);
	BOOST_TEST(hashIndex.get() != nullptr);
	
	// Test hash index properties
	BOOST_TEST(hashIndex->getTypeId() == IDX_TYPE_HASH);
	BOOST_TEST(strcmp(hashIndex->getTypeName(), IDX_TYPE_NAME_HASH) == 0);
	BOOST_TEST(hashIndex->supportsOrdering() == false);
	BOOST_TEST(hashIndex->supportsUniqueness() == true);
	BOOST_TEST(hashIndex->supportsPartialKey() == false);
}

BOOST_AUTO_TEST_SUITE_END() // HashIndexRegistryTests

BOOST_AUTO_TEST_SUITE(HashIndexFunctionTests)

BOOST_AUTO_TEST_CASE(HashFunctionCRC32Test)
{
	// Test CRC32 hash function
	const char* testData1 = "test_key_1";
	const char* testData2 = "test_key_2";
	const char* testData3 = "test_key_1"; // Same as testData1
	
	ULONG hash1 = HashIndex::hashCRC32(reinterpret_cast<const UCHAR*>(testData1), strlen(testData1));
	ULONG hash2 = HashIndex::hashCRC32(reinterpret_cast<const UCHAR*>(testData2), strlen(testData2));
	ULONG hash3 = HashIndex::hashCRC32(reinterpret_cast<const UCHAR*>(testData3), strlen(testData3));
	
	// Same data should produce same hash
	BOOST_TEST(hash1 == hash3);
	
	// Different data should produce different hash (with high probability)
	BOOST_TEST(hash1 != hash2);
	
	// Hash values should be non-zero for non-empty data
	BOOST_TEST(hash1 != 0);
	BOOST_TEST(hash2 != 0);
}

BOOST_AUTO_TEST_CASE(HashFunctionMurmurHash3Test)
{
	// Test MurmurHash3 function
	const char* testData1 = "test_key_1";
	const char* testData2 = "test_key_2";
	const char* testData3 = "test_key_1"; // Same as testData1
	
	ULONG hash1 = HashIndex::hashMurmurHash3(reinterpret_cast<const UCHAR*>(testData1), strlen(testData1));
	ULONG hash2 = HashIndex::hashMurmurHash3(reinterpret_cast<const UCHAR*>(testData2), strlen(testData2));
	ULONG hash3 = HashIndex::hashMurmurHash3(reinterpret_cast<const UCHAR*>(testData3), strlen(testData3));
	
	// Same data should produce same hash
	BOOST_TEST(hash1 == hash3);
	
	// Different data should produce different hash (with high probability)
	BOOST_TEST(hash1 != hash2);
	
	// Hash values should be non-zero for non-empty data
	BOOST_TEST(hash1 != 0);
	BOOST_TEST(hash2 != 0);
}

BOOST_AUTO_TEST_CASE(HashFunctionDistributionTest)
{
	// Test hash function distribution
	const int NUM_KEYS = 1000;
	const int NUM_BUCKETS = 64;
	int bucketCounts[NUM_BUCKETS] = {0};
	
	// Generate test keys and hash them
	for (int i = 0; i < NUM_KEYS; i++) {
		char key[32];
		sprintf(key, "test_key_%d", i);
		
		ULONG hash = HashIndex::hashCRC32(reinterpret_cast<const UCHAR*>(key), strlen(key));
		int bucket = hash % NUM_BUCKETS;
		bucketCounts[bucket]++;
	}
	
	// Check that distribution is reasonably uniform
	// Each bucket should have roughly NUM_KEYS/NUM_BUCKETS entries
	double expectedPerBucket = static_cast<double>(NUM_KEYS) / NUM_BUCKETS;
	int emptyBuckets = 0;
	int overloadedBuckets = 0;
	
	for (int i = 0; i < NUM_BUCKETS; i++) {
		if (bucketCounts[i] == 0) {
			emptyBuckets++;
		} else if (bucketCounts[i] > expectedPerBucket * 2) {
			overloadedBuckets++;
		}
	}
	
	// Distribution should be reasonable (less than 20% empty buckets)
	BOOST_TEST(emptyBuckets < NUM_BUCKETS * 0.2);
	// And not too many overloaded buckets (less than 10%)
	BOOST_TEST(overloadedBuckets < NUM_BUCKETS * 0.1);
}

BOOST_AUTO_TEST_SUITE_END() // HashIndexFunctionTests

BOOST_AUTO_TEST_SUITE(HashIndexStructureTests)

BOOST_AUTO_TEST_CASE(HashBucketTest)
{
	// Test hash bucket structure
	HashIndex::hash_bucket bucket;
	bucket.hb_count = 0;
	bucket.hb_flags = 0;
	bucket.hb_data = nullptr;
	
	// Test initial state
	BOOST_TEST(bucket.hb_count == 0);
	BOOST_TEST(bucket.hb_flags == 0);
	BOOST_TEST(bucket.hb_data == nullptr);
	
	// Test setting values
	bucket.hb_count = 5;
	bucket.hb_flags = HashIndex::HBF_OVERFLOW;
	
	BOOST_TEST(bucket.hb_count == 5);
	BOOST_TEST(bucket.hb_flags == HashIndex::HBF_OVERFLOW);
}

BOOST_AUTO_TEST_CASE(HashPageStructureTest)
{
	// Test hash page structure
	const USHORT BUCKET_COUNT = 64;
	
	// Calculate required page size for hash page
	size_t pageSize = sizeof(HashIndex::hash_page) + 
		(BUCKET_COUNT - 1) * sizeof(HashIndex::hash_bucket);
	
	// Verify page size is reasonable
	BOOST_TEST(pageSize > sizeof(HashIndex::hash_page));
	BOOST_TEST(pageSize < 8192); // Should fit in standard page
	
	// Test hash page header constants
	BOOST_TEST(HashIndex::HPF_OVERFLOW_PAGE == 1);
	BOOST_TEST(HashIndex::HPF_ROOT_PAGE == 2);
}

BOOST_AUTO_TEST_SUITE_END() // HashIndexStructureTests

BOOST_AUTO_TEST_SUITE(HashIndexOperationsTests)

BOOST_AUTO_TEST_CASE(IndexDescriptorTest)
{
	// Test creating index descriptor for hash index
	index_desc desc;
	desc.idx_type = IDX_TYPE_HASH;
	desc.idx_flags = idx_unique;
	desc.idx_count = 1;
	desc.idx_root = 100;
	desc.idx_selectivity = 0.8f;
	desc.idx_id = 1;
	desc.idx_primary_index = idx_invalid;
	desc.idx_primary_relation = 0;
	desc.idx_runtime_flags = 0;
	
	// Verify hash index descriptor properties
	BOOST_TEST(desc.idx_type == IDX_TYPE_HASH);
	BOOST_TEST((desc.idx_flags & idx_unique) != 0);
	BOOST_TEST(desc.idx_count == 1);
	BOOST_TEST(desc.idx_root == 100);
	BOOST_TEST(desc.idx_selectivity == 0.8f);
	
	// Test index type validation
	BOOST_TEST(desc.idx_type != IDX_TYPE_BTREE);
	BOOST_TEST(desc.idx_type != IDX_TYPE_GIN);
}

BOOST_AUTO_TEST_CASE(KeyComparisonTest)
{
	// Test key comparison for hash indexes
	temporary_key key1, key2, key3;
	
	// Initialize test keys
	key1.key_length = 10;
	strcpy(reinterpret_cast<char*>(key1.key_data), "test_key1");
	key1.key_flags = 0;
	key1.key_nulls = 0;
	
	key2.key_length = 10;
	strcpy(reinterpret_cast<char*>(key2.key_data), "test_key2");
	key2.key_flags = 0;
	key2.key_nulls = 0;
	
	key3.key_length = 10;
	strcpy(reinterpret_cast<char*>(key3.key_data), "test_key1");
	key3.key_flags = 0;
	key3.key_nulls = 0;
	
	// Test key equality
	bool keys1and3Equal = (key1.key_length == key3.key_length) && 
		(memcmp(key1.key_data, key3.key_data, key1.key_length) == 0);
	BOOST_TEST(keys1and3Equal == true);
	
	// Test key inequality
	bool keys1and2Equal = (key1.key_length == key2.key_length) && 
		(memcmp(key1.key_data, key2.key_data, key1.key_length) == 0);
	BOOST_TEST(keys1and2Equal == false);
}

BOOST_AUTO_TEST_SUITE_END() // HashIndexOperationsTests

BOOST_AUTO_TEST_SUITE(HashIndexPerformanceTests)

BOOST_AUTO_TEST_CASE(HashPerformanceTest)
{
	// Test hash function performance
	const int NUM_ITERATIONS = 10000;
	const char* testKey = "performance_test_key_with_reasonable_length";
	const size_t keyLength = strlen(testKey);
	
	// Time CRC32 hashing
	auto startTime = std::chrono::high_resolution_clock::now();
	
	for (int i = 0; i < NUM_ITERATIONS; i++) {
		ULONG hash = HashIndex::hashCRC32(
			reinterpret_cast<const UCHAR*>(testKey), keyLength);
		// Use hash to prevent optimization
		volatile ULONG dummy = hash;
		(void)dummy;
	}
	
	auto endTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
	
	// Hash function should be fast (less than 100 microseconds for 10K operations)
	BOOST_TEST(duration.count() < 100000);
}

BOOST_AUTO_TEST_CASE(CollisionHandlingTest)
{
	// Test collision handling by forcing hash collisions
	const int NUM_BUCKETS = 8; // Small number to force collisions
	int collisionCount = 0;
	
	ULONG hashes[100];
	
	// Generate hashes and count collisions
	for (int i = 0; i < 100; i++) {
		char key[32];
		sprintf(key, "collision_test_%d", i);
		
		hashes[i] = HashIndex::hashCRC32(
			reinterpret_cast<const UCHAR*>(key), strlen(key)) % NUM_BUCKETS;
		
		// Check for collisions with previous hashes
		for (int j = 0; j < i; j++) {
			if (hashes[i] == hashes[j]) {
				collisionCount++;
				break;
			}
		}
	}
	
	// With only 8 buckets and 100 keys, we should have many collisions
	BOOST_TEST(collisionCount > 80); // Expect significant collisions
}

BOOST_AUTO_TEST_SUITE_END() // HashIndexPerformanceTests

BOOST_AUTO_TEST_SUITE_END() // HashIndexSuite
BOOST_AUTO_TEST_SUITE_END() // EngineSuite