/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		CompressedHashStorage.h
 *	DESCRIPTION:	Compressed hash bucket storage with LZ4/Zstandard algorithms
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
 * 2025.07.23 - ScratchBird Compressed Hash Storage Implementation
 */

#ifndef JRD_COMPRESSED_HASH_STORAGE_H
#define JRD_COMPRESSED_HASH_STORAGE_H

#include "../jrd/constants.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "PersistentHashStorage.h"
#include <vector>
#include <memory>

namespace Jrd {

// Forward declarations
class MemoryPool;
class thread_db;

//----------------------------
// Compression Algorithm Types
//----------------------------

enum HashCompressionAlgorithm : UCHAR
{
    HASH_COMPRESSION_NONE = 0,          // No compression
    HASH_COMPRESSION_LZ4 = 1,           // LZ4 fast compression
    HASH_COMPRESSION_LZ4HC = 2,         // LZ4 high compression
    HASH_COMPRESSION_ZSTD = 3,          // Zstandard compression
    HASH_COMPRESSION_ZSTD_FAST = 4,     // Zstandard fast mode
    HASH_COMPRESSION_SNAPPY = 5,        // Snappy compression
    HASH_COMPRESSION_DEFLATE = 6,       // Deflate/zlib compression
    HASH_COMPRESSION_ADAPTIVE = 7       // Adaptive algorithm selection
};

//----------------------------
// Compression Quality Levels
//----------------------------

enum HashCompressionLevel : UCHAR
{
    HASH_COMPRESSION_FASTEST = 1,       // Fastest compression (lowest ratio)
    HASH_COMPRESSION_FAST = 3,          // Fast compression
    HASH_COMPRESSION_BALANCED = 6,      // Balanced speed/ratio
    HASH_COMPRESSION_BEST = 9,          // Best compression ratio
    HASH_COMPRESSION_ULTRA = 12         // Ultra compression (slowest)
};

//----------------------------
// Compression Statistics
//----------------------------

struct HashCompressionStatistics
{
    ULONG total_buckets_compressed;     // Total buckets compressed
    ULONG total_uncompressed_size;      // Total uncompressed data size
    ULONG total_compressed_size;        // Total compressed data size
    double average_compression_ratio;   // Average compression ratio
    double best_compression_ratio;      // Best compression ratio achieved
    double worst_compression_ratio;     // Worst compression ratio
    
    ULONG compression_time_ms;          // Total compression time
    ULONG decompression_time_ms;        // Total decompression time
    double average_compression_speed;   // MB/s compression speed
    double average_decompression_speed; // MB/s decompression speed
    
    // Algorithm usage statistics
    ULONG lz4_compressions;             // LZ4 compressions performed
    ULONG zstd_compressions;            // Zstandard compressions performed
    ULONG snappy_compressions;          // Snappy compressions performed
    ULONG adaptive_selections;          // Adaptive algorithm selections
    
    HashCompressionStatistics()
        : total_buckets_compressed(0), total_uncompressed_size(0), total_compressed_size(0),
          average_compression_ratio(1.0), best_compression_ratio(1.0), worst_compression_ratio(1.0),
          compression_time_ms(0), decompression_time_ms(0),
          average_compression_speed(0.0), average_decompression_speed(0.0),
          lz4_compressions(0), zstd_compressions(0), snappy_compressions(0), adaptive_selections(0)
    {
    }
    
    void updateRatios()
    {
        if (total_uncompressed_size > 0) {
            average_compression_ratio = static_cast<double>(total_uncompressed_size) / total_compressed_size;
        }
    }
    
    ULONG getTotalSpaceSaved() const
    {
        return total_uncompressed_size > total_compressed_size ? 
               total_uncompressed_size - total_compressed_size : 0;
    }
    
    double getSpaceSavingsPercentage() const
    {
        return total_uncompressed_size > 0 ? 
               (static_cast<double>(getTotalSpaceSaved()) / total_uncompressed_size) * 100.0 : 0.0;
    }
};

//----------------------------
// Compressed Bucket Header
//----------------------------

struct compressed_bucket_header
{
    ULONG cbh_signature;                // Compression signature/magic number
    USHORT cbh_compression_algorithm;   // Compression algorithm used
    USHORT cbh_compression_level;       // Compression level used
    ULONG cbh_uncompressed_size;        // Original uncompressed size
    ULONG cbh_compressed_size;          // Compressed data size
    ULONG cbh_checksum;                 // CRC32 checksum of compressed data
    USHORT cbh_entry_count;             // Number of entries in bucket
    USHORT cbh_flags;                   // Compression flags
    GDS_TIMESTAMP cbh_compression_time; // When compression was performed
    
    // Compression flags
    static constexpr USHORT CBH_DICTIONARY_COMPRESSED = 0x0001;  // Uses dictionary compression
    static constexpr USHORT CBH_BLOCK_COMPRESSED = 0x0002;       // Block-based compression
    static constexpr USHORT CBH_STREAMING_COMPRESSED = 0x0004;   // Streaming compression
    static constexpr USHORT CBH_ENCRYPTED = 0x0008;              // Compressed data is encrypted
    static constexpr USHORT CBH_INTEGRITY_CHECKED = 0x0010;      // Has integrity checksum
    
    static constexpr ULONG CBH_SIGNATURE_VALUE = 0x48435342;     // "HCSB" - Hash Compressed Storage Bucket
    
    bool isValid() const
    {
        return cbh_signature == CBH_SIGNATURE_VALUE && 
               cbh_compressed_size > 0 && 
               cbh_uncompressed_size > 0;
    }
    
    double getCompressionRatio() const
    {
        return cbh_compressed_size > 0 ? 
               static_cast<double>(cbh_uncompressed_size) / cbh_compressed_size : 1.0;
    }
};

//----------------------------
// Compression Context
//----------------------------

struct HashCompressionContext
{
    HashCompressionAlgorithm algorithm;     // Selected algorithm
    HashCompressionLevel level;             // Compression level
    ULONG dictionary_size;                  // Dictionary size (if applicable)
    UCHAR* dictionary_data;                 // Dictionary data
    ULONG window_size;                      // Compression window size
    bool enable_checksum;                   // Enable integrity checksums
    
    // Algorithm-specific parameters
    union {
        struct {
            bool use_hc_mode;               // Use high compression mode
            ULONG acceleration;             // LZ4 acceleration factor
        } lz4_params;
        
        struct {
            ULONG compression_level;        // Zstd compression level (1-22)
            bool use_long_distance_matching; // Enable long distance matching
            ULONG window_log;              // Window size log
        } zstd_params;
        
        struct {
            ULONG block_size;              // Snappy block size
            bool use_parallel_compression; // Enable parallel compression
        } snappy_params;
    };
    
    HashCompressionContext()
        : algorithm(HASH_COMPRESSION_LZ4), level(HASH_COMPRESSION_BALANCED),
          dictionary_size(0), dictionary_data(nullptr), window_size(65536),
          enable_checksum(true)
    {
        memset(&lz4_params, 0, sizeof(lz4_params));
        lz4_params.acceleration = 1;
    }
};

//----------------------------
// Compression Result
//----------------------------

struct HashCompressionResult
{
    bool success;                           // True if compression succeeded
    ULONG compressed_size;                  // Size of compressed data
    ULONG compression_time_ms;              // Time taken to compress
    double compression_ratio;               // Achieved compression ratio
    ULONG checksum;                         // Data integrity checksum
    ScratchBird::string error_message;      // Error message if failed
    
    HashCompressionResult()
        : success(false), compressed_size(0), compression_time_ms(0),
          compression_ratio(1.0), checksum(0)
    {
    }
};

//----------------------------
// Decompression Result
//----------------------------

struct HashDecompressionResult
{
    bool success;                           // True if decompression succeeded
    ULONG decompressed_size;                // Size of decompressed data
    ULONG decompression_time_ms;            // Time taken to decompress
    bool checksum_valid;                    // True if checksum validation passed
    ScratchBird::string error_message;      // Error message if failed
    
    HashDecompressionResult()
        : success(false), decompressed_size(0), decompression_time_ms(0),
          checksum_valid(true)
    {
    }
};

//----------------------------
// Hash Compression Engine
//----------------------------

/**
 * Main compression engine for hash bucket data
 */
class HashCompressionEngine
{
public:
    explicit HashCompressionEngine(MemoryPool* pool);
    ~HashCompressionEngine();

    // Compression operations
    HashCompressionResult compress(const UCHAR* input_data, ULONG input_size,
                                  UCHAR* output_buffer, ULONG output_buffer_size,
                                  const HashCompressionContext& context);
    
    HashDecompressionResult decompress(const UCHAR* compressed_data, ULONG compressed_size,
                                      UCHAR* output_buffer, ULONG output_buffer_size,
                                      const compressed_bucket_header& header);
    
    // Algorithm-specific compression
    HashCompressionResult compressLZ4(const UCHAR* input_data, ULONG input_size,
                                     UCHAR* output_buffer, ULONG output_buffer_size,
                                     const HashCompressionContext& context);
    
    HashCompressionResult compressZSTD(const UCHAR* input_data, ULONG input_size,
                                      UCHAR* output_buffer, ULONG output_buffer_size,
                                      const HashCompressionContext& context);
    
    HashCompressionResult compressSnappy(const UCHAR* input_data, ULONG input_size,
                                        UCHAR* output_buffer, ULONG output_buffer_size,
                                        const HashCompressionContext& context);
    
    HashCompressionResult compressDeflate(const UCHAR* input_data, ULONG input_size,
                                         UCHAR* output_buffer, ULONG output_buffer_size,
                                         const HashCompressionContext& context);
    
    // Algorithm-specific decompression
    HashDecompressionResult decompressLZ4(const UCHAR* compressed_data, ULONG compressed_size,
                                         UCHAR* output_buffer, ULONG output_buffer_size);
    
    HashDecompressionResult decompressZSTD(const UCHAR* compressed_data, ULONG compressed_size,
                                          UCHAR* output_buffer, ULONG output_buffer_size);
    
    HashDecompressionResult decompressSnappy(const UCHAR* compressed_data, ULONG compressed_size,
                                            UCHAR* output_buffer, ULONG output_buffer_size);
    
    HashDecompressionResult decompressDeflate(const UCHAR* compressed_data, ULONG compressed_size,
                                             UCHAR* output_buffer, ULONG output_buffer_size);
    
    // Adaptive algorithm selection
    HashCompressionAlgorithm selectBestAlgorithm(const UCHAR* sample_data, ULONG sample_size);
    
    HashCompressionLevel selectOptimalLevel(HashCompressionAlgorithm algorithm,
                                           const UCHAR* sample_data, ULONG sample_size,
                                           double target_speed_ratio = 0.5);
    
    // Compression estimation
    ULONG estimateCompressedSize(const UCHAR* input_data, ULONG input_size,
                                HashCompressionAlgorithm algorithm) const;
    
    double estimateCompressionRatio(const UCHAR* input_data, ULONG input_size,
                                   HashCompressionAlgorithm algorithm) const;
    
    ULONG estimateCompressionTime(ULONG input_size, HashCompressionAlgorithm algorithm,
                                 HashCompressionLevel level) const;
    
    // Dictionary management
    bool buildCompressionDictionary(const std::vector<std::pair<UCHAR*, ULONG>>& training_data,
                                   ULONG dictionary_size, UCHAR* dictionary_buffer);
    
    bool loadCompressionDictionary(const UCHAR* dictionary_data, ULONG dictionary_size);
    
    void clearCompressionDictionary();
    
    // Buffer size calculation
    ULONG getMaxCompressedSize(ULONG input_size, HashCompressionAlgorithm algorithm) const;
    ULONG getRecommendedBufferSize(ULONG input_size, HashCompressionAlgorithm algorithm) const;
    
    // Statistics and monitoring
    HashCompressionStatistics getStatistics() const;
    void resetStatistics();
    
    // Configuration
    void setDefaultCompressionLevel(HashCompressionLevel level);
    HashCompressionLevel getDefaultCompressionLevel() const;
    
    void enableChecksumValidation(bool enabled);
    bool isChecksumValidationEnabled() const;

private:
    MemoryPool* m_pool;
    
    // Configuration
    HashCompressionLevel m_default_compression_level;
    bool m_checksum_validation_enabled;
    
    // Statistics
    mutable HashCompressionStatistics m_statistics;
    mutable ScratchBird::Mutex m_statistics_mutex;
    
    // Dictionary storage
    UCHAR* m_dictionary_data;
    ULONG m_dictionary_size;
    
    // Algorithm contexts (for algorithms that support reusable contexts)
    void* m_lz4_context;
    void* m_zstd_context;
    void* m_deflate_context;
    
    // Compression helpers
    void updateCompressionStatistics(HashCompressionAlgorithm algorithm,
                                   const HashCompressionResult& result);
    
    void updateDecompressionStatistics(const HashDecompressionResult& result);
    
    ULONG calculateChecksum(const UCHAR* data, ULONG size) const;
    bool validateChecksum(const UCHAR* data, ULONG size, ULONG expected_checksum) const;
    
    // Algorithm availability checking
    bool isAlgorithmAvailable(HashCompressionAlgorithm algorithm) const;
    void initializeAlgorithmContexts();
    void cleanupAlgorithmContexts();
    
    // Performance testing for algorithm selection
    struct AlgorithmPerformance
    {
        HashCompressionAlgorithm algorithm;
        double compression_ratio;
        double compression_speed;    // MB/s
        double decompression_speed;  // MB/s
        double overall_score;        // Combined performance score
        
        AlgorithmPerformance(HashCompressionAlgorithm alg)
            : algorithm(alg), compression_ratio(1.0), compression_speed(0.0),
              decompression_speed(0.0), overall_score(0.0)
        {
        }
    };
    
    std::vector<AlgorithmPerformance> benchmarkAlgorithms(const UCHAR* sample_data,
                                                          ULONG sample_size) const;
    
    double calculateAlgorithmScore(const AlgorithmPerformance& perf,
                                  double speed_weight = 0.6,
                                  double ratio_weight = 0.4) const;
};

//----------------------------
// Compressed Hash Bucket
//----------------------------

/**
 * Extended hash bucket with compression support
 */
class CompressedHashBucket
{
public:
    explicit CompressedHashBucket(MemoryPool* pool, ULONG bucket_id);
    ~CompressedHashBucket();

    // Bucket lifecycle
    bool loadFromPage(thread_db* tdbb, BufferControl* page_bcb);
    bool savePage(thread_db* tdbb, BufferControl* page_bcb);
    
    // Entry operations
    bool insertEntry(const hash_entry* entry);
    hash_entry* findEntry(ULONG hash_value, const UCHAR* key_data, USHORT key_length);
    bool deleteEntry(ULONG hash_value, const UCHAR* key_data, USHORT key_length);
    bool updateEntry(const hash_entry* old_entry, const hash_entry* new_entry);
    
    // Compression management
    bool compressBucket(const HashCompressionContext& context);
    bool decompressBucket();
    bool isCompressed() const;
    
    // Compression configuration
    void setCompressionEnabled(bool enabled);
    bool isCompressionEnabled() const;
    
    void setCompressionThreshold(ULONG threshold_bytes);
    ULONG getCompressionThreshold() const;
    
    void setCompressionAlgorithm(HashCompressionAlgorithm algorithm);
    HashCompressionAlgorithm getCompressionAlgorithm() const;
    
    // Statistics
    ULONG getUncompressedSize() const;
    ULONG getCompressedSize() const;
    double getCompressionRatio() const;
    ULONG getEntryCount() const;
    
    // Maintenance
    bool needsCompression() const;
    bool needsDecompression() const;
    bool validateIntegrity() const;
    void compactBucket();

private:
    MemoryPool* m_pool;
    ULONG m_bucket_id;
    
    // Bucket data
    std::vector<hash_entry*> m_entries;
    UCHAR* m_raw_data;              // Raw bucket data
    ULONG m_raw_data_size;
    
    // Compression state
    bool m_is_compressed;
    bool m_compression_enabled;
    HashCompressionAlgorithm m_compression_algorithm;
    ULONG m_compression_threshold;
    
    // Compressed data
    UCHAR* m_compressed_data;
    ULONG m_compressed_size;
    compressed_bucket_header m_compression_header;
    
    // Statistics
    ULONG m_uncompressed_size;
    ULONG m_last_compression_size;
    GDS_TIMESTAMP m_last_compression_time;
    
    // Compression engine
    std::unique_ptr<HashCompressionEngine> m_compression_engine;
    
    // Internal operations
    bool serializeEntries(UCHAR* buffer, ULONG buffer_size, ULONG& serialized_size);
    bool deserializeEntries(const UCHAR* buffer, ULONG buffer_size);
    
    void clearEntries();
    void updateStatistics();
    
    // Compression helpers
    bool shouldCompress() const;
    bool performCompression(const HashCompressionContext& context);
    bool performDecompression();
};

//----------------------------
// Compressed Hash Storage Manager
//----------------------------

/**
 * Manager for compressed hash storage operations
 */
class CompressedHashStorageManager
{
public:
    explicit CompressedHashStorageManager(MemoryPool* pool);
    ~CompressedHashStorageManager();

    // Storage operations
    bool initializeCompressedStorage(thread_db* tdbb, const index_desc* idx,
                                   HashCompressionAlgorithm default_algorithm = HASH_COMPRESSION_LZ4);
    
    bool insertCompressedEntry(thread_db* tdbb, ULONG bucket_id,
                             const hash_entry* entry);
    
    hash_entry* findCompressedEntry(thread_db* tdbb, ULONG bucket_id,
                                   ULONG hash_value, const UCHAR* key_data,
                                   USHORT key_length);
    
    bool deleteCompressedEntry(thread_db* tdbb, ULONG bucket_id,
                             ULONG hash_value, const UCHAR* key_data,
                             USHORT key_length);
    
    // Batch operations
    bool compressAllBuckets(thread_db* tdbb, const HashCompressionContext& context);
    bool decompressAllBuckets(thread_db* tdbb);
    
    // Compression policy management
    void setGlobalCompressionPolicy(const HashCompressionContext& policy);
    HashCompressionContext getGlobalCompressionPolicy() const;
    
    void setBucketCompressionPolicy(ULONG bucket_id, const HashCompressionContext& policy);
    HashCompressionContext getBucketCompressionPolicy(ULONG bucket_id) const;
    
    // Adaptive compression
    void enableAdaptiveCompression(bool enabled);
    bool isAdaptiveCompressionEnabled() const;
    
    void analyzeAndOptimizeCompression(thread_db* tdbb);
    
    // Statistics and monitoring
    HashCompressionStatistics getGlobalCompressionStatistics() const;
    HashCompressionStatistics getBucketCompressionStatistics(ULONG bucket_id) const;
    
    void resetCompressionStatistics();
    
    // Maintenance operations
    bool validateCompressedStorage(thread_db* tdbb, ScratchBird::string& error_report);
    bool repairCompressedStorage(thread_db* tdbb, const ScratchBird::string& repair_options);
    bool optimizeCompressedStorage(thread_db* tdbb);
    
    // Configuration
    void setCompressionThreshold(ULONG threshold_bytes);
    ULONG getCompressionThreshold() const;
    
    void setMaxCompressionLevel(HashCompressionLevel max_level);
    HashCompressionLevel getMaxCompressionLevel() const;
    
    // Performance analysis
    struct CompressionPerformanceAnalysis
    {
        double storage_savings_percentage;      // Storage space saved
        double read_performance_impact;         // Read operation impact
        double write_performance_impact;        // Write operation impact
        double cpu_overhead_percentage;         // CPU overhead
        bool compression_beneficial;            // Overall benefit assessment
        ScratchBird::string recommendations;    // Optimization recommendations
        
        CompressionPerformanceAnalysis()
            : storage_savings_percentage(0.0), read_performance_impact(0.0),
              write_performance_impact(0.0), cpu_overhead_percentage(0.0),
              compression_beneficial(false)
        {
        }
    };
    
    CompressionPerformanceAnalysis analyzeCompressionPerformance(thread_db* tdbb) const;

private:
    MemoryPool* m_pool;
    
    // Storage configuration
    const index_desc* m_index_descriptor;
    HashCompressionContext m_global_compression_policy;
    std::map<ULONG, HashCompressionContext> m_bucket_policies;
    
    // Compression management
    std::unique_ptr<HashCompressionEngine> m_compression_engine;
    std::map<ULONG, std::unique_ptr<CompressedHashBucket>> m_compressed_buckets;
    
    // Configuration
    bool m_adaptive_compression_enabled;
    ULONG m_compression_threshold;
    HashCompressionLevel m_max_compression_level;
    
    // Statistics
    mutable HashCompressionStatistics m_global_statistics;
    mutable ScratchBird::Mutex m_statistics_mutex;
    
    // Bucket management
    CompressedHashBucket* getOrCreateBucket(ULONG bucket_id);
    void releaseBucket(ULONG bucket_id);
    void evictUnusedBuckets(ULONG max_cached_buckets = 100);
    
    // Adaptive compression helpers
    void analyzeCompressionEffectiveness(thread_db* tdbb);
    void adjustCompressionPolicies(const std::vector<HashCompressionStatistics>& bucket_stats);
    
    // Performance monitoring
    void recordCompressionOperation(ULONG bucket_id, const HashCompressionResult& result);
    void recordDecompressionOperation(ULONG bucket_id, const HashDecompressionResult& result);
    
    // Maintenance helpers
    bool validateBucketCompression(thread_db* tdbb, ULONG bucket_id, ScratchBird::string& errors);
    bool repairBucketCompression(thread_db* tdbb, ULONG bucket_id);
};

//----------------------------
// Integration with Persistent Hash Storage
//----------------------------

/**
 * Integration layer between compression and persistent hash storage
 */
class HashStorageCompressionIntegration
{
public:
    // Integration setup
    static bool enableCompressionForStorage(PersistentHashStorage* storage,
                                           HashCompressionAlgorithm algorithm = HASH_COMPRESSION_LZ4);
    
    static bool disableCompressionForStorage(PersistentHashStorage* storage);
    
    // Compression hooks
    static bool compressBeforeWrite(PersistentHashStorage* storage, ULONG bucket_id,
                                   UCHAR* bucket_data, ULONG data_size,
                                   UCHAR* compressed_buffer, ULONG& compressed_size);
    
    static bool decompressAfterRead(PersistentHashStorage* storage, ULONG bucket_id,
                                   const UCHAR* compressed_data, ULONG compressed_size,
                                   UCHAR* decompressed_buffer, ULONG& decompressed_size);
    
    // Statistics integration
    static void updateCompressionStatistics(PersistentHashStorage* storage,
                                           const HashCompressionStatistics& stats);
    
    static HashCompressionStatistics getStorageCompressionStatistics(PersistentHashStorage* storage);

private:
    static std::map<PersistentHashStorage*, std::unique_ptr<CompressedHashStorageManager>> s_storage_managers;
    static ScratchBird::Mutex s_integration_mutex;
};

//----------------------------
// Utility Functions
//----------------------------

// Compression algorithm utilities
const char* getCompressionAlgorithmName(HashCompressionAlgorithm algorithm);
HashCompressionAlgorithm parseCompressionAlgorithmName(const char* name);

// Compression analysis utilities
double calculateCompressionEfficiency(ULONG original_size, ULONG compressed_size,
                                     ULONG compression_time_ms, ULONG decompression_time_ms);

bool isCompressionBeneficial(const HashCompressionStatistics& stats,
                           double min_ratio_threshold = 1.1,
                           double max_cpu_overhead = 0.2);

// Algorithm selection utilities
HashCompressionAlgorithm selectOptimalCompressionAlgorithm(
    const std::vector<std::pair<UCHAR*, ULONG>>& sample_data,
    double speed_preference = 0.6);  // 0.0 = prefer ratio, 1.0 = prefer speed

// Buffer size estimation
ULONG estimateCompressionBufferSize(ULONG input_size, HashCompressionAlgorithm algorithm);
ULONG calculateOptimalChunkSize(ULONG total_size, HashCompressionAlgorithm algorithm);

} // namespace Jrd

#endif // JRD_COMPRESSED_HASH_STORAGE_H