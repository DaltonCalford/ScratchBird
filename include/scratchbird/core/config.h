#pragma once

#include <cstdint>

namespace scratchbird::core::config
{

    // Buffer Pool configuration
    constexpr uint32_t DEFAULT_BUFFER_POOL_SIZE = 128; // in pages

    // Heap scan starting page
    constexpr uint32_t HEAP_SCAN_START_PAGE = 7;

    // Number of base schemas
    constexpr int NUM_BASE_SCHEMAS = 8;

} // namespace scratchbird::core::config
