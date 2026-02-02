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
#include <cstring>
#include <functional>
#include <unordered_map>
#include <vector>

namespace scratchbird::sblr {

/**
 * GIN Key Extractor Registry
 *
 * Maps extractor IDs to key extraction functions for GIN indexes.
 * Key extractors parse composite values (arrays, JSONB, text) and
 * return individual keys to be indexed.
 */

// Key extractor function type
using GinKeyExtractor = std::function<std::vector<std::vector<uint8_t>>(const void*, size_t)>;

// Extractor IDs
enum class GinExtractorId : uint16_t {
    DEFAULT = 0,        // No extraction - use value as-is
    ARRAY = 1,          // Extract array elements
    TEXT_TSVECTOR = 2,  // Extract text search tokens
    JSONB_PATH = 3,     // Extract JSONB keys/paths
    JSONB_VALUE = 4,    // Extract JSONB values
};

/**
 * GIN Key Extractor Registry
 * Thread-safe singleton registry for GIN key extractors
 */
class GinExtractorRegistry {
public:
    // Get singleton instance
    static GinExtractorRegistry& instance();

    // Register a key extractor
    void registerExtractor(uint16_t id, GinKeyExtractor extractor);

    // Get a key extractor (returns nullptr if not found)
    GinKeyExtractor getExtractor(uint16_t id) const;

    // Default extractors
    static std::vector<std::vector<uint8_t>> defaultExtractor(const void* data, size_t len);
    static std::vector<std::vector<uint8_t>> arrayExtractor(const void* data, size_t len);

private:
    GinExtractorRegistry();
    ~GinExtractorRegistry() = default;

    // No copy/move
    GinExtractorRegistry(const GinExtractorRegistry&) = delete;
    GinExtractorRegistry& operator=(const GinExtractorRegistry&) = delete;

    std::unordered_map<uint16_t, GinKeyExtractor> extractors_;
};

} // namespace scratchbird::sblr
