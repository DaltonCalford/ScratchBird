#include "SpatialFunctionPerformance.h"
#include "SpatialDataTypes.h"
#include <sstream>
#include <algorithm>

namespace ScratchBird {

//----------------------------
// SpatialFunctionPerformanceMonitor Implementation
//----------------------------

SpatialFunctionPerformanceMonitor::SpatialFunctionPerformanceMonitor(MemoryPool& p)
    : pool(p), functionStats(p), totalSpatialOperations(0), totalOptimizationsSaved(0)
{
}

SpatialFunctionPerformanceMonitor::~SpatialFunctionPerformanceMonitor()
{
}

void SpatialFunctionPerformanceMonitor::recordFunctionCall(const string& functionName, ULONG64 executionTimeMs)
{
    SpatialFunctionStats* stats = const_cast<SpatialFunctionStats*>(functionStats.get(functionName));
    if (!stats)
    {
        functionStats.put(functionName, SpatialFunctionStats());
        stats = const_cast<SpatialFunctionStats*>(functionStats.get(functionName));
    }
    
    if (stats)
    {
        stats->totalCalls++;
        stats->totalExecutionTimeMs += executionTimeMs;
        totalSpatialOperations++;
    }
}

void SpatialFunctionPerformanceMonitor::recordCacheHit(const string& functionName)
{
    SpatialFunctionStats* stats = const_cast<SpatialFunctionStats*>(functionStats.get(functionName));
    if (stats)
    {
        stats->cacheHits++;
        totalOptimizationsSaved++;
    }
}

void SpatialFunctionPerformanceMonitor::recordCacheMiss(const string& functionName)
{
    SpatialFunctionStats* stats = const_cast<SpatialFunctionStats*>(functionStats.get(functionName));
    if (stats)
    {
        stats->cacheMisses++;
    }
}

void SpatialFunctionPerformanceMonitor::recordMbrFilter(const string& functionName, bool passed)
{
    SpatialFunctionStats* stats = const_cast<SpatialFunctionStats*>(functionStats.get(functionName));
    if (stats)
    {
        if (passed)
        {
            stats->mbrFilterPassed++;
            totalOptimizationsSaved++;
        }
        else
        {
            stats->mbrFilterFailed++;
        }
    }
}

void SpatialFunctionPerformanceMonitor::recordGeometryProcessed(const string& functionName, ULONG geometrySize)
{
    SpatialFunctionStats* stats = const_cast<SpatialFunctionStats*>(functionStats.get(functionName));
    if (stats)
    {
        stats->geometriesProcessed++;
        // Update running average
        stats->averageGeometrySize = 
            ((stats->averageGeometrySize * (stats->geometriesProcessed - 1)) + geometrySize) / 
            stats->geometriesProcessed;
    }
}

const SpatialFunctionStats* SpatialFunctionPerformanceMonitor::getFunctionStats(const string& functionName) const
{
    return functionStats.get(functionName);
}

void SpatialFunctionPerformanceMonitor::generatePerformanceReport(string& report) const
{
    std::ostringstream oss;
    
    oss << "ScratchBird Spatial Functions Performance Report\n";
    oss << "===============================================\n\n";
    
    oss << "Overall Statistics:\n";
    oss << "- Total Spatial Operations: " << totalSpatialOperations << "\n";
    oss << "- Total Optimizations Saved: " << totalOptimizationsSaved << "\n";
    oss << "- Optimization Ratio: " << 
        (totalSpatialOperations ? (double)totalOptimizationsSaved / totalSpatialOperations * 100.0 : 0.0) << "%\n\n";
    
    oss << "Function-Specific Statistics:\n";
    oss << "-----------------------------\n";
    
    // Iterate through function statistics
    for (auto it = functionStats.begin(); it != functionStats.end(); ++it)
    {
        const string& funcName = it->first;
        const SpatialFunctionStats& stats = it->second;
        
        oss << funcName << ":\n";
        oss << "  - Total Calls: " << stats.totalCalls << "\n";
        oss << "  - Average Execution Time: " << stats.getAverageExecutionTime() << " ms\n";
        oss << "  - Cache Hit Ratio: " << (stats.getCacheHitRatio() * 100.0) << "%\n";
        oss << "  - MBR Filter Efficiency: " << (stats.getMbrFilterEfficiency() * 100.0) << "%\n";
        oss << "  - Geometries Processed: " << stats.geometriesProcessed << "\n";
        oss << "  - Average Geometry Size: " << stats.averageGeometrySize << " bytes\n\n";
    }
    
    report = oss.str();
}

void SpatialFunctionPerformanceMonitor::resetStatistics()
{
    functionStats.clear();
    totalSpatialOperations = 0;
    totalOptimizationsSaved = 0;
}

void SpatialFunctionPerformanceMonitor::analyzePerformance(string& recommendations) const
{
    std::ostringstream oss;
    
    oss << "ScratchBird Spatial Functions Performance Analysis\n";
    oss << "=================================================\n\n";
    
    oss << "Performance Recommendations:\n";
    
    // Analyze cache performance
    ULONG64 totalCacheHits = 0;
    ULONG64 totalCacheMisses = 0;
    
    for (auto it = functionStats.begin(); it != functionStats.end(); ++it)
    {
        const SpatialFunctionStats& stats = it->second;
        totalCacheHits += stats.cacheHits;
        totalCacheMisses += stats.cacheMisses;
    }
    
    double overallCacheHitRatio = (totalCacheHits + totalCacheMisses) ? 
        (double)totalCacheHits / (totalCacheHits + totalCacheMisses) : 0.0;
    
    if (overallCacheHitRatio < 0.7)
    {
        oss << "1. LOW CACHE EFFICIENCY (" << (overallCacheHitRatio * 100.0) << "%)\n";
        oss << "   - Consider increasing geometry cache size\n";
        oss << "   - Review query patterns for better cache locality\n";
        oss << "   - Implement query result caching\n\n";
    }
    
    // Analyze MBR filter effectiveness
    ULONG64 totalMbrPassed = 0;
    ULONG64 totalMbrFailed = 0;
    
    for (auto it = functionStats.begin(); it != functionStats.end(); ++it)
    {
        const SpatialFunctionStats& stats = it->second;
        totalMbrPassed += stats.mbrFilterPassed;
        totalMbrFailed += stats.mbrFilterFailed;
    }
    
    double mbrEfficiency = (totalMbrPassed + totalMbrFailed) ? 
        (double)totalMbrPassed / (totalMbrPassed + totalMbrFailed) : 0.0;
    
    if (mbrEfficiency > 0.8)
    {
        oss << "2. EXCELLENT MBR FILTERING (" << (mbrEfficiency * 100.0) << "%)\n";
        oss << "   - MBR pre-filtering is highly effective\n";
        oss << "   - Consider expanding MBR-based optimizations\n\n";
    }
    else if (mbrEfficiency < 0.3)
    {
        oss << "2. POOR MBR FILTERING (" << (mbrEfficiency * 100.0) << "%)\n";
        oss << "   - Data may have poor spatial locality\n";
        oss << "   - Consider spatial clustering or indexing\n";
        oss << "   - Review query spatial selectivity\n\n";
    }
    
    // Analyze execution times
    ULONG64 slowFunctions = 0;
    for (auto it = functionStats.begin(); it != functionStats.end(); ++it)
    {
        const SpatialFunctionStats& stats = it->second;
        if (stats.getAverageExecutionTime() > 100.0) // 100ms threshold
        {
            slowFunctions++;
            oss << "3. SLOW FUNCTION: " << it->first << 
                " (avg: " << stats.getAverageExecutionTime() << " ms)\n";
            oss << "   - Consider algorithm optimization\n";
            oss << "   - Review geometry complexity\n";
            oss << "   - Implement parallel processing\n\n";
        }
    }
    
    if (slowFunctions == 0)
    {
        oss << "3. PERFORMANCE: All functions performing within acceptable limits\n\n";
    }
    
    // Overall recommendations
    oss << "General Recommendations:\n";
    oss << "- Monitor spatial index usage and effectiveness\n";
    oss << "- Consider implementing spatial query plan optimization\n";
    oss << "- Review geometry storage formats (WKB compression)\n";
    oss << "- Implement batch processing for multiple spatial operations\n";
    
    recommendations = oss.str();
}

//----------------------------
// SpatialFunctionOptimizer Implementation
//----------------------------

ULONG SpatialFunctionOptimizer::calculateGeometryComplexity(const Geometry* geometry)
{
    if (!geometry)
        return 0;
        
    // Base complexity on coordinate count and geometry type
    ULONG baseComplexity = geometry->getCoordinateCount();
    
    // Weight by geometry type complexity
    switch (geometry->getType())
    {
        case GEOMETRY_POINT:
            return baseComplexity; // Simplest
        case GEOMETRY_LINESTRING:
            return baseComplexity * 2; // Linear complexity
        case GEOMETRY_POLYGON:
            return baseComplexity * 4; // Area calculations are more complex
        case GEOMETRY_MULTIPOINT:
            return baseComplexity * 2;
        case GEOMETRY_MULTILINESTRING:
            return baseComplexity * 3;
        case GEOMETRY_MULTIPOLYGON:
            return baseComplexity * 5; // Most complex
        case GEOMETRY_GEOMETRYCOLLECTION:
            return baseComplexity * 6; // Highest complexity
        default:
            return baseComplexity;
    }
}

bool SpatialFunctionOptimizer::shouldUseMbrFiltering(const Geometry* geom1, const Geometry* geom2, 
                                                    SpatialOperation operation)
{
    if (!geom1 || !geom2)
        return false;
        
    // MBR filtering is most effective for these operations
    switch (operation)
    {
        case SpatialOperation::ST_INTERSECTS:
        case SpatialOperation::ST_CONTAINS:
        case SpatialOperation::ST_WITHIN:
        case SpatialOperation::ST_DISJOINT:
            return true;
            
        case SpatialOperation::ST_TOUCHES:
        case SpatialOperation::ST_CROSSES:
        case SpatialOperation::ST_OVERLAPS:
            // Less effective but still useful for complex geometries
            return (calculateGeometryComplexity(geom1) + calculateGeometryComplexity(geom2)) > 100;
            
        default:
            return false;
    }
}

bool SpatialFunctionOptimizer::shouldUseParallelProcessing(ULONG geometryCount, ULONG averageComplexity)
{
    // Use parallel processing for large datasets with complex geometries
    return (geometryCount > 100 && averageComplexity > 50) || geometryCount > 1000;
}

bool SpatialFunctionOptimizer::shouldCacheGeometry(const Geometry* geometry, ULONG cacheSize)
{
    if (!geometry || cacheSize >= 1000) // Cache is full
        return false;
        
    // Cache geometries that are likely to be reused
    ULONG complexity = calculateGeometryComplexity(geometry);
    
    // Cache complex geometries (expensive to recreate) and simple ones (frequently used)
    return complexity > 100 || complexity < 10;
}

void SpatialFunctionOptimizer::optimizeSpatialQuery(const string& queryPlan, string& optimizedPlan)
{
    // This would implement query plan optimization
    // For now, just copy the original plan
    optimizedPlan = queryPlan;
    
    // Future enhancements could include:
    // - Reordering spatial predicates by selectivity
    // - Suggesting spatial index usage
    // - Recommending join algorithms
    // - Identifying opportunities for parallel execution
}

} // namespace ScratchBird