/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

/**
 * LSM Bloom Filter - Probabilistic data structure for LSM-Tree
 *
 * Purpose: Reduce disk I/O by quickly determining if a key is NOT present
 * in an SSTable before reading from disk.
 *
 * Properties:
 * - No false negatives (if key present, always returns true)
 * - Configurable false positive rate (default: 1%)
 * - Space-efficient (typically 10 bits per key for 1% FPR)
 * - Fast lookups (k hash computations, no disk I/O)
 *
 * Typical usage:
 * - SSTableWriter creates Bloom filter during SSTable write
 * - SSTableReader loads Bloom filter when opening SSTable
 * - get() checks Bloom filter before disk read (90%+ I/O reduction)
 *
 * Based on:
 * - Firebird MGA architecture (no WAL, TIP-based recovery)
 * - RocksDB/LevelDB Bloom filter implementation
 * - LSM_TREE_SPEC.md Section 6
 */

#include <vector>
#include <cstdint>
#include <cmath>
#include <cstring>

namespace scratchbird
{
namespace core
{

/**
 * LSMBloomFilter - Bloom filter for LSM-Tree SSTables
 *
 * Formula for optimal parameters:
 * - m = -n * ln(p) / (ln(2)^2)   (number of bits)
 * - k = (m / n) * ln(2)          (number of hash functions)
 *
 * Where:
 * - n = expected number of keys
 * - p = target false positive rate
 * - m = number of bits in bit array
 * - k = number of hash functions
 *
 * Example: 1000 keys, 1% FPR → 10 bits/key = 1,250 bytes, 7 hash functions
 */
class LSMBloomFilter
{
public:
    /**
     * Constructor
     * @param estimated_num_keys Expected number of keys to insert
     * @param false_positive_rate Target FPR (default: 0.01 = 1%)
     */
    LSMBloomFilter(size_t estimated_num_keys, double false_positive_rate = 0.01);

    /**
     * Add key to Bloom filter
     * @param key Key to add
     */
    void add(const std::vector<uint8_t>& key);

    /**
     * Check if key might be present
     * @param key Key to check
     * @return true if key might be present (or false positive)
     *         false if key is definitely NOT present
     */
    bool mightContain(const std::vector<uint8_t>& key) const;

    /**
     * Serialize Bloom filter to byte array (for SSTable footer)
     * Format: [num_keys][num_bits][num_hashes][bits_array]
     * @param out Output buffer
     */
    void serialize(std::vector<uint8_t>* out) const;

    /**
     * Deserialize Bloom filter from byte array
     * @param data Serialized Bloom filter data
     * @return Pointer to deserialized Bloom filter, or nullptr on error
     */
    static LSMBloomFilter* deserialize(const std::vector<uint8_t>& data);

    /**
     * Get Bloom filter size in bytes
     * @return Size in bytes (bits array + metadata)
     */
    size_t getSizeBytes() const {
        return bits_.size() + sizeof(num_keys_) + sizeof(num_bits_) + sizeof(num_hashes_);
    }

    /**
     * Get number of bits in bit array
     */
    size_t getNumBits() const { return num_bits_; }

    /**
     * Get number of hash functions
     */
    size_t getNumHashes() const { return num_hashes_; }

    /**
     * Get expected number of keys
     */
    size_t getNumKeys() const { return num_keys_; }

private:
    size_t num_keys_;      // Expected number of keys
    size_t num_bits_;      // Number of bits in bit array
    size_t num_hashes_;    // Number of hash functions
    std::vector<uint8_t> bits_;  // Bit array (m bits packed into bytes)

    /**
     * Calculate optimal number of bits
     * Formula: m = -n * ln(p) / (ln(2)^2)
     * @param n Number of keys
     * @param p False positive rate
     * @return Number of bits needed
     */
    static size_t calculateNumBits(size_t n, double p);

    /**
     * Calculate optimal number of hash functions
     * Formula: k = (m / n) * ln(2)
     * @param m Number of bits
     * @param n Number of keys
     * @return Number of hash functions
     */
    static size_t calculateNumHashes(size_t m, size_t n);

    /**
     * Hash function using FNV-1a with seed
     * Fast, good distribution, no external dependencies
     *
     * Alternative: MurmurHash3, xxHash (faster but more complex)
     *
     * @param key Key to hash
     * @param seed Hash seed (0 to k-1)
     * @return 64-bit hash value
     */
    uint64_t hash(const std::vector<uint8_t>& key, size_t seed) const;
};

} // namespace core
} // namespace scratchbird
