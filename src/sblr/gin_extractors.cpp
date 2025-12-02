/**
 * GIN Key Extractor Registry Implementation
 * November 20, 2025
 */

#include "scratchbird/sblr/gin_extractors.h"
#include <cstring>
#include <mutex>

namespace scratchbird::sblr {

// Singleton instance
GinExtractorRegistry& GinExtractorRegistry::instance() {
    static GinExtractorRegistry registry;
    return registry;
}

// Constructor - register default extractors
GinExtractorRegistry::GinExtractorRegistry() {
    // Register default extractor (ID 0) - uses value as-is
    registerExtractor(static_cast<uint16_t>(GinExtractorId::DEFAULT), defaultExtractor);

    // Register array extractor (ID 1)
    registerExtractor(static_cast<uint16_t>(GinExtractorId::ARRAY), arrayExtractor);

    // Phase 4 Enhancement: Add more extractors as needed:
    // - TEXT_TSVECTOR (ID 2) - for full-text search
    // - JSONB_PATH (ID 3) - for JSONB path extraction
    // - JSONB_VALUE (ID 4) - for JSONB value extraction
    // Current extractors (DEFAULT, ARRAY) cover common use cases
}

// Register a key extractor
void GinExtractorRegistry::registerExtractor(uint16_t id, GinKeyExtractor extractor) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    extractors_[id] = extractor;
}

// Get a key extractor
GinKeyExtractor GinExtractorRegistry::getExtractor(uint16_t id) const {
    auto it = extractors_.find(id);
    if (it != extractors_.end()) {
        return it->second;
    }
    // Return default extractor if not found
    return defaultExtractor;
}

// Default extractor - treats entire value as a single key
std::vector<std::vector<uint8_t>> GinExtractorRegistry::defaultExtractor(
    const void* data, size_t len)
{
    if (!data || len == 0) {
        return {};
    }

    std::vector<uint8_t> key(static_cast<const uint8_t*>(data),
                             static_cast<const uint8_t*>(data) + len);
    return {key};
}

// Array extractor - extracts individual array elements
// Simple implementation: assumes array of fixed-size elements
// Format: [element_size:4][num_elements:4][element1][element2]...
std::vector<std::vector<uint8_t>> GinExtractorRegistry::arrayExtractor(
    const void* data, size_t len)
{
    if (!data || len < 8) {
        // Not enough data for header
        return {};
    }

    const uint8_t* bytes = static_cast<const uint8_t*>(data);

    // Read element size (4 bytes, little-endian)
    uint32_t element_size = 0;
    std::memcpy(&element_size, bytes, 4);

    // Read number of elements (4 bytes, little-endian)
    uint32_t num_elements = 0;
    std::memcpy(&num_elements, bytes + 4, 4);

    // Validate
    if (element_size == 0 || num_elements == 0) {
        return {};
    }

    size_t expected_size = 8 + (element_size * num_elements);
    if (len < expected_size) {
        // Data too short
        return {};
    }

    // Extract elements
    std::vector<std::vector<uint8_t>> keys;
    keys.reserve(num_elements);

    const uint8_t* element_data = bytes + 8;
    for (uint32_t i = 0; i < num_elements; i++) {
        std::vector<uint8_t> key(element_data, element_data + element_size);
        keys.push_back(std::move(key));
        element_data += element_size;
    }

    return keys;
}

} // namespace scratchbird::sblr
