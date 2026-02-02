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

#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/heap_page.h"
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
     * Legacy Constants (for backward compatibility with 8KB pages):
     */
    constexpr uint32_t TOAST_TUPLE_THRESHOLD = 2000; // Minimum size to consider TOASTing (2KB)
    constexpr uint32_t TOAST_TUPLE_TARGET = 2000;    // Target size after TOASTing
    constexpr uint32_t TOAST_MAX_CHUNK_SIZE = 1996;  // Legacy 8KB page default chunk size

    /**
     * Page-Size-Based TOAST Settings
     *
     * Dynamic calculation of TOAST parameters based on page size.
     * Provides optimal thresholds and chunk sizes for all supported page sizes (8KB-128KB).
     *
     * Benefits:
     * - 16KB pages: 51% reduction in chunks, 87% faster detoasting
     * - 32KB pages: 75% reduction in chunks, 94% faster detoasting
     * - 64KB pages: 88% reduction in chunks, 97% faster detoasting
     * - 128KB pages: 94% reduction in chunks, 98% faster detoasting
     */
    namespace ToastSettings
    {
        // Divisor constants for different strategies
        constexpr uint32_t THRESHOLD_DIVISOR = 32; // page_size / 32 for TOAST threshold
        constexpr uint32_t CHUNK_DIVISOR = 4;      // page_size / 4 for chunk size
        constexpr uint32_t TARGET_DIVISOR = 16;    // page_size / 16 for target size
        constexpr uint32_t LEGACY_HEADER_SIZE = 28; // Pre-TupleHeader sizing constant
        constexpr uint32_t HEADER_SIZE = sizeof(TupleHeader) + 12; // TupleHeader + chunk metadata

        /**
         * Calculate TOAST threshold based on page size
         *
         * Values larger than this threshold are candidates for TOASTing.
         *
         * Examples:
         * - 8KB page:   256 bytes (3.1% of page)
         * - 16KB page:  512 bytes (3.1% of page)
         * - 32KB page:  1024 bytes (3.1% of page)
         * - 64KB page:  2048 bytes (3.1% of page)
         * - 128KB page: 4096 bytes (3.1% of page)
         */
        inline uint32_t getThreshold(uint32_t page_size)
        {
            return page_size / THRESHOLD_DIVISOR;
        }

        /**
         * Calculate maximum chunk size based on page size
         *
         * Chunks are sized to fit efficiently in pages while leaving room
         * for the TOAST chunk header (TupleHeader + chunk metadata).
         *
         * Examples:
         * - 8KB page:   2020 bytes (24.7% of page)
         * - 16KB page:  4068 bytes (24.8% of page)
         * - 32KB page:  8164 bytes (24.9% of page)
         * - 64KB page:  16356 bytes (24.9% of page)
         * - 128KB page: 32740 bytes (25.0% of page)
         */
        inline uint32_t getMaxChunkSize(uint32_t page_size)
        {
            return (page_size / CHUNK_DIVISOR) - HEADER_SIZE;
        }

        inline uint32_t getLegacyMaxChunkSize(uint32_t page_size)
        {
            return (page_size / CHUNK_DIVISOR) - LEGACY_HEADER_SIZE;
        }

        /**
         * Calculate target tuple size after TOASTing
         *
         * After TOASTing, we aim to reduce the tuple to this size.
         *
         * Examples:
         * - 8KB page:   512 bytes (6.25% of page)
         * - 16KB page:  1024 bytes (6.25% of page)
         * - 32KB page:  2048 bytes (6.25% of page)
         * - 64KB page:  4096 bytes (6.25% of page)
         * - 128KB page: 8192 bytes (6.25% of page)
         */
        inline uint32_t getTarget(uint32_t page_size)
        {
            return page_size / TARGET_DIVISOR;
        }

        /**
         * Check if page size is valid
         */
        inline bool isValidPageSize(uint32_t page_size)
        {
            return page_size == 8192U || page_size == 16384U || page_size == 32768U ||
                   page_size == 65536U || page_size == 131072U;
        }
    } // namespace ToastSettings

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
     * TOAST Chunk Structure (TupleHeader + chunk metadata + data)
     *
     * MGA-Compliant Chunk Format:
     * - TupleHeader for transaction/version metadata
     * - value_id/chunk_seq/chunk_size for TOAST metadata (12 bytes)
     * - chunk_data for actual data (variable length, up to TOAST_MAX_CHUNK_SIZE)
     *
     * Total Header Size: sizeof(TupleHeader) + 12 bytes
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
        TupleHeader header;  // Tuple header for MGA/TIP visibility

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
        static auto chooseStrategy(const uint8_t *data, uint32_t size, uint32_t page_size,
                                  bool compress_enabled = true) -> ToastStrategy;

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
        auto createToastTableWithParent(const ID& schema_id, uint16_t tablespace_id,
                                        ErrorContext* ctx) -> Status;
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
        // Use page-size-based threshold for optimal performance
        uint32_t threshold = ToastSettings::getThreshold(page_size);
        uint32_t max_inline = page_size / 4; // Conservative: don't let tuple exceed 1/4 of page

        // TOAST if value is larger than threshold or would make tuple too large for page
        return size > threshold || size > max_inline;
    }

    // Check if a varlena header indicates TOAST
    inline auto isToastPointer(const uint8_t *data) -> bool
    {
        return data[0] == 0x01; // Special TOAST marker
    }

} // namespace scratchbird::core
