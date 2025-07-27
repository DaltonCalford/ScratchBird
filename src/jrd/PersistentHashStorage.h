/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		PersistentHashStorage.h
 *	DESCRIPTION:	Persistent hash table storage to disk pages for hash indexes
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
 * 2025.07.23 - ScratchBird Persistent Hash Storage Implementation
 */

#ifndef JRD_PERSISTENT_HASH_STORAGE_H
#define JRD_PERSISTENT_HASH_STORAGE_H

#include "../jrd/constants.h"
#include "../jrd/ods.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include <vector>
#include <memory>

namespace Jrd {

// Forward declarations
class thread_db;
class MemoryPool;
class Database;
class PageManager;
struct index_desc;
class BufferControl;

//----------------------------
// Persistent Hash Page Types
//----------------------------

// New page types for persistent hash storage (extending existing ODS)
inline constexpr UCHAR pag_hash_root = 18;      // Hash index root page
inline constexpr UCHAR pag_hash_bucket = 19;    // Hash bucket data page
inline constexpr UCHAR pag_hash_overflow = 20;  // Hash overflow page
inline constexpr UCHAR pag_hash_directory = 21; // Hash directory page

//----------------------------
// Hash Storage Constants
//----------------------------

inline constexpr ULONG HASH_BUCKET_SIZE = 4096;             // Standard bucket page size
inline constexpr ULONG HASH_MAX_BUCKETS_PER_PAGE = 512;     // Maximum buckets per directory page
inline constexpr ULONG HASH_MAX_ENTRIES_PER_BUCKET = 256;   // Maximum entries per bucket
inline constexpr ULONG HASH_OVERFLOW_THRESHOLD = 200;       // Overflow threshold (entries)
inline constexpr ULONG HASH_SPLIT_THRESHOLD = 240;          // Split threshold (entries)
inline constexpr ULONG HASH_MERGE_THRESHOLD = 32;           // Merge threshold (entries)
inline constexpr ULONG HASH_DIRECTORY_LEVELS = 8;           // Maximum directory levels
inline constexpr ULONG HASH_INITIAL_DIRECTORY_SIZE = 256;   // Initial directory size

//----------------------------
// Hash Entry Structure
//----------------------------

struct hash_entry
{
    ULONG he_hash_value;        // Hash value of the key
    USHORT he_key_length;       // Length of the key
    USHORT he_data_length;      // Length of the data
    ULONG he_record_number;     // Record number (for index entries)
    UCHAR he_flags;             // Entry flags
    
    // Variable-length data follows:
    // UCHAR he_key_data[he_key_length];    // Key data
    // UCHAR he_data_data[he_data_length];  // Value data (if any)
    
    // Entry flags
    static constexpr UCHAR HE_DELETED = 0x01;      // Entry is deleted
    static constexpr UCHAR HE_OVERFLOW = 0x02;     // Entry continues in overflow page
    static constexpr UCHAR HE_COMPRESSED = 0x04;   // Entry data is compressed
    static constexpr UCHAR HE_NULL_KEY = 0x08;     // Key contains NULL values
    
    // Helper methods
    UCHAR* getKeyData() { return reinterpret_cast<UCHAR*>(this + 1); }
    const UCHAR* getKeyData() const { return reinterpret_cast<const UCHAR*>(this + 1); }
    
    UCHAR* getValueData() { return getKeyData() + he_key_length; }
    const UCHAR* getValueData() const { return getKeyData() + he_key_length; }
    
    ULONG getTotalSize() const { return sizeof(hash_entry) + he_key_length + he_data_length; }
    
    bool isValid() const { return he_key_length > 0 && !(he_flags & HE_DELETED); }
    bool isDeleted() const { return (he_flags & HE_DELETED) != 0; }
    bool hasOverflow() const { return (he_flags & HE_OVERFLOW) != 0; }
};

//----------------------------
// Hash Root Page Structure
//----------------------------

struct hash_root_page
{
    pag hrp_header;                     // Standard page header
    ULONG hrp_bucket_count;             // Current number of buckets
    ULONG hrp_entry_count;              // Total number of entries
    ULONG hrp_directory_page;           // Root directory page number
    ULONG hrp_overflow_pages;           // Number of overflow pages
    ULONG hrp_split_count;              // Number of bucket splits performed
    ULONG hrp_merge_count;              // Number of bucket merges performed
    double hrp_load_factor;             // Current load factor
    USHORT hrp_key_type;                // Data type of keys
    USHORT hrp_hash_algorithm;          // Hash algorithm used
    ULONG hrp_hash_seed;                // Hash algorithm seed
    UCHAR hrp_flags;                    // Root page flags
    
    // Root page flags
    static constexpr UCHAR HRP_DYNAMIC_HASHING = 0x01;  // Uses dynamic hashing
    static constexpr UCHAR HRP_COMPRESSED_KEYS = 0x02;  // Keys are compressed
    static constexpr UCHAR HRP_STATISTICS_VALID = 0x04; // Statistics are up to date
    static constexpr UCHAR HRP_REBUILD_NEEDED = 0x08;   // Index needs rebuilding
    
    // Statistics for optimization
    ULONG hrp_total_lookups;            // Total lookup operations
    ULONG hrp_successful_lookups;       // Successful lookup operations
    ULONG hrp_collision_count;          // Hash collision count
    ULONG hrp_max_probe_distance;       // Maximum probe distance
    double hrp_average_probe_distance;  // Average probe distance
    
    GDS_TIMESTAMP hrp_creation_time;    // Index creation timestamp
    GDS_TIMESTAMP hrp_last_rebuild;     // Last rebuild timestamp
    GDS_TIMESTAMP hrp_last_statistics_update; // Last statistics update
    
    UCHAR hrp_reserved[32];             // Reserved for future use
};

//----------------------------
// Hash Directory Page Structure
//----------------------------

struct hash_directory_page
{
    pag hdp_header;                     // Standard page header
    ULONG hdp_level;                    // Directory level (0 = leaf level)
    ULONG hdp_parent_page;              // Parent directory page
    ULONG hdp_entry_count;              // Number of entries in this page
    ULONG hdp_max_entries;              // Maximum entries this page can hold
    UCHAR hdp_flags;                    // Directory page flags
    
    // Directory page flags
    static constexpr UCHAR HDP_LEAF_LEVEL = 0x01;      // This is a leaf directory page
    static constexpr UCHAR HDP_COMPRESSED = 0x02;      // Directory is compressed
    static constexpr UCHAR HDP_SORTED = 0x04;          // Entries are sorted
    
    // Variable-length array of directory entries follows
    struct directory_entry
    {
        ULONG de_bucket_id;             // Bucket identifier/hash range
        ULONG de_page_number;           // Page number containing bucket
        USHORT de_entry_count;          // Number of entries in bucket
        USHORT de_flags;                // Entry flags
        
        // Directory entry flags
        static constexpr USHORT DE_SPLIT_NEEDED = 0x01;    // Bucket needs splitting
        static constexpr USHORT DE_MERGE_CANDIDATE = 0x02; // Bucket is merge candidate
        static constexpr USHORT DE_HAS_OVERFLOW = 0x04;    // Bucket has overflow pages
    };
    
    directory_entry* getEntries() { return reinterpret_cast<directory_entry*>(this + 1); }
    const directory_entry* getEntries() const { return reinterpret_cast<const directory_entry*>(this + 1); }
    
    ULONG getMaxEntries() const { 
        return (HASH_BUCKET_SIZE - sizeof(hash_directory_page)) / sizeof(directory_entry);
    }
};

//----------------------------
// Hash Bucket Page Structure
//----------------------------

struct hash_bucket_page
{
    pag hbp_header;                     // Standard page header
    ULONG hbp_bucket_id;                // Bucket identifier
    ULONG hbp_overflow_page;            // Overflow page (if any)
    USHORT hbp_entry_count;             // Number of entries in bucket
    USHORT hbp_free_space;              // Available free space
    USHORT hbp_flags;                   // Bucket page flags
    
    // Bucket page flags
    static constexpr USHORT HBP_COMPRESSED = 0x01;     // Bucket data is compressed
    static constexpr USHORT HBP_SORTED = 0x02;         // Entries are sorted by hash
    static constexpr USHORT HBP_HAS_OVERFLOW = 0x04;   // Has overflow pages
    static constexpr USHORT HBP_SPLIT_IN_PROGRESS = 0x08; // Split operation in progress
    
    // Free space management
    USHORT hbp_free_space_offset;       // Offset to start of free space
    USHORT hbp_entry_offsets[HASH_MAX_ENTRIES_PER_BUCKET]; // Offsets to entries
    
    // Variable-length entry data follows
    UCHAR hbp_data[1];                  // Entry data area
    
    // Helper methods
    hash_entry* getEntry(USHORT index) {
        if (index >= hbp_entry_count) return nullptr;
        return reinterpret_cast<hash_entry*>(hbp_data + hbp_entry_offsets[index]);
    }
    
    const hash_entry* getEntry(USHORT index) const {
        if (index >= hbp_entry_count) return nullptr;
        return reinterpret_cast<const hash_entry*>(hbp_data + hbp_entry_offsets[index]);
    }
    
    ULONG getAvailableSpace() const {
        return HASH_BUCKET_SIZE - sizeof(hash_bucket_page) - hbp_free_space_offset;
    }
    
    bool needsSplit() const {
        return hbp_entry_count >= HASH_SPLIT_THRESHOLD;
    }
    
    bool canMerge() const {
        return hbp_entry_count <= HASH_MERGE_THRESHOLD;
    }
};

//----------------------------
// Hash Overflow Page Structure
//----------------------------

struct hash_overflow_page
{
    pag hop_header;                     // Standard page header
    ULONG hop_bucket_page;              // Parent bucket page
    ULONG hop_next_overflow;            // Next overflow page (chain)
    USHORT hop_entry_count;             // Number of entries
    USHORT hop_free_space;              // Available free space
    
    // Entry management (similar to bucket page)
    USHORT hop_free_space_offset;       // Offset to start of free space
    USHORT hop_entry_offsets[HASH_MAX_ENTRIES_PER_BUCKET]; // Entry offsets
    
    UCHAR hop_data[1];                  // Entry data area
    
    hash_entry* getEntry(USHORT index) {
        if (index >= hop_entry_count) return nullptr;
        return reinterpret_cast<hash_entry*>(hop_data + hop_entry_offsets[index]);
    }
    
    const hash_entry* getEntry(USHORT index) const {
        if (index >= hop_entry_count) return nullptr;
        return reinterpret_cast<const hash_entry*>(hop_data + hop_entry_offsets[index]);
    }
};

//----------------------------
// Hash Storage Statistics
//----------------------------

struct HashStorageStatistics
{
    ULONG total_pages;                  // Total pages used
    ULONG bucket_pages;                 // Number of bucket pages
    ULONG overflow_pages;               // Number of overflow pages
    ULONG directory_pages;              // Number of directory pages
    ULONG total_entries;                // Total entries stored
    ULONG deleted_entries;              // Number of deleted entries
    double load_factor;                 // Current load factor
    double space_utilization;           // Space utilization percentage
    ULONG collision_count;              // Total hash collisions
    double average_probe_distance;      // Average probe distance
    ULONG split_operations;             // Number of bucket splits
    ULONG merge_operations;             // Number of bucket merges
    ULONG overflow_chain_count;         // Number of overflow chains
    double average_overflow_chain_length; // Average overflow chain length
    
    HashStorageStatistics()
        : total_pages(0), bucket_pages(0), overflow_pages(0), directory_pages(0),
          total_entries(0), deleted_entries(0), load_factor(0.0), space_utilization(0.0),
          collision_count(0), average_probe_distance(0.0), split_operations(0),
          merge_operations(0), overflow_chain_count(0), average_overflow_chain_length(0.0)
    {
    }
};

//----------------------------
// Persistent Hash Storage Engine
//----------------------------

/**
 * Main engine for persistent hash table storage on disk pages
 */
class PersistentHashStorage
{
public:
    explicit PersistentHashStorage(Database* database, MemoryPool* pool);
    ~PersistentHashStorage();

    // Storage lifecycle
    bool createHashIndex(thread_db* tdbb, const index_desc* idx,
                        ULONG initial_bucket_count = HASH_INITIAL_DIRECTORY_SIZE);
    
    bool openHashIndex(thread_db* tdbb, const index_desc* idx);
    void closeHashIndex(thread_db* tdbb);
    bool dropHashIndex(thread_db* tdbb, const index_desc* idx);
    
    // Data operations
    bool insertEntry(thread_db* tdbb, ULONG hash_value, const UCHAR* key_data,
                    USHORT key_length, ULONG record_number);
    
    bool findEntry(thread_db* tdbb, ULONG hash_value, const UCHAR* key_data,
                  USHORT key_length, ULONG& record_number) const;
    
    bool deleteEntry(thread_db* tdbb, ULONG hash_value, const UCHAR* key_data,
                    USHORT key_length, ULONG record_number);
    
    bool updateEntry(thread_db* tdbb, ULONG hash_value, const UCHAR* old_key_data,
                    USHORT old_key_length, const UCHAR* new_key_data,
                    USHORT new_key_length, ULONG record_number);
    
    // Bulk operations
    bool insertBatch(thread_db* tdbb, const std::vector<hash_entry*>& entries);
    std::vector<ULONG> findMultiple(thread_db* tdbb, const std::vector<ULONG>& hash_values,
                                    const std::vector<std::pair<UCHAR*, USHORT>>& keys) const;
    
    // Iterator interface
    class HashIterator
    {
    public:
        explicit HashIterator(const PersistentHashStorage* storage);
        ~HashIterator();
        
        bool first(thread_db* tdbb);
        bool next(thread_db* tdbb);
        bool isValid() const;
        
        const hash_entry* getCurrentEntry() const;
        ULONG getCurrentHashValue() const;
        ULONG getCurrentRecordNumber() const;
        
        void reset();

    private:
        const PersistentHashStorage* m_storage;
        ULONG m_current_bucket;
        ULONG m_current_page;
        USHORT m_current_entry_index;
        BufferControl* m_current_bcb;
        bool m_is_valid;
        
        bool advanceToNextEntry(thread_db* tdbb);
        bool loadNextPage(thread_db* tdbb);
    };
    
    HashIterator createIterator() const;
    
    // Maintenance operations
    bool rebuildIndex(thread_db* tdbb);
    bool optimizeStorage(thread_db* tdbb);
    bool validateConsistency(thread_db* tdbb, ScratchBird::string& error_report) const;
    bool repairCorruption(thread_db* tdbb, const ScratchBird::string& repair_options);
    
    // Statistics and monitoring
    HashStorageStatistics getStatistics(thread_db* tdbb) const;
    void updateStatistics(thread_db* tdbb);
    void resetStatistics(thread_db* tdbb);
    
    // Configuration
    void setLoadFactorThreshold(double threshold);
    double getLoadFactorThreshold() const;
    
    void setSplitThreshold(ULONG threshold);
    ULONG getSplitThreshold() const;
    
    void setMergeThreshold(ULONG threshold);
    ULONG getMergeThreshold() const;
    
    // Analysis and tuning
    struct PerformanceAnalysis
    {
        double insertion_efficiency;       // Insertion performance rating
        double lookup_efficiency;          // Lookup performance rating
        double space_efficiency;           // Space utilization rating
        double maintenance_overhead;       // Maintenance cost rating
        bool needs_rebuilding;             // True if rebuild recommended
        ScratchBird::string recommendations; // Optimization recommendations
        
        PerformanceAnalysis()
            : insertion_efficiency(0.0), lookup_efficiency(0.0),
              space_efficiency(0.0), maintenance_overhead(0.0),
              needs_rebuilding(false)
        {
        }
    };
    
    PerformanceAnalysis analyzePerformance(thread_db* tdbb) const;
    void applyOptimizationRecommendations(thread_db* tdbb, const PerformanceAnalysis& analysis);

private:
    Database* m_database;
    MemoryPool* m_pool;
    
    // Index configuration
    const index_desc* m_index_descriptor;
    ULONG m_root_page_number;
    hash_root_page* m_root_page;
    BufferControl* m_root_bcb;
    
    // Storage parameters
    double m_load_factor_threshold;
    ULONG m_split_threshold;
    ULONG m_merge_threshold;
    USHORT m_hash_algorithm;
    ULONG m_hash_seed;
    
    // Page management
    ULONG allocateNewPage(thread_db* tdbb, UCHAR page_type);
    void deallocatePage(thread_db* tdbb, ULONG page_number);
    
    BufferControl* fetchPage(thread_db* tdbb, ULONG page_number, UCHAR expected_type) const;
    void releasePage(thread_db* tdbb, BufferControl* bcb, bool modified = false) const;
    
    // Directory management
    ULONG findBucketPage(thread_db* tdbb, ULONG hash_value) const;
    bool insertDirectoryEntry(thread_db* tdbb, ULONG bucket_id, ULONG page_number);
    bool removeDirectoryEntry(thread_db* tdbb, ULONG bucket_id);
    void rebuildDirectory(thread_db* tdbb);
    
    // Bucket operations
    BufferControl* findOrCreateBucket(thread_db* tdbb, ULONG hash_value);
    bool insertEntryInBucket(thread_db* tdbb, BufferControl* bucket_bcb,
                            ULONG hash_value, const UCHAR* key_data,
                            USHORT key_length, ULONG record_number);
    
    hash_entry* findEntryInBucket(thread_db* tdbb, BufferControl* bucket_bcb,
                                 ULONG hash_value, const UCHAR* key_data,
                                 USHORT key_length) const;
    
    bool deleteEntryFromBucket(thread_db* tdbb, BufferControl* bucket_bcb,
                              hash_entry* entry);
    
    // Bucket management
    bool splitBucket(thread_db* tdbb, BufferControl* bucket_bcb);
    bool mergeBuckets(thread_db* tdbb, ULONG bucket_id1, ULONG bucket_id2);
    void redistributeEntries(thread_db* tdbb, BufferControl* old_bucket_bcb,
                            BufferControl* new_bucket_bcb, ULONG split_hash);
    
    // Overflow management
    BufferControl* createOverflowPage(thread_db* tdbb, ULONG bucket_page);
    BufferControl* findOverflowPage(thread_db* tdbb, BufferControl* bucket_bcb,
                                   ULONG hash_value, const UCHAR* key_data,
                                   USHORT key_length) const;
    void cleanupOverflowChain(thread_db* tdbb, ULONG overflow_page);
    
    // Entry management
    hash_entry* allocateEntry(UCHAR* data_area, ULONG& free_offset,
                             ULONG hash_value, const UCHAR* key_data,
                             USHORT key_length, ULONG record_number) const;
    
    void deallocateEntry(hash_bucket_page* bucket, USHORT entry_index);
    void compactBucketPage(hash_bucket_page* bucket);
    
    // Hash algorithms
    ULONG calculateHash(const UCHAR* key_data, USHORT key_length) const;
    ULONG calculateSecondaryHash(ULONG primary_hash) const;
    ULONG getBucketId(ULONG hash_value, ULONG bucket_count) const;
    
    // Statistics helpers
    void updateRootPageStatistics(thread_db* tdbb);
    void collectBucketStatistics(thread_db* tdbb, HashStorageStatistics& stats) const;
    void analyzeHashDistribution(thread_db* tdbb, std::vector<ULONG>& distribution) const;
    
    // Consistency checking
    bool validateRootPage(thread_db* tdbb, ScratchBird::string& errors) const;
    bool validateDirectoryStructure(thread_db* tdbb, ScratchBird::string& errors) const;
    bool validateBucketPages(thread_db* tdbb, ScratchBird::string& errors) const;
    bool validateEntryConsistency(thread_db* tdbb, ScratchBird::string& errors) const;
    
    // Utility methods
    ULONG estimateOptimalBucketCount(ULONG entry_count) const;
    double calculateIdealLoadFactor(ULONG entry_count, ULONG bucket_count) const;
    bool shouldSplitBucket(const hash_bucket_page* bucket) const;
    bool shouldMergeBuckets(const hash_bucket_page* bucket1, const hash_bucket_page* bucket2) const;
    
    // Transaction support
    void logOperation(thread_db* tdbb, const char* operation, ULONG page_number);
    bool rollbackOperation(thread_db* tdbb, const char* operation, ULONG page_number);
};

//----------------------------
// Hash Storage Manager
//----------------------------

/**
 * Global manager for all persistent hash storage instances
 */
class HashStorageManager
{
public:
    static HashStorageManager* getInstance();
    
    // Storage instance management
    PersistentHashStorage* getStorage(Database* database, const index_desc* idx);
    void releaseStorage(PersistentHashStorage* storage);
    void clearStorageCache();
    
    // Global operations
    void flushAllStorages(thread_db* tdbb);
    void optimizeAllStorages(thread_db* tdbb);
    HashStorageStatistics getGlobalStatistics() const;
    
    // Configuration
    void setGlobalLoadFactorThreshold(double threshold);
    void setGlobalSplitThreshold(ULONG threshold);
    void setGlobalMergeThreshold(ULONG threshold);
    
    // Monitoring and diagnostics
    void startPerformanceMonitoring();
    void stopPerformanceMonitoring();
    ScratchBird::string generateGlobalDiagnosticsReport() const;

private:
    HashStorageManager();
    ~HashStorageManager();
    
    static HashStorageManager* s_instance;
    static ScratchBird::Mutex s_instance_mutex;
    
    struct StorageInstance
    {
        Database* database;
        USHORT index_id;
        std::unique_ptr<PersistentHashStorage> storage;
        ULONG reference_count;
        GDS_TIMESTAMP last_access;
        
        StorageInstance(Database* db, USHORT idx_id)
            : database(db), index_id(idx_id), reference_count(0), last_access(0) {}
    };
    
    std::vector<StorageInstance> m_storage_instances;
    mutable ScratchBird::Mutex m_instances_mutex;
    
    // Global configuration
    double m_global_load_factor_threshold;
    ULONG m_global_split_threshold;
    ULONG m_global_merge_threshold;
    bool m_performance_monitoring_enabled;
    
    // Cache management
    void evictUnusedInstances();
    StorageInstance* findStorageInstance(Database* database, USHORT index_id);
    void addStorageInstance(Database* database, USHORT index_id,
                           std::unique_ptr<PersistentHashStorage> storage);
};

//----------------------------
// Page Buffer Extensions
//----------------------------

/**
 * Extensions to page buffer management for hash storage
 */
class HashPageBufferExtensions
{
public:
    // Specialized buffer management for hash pages
    static BufferControl* fetchHashPage(thread_db* tdbb, ULONG page_number,
                                       UCHAR expected_type, bool for_update = false);
    
    static void releaseHashPage(thread_db* tdbb, BufferControl* bcb, bool modified = false);
    
    // Hash page validation
    static bool validateHashPage(const BufferControl* bcb, UCHAR expected_type);
    
    // Optimized hash page operations
    static void prefetchHashPages(thread_db* tdbb, const std::vector<ULONG>& page_numbers);
    static void flushHashPages(thread_db* tdbb, const std::vector<ULONG>& page_numbers);
    
    // Hash page cache optimization
    static void optimizeHashPageCache(thread_db* tdbb, const index_desc* idx);
    static void warmHashPageCache(thread_db* tdbb, const index_desc* idx);

private:
    static bool isHashPageType(UCHAR page_type);
    static void logHashPageAccess(thread_db* tdbb, ULONG page_number, const char* operation);
};

//----------------------------
// Integration with Existing Hash Index
//----------------------------

/**
 * Integration layer between persistent storage and existing hash index implementation
 */
class HashIndexStorageIntegration
{
public:
    // Storage backend registration
    static void registerPersistentStorage();
    static void unregisterPersistentStorage();
    
    // Storage lifecycle hooks
    static bool createStorageForIndex(thread_db* tdbb, const index_desc* idx);
    static bool openStorageForIndex(thread_db* tdbb, const index_desc* idx);
    static void closeStorageForIndex(thread_db* tdbb, const index_desc* idx);
    
    // Data operation hooks
    static bool insertUsingStorage(thread_db* tdbb, const index_desc* idx,
                                  ULONG hash_value, const UCHAR* key_data,
                                  USHORT key_length, ULONG record_number);
    
    static bool lookupUsingStorage(thread_db* tdbb, const index_desc* idx,
                                  ULONG hash_value, const UCHAR* key_data,
                                  USHORT key_length, ULONG& record_number);
    
    static bool deleteUsingStorage(thread_db* tdbb, const index_desc* idx,
                                  ULONG hash_value, const UCHAR* key_data,
                                  USHORT key_length, ULONG record_number);
    
    // Performance monitoring integration
    static void updatePerformanceMetrics(const index_desc* idx,
                                        const HashStorageStatistics& stats);

private:
    static std::map<USHORT, PersistentHashStorage*> s_storage_instances;
    static ScratchBird::Mutex s_integration_mutex;
};

//----------------------------
// Utility Functions
//----------------------------

// Hash storage detection and validation
bool isPersistentHashStorage(const index_desc* idx);
bool validateHashStoragePages(thread_db* tdbb, ULONG root_page);

// Hash value utilities
ULONG calculateCRC32Hash(const UCHAR* data, USHORT length, ULONG seed = 0);
ULONG calculateMurmurHash3(const UCHAR* data, USHORT length, ULONG seed = 0);
ULONG calculateFNVHash(const UCHAR* data, USHORT length);

// Storage analysis utilities
double calculateStorageEfficiency(const HashStorageStatistics& stats);
ULONG estimateStorageGrowth(const HashStorageStatistics& current_stats, double growth_factor);
bool needsStorageOptimization(const HashStorageStatistics& stats);

// Page type utilities
const char* getHashPageTypeName(UCHAR page_type);
bool isValidHashPageType(UCHAR page_type);

} // namespace Jrd

#endif // JRD_PERSISTENT_HASH_STORAGE_H