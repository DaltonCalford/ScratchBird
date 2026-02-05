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

#include <cstdint>
#include <vector>
#include <cstring>
#include <algorithm>

namespace scratchbird::core
{

/**
 * BRIN Minmax Operator Class
 *
 * Provides min/max summary functions for BRIN indexes.
 * Supports comparison of values to determine range overlap.
 */
class BrinMinmaxOps
{
public:
    /**
     * Compare two values
     * @return <0 if a < b, 0 if a == b, >0 if a > b
     */
    static int compare(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
    {
        // Lexicographic byte comparison (assumes big-endian encoding for numeric values)
        size_t min_len = std::min(a.size(), b.size());

        int result = std::memcmp(a.data(), b.data(), min_len);
        if (result != 0)
            return result;

        // If prefixes are equal, shorter is less than longer
        if (a.size() < b.size())
            return -1;
        else if (a.size() > b.size())
            return 1;
        else
            return 0;
    }

    /**
     * Update min value
     * Returns true if min was updated
     */
    static bool updateMin(std::vector<uint8_t>& current_min,
                         const std::vector<uint8_t>& new_value)
    {
        if (current_min.empty() || compare(new_value, current_min) < 0)
        {
            current_min = new_value;
            return true;
        }
        return false;
    }

    /**
     * Update max value
     * Returns true if max was updated
     */
    static bool updateMax(std::vector<uint8_t>& current_max,
                         const std::vector<uint8_t>& new_value)
    {
        if (current_max.empty() || compare(new_value, current_max) > 0)
        {
            current_max = new_value;
            return true;
        }
        return false;
    }

    /**
     * Check if a range [range_min, range_max] overlaps with query [query_min, query_max]
     *
     * Returns true if there could be matching values in the range.
     *
     * Overlap conditions:
     * 1. Range contains query_min: range_min <= query_min <= range_max
     * 2. Range contains query_max: range_min <= query_max <= range_max
     * 3. Query contains range: query_min <= range_min <= range_max <= query_max
     * 4. Range and query partially overlap
     *
     * Simplified: NOT (range_max < query_min OR range_min > query_max)
     */
    static bool rangeOverlaps(const std::vector<uint8_t>& range_min,
                             const std::vector<uint8_t>& range_max,
                             const std::vector<uint8_t>* query_min,
                             const std::vector<uint8_t>* query_max)
    {
        // Handle NULL boundaries (represent infinity)
        if (!query_min && !query_max)
        {
            // No bounds specified - matches everything
            return true;
        }

        if (query_max)
        {
            // Check if range_min > query_max (no overlap)
            if (compare(range_min, *query_max) > 0)
                return false;
        }

        if (query_min)
        {
            // Check if range_max < query_min (no overlap)
            if (compare(range_max, *query_min) < 0)
                return false;
        }

        return true;
    }

    /**
     * Serialize integer to bytes (for numeric BRIN indexes)
     */
    static std::vector<uint8_t> serializeInt64(int64_t value)
    {
        std::vector<uint8_t> result(sizeof(int64_t));
        std::memcpy(result.data(), &value, sizeof(int64_t));
        return result;
    }

    /**
     * Deserialize bytes to integer
     */
    static int64_t deserializeInt64(const std::vector<uint8_t>& data)
    {
        if (data.size() < sizeof(int64_t))
            return 0;

        int64_t value;
        std::memcpy(&value, data.data(), sizeof(int64_t));
        return value;
    }
};

} // namespace scratchbird::core
