#include "scratchbird/core/btree.h"
#include "scratchbird/core/btree_page.h"
#include <algorithm>
#include <cstring>

namespace scratchbird::core
{

    // Helper class for B-tree prefix compression
    class BTreeCompression
    {
    public:
        // Calculate common prefix length between two keys
        static uint16_t calculatePrefixLength(const std::vector<uint8_t> &key1,
                                              const std::vector<uint8_t> &key2)
        {
            size_t min_len = std::min(key1.size(), key2.size());
            uint16_t prefix_len = 0;

            for (size_t i = 0; i < min_len; i++)
            {
                if (key1[i] != key2[i])
                {
                    break;
                }
                prefix_len++;
            }

            return prefix_len;
        }

        // Calculate optimal prefix for a page
        // Returns the common prefix shared by all keys on the page
        static std::vector<uint8_t>
        calculatePagePrefix(const std::vector<std::vector<uint8_t>> &keys)
        {
            if (keys.empty())
            {
                return {};
            }

            if (keys.size() == 1)
            {
                return {}; // Single key - no compression benefit
            }

            // Find minimum prefix length across all consecutive pairs
            uint16_t min_prefix = 0xFFFF;

            for (size_t i = 0; i < keys.size() - 1; i++)
            {
                uint16_t prefix_len = calculatePrefixLength(keys[i], keys[i + 1]);
                min_prefix = std::min(min_prefix, prefix_len);
            }

            // Extract common prefix from first key
            if (min_prefix == 0 || min_prefix == 0xFFFF)
            {
                return {};
            }

            return std::vector<uint8_t>(keys[0].begin(), keys[0].begin() + min_prefix);
        }

        // Calculate space savings from prefix compression
        static uint32_t calculateSpaceSavings(const std::vector<std::vector<uint8_t>> &keys,
                                              uint16_t prefix_len)
        {
            if (prefix_len == 0 || keys.empty())
            {
                return 0;
            }

            // Each key saves prefix_len bytes
            // But we need to store the prefix once
            uint32_t savings = prefix_len * keys.size();
            uint32_t overhead = prefix_len; // Stored in page header area

            return (savings > overhead) ? (savings - overhead) : 0;
        }

        // Compress a key given a prefix
        static std::vector<uint8_t> compressKey(const std::vector<uint8_t> &key,
                                                const std::vector<uint8_t> &prefix)
        {
            if (prefix.empty() || key.size() < prefix.size())
            {
                return key;
            }

            // Verify prefix matches
            if (memcmp(key.data(), prefix.data(), prefix.size()) != 0)
            {
                // Prefix doesn't match - return full key
                return key;
            }

            // Return suffix (part after prefix)
            return std::vector<uint8_t>(key.begin() + prefix.size(), key.end());
        }

        // Decompress a key given a prefix
        static std::vector<uint8_t> decompressKey(const std::vector<uint8_t> &compressed_key,
                                                  const std::vector<uint8_t> &prefix)
        {
            if (prefix.empty())
            {
                return compressed_key;
            }

            // Concatenate prefix + suffix
            std::vector<uint8_t> full_key;
            full_key.reserve(prefix.size() + compressed_key.size());
            full_key.insert(full_key.end(), prefix.begin(), prefix.end());
            full_key.insert(full_key.end(), compressed_key.begin(), compressed_key.end());

            return full_key;
        }

        // Estimate if compression is worthwhile for a given set of keys
        static bool shouldCompress(const std::vector<std::vector<uint8_t>> &keys,
                                   uint16_t min_prefix_len = 4) // Minimum 4 bytes to be worth it
        {
            if (keys.size() < 2)
            {
                return false; // Need at least 2 keys
            }

            auto prefix = calculatePagePrefix(keys);

            if (prefix.size() < min_prefix_len)
            {
                return false; // Prefix too short
            }

            uint32_t savings = calculateSpaceSavings(keys, prefix.size());

            // Worthwhile if we save at least 32 bytes per page
            return savings >= 32;
        }

        // Calculate suffix truncation length
        // For internal nodes, we can truncate the suffix of separator keys
        static uint16_t calculateSuffixTruncation(const std::vector<uint8_t> &separator_key,
                                                  const std::vector<uint8_t> &next_key)
        {
            // Find the first byte where they differ
            size_t min_len = std::min(separator_key.size(), next_key.size());

            for (size_t i = 0; i < min_len; i++)
            {
                if (separator_key[i] != next_key[i])
                {
                    // We can truncate everything after position i+1
                    uint16_t keep_len = i + 1;
                    uint16_t trunc_len = separator_key.size() - keep_len;
                    return trunc_len;
                }
            }

            // Keys are identical up to min_len
            // For separator keys, we need at least one byte difference
            return 0;
        }

        // Truncate suffix from a separator key
        static std::vector<uint8_t> truncateSuffix(const std::vector<uint8_t> &key,
                                                   uint16_t suffix_trunc_len)
        {
            if (suffix_trunc_len == 0 || suffix_trunc_len >= key.size())
            {
                return key;
            }

            size_t keep_len = key.size() - suffix_trunc_len;
            return std::vector<uint8_t>(key.begin(), key.begin() + keep_len);
        }

        // Reconstruct full key from truncated separator
        // (Used during tree navigation)
        static std::vector<uint8_t>
        reconstructFromTruncated(const std::vector<uint8_t> &truncated_key,
                                 uint16_t original_length)
        {
            // For separator keys, we don't need full reconstruction
            // The truncated version is sufficient for comparison
            return truncated_key;
        }
    };

} // namespace scratchbird::core
