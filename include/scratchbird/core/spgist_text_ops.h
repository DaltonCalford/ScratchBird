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

#include "scratchbird/core/spgist_index.h"
#include <string>
#include <algorithm>
#include <cstring>

namespace scratchbird::core
{

/**
 * Text utilities for radix tree
 */
class TextUtils
{
public:
    /**
     * Find common prefix between two strings
     */
    static std::string commonPrefix(const std::string& a, const std::string& b)
    {
        size_t min_len = std::min(a.length(), b.length());
        size_t i = 0;

        while (i < min_len && a[i] == b[i])
        {
            ++i;
        }

        return a.substr(0, i);
    }

    /**
     * Serialize string to byte vector
     */
    static std::vector<uint8_t> serialize(const std::string& str)
    {
        std::vector<uint8_t> result(str.begin(), str.end());
        return result;
    }

    /**
     * Deserialize byte vector to string
     */
    static std::string deserialize(const std::vector<uint8_t>& data)
    {
        return std::string(data.begin(), data.end());
    }
};

/**
 * SP-GiST operator class for text using radix tree partitioning
 *
 * Implements radix tree (prefix tree): each inner node stores a common
 * prefix and children represent different continuations. Ideal for:
 * - Prefix search (LIKE 'abc%')
 * - Autocomplete
 * - Dictionary lookups
 *
 * Example tree structure:
 *
 * Root:
 *   prefix=""
 *   children: ['a', 'b', 'c']
 *
 * Node 'a':
 *   prefix="a"
 *   children: ['p', 'r', 't']
 *
 * Node 'ap':
 *   prefix="ap"
 *   children: ['p' (apple), 'p' (application)]
 *
 * Leaves:
 *   "apple"
 *   "application"
 *   "art"
 *   "arrow"
 */
class SPGiSTTextOperatorClass : public SPGiSTOperatorClass
{
public:
    static constexpr uint32_t OPCLASS_ID = 2;
    static constexpr const char* OPCLASS_NAME = "text_ops";

    uint32_t getOpClassId() const override { return OPCLASS_ID; }
    std::string getOpClassName() const override { return OPCLASS_NAME; }

    Config config() const override
    {
        Config cfg;
        cfg.canReturnData = true;    // Supports index-only scans
        cfg.labelSize = 0;            // Variable-size labels (characters/strings)
        cfg.maxInnerNodes = 256;      // Can have many children (one per character)
        return cfg;
    }

    SPGiSTTraversal choose(
        const std::vector<uint8_t>& innerPrefix,
        const std::vector<SPGiSTNodeLabel>& nodeLabels,
        const std::vector<uint8_t>& query) const override
    {
        std::string prefix_str = TextUtils::deserialize(innerPrefix);
        std::string query_str = TextUtils::deserialize(query);

        SPGiSTTraversal result;

        // Check if query starts with prefix
        if (query_str.length() < prefix_str.length() ||
            query_str.substr(0, prefix_str.length()) != prefix_str)
        {
            // Query doesn't match prefix, need to split node
            result.match_type = SPGiSTMatchType::MATCH_SPLIT;
            return result;
        }

        // Remove prefix from query
        std::string remaining = query_str.substr(prefix_str.length());

        if (remaining.empty())
        {
            // Query exactly matches prefix
            result.match_type = SPGiSTMatchType::MATCH_NODE;
            result.node_index = 0;  // Go to first child
            return result;
        }

        // Get first character after prefix
        char first_char = remaining[0];

        // Find matching child
        for (size_t i = 0; i < nodeLabels.size(); ++i)
        {
            std::string label_str = TextUtils::deserialize(nodeLabels[i].data);
            if (!label_str.empty() && label_str[0] == first_char)
            {
                result.match_type = SPGiSTMatchType::MATCH_NODE;
                result.node_index = i;
                return result;
            }
        }

        // No matching child, need to add one
        result.match_type = SPGiSTMatchType::MATCH_ADD_NODE;
        result.prefix = TextUtils::serialize(std::string(1, first_char));
        return result;
    }

    void pickSplit(
        const std::vector<std::vector<uint8_t>>& values,
        std::vector<uint8_t>& prefix,
        std::vector<std::vector<uint8_t>>& labels,
        std::vector<size_t>& assignments) const override
    {
        if (values.empty())
            return;

        // Find common prefix of all values
        std::string common_prefix_str = TextUtils::deserialize(values[0]);

        for (size_t i = 1; i < values.size(); ++i)
        {
            std::string value_str = TextUtils::deserialize(values[i]);
            common_prefix_str = TextUtils::commonPrefix(common_prefix_str, value_str);

            if (common_prefix_str.empty())
                break;
        }

        prefix = TextUtils::serialize(common_prefix_str);

        // Group values by next character after common prefix
        std::map<char, std::vector<size_t>> groups;

        for (size_t i = 0; i < values.size(); ++i)
        {
            std::string value_str = TextUtils::deserialize(values[i]);

            if (value_str.length() > common_prefix_str.length())
            {
                char next_char = value_str[common_prefix_str.length()];
                groups[next_char].push_back(i);
            }
            else
            {
                // Value exactly matches prefix, put in special group
                groups['\0'].push_back(i);
            }
        }

        // Create labels and assignments
        size_t label_index = 0;
        for (const auto& [ch, indices] : groups)
        {
            std::string label_str(1, ch);
            labels.push_back(TextUtils::serialize(label_str));

            for (size_t idx : indices)
            {
                assignments[idx] = label_index;
            }

            label_index++;
        }

        // Initialize assignments vector if needed
        if (assignments.size() < values.size())
        {
            assignments.resize(values.size(), 0);
        }
    }

    bool innerConsistent(
        const std::vector<uint8_t>& innerPrefix,
        const std::vector<uint8_t>& nodeLabel,
        const std::vector<uint8_t>& query) const override
    {
        std::string prefix_str = TextUtils::deserialize(innerPrefix);
        std::string label_str = TextUtils::deserialize(nodeLabel);
        std::string query_str = TextUtils::deserialize(query);

        // Combined path is prefix + label
        std::string path = prefix_str + label_str;

        // For prefix queries (LIKE 'abc%'), check if path is compatible with query
        // Query could be a prefix of path, or path could be a prefix of query

        size_t min_len = std::min(path.length(), query_str.length());

        for (size_t i = 0; i < min_len; ++i)
        {
            if (path[i] != query_str[i])
            {
                return false; // Paths diverge
            }
        }

        return true; // One is a prefix of the other
    }

    bool leafConsistent(
        const std::vector<uint8_t>& leafValue,
        const std::vector<uint8_t>& query) const override
    {
        std::string leaf_str = TextUtils::deserialize(leafValue);
        std::string query_str = TextUtils::deserialize(query);

        // For exact match
        if (leaf_str == query_str)
        {
            return true;
        }

        // For prefix match (LIKE 'abc%')
        // Check if query ends with '%' wildcard indicator
        // In practice, the query should be pre-processed to remove wildcards
        // For now, support exact match and prefix match

        if (query_str.length() <= leaf_str.length())
        {
            return leaf_str.substr(0, query_str.length()) == query_str;
        }

        return false;
    }
};

} // namespace scratchbird::core
