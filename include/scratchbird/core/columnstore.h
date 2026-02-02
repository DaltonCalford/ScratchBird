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
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/buffer_pool.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <mutex>

namespace scratchbird::core
{

// Forward declarations
class Database;
class BufferPool;
class PageManager;
class TransactionManager;
struct ErrorContext;

/**
 * Columnstore Index - Column-Oriented Storage for Analytical Workloads
 *
 * Columnstore provides efficient storage and querying for analytical workloads
 * by storing data column-by-column rather than row-by-row. This enables:
 * - Better compression (similar values grouped together)
 * - Faster scans (only read needed columns)
 * - Vectorized processing (batch operations on arrays)
 * - Predicate pushdown (filter before decompression)
 *
 * ## Use Cases
 * - Data warehousing and OLAP queries
 * - Analytics dashboards
 * - Time-series data
 * - Log aggregation
 * - Reporting and BI workloads
 *
 * ## Architecture (Phase 1 - Simplified)
 *
 * ```
 * Table with columns: (id INT, name VARCHAR, age INT, salary DECIMAL)
 *
 * Row Store (Heap):
 *   Page 1: [1, "Alice", 25, 50000] [2, "Bob", 30, 60000] ...
 *
 * Column Store:
 *   Column "id":     [1, 2, 3, 4, 5, ...] (compressed)
 *   Column "name":   ["Alice", "Bob", "Carol", ...] (compressed)
 *   Column "age":    [25, 30, 28, 35, ...] (compressed)
 *   Column "salary": [50000, 60000, 55000, ...] (compressed)
 *
 * Query: SELECT AVG(salary) FROM employees WHERE age > 30
 *   → Scan only "age" and "salary" columns (skip id, name)
 *   → Filter age > 30 using compressed data
 *   → Compute AVG on filtered salary values
 *   → No row reconstruction needed!
 * ```
 *
 * ## MGA Compliance (Phase 1)
 *
 * - **xmin/xmax tracking**: Each column segment has xmin/xmax
 * - **TIP-based visibility**: Column scans filter invisible rows
 * - **Version vectors**: Track row versions per column
 * - **Garbage collection**: Remove dead column values
 * - **Stable TIDs**: Column values reference stable tuple IDs
 *
 * ## Compression (Phase 1 - RLE only)
 *
 * **Run-Length Encoding (RLE):**
 * - Efficient for columns with repeated values
 * - Example: [1, 1, 1, 2, 2, 3, 3, 3, 3] → [(1, 3), (2, 2), (3, 4)]
 * - Typical compression: 5-10x for sorted/low-cardinality columns
 *
 * **Deferred to Phase 2:**
 * - Dictionary encoding (string columns)
 * - Bit-packing (integer columns with small range)
 * - Delta encoding (timestamps, sequential IDs)
 * - Frame-of-reference (subtract baseline value)
 *
 * ## Implementation Notes (Phase 1)
 *
 * - Single-column segments (one column per segment)
 * - RLE compression only
 * - Fixed-size batch processing (1024 values)
 * - Simple predicate pushdown (>, <, =, !=)
 * - No hybrid row-column (pure column store)
 * - No automatic tiering
 * - Direct column scans (no late materialization yet)
 */

// Columnstore page flags
enum class ColumnstoreFlags : uint16_t
{
    COMPRESSED = 0x0001,    // Segment is compressed
    SORTED = 0x0002,        // Values are sorted
    HAS_NULLS = 0x0004,     // Segment contains NULL values
    HAS_GARBAGE = 0x0008,   // Segment has deleted values
    CONTINUATION = 0x0010   // This is a continuation page for a multi-page segment
};

// Compression types
enum class CompressionType : uint8_t
{
    NONE = 0,           // No compression
    RLE = 1,            // Run-Length Encoding
    DICTIONARY = 2,     // Dictionary encoding (Phase 2)
    BITPACK = 3,        // Bit-packing (Phase 2)
    DELTA = 4           // Delta encoding (Phase 2)
};

#pragma pack(push, 1)

/**
 * Columnstore metadata page - page 0 of a columnstore index
 * Stores index configuration for durable persistence
 */
struct SBColumnstoreMetadataPage
{
    // Standard page header
    PageHeader cs_header;

    // Index identification
    ID cs_index_uuid;  // Index UUID v7
    ID cs_table_uuid;  // Table this index belongs to

    // Columnstore configuration
    uint32_t cs_segment_size;      // Target segment size in rows
    uint8_t cs_compression_type;   // Default compression type
    uint8_t cs_reserved1;
    uint16_t cs_column_count;      // Number of columns in this index

    // Root page pointer
    uint32_t cs_first_segment_page; // First segment page (0 if none yet)

    // Statistics
    uint64_t cs_total_segments;
    uint64_t cs_total_rows;

    // MGA compliance
    uint64_t cs_xmin; // Metadata creation transaction
    uint64_t cs_xmax; // Metadata deletion transaction (0 if active)

    uint8_t cs_padding[64]; // Reserved for future use

    // Column UUIDs follow immediately after header
    // Array of ID structs (16 bytes each)
};

/**
 * Columnstore segment page structure
 *
 * Stores a column segment with compressed values.
 * Each segment stores values for a single column.
 */
struct SBColumnstorePage
{
    // Standard page header
    PageHeader cs_header; // Standard ScratchBird page header

    // Index identification
    ID cs_index_uuid;  // Index UUID v7
    ID cs_table_uuid;  // Table this index belongs to
    ID cs_column_uuid; // Column this segment stores

    // Columnstore metadata
    uint16_t cs_flags;              // Page flags (see ColumnstoreFlags)
    uint16_t cs_row_count;          // Number of values in segment
    uint16_t cs_null_count;         // Number of NULL values
    uint8_t cs_compression_type;    // CompressionType enum value
    uint8_t cs_data_type;           // DataType enum value
    uint32_t cs_compressed_size;    // Size of compressed data in bytes
    uint32_t cs_uncompressed_size;  // Original size before compression

    // Value range (for predicate pushdown)
    int64_t cs_min_value;  // Minimum value (for integers)
    int64_t cs_max_value;  // Maximum value (for integers)

    // TID mapping
    OnDiskTID cs_first_tid;  // First TID in this segment
    OnDiskTID cs_last_tid;   // Last TID in this segment

    // MGA compliance
    uint64_t cs_xmin; // Segment creation transaction
    uint64_t cs_xmax; // Segment deletion transaction (0 if active)
    uint64_t cs_lsn;  // Last LSN that modified this segment

    // Sibling navigation
    uint64_t cs_prev_segment; // Previous segment page number
    uint64_t cs_next_segment; // Next segment page number

    uint8_t cs_padding[64]; // Reserved for future use

    // Compressed data follows immediately after header
    // Format depends on cs_compression_type
};

#pragma pack(pop)

/**
 * RLE Run structure (for Run-Length Encoding)
 */
struct RLERun
{
    uint64_t value;  // The value (interpreted based on data type)
    uint32_t count;  // Number of repetitions
    uint32_t start_offset;  // Starting offset in segment
};

/**
 * Columnstore segment - in-memory representation
 */
struct ColumnSegment
{
    ID column_uuid;
    DataType data_type;
    std::vector<uint8_t> data;  // Raw or compressed data
    CompressionType compression;
    uint32_t row_count;
    uint32_t null_count;
    std::vector<bool> null_bitmap;  // NULL indicators
    TID first_tid;
    TID last_tid;
    uint64_t xmin;
    uint64_t xmax;
    std::vector<TID> tid_map;
    std::vector<uint8_t> visibility_bitmap;
    int64_t min_value;
    int64_t max_value;
    uint32_t page_count;
    uint32_t next_segment_page;
};

/**
 * Dictionary for dictionary encoding
 */
struct Dictionary
{
    std::unordered_map<std::string, uint32_t> value_to_code;  // String value -> integer code
    std::vector<std::string> code_to_value;  // Integer code -> string value
    uint32_t next_code;  // Next available code

    Dictionary() : next_code(0) {}

    // Add value to dictionary, return code
    uint32_t addValue(const std::string& value)
    {
        auto it = value_to_code.find(value);
        if (it != value_to_code.end())
        {
            return it->second;  // Already in dictionary
        }

        uint32_t code = next_code++;
        value_to_code[value] = code;
        code_to_value.push_back(value);
        return code;
    }

    // Get code for value (returns -1 if not found)
    int32_t getCode(const std::string& value) const
    {
        auto it = value_to_code.find(value);
        return (it != value_to_code.end()) ? static_cast<int32_t>(it->second) : -1;
    }

    // Get value for code
    bool getValue(uint32_t code, std::string* value_out) const
    {
        if (code >= code_to_value.size())
            return false;
        *value_out = code_to_value[code];
        return true;
    }

    // Get dictionary size
    size_t size() const { return code_to_value.size(); }

    // Clear dictionary
    void clear()
    {
        value_to_code.clear();
        code_to_value.clear();
        next_code = 0;
    }
};

/**
 * Columnstore Index metadata (stored in catalog)
 */
struct SBColumnstoreIndex
{
    ID idx_uuid;                        // Index UUID v7
    ID idx_table_uuid;                  // Table UUID
    std::vector<ID> idx_column_uuids;   // Indexed columns
    uint32_t idx_root_page;             // Root segment page
    uint16_t idx_tablespace_id = 0;
    uint32_t idx_segment_size;          // Rows per segment (default 1024)
    uint8_t idx_compression_type;       // Default compression type
    uint64_t idx_total_segments;        // Total number of segments
    uint64_t idx_total_rows;            // Total number of rows
    uint64_t idx_creation_xid;          // Transaction that created index
};

/**
 * Column scan result (batch of values)
 */
struct ColumnScanBatch
{
    std::vector<TID> tids;           // Tuple IDs
    std::vector<uint8_t> values;     // Decompressed values
    std::vector<bool> null_flags;    // NULL indicators
    uint32_t count;                  // Number of values in batch
    DataType data_type;              // Data type of values
};

/**
 * Predicate for column filtering
 */
struct ColumnPredicate
{
    enum class Op
    {
        EQUAL,
        NOT_EQUAL,
        LESS_THAN,
        LESS_EQUAL,
        GREATER_THAN,
        GREATER_EQUAL,
        IS_NULL,
        IS_NOT_NULL
    };

    Op op;
    int64_t value;  // Comparison value (for integer types)
};

/**
 * Scan iterator state (for multi-call scans)
 *
 * Phase 5: Tracks position across multiple scan() calls
 * Allows processing large result sets in batches of 1024 rows
 */
struct ColumnScanIterator
{
    ID column_uuid;                  // Column being scanned
    uint32_t current_segment_page;   // Current segment page number (0 = done)
    uint32_t offset_in_segment;      // Offset within current segment
    uint64_t current_xid;            // Transaction ID for visibility
    ColumnPredicate predicate;       // Filter predicate
    bool has_predicate;              // Whether predicate is set
    bool scan_complete;              // True when scan has finished

    // Cached decompressed segment data (to avoid repeated decompression)
    ColumnSegment cached_segment;
    bool segment_cached;

    ColumnScanIterator()
        : current_segment_page(0), offset_in_segment(0),
          current_xid(0), has_predicate(false), scan_complete(false),
          segment_cached(false) {}
};

/**
 * Columnstore Index Implementation
 */
class ColumnstoreIndex : public IndexGCInterface
{
public:
    ColumnstoreIndex(Database *db, SBColumnstoreIndex index_info);
    ~ColumnstoreIndex();

    // Static factory methods
    static Status create(Database *db,
                        const UuidV7Bytes &index_uuid,
                        const UuidV7Bytes &table_uuid,
                        const std::vector<UuidV7Bytes> &column_uuids,
                        uint32_t segment_size = 1024,
                        CompressionType compression = CompressionType::RLE,
                        GPID root_gpid = 0,
                        ErrorContext *ctx = nullptr);
    static Status create(Database *db,
                        const UuidV7Bytes &index_uuid,
                        const UuidV7Bytes &table_uuid,
                        const std::vector<UuidV7Bytes> &column_uuids,
                        uint32_t segment_size,
                        CompressionType compression,
                        uint32_t *root_page_out,
                        ErrorContext *ctx = nullptr);

    static std::unique_ptr<ColumnstoreIndex> open(Database *db,
                                                   const UuidV7Bytes &index_uuid,
                                                   GPID root_gpid,
                                                   uint32_t segment_size = 1024,
                                                   ErrorContext *ctx = nullptr);

    /**
     * Insert values into column segments
     *
     * Phase 1: Simple append to segments
     *
     * @param column_uuid Column to insert into
     * @param tid Tuple ID
     * @param value Value to insert
     * @param is_null Whether value is NULL
     * @param ctx Error context
     */
    Status insert(const ID &column_uuid,
                 const TID &tid,
                 const void *value,
                 size_t value_len,
                 bool is_null,
                 ErrorContext *ctx = nullptr);

    /**
     * Scan column with optional predicate (buffered values only)
     *
     * Phase 1-4: Sequential scan with simple filtering (buffered values only)
     * Phase 5: Use beginScan/scanNext/endScan for full disk segment scanning
     *
     * @param column_uuid Column to scan
     * @param predicate Optional filter predicate
     * @param current_xid Current transaction ID for visibility
     * @param batch_out Output batch of values
     * @param ctx Error context
     */
    Status scan(const ID &column_uuid,
               const ColumnPredicate *predicate,
               uint64_t current_xid,
               ColumnScanBatch *batch_out,
               ErrorContext *ctx = nullptr);

    /**
     * Begin a batch scan iterator (Phase 5)
     *
     * Initializes an iterator for scanning a column in batches.
     * Use scanNext() to retrieve batches of up to 1024 rows.
     * Call endScan() when finished to clean up resources.
     *
     * Example:
     *   ColumnScanIterator iter;
     *   index->beginScan(column_uuid, &predicate, xid, &iter, &ctx);
     *   while (!iter.scan_complete) {
     *       ColumnScanBatch batch;
     *       index->scanNext(&iter, &batch, &ctx);
     *       // Process batch...
     *   }
     *   index->endScan(&iter, &ctx);
     *
     * @param column_uuid Column to scan
     * @param predicate Optional filter predicate (NULL for no filter)
     * @param current_xid Current transaction ID for visibility
     * @param iterator_out Output iterator state
     * @param ctx Error context
     */
    Status beginScan(const ID &column_uuid,
                    const ColumnPredicate *predicate,
                    uint64_t current_xid,
                    ColumnScanIterator *iterator_out,
                    ErrorContext *ctx = nullptr);

    /**
     * Get next batch from scan iterator (Phase 5)
     *
     * Returns up to 1024 rows per call.
     * Sets iterator->scan_complete = true when no more rows.
     *
     * @param iterator Scan iterator (updated with new position)
     * @param batch_out Output batch (cleared and filled)
     * @param ctx Error context
     */
    Status scanNext(ColumnScanIterator *iterator,
                   ColumnScanBatch *batch_out,
                   ErrorContext *ctx = nullptr);

    /**
     * End scan iterator and clean up resources (Phase 5)
     *
     * @param iterator Scan iterator to clean up
     * @param ctx Error context
     */
    Status endScan(ColumnScanIterator *iterator,
                  ErrorContext *ctx = nullptr);

    /**
     * Get statistics
     */
    struct ColumnstoreStats
    {
        uint64_t total_segments;
        uint64_t total_rows;
        uint64_t compressed_bytes;
        uint64_t uncompressed_bytes;
        double compression_ratio;
        uint64_t null_count;
    };

    Status getStats(ColumnstoreStats *stats_out, ErrorContext *ctx = nullptr);

    // Update TIDs after tablespace migration (GPID remap)
    Status updateTIDsAfterMigration(const std::unordered_map<TID, TID> &tid_mapping,
                                    uint64_t *tids_updated_out = nullptr,
                                    uint64_t *pages_modified_out = nullptr,
                                    ErrorContext *ctx = nullptr);

    // IndexGCInterface
    Status removeDeadEntries(const std::vector<TID> &dead_tids,
                             uint64_t *entries_removed_out = nullptr,
                             uint64_t *pages_modified_out = nullptr,
                             ErrorContext *ctx = nullptr) override;
    const char *indexTypeName() const override { return "COLUMNSTORE"; }

    // Compression methods (public for testing)

    /**
     * Compress segment using RLE
     */
    Status compressRLE(const ColumnSegment &segment,
                      std::vector<uint8_t> *compressed_out,
                      ErrorContext *ctx);

    /**
     * Decompress RLE segment
     */
    Status decompressRLE(const std::vector<uint8_t> &compressed,
                        DataType data_type,
                        uint32_t row_count,
                        ColumnSegment *segment_out,
                        ErrorContext *ctx);

    /**
     * Compress segment using dictionary encoding
     */
    Status compressDictionary(const ColumnSegment &segment,
                              std::vector<uint8_t> *compressed_out,
                              Dictionary *dict_out,
                              ErrorContext *ctx);

    /**
     * Decompress dictionary-encoded segment
     */
    Status decompressDictionary(const std::vector<uint8_t> &compressed,
                               const Dictionary &dict,
                               DataType data_type,
                               uint32_t row_count,
                               ColumnSegment *segment_out,
                               ErrorContext *ctx);

    /**
     * Compress segment using bit-packing
     *
     * Minimizes storage by packing values into the minimum number of bits needed.
     * Example: Values 0-7 need only 3 bits each instead of 32 bits (int32_t).
     *
     * Algorithm:
     * 1. Find min/max values in segment
     * 2. Calculate bits needed: ceil(log2(max - min + 1))
     * 3. Subtract min from all values (normalize to 0)
     * 4. Pack normalized values into bit array
     *
     * @param segment Input segment with integer values
     * @param compressed_out Output compressed data
     * @param ctx Error context
     */
    Status compressBitpack(const ColumnSegment &segment,
                          std::vector<uint8_t> *compressed_out,
                          ErrorContext *ctx);

    /**
     * Decompress bit-packed segment
     *
     * @param compressed Compressed bit-packed data
     * @param data_type Original data type
     * @param row_count Number of values
     * @param segment_out Output decompressed segment
     * @param ctx Error context
     */
    Status decompressBitpack(const std::vector<uint8_t> &compressed,
                            DataType data_type,
                            uint32_t row_count,
                            ColumnSegment *segment_out,
                            ErrorContext *ctx);

    Status compressDelta(const ColumnSegment &segment,
                         std::vector<uint8_t> *compressed_out,
                         ErrorContext *ctx);

    Status decompressDelta(const std::vector<uint8_t> &compressed,
                           DataType data_type,
                           uint32_t row_count,
                           ColumnSegment *segment_out,
                           ErrorContext *ctx);

    /**
     * Apply predicate to segment (predicate pushdown)
     *
     * This method implements batch predicate evaluation with min/max pruning.
     * It returns matching offsets for values that satisfy the predicate.
     *
     * @param segment Input segment to filter
     * @param predicate Predicate to apply
     * @param matching_offsets Output vector of matching offsets
     * @param ctx Error context
     */
    Status applyPredicate(const ColumnSegment &segment,
                         const ColumnPredicate &predicate,
                         std::vector<uint32_t> *matching_offsets,
                         ErrorContext *ctx);

private:
    Database *db_;
    SBColumnstoreIndex index_info_;
    uint32_t metadata_page_ = 0;

    GPID indexGPID(uint64_t page_num) const;
    Status pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx = nullptr,
                        BufferPool::AccessStrategy strategy = BufferPool::AccessStrategy::Normal);
    Status unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx = nullptr);

    // Last segment page cache - avoids O(n) traversal when appending segments
    uint32_t last_segment_page_ = 0;

    // Insert buffering (per-column buffers)
    struct BufferedValue
    {
        std::vector<uint8_t> data;
        bool is_null;
        TID tid;
        uint64_t xmin;
    };
    std::unordered_map<ID, std::vector<BufferedValue>> column_buffers_;
    std::mutex buffer_mutex_;

    // Helper methods

    /**
     * Find segment containing TID
     */
    Status findSegment(const ID &column_uuid,
                      const TID &tid,
                      uint32_t *segment_page_out,
                      ErrorContext *ctx);

    /**
     * Create new segment page
     */
    Status createSegment(const ID &column_uuid,
                        const ColumnSegment &segment,
                        uint32_t *segment_page_out,
                        ErrorContext *ctx);

    /**
     * Read segment from page
     */
    Status readSegment(uint32_t segment_page,
                      ColumnSegment *segment_out,
                      ErrorContext *ctx);

    /**
     * Check if value is visible to transaction
     */
    bool isValueVisible(uint64_t value_xmin,
                       uint64_t value_xmax,
                       uint64_t current_xid,
                       ErrorContext *ctx) const;

    /**
     * Flush buffered values to a segment
     */
    Status flushSegment(const ID &column_uuid, ErrorContext *ctx);
    Status updateMetadataPage(ErrorContext *ctx);

    /**
     * Get column data type from catalog
     *
     * Looks up column metadata from the catalog system to determine
     * the correct data type for a column UUID.
     *
     * @param column_uuid Column UUID to look up
     * @param data_type_out Output data type
     * @param value_size_out Output size in bytes for fixed-size types
     * @param ctx Error context
     * @return Status::OK on success, error otherwise
     */
    Status getColumnDataType(const ID &column_uuid,
                            DataType *data_type_out,
                            size_t *value_size_out,
                            ErrorContext *ctx);

    /**
     * Create metadata page (page 0) for columnstore index
     * Stores configuration for durable persistence
     */
    static Status createMetadataPage(Database *db,
                                     const UuidV7Bytes &index_uuid,
                                     const UuidV7Bytes &table_uuid,
                                     const std::vector<UuidV7Bytes> &column_uuids,
                                     uint32_t segment_size,
                                     CompressionType compression,
                                     GPID metadata_gpid,
                                     ErrorContext *ctx);

    /**
     * Read metadata from page 0
     * Populates index_info_ with stored configuration
     */
    Status readMetadataPage(uint32_t metadata_page, ErrorContext *ctx);
};

} // namespace scratchbird::core
