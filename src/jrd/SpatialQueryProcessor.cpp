#include "SpatialQueryProcessor.h"
#include "SpatialIndex.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <sstream>
#include <iomanip>

using namespace ScratchBird;

//============================================================================
// SpatialQueryContext Implementation
//============================================================================

SpatialQueryContext::SpatialQueryContext()
    : operation(SpatialOperation::ST_INTERSECTS), strategy(QueryStrategy::INDEX_FILTER),
      resultFormat(ResultFormat::GEOMETRY), queryGeometry(nullptr), bufferDistance(0.0),
      tolerance(0.0), k(0), sourceSRID(0), targetSRID(0), sourceSRS(nullptr), 
      targetSRS(nullptr), useApproximation(false), enableCaching(true),
      selectivityThreshold(0.1), maxResults(0), estimatedCost(0.0), 
      actualCost(0.0), pagesAccessed(0), geometriesProcessed(0)
{
}

SpatialQueryContext::~SpatialQueryContext()
{
    if (queryGeometry) {
        delete queryGeometry;
        queryGeometry = nullptr;
    }
}

void SpatialQueryContext::reset()
{
    if (queryGeometry) {
        delete queryGeometry;
        queryGeometry = nullptr;
    }
    
    operation = SpatialOperation::ST_INTERSECTS;
    strategy = QueryStrategy::INDEX_FILTER;
    resultFormat = ResultFormat::GEOMETRY;
    queryMBR.reset();
    bufferDistance = 0.0;
    tolerance = 0.0;
    k = 0;
    sourceSRID = 0;
    targetSRID = 0;
    sourceSRS = nullptr;
    targetSRS = nullptr;
    useApproximation = false;
    enableCaching = true;
    selectivityThreshold = 0.1;
    maxResults = 0;
    estimatedCost = 0.0;
    actualCost = 0.0;
    pagesAccessed = 0;
    geometriesProcessed = 0;
}

bool SpatialQueryContext::isValid() const
{
    return queryGeometry != nullptr || queryMBR.isValid();
}

string SpatialQueryContext::toString() const
{
    std::ostringstream oss;
    oss << "SpatialQuery{op=" << static_cast<int>(operation)
        << ", strategy=" << static_cast<int>(strategy)
        << ", format=" << static_cast<int>(resultFormat);
    
    if (queryMBR.isValid()) {
        oss << ", mbr=" << queryMBR.toString();
    }
    
    if (bufferDistance > 0.0) {
        oss << ", buffer=" << bufferDistance;
    }
    
    if (k > 0) {
        oss << ", k=" << k;
    }
    
    oss << "}";
    return oss.str();
}

//============================================================================
// SpatialQueryResult Implementation
//============================================================================

SpatialQueryResult::SpatialQueryResult()
    : recordNumber(0), geometry(nullptr), distance(0.0), measure(0.0),
      isApproximate(false), confidence(1.0)
{
}

SpatialQueryResult::SpatialQueryResult(Jrd::RecordNumber record, const ExtendedMBR& boundingBox)
    : recordNumber(record), mbr(boundingBox), geometry(nullptr), distance(0.0),
      measure(0.0), isApproximate(false), confidence(1.0)
{
}

SpatialQueryResult::SpatialQueryResult(Jrd::RecordNumber record, Geometry* geom)
    : recordNumber(record), geometry(geom), distance(0.0), measure(0.0),
      isApproximate(false), confidence(1.0)
{
    if (geometry) {
        mbr = geometry->getMBR();
    }
}

SpatialQueryResult::~SpatialQueryResult()
{
    cleanup();
}

SpatialQueryResult::SpatialQueryResult(const SpatialQueryResult& other)
    : recordNumber(other.recordNumber), mbr(other.mbr), geometry(nullptr),
      distance(other.distance), measure(other.measure), isApproximate(other.isApproximate),
      confidence(other.confidence), errorMessage(other.errorMessage)
{
    if (other.geometry) {
        geometry = other.geometry->clone(MemoryPool::getContextPool());
    }
}

SpatialQueryResult& SpatialQueryResult::operator=(const SpatialQueryResult& other)
{
    if (this != &other) {
        cleanup();
        
        recordNumber = other.recordNumber;
        mbr = other.mbr;
        distance = other.distance;
        measure = other.measure;
        isApproximate = other.isApproximate;
        confidence = other.confidence;
        errorMessage = other.errorMessage;
        
        if (other.geometry) {
            geometry = other.geometry->clone(MemoryPool::getContextPool());
        }
    }
    return *this;
}

bool SpatialQueryResult::isValid() const
{
    return recordNumber > 0 && (geometry != nullptr || mbr.isValid());
}

void SpatialQueryResult::cleanup()
{
    if (geometry) {
        delete geometry;
        geometry = nullptr;
    }
}

//============================================================================
// SpatialJoinContext Implementation
//============================================================================

SpatialJoinContext::SpatialJoinContext()
    : leftIndex(nullptr), rightIndex(nullptr), joinOperation(SpatialOperation::ST_INTERSECTS),
      joinDistance(0.0), innerJoin(true), useNestedLoop(false), maxJoinResults(0)
{
}

bool SpatialJoinContext::isValid() const
{
    return leftIndex != nullptr && rightIndex != nullptr;
}

//============================================================================
// SpatialQueryProcessor Implementation
//============================================================================

SpatialQueryProcessor::SpatialQueryProcessor(MemoryPool& p)
    : pool(p), defaultSRS(nullptr), queryCache(p), cacheHits(0), cacheMisses(0),
      totalQueries(0), totalExecutionTime(0.0), totalGeometriesProcessed(0),
      indexSelectivityThreshold(0.1), maxCacheSize(1000), geometricTolerancePPM(1.0)
{
}

SpatialQueryProcessor::~SpatialQueryProcessor()
{
    clearQueryCache();
}

bool SpatialQueryProcessor::executeQuery(Jrd::thread_db* tdbb, SpatialIndex& spatialIndex,
                                        const SpatialQueryContext& context,
                                        std::vector<SpatialQueryResult>& results)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    totalQueries++;
    
    try {
        // Validate query context
        if (!context.isValid()) {
            return false;
        }
        
        // Check cache first if enabled
        if (context.enableCaching) {
            string cacheKey = generateCacheKey(context);
            if (getCachedResults(cacheKey, results)) {
                cacheHits++;
                return true;
            }
            cacheMisses++;
        }
        
        // Execute query based on strategy
        bool success = false;
        switch (context.strategy) {
            case QueryStrategy::INDEX_ONLY:
                success = executeIndexQuery(tdbb, spatialIndex, context, results);
                break;
                
            case QueryStrategy::INDEX_FILTER:
                success = executeFilterQuery(tdbb, spatialIndex, context, results);
                break;
                
            case QueryStrategy::FULL_SCAN:
                success = executeFullScanQuery(tdbb, spatialIndex, context, results);
                break;
                
            case QueryStrategy::HYBRID:
                // Try index first, fall back to full scan if needed
                success = executeFilterQuery(tdbb, spatialIndex, context, results);
                if (!success) {
                    success = executeFullScanQuery(tdbb, spatialIndex, context, results);
                }
                break;
                
            case QueryStrategy::PARALLEL:
                // For now, fall back to INDEX_FILTER
                success = executeFilterQuery(tdbb, spatialIndex, context, results);
                break;
        }
        
        // Apply result transformations if needed
        if (success && context.targetSRS && context.sourceSRS) {
            transformResults(results, *context.sourceSRS, *context.targetSRS);
        }
        
        // Limit results if specified
        if (success && context.maxResults > 0) {
            limitResults(results, context.maxResults);
        }
        
        // Cache results if enabled
        if (success && context.enableCaching) {
            string cacheKey = generateCacheKey(context);
            cacheResults(cacheKey, results);
        }
        
        // Record performance statistics
        auto endTime = std::chrono::high_resolution_clock::now();
        double executionTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        totalExecutionTime += executionTime;
        
        if (success) {
            totalGeometriesProcessed += results.size();
            logQueryExecution(context, executionTime, results.size());
        }
        
        return success;
        
    } catch (...) {
        return false;
    }
}

bool SpatialQueryProcessor::executeSpatialJoin(Jrd::thread_db* tdbb, const SpatialJoinContext& joinContext,
                                              std::vector<std::pair<SpatialQueryResult, SpatialQueryResult>>& results)
{
    try {
        if (!joinContext.isValid()) {
            return false;
        }
        
        // For now, implement a simple nested loop join
        // More sophisticated join algorithms (sort-merge, hash join) could be added
        
        SpatialQueryContext leftContext;
        leftContext.operation = SpatialOperation::ST_INTERSECTS;
        leftContext.strategy = QueryStrategy::INDEX_FILTER;
        leftContext.resultFormat = ResultFormat::GEOMETRY;
        
        // Get all geometries from left index
        std::vector<SpatialQueryResult> leftResults;
        ExtendedMBR worldBounds(-180.0, -90.0, 180.0, 90.0);
        leftContext.queryMBR = worldBounds;
        
        if (!executeQuery(tdbb, *joinContext.leftIndex, leftContext, leftResults)) {
            return false;
        }
        
        // For each left geometry, find matching right geometries
        for (const SpatialQueryResult& leftResult : leftResults) {
            if (!leftResult.geometry) continue;
            
            SpatialQueryContext rightContext;
            rightContext.operation = joinContext.joinOperation;
            rightContext.strategy = QueryStrategy::INDEX_FILTER;
            rightContext.resultFormat = ResultFormat::GEOMETRY;
            rightContext.queryGeometry = leftResult.geometry->clone(pool);
            rightContext.queryMBR = leftResult.mbr;
            rightContext.bufferDistance = joinContext.joinDistance;
            
            std::vector<SpatialQueryResult> rightResults;
            if (executeQuery(tdbb, *joinContext.rightIndex, rightContext, rightResults)) {
                for (const SpatialQueryResult& rightResult : rightResults) {
                    results.emplace_back(leftResult, rightResult);
                    
                    // Check result limit
                    if (joinContext.maxJoinResults > 0 && 
                        results.size() >= joinContext.maxJoinResults) {
                        return true;
                    }
                }
            }
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

//============================================================================
// Spatial Predicate Operations
//============================================================================

bool SpatialQueryProcessor::processIntersects(Jrd::thread_db* tdbb, SpatialIndex& index,
                                             const Geometry& queryGeom, std::vector<SpatialQueryResult>& results)
{
    SpatialQueryContext context;
    context.operation = SpatialOperation::ST_INTERSECTS;
    context.strategy = QueryStrategy::INDEX_FILTER;
    context.queryGeometry = queryGeom.clone(pool);
    context.queryMBR = queryGeom.getMBR();
    
    return executeQuery(tdbb, index, context, results);
}

bool SpatialQueryProcessor::processContains(Jrd::thread_db* tdbb, SpatialIndex& index,
                                           const Geometry& queryGeom, std::vector<SpatialQueryResult>& results)
{
    SpatialQueryContext context;
    context.operation = SpatialOperation::ST_CONTAINS;
    context.strategy = QueryStrategy::INDEX_FILTER;
    context.queryGeometry = queryGeom.clone(pool);
    context.queryMBR = queryGeom.getMBR();
    
    return executeQuery(tdbb, index, context, results);
}

bool SpatialQueryProcessor::processWithin(Jrd::thread_db* tdbb, SpatialIndex& index,
                                         const Geometry& queryGeom, std::vector<SpatialQueryResult>& results)
{
    SpatialQueryContext context;
    context.operation = SpatialOperation::ST_WITHIN;
    context.strategy = QueryStrategy::INDEX_FILTER;
    context.queryGeometry = queryGeom.clone(pool);
    context.queryMBR = queryGeom.getMBR();
    
    return executeQuery(tdbb, index, context, results);
}

bool SpatialQueryProcessor::processDWithin(Jrd::thread_db* tdbb, SpatialIndex& index,
                                          const Geometry& queryGeom, double maxDistance,
                                          std::vector<SpatialQueryResult>& results)
{
    SpatialQueryContext context;
    context.operation = SpatialOperation::ST_DWITHIN;
    context.strategy = QueryStrategy::INDEX_FILTER;
    context.queryGeometry = queryGeom.clone(pool);
    context.queryMBR = queryGeom.getMBR().getExpanded(maxDistance);
    context.bufferDistance = maxDistance;
    
    return executeQuery(tdbb, index, context, results);
}

bool SpatialQueryProcessor::processKNN(Jrd::thread_db* tdbb, SpatialIndex& index,
                                      const Geometry& queryGeom, ULONG k, std::vector<SpatialQueryResult>& results)
{
    SpatialQueryContext context;
    context.operation = SpatialOperation::ST_KNN;
    context.strategy = QueryStrategy::INDEX_ONLY;
    context.queryGeometry = queryGeom.clone(pool);
    context.queryMBR = queryGeom.getMBR();
    context.k = k;
    context.maxResults = k;
    
    return executeQuery(tdbb, index, context, results);
}

//============================================================================
// Query Execution Strategies
//============================================================================

bool SpatialQueryProcessor::executeIndexQuery(Jrd::thread_db* tdbb, SpatialIndex& index,
                                             const SpatialQueryContext& context, 
                                             std::vector<SpatialQueryResult>& results)
{
    try {
        // Convert spatial operation to spatial query type
        SpatialQueryType queryType = SPATIAL_QUERY_INTERSECTS;
        switch (context.operation) {
            case SpatialOperation::ST_INTERSECTS:
                queryType = SPATIAL_QUERY_INTERSECTS;
                break;
            case SpatialOperation::ST_CONTAINS:
                queryType = SPATIAL_QUERY_CONTAINS;
                break;
            case SpatialOperation::ST_WITHIN:
                queryType = SPATIAL_QUERY_CONTAINED_BY;
                break;
            case SpatialOperation::ST_DWITHIN:
                queryType = SPATIAL_QUERY_WITHIN_DISTANCE;
                break;
            case SpatialOperation::ST_KNN:
                queryType = SPATIAL_QUERY_KNN;
                break;
            default:
                queryType = SPATIAL_QUERY_INTERSECTS;
                break;
        }
        
        // Create spatial retrieval context
        SpatialRetrieval retrieval(queryType, context.queryMBR, pool);
        if (context.queryGeometry) {
            retrieval.setQueryGeometry(*context.queryGeometry);
        }
        retrieval.setMaxDistance(context.bufferDistance);
        retrieval.setK(context.k);
        
        // Execute spatial lookup
        bool success = index.spatialLookup(tdbb, queryType, context.queryMBR, &retrieval);
        
        if (success) {
            // Convert retrieval results to query results
            Jrd::RecordNumber recordNum;
            ExtendedMBR mbr;
            
            while (retrieval.getNextResult(recordNum, mbr)) {
                SpatialQueryResult result(recordNum, mbr);
                results.push_back(result);
            }
        }
        
        return success;
        
    } catch (...) {
        return false;
    }
}

bool SpatialQueryProcessor::executeFilterQuery(Jrd::thread_db* tdbb, SpatialIndex& index,
                                              const SpatialQueryContext& context,
                                              std::vector<SpatialQueryResult>& results)
{
    try {
        // First, get candidates using index
        std::vector<SpatialQueryResult> candidates;
        if (!executeIndexQuery(tdbb, index, context, candidates)) {
            return false;
        }
        
        // Apply geometric filter to candidates
        for (const SpatialQueryResult& candidate : candidates) {
            bool passesFilter = true;
            
            if (context.queryGeometry && candidate.geometry) {
                passesFilter = applyGeometricFilter(*context.queryGeometry, 
                                                  *candidate.geometry, 
                                                  context.operation, 
                                                  context.tolerance);
            } else {
                // Fall back to MBR-based filtering
                passesFilter = applyMBRFilter(context.queryMBR, candidate.mbr, 
                                            context.operation, context.bufferDistance);
            }
            
            if (passesFilter) {
                results.push_back(candidate);
            }
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

bool SpatialQueryProcessor::executeFullScanQuery(Jrd::thread_db* tdbb, SpatialIndex& index,
                                                const SpatialQueryContext& context,
                                                std::vector<SpatialQueryResult>& results)
{
    // Full scan implementation would require iteration over all records
    // This is a placeholder implementation
    return executeFilterQuery(tdbb, index, context, results);
}

//============================================================================
// Geometric Filtering
//============================================================================

bool SpatialQueryProcessor::applyGeometricFilter(const Geometry& queryGeom, const Geometry& candidateGeom,
                                                SpatialOperation operation, double tolerance)
{
    try {
        switch (operation) {
            case SpatialOperation::ST_INTERSECTS:
                return queryGeom.intersects(candidateGeom);
                
            case SpatialOperation::ST_CONTAINS:
                return queryGeom.contains(candidateGeom);
                
            case SpatialOperation::ST_WITHIN:
                return candidateGeom.contains(queryGeom);
                
            case SpatialOperation::ST_TOUCHES:
                return queryGeom.touches(candidateGeom);
                
            case SpatialOperation::ST_CROSSES:
                return queryGeom.crosses(candidateGeom);
                
            case SpatialOperation::ST_OVERLAPS:
                return queryGeom.overlaps(candidateGeom);
                
            case SpatialOperation::ST_EQUALS:
                return queryGeom.equals(candidateGeom);
                
            case SpatialOperation::ST_DISJOINT:
                return queryGeom.disjoint(candidateGeom);
                
            case SpatialOperation::ST_DWITHIN:
                return queryGeom.distance(candidateGeom) <= tolerance;
                
            default:
                return queryGeom.intersects(candidateGeom);
        }
        
    } catch (...) {
        return false;
    }
}

bool SpatialQueryProcessor::applyMBRFilter(const ExtendedMBR& queryMBR, const ExtendedMBR& candidateMBR,
                                          SpatialOperation operation, double bufferDistance)
{
    try {
        ExtendedMBR bufferedQueryMBR = queryMBR;
        if (bufferDistance > 0.0) {
            bufferedQueryMBR.expandByDistance(bufferDistance);
        }
        
        switch (operation) {
            case SpatialOperation::ST_INTERSECTS:
            case SpatialOperation::ST_TOUCHES:
            case SpatialOperation::ST_CROSSES:
            case SpatialOperation::ST_OVERLAPS:
                return bufferedQueryMBR.intersects(candidateMBR);
                
            case SpatialOperation::ST_CONTAINS:
                return bufferedQueryMBR.contains(candidateMBR);
                
            case SpatialOperation::ST_WITHIN:
                return candidateMBR.contains(bufferedQueryMBR);
                
            case SpatialOperation::ST_EQUALS:
                return bufferedQueryMBR.equals(candidateMBR);
                
            case SpatialOperation::ST_DISJOINT:
                return !bufferedQueryMBR.intersects(candidateMBR);
                
            case SpatialOperation::ST_DWITHIN:
                return bufferedQueryMBR.intersects(candidateMBR);
                
            default:
                return bufferedQueryMBR.intersects(candidateMBR);
        }
        
    } catch (...) {
        return false;
    }
}

//============================================================================
// Performance and Optimization Methods
//============================================================================

void SpatialQueryProcessor::optimizeQuery(SpatialQueryContext& context, SpatialIndex& index)
{
    // Choose optimal strategy based on query characteristics
    context.strategy = chooseStrategy(context, index);
    
    // Adjust selectivity threshold if needed
    double estimatedSelectivity = estimateSelectivity(context, index);
    if (estimatedSelectivity > indexSelectivityThreshold) {
        context.strategy = QueryStrategy::FULL_SCAN;
    }
    
    // Enable approximation for large result sets
    if (estimateResultSize(context, index) > 10000) {
        context.useApproximation = true;
    }
}

double SpatialQueryProcessor::estimateQueryCost(const SpatialQueryContext& context, SpatialIndex& index)
{
    // Simple cost estimation based on query MBR area and index statistics
    double queryArea = context.queryMBR.area();
    ExtendedMBR indexBounds = index.getBoundingBox(nullptr);
    double indexArea = indexBounds.area();
    
    if (indexArea == 0.0) return 1000.0; // High cost for invalid index
    
    double selectivity = queryArea / indexArea;
    selectivity = std::max(0.0001, std::min(1.0, selectivity));
    
    // Base cost depends on operation complexity
    double baseCost = 1.0;
    switch (context.operation) {
        case SpatialOperation::ST_INTERSECTS:
            baseCost = 1.0;
            break;
        case SpatialOperation::ST_CONTAINS:
        case SpatialOperation::ST_WITHIN:
            baseCost = 2.0;
            break;
        case SpatialOperation::ST_DISTANCE:
        case SpatialOperation::ST_DWITHIN:
            baseCost = 3.0;
            break;
        case SpatialOperation::ST_KNN:
            baseCost = 5.0;
            break;
        default:
            baseCost = 2.0;
            break;
    }
    
    ULONG estimatedGeometries = index.getGeometryCount(nullptr);
    return baseCost * selectivity * estimatedGeometries;
}

QueryStrategy SpatialQueryProcessor::chooseStrategy(const SpatialQueryContext& context, SpatialIndex& index)
{
    double estimatedCost = estimateQueryCost(context, index);
    double selectivity = estimateSelectivity(context, index);
    
    // High selectivity queries benefit from index-only access
    if (selectivity < 0.01) {
        return QueryStrategy::INDEX_ONLY;
    }
    
    // Medium selectivity queries benefit from filtering
    if (selectivity < indexSelectivityThreshold) {
        return QueryStrategy::INDEX_FILTER;
    }
    
    // Low selectivity queries may be better with full scan
    if (selectivity > 0.5) {
        return QueryStrategy::FULL_SCAN;
    }
    
    return QueryStrategy::INDEX_FILTER; // Default
}

double SpatialQueryProcessor::estimateSelectivity(const SpatialQueryContext& context, SpatialIndex& index)
{
    ExtendedMBR indexBounds = index.getBoundingBox(nullptr);
    double indexArea = indexBounds.area();
    
    if (indexArea == 0.0) return 1.0;
    
    double queryArea = context.queryMBR.area();
    if (context.bufferDistance > 0.0) {
        ExtendedMBR bufferedMBR = context.queryMBR.getExpanded(context.bufferDistance);
        queryArea = bufferedMBR.area();
    }
    
    return std::min(1.0, queryArea / indexArea);
}

//============================================================================
// Cache Management
//============================================================================

void SpatialQueryProcessor::enableQueryCache(ULONG maxSize)
{
    maxCacheSize = maxSize;
    if (queryCache.count() > maxCacheSize) {
        clearQueryCache();
    }
}

void SpatialQueryProcessor::disableQueryCache()
{
    maxCacheSize = 0;
    clearQueryCache();
}

void SpatialQueryProcessor::clearQueryCache()
{
    queryCache.clear();
    cacheHits = 0;
    cacheMisses = 0;
}

double SpatialQueryProcessor::getCacheHitRate() const
{
    ULONG totalAccesses = cacheHits + cacheMisses;
    return totalAccesses > 0 ? static_cast<double>(cacheHits) / totalAccesses : 0.0;
}

string SpatialQueryProcessor::generateCacheKey(const SpatialQueryContext& context)
{
    std::ostringstream oss;
    oss << static_cast<int>(context.operation) << "|";
    oss << std::fixed << std::setprecision(6);
    oss << context.queryMBR.minX << "," << context.queryMBR.minY << ",";
    oss << context.queryMBR.maxX << "," << context.queryMBR.maxY << "|";
    oss << context.bufferDistance << "|" << context.k;
    return oss.str();
}

bool SpatialQueryProcessor::getCachedResults(const string& cacheKey, std::vector<SpatialQueryResult>& results)
{
    if (maxCacheSize == 0) return false;
    
    try {
        FB_SIZE_T pos;
        if (queryCache.get(cacheKey, pos)) {
            results = queryCache.valueAt(pos);
            return true;
        }
    } catch (...) {
        // Cache miss or error
    }
    
    return false;
}

void SpatialQueryProcessor::cacheResults(const string& cacheKey, const std::vector<SpatialQueryResult>& results)
{
    if (maxCacheSize == 0) return;
    
    try {
        // Evict oldest entry if cache is full
        if (queryCache.count() >= maxCacheSize) {
            evictOldestCacheEntry();
        }
        
        queryCache.put(cacheKey, results);
    } catch (...) {
        // Cache storage error - ignore
    }
}

void SpatialQueryProcessor::evictOldestCacheEntry()
{
    if (queryCache.count() > 0) {
        // Simple eviction - remove first entry
        // More sophisticated LRU could be implemented
        queryCache.remove(0);
    }
}

//============================================================================
// Performance Monitoring
//============================================================================

SpatialQueryProcessor::QueryPerformanceStats SpatialQueryProcessor::getPerformanceStats() const
{
    QueryPerformanceStats stats;
    stats.totalQueries = totalQueries;
    stats.totalExecutionTime = totalExecutionTime;
    stats.averageExecutionTime = totalQueries > 0 ? totalExecutionTime / totalQueries : 0.0;
    stats.totalGeometriesProcessed = totalGeometriesProcessed;
    stats.averageGeometriesPerQuery = totalQueries > 0 ? 
        static_cast<double>(totalGeometriesProcessed) / totalQueries : 0.0;
    stats.cacheHitRate = getCacheHitRate();
    stats.slowQueries = 0; // Would need to track this
    stats.mostExpensiveOperation = "ST_INTERSECTS"; // Placeholder
    stats.maxExecutionTime = 0.0; // Would need to track this
    
    return stats;
}

void SpatialQueryProcessor::resetPerformanceStats()
{
    totalQueries = 0;
    totalExecutionTime = 0.0;
    totalGeometriesProcessed = 0;
    cacheHits = 0;
    cacheMisses = 0;
}

string SpatialQueryProcessor::generatePerformanceReport() const
{
    QueryPerformanceStats stats = getPerformanceStats();
    
    std::ostringstream report;
    report << "Spatial Query Processor Performance Report\n";
    report << "==========================================\n\n";
    report << "Total Queries: " << stats.totalQueries << "\n";
    report << "Total Execution Time: " << std::fixed << std::setprecision(2) 
           << stats.totalExecutionTime << " ms\n";
    report << "Average Execution Time: " << stats.averageExecutionTime << " ms\n";
    report << "Total Geometries Processed: " << stats.totalGeometriesProcessed << "\n";
    report << "Average Geometries per Query: " << std::setprecision(1) 
           << stats.averageGeometriesPerQuery << "\n";
    report << "Cache Hit Rate: " << std::setprecision(1) 
           << (stats.cacheHitRate * 100) << "%\n";
    
    return report.str();
}

void SpatialQueryProcessor::logQueryExecution(const SpatialQueryContext& context, 
                                             double executionTime, ULONG resultCount)
{
    // Log query execution for monitoring and debugging
    // This would integrate with ScratchBird's logging system
    updateOperationStats(context.operation, executionTime);
}

void SpatialQueryProcessor::updateOperationStats(SpatialOperation operation, double executionTime)
{
    // Update per-operation statistics
    // This would maintain detailed statistics per operation type
}

//============================================================================
// SpatialQueryBuilder Implementation
//============================================================================

SpatialQueryBuilder::SpatialQueryBuilder(MemoryPool& p) : pool(p)
{
}

SpatialQueryBuilder::~SpatialQueryBuilder()
{
}

SpatialQueryBuilder& SpatialQueryBuilder::operation(SpatialOperation op)
{
    context.operation = op;
    return *this;
}

SpatialQueryBuilder& SpatialQueryBuilder::intersects(const Geometry& geom)
{
    context.operation = SpatialOperation::ST_INTERSECTS;
    context.queryGeometry = geom.clone(pool);
    context.queryMBR = geom.getMBR();
    return *this;
}

SpatialQueryBuilder& SpatialQueryBuilder::contains(const Geometry& geom)
{
    context.operation = SpatialOperation::ST_CONTAINS;
    context.queryGeometry = geom.clone(pool);
    context.queryMBR = geom.getMBR();
    return *this;
}

SpatialQueryBuilder& SpatialQueryBuilder::within(const Geometry& geom)
{
    context.operation = SpatialOperation::ST_WITHIN;
    context.queryGeometry = geom.clone(pool);
    context.queryMBR = geom.getMBR();
    return *this;
}

SpatialQueryBuilder& SpatialQueryBuilder::dwithin(const Geometry& geom, double distance)
{
    context.operation = SpatialOperation::ST_DWITHIN;
    context.queryGeometry = geom.clone(pool);
    context.queryMBR = geom.getMBR().getExpanded(distance);
    context.bufferDistance = distance;
    return *this;
}

SpatialQueryBuilder& SpatialQueryBuilder::knn(const Geometry& geom, ULONG k)
{
    context.operation = SpatialOperation::ST_KNN;
    context.queryGeometry = geom.clone(pool);
    context.queryMBR = geom.getMBR();
    context.k = k;
    context.maxResults = k;
    return *this;
}

SpatialQueryBuilder& SpatialQueryBuilder::tolerance(double tol)
{
    context.tolerance = tol;
    return *this;
}

SpatialQueryBuilder& SpatialQueryBuilder::srid(SRID sourceSRID, SRID targetSRID)
{
    context.sourceSRID = sourceSRID;
    if (targetSRID != 0) {
        context.targetSRID = targetSRID;
    }
    return *this;
}

SpatialQueryBuilder& SpatialQueryBuilder::maxResults(ULONG max)
{
    context.maxResults = max;
    return *this;
}

SpatialQueryBuilder& SpatialQueryBuilder::strategy(QueryStrategy strat)
{
    context.strategy = strat;
    return *this;
}

SpatialQueryBuilder& SpatialQueryBuilder::format(ResultFormat fmt)
{
    context.resultFormat = fmt;
    return *this;
}

SpatialQueryBuilder& SpatialQueryBuilder::useApproximation(bool enable)
{
    context.useApproximation = enable;
    return *this;
}

SpatialQueryBuilder& SpatialQueryBuilder::enableCaching(bool enable)
{
    context.enableCaching = enable;
    return *this;
}

SpatialQueryBuilder& SpatialQueryBuilder::selectivityThreshold(double threshold)
{
    context.selectivityThreshold = threshold;
    return *this;
}

SpatialQueryContext SpatialQueryBuilder::build()
{
    return context;
}

bool SpatialQueryBuilder::execute(Jrd::thread_db* tdbb, SpatialIndex& index, 
                                 SpatialQueryProcessor& processor,
                                 std::vector<SpatialQueryResult>& results)
{
    return processor.executeQuery(tdbb, index, context, results);
}

void SpatialQueryBuilder::reset()
{
    context.reset();
}