#ifndef SPATIAL_FUNCTION_PERFORMANCE_H
#define SPATIAL_FUNCTION_PERFORMANCE_H

#include "firebird.h"
#include "common/classes/alloc.h"
#include "common/classes/GenericMap.h"
#include <chrono>

namespace ScratchBird {

//----------------------------
// Spatial Function Performance Monitor
//----------------------------

struct SpatialFunctionStats
{
    ULONG64 totalCalls;
    ULONG64 totalExecutionTimeMs;
    ULONG64 cacheHits;
    ULONG64 cacheMisses;
    ULONG64 mbrFilterPassed;
    ULONG64 mbrFilterFailed;
    ULONG64 geometriesProcessed;
    ULONG64 averageGeometrySize;
    
    SpatialFunctionStats() : totalCalls(0), totalExecutionTimeMs(0), cacheHits(0), 
        cacheMisses(0), mbrFilterPassed(0), mbrFilterFailed(0), 
        geometriesProcessed(0), averageGeometrySize(0) {}
        
    double getCacheHitRatio() const 
    {
        ULONG64 total = cacheHits + cacheMisses;
        return total ? (double)cacheHits / total : 0.0;
    }
    
    double getAverageExecutionTime() const
    {
        return totalCalls ? (double)totalExecutionTimeMs / totalCalls : 0.0;
    }
    
    double getMbrFilterEfficiency() const
    {
        ULONG64 total = mbrFilterPassed + mbrFilterFailed;
        return total ? (double)mbrFilterPassed / total : 0.0;
    }
};

class SpatialFunctionPerformanceMonitor
{
private:
    MemoryPool& pool;
    GenericMap<Pair<NonPooled<string, SpatialFunctionStats>>> functionStats;
    mutable ULONG64 totalSpatialOperations;
    mutable ULONG64 totalOptimizationsSaved;
    
public:
    SpatialFunctionPerformanceMonitor(MemoryPool& p);
    ~SpatialFunctionPerformanceMonitor();
    
    // Performance tracking
    void recordFunctionCall(const string& functionName, ULONG64 executionTimeMs);
    void recordCacheHit(const string& functionName);
    void recordCacheMiss(const string& functionName);
    void recordMbrFilter(const string& functionName, bool passed);
    void recordGeometryProcessed(const string& functionName, ULONG geometrySize);
    
    // Statistics retrieval
    const SpatialFunctionStats* getFunctionStats(const string& functionName) const;
    ULONG64 getTotalSpatialOperations() const { return totalSpatialOperations; }
    ULONG64 getTotalOptimizationsSaved() const { return totalOptimizationsSaved; }
    
    // Performance analysis
    void generatePerformanceReport(string& report) const;
    void resetStatistics();
    
    // Optimization recommendations
    void analyzePerformance(string& recommendations) const;
};

//----------------------------
// Performance Timer Utility
//----------------------------

class SpatialFunctionTimer
{
private:
    std::chrono::high_resolution_clock::time_point startTime;
    SpatialFunctionPerformanceMonitor* monitor;
    string functionName;
    
public:
    SpatialFunctionTimer(SpatialFunctionPerformanceMonitor* mon, const string& funcName)
        : monitor(mon), functionName(funcName)
    {
        startTime = std::chrono::high_resolution_clock::now();
    }
    
    ~SpatialFunctionTimer()
    {
        if (monitor)
        {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            monitor->recordFunctionCall(functionName, duration.count());
        }
    }
};

// Macro for easy performance timing
#define SPATIAL_FUNCTION_TIMER(monitor, funcName) \
    SpatialFunctionTimer __timer(monitor, funcName)

//----------------------------
// Advanced Performance Optimizations
//----------------------------

class SpatialFunctionOptimizer
{
public:
    // Geometry complexity analysis
    static ULONG calculateGeometryComplexity(const Geometry* geometry);
    
    // Optimal algorithm selection based on geometry characteristics
    static bool shouldUseMbrFiltering(const Geometry* geom1, const Geometry* geom2, 
                                     SpatialOperation operation);
    
    // Parallel processing eligibility
    static bool shouldUseParallelProcessing(ULONG geometryCount, 
                                          ULONG averageComplexity);
    
    // Memory usage optimization
    static bool shouldCacheGeometry(const Geometry* geometry, ULONG cacheSize);
    
    // Query plan optimization
    static void optimizeSpatialQuery(const string& queryPlan, string& optimizedPlan);
};

} // namespace ScratchBird

#endif // SPATIAL_FUNCTION_PERFORMANCE_H