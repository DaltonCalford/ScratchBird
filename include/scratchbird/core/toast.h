#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/catalog_manager.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <atomic>

namespace scratchbird::core
{

    // Forward declarations
    class Database;
    class BufferPool;
    class PageManager;
    struct ErrorContext;

    // TOAST (The Oversized-Attribute Storage Technique) constants
    constexpr uint32_t TOAST_TUPLE_THRESHOLD = 2000; // Minimum size to consider TOASTing
    constexpr uint32_t TOAST_TUPLE_TARGET = 2000;    // Target size after TOASTing
    constexpr uint32_t TOAST_MAX_CHUNK_SIZE = 1996;  // Max chunk size (leaves room for headers)

    // TOAST strategies
    enum class ToastStrategy : uint8_t
    {
        PLAIN = 0,      // Store inline (no TOAST)
        EXTENDED = 1,   // Store out-of-line, uncompressed
        COMPRESSED = 2, // Store inline, compressed
        EXTERNAL = 3,   // Store out-of-line, compressed
    };

// TOAST pointer - stored in main tuple instead of actual data
#pragma pack(push, 1)
    struct ToastPointer
    {
        uint8_t va_header;      // Varlena header byte (0x01 = TOAST)
        uint8_t va_tag;         // Type tag and compression info
        uint32_t va_rawsize;    // Original (uncompressed) data size
        uint32_t va_extsize;    // External stored size
        uint32_t va_valueid;    // Unique identifier for this TOAST value
        uint32_t va_toastrelid; // TOAST table ID
    };
#pragma pack(pop)

// TOAST chunk - one piece of a large value
// Firebird MGA compliant: includes xmin/xmax for TIP-based visibility
#pragma pack(push, 1)
    struct ToastChunk
    {
        uint64_t xmin;                            // Transaction that created this chunk (Firebird MGA)
        uint64_t xmax;                            // Transaction that deleted this chunk (0 if active)
        uint32_t chunk_id;                        // Unique ID of the owning TOAST value
        uint32_t chunk_seq;                       // Sequence number (0-based)
        uint32_t chunk_size;                      // Size of data in this chunk
        uint8_t chunk_data[TOAST_MAX_CHUNK_SIZE]; // Actual data
    };
#pragma pack(pop)

    // TOAST table entry - stored in TOAST table
    struct ToastTableEntry
    {
        uint64_t xmin;             // Transaction that created this
        uint64_t xmax;             // Transaction that deleted this (or 0)
        uint32_t value_id;         // Unique TOAST value ID
        uint32_t chunk_seq;        // Chunk sequence number
        uint32_t chunk_size;       // Size of this chunk
        std::vector<uint8_t> data; // Chunk data
    };

// TOAST value header for compressed data
#pragma pack(push, 1)
    struct ToastCompressHeader
    {
        uint32_t rawsize;    // Uncompressed size
        uint8_t compression; // Compression algorithm used
    };
#pragma pack(pop)

    // TOAST manager - handles large attribute storage
    class ToastManager
    {
    public:
        ToastManager(Database *db, const ID &table_id);
        ~ToastManager();

        // Initialize TOAST subsystem for a table
        auto initialize(ErrorContext *ctx = nullptr) -> Status;

        // Create TOAST table for a regular table
        auto createToastTable(ErrorContext *ctx = nullptr) -> Status;

        // TOAST a large value
        // Returns the ToastPointer to store in the main tuple
        auto toastValue(const uint8_t *data, uint32_t size, ToastStrategy strategy, uint64_t xmin,
                        ToastPointer *pointer_out, ErrorContext *ctx = nullptr) -> Status;

        // Detoast a value
        // Returns the reconstructed data
        auto detoastValue(const ToastPointer *pointer, std::vector<uint8_t> *data_out,
                          uint64_t xmin, ErrorContext *ctx = nullptr) -> Status;

        // Delete TOASTed value
        auto deleteToastValue(uint32_t value_id, uint64_t xmax, ErrorContext *ctx = nullptr)
            -> Status;

        // Delete TOASTed value using heap scan (fallback)
        auto deleteToastValueHeapScan(uint32_t value_id, uint64_t xmax, ErrorContext *ctx = nullptr)
            -> Status;

        // Check if a value should be TOASTed
        static auto shouldToast(uint32_t size, uint32_t page_size) -> bool;

        // Determine best TOAST strategy for a value
        static auto chooseStrategy(const uint8_t *data, uint32_t size, bool compress_enabled = true)
            -> ToastStrategy;

        // Check if data is a TOAST pointer (Phase 3: Index TOAST Integration)
        // Returns true if the data is exactly 18 bytes and has TOAST pointer magic
        static auto isToastPointer(const uint8_t *data, size_t size) -> bool;

        // Detoast a value if it's a TOAST pointer, otherwise return original data
        // (Phase 3: Index TOAST Integration helper for index insert operations)
        auto detoastIfNeeded(const uint8_t *data, size_t size, std::vector<uint8_t> *result,
                            uint64_t xid, ErrorContext *ctx = nullptr) -> Status;

        // Get TOAST table ID for a regular table
        [[nodiscard]] auto toastTableId() const -> const ID &
        {
            return toast_table_id_;
        }

    private:
        Database *db_;
        ID table_id_;       // Regular table ID
        ID toast_table_id_; // Associated TOAST table ID
        std::atomic<uint32_t>
            next_value_id_; // Next TOAST value ID to assign (atomic for thread safety)

        // Helper methods
        auto initializeNextValueId(ErrorContext *ctx) -> Status;

        auto writeToastChunks(uint32_t value_id, const uint8_t *data, uint32_t size, uint64_t xmin,
                              ErrorContext *ctx) -> Status;

        auto readToastChunks(uint32_t value_id, std::vector<uint8_t> *data_out, uint64_t xmin,
                             ErrorContext *ctx) -> Status;

        auto readToastChunksHeapScan(uint32_t value_id, std::vector<uint8_t> *data_out,
                                     uint64_t xmin, ErrorContext *ctx) -> Status;

        auto compressData(const uint8_t *src, uint32_t src_size, std::vector<uint8_t> *dst,
                          ErrorContext *ctx) -> Status;

        auto decompressData(const uint8_t *src, uint32_t src_size, uint32_t uncompressed_size,
                            std::vector<uint8_t> *dst, ErrorContext *ctx) -> Status;
    };

    // Inline functions
    inline auto ToastManager::shouldToast(uint32_t size, uint32_t page_size) -> bool
    {
        // TOAST if value is larger than threshold or
        // if it would make tuple too large for page
        return size > TOAST_TUPLE_THRESHOLD || size > (page_size / 4); // Conservative: 1/4 of page
    }

    // Check if a varlena header indicates TOAST
    inline auto isToastPointer(const uint8_t *data) -> bool
    {
        return data[0] == 0x01; // Special TOAST marker
    }

} // namespace scratchbird::core
