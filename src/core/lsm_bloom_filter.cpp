/**
 * LSM Bloom Filter Implementation
 *
 * Probabilistic membership test for LSM-Tree SSTables
 *
 * Algorithm:
 * - Uses k hash functions to set/check bits in bit array
 * - Formula for optimal parameters (false positive rate p, n keys):
 *   - m (bits) = -n * ln(p) / (ln(2)^2)
 *   - k (hashes) = (m / n) * ln(2)
 *
 * Hash Function: FNV-1a with seed variation
 * - Fast, good distribution
 * - Multiple hashes via different seeds
 */

#include "scratchbird/core/lsm_tree.h"
#include <algorithm>

namespace scratchbird
{
namespace core
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

LSMBloomFilter::LSMBloomFilter(size_t expected_keys, double false_positive_rate)
{
    // Calculate optimal parameters
    num_bits_ = calculateNumBits(expected_keys, false_positive_rate);
    num_hashes_ = calculateNumHashes(num_bits_, expected_keys);

    // Allocate bit array (rounded up to bytes)
    size_t num_bytes = (num_bits_ + 7) / 8;
    bits_.resize(num_bytes, 0);
}

LSMBloomFilter::~LSMBloomFilter()
{
    // std::vector handles cleanup
}

// ============================================================================
// Core Operations
// ============================================================================

void LSMBloomFilter::add(const std::vector<uint8_t> &key)
{
    for (size_t i = 0; i < num_hashes_; i++)
    {
        uint64_t h = hash(key, i);
        size_t bit_pos = h % num_bits_;
        size_t byte_pos = bit_pos / 8;
        size_t bit_offset = bit_pos % 8;
        bits_[byte_pos] |= (1 << bit_offset);
    }
}

bool LSMBloomFilter::mightContain(const std::vector<uint8_t> &key) const
{
    for (size_t i = 0; i < num_hashes_; i++)
    {
        uint64_t h = hash(key, i);
        size_t bit_pos = h % num_bits_;
        size_t byte_pos = bit_pos / 8;
        size_t bit_offset = bit_pos % 8;

        if ((bits_[byte_pos] & (1 << bit_offset)) == 0)
        {
            return false;  // Definitely not present
        }
    }
    return true;  // Possibly present (might be false positive)
}

// ============================================================================
// Serialization / Deserialization
// ============================================================================

void LSMBloomFilter::serialize(std::vector<uint8_t> *output) const
{
    output->clear();
    output->reserve(16 + bits_.size());

    // Write num_bits (8 bytes)
    for (int i = 0; i < 8; i++)
    {
        output->push_back((num_bits_ >> (i * 8)) & 0xFF);
    }

    // Write num_hashes (8 bytes)
    for (int i = 0; i < 8; i++)
    {
        output->push_back((num_hashes_ >> (i * 8)) & 0xFF);
    }

    // Write bit array
    output->insert(output->end(), bits_.begin(), bits_.end());
}

LSMBloomFilter *LSMBloomFilter::deserialize(const std::vector<uint8_t> &data)
{
    if (data.size() < 16)
    {
        return nullptr;  // Invalid data
    }

    // Read num_bits (8 bytes)
    size_t num_bits = 0;
    for (int i = 0; i < 8; i++)
    {
        num_bits |= (static_cast<size_t>(data[i]) << (i * 8));
    }

    // Read num_hashes (8 bytes)
    size_t num_hashes = 0;
    for (int i = 0; i < 8; i++)
    {
        num_hashes |= (static_cast<size_t>(data[8 + i]) << (i * 8));
    }

    // Calculate expected size
    size_t expected_bytes = (num_bits + 7) / 8;
    if (data.size() != 16 + expected_bytes)
    {
        return nullptr;  // Size mismatch
    }

    // Create bloom filter with dummy parameters (will be overwritten)
    LSMBloomFilter *bf = new LSMBloomFilter(1, 0.01);
    bf->num_bits_ = num_bits;
    bf->num_hashes_ = num_hashes;
    bf->bits_.assign(data.begin() + 16, data.end());

    return bf;
}

size_t LSMBloomFilter::getSizeBytes() const
{
    return bits_.size();
}

// ============================================================================
// Private Helper Methods
// ============================================================================

size_t LSMBloomFilter::calculateNumBits(size_t n, double p)
{
    if (n == 0 || p <= 0.0 || p >= 1.0)
    {
        return 1000;  // Default fallback
    }

    // m = -n * ln(p) / (ln(2)^2)
    double m = -static_cast<double>(n) * std::log(p) / (std::log(2.0) * std::log(2.0));
    return std::max(static_cast<size_t>(m), static_cast<size_t>(8));  // Minimum 8 bits
}

size_t LSMBloomFilter::calculateNumHashes(size_t m, size_t n)
{
    if (n == 0)
    {
        return 1;  // Default fallback
    }

    // k = (m / n) * ln(2)
    double k = (static_cast<double>(m) / static_cast<double>(n)) * std::log(2.0);
    return std::max(static_cast<size_t>(k), static_cast<size_t>(1));  // Minimum 1 hash
}

uint64_t LSMBloomFilter::hash(const std::vector<uint8_t> &key, size_t seed) const
{
    // FNV-1a hash with seed variation
    // FNV offset basis: 14695981039346656037
    // FNV prime: 1099511628211
    uint64_t h = 14695981039346656037ULL + seed;

    for (uint8_t byte : key)
    {
        h ^= byte;
        h *= 1099511628211ULL;
    }

    return h;
}

} // namespace core
} // namespace scratchbird
