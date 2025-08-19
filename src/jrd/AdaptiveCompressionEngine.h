/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		AdaptiveCompressionEngine.h
 *	DESCRIPTION:	Adaptive compression algorithm selection for bitmap indexes
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
 * 2025.07.23 - ScratchBird Adaptive Compression Engine
 */

#ifndef JRD_ADAPTIVE_COMPRESSION_ENGINE_H
#define JRD_ADAPTIVE_COMPRESSION_ENGINE_H

#include "../jrd/constants.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "BitmapIndex.h"
#include <vector>
#include <memory>

namespace Jrd {

// Forward declarations
class MemoryPool;
class CompressedBitmap;

//----------------------------
// Data Pattern Analysis Constants
//----------------------------

inline constexpr double PATTERN_SPARSE_THRESHOLD = 0.1;        // <10% density = sparse
inline constexpr double PATTERN_DENSE_THRESHOLD = 0.7;         // >70% density = dense
inline constexpr double PATTERN_RUN_THRESHOLD = 0.3;           // >30% runs = run-friendly
inline constexpr ULONG PATTERN_MIN_SAMPLE_SIZE = 1000;         // Minimum bits for analysis
inline constexpr ULONG PATTERN_MAX_SAMPLE_SIZE = 100000;       // Maximum bits for analysis
inline constexpr double PATTERN_NOISE_THRESHOLD = 0.05;        // <5% noise = clean pattern

//----------------------------
// Data Pattern Types
//----------------------------

enum BitmapDataPattern : UCHAR
{
    PATTERN_UNKNOWN = 0,            // Pattern not yet determined
    PATTERN_SPARSE_RANDOM = 1,      // Sparse with random distribution
    PATTERN_SPARSE_CLUSTERED = 2,   // Sparse with clustering
    PATTERN_DENSE_RANDOM = 3,       // Dense with random distribution
    PATTERN_DENSE_CLUSTERED = 4,    // Dense with clustering
    PATTERN_RUN_LENGTH = 5,         // Long consecutive runs
    PATTERN_ALTERNATING = 6,        // Alternating patterns
    PATTERN_PERIODIC = 7,           // Periodic/cyclic patterns
    PATTERN_MIXED = 8               // Multiple pattern types
};

//----------------------------
// Compression Performance Metrics
//----------------------------

struct CompressionMetrics
{
    BitmapCompressionType algorithm;        // Compression algorithm used
    ULONG original_size;                    // Original data size
    ULONG compressed_size;                  // Compressed data size
    double compression_ratio;               // Compression ratio achieved
    ULONG compression_time_ms;              // Time to compress (milliseconds)
    ULONG decompression_time_ms;            // Time to decompress (milliseconds)
    double cpu_efficiency;                  // CPU cycles per byte
    double memory_overhead;                 // Memory overhead percentage
    bool compression_successful;            // True if compression succeeded
    
    CompressionMetrics()
        : algorithm(BITMAP_COMPRESSION_NONE), original_size(0), compressed_size(0),
          compression_ratio(1.0), compression_time_ms(0), decompression_time_ms(0),
          cpu_efficiency(0.0), memory_overhead(0.0), compression_successful(false)
    {
    }
    
    double getOverallScore() const
    {
        if (!compression_successful) return 0.0;
        
        // Weighted score: 40% compression, 30% speed, 20% CPU, 10% memory
        double compression_score = std::min(compression_ratio / 2.0, 1.0);
        double speed_score = std::max(0.0, 1.0 - (compression_time_ms / 1000.0));
        double cpu_score = std::max(0.0, 1.0 - cpu_efficiency);
        double memory_score = std::max(0.0, 1.0 - memory_overhead);
        
        return compression_score * 0.4 + speed_score * 0.3 + cpu_score * 0.2 + memory_score * 0.1;
    }
};

//----------------------------
// Data Pattern Analysis Engine
//----------------------------

/**
 * Analyzes bitmap data patterns to determine optimal compression algorithm
 */
class BitmapPatternAnalyzer
{
public:
    explicit BitmapPatternAnalyzer(MemoryPool* pool);
    ~BitmapPatternAnalyzer();

    // Pattern analysis methods
    BitmapDataPattern analyzePattern(const UCHAR* bitmap_data, ULONG data_size,
                                   ULONG bit_count) const;
    
    BitmapDataPattern analyzePattern(const CompressedBitmap* bitmap) const;
    
    // Pattern characteristics extraction
    struct PatternCharacteristics
    {
        double density_ratio;           // Ratio of set bits to total bits
        double clustering_factor;       // Measure of bit clustering (0-1)
        double run_length_factor;       // Measure of consecutive runs (0-1)
        double periodicity_factor;      // Measure of periodic patterns (0-1)
        double randomness_factor;       // Measure of randomness (0-1)
        ULONG average_run_length;       // Average consecutive bit run length
        ULONG max_run_length;           // Maximum consecutive bit run length
        ULONG distinct_run_count;       // Number of distinct runs
        double noise_level;             // Level of pattern noise (0-1)
        
        PatternCharacteristics()
            : density_ratio(0.0), clustering_factor(0.0), run_length_factor(0.0),
              periodicity_factor(0.0), randomness_factor(0.0), average_run_length(0),
              max_run_length(0), distinct_run_count(0), noise_level(0.0)
        {
        }
    };
    
    PatternCharacteristics extractCharacteristics(const UCHAR* bitmap_data, 
                                                 ULONG data_size, ULONG bit_count) const;
    
    // Pattern prediction for future data
    BitmapDataPattern predictPattern(const PatternCharacteristics& characteristics) const;
    
    // Pattern evolution tracking
    void updatePatternHistory(const PatternCharacteristics& characteristics);
    BitmapDataPattern getTrendingPattern() const;

private:
    MemoryPool* m_pool;
    std::vector<PatternCharacteristics> m_pattern_history;
    static constexpr ULONG MAX_HISTORY_SIZE = 100;
    
    // Analysis helper methods
    double calculateDensity(const UCHAR* bitmap_data, ULONG data_size, ULONG bit_count) const;
    double calculateClustering(const UCHAR* bitmap_data, ULONG data_size, ULONG bit_count) const;
    double calculateRunLengthFactor(const UCHAR* bitmap_data, ULONG data_size, ULONG bit_count) const;
    double calculatePeriodicity(const UCHAR* bitmap_data, ULONG data_size, ULONG bit_count) const;
    double calculateRandomness(const UCHAR* bitmap_data, ULONG data_size, ULONG bit_count) const;
    std::vector<ULONG> extractRunLengths(const UCHAR* bitmap_data, ULONG data_size, ULONG bit_count) const;
    
    // Statistical analysis
    double calculateEntropy(const UCHAR* bitmap_data, ULONG data_size) const;
    double calculateAutocorrelation(const UCHAR* bitmap_data, ULONG data_size, ULONG lag) const;
    std::vector<ULONG> findPeriodicPatterns(const UCHAR* bitmap_data, ULONG data_size) const;
    
    // Pattern classification
    BitmapDataPattern classifyByDensityAndClustering(double density, double clustering) const;
    BitmapDataPattern classifyByRunLength(const std::vector<ULONG>& run_lengths, double density) const;
    BitmapDataPattern classifyByPeriodicity(double periodicity, double density) const;
};

//----------------------------
// Compression Algorithm Selector
//----------------------------

/**
 * Selects optimal compression algorithm based on data patterns and performance history
 */
class CompressionAlgorithmSelector
{
public:
    explicit CompressionAlgorithmSelector(MemoryPool* pool);
    ~CompressionAlgorithmSelector();

    // Algorithm selection methods
    BitmapCompressionType selectOptimalAlgorithm(BitmapDataPattern pattern,
                                                ULONG data_size,
                                                const BitmapPatternAnalyzer::PatternCharacteristics& characteristics) const;
    
    BitmapCompressionType selectOptimalAlgorithm(const CompressedBitmap* bitmap) const;
    
    // Performance-based selection
    BitmapCompressionType selectByPerformanceHistory(BitmapDataPattern pattern,
                                                   ULONG data_size) const;
    
    // Multi-algorithm testing
    struct AlgorithmTestResult
    {
        BitmapCompressionType algorithm;
        CompressionMetrics metrics;
        bool is_winner;
        
        AlgorithmTestResult(BitmapCompressionType alg)
            : algorithm(alg), is_winner(false) {}
    };
    
    std::vector<AlgorithmTestResult> testAllAlgorithms(const UCHAR* bitmap_data,
                                                      ULONG data_size,
                                                      ULONG bit_count) const;
    
    BitmapCompressionType selectWinnerFromTests(const std::vector<AlgorithmTestResult>& results) const;
    
    // Performance learning
    void recordPerformance(BitmapDataPattern pattern, ULONG data_size,
                          const CompressionMetrics& metrics);
    
    void updateAlgorithmRankings(BitmapDataPattern pattern);
    
    // Configuration
    void setPerformanceWeights(double compression_weight, double speed_weight,
                              double cpu_weight, double memory_weight);
    
    void enableAlgorithm(BitmapCompressionType algorithm, bool enabled);
    bool isAlgorithmEnabled(BitmapCompressionType algorithm) const;

private:
    MemoryPool* m_pool;
    
    // Performance history storage
    struct AlgorithmPerformanceHistory
    {
        BitmapCompressionType algorithm;
        BitmapDataPattern pattern;
        std::vector<CompressionMetrics> metrics_history;
        double average_score;
        ULONG usage_count;
        
        AlgorithmPerformanceHistory(BitmapCompressionType alg, BitmapDataPattern pat)
            : algorithm(alg), pattern(pat), average_score(0.0), usage_count(0) {}
    };
    
    std::vector<AlgorithmPerformanceHistory> m_performance_history;
    static constexpr ULONG MAX_PERFORMANCE_HISTORY = 1000;
    
    // Algorithm rankings per pattern
    struct PatternAlgorithmRanking
    {
        BitmapDataPattern pattern;
        std::vector<std::pair<BitmapCompressionType, double>> algorithm_scores;
        
        explicit PatternAlgorithmRanking(BitmapDataPattern pat) : pattern(pat) {}
    };
    
    std::vector<PatternAlgorithmRanking> m_algorithm_rankings;
    
    // Configuration
    double m_compression_weight;
    double m_speed_weight;
    double m_cpu_weight;
    double m_memory_weight;
    
    std::vector<bool> m_algorithm_enabled;
    
    // Helper methods
    AlgorithmPerformanceHistory* findPerformanceHistory(BitmapCompressionType algorithm,
                                                        BitmapDataPattern pattern);
    
    PatternAlgorithmRanking* findAlgorithmRanking(BitmapDataPattern pattern);
    
    void initializeDefaultRankings();
    void updateAverageScore(AlgorithmPerformanceHistory& history);
    
    // Algorithm-specific selection logic
    BitmapCompressionType selectForSparseRandom(const BitmapPatternAnalyzer::PatternCharacteristics& characteristics) const;
    BitmapCompressionType selectForSparseClustered(const BitmapPatternAnalyzer::PatternCharacteristics& characteristics) const;
    BitmapCompressionType selectForDenseRandom(const BitmapPatternAnalyzer::PatternCharacteristics& characteristics) const;
    BitmapCompressionType selectForDenseClustered(const BitmapPatternAnalyzer::PatternCharacteristics& characteristics) const;
    BitmapCompressionType selectForRunLength(const BitmapPatternAnalyzer::PatternCharacteristics& characteristics) const;
    BitmapCompressionType selectForAlternating(const BitmapPatternAnalyzer::PatternCharacteristics& characteristics) const;
    BitmapCompressionType selectForPeriodic(const BitmapPatternAnalyzer::PatternCharacteristics& characteristics) const;
    BitmapCompressionType selectForMixed(const BitmapPatternAnalyzer::PatternCharacteristics& characteristics) const;
};

//----------------------------
// Adaptive Compression Engine
//----------------------------

/**
 * Main adaptive compression engine that combines pattern analysis and algorithm selection
 */
class AdaptiveCompressionEngine
{
public:
    explicit AdaptiveCompressionEngine(MemoryPool* pool);
    ~AdaptiveCompressionEngine();

    // Main compression interface
    CompressedBitmap* compressWithOptimalAlgorithm(const UCHAR* bitmap_data,
                                                   ULONG data_size,
                                                   ULONG bit_count);
    
    CompressedBitmap* compressExistingBitmap(CompressedBitmap* bitmap);
    
    // Recompression optimization
    bool shouldRecompress(const CompressedBitmap* bitmap) const;
    CompressedBitmap* recompressWithBetterAlgorithm(CompressedBitmap* bitmap);
    
    // Pattern-aware bulk operations
    std::vector<CompressedBitmap*> compressBitmapCollection(
        const std::vector<std::pair<UCHAR*, ULONG>>& bitmaps_data,
        const std::vector<ULONG>& bit_counts);
    
    // Performance monitoring
    struct CompressionStatistics
    {
        ULONG total_compressions;
        ULONG successful_compressions;
        double average_compression_ratio;
        double average_compression_time_ms;
        ULONG algorithm_usage_count[5];  // Count for each algorithm type
        BitmapDataPattern most_common_pattern;
        BitmapCompressionType most_successful_algorithm;
        
        CompressionStatistics()
            : total_compressions(0), successful_compressions(0),
              average_compression_ratio(1.0), average_compression_time_ms(0.0),
              most_common_pattern(PATTERN_UNKNOWN),
              most_successful_algorithm(BITMAP_COMPRESSION_NONE)
        {
            memset(algorithm_usage_count, 0, sizeof(algorithm_usage_count));
        }
    };
    
    CompressionStatistics getStatistics() const;
    void resetStatistics();
    
    // Configuration and tuning
    void setAdaptiveLearningEnabled(bool enabled);
    bool isAdaptiveLearningEnabled() const;
    
    void setPerformanceLoggingEnabled(bool enabled);
    bool isPerformanceLoggingEnabled() const;
    
    void tuneForWorkload(const std::vector<CompressedBitmap*>& sample_bitmaps);
    
    // Algorithm recommendation
    BitmapCompressionType recommendAlgorithmForWorkload(
        const std::vector<BitmapDataPattern>& patterns,
        const std::vector<ULONG>& data_sizes) const;
    
    ScratchBird::string generatePerformanceReport() const;

private:
    MemoryPool* m_pool;
    std::unique_ptr<BitmapPatternAnalyzer> m_pattern_analyzer;
    std::unique_ptr<CompressionAlgorithmSelector> m_algorithm_selector;
    
    // Configuration
    bool m_adaptive_learning_enabled;
    bool m_performance_logging_enabled;
    
    // Statistics tracking
    mutable CompressionStatistics m_statistics;
    mutable ScratchBird::Mutex m_statistics_mutex;
    
    // Performance optimization
    struct CacheEntry
    {
        ULONG data_hash;                    // Hash of bitmap data
        BitmapDataPattern pattern;          // Detected pattern
        BitmapCompressionType algorithm;    // Selected algorithm
        CompressionMetrics metrics;         // Performance metrics
        ULONG access_count;                 // Number of cache hits
        GDS_TIMESTAMP last_access;          // Last access time
        
        CacheEntry() : data_hash(0), pattern(PATTERN_UNKNOWN),
                      algorithm(BITMAP_COMPRESSION_NONE), access_count(0), last_access(0) {}
    };
    
    mutable std::vector<CacheEntry> m_pattern_cache;
    static constexpr ULONG MAX_CACHE_SIZE = 1000;
    
    // Helper methods
    ULONG calculateDataHash(const UCHAR* bitmap_data, ULONG data_size) const;
    CacheEntry* findCacheEntry(ULONG data_hash) const;
    void addCacheEntry(const CacheEntry& entry) const;
    void evictOldCacheEntries() const;
    
    void updateStatistics(const CompressionMetrics& metrics) const;
    void logPerformance(BitmapDataPattern pattern, const CompressionMetrics& metrics) const;
    
    // Workload tuning helpers
    std::vector<BitmapDataPattern> analyzeWorkloadPatterns(
        const std::vector<CompressedBitmap*>& sample_bitmaps) const;
    
    void optimizeForCommonPatterns(const std::vector<BitmapDataPattern>& patterns);
    void optimizeAlgorithmWeights(const std::vector<CompressionMetrics>& sample_metrics);
};

//----------------------------
// Compression Testing Framework
//----------------------------

/**
 * Framework for testing and benchmarking compression algorithms
 */
class CompressionTestFramework
{
public:
    explicit CompressionTestFramework(MemoryPool* pool);
    ~CompressionTestFramework();

    // Test data generation
    UCHAR* generateTestBitmap(BitmapDataPattern pattern, ULONG bit_count, ULONG& data_size) const;
    std::vector<UCHAR*> generateTestSuite(const std::vector<BitmapDataPattern>& patterns,
                                         const std::vector<ULONG>& bit_counts,
                                         std::vector<ULONG>& data_sizes) const;
    
    // Algorithm benchmarking
    CompressionMetrics benchmarkAlgorithm(BitmapCompressionType algorithm,
                                         const UCHAR* bitmap_data,
                                         ULONG data_size,
                                         ULONG bit_count) const;
    
    std::vector<CompressionMetrics> benchmarkAllAlgorithms(const UCHAR* bitmap_data,
                                                          ULONG data_size,
                                                          ULONG bit_count) const;
    
    // Comprehensive testing
    struct TestResults
    {
        BitmapDataPattern pattern;
        ULONG bit_count;
        std::vector<CompressionMetrics> algorithm_results;
        BitmapCompressionType best_algorithm;
        double best_score;
        
        TestResults(BitmapDataPattern pat, ULONG bits)
            : pattern(pat), bit_count(bits), best_algorithm(BITMAP_COMPRESSION_NONE), best_score(0.0) {}
    };
    
    std::vector<TestResults> runComprehensiveTests(const std::vector<BitmapDataPattern>& patterns,
                                                  const std::vector<ULONG>& bit_counts) const;
    
    // Performance analysis
    ScratchBird::string generateTestReport(const std::vector<TestResults>& results) const;
    void saveTestResults(const std::vector<TestResults>& results, const ScratchBird::string& filename) const;
    
    // Algorithm validation
    bool validateCompressionAlgorithm(BitmapCompressionType algorithm,
                                     const UCHAR* bitmap_data,
                                     ULONG data_size,
                                     ULONG bit_count) const;

private:
    MemoryPool* m_pool;
    
    // Test data generation helpers
    void generateSparseRandomBitmap(UCHAR* bitmap_data, ULONG data_size, ULONG bit_count, double density) const;
    void generateSparseClusteredBitmap(UCHAR* bitmap_data, ULONG data_size, ULONG bit_count, double density) const;
    void generateDenseRandomBitmap(UCHAR* bitmap_data, ULONG data_size, ULONG bit_count, double density) const;
    void generateDenseClusteredBitmap(UCHAR* bitmap_data, ULONG data_size, ULONG bit_count, double density) const;
    void generateRunLengthBitmap(UCHAR* bitmap_data, ULONG data_size, ULONG bit_count) const;
    void generateAlternatingBitmap(UCHAR* bitmap_data, ULONG data_size, ULONG bit_count) const;
    void generatePeriodicBitmap(UCHAR* bitmap_data, ULONG data_size, ULONG bit_count, ULONG period) const;
    
    // Timing and measurement utilities
    ULONG measureCompressionTime(BitmapCompressionType algorithm,
                                const UCHAR* bitmap_data,
                                ULONG data_size) const;
    
    ULONG measureDecompressionTime(BitmapCompressionType algorithm,
                                  const UCHAR* compressed_data,
                                  ULONG compressed_size) const;
    
    double measureCPUEfficiency(BitmapCompressionType algorithm,
                               const UCHAR* bitmap_data,
                               ULONG data_size) const;
    
    // Validation helpers
    bool compareDecompressedData(const UCHAR* original_data, ULONG original_size,
                                const UCHAR* decompressed_data, ULONG decompressed_size) const;
};

//----------------------------
// Global Adaptive Compression Manager
//----------------------------

/**
 * Global manager for adaptive compression across the entire database
 */
class GlobalAdaptiveCompressionManager
{
public:
    static GlobalAdaptiveCompressionManager* getInstance();
    
    // Engine management
    AdaptiveCompressionEngine* getEngine(MemoryPool* pool);
    void returnEngine(AdaptiveCompressionEngine* engine);
    
    // Global configuration
    void setGlobalCompressionPolicy(const ScratchBird::string& policy);
    ScratchBird::string getGlobalCompressionPolicy() const;
    
    // System-wide statistics
    CompressionStatistics getGlobalStatistics() const;
    void resetGlobalStatistics();
    
    // Performance monitoring
    void startPerformanceMonitoring();
    void stopPerformanceMonitoring();
    ScratchBird::string generateGlobalPerformanceReport() const;

private:
    GlobalAdaptiveCompressionManager();
    ~GlobalAdaptiveCompressionManager();
    
    static GlobalAdaptiveCompressionManager* s_instance;
    static ScratchBird::Mutex s_instance_mutex;
    
    std::vector<AdaptiveCompressionEngine*> m_engine_pool;
    ScratchBird::Mutex m_engine_pool_mutex;
    
    ScratchBird::string m_global_policy;
    bool m_performance_monitoring_enabled;
    CompressionStatistics m_global_statistics;
    mutable ScratchBird::Mutex m_statistics_mutex;
};

} // namespace Jrd

#endif // JRD_ADAPTIVE_COMPRESSION_ENGINE_H