/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created for the ScratchBird Open Source 
 *  RDBMS project.
 *
 *  Copyright (c) 2025 ScratchBird Project
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 * 2025.07.23 - ScratchBird Bitmap Index Implementation
 */

#ifndef JRD_BITMAP_INDEX_H
#define JRD_BITMAP_INDEX_H

#include "IndexType.h"
#include "constants.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/array.h"
#include "../common/gdsassert.h"
#include <vector>
#include <map>
#include <memory>

namespace Jrd {

// Forward declarations
class thread_db;
class MemoryPool;
class IndexRetrieval;
struct index_desc;
struct dsc;

//----------------------------
// Bitmap Index Constants
//----------------------------

// Bitmap index type constant (follows GIN = 3)
inline constexpr int IDX_TYPE_BITMAP = 4;

// Bitmap compression strategies
enum BitmapCompressionType : UCHAR
{
    BITMAP_COMPRESSION_NONE = 0,        // No compression
    BITMAP_COMPRESSION_RLE = 1,         // Run-Length Encoding
    BITMAP_COMPRESSION_WAH = 2,         // Word-Aligned Hybrid
    BITMAP_COMPRESSION_EWAH = 3,        // Enhanced Word-Aligned Hybrid
    BITMAP_COMPRESSION_ROARING = 4      // Roaring Bitmaps
};

// Bitmap index configuration constants
inline constexpr ULONG BITMAP_DEFAULT_CHUNK_SIZE = 65536;      // 64KB chunks
inline constexpr ULONG BITMAP_MAX_DISTINCT_VALUES = 10000;     // Maximum distinct values
inline constexpr double BITMAP_CARDINALITY_THRESHOLD = 0.1;    // 10% cardinality threshold
inline constexpr ULONG BITMAP_RLE_MIN_RUN_LENGTH = 4;          // Minimum run length for RLE

// Bitmap operation types
enum BitmapOperationType : UCHAR
{
    BITMAP_OP_AND = 0,          // Bitwise AND operation
    BITMAP_OP_OR = 1,           // Bitwise OR operation
    BITMAP_OP_XOR = 2,          // Bitwise XOR operation
    BITMAP_OP_NOT = 3,          // Bitwise NOT operation
    BITMAP_OP_ANDNOT = 4        // AND NOT operation (A AND (NOT B))
};

//----------------------------
// Bitmap Storage Classes
//----------------------------

/**
 * Compressed bitmap storage with various compression algorithms
 */
class CompressedBitmap
{
public:
    explicit CompressedBitmap(MemoryPool* pool, BitmapCompressionType compression = BITMAP_COMPRESSION_RLE);
    ~CompressedBitmap();

    // Basic bitmap operations
    void setBit(ULONG bit_position);
    void clearBit(ULONG bit_position);
    bool testBit(ULONG bit_position) const;
    
    // Bulk operations
    void setBitRange(ULONG start_bit, ULONG end_bit);
    void clearBitRange(ULONG start_bit, ULONG end_bit);
    void setAll();
    void clearAll();
    
    // Bitmap operations
    void bitwiseAnd(const CompressedBitmap& other);
    void bitwiseOr(const CompressedBitmap& other);
    void bitwiseXor(const CompressedBitmap& other);
    void bitwiseNot();
    void bitwiseAndNot(const CompressedBitmap& other);
    
    // Statistics and properties
    ULONG getSetBitCount() const;
    ULONG getTotalBitCount() const;
    double getCompressionRatio() const;
    ULONG getStorageSize() const;
    bool isEmpty() const;
    
    // Iteration support
    class BitIterator
    {
    public:
        explicit BitIterator(const CompressedBitmap* bitmap);
        bool hasNext() const;
        ULONG getNext();
        void reset();
        
    private:
        const CompressedBitmap* m_bitmap;
        ULONG m_current_position;
        bool m_initialized;
    };
    
    BitIterator getIterator() const;
    
    // Serialization
    ULONG serialize(UCHAR* buffer, ULONG buffer_size) const;
    bool deserialize(const UCHAR* buffer, ULONG buffer_size);
    
    // Compression management
    void compress();
    void decompress();
    bool isCompressed() const;
    BitmapCompressionType getCompressionType() const;
    void setCompressionType(BitmapCompressionType type);

private:
    MemoryPool* m_pool;
    BitmapCompressionType m_compression_type;
    bool m_is_compressed;
    ULONG m_bit_count;
    ULONG m_set_bit_count;
    
    // Storage for compressed/uncompressed data
    UCHAR* m_data;
    ULONG m_data_size;
    ULONG m_allocated_size;
    
    // Compression-specific methods
    void compressRLE();
    void compressWAH();
    void compressEWAH();
    void compressRoaring();
    
    void decompressRLE();
    void decompressWAH();
    void decompressEWAH();
    void decompressRoaring();
    
    // Utility methods
    void ensureCapacity(ULONG required_bits);
    void updateSetBitCount();
    ULONG calculateStorageSize() const;
};

/**
 * Value-to-bitmap mapping for distinct values in the indexed column
 */
template<typename ValueType>
class BitmapValueMap
{
public:
    explicit BitmapValueMap(MemoryPool* pool);
    ~BitmapValueMap();
    
    // Value management
    CompressedBitmap* getBitmapForValue(const ValueType& value, bool create_if_missing = false);
    bool hasValue(const ValueType& value) const;
    void removeValue(const ValueType& value);
    void clearAll();
    
    // Statistics
    ULONG getDistinctValueCount() const;
    ULONG getTotalBitmapSize() const;
    double getAverageCompressionRatio() const;
    
    // Iteration
    typedef typename std::map<ValueType, std::unique_ptr<CompressedBitmap>>::const_iterator const_iterator;
    const_iterator begin() const;
    const_iterator end() const;
    
    // Bulk operations
    void compressAllBitmaps();
    void decompressAllBitmaps();
    
    // Value operations
    std::vector<ValueType> getAllValues() const;
    ValueType getMostFrequentValue() const;
    ValueType getLeastFrequentValue() const;

private:
    MemoryPool* m_pool;
    std::map<ValueType, std::unique_ptr<CompressedBitmap>> m_value_map;
    BitmapCompressionType m_compression_type;
};

//----------------------------
// Bitmap Index Implementation
//----------------------------

/**
 * Main Bitmap Index class implementing the IndexType interface
 */
class BitmapIndex : public IndexType
{
public:
    explicit BitmapIndex();
    virtual ~BitmapIndex();

    // IndexType interface implementation
    virtual bool initialize(thread_db* tdbb, const index_desc* idx) override;
    virtual bool insert(thread_db* tdbb, const dsc* key, RecordNumber record) override;
    virtual bool lookup(thread_db* tdbb, const dsc* key, IndexRetrieval* retrieval) override;
    virtual bool remove(thread_db* tdbb, const dsc* key, RecordNumber record) override;
    virtual double calculateSelectivity(const dsc* key) override;
    virtual void getStatistics(IndexStatistics* stats) override;
    virtual IndexType getType() const override { return IDX_TYPE_BITMAP; }
    
    // Bitmap-specific operations
    bool lookupMultipleValues(thread_db* tdbb, 
                             const std::vector<dsc*>& keys, 
                             BitmapOperationType operation,
                             IndexRetrieval* retrieval);
    
    bool executeComplexQuery(thread_db* tdbb,
                            const BitmapQueryExpression& query,
                            IndexRetrieval* retrieval);
    
    // Index maintenance
    bool rebuild(thread_db* tdbb);
    bool optimize(thread_db* tdbb);
    bool validate(thread_db* tdbb);
    
    // Statistics and analysis
    double getCardinality() const;
    ULONG getDistinctValueCount() const;
    double getCompressionRatio() const;
    ULONG getStorageSize() const;
    bool isSuitableForBitmap(thread_db* tdbb, const index_desc* idx) const;
    
    // Configuration
    void setCompressionType(BitmapCompressionType type);
    BitmapCompressionType getCompressionType() const;
    void setChunkSize(ULONG chunk_size);
    ULONG getChunkSize() const;
    
    // Bitmap operations
    static CompressedBitmap* performBitmapOperation(const CompressedBitmap& left,
                                                   const CompressedBitmap& right,
                                                   BitmapOperationType operation,
                                                   MemoryPool* pool);
    
    // Utility methods
    static bool isLowCardinalityColumn(thread_db* tdbb, const index_desc* idx);
    static double estimateCardinalityRatio(thread_db* tdbb, const index_desc* idx);
    static ULONG estimateDistinctValues(thread_db* tdbb, const index_desc* idx);

protected:
    // Internal bitmap management
    void loadBitmaps(thread_db* tdbb);
    void saveBitmaps(thread_db* tdbb);
    void invalidateCache();
    
    // Value extraction and normalization
    ScratchBird::string extractValueAsString(const dsc* key) const;
    SLONG extractValueAsLong(const dsc* key) const;
    double extractValueAsDouble(const dsc* key) const;
    
    // Bitmap retrieval optimization
    CompressedBitmap* getCachedBitmap(const ScratchBird::string& value);
    void cacheBitmap(const ScratchBird::string& value, CompressedBitmap* bitmap);
    void evictLeastUsedBitmaps();

private:
    // Index configuration
    const index_desc* m_index_descriptor;
    MemoryPool* m_pool;
    BitmapCompressionType m_compression_type;
    ULONG m_chunk_size;
    
    // Bitmap storage
    BitmapValueMap<ScratchBird::string>* m_string_map;
    BitmapValueMap<SLONG>* m_long_map;
    BitmapValueMap<double>* m_double_map;
    
    // Statistics and monitoring
    ULONG m_total_records;
    ULONG m_distinct_values;
    double m_cardinality_ratio;
    ULONG m_cache_hits;
    ULONG m_cache_misses;
    
    // Performance optimization
    mutable ScratchBird::Mutex m_mutex;
    std::map<ScratchBird::string, CompressedBitmap*> m_bitmap_cache;
    ULONG m_max_cache_size;
    
    // Internal helpers
    void updateStatistics();
    bool shouldUseBitmapForQuery(const dsc* key) const;
    void optimizeBitmapStorage();
    void analyzeValueDistribution(thread_db* tdbb);
};

//----------------------------
// Bitmap Query Expression Classes
//----------------------------

/**
 * Abstract base class for bitmap query expressions
 */
class BitmapQueryExpression
{
public:
    virtual ~BitmapQueryExpression() = default;
    virtual CompressedBitmap* evaluate(const BitmapIndex* index, thread_db* tdbb) const = 0;
    virtual double estimateSelectivity(const BitmapIndex* index) const = 0;
    virtual ScratchBird::string toString() const = 0;
};

/**
 * Simple value query expression (column = value)
 */
class BitmapValueExpression : public BitmapQueryExpression
{
public:
    explicit BitmapValueExpression(const dsc* value);
    virtual CompressedBitmap* evaluate(const BitmapIndex* index, thread_db* tdbb) const override;
    virtual double estimateSelectivity(const BitmapIndex* index) const override;
    virtual ScratchBird::string toString() const override;

private:
    dsc m_value;
};

/**
 * Binary operation expression (expr1 AND/OR expr2)
 */
class BitmapBinaryExpression : public BitmapQueryExpression
{
public:
    BitmapBinaryExpression(std::unique_ptr<BitmapQueryExpression> left,
                          std::unique_ptr<BitmapQueryExpression> right,
                          BitmapOperationType op);
    
    virtual CompressedBitmap* evaluate(const BitmapIndex* index, thread_db* tdbb) const override;
    virtual double estimateSelectivity(const BitmapIndex* index) const override;
    virtual ScratchBird::string toString() const override;

private:
    std::unique_ptr<BitmapQueryExpression> m_left;
    std::unique_ptr<BitmapQueryExpression> m_right;
    BitmapOperationType m_operation;
};

/**
 * IN expression (column IN (value1, value2, ...))
 */
class BitmapInExpression : public BitmapQueryExpression
{
public:
    explicit BitmapInExpression(const std::vector<dsc*>& values);
    virtual CompressedBitmap* evaluate(const BitmapIndex* index, thread_db* tdbb) const override;
    virtual double estimateSelectivity(const BitmapIndex* index) const override;
    virtual ScratchBird::string toString() const override;

private:
    std::vector<dsc> m_values;
};

//----------------------------
// Bitmap Index Factory
//----------------------------

/**
 * Factory class for creating bitmap indexes
 */
class BitmapIndexFactory
{
public:
    static std::unique_ptr<BitmapIndex> createBitmapIndex(thread_db* tdbb, 
                                                         const index_desc* idx);
    
    static bool isSuitableForBitmapIndex(thread_db* tdbb, const index_desc* idx);
    static BitmapCompressionType recommendCompressionType(thread_db* tdbb, 
                                                          const index_desc* idx);
    static ULONG recommendChunkSize(thread_db* tdbb, const index_desc* idx);
    
    // Analysis methods
    static double analyzeCardinalityRatio(thread_db* tdbb, const index_desc* idx);
    static ULONG analyzeDistinctValueCount(thread_db* tdbb, const index_desc* idx);
    static ULONG analyzeAverageValueSize(thread_db* tdbb, const index_desc* idx);
};

//----------------------------
// Utility Functions
//----------------------------

// Bitmap index type detection
bool isBitmapIndex(const index_desc* idx);
bool isBitmapIndexType(IndexType type);

// Cardinality analysis
double calculateCardinalityRatio(ULONG distinct_values, ULONG total_records);
bool isLowCardinality(double cardinality_ratio);
ULONG estimateOptimalChunkSize(ULONG total_records, ULONG distinct_values);

// Compression utilities
BitmapCompressionType selectOptimalCompression(ULONG distinct_values, 
                                               ULONG average_bitmap_size,
                                               double sparsity_ratio);
double estimateCompressionRatio(BitmapCompressionType type, 
                               ULONG distinct_values,
                               double sparsity_ratio);

} // namespace Jrd

#endif // JRD_BITMAP_INDEX_H