/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		BitmapIndexTest.h
 *	DESCRIPTION:	Unit test suite for bitmap index functionality
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
 * 2025.07.23 - ScratchBird Bitmap Index Unit Test Suite
 */

#ifndef JRD_BITMAP_INDEX_TEST_H
#define JRD_BITMAP_INDEX_TEST_H

#include "../jrd/constants.h"
#include "../jrd/BitmapIndex.h"
#include "../jrd/BitmapIndexSystemCatalog.h"
#include "../jrd/recsrc/BitmapIndexTableScan.h"
#include "../jrd/optimizer/BitmapIndexCostModel.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/array.h"

namespace Jrd {

// Forward declarations
class thread_db;
class jrd_tra;
class jrd_rel;
struct index_desc;

//----------------------------
// Test Framework Base Class
//----------------------------
class BitmapIndexTestBase
{
public:
	BitmapIndexTestBase();
	virtual ~BitmapIndexTestBase();

	// Test framework methods
	virtual void setUp();
	virtual void tearDown();
	virtual void runTest() = 0;
	
	// Test utilities
	void assertTrue(bool condition, const char* message);
	void assertFalse(bool condition, const char* message);
	void assertEqual(SLONG expected, SLONG actual, const char* message);
	void assertEqual(double expected, double actual, double tolerance, const char* message);
	void assertNotNull(void* ptr, const char* message);
	void assertNull(void* ptr, const char* message);

protected:
	// Test infrastructure
	thread_db* m_tdbb;
	jrd_tra* m_transaction;
	jrd_rel* m_test_relation;
	index_desc* m_bitmap_index;
	MemoryPool* m_pool;
	
	// Test data management
	void createTestDatabase();
	void createTestRelation();
	void createTestBitmapIndex();
	void insertTestData(ULONG record_count, ULONG distinct_values);
	void cleanupTestData();
	
	// Helper methods
	RecordNumber insertRecord(SLONG value);
	bool deleteRecord(RecordNumber record_num);
	bool updateRecord(RecordNumber record_num, SLONG new_value);
	ULONG countRecords(SLONG value);
	
	// Validation utilities
	void validateBitmapConsistency();
	void validateIndexStatistics();
	void validateSystemCatalog();

private:
	ULONG m_test_counter;
	ULONG m_passed_tests;
	ULONG m_failed_tests;
	
	void logTestResult(bool passed, const char* message);
};

//----------------------------
// Core Bitmap Index Tests
//----------------------------
class BitmapIndexCoreTest : public BitmapIndexTestBase
{
public:
	void runTest() override;

private:
	// Basic functionality tests
	void testBitmapIndexCreation();
	void testBitmapIndexDestruction();
	void testBitmapIndexInitialization();
	void testIndexTypeIdentification();
	
	// Data insertion tests
	void testInsertSingleValue();
	void testInsertMultipleValues();
	void testInsertDuplicateValues();
	void testInsertNullValues();
	void testInsertLargeDataset();
	
	// Data lookup tests
	void testLookupExistingValue();
	void testLookupNonExistentValue();
	void testLookupNullValue();
	void testLookupMultipleValues();
	
	// Bitmap operations tests
	void testBitmapUnion();
	void testBitmapIntersection();
	void testBitmapComplement();
	void testBitmapXor();
};

//----------------------------
// Compression and Storage Tests
//----------------------------
class BitmapIndexCompressionTest : public BitmapIndexTestBase
{
public:
	void runTest() override;

private:
	// Compression algorithm tests
	void testRLECompression();
	void testLZ4Compression();
	void testZstdCompression();
	void testAutoCompressionSelection();
	
	// Compression efficiency tests
	void testCompressionRatio();
	void testCompressionPerformance();
	void testDecompressionAccuracy();
	
	// Storage tests
	void testBitmapPersistence();
	void testBitmapRecovery();
	void testChunkProcessing();
	void testLargeBitmapHandling();
};

//----------------------------
// Query Execution Tests
//----------------------------
class BitmapIndexQueryTest : public BitmapIndexTestBase
{
public:
	void runTest() override;

private:
	// Query type tests
	void testEqualityQuery();
	void testInListQuery();
	void testNullQuery();
	void testNotNullQuery();
	void testRangeQuery();
	void testMultiValueQuery();
	
	// Query execution strategy tests
	void testSequentialExecution();
	void testParallelExecution();
	void testHybridExecution();
	void testChunkedExecution();
	
	// Query result tests
	void testQueryResultAccuracy();
	void testQueryResultCompleteness();
	void testQueryResultOrder();
	
	// Performance tests
	void testQueryPerformance();
	void testSelectivityAccuracy();
};

//----------------------------
// System Catalog Tests
//----------------------------
class BitmapIndexSystemCatalogTest : public BitmapIndexTestBase
{
public:
	void runTest() override;

private:
	// Metadata storage tests
	void testMetadataStorage();
	void testMetadataRetrieval();
	void testMetadataUpdate();
	void testMetadataDeletion();
	
	// Statistics tests
	void testCardinalityTracking();
	void testCompressionStatistics();
	void testCacheStatistics();
	void testSizeStatistics();
	
	// Configuration tests
	void testOptionsStorage();
	void testOptionsRetrieval();
	void testOptionsValidation();
	void testOptionsReset();
	
	// Maintenance tests
	void testMaintenanceScheduling();
	void testMaintenanceCompletion();
	void testStatisticsCollection();
	void testMetadataRepair();
};

//----------------------------
// Cost Model Tests
//----------------------------
class BitmapIndexCostModelTest : public BitmapIndexTestBase
{
public:
	void runTest() override;

private:
	// Cost estimation tests
	void testEqualityCostEstimation();
	void testInListCostEstimation();
	void testNullCostEstimation();
	void testRangeCostEstimation();
	
	// Selectivity tests
	void testSelectivityCalculation();
	void testCardinalityImpact();
	void testStatisticsAccuracy();
	
	// Optimization tests
	void testStrategyRecommendation();
	void testCompressionRecommendation();
	void testChunkSizeRecommendation();
	
	// Cost model validation tests
	void testCostFactorConfiguration();
	void testCostModelAccuracy();
	void testOptimizerIntegration();
};

//----------------------------
// Cardinality and Performance Tests
//----------------------------
class BitmapIndexCardinalityTest : public BitmapIndexTestBase
{
public:
	void runTest() override;

private:
	// Low cardinality tests
	void testVeryLowCardinality();    // 2-10 distinct values
	void testLowCardinality();        // 10-100 distinct values
	void testMediumCardinality();     // 100-1000 distinct values
	void testHighCardinality();       // 1000-10000 distinct values
	void testVeryHighCardinality();   // > 10000 distinct values
	
	// Cardinality threshold tests
	void testCardinalityThresholds();
	void testDynamicThresholdAdjustment();
	void testCardinalityValidation();
	
	// Performance scaling tests
	void testPerformanceScaling();
	void testMemoryUsageScaling();
	void testIndexSizeScaling();
	
	// Cardinality-specific optimizations
	void testLowCardinalityOptimizations();
	void testHighCardinalityWarnings();
	void testCardinalityBasedRecommendations();
};

//----------------------------
// Error Handling and Edge Case Tests
//----------------------------
class BitmapIndexErrorTest : public BitmapIndexTestBase
{
public:
	void runTest() override;

private:
	// Error condition tests
	void testInvalidIndexType();
	void testCorruptedBitmap();
	void testInsufficientMemory();
	void testDiskSpaceExhaustion();
	void testTransactionRollback();
	
	// Edge case tests
	void testEmptyBitmap();
	void testSingleRecordBitmap();
	void testMaximumSizeBitmap();
	void testExtremeLowCardinality();
	void testExtremeHighCardinality();
	
	// Recovery tests
	void testErrorRecovery();
	void testPartialFailureRecovery();
	void testMetadataRecovery();
	void testConsistencyRepair();
	
	// Boundary condition tests
	void testMaximumRecordCount();
	void testMaximumDistinctValues();
	void testMaximumBitmapSize();
	void testMinimumChunkSize();
	void testMaximumChunkSize();
};

//----------------------------
// Concurrency and Transaction Tests
//----------------------------
class BitmapIndexConcurrencyTest : public BitmapIndexTestBase
{
public:
	void runTest() override;

private:
	// Transaction tests
	void testTransactionIsolation();
	void testTransactionCommit();
	void testTransactionRollback();
	void testNestedTransactions();
	
	// Concurrency tests
	void testConcurrentReads();
	void testConcurrentWrites();
	void testReadWriteConcurrency();
	void testDeadlockPrevention();
	
	// Consistency tests
	void testMVCCConsistency();
	void testACIDCompliance();
	void testConsistentReads();
	void testDurability();
	
	// Lock management tests
	void testLockAcquisition();
	void testLockRelease();
	void testLockEscalation();
	void testLockTimeout();
};

//----------------------------
// Integration Tests
//----------------------------
class BitmapIndexIntegrationTest : public BitmapIndexTestBase
{
public:
	void runTest() override;

private:
	// DDL integration tests
	void testCreateIndexDDL();
	void testDropIndexDDL();
	void testAlterIndexDDL();
	void testIndexRebuild();
	
	// DML integration tests
	void testInsertWithBitmapIndex();
	void testUpdateWithBitmapIndex();
	void testDeleteWithBitmapIndex();
	void testBulkOperations();
	
	// Query optimizer integration
	void testOptimizerSelection();
	void testCostBasedOptimization();
	void testPlanGeneration();
	void testExecutionStatistics();
	
	// Schema integration tests
	void testSchemaAwareness();
	void testCrossSchemaQueries();
	void testSchemaEvolution();
	
	// Client tool integration
	void testISQLIntegration();
	void testGBAKIntegration();
	void testAdministrativeTools();
};

//----------------------------
// Performance Benchmark Tests
//----------------------------
class BitmapIndexBenchmarkTest : public BitmapIndexTestBase
{
public:
	void runTest() override;

private:
	// Performance baseline tests
	void benchmarkTableScan();
	void benchmarkBTreeIndex();
	void benchmarkHashIndex();
	void benchmarkBitmapIndex();
	
	// Comparative performance tests
	void compareBitmapVsBTree();
	void compareBitmapVsHash();
	void compareBitmapVsTableScan();
	
	// Scalability tests
	void testSmallDataset();      // < 1,000 records
	void testMediumDataset();     // 1,000 - 100,000 records
	void testLargeDataset();      // 100,000 - 1,000,000 records
	void testVeryLargeDataset();  // > 1,000,000 records
	
	// Performance validation
	void validatePerformanceGains();
	void validateMemoryUsage();
	void validateDiskUsage();
	void validateCPUUsage();
};

//----------------------------
// Test Suite Runner
//----------------------------
class BitmapIndexTestSuite
{
public:
	BitmapIndexTestSuite();
	~BitmapIndexTestSuite();

	// Test suite management
	void addTest(BitmapIndexTestBase* test);
	void runAllTests();
	void runTest(const ScratchBird::string& test_name);
	
	// Results and reporting
	ULONG getTotalTests() const;
	ULONG getPassedTests() const;
	ULONG getFailedTests() const;
	double getSuccessRate() const;
	
	void printTestResults();
	void printDetailedResults();
	void exportTestResults(const ScratchBird::string& filename);

private:
	ScratchBird::ObjectsArray<BitmapIndexTestBase*> m_tests;
	ScratchBird::ObjectsArray<ScratchBird::string> m_test_names;
	ULONG m_total_tests;
	ULONG m_passed_tests;
	ULONG m_failed_tests;
	
	void initializeTestSuite();
	void cleanupTestSuite();
	void recordTestResult(const ScratchBird::string& test_name, bool passed);
};

//----------------------------
// Test Utilities and Helpers
//----------------------------
class BitmapIndexTestUtils
{
public:
	// Data generation utilities
	static void generateTestData(ScratchBird::ObjectsArray<SLONG>& data, 
		ULONG record_count, ULONG distinct_values);
	
	static void generateSkewedData(ScratchBird::ObjectsArray<SLONG>& data,
		ULONG record_count, ULONG distinct_values, double skew_factor);
	
	static void generateSequentialData(ScratchBird::ObjectsArray<SLONG>& data,
		ULONG record_count, SLONG start_value);
	
	static void generateRandomData(ScratchBird::ObjectsArray<SLONG>& data,
		ULONG record_count, SLONG min_value, SLONG max_value);
	
	// Validation utilities
	static bool validateBitmapAccuracy(const RecordBitmap* bitmap,
		const ScratchBird::ObjectsArray<RecordNumber>& expected_records);
	
	static bool validateCardinality(ULONG actual_cardinality, 
		ULONG expected_cardinality, double tolerance);
	
	static bool validateCompressionRatio(double actual_ratio,
		double expected_ratio, double tolerance);
	
	// Performance measurement utilities
	static ULONG measureQueryTime(BitmapIndexTableScan* scan);
	static ULONG measureMemoryUsage(const BitmapIndex* index);
	static double measureCompressionRatio(const RecordBitmap* bitmap);
	
	// Test data cleanup
	static void cleanupTestDatabase();
	static void cleanupTestRelations();
	static void cleanupTestIndexes();
};

} // namespace Jrd

#endif // JRD_BITMAP_INDEX_TEST_H