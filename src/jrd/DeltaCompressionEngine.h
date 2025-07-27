/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		DeltaCompressionEngine.h
 *	DESCRIPTION:	Delta compression for frequently updated bitmap indexes
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
 * 2025.07.23 - ScratchBird Delta Compression Implementation
 */

#ifndef JRD_DELTA_COMPRESSION_ENGINE_H
#define JRD_DELTA_COMPRESSION_ENGINE_H

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
class CompressedBitmap;
struct index_desc;

//----------------------------
// Delta Compression Constants
//----------------------------

inline constexpr ULONG DELTA_COMPRESSION_VERSION = 1;              // Delta format version
inline constexpr ULONG MAX_DELTA_CHAIN_LENGTH = 50;                // Maximum delta chain before rebase
inline constexpr ULONG DELTA_REBASE_THRESHOLD = 32;                // Rebase when chain gets this long
inline constexpr ULONG DELTA_MERGE_BATCH_SIZE = 10;                // Delta operations to batch before merge
inline constexpr ULONG MAX_CACHED_DELTAS = 1000;                   // Maximum cached delta operations
inline constexpr double DELTA_COMPRESSION_RATIO_THRESHOLD = 0.6;   // Minimum compression ratio to keep delta

//----------------------------
// Delta Operation Types
//----------------------------

enum DeltaOperationType : UCHAR
{
    DELTA_OP_SET_BIT = 0,           // Set bit at position
    DELTA_OP_CLEAR_BIT = 1,         // Clear bit at position
    DELTA_OP_FLIP_BIT = 2,          // Flip bit at position
    DELTA_OP_SET_RANGE = 3,         // Set range of bits
    DELTA_OP_CLEAR_RANGE = 4,       // Clear range of bits
    DELTA_OP_FLIP_RANGE = 5,        // Flip range of bits
    DELTA_OP_INSERT_RUN = 6,        // Insert run of set bits
    DELTA_OP_DELETE_RUN = 7,        // Delete run of set bits
    DELTA_OP_MOVE_RUN = 8,          // Move run to different position
    DELTA_OP_COPY_RANGE = 9,        // Copy range from one position to another
    DELTA_OP_FILL_PATTERN = 10,     // Fill range with repeating pattern
    DELTA_OP_COMPRESS_SEGMENT = 11  // Apply compression to segment
};

//----------------------------
// Delta Compression Strategy
//----------------------------

enum DeltaCompressionStrategy : UCHAR
{
    DELTA_STRATEGY_INCREMENTAL = 0,     // Store only changes since last version
    DELTA_STRATEGY_HIERARCHICAL = 1,    // Multi-level delta with snapshots
    DELTA_STRATEGY_ROLLING = 2,         // Rolling window of recent changes
    DELTA_STRATEGY_CHECKPOINT = 3,      // Periodic checkpoints with deltas
    DELTA_STRATEGY_ADAPTIVE = 4,        // Adaptive strategy based on update patterns
    DELTA_STRATEGY_HYBRID = 5           // Combination of multiple strategies
};

//----------------------------
// Delta Operation Record
//----------------------------

struct DeltaOperation
{
    DeltaOperationType op_type;         // Type of operation
    ULONG start_position;               // Starting bit position
    ULONG end_position;                 // Ending bit position (for ranges)
    ULONG parameter;                    // Operation-specific parameter
    GDS_TIMESTAMP timestamp;            // When operation was applied
    ULONG operation_id;                 // Unique operation identifier
    USHORT flags;                       // Operation flags
    
    // Operation flags
    static constexpr USHORT DELTA_FLAG_COMPRESSED = 0x0001;        // Operation data is compressed
    static constexpr USHORT DELTA_FLAG_BATCHED = 0x0002;          // Part of batch operation
    static constexpr USHORT DELTA_FLAG_OPTIMIZED = 0x0004;        // Operation has been optimized
    static constexpr USHORT DELTA_FLAG_CHECKPOINT = 0x0008;       // Operation creates checkpoint
    static constexpr USHORT DELTA_FLAG_REVERSIBLE = 0x0010;       // Operation can be reversed
    
    // Optional variable-length data follows for complex operations
    UCHAR* operation_data;              // Additional operation data
    ULONG data_length;                  // Length of operation data
    
    DeltaOperation()
        : op_type(DELTA_OP_SET_BIT), start_position(0), end_position(0),
          parameter(0), timestamp(0), operation_id(0), flags(0),
          operation_data(nullptr), data_length(0)
    {
    }
    
    // Helper methods
    bool isRangeOperation() const {
        return op_type >= DELTA_OP_SET_RANGE && op_type <= DELTA_OP_FLIP_RANGE;
    }
    
    bool isComplexOperation() const {
        return op_type >= DELTA_OP_INSERT_RUN;
    }
    
    ULONG getAffectedBitCount() const {
        return end_position > start_position ? end_position - start_position + 1 : 1;
    }
    
    ULONG getSerializedSize() const {
        return sizeof(DeltaOperation) + data_length;
    }
};

//----------------------------
// Delta Bitmap Version
//----------------------------

struct DeltaBitmapVersion
{
    ULONG version_number;               // Sequential version number
    CompressedBitmap* base_bitmap;      // Base bitmap (nullptr for delta-only)
    std::vector<DeltaOperation> operations; // Delta operations since base
    GDS_TIMESTAMP creation_time;        // When version was created
    GDS_TIMESTAMP last_modified;        // Last modification time
    ULONG total_operations;             // Total operations in this version
    ULONG compressed_size;              // Size when compressed
    double compression_ratio;           // Compression ratio achieved
    bool is_checkpoint;                 // True if this is a checkpoint version
    
    DeltaBitmapVersion()
        : version_number(0), base_bitmap(nullptr), creation_time(0),
          last_modified(0), total_operations(0), compressed_size(0),
          compression_ratio(1.0), is_checkpoint(false)
    {
    }
    
    ~DeltaBitmapVersion() {
        delete base_bitmap;
    }
    
    // Version management
    bool needsRebase() const {
        return operations.size() >= DELTA_REBASE_THRESHOLD;
    }
    
    bool isEfficient() const {
        return compression_ratio >= DELTA_COMPRESSION_RATIO_THRESHOLD;
    }
    
    ULONG getChainLength() const {
        return static_cast<ULONG>(operations.size());
    }
};

//----------------------------
// Delta Update Statistics
//----------------------------

struct DeltaUpdateStatistics
{
    ULONG total_updates;                // Total update operations
    ULONG set_operations;               // Bit set operations
    ULONG clear_operations;             // Bit clear operations
    ULONG range_operations;             // Range operations
    ULONG batch_operations;             // Batch operations
    
    double average_operation_size;      // Average bits affected per operation
    double update_frequency;            // Updates per second
    double compression_efficiency;      // Overall compression efficiency
    ULONG rebase_operations;            // Number of rebases performed
    ULONG checkpoint_operations;        // Number of checkpoints created
    
    // Hot spot analysis
    std::map<ULONG, ULONG> hot_regions; // Frequently updated bit regions
    ULONG max_hotspot_frequency;        // Maximum updates in any region
    double hotspot_concentration;       // Concentration of updates in hotspots
    
    DeltaUpdateStatistics()
        : total_updates(0), set_operations(0), clear_operations(0),
          range_operations(0), batch_operations(0), average_operation_size(0.0),
          update_frequency(0.0), compression_efficiency(1.0),
          rebase_operations(0), checkpoint_operations(0),
          max_hotspot_frequency(0), hotspot_concentration(0.0)
    {
    }
    
    void recordOperation(const DeltaOperation& op) {
        total_updates++;
        
        switch (op.op_type) {
            case DELTA_OP_SET_BIT:
            case DELTA_OP_SET_RANGE:
            case DELTA_OP_INSERT_RUN:
                set_operations++;
                break;
            case DELTA_OP_CLEAR_BIT:
            case DELTA_OP_CLEAR_RANGE:
            case DELTA_OP_DELETE_RUN:
                clear_operations++;
                break;
            default:
                if (op.isRangeOperation()) {
                    range_operations++;
                }
                break;
        }
        
        if (op.flags & DeltaOperation::DELTA_FLAG_BATCHED) {
            batch_operations++;
        }
        
        // Update hot regions
        ULONG region = op.start_position / 1024; // 1KB regions
        hot_regions[region]++;
        if (hot_regions[region] > max_hotspot_frequency) {
            max_hotspot_frequency = hot_regions[region];
        }
    }
};

//----------------------------
// Delta Compression Engine
//----------------------------

/**
 * Main engine for delta compression of bitmap indexes
 */
class DeltaCompressionEngine
{
public:
    explicit DeltaCompressionEngine(MemoryPool* pool);
    ~DeltaCompressionEngine();

    // Delta bitmap lifecycle
    bool createDeltaBitmap(const index_desc* idx, const CompressedBitmap* initial_bitmap);
    bool loadDeltaBitmap(thread_db* tdbb, const index_desc* idx);
    bool saveDeltaBitmap(thread_db* tdbb, const index_desc* idx);
    void closeDeltaBitmap(const index_desc* idx);
    
    // Delta operations
    bool applyDeltaOperation(thread_db* tdbb, const index_desc* idx, const DeltaOperation& operation);
    bool applyDeltaBatch(thread_db* tdbb, const index_desc* idx, const std::vector<DeltaOperation>& operations);
    
    // Single bit operations
    bool setBit(thread_db* tdbb, const index_desc* idx, ULONG bit_position);
    bool clearBit(thread_db* tdbb, const index_desc* idx, ULONG bit_position);
    bool flipBit(thread_db* tdbb, const index_desc* idx, ULONG bit_position);
    
    // Range operations
    bool setBitRange(thread_db* tdbb, const index_desc* idx, ULONG start_pos, ULONG end_pos);
    bool clearBitRange(thread_db* tdbb, const index_desc* idx, ULONG start_pos, ULONG end_pos);
    bool flipBitRange(thread_db* tdbb, const index_desc* idx, ULONG start_pos, ULONG end_pos);
    
    // Complex operations
    bool insertRun(thread_db* tdbb, const index_desc* idx, ULONG position, ULONG run_length);
    bool deleteRun(thread_db* tdbb, const index_desc* idx, ULONG position, ULONG run_length);
    bool moveRun(thread_db* tdbb, const index_desc* idx, ULONG from_pos, ULONG to_pos, ULONG run_length);
    
    // Bitmap reconstruction
    CompressedBitmap* reconstructBitmap(thread_db* tdbb, const index_desc* idx, ULONG target_version = 0) const;
    CompressedBitmap* getCurrentBitmap(thread_db* tdbb, const index_desc* idx) const;
    
    // Version management
    bool createCheckpoint(thread_db* tdbb, const index_desc* idx);
    bool rebaseToCheckpoint(thread_db* tdbb, const index_desc* idx, ULONG checkpoint_version);
    std::vector<ULONG> getAvailableVersions(const index_desc* idx) const;
    bool rollbackToVersion(thread_db* tdbb, const index_desc* idx, ULONG target_version);
    
    // Optimization operations
    bool optimizeDeltaChain(thread_db* tdbb, const index_desc* idx);
    bool compressDeltaOperations(thread_db* tdbb, const index_desc* idx);
    bool mergeDeltaOperations(thread_db* tdbb, const index_desc* idx, ULONG max_operations = DELTA_MERGE_BATCH_SIZE);
    
    // Configuration and strategy
    void setCompressionStrategy(const index_desc* idx, DeltaCompressionStrategy strategy);
    DeltaCompressionStrategy getCompressionStrategy(const index_desc* idx) const;
    
    void setRebaseThreshold(ULONG threshold);
    ULONG getRebaseThreshold() const;
    
    void setCheckpointInterval(ULONG interval_operations);
    ULONG getCheckpointInterval() const;
    
    // Statistics and analysis
    DeltaUpdateStatistics getUpdateStatistics(const index_desc* idx) const;
    void resetStatistics(const index_desc* idx);
    
    // Performance analysis
    struct DeltaPerformanceAnalysis
    {
        double compression_efficiency;      // Overall compression efficiency
        double update_performance;          // Update operation performance
        double reconstruction_performance;  // Bitmap reconstruction performance
        ULONG average_chain_length;         // Average delta chain length
        ULONG storage_overhead;             // Storage overhead vs direct storage
        bool needs_optimization;            // True if optimization recommended
        ScratchBird::string recommendations; // Performance recommendations
        
        DeltaPerformanceAnalysis()
            : compression_efficiency(1.0), update_performance(1.0),
              reconstruction_performance(1.0), average_chain_length(0),
              storage_overhead(0), needs_optimization(false)
        {
        }
    };
    
    DeltaPerformanceAnalysis analyzePerformance(thread_db* tdbb, const index_desc* idx) const;
    void applyPerformanceOptimizations(thread_db* tdbb, const index_desc* idx, const DeltaPerformanceAnalysis& analysis);
    
    // Maintenance operations
    bool validateDeltaIntegrity(thread_db* tdbb, const index_desc* idx, ScratchBird::string& error_report) const;
    bool repairDeltaCorruption(thread_db* tdbb, const index_desc* idx, const ScratchBird::string& repair_options);
    void cleanupOldVersions(thread_db* tdbb, const index_desc* idx, ULONG keep_versions = 10);

private:
    MemoryPool* m_pool;
    
    // Delta bitmap storage
    struct DeltaBitmapInfo
    {
        const index_desc* index_descriptor;
        std::vector<std::unique_ptr<DeltaBitmapVersion>> versions;
        ULONG current_version;
        ULONG next_operation_id;
        DeltaCompressionStrategy strategy;
        DeltaUpdateStatistics statistics;
        
        DeltaBitmapInfo(const index_desc* idx)
            : index_descriptor(idx), current_version(0), next_operation_id(1),
              strategy(DELTA_STRATEGY_INCREMENTAL)
        {
        }
    };
    
    std::map<USHORT, std::unique_ptr<DeltaBitmapInfo>> m_delta_bitmaps; // Index ID -> Delta info
    mutable ScratchBird::Mutex m_bitmaps_mutex;
    
    // Configuration
    ULONG m_rebase_threshold;
    ULONG m_checkpoint_interval;
    ULONG m_max_cached_operations;
    DeltaCompressionStrategy m_default_strategy;
    
    // Operation caching
    std::map<USHORT, std::vector<DeltaOperation>> m_pending_operations;
    mutable ScratchBird::Mutex m_operations_mutex;
    
    // Internal helpers
    DeltaBitmapInfo* findDeltaBitmapInfo(const index_desc* idx);
    const DeltaBitmapInfo* findDeltaBitmapInfo(const index_desc* idx) const;
    DeltaBitmapInfo* getOrCreateDeltaBitmapInfo(const index_desc* idx);
    
    // Version management helpers
    DeltaBitmapVersion* getCurrentVersion(DeltaBitmapInfo* info);
    const DeltaBitmapVersion* getCurrentVersion(const DeltaBitmapInfo* info) const;
    DeltaBitmapVersion* createNewVersion(DeltaBitmapInfo* info, bool is_checkpoint = false);
    
    // Operation optimization
    std::vector<DeltaOperation> optimizeOperationSequence(const std::vector<DeltaOperation>& operations) const;
    bool canMergeOperations(const DeltaOperation& op1, const DeltaOperation& op2) const;
    DeltaOperation mergeOperations(const DeltaOperation& op1, const DeltaOperation& op2) const;
    
    // Compression helpers
    bool compressOperation(DeltaOperation& operation) const;
    bool decompressOperation(const DeltaOperation& compressed_op, DeltaOperation& decompressed_op) const;
    
    // Strategy-specific implementations
    bool applyIncrementalStrategy(thread_db* tdbb, DeltaBitmapInfo* info, const DeltaOperation& operation);
    bool applyHierarchicalStrategy(thread_db* tdbb, DeltaBitmapInfo* info, const DeltaOperation& operation);
    bool applyRollingStrategy(thread_db* tdbb, DeltaBitmapInfo* info, const DeltaOperation& operation);
    bool applyCheckpointStrategy(thread_db* tdbb, DeltaBitmapInfo* info, const DeltaOperation& operation);
    bool applyAdaptiveStrategy(thread_db* tdbb, DeltaBitmapInfo* info, const DeltaOperation& operation);
    
    // Reconstruction algorithms
    CompressedBitmap* reconstructFromBase(const DeltaBitmapVersion* version) const;
    CompressedBitmap* reconstructFromDeltas(const std::vector<DeltaOperation>& operations,
                                          const CompressedBitmap* base_bitmap) const;
    
    // Hot spot analysis
    void analyzeHotSpots(DeltaBitmapInfo* info);
    std::vector<std::pair<ULONG, ULONG>> identifyHotRegions(const DeltaUpdateStatistics& stats) const;
    void optimizeForHotSpots(thread_db* tdbb, DeltaBitmapInfo* info, const std::vector<std::pair<ULONG, ULONG>>& hot_regions);
    
    // Storage management
    bool persistDeltaVersion(thread_db* tdbb, const DeltaBitmapInfo* info, const DeltaBitmapVersion* version);
    bool loadDeltaVersion(thread_db* tdbb, DeltaBitmapInfo* info, ULONG version_number);
    void evictOldVersions(DeltaBitmapInfo* info, ULONG keep_count);
    
    // Validation helpers
    bool validateOperationSequence(const std::vector<DeltaOperation>& operations) const;
    bool validateBitmapConsistency(const CompressedBitmap* bitmap, const std::vector<DeltaOperation>& operations) const;
    
    // Performance monitoring
    void recordOperationPerformance(const index_desc* idx, const DeltaOperation& operation, ULONG execution_time_ms);
    void updateCompressionEfficiency(DeltaBitmapInfo* info);
    
    // Operation execution
    bool executeSingleBitOperation(CompressedBitmap* bitmap, const DeltaOperation& operation);
    bool executeRangeOperation(CompressedBitmap* bitmap, const DeltaOperation& operation);
    bool executeComplexOperation(CompressedBitmap* bitmap, const DeltaOperation& operation);
};

//----------------------------
// Delta Bitmap Manager
//----------------------------

/**
 * Global manager for delta-compressed bitmap indexes
 */
class DeltaBitmapManager
{
public:
    static DeltaBitmapManager* getInstance();
    
    // Global delta operations
    bool enableDeltaCompression(thread_db* tdbb, const index_desc* idx,
                               DeltaCompressionStrategy strategy = DELTA_STRATEGY_ADAPTIVE);
    
    bool disableDeltaCompression(thread_db* tdbb, const index_desc* idx);
    bool isDeltaCompressionEnabled(const index_desc* idx) const;
    
    // Batch update operations
    bool beginBatchUpdate(thread_db* tdbb, const index_desc* idx);
    bool addBatchOperation(thread_db* tdbb, const index_desc* idx, const DeltaOperation& operation);
    bool commitBatchUpdate(thread_db* tdbb, const index_desc* idx);
    bool rollbackBatchUpdate(thread_db* tdbb, const index_desc* idx);
    
    // Global maintenance
    void performGlobalOptimization(thread_db* tdbb);
    void performGlobalCleanup(thread_db* tdbb, ULONG keep_versions = 10);
    void performGlobalCheckpoint(thread_db* tdbb);
    
    // Global statistics
    struct GlobalDeltaStatistics
    {
        ULONG total_delta_indexes;          // Total delta-compressed indexes
        ULONG total_operations;             // Total delta operations
        double average_compression_ratio;   // Average compression ratio
        ULONG total_storage_saved;          // Total storage saved vs direct
        ULONG active_versions;              // Active versions across all indexes
        double global_update_frequency;     // Global update frequency
        
        GlobalDeltaStatistics()
            : total_delta_indexes(0), total_operations(0),
              average_compression_ratio(1.0), total_storage_saved(0),
              active_versions(0), global_update_frequency(0.0)
        {
        }
    };
    
    GlobalDeltaStatistics getGlobalStatistics() const;
    void resetGlobalStatistics();
    
    // Configuration
    void setGlobalRebaseThreshold(ULONG threshold);
    void setGlobalCheckpointInterval(ULONG interval);
    void setGlobalCompressionStrategy(DeltaCompressionStrategy strategy);

private:
    DeltaBitmapManager();
    ~DeltaBitmapManager();
    
    static DeltaBitmapManager* s_instance;
    static ScratchBird::Mutex s_instance_mutex;
    
    std::unique_ptr<DeltaCompressionEngine> m_compression_engine;
    
    // Batch update state
    struct BatchUpdateState
    {
        const index_desc* index_descriptor;
        std::vector<DeltaOperation> pending_operations;
        GDS_TIMESTAMP batch_start_time;
        bool is_active;
        
        BatchUpdateState(const index_desc* idx)
            : index_descriptor(idx), batch_start_time(0), is_active(false)
        {
        }
    };
    
    std::map<USHORT, std::unique_ptr<BatchUpdateState>> m_batch_states;
    mutable ScratchBird::Mutex m_batch_mutex;
    
    // Global configuration
    ULONG m_global_rebase_threshold;
    ULONG m_global_checkpoint_interval;
    DeltaCompressionStrategy m_global_strategy;
    
    // Statistics
    mutable GlobalDeltaStatistics m_global_statistics;
    mutable ScratchBird::Mutex m_statistics_mutex;
    
    // Internal helpers
    void updateGlobalStatistics();
    BatchUpdateState* findBatchState(const index_desc* idx);
    void cleanupBatchState(const index_desc* idx);
};

//----------------------------
// Integration with Bitmap Index
//----------------------------

/**
 * Integration layer between delta compression and bitmap index implementation
 */
class DeltaBitmapIndexIntegration
{
public:
    // Integration setup
    static bool enableDeltaCompressionForIndex(const index_desc* idx,
                                              DeltaCompressionStrategy strategy = DELTA_STRATEGY_ADAPTIVE);
    
    static bool disableDeltaCompressionForIndex(const index_desc* idx);
    
    // Update hooks
    static bool updateBitmapWithDelta(thread_db* tdbb, const index_desc* idx,
                                     ULONG record_number, bool set_bit);
    
    static bool batchUpdateBitmapWithDeltas(thread_db* tdbb, const index_desc* idx,
                                           const std::vector<std::pair<ULONG, bool>>& updates);
    
    // Query hooks
    static CompressedBitmap* getBitmapWithDeltas(thread_db* tdbb, const index_desc* idx);
    static bool testBitWithDeltas(thread_db* tdbb, const index_desc* idx, ULONG bit_position);
    
    // Maintenance hooks
    static void performDeltaOptimization(thread_db* tdbb, const index_desc* idx);
    static void createDeltaCheckpoint(thread_db* tdbb, const index_desc* idx);

private:
    static std::map<USHORT, bool> s_delta_enabled_indexes;
    static ScratchBird::Mutex s_integration_mutex;
};

//----------------------------
// Utility Functions
//----------------------------

// Delta operation utilities
ScratchBird::string serializeDeltaOperation(const DeltaOperation& operation);
bool deserializeDeltaOperation(const ScratchBird::string& serialized_data, DeltaOperation& operation);

// Compression analysis
double calculateDeltaCompressionRatio(const std::vector<DeltaOperation>& operations,
                                     ULONG original_bitmap_size);

bool isDeltaCompressionBeneficial(const DeltaUpdateStatistics& stats,
                                 double min_compression_ratio = DELTA_COMPRESSION_RATIO_THRESHOLD);

// Strategy selection
DeltaCompressionStrategy selectOptimalStrategy(const DeltaUpdateStatistics& stats);
bool shouldCreateCheckpoint(const DeltaUpdateStatistics& stats, ULONG chain_length);

// Hot spot detection
std::vector<std::pair<ULONG, ULONG>> detectUpdateHotSpots(const std::map<ULONG, ULONG>& update_regions,
                                                          double threshold_percentile = 0.8);

// Performance estimation
ULONG estimateDeltaReconstructionTime(ULONG chain_length, ULONG bitmap_size);
double estimateCompressionOverhead(const std::vector<DeltaOperation>& operations);

} // namespace Jrd

#endif // JRD_DELTA_COMPRESSION_ENGINE_H