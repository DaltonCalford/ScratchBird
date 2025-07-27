#ifndef RTREE_ALGORITHMS_H
#define RTREE_ALGORITHMS_H

#include "RTreePageStructures.h"
#include "SpatialDataTypes.h"
#include "MBROperations.h"
#include "common/classes/alloc.h"
#include "common/classes/array.h"
#include <vector>
#include <queue>
#include <stack>

namespace ScratchBird {

// Forward declarations
class RTreeIndex;
class RTreeSearchResult;
class RTreeInsertContext;
class RTreeDeleteContext;

// R-Tree search query types
enum RTreeQueryType : UCHAR
{
    RTREE_QUERY_INTERSECTS = 1,    // Find geometries that intersect query MBR
    RTREE_QUERY_CONTAINS = 2,      // Find geometries contained within query MBR
    RTREE_QUERY_CONTAINED = 3,     // Find geometries that contain query MBR
    RTREE_QUERY_TOUCHES = 4,       // Find geometries that touch query MBR
    RTREE_QUERY_CROSSES = 5,       // Find geometries that cross query MBR
    RTREE_QUERY_OVERLAPS = 6,      // Find geometries that overlap query MBR
    RTREE_QUERY_WITHIN_DISTANCE = 7,// Find geometries within distance
    RTREE_QUERY_KNN = 8            // K-nearest neighbor query
};

// R-Tree search result entry
struct RTreeSearchResult
{
    RecordNumber recordNumber;     // Record number of matching geometry
    ExtendedMBR mbr;              // MBR of the geometry
    double distance;              // Distance to query (for KNN queries)
    Geometry* geometry;           // Full geometry (optional, loaded on demand)
    
    RTreeSearchResult() : recordNumber(0), distance(0.0), geometry(nullptr) {}
    RTreeSearchResult(RecordNumber rec, const ExtendedMBR& m, double d = 0.0)
        : recordNumber(rec), mbr(m), distance(d), geometry(nullptr) {}
};

// R-Tree search parameters
struct RTreeSearchParams
{
    RTreeQueryType queryType;
    ExtendedMBR queryMBR;
    double maxDistance;           // For distance queries
    ULONG maxResults;             // Maximum results to return (0 = unlimited)
    ULONG k;                      // For KNN queries
    bool loadGeometry;            // Whether to load full geometry
    bool sortByDistance;          // Whether to sort results by distance
    
    RTreeSearchParams() : queryType(RTREE_QUERY_INTERSECTS), maxDistance(0.0),
                         maxResults(0), k(0), loadGeometry(false), sortByDistance(false) {}
};

// R-Tree insertion context for managing split propagation
class RTreeInsertContext
{
private:
    RTreeIndex& index;
    MemoryPool& pool;
    std::vector<ULONG> pathPages;        // Path from root to insertion point
    std::vector<USHORT> pathIndices;     // Entry indices along the path
    bool needsSplit;                     // Whether split occurred
    
public:
    RTreeInsertContext(RTreeIndex& idx, MemoryPool& p);
    ~RTreeInsertContext();
    
    // Path management
    void addToPath(ULONG pageNumber, USHORT entryIndex);
    void clearPath();
    const std::vector<ULONG>& getPathPages() const { return pathPages; }
    const std::vector<USHORT>& getPathIndices() const { return pathIndices; }
    
    // Split handling
    bool getNeedsSplit() const { return needsSplit; }
    void setNeedsSplit(bool splits) { needsSplit = splits; }
    
    // Context operations
    void propagateSplit(RTreePage* newPage1, RTreePage* newPage2, USHORT level);
    void updateMBRs();
    void adjustTree();
};

// R-Tree deletion context for managing underflow and merging
class RTreeDeleteContext
{
private:
    RTreeIndex& index;
    MemoryPool& pool;
    std::vector<ULONG> underflowPages;   // Pages that need reinsert/merge
    std::vector<RTreeEntry*> orphanedEntries; // Entries needing reinsertion
    
public:
    RTreeDeleteContext(RTreeIndex& idx, MemoryPool& p);
    ~RTreeDeleteContext();
    
    // Underflow management
    void addUnderflowPage(ULONG pageNumber);
    void addOrphanedEntry(RTreeEntry* entry);
    
    // Context operations
    void handleUnderflow();
    void reinsertOrphans();
    void condenseTree();
};

// Main R-Tree algorithm implementation
class RTreeAlgorithms
{
private:
    RTreeIndex& index;
    MemoryPool& pool;
    
public:
    RTreeAlgorithms(RTreeIndex& idx, MemoryPool& p);
    ~RTreeAlgorithms();
    
    // Core R-Tree operations
    bool insert(const ExtendedMBR& mbr, RecordNumber recordNumber, const Geometry* geometry = nullptr);
    bool remove(const ExtendedMBR& mbr, RecordNumber recordNumber);
    std::vector<RTreeSearchResult> search(const RTreeSearchParams& params);
    
    // Specialized search operations
    std::vector<RTreeSearchResult> intersectionSearch(const ExtendedMBR& queryMBR);
    std::vector<RTreeSearchResult> containmentSearch(const ExtendedMBR& queryMBR);
    std::vector<RTreeSearchResult> withinDistanceSearch(const ExtendedMBR& queryMBR, double maxDistance);
    std::vector<RTreeSearchResult> kNearestNeighbors(const ExtendedMBR& queryMBR, ULONG k);
    
    // Bulk operations
    bool bulkInsert(const std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries);
    bool bulkDelete(const std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries);
    
    // Tree maintenance operations
    void rebuild();
    void optimize();
    double calculateTreeQuality();
    void balanceTree();
    
    // Statistics and analysis
    struct TreeStatistics
    {
        ULONG totalPages;
        ULONG leafPages;
        ULONG internalPages;
        ULONG totalEntries;
        USHORT maxLevel;
        double averageFanout;
        double averagePageUtilization;
        double totalArea;
        double totalOverlap;
        double averageSearchCost;
        double storageEfficiency;
    };
    
    TreeStatistics getStatistics();
    void analyzePerformance(const std::vector<RTreeSearchParams>& queries, string& report);
    
private:
    // Insert algorithm helpers
    ULONG chooseLeaf(const ExtendedMBR& mbr);
    bool insertIntoLeaf(RTreePage* leafPage, const ExtendedMBR& mbr, RecordNumber recordNumber, const Geometry* geometry);
    void splitNode(RTreePage* page, RTreeInsertContext& context);
    void adjustTree(RTreeInsertContext& context);
    
    // Search algorithm helpers
    void searchRecursive(ULONG pageNumber, const RTreeSearchParams& params, std::vector<RTreeSearchResult>& results);
    bool testSpatialRelation(const ExtendedMBR& entryMBR, const RTreeSearchParams& params);
    void knnSearchRecursive(ULONG pageNumber, const ExtendedMBR& queryMBR, ULONG k, 
                           std::priority_queue<std::pair<double, RTreeSearchResult>>& results);
    
    // Delete algorithm helpers
    RTreePage* findLeaf(const ExtendedMBR& mbr, RecordNumber recordNumber);
    void condenseTree(RTreeDeleteContext& context, ULONG leafPage);
    bool mergeNodes(RTreePage* page1, RTreePage* page2);
    
    // Node split algorithms
    std::pair<RTreePage*, RTreePage*> linearSplit(RTreePage* page);
    std::pair<RTreePage*, RTreePage*> quadraticSplit(RTreePage* page);
    std::pair<RTreePage*, RTreePage*> rStarSplit(RTreePage* page);
    
    // Tree structure helpers
    void updateMBR(ULONG pageNumber, USHORT entryIndex, const ExtendedMBR& newMBR);
    ULONG createNewRoot(RTreePage* leftChild, RTreePage* rightChild);
    void deleteRoot();
    
    // Bulk loading helpers
    std::vector<RTreePage*> bulkLoadBottomUp(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries);
    void sortTileRecursive(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries, ULONG fanout);
    void hilbertSort(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries);
    
    // Optimization helpers
    void rebalanceSubtree(ULONG rootPageNumber);
    void eliminateDeadSpace(RTreePage* page);
    void minimizeOverlap(RTreePage* page);
    
    // Utility functions
    double calculateEnlargement(const ExtendedMBR& existingMBR, const ExtendedMBR& newMBR);
    double calculateOverlapIncrease(RTreePage* page, const ExtendedMBR& newMBR, USHORT excludeIndex);
    USHORT selectBestChild(RTreePage* page, const ExtendedMBR& mbr);
    bool isUnderflow(RTreePage* page);
    void collectSubtreeEntries(ULONG pageNumber, std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries);
};

// R-Tree bulk loading utilities
class RTreeBulkLoader
{
private:
    RTreeIndex& index;
    MemoryPool& pool;
    ULONG targetFanout;
    
    // Sort-Tile-Recursive (STR) parameters
    struct STRParams
    {
        ULONG totalEntries;
        ULONG leafCapacity;
        ULONG numLeaves;
        ULONG stripeCapacity;
        ULONG numStripes;
    };
    
public:
    RTreeBulkLoader(RTreeIndex& idx, MemoryPool& p, ULONG fanout = RTREE_FANOUT_TARGET);
    ~RTreeBulkLoader();
    
    // Bulk loading methods
    bool loadFromSortedData(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries);
    bool loadFromUnsortedData(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries);
    
    // Sorting strategies
    void sortByHilbert(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries);
    void sortByZOrder(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries);
    void sortTileRecursive(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries);
    
    // Performance optimization
    void setTargetFanout(ULONG fanout) { targetFanout = fanout; }
    ULONG getOptimalFanout(ULONG totalEntries);
    
private:
    // STR implementation
    STRParams calculateSTRParams(ULONG totalEntries);
    std::vector<RTreePage*> createLeafLevel(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries);
    std::vector<RTreePage*> createInternalLevel(std::vector<RTreePage*>& childPages, USHORT level);
    
    // Hilbert curve utilities
    ULONG64 calculateHilbertValue(const ExtendedMBR& mbr, USHORT order = 16);
    ULONG64 hilbertCurve2D(ULONG x, ULONG y, USHORT order);
    
    // Z-order curve utilities
    ULONG64 calculateZOrderValue(const ExtendedMBR& mbr);
    ULONG64 interleaveBits(ULONG x, ULONG y);
};

// R-Tree query optimizer for complex spatial queries
class RTreeQueryOptimizer
{
private:
    RTreeIndex& index;
    MemoryPool& pool;
    
    // Query execution plan
    struct QueryPlan
    {
        std::vector<RTreeQueryType> operations;
        std::vector<ExtendedMBR> queryMBRs;
        std::vector<double> selectivities;
        double estimatedCost;
        bool useIndex;
    };
    
public:
    RTreeQueryOptimizer(RTreeIndex& idx, MemoryPool& p);
    ~RTreeQueryOptimizer();
    
    // Query optimization
    QueryPlan optimizeQuery(const std::vector<RTreeSearchParams>& queries);
    double estimateQueryCost(const RTreeSearchParams& params);
    double estimateSelectivity(const RTreeSearchParams& params);
    
    // Index usage analysis
    bool shouldUseIndex(const RTreeSearchParams& params);
    double calculateIndexBenefit(const RTreeSearchParams& params);
    
    // Multi-query optimization
    std::vector<RTreeSearchResult> executeMultiQuery(const std::vector<RTreeSearchParams>& queries);
    void optimizeQueryOrder(std::vector<RTreeSearchParams>& queries);
    
private:
    // Cost estimation
    double estimatePageReads(const ExtendedMBR& queryMBR, USHORT level);
    double estimateOverlapRatio(const ExtendedMBR& queryMBR);
    double estimateFilterCost(RTreeQueryType queryType);
    
    // Selectivity estimation
    double calculateAreaSelectivity(const ExtendedMBR& queryMBR);
    double calculateDistanceSelectivity(const ExtendedMBR& queryMBR, double maxDistance);
    double adjustSelectivityForDistribution(double rawSelectivity, const ExtendedMBR& queryMBR);
};

// R-Tree maintenance and optimization utilities
class RTreeMaintenance
{
private:
    RTreeIndex& index;
    MemoryPool& pool;
    
public:
    RTreeMaintenance(RTreeIndex& idx, MemoryPool& p);
    ~RTreeMaintenance();
    
    // Tree validation
    bool validateTree();
    std::vector<string> validateStructure();
    bool validateMBRConsistency();
    bool validateParentChildRelations();
    
    // Tree optimization
    void optimizeTree();
    void rebalanceTree();
    void minimizeOverlap();
    void eliminateDeadSpace();
    void consolidatePages();
    
    // Defragmentation
    void defragmentTree();
    double calculateFragmentation();
    void compactTree();
    
    // Statistics collection
    void updateStatistics();
    void collectDetailedStatistics(RTreeAlgorithms::TreeStatistics& stats);
    
    // Performance monitoring
    struct PerformanceMetrics
    {
        ULONG64 totalSearches;
        ULONG64 totalInserts;
        ULONG64 totalDeletes;
        double averageSearchTime;
        double averageInsertTime;
        double averageDeleteTime;
        double cacheHitRate;
        double pageUtilization;
    };
    
    PerformanceMetrics getPerformanceMetrics();
    void resetPerformanceMetrics();
    
private:
    // Validation helpers
    bool validatePage(RTreePage* page, USHORT expectedLevel);
    bool validateMBRCoverage(RTreePage* parentPage, RTreePage* childPage, USHORT entryIndex);
    
    // Optimization helpers
    void optimizeSubtree(ULONG pageNumber);
    bool shouldMergePages(RTreePage* page1, RTreePage* page2);
    void redistributeEntries(std::vector<RTreePage*>& pages);
    
    // Defragmentation helpers
    std::vector<ULONG> identifyFragmentedPages();
    void relocateEntries(const std::vector<ULONG>& fragmentedPages);
};

// R-Tree concurrent access support
class RTreeConcurrency
{
private:
    RTreeIndex& index;
    MemoryPool& pool;
    
    // Lock modes
    enum LockMode
    {
        LOCK_SHARED,
        LOCK_EXCLUSIVE,
        LOCK_INTENT_SHARED,
        LOCK_INTENT_EXCLUSIVE
    };
    
    // Lock manager interface
    struct PageLock
    {
        ULONG pageNumber;
        LockMode mode;
        ULONG threadId;
        ULONG64 timestamp;
    };
    
    std::vector<PageLock> activeLocks;
    
public:
    RTreeConcurrency(RTreeIndex& idx, MemoryPool& p);
    ~RTreeConcurrency();
    
    // Lock management
    bool acquireLock(ULONG pageNumber, LockMode mode);
    void releaseLock(ULONG pageNumber);
    void releaseAllLocks();
    
    // Concurrent operations
    bool concurrentInsert(const ExtendedMBR& mbr, RecordNumber recordNumber, const Geometry* geometry = nullptr);
    bool concurrentDelete(const ExtendedMBR& mbr, RecordNumber recordNumber);
    std::vector<RTreeSearchResult> concurrentSearch(const RTreeSearchParams& params);
    
    // Deadlock handling
    bool detectDeadlock();
    void resolveDeadlock();
    
private:
    // Lock protocol helpers
    bool isLockCompatible(LockMode existingMode, LockMode requestedMode);
    void upgradeToExclusive(ULONG pageNumber);
    void downgradeLock(ULONG pageNumber, LockMode newMode);
    
    // Concurrent algorithm modifications
    ULONG chooseLeafConcurrent(const ExtendedMBR& mbr);
    void splitNodeConcurrent(RTreePage* page, RTreeInsertContext& context);
};

// Performance analysis and profiling
namespace RTreeProfiler
{
    // Profiling data structures
    struct OperationProfile
    {
        string operationName;
        ULONG64 callCount;
        double totalTime;
        double averageTime;
        double minTime;
        double maxTime;
    };
    
    struct QueryProfile
    {
        RTreeQueryType queryType;
        ExtendedMBR queryMBR;
        ULONG resultCount;
        double executionTime;
        ULONG pagesAccessed;
        double selectivity;
    };
    
    // Profiling interface
    void startProfiling();
    void stopProfiling();
    void resetProfile();
    
    // Operation timing
    void recordOperation(const string& operation, double duration);
    void recordQuery(const QueryProfile& profile);
    
    // Analysis functions
    std::vector<OperationProfile> getOperationProfiles();
    std::vector<QueryProfile> getQueryProfiles();
    string generatePerformanceReport();
    
    // Bottleneck identification
    std::vector<string> identifyBottlenecks();
    string analyzeQueryPerformance(const std::vector<QueryProfile>& profiles);
    void suggestOptimizations(string& suggestions);
}

} // namespace ScratchBird

#endif // RTREE_ALGORITHMS_H