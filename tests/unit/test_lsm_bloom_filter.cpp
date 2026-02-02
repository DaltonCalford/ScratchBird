/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Unit Tests for LSM Bloom Filter
 *
 * Test coverage:
 * 1. Basic add/mightContain operations
 * 2. False positive rate measurement
 * 3. Serialization/deserialization
 * 4. Different bit sizes (false positive rates)
 * 5. Large datasets (100K keys)
 */

#include "scratchbird/core/lsm_tree.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <iomanip>

using namespace scratchbird::core;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Generate random key
 */
std::vector<uint8_t> generateKey(size_t index)
{
    std::string key_str = "key_" + std::to_string(index);
    return std::vector<uint8_t>(key_str.begin(), key_str.end());
}

/**
 * Generate random absent key (not in the set)
 */
std::vector<uint8_t> generateAbsentKey(size_t index)
{
    std::string key_str = "absent_key_" + std::to_string(index);
    return std::vector<uint8_t>(key_str.begin(), key_str.end());
}

// ============================================================================
// Test Cases
// ============================================================================

/**
 * Test 1: Basic functionality - Add 1000 keys and verify all present
 */
void testBasicFunctionality()
{
    std::cout << "\n=== Test 1: Basic Functionality ===\n";

    LSMBloomFilter bf(1000, 0.01);  // 1000 keys, 1% false positive rate

    // Add 1000 keys
    for (size_t i = 0; i < 1000; i++)
    {
        std::vector<uint8_t> key = generateKey(i);
        bf.add(key);
    }

    // Verify all added keys are detected
    size_t true_positives = 0;
    for (size_t i = 0; i < 1000; i++)
    {
        std::vector<uint8_t> key = generateKey(i);
        if (bf.mightContain(key))
        {
            true_positives++;
        }
    }

    std::cout << "  ✓ Added 1000 keys\n";
    std::cout << "  ✓ True positives: " << true_positives << "/1000";

    if (true_positives == 1000)
    {
        std::cout << " (ALL keys detected)\n";
    }
    else
    {
        std::cout << " (ERROR: Some keys missed!)\n";
        exit(1);
    }
}

/**
 * Test 2: False positive rate measurement
 */
void testFalsePositiveRate()
{
    std::cout << "\n=== Test 2: False Positive Rate Measurement ===\n";

    LSMBloomFilter bf(1000, 0.01);  // 1000 keys, 1% target FPR

    // Add 1000 keys
    for (size_t i = 0; i < 1000; i++)
    {
        std::vector<uint8_t> key = generateKey(i);
        bf.add(key);
    }

    // Check 1000 absent keys
    size_t false_positives = 0;
    for (size_t i = 0; i < 1000; i++)
    {
        std::vector<uint8_t> key = generateAbsentKey(i);
        if (bf.mightContain(key))
        {
            false_positives++;
        }
    }

    double fpr = static_cast<double>(false_positives) / 1000.0;
    std::cout << "  ✓ Checked 1000 absent keys\n";
    std::cout << "  ✓ False positives: " << false_positives << "/1000";
    std::cout << " (" << std::fixed << std::setprecision(2) << (fpr * 100.0) << "%)\n";
    std::cout << "  ✓ Target: <2% (expected ~1%)\n";

    if (fpr > 0.02)
    {
        std::cout << "  ERROR: False positive rate too high!\n";
        exit(1);
    }
}

/**
 * Test 3: Serialization and deserialization
 */
void testSerialization()
{
    std::cout << "\n=== Test 3: Serialization/Deserialization ===\n";

    // Create and populate bloom filter
    LSMBloomFilter bf1(1000, 0.01);
    for (size_t i = 0; i < 1000; i++)
    {
        std::vector<uint8_t> key = generateKey(i);
        bf1.add(key);
    }

    // Serialize
    std::vector<uint8_t> serialized;
    bf1.serialize(&serialized);
    std::cout << "  ✓ Serialized bloom filter: " << serialized.size() << " bytes\n";

    // Deserialize
    LSMBloomFilter *bf2 = LSMBloomFilter::deserialize(serialized);
    if (bf2 == nullptr)
    {
        std::cout << "  ERROR: Deserialization failed!\n";
        exit(1);
    }
    std::cout << "  ✓ Deserialized bloom filter\n";

    // Verify all keys still detected after deserialization
    size_t detected = 0;
    for (size_t i = 0; i < 1000; i++)
    {
        std::vector<uint8_t> key = generateKey(i);
        if (bf2->mightContain(key))
        {
            detected++;
        }
    }

    std::cout << "  ✓ Keys detected after deserialization: " << detected << "/1000\n";

    if (detected != 1000)
    {
        std::cout << "  ERROR: Some keys lost during serialization!\n";
        delete bf2;
        exit(1);
    }

    delete bf2;
}

/**
 * Test 4: Different bit sizes (false positive rates)
 */
void testDifferentBitSizes()
{
    std::cout << "\n=== Test 4: Different Bit Sizes ===\n";

    struct TestCase
    {
        size_t expected_keys;
        double fpr;
        const char *description;
    };

    TestCase test_cases[] = {
        {1000, 0.10, "Low precision (10% FPR)"},
        {1000, 0.01, "Medium precision (1% FPR)"},
        {1000, 0.001, "High precision (0.1% FPR)"}
    };

    for (const auto &tc : test_cases)
    {
        LSMBloomFilter bf(tc.expected_keys, tc.fpr);

        // Add keys
        for (size_t i = 0; i < tc.expected_keys; i++)
        {
            std::vector<uint8_t> key = generateKey(i);
            bf.add(key);
        }

        // Measure actual FPR
        size_t false_positives = 0;
        for (size_t i = 0; i < tc.expected_keys; i++)
        {
            std::vector<uint8_t> key = generateAbsentKey(i);
            if (bf.mightContain(key))
            {
                false_positives++;
            }
        }

        double actual_fpr = static_cast<double>(false_positives) / tc.expected_keys;
        std::cout << "  ✓ " << tc.description << ": ";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << (actual_fpr * 100.0) << "% actual";
        std::cout << " (size: " << bf.getSizeBytes() << " bytes)\n";
    }
}

/**
 * Test 5: Large dataset (100K keys)
 */
void testLargeDataset()
{
    std::cout << "\n=== Test 5: Large Dataset (100K keys) ===\n";

    LSMBloomFilter bf(100000, 0.01);  // 100K keys, 1% FPR

    std::cout << "  Adding 100,000 keys...\n";
    for (size_t i = 0; i < 100000; i++)
    {
        std::vector<uint8_t> key = generateKey(i);
        bf.add(key);
    }
    std::cout << "  ✓ Added 100,000 keys\n";

    // Verify all keys detected
    std::cout << "  Verifying keys...\n";
    size_t detected = 0;
    for (size_t i = 0; i < 100000; i++)
    {
        std::vector<uint8_t> key = generateKey(i);
        if (bf.mightContain(key))
        {
            detected++;
        }
    }
    std::cout << "  ✓ Keys detected: " << detected << "/100000\n";

    if (detected != 100000)
    {
        std::cout << "  ERROR: Some keys missed!\n";
        exit(1);
    }

    // Measure FPR with 10K absent keys
    std::cout << "  Measuring false positive rate (10K absent keys)...\n";
    size_t false_positives = 0;
    for (size_t i = 0; i < 10000; i++)
    {
        std::vector<uint8_t> key = generateAbsentKey(i);
        if (bf.mightContain(key))
        {
            false_positives++;
        }
    }

    double fpr = static_cast<double>(false_positives) / 10000.0;
    std::cout << "  ✓ False positive rate: " << std::fixed << std::setprecision(2);
    std::cout << (fpr * 100.0) << "%\n";
    std::cout << "  ✓ Bloom filter size: " << bf.getSizeBytes() << " bytes\n";

    if (fpr > 0.02)
    {
        std::cout << "  ERROR: False positive rate too high!\n";
        exit(1);
    }
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main()
{
    std::cout << "========================================\n";
    std::cout << "   LSM Bloom Filter - Unit Tests\n";
    std::cout << "========================================\n";

    testBasicFunctionality();
    testFalsePositiveRate();
    testSerialization();
    testDifferentBitSizes();
    testLargeDataset();

    std::cout << "\n========================================\n";
    std::cout << "  ✅ ALL BLOOM FILTER TESTS PASSED\n";
    std::cout << "========================================\n";

    return;
}
