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
 * 2025.07.23 - ScratchBird GIN Index Implementation - Test Configuration
 */

#ifndef JRD_GIN_TEST_CONFIG_H
#define JRD_GIN_TEST_CONFIG_H

//----------------------------
// Test Configuration Constants
//----------------------------

// Test dataset sizes
inline constexpr int GIN_TEST_SMALL_DATASET = 100;
inline constexpr int GIN_TEST_MEDIUM_DATASET = 1000;
inline constexpr int GIN_TEST_LARGE_DATASET = 10000;
inline constexpr int GIN_TEST_XLARGE_DATASET = 100000;

// Test performance thresholds (in seconds)
inline constexpr double GIN_TEST_FAST_THRESHOLD = 0.001;      // 1ms
inline constexpr double GIN_TEST_MODERATE_THRESHOLD = 0.01;   // 10ms
inline constexpr double GIN_TEST_SLOW_THRESHOLD = 0.1;        // 100ms
inline constexpr double GIN_TEST_TIMEOUT_THRESHOLD = 1.0;     // 1 second

// Test text samples for tokenization
inline constexpr const char* GIN_TEST_SIMPLE_TEXT = "hello world";
inline constexpr const char* GIN_TEST_MEDIUM_TEXT = "The quick brown fox jumps over the lazy dog";
inline constexpr const char* GIN_TEST_COMPLEX_TEXT = "This is a more complex text with punctuation, numbers 123, and symbols @#$%";
inline constexpr const char* GIN_TEST_UNICODE_TEXT = "café naïve résumé Москва 北京 العربية";
inline constexpr const char* GIN_TEST_LONG_TEXT = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

// Test token lists
inline constexpr const char* GIN_TEST_SINGLE_TOKEN = "single";
inline constexpr const char* GIN_TEST_DUPLICATE_TOKENS = "test test test duplicate duplicate";
inline constexpr const char* GIN_TEST_STOPWORDS = "the and or but in on at to from with by";
inline constexpr const char* GIN_TEST_MIXED_CASE = "MiXeD CaSe TeXt With UPPER and lower";
inline constexpr const char* GIN_TEST_PUNCTUATION = "Hello, world! How are you? Fine, thanks.";

// Test selectivity ranges
inline constexpr double GIN_TEST_MIN_SELECTIVITY = 0.000001;  // Very selective
inline constexpr double GIN_TEST_LOW_SELECTIVITY = 0.001;     // Low selectivity
inline constexpr double GIN_TEST_MED_SELECTIVITY = 0.01;      // Medium selectivity
inline constexpr double GIN_TEST_HIGH_SELECTIVITY = 0.1;      // High selectivity
inline constexpr double GIN_TEST_MAX_SELECTIVITY = 1.0;       // No selectivity

// Test cost ranges
inline constexpr double GIN_TEST_MIN_COST = 0.1;              // Minimum reasonable cost
inline constexpr double GIN_TEST_LOW_COST = 1.0;              // Low cost threshold
inline constexpr double GIN_TEST_MED_COST = 5.0;              // Medium cost threshold
inline constexpr double GIN_TEST_HIGH_COST = 20.0;            // High cost threshold
inline constexpr double GIN_TEST_MAX_COST = 100.0;            // Maximum reasonable cost

// Test token count limits
inline constexpr int GIN_TEST_MIN_TOKENS = 1;
inline constexpr int GIN_TEST_OPTIMAL_TOKENS = 3;
inline constexpr int GIN_TEST_MAX_TOKENS = 50;
inline constexpr int GIN_TEST_STRESS_TOKENS = 1000;

// Test error tolerances
inline constexpr double GIN_TEST_FLOAT_TOLERANCE = 0.0001;    // Floating point comparison tolerance
inline constexpr double GIN_TEST_PERCENTAGE_TOLERANCE = 0.05; // 5% tolerance for performance tests

//----------------------------
// Test Utility Macros
//----------------------------

#define GIN_TEST_EXPECT_RANGE(value, min_val, max_val, test_name) \
    do { \
        if ((value) < (min_val) || (value) > (max_val)) { \
            std::cout << "[FAIL] " << test_name << ": value " << (value) \
                      << " not in range [" << (min_val) << ", " << (max_val) << "]" << std::endl; \
            return false; \
        } \
    } while(0)

#define GIN_TEST_EXPECT_CLOSE(actual, expected, tolerance, test_name) \
    do { \
        double diff = std::abs((actual) - (expected)); \
        if (diff > (tolerance)) { \
            std::cout << "[FAIL] " << test_name << ": expected " << (expected) \
                      << ", got " << (actual) << " (diff: " << diff << ")" << std::endl; \
            return false; \
        } \
    } while(0)

#define GIN_TEST_MEASURE_TIME(code_block, elapsed_time) \
    do { \
        auto start_time = std::chrono::high_resolution_clock::now(); \
        code_block; \
        auto end_time = std::chrono::high_resolution_clock::now(); \
        elapsed_time = std::chrono::duration<double>(end_time - start_time).count(); \
    } while(0)

//----------------------------
// Test Data Structures
//----------------------------

struct GinTestCase
{
    const char* name;
    const char* description;
    const char* input_text;
    int expected_token_count;
    double expected_cost_min;
    double expected_cost_max;
    double expected_selectivity_min;
    double expected_selectivity_max;
    bool should_pass;
};

struct GinPerformanceTest
{
    const char* name;
    int dataset_size;
    double timeout_seconds;
    double expected_throughput;  // operations per second
    bool memory_intensive;
};

struct GinComparisonTest
{
    const char* name;
    GinQueryType query_type;
    const char* test_text;
    bool gin_should_win;        // vs B-tree
    double cost_improvement;    // expected improvement ratio
};

//----------------------------
// Test Dataset Arrays
//----------------------------

static const GinTestCase GIN_BASIC_TEST_CASES[] = {
    {
        "empty_string",
        "Test empty string handling",
        "",
        0,
        GIN_TEST_MIN_COST,
        GIN_TEST_HIGH_COST,
        GIN_TEST_MAX_SELECTIVITY,
        GIN_TEST_MAX_SELECTIVITY,
        true
    },
    {
        "single_word", 
        "Test single word tokenization",
        GIN_TEST_SINGLE_TOKEN,
        1,
        GIN_TEST_MIN_COST,
        GIN_TEST_MED_COST,
        GIN_TEST_MIN_SELECTIVITY,
        GIN_TEST_HIGH_SELECTIVITY,
        true
    },
    {
        "simple_phrase",
        "Test simple phrase",
        GIN_TEST_SIMPLE_TEXT,
        2,
        GIN_TEST_MIN_COST,
        GIN_TEST_MED_COST,
        GIN_TEST_MIN_SELECTIVITY,
        GIN_TEST_MED_SELECTIVITY,
        true
    },
    {
        "complex_text",
        "Test complex text with punctuation",
        GIN_TEST_COMPLEX_TEXT,
        10,  // approximate
        GIN_TEST_LOW_COST,
        GIN_TEST_HIGH_COST,
        GIN_TEST_MIN_SELECTIVITY,
        GIN_TEST_LOW_SELECTIVITY,
        true
    },
    {
        "unicode_text",
        "Test Unicode text handling",
        GIN_TEST_UNICODE_TEXT,
        4,   // approximate
        GIN_TEST_LOW_COST,
        GIN_TEST_MED_COST,
        GIN_TEST_MIN_SELECTIVITY,
        GIN_TEST_MED_SELECTIVITY,
        true
    }
};

static const GinPerformanceTest GIN_PERFORMANCE_TESTS[] = {
    {
        "small_dataset",
        GIN_TEST_SMALL_DATASET,
        GIN_TEST_MODERATE_THRESHOLD,
        1000.0,  // 1000 ops/sec
        false
    },
    {
        "medium_dataset",
        GIN_TEST_MEDIUM_DATASET,
        GIN_TEST_SLOW_THRESHOLD,
        500.0,   // 500 ops/sec
        false
    },
    {
        "large_dataset",
        GIN_TEST_LARGE_DATASET,
        GIN_TEST_TIMEOUT_THRESHOLD,
        100.0,   // 100 ops/sec
        true
    }
};

static const GinComparisonTest GIN_COMPARISON_TESTS[] = {
    {
        "contains_vs_btree",
        GIN_QUERY_CONTAINS,
        GIN_TEST_MEDIUM_TEXT,
        true,
        2.0  // GIN should be 2x better
    },
    {
        "phrase_vs_btree",
        GIN_QUERY_PHRASE,
        GIN_TEST_MEDIUM_TEXT,
        true,
        3.0  // GIN should be 3x better for phrases
    },
    {
        "similarity_vs_btree",
        GIN_QUERY_SIMILAR,
        GIN_TEST_MEDIUM_TEXT,
        true,
        1.5  // GIN should be 1.5x better for similarity
    }
};

//----------------------------
// Test Configuration Functions
//----------------------------

inline int getTestCaseCount() 
{
    return sizeof(GIN_BASIC_TEST_CASES) / sizeof(GIN_BASIC_TEST_CASES[0]);
}

inline int getPerformanceTestCount()
{
    return sizeof(GIN_PERFORMANCE_TESTS) / sizeof(GIN_PERFORMANCE_TESTS[0]);
}

inline int getComparisonTestCount()
{
    return sizeof(GIN_COMPARISON_TESTS) / sizeof(GIN_COMPARISON_TESTS[0]);
}

inline const GinTestCase* getTestCase(int index)
{
    if (index >= 0 && index < getTestCaseCount())
        return &GIN_BASIC_TEST_CASES[index];
    return nullptr;
}

inline const GinPerformanceTest* getPerformanceTest(int index)
{
    if (index >= 0 && index < getPerformanceTestCount())
        return &GIN_PERFORMANCE_TESTS[index];
    return nullptr;
}

inline const GinComparisonTest* getComparisonTest(int index)
{
    if (index >= 0 && index < getComparisonTestCount())
        return &GIN_COMPARISON_TESTS[index];
    return nullptr;
}

//----------------------------
// Test Environment Settings
//----------------------------

// Enable verbose test output
#ifndef GIN_TEST_VERBOSE
#define GIN_TEST_VERBOSE 0
#endif

// Enable performance testing
#ifndef GIN_TEST_PERFORMANCE
#define GIN_TEST_PERFORMANCE 1
#endif

// Enable memory testing
#ifndef GIN_TEST_MEMORY
#define GIN_TEST_MEMORY 1
#endif

// Enable stress testing
#ifndef GIN_TEST_STRESS
#define GIN_TEST_STRESS 0
#endif

// Test result file paths
#ifndef GIN_TEST_LOG_FILE
#define GIN_TEST_LOG_FILE "gin_test_results.log"
#endif

#ifndef GIN_TEST_PERF_FILE
#define GIN_TEST_PERF_FILE "gin_performance_results.log"
#endif

#endif // JRD_GIN_TEST_CONFIG_H