/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		MultiColumnBitmapIndex.h
 *	DESCRIPTION:	Multi-column bitmap support for composite bitmap indexes
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * 2025.07.23 - ScratchBird Multi-Column Bitmap Index Implementation
 */

#ifndef JRD_MULTI_COLUMN_BITMAP_INDEX_H
#define JRD_MULTI_COLUMN_BITMAP_INDEX_H

#include "../jrd/constants.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "BitmapIndex.h"
#include <vector>
#include <memory>
#include <map>

namespace Jrd {

// Forward declarations
class thread_db;
class MemoryPool;
struct index_desc;
class CompressedBitmap;
class RecordSource;

//----------------------------
// Multi-Column Bitmap Constants
//----------------------------

inline constexpr ULONG MAX_MULTICOLUMN_BITMAP_COLUMNS = 16;    // Maximum columns in composite index
inline constexpr ULONG MULTICOLUMN_VALUE_HASH_SIZE = 65536;    // Hash table size for value combinations
inline constexpr double MULTICOLUMN_CARDINALITY_THRESHOLD = 0.2; // 20% cardinality threshold
inline constexpr ULONG MULTICOLUMN_MIN_SELECTIVITY_BENEFIT = 2; // Minimum selectivity improvement

//----------------------------
// Column Value Encoding Types
//----------------------------

enum MultiColumnValueEncoding : UCHAR
{
    MULTICOLUMN_ENCODING_CONCATENATED = 0,  // Simple string concatenation
    MULTICOLUMN_ENCODING_HASH = 1,          // Hash-based encoding
    MULTICOLUMN_ENCODING_HIERARCHICAL = 2,  // Hierarchical encoding
    MULTICOLUMN_ENCODING_BITPACKED = 3,     // Bit-packed encoding
    MULTICOLUMN_ENCODING_DICTIONARY = 4     // Dictionary-based encoding
};

//----------------------------
// Query Pattern Types
//----------------------------

enum MultiColumnQueryPattern : UCHAR
{
    MULTICOLUMN_QUERY_EXACT = 0,        // Exact match on all columns
    MULTICOLUMN_QUERY_PREFIX = 1,       // Match on leading columns only
    MULTICOLUMN_QUERY_PARTIAL = 2,      // Match on some columns (any position)
    MULTICOLUMN_QUERY_RANGE = 3,        // Range query on ordered columns
    MULTICOLUMN_QUERY_IN_LIST = 4,      // IN list on multiple columns
    MULTICOLUMN_QUERY_MIXED = 5         // Mixed predicates (=, IN, RANGE)
};

//----------------------------
// Multi-Column Value Container
//----------------------------

/**
 * Container for multi-column values with various encoding strategies
 */
class MultiColumnValue
{
public:
    explicit MultiColumnValue(MemoryPool* pool, ULONG column_count);
    ~MultiColumnValue();

    // Value management
    void setColumnValue(ULONG column_index, const dsc* value);
    void setColumnValues(const std::vector<dsc*>& values);
    const dsc* getColumnValue(ULONG column_index) const;
    
    // Encoding and hashing
    ScratchBird::string getEncodedValue(MultiColumnValueEncoding encoding) const;
    ULONG getHashCode() const;
    
    // Comparison operations
    bool equals(const MultiColumnValue& other) const;
    int compare(const MultiColumnValue& other) const;
    bool matches(const MultiColumnValue& pattern, ULONG prefix_length) const;
    
    // Serialization
    ULONG serialize(UCHAR* buffer, ULONG buffer_size) const;
    bool deserialize(const UCHAR* buffer, ULONG buffer_size);
    ULONG getSerializedSize() const;
    
    // Properties
    ULONG getColumnCount() const { return m_column_count; }
    bool isNull(ULONG column_index) const;
    bool hasNullColumns() const;
    
    // String representation
    ScratchBird::string toString() const;

private:
    MemoryPool* m_pool;
    ULONG m_column_count;
    std::vector<dsc> m_column_values;
    std::vector<bool> m_null_flags;
    mutable ULONG m_cached_hash;
    mutable bool m_hash_valid;
    
    // Encoding helpers
    ScratchBird::string encodeConcatenated() const;
    ScratchBird::string encodeHierarchical() const;
    ScratchBird::string encodeBitPacked() const;
    ScratchBird::string encodeDictionary() const;
    
    void updateHash() const;
    ULONG calculateHash() const;
};

//----------------------------
// Multi-Column Bitmap Storage
//----------------------------

/**
 * Storage manager for multi-column bitmap combinations
 */
class MultiColumnBitmapStorage
{
public:
    explicit MultiColumnBitmapStorage(MemoryPool* pool, ULONG column_count,
                                     MultiColumnValueEncoding encoding);
    ~MultiColumnBitmapStorage();

    // Bitmap management
    CompressedBitmap* getBitmapForValue(const MultiColumnValue& value, bool create_if_missing = false);
    bool removeBitmapForValue(const MultiColumnValue& value);
    void clearAllBitmaps();
    
    // Bulk operations
    std::vector<CompressedBitmap*> getBitmapsForPrefix(const MultiColumnValue& prefix,
                                                       ULONG prefix_length) const;
    
    std::vector<CompressedBitmap*> getBitmapsForPattern(const MultiColumnValue& pattern,
                                                        const std::vector<bool>& column_mask) const;
    
    // Statistics and analysis
    ULONG getDistinctValueCount() const;
    ULONG getTotalBitmapCount() const;
    ULONG getAverageBitmapSize() const;
    double getCompressionRatio() const;
    ULONG getStorageSize() const;
    
    // Value distribution analysis
    struct ValueDistribution
    {
        MultiColumnValue value;
        ULONG frequency;
        ULONG bitmap_size;
        double selectivity;
        
        ValueDistribution(const MultiColumnValue& val) : value(val), frequency(0), bitmap_size(0), selectivity(0.0) {}
    };
    
    std::vector<ValueDistribution> analyzeValueDistribution() const;
    
    // Optimization
    void optimize();
    void compressAllBitmaps();
    void rebuildStorage(MultiColumnValueEncoding new_encoding);

private:
    MemoryPool* m_pool;
    ULONG m_column_count;
    MultiColumnValueEncoding m_encoding;
    
    // Storage implementations
    std::map<ScratchBird::string, std::unique_ptr<CompressedBitmap>> m_bitmap_map;  // String-based storage
    std::vector<std::unique_ptr<CompressedBitmap>> m_bitmap_array;                  // Array-based storage
    std::map<ULONG, std::unique_ptr<CompressedBitmap>> m_hash_map;                  // Hash-based storage
    
    // Configuration
    bool m_use_hash_storage;
    bool m_use_array_storage;
    ULONG m_hash_table_size;
    
    // Performance tracking
    ULONG m_lookup_count;
    ULONG m_creation_count;
    ULONG m_collision_count;
    
    // Storage helpers
    ScratchBird::string getStorageKey(const MultiColumnValue& value) const;
    ULONG getHashIndex(const MultiColumnValue& value) const;
    void selectOptimalStorage();
    
    // Bitmap lifecycle management
    void addBitmap(const ScratchBird::string& key, CompressedBitmap* bitmap);
    CompressedBitmap* findBitmap(const ScratchBird::string& key) const;
    void removeBitmap(const ScratchBird::string& key);
};

//----------------------------
// Multi-Column Query Processor
//----------------------------

/**
 * Processes queries against multi-column bitmap indexes
 */
class MultiColumnQueryProcessor
{
public:
    explicit MultiColumnQueryProcessor(MemoryPool* pool);
    ~MultiColumnQueryProcessor();

    // Query processing interface
    CompressedBitmap* processQuery(const MultiColumnBitmapStorage& storage,
                                  MultiColumnQueryPattern pattern,
                                  const std::vector<dsc*>& query_values,
                                  const std::vector<bool>& column_mask) const;
    
    // Pattern-specific processing
    CompressedBitmap* processExactQuery(const MultiColumnBitmapStorage& storage,
                                       const std::vector<dsc*>& values) const;
    
    CompressedBitmap* processPrefixQuery(const MultiColumnBitmapStorage& storage,
                                        const std::vector<dsc*>& prefix_values,
                                        ULONG prefix_length) const;
    
    CompressedBitmap* processPartialQuery(const MultiColumnBitmapStorage& storage,
                                         const std::vector<dsc*>& values,
                                         const std::vector<bool>& column_mask) const;
    
    CompressedBitmap* processRangeQuery(const MultiColumnBitmapStorage& storage,
                                       const std::vector<dsc*>& min_values,
                                       const std::vector<dsc*>& max_values) const;
    
    CompressedBitmap* processInListQuery(const MultiColumnBitmapStorage& storage,
                                        const std::vector<std::vector<dsc*>>& value_lists) const;
    
    CompressedBitmap* processMixedQuery(const MultiColumnBitmapStorage& storage,
                                       const MultiColumnQueryDescriptor& query) const;
    
    // Query optimization
    struct QueryOptimization
    {
        MultiColumnQueryPattern optimal_pattern;
        std::vector<ULONG> column_order;        // Optimal column processing order
        double estimated_selectivity;
        ULONG estimated_result_size;
        bool use_prefix_optimization;           // True if prefix matching beneficial
        bool use_bitmap_intersection;          // True if intersection beneficial
        
        QueryOptimization()
            : optimal_pattern(MULTICOLUMN_QUERY_EXACT), estimated_selectivity(1.0),
              estimated_result_size(0), use_prefix_optimization(false),
              use_bitmap_intersection(true)
        {
        }
    };
    
    QueryOptimization optimizeQuery(const MultiColumnBitmapStorage& storage,
                                   const std::vector<dsc*>& query_values,
                                   const std::vector<bool>& column_mask) const;
    
    // Selectivity estimation
    double estimateQuerySelectivity(const MultiColumnBitmapStorage& storage,
                                   MultiColumnQueryPattern pattern,
                                   const std::vector<dsc*>& query_values) const;

private:
    MemoryPool* m_pool;
    
    // Query execution helpers
    std::vector<CompressedBitmap*> findMatchingBitmaps(const MultiColumnBitmapStorage& storage,
                                                       const MultiColumnValue& pattern,
                                                       const std::vector<bool>& column_mask) const;
    
    CompressedBitmap* combineBitmaps(const std::vector<CompressedBitmap*>& bitmaps,
                                    BitmapOperationType operation) const;
    
    // Range query helpers
    bool isValueInRange(const MultiColumnValue& value,
                       const MultiColumnValue& min_value,
                       const MultiColumnValue& max_value) const;
    
    std::vector<CompressedBitmap*> findBitmapsInRange(const MultiColumnBitmapStorage& storage,
                                                      const MultiColumnValue& min_value,
                                                      const MultiColumnValue& max_value) const;
    
    // Optimization helpers
    std::vector<ULONG> determineOptimalColumnOrder(const MultiColumnBitmapStorage& storage,
                                                   const std::vector<dsc*>& query_values) const;
    
    double calculateColumnSelectivity(const MultiColumnBitmapStorage& storage,
                                     ULONG column_index, const dsc* value) const;
};

//----------------------------
// Multi-Column Query Descriptor
//----------------------------

struct MultiColumnQueryDescriptor
{
    enum ColumnPredicate : UCHAR
    {
        PREDICATE_EQUALS = 0,       // column = value
        PREDICATE_IN_LIST = 1,      // column IN (val1, val2, ...)
        PREDICATE_RANGE = 2,        // column BETWEEN min AND max
        PREDICATE_IS_NULL = 3,      // column IS NULL
        PREDICATE_IS_NOT_NULL = 4,  // column IS NOT NULL
        PREDICATE_IGNORED = 5       // column not used in query
    };
    
    struct ColumnQuery
    {
        ULONG column_index;
        ColumnPredicate predicate;
        std::vector<dsc> values;        // Values for equals/in-list/range
        bool is_range_inclusive_min;    // For range queries
        bool is_range_inclusive_max;    // For range queries
        
        ColumnQuery(ULONG idx) : column_index(idx), predicate(PREDICATE_IGNORED),
                                is_range_inclusive_min(true), is_range_inclusive_max(true) {}
    };
    
    std::vector<ColumnQuery> column_queries;
    MultiColumnQueryPattern overall_pattern;
    double estimated_selectivity;
    
    MultiColumnQueryDescriptor() : overall_pattern(MULTICOLUMN_QUERY_EXACT), estimated_selectivity(1.0) {}
    
    bool hasColumnPredicate(ULONG column_index) const;
    const ColumnQuery* getColumnQuery(ULONG column_index) const;
    std::vector<ULONG> getActiveColumns() const;
};

//----------------------------
// Multi-Column Bitmap Index Implementation
//----------------------------

/**
 * Main multi-column bitmap index class
 */
class MultiColumnBitmapIndex : public BitmapIndex
{
public:
    explicit MultiColumnBitmapIndex(ULONG column_count);
    virtual ~MultiColumnBitmapIndex();

    // IndexType interface implementation
    virtual bool initialize(thread_db* tdbb, const index_desc* idx) override;
    virtual bool insert(thread_db* tdbb, const dsc* key, RecordNumber record) override;
    virtual bool lookup(thread_db* tdbb, const dsc* key, IndexRetrieval* retrieval) override;
    virtual bool remove(thread_db* tdbb, const dsc* key, RecordNumber record) override;
    virtual double calculateSelectivity(const dsc* key) override;
    
    // Multi-column specific interface
    bool insertMultiColumn(thread_db* tdbb, const std::vector<dsc*>& column_values, RecordNumber record);
    
    bool lookupMultiColumn(thread_db* tdbb, const std::vector<dsc*>& column_values,
                          IndexRetrieval* retrieval);
    
    bool lookupWithPattern(thread_db* tdbb, MultiColumnQueryPattern pattern,
                          const std::vector<dsc*>& query_values,
                          const std::vector<bool>& column_mask,
                          IndexRetrieval* retrieval);
    
    bool removeMultiColumn(thread_db* tdbb, const std::vector<dsc*>& column_values, RecordNumber record);
    
    // Query processing
    double calculateMultiColumnSelectivity(const std::vector<dsc*>& column_values) const;
    
    double calculatePatternSelectivity(MultiColumnQueryPattern pattern,
                                      const std::vector<dsc*>& query_values,
                                      const std::vector<bool>& column_mask) const;
    
    // Configuration and optimization
    void setValueEncoding(MultiColumnValueEncoding encoding);
    MultiColumnValueEncoding getValueEncoding() const;
    
    void optimizeForQueryPattern(MultiColumnQueryPattern primary_pattern);
    void rebuildWithOptimalEncoding();
    
    // Statistics and analysis
    struct MultiColumnStatistics : public IndexStatistics
    {
        ULONG column_count;
        ULONG distinct_value_combinations;
        double average_column_selectivity;
        std::vector<double> individual_column_selectivities;
        MultiColumnValueEncoding current_encoding;
        ULONG prefix_query_count;
        ULONG partial_query_count;
        ULONG exact_query_count;
        
        MultiColumnStatistics() : column_count(0), distinct_value_combinations(0),
                                 average_column_selectivity(1.0), current_encoding(MULTICOLUMN_ENCODING_CONCATENATED),
                                 prefix_query_count(0), partial_query_count(0), exact_query_count(0) {}
    };
    
    void getMultiColumnStatistics(MultiColumnStatistics* stats) const;
    
    // Maintenance operations
    bool validateConsistency(thread_db* tdbb) const;
    void optimizeStorage();
    void analyzeQueryPatterns();

protected:
    // Multi-column key handling
    MultiColumnValue extractMultiColumnValue(const dsc* composite_key) const;
    MultiColumnValue extractMultiColumnValue(const std::vector<dsc*>& column_values) const;
    
    dsc* encodeCompositeKey(const MultiColumnValue& multi_value) const;
    
    // Storage management
    void initializeStorage(thread_db* tdbb);
    void loadExistingBitmaps(thread_db* tdbb);
    void saveAllBitmaps(thread_db* tdbb);

private:
    ULONG m_column_count;
    std::vector<USHORT> m_column_field_ids;
    std::vector<UCHAR> m_column_data_types;
    
    std::unique_ptr<MultiColumnBitmapStorage> m_storage;
    std::unique_ptr<MultiColumnQueryProcessor> m_query_processor;
    
    MultiColumnValueEncoding m_value_encoding;
    MultiColumnQueryPattern m_primary_query_pattern;
    
    // Performance tracking
    mutable MultiColumnStatistics m_statistics;
    mutable ScratchBird::Mutex m_statistics_mutex;
    
    // Query pattern analysis
    std::map<MultiColumnQueryPattern, ULONG> m_query_pattern_frequency;
    std::vector<ULONG> m_column_usage_frequency;
    
    // Internal helpers
    void updateQueryPatternStatistics(MultiColumnQueryPattern pattern) const;
    void updateColumnUsageStatistics(const std::vector<bool>& column_mask) const;
    
    bool isCompositeKeyValid(const dsc* composite_key) const;
    bool areColumnValuesValid(const std::vector<dsc*>& column_values) const;
    
    // Encoding optimization
    MultiColumnValueEncoding determineOptimalEncoding() const;
    void migrateToNewEncoding(MultiColumnValueEncoding new_encoding);
};

//----------------------------
// Multi-Column Bitmap Index Factory
//----------------------------

/**
 * Factory for creating optimized multi-column bitmap indexes
 */
class MultiColumnBitmapIndexFactory
{
public:
    // Index creation
    static std::unique_ptr<MultiColumnBitmapIndex> createIndex(thread_db* tdbb,
                                                              const index_desc* idx);
    
    // Suitability analysis
    static bool isSuitableForMultiColumnBitmap(thread_db* tdbb, const index_desc* idx);
    
    static double estimateMultiColumnCardinality(thread_db* tdbb, const index_desc* idx);
    
    // Configuration recommendations
    static MultiColumnValueEncoding recommendEncoding(thread_db* tdbb, const index_desc* idx);
    
    static MultiColumnQueryPattern predictPrimaryQueryPattern(thread_db* tdbb, const index_desc* idx);
    
    // Performance analysis
    struct PerformanceAnalysis
    {
        bool is_beneficial;                     // True if multi-column bitmap is beneficial
        double expected_selectivity_improvement; // Expected selectivity improvement ratio
        ULONG storage_overhead_percentage;      // Storage overhead vs single-column indexes
        double query_performance_improvement;   // Expected query performance improvement
        MultiColumnValueEncoding recommended_encoding;
        
        PerformanceAnalysis()
            : is_beneficial(false), expected_selectivity_improvement(1.0),
              storage_overhead_percentage(0), query_performance_improvement(1.0),
              recommended_encoding(MULTICOLUMN_ENCODING_CONCATENATED)
        {
        }
    };
    
    static PerformanceAnalysis analyzePerformanceBenefit(thread_db* tdbb, const index_desc* idx);

private:
    // Analysis helpers
    static std::vector<double> analyzeColumnSelectivities(thread_db* tdbb, const index_desc* idx);
    static std::vector<std::vector<ULONG>> analyzeValueCombinations(thread_db* tdbb, const index_desc* idx);
    static double estimateCorrelationBetweenColumns(thread_db* tdbb, const index_desc* idx,
                                                   ULONG col1, ULONG col2);
    
    // Workload analysis
    static std::map<MultiColumnQueryPattern, ULONG> analyzeQueryWorkload(thread_db* tdbb,
                                                                         const index_desc* idx);
    
    static std::vector<bool> analyzeColumnUsagePatterns(thread_db* tdbb, const index_desc* idx);
};

//----------------------------
// Multi-Column Bitmap Table Scan
//----------------------------

/**
 * Specialized record source for multi-column bitmap scans
 */
class MultiColumnBitmapTableScan : public RecordSource
{
public:
    MultiColumnBitmapTableScan(CompilerScratch* csb, jrd_rel* relation,
                              MultiColumnBitmapIndex* index,
                              MultiColumnQueryPattern pattern,
                              const std::vector<dsc*>& query_values,
                              const std::vector<bool>& column_mask);
    
    virtual ~MultiColumnBitmapTableScan();

    // RecordSource interface
    virtual void open(thread_db* tdbb) override;
    virtual void close(thread_db* tdbb) override;
    virtual bool getRecord(thread_db* tdbb) override;
    virtual bool refetchRecord(thread_db* tdbb) override;
    virtual WriteLockResult lockRecord(thread_db* tdbb, bool skipLocked = false) override;
    virtual void markRecursive() override;
    virtual void invalidateRecords(jrd_req* request) override;
    
    // Cost and cardinality
    virtual double getCost() const override;
    virtual ULONG getCardinality() const override;
    
    // Multi-column specific
    void setQueryPattern(MultiColumnQueryPattern pattern);
    void setColumnMask(const std::vector<bool>& column_mask);
    MultiColumnBitmapIndex::MultiColumnStatistics getScanStatistics() const;

private:
    CompilerScratch* m_csb;
    jrd_rel* m_relation;
    MultiColumnBitmapIndex* m_index;
    
    MultiColumnQueryPattern m_query_pattern;
    std::vector<dsc> m_query_values;
    std::vector<bool> m_column_mask;
    
    CompressedBitmap* m_result_bitmap;
    CompressedBitmap::BitIterator m_bitmap_iterator;
    bool m_is_open;
    ULONG m_current_position;
    
    // Performance tracking
    ULONG m_records_scanned;
    ULONG m_records_returned;
    ULONG m_bitmap_operations;
    
    void initializeBitmapIterator(thread_db* tdbb);
    bool fetchNextRecordFromBitmap(thread_db* tdbb);
    void updateScanStatistics();
};

//----------------------------
// Utility Functions
//----------------------------

// Multi-column bitmap index detection
bool isMultiColumnBitmapIndex(const index_desc* idx);
ULONG getMultiColumnBitmapColumnCount(const index_desc* idx);

// Value encoding utilities
ScratchBird::string encodeMultiColumnValue(const std::vector<dsc*>& values,
                                          MultiColumnValueEncoding encoding);

MultiColumnValue decodeMultiColumnValue(const ScratchBird::string& encoded_value,
                                       MultiColumnValueEncoding encoding,
                                       ULONG column_count,
                                       MemoryPool* pool);

// Query pattern utilities
MultiColumnQueryPattern detectQueryPattern(const std::vector<dsc*>& query_values,
                                           const std::vector<bool>& column_mask);

bool isQueryPatternOptimal(MultiColumnQueryPattern pattern,
                          const MultiColumnBitmapIndex::MultiColumnStatistics& stats);

// Cardinality estimation
double estimateMultiColumnSelectivity(const std::vector<double>& individual_selectivities,
                                     const std::vector<std::vector<double>>& correlation_matrix);

ULONG estimateResultCardinality(ULONG total_records,
                               const std::vector<double>& column_selectivities,
                               MultiColumnQueryPattern pattern);

} // namespace Jrd

#endif // JRD_MULTI_COLUMN_BITMAP_INDEX_H