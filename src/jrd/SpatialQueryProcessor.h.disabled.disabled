/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		SpatialQueryProcessor.h
 *	DESCRIPTION:	Advanced spatial query processor for geometric operations
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
 * Contributor(s): ________________________________________.
 *
 * 2025.07.24 - ScratchBird Spatial Query Processor Implementation
 */

#ifndef JRD_SPATIAL_QUERY_PROCESSOR_H
#define JRD_SPATIAL_QUERY_PROCESSOR_H

#include "SpatialDataTypes.h"
#include "RTreeAlgorithms.h"
#include "SpatialReferenceSystem.h"
#include "MBROperations.h"
#include "common/classes/array.h"
#include "common/classes/GenericMap.h"
#include <vector>
#include <map>
#include <memory>

namespace ScratchBird {

// Forward declarations
class SpatialIndex;
class RTreeIndex;

//----------------------------
// Spatial Query Types and Parameters
//----------------------------

// Extended spatial query types for complex operations
enum class SpatialOperation : UCHAR
{
    // Basic spatial predicates
    ST_INTERSECTS = 1,
    ST_CONTAINS = 2,
    ST_WITHIN = 3,
    ST_TOUCHES = 4,
    ST_CROSSES = 5,
    ST_OVERLAPS = 6,
    ST_EQUALS = 7,
    ST_DISJOINT = 8,
    
    // Distance operations
    ST_DISTANCE = 10,
    ST_DISTANCE_SPHERE = 11,
    ST_DISTANCE_SPHEROID = 12,
    ST_DWITHIN = 13,
    ST_DFULLYWITHIN = 14,
    
    // Nearest neighbor
    ST_KNN = 20,
    ST_NEAREST = 21,
    
    // Spatial analytics
    ST_AREA = 30,
    ST_LENGTH = 31,
    ST_PERIMETER = 32,
    ST_CENTROID = 33,
    ST_ENVELOPE = 34,
    ST_CONVEX_HULL = 35,
    ST_BOUNDARY = 36,
    
    // Geometric operations
    ST_INTERSECTION = 40,
    ST_UNION = 41,
    ST_DIFFERENCE = 42,
    ST_SYMDIFFERENCE = 43,
    ST_BUFFER = 44,
    ST_SIMPLIFY = 45,
    
    // Topological operations
    ST_IS_SIMPLE = 50,
    ST_IS_VALID = 51,
    ST_IS_EMPTY = 52,
    ST_IS_CLOSED = 53,
    ST_IS_RING = 54,
    
    // Coordinate system transformations
    ST_TRANSFORM = 60,
    ST_SETSRID = 61
};

// Query execution strategy
enum class QueryStrategy : UCHAR
{
    INDEX_ONLY = 1,        // Use only spatial index
    INDEX_FILTER = 2,      // Index + geometric filter
    FULL_SCAN = 3,         // Full table scan with geometric filter
    HYBRID = 4,            // Combination of strategies
    PARALLEL = 5           // Parallel execution
};

// Query result format
enum class ResultFormat : UCHAR
{
    GEOMETRY = 1,          // Return full geometry objects
    MBR = 2,               // Return only MBRs
    RECORD_NUMBERS = 3,    // Return only record numbers
    SUMMARY = 4,           // Return summary statistics
    SPATIAL_JOIN = 5       // Return joined results
};

// Spatial query context
struct SpatialQueryContext
{
    SpatialOperation operation;
    QueryStrategy strategy;
    ResultFormat resultFormat;
    
    // Query geometry parameters
    Geometry* queryGeometry;
    ExtendedMBR queryMBR;
    double bufferDistance;
    double tolerance;
    ULONG k; // For KNN queries
    
    // Coordinate system
    SRID sourceSRID;
    SRID targetSRID;
    SpatialReferenceSystem* sourceSRS;
    SpatialReferenceSystem* targetSRS;
    
    // Query optimization
    bool useApproximation;
    bool enableCaching;
    double selectivityThreshold;
    ULONG maxResults;
    
    // Performance monitoring
    mutable double estimatedCost;
    mutable double actualCost;
    mutable ULONG pagesAccessed;
    mutable ULONG geometriesProcessed;
    
    SpatialQueryContext();
    ~SpatialQueryContext();
    
    void reset();
    bool isValid() const;
    string toString() const;
};

// Spatial query result
struct SpatialQueryResult
{
    Jrd::RecordNumber recordNumber;
    ExtendedMBR mbr;
    Geometry* geometry;
    double distance;
    double measure; // Generic measurement (area, length, etc.)
    
    // Result metadata
    bool isApproximate;
    double confidence;
    string errorMessage;
    
    SpatialQueryResult();
    SpatialQueryResult(Jrd::RecordNumber record, const ExtendedMBR& boundingBox);
    SpatialQueryResult(Jrd::RecordNumber record, Geometry* geom);
    ~SpatialQueryResult();
    
    SpatialQueryResult(const SpatialQueryResult& other);
    SpatialQueryResult& operator=(const SpatialQueryResult& other);
    
    bool isValid() const;
    void cleanup();
};

// Spatial join parameters
struct SpatialJoinContext
{
    SpatialIndex* leftIndex;
    SpatialIndex* rightIndex;
    SpatialOperation joinOperation;
    double joinDistance;
    bool innerJoin;
    bool useNestedLoop;
    ULONG maxJoinResults;
    
    SpatialJoinContext();
    bool isValid() const;
};

//----------------------------
// Advanced Spatial Query Processor
//----------------------------
class SpatialQueryProcessor
{
private:
    MemoryPool& pool;
    SpatialReferenceSystem* defaultSRS;
    
    // Query cache for frequently used queries
    mutable GenericMap<Pair<NonPooled<string, std::vector<SpatialQueryResult>>>> queryCache;
    mutable ULONG cacheHits;
    mutable ULONG cacheMisses;
    
    // Performance monitoring
    mutable ULONG64 totalQueries;
    mutable double totalExecutionTime;
    mutable ULONG64 totalGeometriesProcessed;
    
    // Query optimization settings
    double indexSelectivityThreshold;
    ULONG maxCacheSize;
    double geometricTolerancePPM;
    
public:
    SpatialQueryProcessor(MemoryPool& p);
    ~SpatialQueryProcessor();
    
    //----------------------------
    // Primary Query Interface
    //----------------------------
    
    /**
     * Execute a spatial query against a spatial index
     */
    bool executeQuery(Jrd::thread_db* tdbb, SpatialIndex& spatialIndex, 
                     const SpatialQueryContext& context,
                     std::vector<SpatialQueryResult>& results);
    
    /**
     * Execute a spatial join between two spatial indexes
     */
    bool executeSpatialJoin(Jrd::thread_db* tdbb, const SpatialJoinContext& joinContext,
                          std::vector<std::pair<SpatialQueryResult, SpatialQueryResult>>& results);
    
    //----------------------------
    // Spatial Predicate Operations
    //----------------------------
    
    // Basic spatial predicates
    bool processIntersects(Jrd::thread_db* tdbb, SpatialIndex& index, 
                         const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    bool processContains(Jrd::thread_db* tdbb, SpatialIndex& index,
                       const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    bool processWithin(Jrd::thread_db* tdbb, SpatialIndex& index,
                     const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    bool processTouches(Jrd::thread_db* tdbb, SpatialIndex& index,
                      const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    bool processCrosses(Jrd::thread_db* tdbb, SpatialIndex& index,
                      const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    bool processOverlaps(Jrd::thread_db* tdbb, SpatialIndex& index,
                       const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    bool processEquals(Jrd::thread_db* tdbb, SpatialIndex& index,
                     const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    bool processDisjoint(Jrd::thread_db* tdbb, SpatialIndex& index,
                       const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    //----------------------------
    // Distance Operations
    //----------------------------
    
    // Distance calculations
    bool processDistance(Jrd::thread_db* tdbb, SpatialIndex& index,
                       const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    bool processDistanceSphere(Jrd::thread_db* tdbb, SpatialIndex& index,
                             const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    bool processDistanceSpheroid(Jrd::thread_db* tdbb, SpatialIndex& index,
                               const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    // Distance-based queries
    bool processDWithin(Jrd::thread_db* tdbb, SpatialIndex& index,
                      const Geometry& queryGeom, double maxDistance, 
                      std::vector<SpatialQueryResult>& results);
    
    bool processDFullyWithin(Jrd::thread_db* tdbb, SpatialIndex& index,
                           const Geometry& queryGeom, double maxDistance,
                           std::vector<SpatialQueryResult>& results);
    
    //----------------------------
    // Nearest Neighbor Operations
    //----------------------------
    
    // K-Nearest Neighbors
    bool processKNN(Jrd::thread_db* tdbb, SpatialIndex& index,
                  const Geometry& queryGeom, ULONG k, std::vector<SpatialQueryResult>& results);
    
    bool processNearest(Jrd::thread_db* tdbb, SpatialIndex& index,
                      const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    //----------------------------
    // Spatial Analytics
    //----------------------------
    
    // Geometric measurements
    bool processArea(Jrd::thread_db* tdbb, SpatialIndex& index,
                   const std::vector<Jrd::RecordNumber>& records, std::vector<SpatialQueryResult>& results);
    
    bool processLength(Jrd::thread_db* tdbb, SpatialIndex& index,
                     const std::vector<Jrd::RecordNumber>& records, std::vector<SpatialQueryResult>& results);
    
    bool processPerimeter(Jrd::thread_db* tdbb, SpatialIndex& index,
                        const std::vector<Jrd::RecordNumber>& records, std::vector<SpatialQueryResult>& results);
    
    // Geometric properties
    bool processCentroid(Jrd::thread_db* tdbb, SpatialIndex& index,
                       const std::vector<Jrd::RecordNumber>& records, std::vector<SpatialQueryResult>& results);
    
    bool processEnvelope(Jrd::thread_db* tdbb, SpatialIndex& index,
                       const std::vector<Jrd::RecordNumber>& records, std::vector<SpatialQueryResult>& results);
    
    bool processConvexHull(Jrd::thread_db* tdbb, SpatialIndex& index,
                         const std::vector<Jrd::RecordNumber>& records, std::vector<SpatialQueryResult>& results);
    
    bool processBoundary(Jrd::thread_db* tdbb, SpatialIndex& index,
                       const std::vector<Jrd::RecordNumber>& records, std::vector<SpatialQueryResult>& results);
    
    //----------------------------
    // Geometric Operations
    //----------------------------
    
    // Set operations
    bool processIntersection(Jrd::thread_db* tdbb, SpatialIndex& index,
                           const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    bool processUnion(Jrd::thread_db* tdbb, SpatialIndex& index,
                    const std::vector<Jrd::RecordNumber>& records, std::vector<SpatialQueryResult>& results);
    
    bool processDifference(Jrd::thread_db* tdbb, SpatialIndex& index,
                         const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    bool processSymDifference(Jrd::thread_db* tdbb, SpatialIndex& index,
                            const Geometry& queryGeom, std::vector<SpatialQueryResult>& results);
    
    // Geometric transformations
    bool processBuffer(Jrd::thread_db* tdbb, SpatialIndex& index,
                     const std::vector<Jrd::RecordNumber>& records, double bufferDistance,
                     std::vector<SpatialQueryResult>& results);
    
    bool processSimplify(Jrd::thread_db* tdbb, SpatialIndex& index,
                       const std::vector<Jrd::RecordNumber>& records, double tolerance,
                       std::vector<SpatialQueryResult>& results);
    
    //----------------------------
    // Topological Validation
    //----------------------------
    
    bool processIsSimple(Jrd::thread_db* tdbb, SpatialIndex& index,
                       const std::vector<Jrd::RecordNumber>& records, std::vector<SpatialQueryResult>& results);
    
    bool processIsValid(Jrd::thread_db* tdbb, SpatialIndex& index,
                      const std::vector<Jrd::RecordNumber>& records, std::vector<SpatialQueryResult>& results);
    
    bool processIsEmpty(Jrd::thread_db* tdbb, SpatialIndex& index,
                      const std::vector<Jrd::RecordNumber>& records, std::vector<SpatialQueryResult>& results);
    
    bool processIsClosed(Jrd::thread_db* tdbb, SpatialIndex& index,
                       const std::vector<Jrd::RecordNumber>& records, std::vector<SpatialQueryResult>& results);
    
    bool processIsRing(Jrd::thread_db* tdbb, SpatialIndex& index,
                     const std::vector<Jrd::RecordNumber>& records, std::vector<SpatialQueryResult>& results);
    
    //----------------------------
    // Coordinate System Operations
    //----------------------------
    
    bool processTransform(Jrd::thread_db* tdbb, SpatialIndex& index,
                        const std::vector<Jrd::RecordNumber>& records, SRID targetSRID,
                        std::vector<SpatialQueryResult>& results);
    
    bool processSetSRID(Jrd::thread_db* tdbb, SpatialIndex& index,
                      const std::vector<Jrd::RecordNumber>& records, SRID newSRID,
                      std::vector<SpatialQueryResult>& results);
    
    //----------------------------
    // Query Optimization
    //----------------------------
    
    /**
     * Optimize a spatial query context for best performance
     */
    void optimizeQuery(SpatialQueryContext& context, SpatialIndex& index);
    
    /**
     * Estimate the cost of executing a spatial query
     */
    double estimateQueryCost(const SpatialQueryContext& context, SpatialIndex& index);
    
    /**
     * Choose the best execution strategy for a query
     */
    QueryStrategy chooseStrategy(const SpatialQueryContext& context, SpatialIndex& index);
    
    /**
     * Determine if spatial index should be used for a query
     */
    bool shouldUseIndex(const SpatialQueryContext& context, SpatialIndex& index);
    
    //----------------------------
    // Query Caching
    //----------------------------
    
    void enableQueryCache(ULONG maxSize = 1000);
    void disableQueryCache();
    void clearQueryCache();
    double getCacheHitRate() const;
    
    //----------------------------
    // Performance Monitoring
    //----------------------------
    
    struct QueryPerformanceStats
    {
        ULONG64 totalQueries;
        double averageExecutionTime;
        double totalExecutionTime;
        ULONG64 totalGeometriesProcessed;
        double averageGeometriesPerQuery;
        double cacheHitRate;
        ULONG slowQueries; // Queries taking > 1 second
        
        string mostExpensiveOperation;
        double maxExecutionTime;
        
        std::map<SpatialOperation, ULONG64> operationCounts;
        std::map<SpatialOperation, double> operationTotalTime;
    };
    
    QueryPerformanceStats getPerformanceStats() const;
    void resetPerformanceStats();
    
    string generatePerformanceReport() const;
    void logQueryExecution(const SpatialQueryContext& context, double executionTime, ULONG resultCount);
    
    //----------------------------
    // Configuration
    //----------------------------
    
    void setIndexSelectivityThreshold(double threshold) { indexSelectivityThreshold = threshold; }
    double getIndexSelectivityThreshold() const { return indexSelectivityThreshold; }
    
    void setGeometricTolerance(double tolerancePPM) { geometricTolerancePPM = tolerancePPM; }
    double getGeometricTolerance() const { return geometricTolerancePPM; }
    
    void setDefaultSRS(SpatialReferenceSystem* srs) { defaultSRS = srs; }
    SpatialReferenceSystem* getDefaultSRS() const { return defaultSRS; }
    
private:
    //----------------------------
    // Internal Helper Methods
    //----------------------------
    
    // Query execution helpers
    bool executeIndexQuery(Jrd::thread_db* tdbb, SpatialIndex& index,
                         const SpatialQueryContext& context, std::vector<SpatialQueryResult>& results);
    
    bool executeFilterQuery(Jrd::thread_db* tdbb, SpatialIndex& index,
                          const SpatialQueryContext& context, std::vector<SpatialQueryResult>& results);
    
    bool executeFullScanQuery(Jrd::thread_db* tdbb, SpatialIndex& index,
                            const SpatialQueryContext& context, std::vector<SpatialQueryResult>& results);
    
    // Geometric filtering
    bool applyGeometricFilter(const Geometry& queryGeom, const Geometry& candidateGeom,
                            SpatialOperation operation, double tolerance = 0.0);
    
    bool applyMBRFilter(const ExtendedMBR& queryMBR, const ExtendedMBR& candidateMBR,
                       SpatialOperation operation, double bufferDistance = 0.0);
    
    // Result processing
    void processRTreeResults(const std::vector<RTreeSearchResult>& rtreeResults,
                           const SpatialQueryContext& context, std::vector<SpatialQueryResult>& results);
    
    void transformResults(std::vector<SpatialQueryResult>& results, 
                         const SpatialReferenceSystem& sourceSRS, const SpatialReferenceSystem& targetSRS);
    
    void sortResultsByDistance(std::vector<SpatialQueryResult>& results, const Geometry& referenceGeom);
    void limitResults(std::vector<SpatialQueryResult>& results, ULONG maxResults);
    
    // Geometric algorithms
    double calculatePreciseDistance(const Geometry& geom1, const Geometry& geom2, 
                                  const SpatialReferenceSystem* srs = nullptr);
    
    double calculateSphericalDistance(const Coordinate& coord1, const Coordinate& coord2);
    double calculateSpheroidalDistance(const Coordinate& coord1, const Coordinate& coord2, 
                                     const SpatialReferenceSystem& srs);
    
    Geometry* calculateIntersectionGeometry(const Geometry& geom1, const Geometry& geom2);
    Geometry* calculateUnionGeometry(const std::vector<Geometry*>& geometries);
    Geometry* calculateBufferGeometry(const Geometry& geometry, double bufferDistance);
    
    // Query optimization helpers
    double estimateSelectivity(const SpatialQueryContext& context, SpatialIndex& index);
    double estimateResultSize(const SpatialQueryContext& context, SpatialIndex& index);
    bool isHighSelectivityQuery(const SpatialQueryContext& context, SpatialIndex& index);
    
    // Cache management
    string generateCacheKey(const SpatialQueryContext& context);
    bool getCachedResults(const string& cacheKey, std::vector<SpatialQueryResult>& results);
    void cacheResults(const string& cacheKey, const std::vector<SpatialQueryResult>& results);
    void evictOldestCacheEntry();
    
    // Error handling
    void handleQueryError(const string& operation, const string& error);
    void validateQueryContext(const SpatialQueryContext& context);
    
    // Performance monitoring helpers
    void recordQueryStart(const SpatialQueryContext& context);
    void recordQueryEnd(const SpatialQueryContext& context, double executionTime, ULONG resultCount);
    void updateOperationStats(SpatialOperation operation, double executionTime);
};

//----------------------------
// Spatial Query Builder - Helper class for constructing complex queries
//----------------------------
class SpatialQueryBuilder
{
private:
    SpatialQueryContext context;
    MemoryPool& pool;
    
public:
    SpatialQueryBuilder(MemoryPool& p);
    ~SpatialQueryBuilder();
    
    // Operation selection
    SpatialQueryBuilder& operation(SpatialOperation op);
    SpatialQueryBuilder& intersects(const Geometry& geom);
    SpatialQueryBuilder& contains(const Geometry& geom);
    SpatialQueryBuilder& within(const Geometry& geom);
    SpatialQueryBuilder& dwithin(const Geometry& geom, double distance);
    SpatialQueryBuilder& knn(const Geometry& geom, ULONG k);
    
    // Query parameters
    SpatialQueryBuilder& tolerance(double tol);
    SpatialQueryBuilder& srid(SRID sourceSRID, SRID targetSRID = 0);
    SpatialQueryBuilder& maxResults(ULONG max);
    SpatialQueryBuilder& strategy(QueryStrategy strat);
    SpatialQueryBuilder& format(ResultFormat fmt);
    
    // Optimization hints
    SpatialQueryBuilder& useApproximation(bool enable = true);
    SpatialQueryBuilder& enableCaching(bool enable = true);
    SpatialQueryBuilder& selectivityThreshold(double threshold);
    
    // Build and execute
    SpatialQueryContext build();
    bool execute(Jrd::thread_db* tdbb, SpatialIndex& index, SpatialQueryProcessor& processor,
               std::vector<SpatialQueryResult>& results);
    
    // Reset for reuse
    void reset();
};

} // namespace ScratchBird

#endif // JRD_SPATIAL_QUERY_PROCESSOR_H