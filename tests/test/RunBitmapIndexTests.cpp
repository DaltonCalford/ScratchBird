/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		RunBitmapIndexTests.cpp
 *	DESCRIPTION:	Test runner for bitmap index unit tests
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
 * 2025.07.23 - ScratchBird Bitmap Index Test Runner
 */

#include "firebird.h"
#include "BitmapIndexTest.h"
#include <iostream>
#include <cstdlib>
#include <ctime>

using namespace Jrd;

//----------------------------
// Test Configuration
//----------------------------
struct TestConfiguration
{
	bool run_core_tests;
	bool run_compression_tests;
	bool run_query_tests;
	bool run_catalog_tests;
	bool run_cost_model_tests;
	bool run_cardinality_tests;
	bool run_error_tests;
	bool run_concurrency_tests;
	bool run_integration_tests;
	bool run_benchmark_tests;
	bool verbose_output;
	bool export_results;
	ScratchBird::string export_filename;
	
	TestConfiguration()
	{
		// Default configuration runs all tests
		run_core_tests = true;
		run_compression_tests = true;
		run_query_tests = true;
		run_catalog_tests = true;
		run_cost_model_tests = true;
		run_cardinality_tests = true;
		run_error_tests = true;
		run_concurrency_tests = true;
		run_integration_tests = true;
		run_benchmark_tests = false; // Benchmarks are time-consuming
		verbose_output = false;
		export_results = false;
		export_filename = "bitmap_index_test_results.xml";
	}
};

//----------------------------
// Command Line Parser
//----------------------------
class CommandLineParser
{
public:
	static TestConfiguration parseCommandLine(int argc, char* argv[])
	{
		TestConfiguration config;
		
		for (int i = 1; i < argc; i++) {
			ScratchBird::string arg = argv[i];
			
			if (arg == "--help" || arg == "-h") {
				printUsage();
				exit(0);
			}
			else if (arg == "--verbose" || arg == "-v") {
				config.verbose_output = true;
			}
			else if (arg == "--export") {
				config.export_results = true;
				if (i + 1 < argc) {
					config.export_filename = argv[++i];
				}
			}
			else if (arg == "--core-only") {
				disableAllTests(config);
				config.run_core_tests = true;
			}
			else if (arg == "--compression-only") {
				disableAllTests(config);
				config.run_compression_tests = true;
			}
			else if (arg == "--query-only") {
				disableAllTests(config);
				config.run_query_tests = true;
			}
			else if (arg == "--catalog-only") {
				disableAllTests(config);
				config.run_catalog_tests = true;
			}
			else if (arg == "--cost-model-only") {
				disableAllTests(config);
				config.run_cost_model_tests = true;
			}
			else if (arg == "--cardinality-only") {
				disableAllTests(config);
				config.run_cardinality_tests = true;
			}
			else if (arg == "--error-only") {
				disableAllTests(config);
				config.run_error_tests = true;
			}
			else if (arg == "--concurrency-only") {
				disableAllTests(config);
				config.run_concurrency_tests = true;
			}
			else if (arg == "--integration-only") {
				disableAllTests(config);
				config.run_integration_tests = true;
			}
			else if (arg == "--benchmark") {
				config.run_benchmark_tests = true;
			}
			else if (arg == "--no-core") {
				config.run_core_tests = false;
			}
			else if (arg == "--no-compression") {
				config.run_compression_tests = false;
			}
			else if (arg == "--no-query") {
				config.run_query_tests = false;
			}
			else if (arg == "--no-catalog") {
				config.run_catalog_tests = false;
			}
			else if (arg == "--no-cost-model") {
				config.run_cost_model_tests = false;
			}
			else if (arg == "--no-cardinality") {
				config.run_cardinality_tests = false;
			}
			else if (arg == "--no-error") {
				config.run_error_tests = false;
			}
			else if (arg == "--no-concurrency") {
				config.run_concurrency_tests = false;
			}
			else if (arg == "--no-integration") {
				config.run_integration_tests = false;
			}
			else {
				std::cerr << "Unknown option: " << arg.c_str() << std::endl;
				printUsage();
				exit(1);
			}
		}
		
		return config;
	}

private:
	static void printUsage()
	{
		std::cout << "ScratchBird Bitmap Index Test Suite" << std::endl;
		std::cout << "====================================" << std::endl;
		std::cout << std::endl;
		std::cout << "Usage: RunBitmapIndexTests [options]" << std::endl;
		std::cout << std::endl;
		std::cout << "Options:" << std::endl;
		std::cout << "  -h, --help              Show this help message" << std::endl;
		std::cout << "  -v, --verbose           Enable verbose output" << std::endl;
		std::cout << "  --export [filename]     Export results to XML file" << std::endl;
		std::cout << std::endl;
		std::cout << "Test Selection:" << std::endl;
		std::cout << "  --core-only             Run only core functionality tests" << std::endl;
		std::cout << "  --compression-only      Run only compression tests" << std::endl;
		std::cout << "  --query-only            Run only query execution tests" << std::endl;
		std::cout << "  --catalog-only          Run only system catalog tests" << std::endl;
		std::cout << "  --cost-model-only       Run only cost model tests" << std::endl;
		std::cout << "  --cardinality-only      Run only cardinality tests" << std::endl;
		std::cout << "  --error-only            Run only error handling tests" << std::endl;
		std::cout << "  --concurrency-only      Run only concurrency tests" << std::endl;
		std::cout << "  --integration-only      Run only integration tests" << std::endl;
		std::cout << "  --benchmark             Include benchmark tests (slow)" << std::endl;
		std::cout << std::endl;
		std::cout << "Test Exclusion:" << std::endl;
		std::cout << "  --no-core               Skip core functionality tests" << std::endl;
		std::cout << "  --no-compression        Skip compression tests" << std::endl;
		std::cout << "  --no-query              Skip query execution tests" << std::endl;
		std::cout << "  --no-catalog            Skip system catalog tests" << std::endl;
		std::cout << "  --no-cost-model         Skip cost model tests" << std::endl;
		std::cout << "  --no-cardinality        Skip cardinality tests" << std::endl;
		std::cout << "  --no-error              Skip error handling tests" << std::endl;
		std::cout << "  --no-concurrency        Skip concurrency tests" << std::endl;
		std::cout << "  --no-integration        Skip integration tests" << std::endl;
		std::cout << std::endl;
		std::cout << "Examples:" << std::endl;
		std::cout << "  RunBitmapIndexTests                    # Run all tests (except benchmarks)" << std::endl;
		std::cout << "  RunBitmapIndexTests --core-only        # Run only core tests" << std::endl;
		std::cout << "  RunBitmapIndexTests --verbose          # Run with verbose output" << std::endl;
		std::cout << "  RunBitmapIndexTests --benchmark        # Include benchmark tests" << std::endl;
		std::cout << "  RunBitmapIndexTests --export results.xml # Export results to file" << std::endl;
	}
	
	static void disableAllTests(TestConfiguration& config)
	{
		config.run_core_tests = false;
		config.run_compression_tests = false;
		config.run_query_tests = false;
		config.run_catalog_tests = false;
		config.run_cost_model_tests = false;
		config.run_cardinality_tests = false;
		config.run_error_tests = false;
		config.run_concurrency_tests = false;
		config.run_integration_tests = false;
		config.run_benchmark_tests = false;
	}
};

//----------------------------
// Test Suite Builder
//----------------------------
class TestSuiteBuilder
{
public:
	static BitmapIndexTestSuite* buildTestSuite(const TestConfiguration& config)
	{
		BitmapIndexTestSuite* suite = new BitmapIndexTestSuite();
		
		if (config.run_core_tests) {
			std::cout << "Adding Core Functionality Tests..." << std::endl;
			suite->addTest(new BitmapIndexCoreTest());
		}
		
		if (config.run_compression_tests) {
			std::cout << "Adding Compression Tests..." << std::endl;
			suite->addTest(new BitmapIndexCompressionTest());
		}
		
		if (config.run_query_tests) {
			std::cout << "Adding Query Execution Tests..." << std::endl;
			suite->addTest(new BitmapIndexQueryTest());
		}
		
		if (config.run_catalog_tests) {
			std::cout << "Adding System Catalog Tests..." << std::endl;
			suite->addTest(new BitmapIndexSystemCatalogTest());
		}
		
		if (config.run_cost_model_tests) {
			std::cout << "Adding Cost Model Tests..." << std::endl;
			suite->addTest(new BitmapIndexCostModelTest());
		}
		
		if (config.run_cardinality_tests) {
			std::cout << "Adding Cardinality Tests..." << std::endl;
			suite->addTest(new BitmapIndexCardinalityTest());
		}
		
		if (config.run_error_tests) {
			std::cout << "Adding Error Handling Tests..." << std::endl;
			suite->addTest(new BitmapIndexErrorTest());
		}
		
		if (config.run_concurrency_tests) {
			std::cout << "Adding Concurrency Tests..." << std::endl;
			suite->addTest(new BitmapIndexConcurrencyTest());
		}
		
		if (config.run_integration_tests) {
			std::cout << "Adding Integration Tests..." << std::endl;
			suite->addTest(new BitmapIndexIntegrationTest());
		}
		
		if (config.run_benchmark_tests) {
			std::cout << "Adding Performance Benchmark Tests..." << std::endl;
			std::cout << "Warning: Benchmark tests may take several minutes to complete." << std::endl;
			suite->addTest(new BitmapIndexBenchmarkTest());
		}
		
		return suite;
	}
};

//----------------------------
// Test Result Reporter
//----------------------------
class TestResultReporter
{
public:
	static void reportResults(const BitmapIndexTestSuite* suite, const TestConfiguration& config)
	{
		if (config.verbose_output) {
			suite->printDetailedResults();
		} else {
			suite->printTestResults();
		}
		
		if (config.export_results) {
			std::cout << std::endl << "Exporting results to: " << config.export_filename.c_str() << std::endl;
			suite->exportTestResults(config.export_filename);
		}
		
		// Print summary statistics
		printSummaryStatistics(suite);
		
		// Print recommendations based on results
		printRecommendations(suite);
	}

private:
	static void printSummaryStatistics(const BitmapIndexTestSuite* suite)
	{
		std::cout << std::endl;
		std::cout << "Summary Statistics:" << std::endl;
		std::cout << "==================" << std::endl;
		std::cout << "Total Test Categories: " << getTestCategoryCount(suite) << std::endl;
		std::cout << "Total Individual Tests: " << suite->getTotalTests() << std::endl;
		std::cout << "Overall Success Rate: " << suite->getSuccessRate() << "%" << std::endl;
		
		if (suite->getSuccessRate() >= 95.0) {
			std::cout << "Quality Assessment: EXCELLENT ✅" << std::endl;
		} else if (suite->getSuccessRate() >= 85.0) {
			std::cout << "Quality Assessment: GOOD ⚠️" << std::endl;
		} else if (suite->getSuccessRate() >= 70.0) {
			std::cout << "Quality Assessment: NEEDS IMPROVEMENT ⚠️" << std::endl;
		} else {
			std::cout << "Quality Assessment: CRITICAL ISSUES ❌" << std::endl;
		}
	}
	
	static void printRecommendations(const BitmapIndexTestSuite* suite)
	{
		std::cout << std::endl;
		std::cout << "Recommendations:" << std::endl;
		std::cout << "===============" << std::endl;
		
		if (suite->getFailedTests() == 0) {
			std::cout << "🎉 All tests passed! The bitmap index implementation is ready for production." << std::endl;
			std::cout << "📋 Consider running benchmark tests to validate performance characteristics." << std::endl;
			std::cout << "🔍 Regular regression testing is recommended for ongoing development." << std::endl;
		} else if (suite->getSuccessRate() >= 90.0) {
			std::cout << "⚠️  Most tests passed with minor issues. Review failed tests before deployment." << std::endl;
			std::cout << "🔧 Address failing tests and re-run the test suite." << std::endl;
		} else if (suite->getSuccessRate() >= 70.0) {
			std::cout << "⚠️  Significant issues detected. Implementation needs additional work." << std::endl;
			std::cout << "🔍 Focus on failed test categories for debugging and fixes." << std::endl;
			std::cout << "❌ NOT recommended for production deployment." << std::endl;
		} else {
			std::cout << "❌ Critical issues detected. Major rework required." << std::endl;
			std::cout << "🚫 Implementation is NOT ready for production use." << std::endl;
			std::cout << "🔧 Comprehensive debugging and fixes needed across multiple areas." << std::endl;
		}
		
		// Specific recommendations based on failed categories
		if (suite->getFailedTests() > 0) {
			std::cout << std::endl << "Debugging Guidelines:" << std::endl;
			std::cout << "• Check system catalog metadata consistency" << std::endl;
			std::cout << "• Validate bitmap compression and decompression" << std::endl;
			std::cout << "• Verify query execution correctness" << std::endl;
			std::cout << "• Test with various cardinality scenarios" << std::endl;
			std::cout << "• Review error handling and edge cases" << std::endl;
		}
	}
	
	static ULONG getTestCategoryCount(const BitmapIndexTestSuite* suite)
	{
		// This would count the actual test categories
		// For now, return the maximum possible categories
		return 10;
	}
};

//----------------------------
// Main Test Runner
//----------------------------
int main(int argc, char* argv[])
{
	// Initialize random seed for test data generation
	srand(static_cast<unsigned int>(time(nullptr)));
	
	try {
		// Parse command line arguments
		TestConfiguration config = CommandLineParser::parseCommandLine(argc, argv);
		
		// Print banner
		std::cout << "ScratchBird Bitmap Index Test Suite v1.0" << std::endl;
		std::cout << "=========================================" << std::endl;
		std::cout << "Testing comprehensive bitmap index functionality..." << std::endl;
		std::cout << std::endl;
		
		// Build test suite based on configuration
		BitmapIndexTestSuite* suite = TestSuiteBuilder::buildTestSuite(config);
		
		if (!suite) {
			std::cerr << "Failed to create test suite!" << std::endl;
			return 1;
		}
		
		// Record start time
		clock_t start_time = clock();
		
		// Run all configured tests
		suite->runAllTests();
		
		// Record end time
		clock_t end_time = clock();
		double execution_time = static_cast<double>(end_time - start_time) / CLOCKS_PER_SEC;
		
		std::cout << std::endl;
		std::cout << "Test execution completed in " << execution_time << " seconds." << std::endl;
		
		// Report results
		TestResultReporter::reportResults(suite, config);
		
		// Determine exit code based on test results
		int exit_code = (suite->getFailedTests() == 0) ? 0 : 1;
		
		// Cleanup
		delete suite;
		
		return exit_code;
	}
	catch (const Exception& ex) {
		std::cerr << "Test suite failed with exception: " << ex.what() << std::endl;
		return 2;
	}
	catch (const std::exception& ex) {
		std::cerr << "Test suite failed with standard exception: " << ex.what() << std::endl;
		return 2;
	}
	catch (...) {
		std::cerr << "Test suite failed with unknown exception!" << std::endl;
		return 2;
	}
}