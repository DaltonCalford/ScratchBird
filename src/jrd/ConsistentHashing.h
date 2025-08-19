/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ConsistentHashing.h
 *	DESCRIPTION:	Consistent hashing for even key distribution in hash indexes
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
 * 2025.07.23 - ScratchBird Consistent Hashing Implementation
 */

#ifndef JRD_CONSISTENT_HASHING_H
#define JRD_CONSISTENT_HASHING_H

#include "../jrd/constants.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include <vector>
#include <map>
#include <memory>
#include <algorithm>

namespace Jrd {

// Forward declarations
class MemoryPool;
class thread_db;

//----------------------------
// Consistent Hashing Constants
//----------------------------

inline constexpr ULONG CONSISTENT_HASH_RING_SIZE = 4294967296UL;    // 2^32 hash ring size
inline constexpr ULONG DEFAULT_VIRTUAL_NODES = 150;                 // Virtual nodes per physical node
inline constexpr ULONG MIN_VIRTUAL_NODES = 50;                      // Minimum virtual nodes
inline constexpr ULONG MAX_VIRTUAL_NODES = 500;                     // Maximum virtual nodes
inline constexpr double LOAD_BALANCE_THRESHOLD = 0.15;              // 15% load imbalance threshold
inline constexpr ULONG REBALANCE_SAMPLE_SIZE = 10000;               // Sample size for rebalancing
inline constexpr double HOTSPOT_THRESHOLD = 2.0;                    // Hotspot detection threshold (2x average)

//----------------------------
// Hash Node Types
//----------------------------

enum HashNodeType : UCHAR
{
    HASH_NODE_PHYSICAL = 0,         // Physical hash bucket/partition
    HASH_NODE_VIRTUAL = 1,          // Virtual node for load balancing
    HASH_NODE_REPLICA = 2,          // Replica node for fault tolerance
    HASH_NODE_TEMPORARY = 3         // Temporary node during rebalancing
};

//----------------------------
// Hash Node State
//----------------------------

enum HashNodeState : UCHAR
{
    HASH_NODE_ACTIVE = 0,           // Node is active and serving requests
    HASH_NODE_DRAINING = 1,         // Node is being drained for removal
    HASH_NODE_JOINING = 2,          // Node is joining the ring
    HASH_NODE_FAILED = 3,           // Node has failed
    HASH_NODE_MAINTENANCE = 4       // Node is in maintenance mode
};

//----------------------------
// Hash Ring Node
//----------------------------

struct HashRingNode
{
    ULONG hash_position;            // Position on the hash ring (0 to 2^32-1)
    ULONG physical_node_id;         // Physical node/bucket identifier
    ULONG virtual_node_id;          // Virtual node identifier
    HashNodeType node_type;         // Type of node
    HashNodeState node_state;       // Current state of node
    
    // Load tracking
    ULONG key_count;                // Number of keys assigned to this node
    ULONG access_count;             // Number of accesses to this node
    double load_factor;             // Current load factor
    GDS_TIMESTAMP last_access;      // Last access timestamp
    
    // Rebalancing support
    ULONG keys_migrated_out;        // Keys migrated from this node
    ULONG keys_migrated_in;         // Keys migrated to this node
    bool is_hotspot;                // True if node is identified as hotspot
    
    HashRingNode()
        : hash_position(0), physical_node_id(0), virtual_node_id(0),
          node_type(HASH_NODE_PHYSICAL), node_state(HASH_NODE_ACTIVE),
          key_count(0), access_count(0), load_factor(0.0), last_access(0),
          keys_migrated_out(0), keys_migrated_in(0), is_hotspot(false)
    {
    }
    
    HashRingNode(ULONG pos, ULONG phys_id, ULONG virt_id, HashNodeType type)
        : hash_position(pos), physical_node_id(phys_id), virtual_node_id(virt_id),
          node_type(type), node_state(HASH_NODE_ACTIVE),
          key_count(0), access_count(0), load_factor(0.0), last_access(0),
          keys_migrated_out(0), keys_migrated_in(0), is_hotspot(false)
    {
    }
    
    bool operator<(const HashRingNode& other) const {
        return hash_position < other.hash_position;
    }
    
    void updateLoadFactor(ULONG total_keys, ULONG total_nodes) {
        if (total_nodes > 0) {
            double expected_load = static_cast<double>(total_keys) / total_nodes;
            load_factor = expected_load > 0 ? key_count / expected_load : 0.0;
        }
    }
};

//----------------------------
// Migration Record
//----------------------------

struct KeyMigrationRecord
{
    ULONG key_hash;                 // Hash value of migrated key
    ULONG source_node_id;           // Source physical node
    ULONG target_node_id;           // Target physical node
    GDS_TIMESTAMP migration_time;   // When migration occurred
    ULONG key_size;                 // Size of migrated key
    bool migration_completed;       // True if migration completed successfully
    
    KeyMigrationRecord()
        : key_hash(0), source_node_id(0), target_node_id(0),
          migration_time(0), key_size(0), migration_completed(false)
    {
    }
};

//----------------------------
// Load Balancing Statistics
//----------------------------

struct LoadBalancingStatistics
{
    ULONG total_nodes;              // Total physical nodes
    ULONG total_virtual_nodes;      // Total virtual nodes
    ULONG total_keys;               // Total keys in hash ring
    double average_load;            // Average load per node
    double load_variance;           // Load distribution variance
    double load_imbalance_ratio;    // Maximum load / minimum load
    
    ULONG hotspot_count;            // Number of hotspot nodes
    ULONG underutilized_count;      // Number of underutilized nodes
    ULONG migrations_performed;     // Total migrations performed
    ULONG keys_migrated;            // Total keys migrated
    
    double distribution_quality;    // Overall distribution quality (0-1)
    bool requires_rebalancing;      // True if rebalancing needed
    
    LoadBalancingStatistics()
        : total_nodes(0), total_virtual_nodes(0), total_keys(0),
          average_load(0.0), load_variance(0.0), load_imbalance_ratio(1.0),
          hotspot_count(0), underutilized_count(0), migrations_performed(0),
          keys_migrated(0), distribution_quality(1.0), requires_rebalancing(false)
    {
    }
};

//----------------------------
// Consistent Hash Ring
//----------------------------

/**
 * Main consistent hashing implementation with virtual nodes and load balancing
 */
class ConsistentHashRing
{
public:
    explicit ConsistentHashRing(MemoryPool* pool, ULONG virtual_nodes_per_physical = DEFAULT_VIRTUAL_NODES);
    ~ConsistentHashRing();

    // Ring management
    bool addPhysicalNode(ULONG node_id, ULONG virtual_node_count = 0);
    bool removePhysicalNode(ULONG node_id);
    bool updateNodeState(ULONG node_id, HashNodeState new_state);
    
    // Key routing
    ULONG findNodeForKey(ULONG key_hash) const;
    ULONG findNodeForKey(const UCHAR* key_data, USHORT key_length) const;
    
    std::vector<ULONG> findReplicaNodes(ULONG key_hash, ULONG replica_count) const;
    
    // Load balancing
    void recordKeyAccess(ULONG key_hash, ULONG access_count = 1);
    void recordKeyInsertion(ULONG key_hash, ULONG key_size = 1);
    void recordKeyDeletion(ULONG key_hash, ULONG key_size = 1);
    
    // Rebalancing operations
    bool needsRebalancing() const;
    std::vector<KeyMigrationRecord> planRebalancing();
    bool executeRebalancing(const std::vector<KeyMigrationRecord>& migration_plan);
    
    // Hotspot detection and mitigation
    std::vector<ULONG> detectHotspots(double threshold = HOTSPOT_THRESHOLD) const;
    bool mitigateHotspot(ULONG node_id, ULONG additional_virtual_nodes);
    
    // Statistics and monitoring
    LoadBalancingStatistics getStatistics() const;
    void resetStatistics();
    
    // Ring analysis
    double calculateLoadImbalance() const;
    double calculateDistributionQuality() const;
    std::vector<std::pair<ULONG, double>> getNodeLoadDistribution() const;
    
    // Configuration
    void setVirtualNodesPerPhysical(ULONG count);
    ULONG getVirtualNodesPerPhysical() const;
    
    void setLoadBalanceThreshold(double threshold);
    double getLoadBalanceThreshold() const;
    
    void setHotspotThreshold(double threshold);
    double getHotspotThreshold() const;
    
    // Debugging and diagnostics
    void printRingStatistics() const;
    ScratchBird::string generateDiagnosticsReport() const;
    bool validateRingConsistency() const;

private:
    MemoryPool* m_pool;
    
    // Ring configuration
    ULONG m_virtual_nodes_per_physical;
    double m_load_balance_threshold;
    double m_hotspot_threshold;
    
    // Hash ring storage (sorted by hash position)
    std::vector<HashRingNode> m_ring_nodes;
    
    // Physical node tracking
    std::map<ULONG, std::vector<ULONG>> m_physical_to_virtual;  // Physical -> Virtual node IDs
    std::map<ULONG, ULONG> m_virtual_to_physical;              // Virtual -> Physical node ID
    
    // Load tracking
    std::map<ULONG, ULONG> m_node_key_counts;      // Keys per physical node
    std::map<ULONG, ULONG> m_node_access_counts;   // Accesses per physical node
    std::vector<KeyMigrationRecord> m_migration_history;
    
    // Statistics
    mutable LoadBalancingStatistics m_cached_statistics;
    mutable bool m_statistics_dirty;
    
    // Ring operations
    void addVirtualNode(ULONG physical_node_id, ULONG virtual_node_id, ULONG hash_position);
    void removeVirtualNode(ULONG virtual_node_id);
    void rebuildRing();
    
    // Hash calculation
    ULONG calculateNodeHash(ULONG physical_node_id, ULONG virtual_node_id) const;
    ULONG calculateKeyHash(const UCHAR* key_data, USHORT key_length) const;
    
    // Load balancing helpers
    void updateNodeStatistics();
    void identifyHotspots();
    void identifyUnderutilizedNodes();
    
    // Migration planning
    std::vector<KeyMigrationRecord> planHotspotMitigation(ULONG hotspot_node_id);
    std::vector<KeyMigrationRecord> planLoadRebalancing();
    void optimizeMigrationPlan(std::vector<KeyMigrationRecord>& plan);
    
    // Ring search and navigation
    std::vector<HashRingNode>::const_iterator findSuccessorNode(ULONG hash_position) const;
    std::vector<HashRingNode>::const_iterator findPredecessorNode(ULONG hash_position) const;
    
    // Validation helpers
    bool validateNodeConfiguration() const;
    bool validateVirtualNodeDistribution() const;
    
    // Statistics calculation
    void calculateStatistics() const;
    double calculateVariance() const;
    double calculateGiniCoefficient() const;
};

//----------------------------
// Hash Function Algorithms
//----------------------------

/**
 * Collection of hash functions optimized for consistent hashing
 */
class ConsistentHashFunctions
{
public:
    // Primary hash functions
    static ULONG murmurHash3(const UCHAR* data, USHORT length, ULONG seed = 0);
    static ULONG cityHash64(const UCHAR* data, USHORT length);
    static ULONG xxHash64(const UCHAR* data, USHORT length, ULONG seed = 0);
    static ULONG sipHash24(const UCHAR* data, USHORT length, ULONG seed = 0);
    
    // Composite hash functions for better distribution
    static ULONG consistentHash(const UCHAR* data, USHORT length, ULONG seed = 0);
    static ULONG jumpConsistentHash(ULONG key, ULONG num_buckets);
    
    // Hash quality analysis
    static double analyzeHashDistribution(const std::vector<ULONG>& hash_values);
    static bool validateHashFunction(ULONG (*hash_func)(const UCHAR*, USHORT, ULONG),
                                    const std::vector<std::pair<UCHAR*, USHORT>>& test_data);
    
    // Hash algorithm selection
    enum HashAlgorithm : UCHAR
    {
        HASH_MURMUR3 = 0,
        HASH_CITYHASH = 1,
        HASH_XXHASH = 2,
        HASH_SIPHASH = 3,
        HASH_CONSISTENT = 4,
        HASH_JUMP = 5
    };
    
    static ULONG calculateHash(const UCHAR* data, USHORT length, HashAlgorithm algorithm, ULONG seed = 0);
    static HashAlgorithm selectOptimalAlgorithm(const std::vector<std::pair<UCHAR*, USHORT>>& sample_data);

private:
    // Hash function implementations
    static ULONG murmurHash3_32(const UCHAR* key, USHORT len, ULONG seed);
    static ULONG cityHash64_impl(const UCHAR* s, USHORT len);
    static ULONG xxHash64_impl(const UCHAR* input, USHORT len, ULONG seed);
    static ULONG sipHash24_impl(const UCHAR* in, USHORT inlen, ULONG seed);
    
    // Quality metrics
    static double calculateChiSquare(const std::vector<ULONG>& observed, ULONG expected);
    static double calculateEntropy(const std::vector<ULONG>& values);
    static double calculateAvalancheEffect(ULONG (*hash_func)(const UCHAR*, USHORT, ULONG));
};

//----------------------------
// Dynamic Rebalancing Engine
//----------------------------

/**
 * Engine for dynamic rebalancing of consistent hash rings
 */
class DynamicRebalancingEngine
{
public:
    explicit DynamicRebalancingEngine(MemoryPool* pool);
    ~DynamicRebalancingEngine();

    // Rebalancing strategies
    enum RebalancingStrategy : UCHAR
    {
        REBALANCE_CONSERVATIVE = 0,     // Minimal data movement
        REBALANCE_AGGRESSIVE = 1,       // Optimal distribution with more movement
        REBALANCE_HOTSPOT_FOCUSED = 2,  // Focus on hotspot mitigation
        REBALANCE_GRADUAL = 3,          // Gradual rebalancing over time
        REBALANCE_EMERGENCY = 4         // Emergency rebalancing for failed nodes
    };
    
    // Main rebalancing interface
    std::vector<KeyMigrationRecord> planRebalancing(const ConsistentHashRing& ring,
                                                    RebalancingStrategy strategy);
    
    double estimateRebalancingCost(const std::vector<KeyMigrationRecord>& plan) const;
    
    bool executeRebalancingPlan(ConsistentHashRing& ring,
                               const std::vector<KeyMigrationRecord>& plan,
                               thread_db* tdbb);
    
    // Strategy-specific planning
    std::vector<KeyMigrationRecord> planConservativeRebalancing(const ConsistentHashRing& ring);
    std::vector<KeyMigrationRecord> planAggressiveRebalancing(const ConsistentHashRing& ring);
    std::vector<KeyMigrationRecord> planHotspotRebalancing(const ConsistentHashRing& ring);
    std::vector<KeyMigrationRecord> planGradualRebalancing(const ConsistentHashRing& ring, ULONG max_migrations);
    std::vector<KeyMigrationRecord> planEmergencyRebalancing(const ConsistentHashRing& ring, ULONG failed_node_id);
    
    // Rebalancing optimization
    void optimizeMigrationOrder(std::vector<KeyMigrationRecord>& plan) const;
    void minimizeDataMovement(std::vector<KeyMigrationRecord>& plan) const;
    void parallelizeMigrations(std::vector<KeyMigrationRecord>& plan, ULONG max_parallel) const;
    
    // Progress monitoring
    struct RebalancingProgress
    {
        ULONG total_migrations;
        ULONG completed_migrations;
        ULONG failed_migrations;
        ULONG keys_migrated;
        ULONG bytes_migrated;
        double completion_percentage;
        GDS_TIMESTAMP start_time;
        GDS_TIMESTAMP estimated_completion;
        
        RebalancingProgress()
            : total_migrations(0), completed_migrations(0), failed_migrations(0),
              keys_migrated(0), bytes_migrated(0), completion_percentage(0.0),
              start_time(0), estimated_completion(0)
        {
        }
    };
    
    RebalancingProgress getProgress() const;
    void resetProgress();
    
    // Configuration
    void setMaxConcurrentMigrations(ULONG max_concurrent);
    void setMigrationBatchSize(ULONG batch_size);
    void setRebalancingTimeout(ULONG timeout_seconds);

private:
    MemoryPool* m_pool;
    
    // Configuration
    ULONG m_max_concurrent_migrations;
    ULONG m_migration_batch_size;
    ULONG m_rebalancing_timeout;
    
    // Progress tracking
    RebalancingProgress m_progress;
    mutable ScratchBird::Mutex m_progress_mutex;
    
    // Migration execution
    bool executeSingleMigration(const KeyMigrationRecord& migration, thread_db* tdbb);
    bool executeMigrationBatch(const std::vector<KeyMigrationRecord>& batch, thread_db* tdbb);
    
    // Cost estimation
    double calculateMigrationCost(const KeyMigrationRecord& migration) const;
    double calculateNetworkCost(ULONG source_node, ULONG target_node, ULONG data_size) const;
    double calculateDiskCost(ULONG data_size) const;
    
    // Planning helpers
    std::vector<ULONG> identifyOverloadedNodes(const ConsistentHashRing& ring, double threshold) const;
    std::vector<ULONG> identifyUnderloadedNodes(const ConsistentHashRing& ring, double threshold) const;
    ULONG findOptimalTargetNode(const ConsistentHashRing& ring, ULONG source_node, ULONG key_hash) const;
    
    // Migration validation
    bool validateMigrationPlan(const std::vector<KeyMigrationRecord>& plan) const;
    bool checkMigrationConstraints(const KeyMigrationRecord& migration) const;
};

//----------------------------
// Hash Ring Manager
//----------------------------

/**
 * Global manager for consistent hash rings across the database
 */
class HashRingManager
{
public:
    static HashRingManager* getInstance();
    
    // Ring management
    ConsistentHashRing* createHashRing(const ScratchBird::string& ring_name,
                                      ULONG virtual_nodes_per_physical = DEFAULT_VIRTUAL_NODES);
    
    ConsistentHashRing* getHashRing(const ScratchBird::string& ring_name);
    bool destroyHashRing(const ScratchBird::string& ring_name);
    
    // Global operations
    void rebalanceAllRings();
    LoadBalancingStatistics getGlobalStatistics() const;
    void resetAllStatistics();
    
    // Configuration
    void setGlobalVirtualNodesCount(ULONG count);
    void setGlobalLoadBalanceThreshold(double threshold);
    void setGlobalHotspotThreshold(double threshold);
    
    // Monitoring
    void startMonitoring();
    void stopMonitoring();
    ScratchBird::string generateGlobalReport() const;

private:
    HashRingManager();
    ~HashRingManager();
    
    static HashRingManager* s_instance;
    static ScratchBird::Mutex s_instance_mutex;
    
    struct RingInstance
    {
        ScratchBird::string name;
        std::unique_ptr<ConsistentHashRing> ring;
        std::unique_ptr<DynamicRebalancingEngine> rebalancer;
        GDS_TIMESTAMP creation_time;
        GDS_TIMESTAMP last_access;
        
        RingInstance(const ScratchBird::string& ring_name)
            : name(ring_name), creation_time(0), last_access(0) {}
    };
    
    std::vector<RingInstance> m_rings;
    mutable ScratchBird::Mutex m_rings_mutex;
    
    // Global configuration
    ULONG m_global_virtual_nodes;
    double m_global_load_balance_threshold;
    double m_global_hotspot_threshold;
    bool m_monitoring_enabled;
    
    // Monitoring thread
    void monitoringThreadProc();
    bool m_monitoring_active;
    
    RingInstance* findRingInstance(const ScratchBird::string& name);
};

//----------------------------
// Integration with Hash Index
//----------------------------

/**
 * Integration layer between consistent hashing and hash index implementation
 */
class HashIndexConsistentHashingIntegration
{
public:
    // Integration setup
    static bool enableConsistentHashing(const ScratchBird::string& index_name,
                                       ULONG initial_node_count,
                                       ULONG virtual_nodes_per_physical = DEFAULT_VIRTUAL_NODES);
    
    static bool disableConsistentHashing(const ScratchBird::string& index_name);
    
    // Hash routing
    static ULONG routeKeyToNode(const ScratchBird::string& index_name,
                               const UCHAR* key_data, USHORT key_length);
    
    static std::vector<ULONG> routeKeyToReplicas(const ScratchBird::string& index_name,
                                                const UCHAR* key_data, USHORT key_length,
                                                ULONG replica_count);
    
    // Load balancing integration
    static void recordKeyOperation(const ScratchBird::string& index_name,
                                  const UCHAR* key_data, USHORT key_length,
                                  const char* operation);
    
    static bool triggerRebalancing(const ScratchBird::string& index_name);
    
    // Statistics integration
    static LoadBalancingStatistics getIndexStatistics(const ScratchBird::string& index_name);
    static void updateIndexConfiguration(const ScratchBird::string& index_name,
                                        ULONG virtual_nodes, double load_threshold);

private:
    static std::map<ScratchBird::string, ScratchBird::string> s_index_to_ring_mapping;
    static ScratchBird::Mutex s_integration_mutex;
};

//----------------------------
// Utility Functions
//----------------------------

// Hash ring analysis
double calculateHashRingBalance(const std::vector<ULONG>& node_loads);
double calculateLoadDistributionQuality(const std::vector<double>& load_factors);
bool detectLoadImbalance(const std::vector<ULONG>& node_loads, double threshold = LOAD_BALANCE_THRESHOLD);

// Key distribution analysis
std::vector<ULONG> analyzeKeyDistribution(const std::vector<ULONG>& key_hashes,
                                         ULONG num_nodes, ULONG virtual_nodes_per_physical);

ULONG estimateOptimalVirtualNodes(ULONG num_physical_nodes, ULONG expected_keys,
                                 double target_balance_quality = 0.95);

// Migration utilities
ULONG calculateMigrationSize(const std::vector<KeyMigrationRecord>& migrations);
double estimateMigrationTime(const std::vector<KeyMigrationRecord>& migrations,
                           double migration_rate_keys_per_second);

// Hash algorithm utilities
bool validateHashAlgorithmSuitability(const UCHAR* sample_data, USHORT data_size,
                                     ConsistentHashFunctions::HashAlgorithm algorithm);

ConsistentHashFunctions::HashAlgorithm selectBestHashAlgorithm(
    const std::vector<std::pair<UCHAR*, USHORT>>& sample_data);

} // namespace Jrd

#endif // JRD_CONSISTENT_HASHING_H