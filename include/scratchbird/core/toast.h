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

    /**
     * TOAST (The Oversized-Attribute Storage Technique)
     *
     * MGA-Compliant Implementation:
     * - Uses TIP (Transaction Inventory Pages) for visibility, NOT snapshots
     * - TOAST chunks include xmin/xmax for transaction versioning
     * - Crash recovery via TIP state, NO WAL replay
     * - Garbage collection integrated into vacuum (3-phase GC)
     *
     * Constants:
     */
    constexpr uint32_t TOAST_TUPLE_THRESHOLD = 2000; // Minimum size to consider TOASTing (2KB)
    constexpr uint32_t TOAST_TUPLE_TARGET = 2000;    // Target size after TOASTing
    constexpr uint32_t TOAST_MAX_CHUNK_SIZE = 1996;  // Max chunk size (leaves room for 28-byte header)

    /**
     * TOAST Storage Strategies
     *
     * Determines how large values are stored:
     */
    enum class ToastStrategy : uint8_t
    {
        PLAIN = 0,      // Store inline (no TOAST) - for small values < 2KB
        EXTENDED = 1,   // Store out-of-line, uncompressed - for medium values or incompressible data
        COMPRESSED = 2, // Store inline, compressed - not implemented (future)
        EXTERNAL = 3,   // Store out-of-line, compressed - for large compressible values
    };

    /**
     * TOAST Pointer Structure (18 bytes)
     *
     * Stored in main tuple instead of actual data when value is TOASTed.
     * Points to chunks stored in TOAST table.
     *
     * Magic Byte Detection: va_header == 0x01 indicates a TOAST pointer
     */
#pragma pack(push, 1)
    struct ToastPointer
    {
        uint8_t va_header;      // Varlena header byte (0x01 = TOAST magic byte)
        uint8_t va_tag;         // Type tag and compression info
        uint32_t va_rawsize;    // Original (uncompressed) data size
        uint32_t va_extsize;    // External stored size (after compression if applicable)
        uint32_t va_valueid;    // Unique identifier for this TOAST value
        uint32_t va_toastrelid; // TOAST table ID
    };
#pragma pack(pop)

    /**
     * TOAST Chunk Structure (28-byte header + data)
     *
     * MGA-Compliant Chunk Format:
     * - xmin/xmax for transaction versioning (16 bytes)
     * - value_id/chunk_seq/chunk_size for TOAST metadata (12 bytes)
     * - chunk_data for actual data (variable length, up to TOAST_MAX_CHUNK_SIZE)
     *
     * Total Header Size: 28 bytes
     *
     * Visibility:
     * - Chunk visible if xmin committed (via TIP) AND xmax NOT committed (via TIP)
     * - Uses ToastVisibility::isChunkVisible() for TIP-based visibility checks
     *
     * Crash Recovery:
     * - If transaction crashes, TIP marks it as TX_ABORTED
     * - Chunks with aborted xmin become invisible
     * - Garbage collection physically removes orphaned chunks
     */
#pragma pack(push, 1)
    struct ToastChunk
    {
        // MGA Transaction Fields (16 bytes)
        uint64_t xmin;       // Transaction that created this chunk (for visibility via TIP)
        uint64_t xmax;       // Transaction that deleted this chunk (0 = active, non-zero = deleted)

        // TOAST Metadata (12 bytes)
        uint32_t chunk_id;   // Unique ID of the owning TOAST value
        uint32_t chunk_seq;  // Sequence number (0-based)
        uint32_t chunk_size; // Size of data in this chunk

        // Chunk Data (variable length, up to TOAST_MAX_CHUNK_SIZE)
        uint8_t chunk_data[TOAST_MAX_CHUNK_SIZE]; // Actual data bytes
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

        // MGA-compliant soft delete: Mark TOAST chunk as deleted by updating xmax only
        // Does NOT mark item pointer as deleted, allowing older transactions to still see the chunk
        auto markToastChunkDeleted(uint32_t page_id, uint16_t item_id, uint64_t xmax,
                                   ErrorContext *ctx) -> Status;
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
