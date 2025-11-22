/**
 * LSM Bloom Filter Implementation
 *
 * Based on:
 * - Standard Bloom filter algorithm
 * - FNV-1a hash function (fast, good distribution)
 * - LSM_TREE_SPEC.md Section 6: Bloom Filters
 *
 * Performance:
 * - add(): O(k) where k = number of hash functions (~7 for 1% FPR)
 * - mightContain(): O(k)
 * - Memory: ~10 bits per key for 1% FPR (~1.25 bytes per key)
 *
 * Example:
 * - 100K keys, 1% FPR → 120KB Bloom filter
 * - Saves 99%+ disk reads for non-existent keys
 */

#include "scratchbird/core/lsm_bloom_filter.h"
#include <algorithm>

namespace scratchbird
{
namespace core
{

// ============================================================================
// Constructor
// ============================================================================

LSMBloomFilter::LSMBloomFilter(size_t estimated_num_keys, double false_positive_rate)
    : num_keys_(estimated_num_keys),
      num_bits_(calculateNumBits(estimated_num_keys, false_positive_rate)),
      num_hashes_(calculateNumHashes(num_bits_, estimated_num_keys)),
      bits_((num_bits_ + 7) / 8, 0)  // Round up to nearest byte
{
    // Ensure at least 1 hash function
    if (num_hashes_ == 0) {
        num_hashes_ = 1;
    }

    // Ensure at least 8 bits (1 byte)
    if (num_bits_ == 0) {
        num_bits_ = 8;
        bits_.resize(1, 0);
    }
}

// ============================================================================
// Core Operations
// ============================================================================

void LSMBloomFilter::add(const std::vector<uint8_t>& key)
{
    // Set k bits (one per hash function)
    for (size_t i = 0; i < num_hashes_; i++)
    {
        uint64_t h = hash(key, i);
        size_t bit_pos = h % num_bits_;
        size_t byte_pos = bit_pos / 8;
        size_t bit_offset = bit_pos % 8;

        bits_[byte_pos] |= (1 << bit_offset);
    }
}

bool LSMBloomFilter::mightContain(const std::vector<uint8_t>& key) const
{
    // Check all k bits
    for (size_t i = 0; i < num_hashes_; i++)
    {
        uint64_t h = hash(key, i);
        size_t bit_pos = h % num_bits_;
        size_t byte_pos = bit_pos / 8;
        size_t bit_offset = bit_pos % 8;

        // If any bit is 0, key is definitely NOT present
        if ((bits_[byte_pos] & (1 << bit_offset)) == 0)
        {
            return false;
        }
    }

    // All bits are 1 → key might be present (or false positive)
    return true;
}

// ============================================================================
// Serialization
// ============================================================================

void LSMBloomFilter::serialize(std::vector<uint8_t>* out) const
{
    if (!out)
    {
        return;
    }

    out->clear();

    // Format: [num_keys (8 bytes)][num_bits (8 bytes)][num_hashes (8 bytes)][bits array (variable)]

    // num_keys
    out->insert(out->end(), (uint8_t*)&num_keys_, (uint8_t*)&num_keys_ + sizeof(num_keys_));

    // num_bits
    out->insert(out->end(), (uint8_t*)&num_bits_, (uint8_t*)&num_bits_ + sizeof(num_bits_));

    // num_hashes
    out->insert(out->end(), (uint8_t*)&num_hashes_, (uint8_t*)&num_hashes_ + sizeof(num_hashes_));

    // bits array
    out->insert(out->end(), bits_.begin(), bits_.end());
}

LSMBloomFilter* LSMBloomFilter::deserialize(const std::vector<uint8_t>& data)
{
    // Minimum size: 3 * 8 bytes (num_keys, num_bits, num_hashes)
    if (data.size() < 24)
    {
        return nullptr;
    }

    size_t pos = 0;

    // Read num_keys
    size_t num_keys;
    std::memcpy(&num_keys, data.data() + pos, sizeof(num_keys));
    pos += sizeof(num_keys);

    // Read num_bits
    size_t num_bits;
    std::memcpy(&num_bits, data.data() + pos, sizeof(num_bits));
    pos += sizeof(num_bits);

    // Read num_hashes
    size_t num_hashes;
    std::memcpy(&num_hashes, data.data() + pos, sizeof(num_hashes));
    pos += sizeof(num_hashes);

    // Calculate expected bits array size
    size_t expected_bits_size = (num_bits + 7) / 8;
    if (data.size() != pos + expected_bits_size)
    {
        return nullptr;  // Size mismatch
    }

    // Create Bloom filter with dummy FPR (will be overridden)
    LSMBloomFilter* bf = new LSMBloomFilter(num_keys, 0.01);

    // Override calculated values with deserialized values
    bf->num_bits_ = num_bits;
    bf->num_hashes_ = num_hashes;

    // Copy bits array
    bf->bits_.resize(expected_bits_size);
    std::memcpy(bf->bits_.data(), data.data() + pos, expected_bits_size);

    return bf;
}

// ============================================================================
// Helper Methods (Private)
// ============================================================================

size_t LSMBloomFilter::calculateNumBits(size_t n, double p)
{
    if (n == 0 || p <= 0.0 || p >= 1.0)
    {
        return 64;  // Default to 64 bits
    }

    // Formula: m = -n * ln(p) / (ln(2)^2)
    double m = -static_cast<double>(n) * std::log(p) / (std::log(2.0) * std::log(2.0));

    // Round up to nearest integer
    size_t num_bits = static_cast<size_t>(std::ceil(m));

    // Ensure at least 8 bits (1 byte)
    if (num_bits < 8)
    {
        num_bits = 8;
    }

    return num_bits;
}

size_t LSMBloomFilter::calculateNumHashes(size_t m, size_t n)
{
    if (n == 0)
    {
        return 1;  // Default to 1 hash function
    }

    // Formula: k = (m / n) * ln(2)
    double k = (static_cast<double>(m) / static_cast<double>(n)) * std::log(2.0);

    // Round to nearest integer
    size_t num_hashes = static_cast<size_t>(std::round(k));

    // Ensure at least 1 hash function
    if (num_hashes == 0)
    {
        num_hashes = 1;
    }

    // Limit to reasonable maximum (e.g., 20 hash functions)
    if (num_hashes > 20)
    {
        num_hashes = 20;
    }

    return num_hashes;
}

uint64_t LSMBloomFilter::hash(const std::vector<uint8_t>& key, size_t seed) const
{
    // FNV-1a hash with seed
    // See: http://www.isthe.com/chongo/tech/comp/fnv/
    //
    // FNV-1a is chosen for:
    // - Speed: Very fast, no complex operations
    // - Good distribution: Suitable for Bloom filters
    // - No external dependencies: Pure C++
    //
    // Alternative hash functions (not implemented here):
    // - MurmurHash3: Faster, better distribution, more complex
    // - xxHash: Fastest, excellent distribution, requires external library
    // - CityHash: Google's hash, optimized for x86, requires external library

    // FNV-1a constants
    const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    const uint64_t FNV_PRIME = 1099511628211ULL;

    // Initialize hash with offset basis + seed
    uint64_t h = FNV_OFFSET_BASIS + seed;

    // Process each byte
    for (uint8_t byte : key)
    {
        h ^= byte;       // XOR with byte
        h *= FNV_PRIME;  // Multiply by FNV prime
    }

    return h;
}

} // namespace core
} // namespace scratchbird
