/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		SpatialIndex.h
 *	DESCRIPTION:	Spatial index implementation using R-Tree for geographic data
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
 * 2025.07.24 - ScratchBird Spatial Index Implementation
 */

#ifndef JRD_SPATIAL_INDEX_H
#define JRD_SPATIAL_INDEX_H

#include "IndexType.h"
#include "RTreeAlgorithms.h"
#include "RTreePageStructures.h"
#include "SpatialDataTypes.h"
#include "SpatialReferenceSystem.h"
#include "MBROperations.h"
#include "WKTParser.h"
#include "WKBParser.h"
#include "constants.h"
#include "../common/classes/array.h"
#include "../common/classes/GenericMap.h"
#include <memory>

namespace ScratchBird {

// Forward declarations
class RTreeIndex;
class SpatialQueryProcessor;

//----------------------------
// Spatial Index Constants
//----------------------------
const char IDX_TYPE_NAME_SPATIAL[] = "RTREE";
const int IDX_TYPE_ID_SPATIAL = 4;
const char IDX_VERSION_SPATIAL[] = "1.0.0";

// Spatial index specific constants
const USHORT SPATIAL_DEFAULT_PAGE_SIZE = 8192;
const USHORT SPATIAL_MIN_ENTRIES = 8;
const USHORT SPATIAL_MAX_ENTRIES = 64;
const USHORT SPATIAL_MAX_LEVELS = 16;
const SRID SPATIAL_DEFAULT_SRID = 4326; // WGS84

// Spatial query types
enum SpatialQueryType : UCHAR
{
    SPATIAL_QUERY_INTERSECTS = 1,
    SPATIAL_QUERY_CONTAINS = 2,
    SPATIAL_QUERY_CONTAINED_BY = 3,
    SPATIAL_QUERY_TOUCHES = 4,
    SPATIAL_QUERY_CROSSES = 5,
    SPATIAL_QUERY_OVERLAPS = 6,
    SPATIAL_QUERY_WITHIN_DISTANCE = 7,
    SPATIAL_QUERY_KNN = 8
};

// Spatial index statistics
struct SpatialIndexStatistics
{
    ULONG total_geometries;        // Total number of geometries
    ULONG total_pages;             // Total number of R-Tree pages
    ULONG leaf_pages;              // Number of leaf pages
    ULONG internal_pages;          // Number of internal pages
    USHORT tree_height;            // Height of the R-Tree
    double average_fanout;         // Average fanout per node
    double page_utilization;       // Average page utilization
    double total_area;             // Total area covered by all MBRs
    double total_overlap;          // Total overlap between MBRs
    double index_quality;          // Overall index quality score
    ULONG storage_bytes;           // Total storage used in bytes
    ExtendedMBR bounding_box;      // Overall bounding box of all geometries
};

//----------------------------
// SpatialRetrieval - Spatial-specific retrieval context
//----------------------------
class SpatialRetrieval
{
private:
    SpatialQueryType queryType;
    ExtendedMBR queryMBR;
    Geometry* queryGeometry;
    double maxDistance;
    ULONG k; // For KNN queries
    SpatialReferenceSystem* srs;
    MemoryPool& pool;
    
    std::vector<RTreeSearchResult> results;
    ULONG currentIndex;
    bool resultsValid;
    
public:
    SpatialRetrieval(SpatialQueryType type, const ExtendedMBR& mbr, MemoryPool& p);
    SpatialRetrieval(SpatialQueryType type, const Geometry& geom, MemoryPool& p);
    ~SpatialRetrieval();
    
    // Query configuration
    void setQueryMBR(const ExtendedMBR& mbr) { queryMBR = mbr; resultsValid = false; }
    void setQueryGeometry(const Geometry& geom);
    void setMaxDistance(double distance) { maxDistance = distance; resultsValid = false; }
    void setK(ULONG kValue) { k = kValue; resultsValid = false; }
    void setSRS(SpatialReferenceSystem* srsPtr) { srs = srsPtr; }
    
    // Query properties
    SpatialQueryType getQueryType() const { return queryType; }
    const ExtendedMBR& getQueryMBR() const { return queryMBR; }
    const Geometry* getQueryGeometry() const { return queryGeometry; }
    double getMaxDistance() const { return maxDistance; }
    ULONG getK() const { return k; }
    
    // Result management
    void setResults(const std::vector<RTreeSearchResult>& newResults);
    bool getNextResult(RecordNumber& recordNum, ExtendedMBR& mbr);
    void reset();
    ULONG getResultCount() const { return results.size(); }
    bool hasMoreResults() const { return currentIndex < results.size(); }
    
    // Coordinate transformations
    bool transformQuery(const SpatialReferenceSystem& targetSRS);
    bool transformResults(const SpatialReferenceSystem& targetSRS);
};

//----------------------------
// SpatialIndex - Main spatial index implementation class
//----------------------------
class SpatialIndex : public Jrd::IndexType
{
private:
    // Core components
    std::unique_ptr<RTreeIndex> rtreeIndex;
    std::unique_ptr<RTreeAlgorithms> algorithms;
    std::unique_ptr<SpatialQueryProcessor> queryProcessor;
    std::unique_ptr<SRSRegistry> srsRegistry;
    MemoryPool& pool;
    
    // Index properties
    Jrd::Database* database;
    Jrd::jrd_rel* relation;
    const Jrd::index_desc* indexDesc;
    SRID spatialSRID;
    USHORT dimensions;
    RTreeSplitStrategy splitStrategy;
    
    // Statistics and monitoring
    mutable SpatialIndexStatistics statistics;
    mutable bool statisticsValid;
    ULONG64 totalInserts;
    ULONG64 totalDeletes;
    ULONG64 totalSearches;
    
public:
    // Constructor and destructor
    SpatialIndex(MemoryPool& p);
    virtual ~SpatialIndex();
    
    //----------------------------
    // IndexType interface implementation
    //----------------------------
    
    virtual Jrd::index_error_t initialize(Jrd::thread_db* tdbb, Jrd::Database* database, 
                                         Jrd::jrd_rel* relation, const Jrd::index_desc* desc) override;
    
    virtual Jrd::index_error_t insert(Jrd::thread_db* tdbb, const dsc* key, 
                                     Jrd::RecordNumber record, Jrd::jrd_tra* transaction) override;
    
    virtual bool lookup(Jrd::thread_db* tdbb, const dsc* key, Jrd::IndexRetrieval* retrieval) override;
    
    virtual Jrd::index_error_t remove(Jrd::thread_db* tdbb, const dsc* key, 
                                     Jrd::RecordNumber record, Jrd::jrd_tra* transaction) override;
    
    virtual double calculateSelectivity(Jrd::thread_db* tdbb, const dsc* key) override;
    
    virtual Jrd::index_error_t getStatistics(Jrd::thread_db* tdbb, Jrd::IndexStatistics* stats) override;
    
    virtual Jrd::index_error_t validate(Jrd::thread_db* tdbb) override;
    
    virtual Jrd::index_error_t rebuild(Jrd::thread_db* tdbb, Jrd::jrd_tra* transaction) override;
    
    virtual const char* getTypeName() const override { return IDX_TYPE_NAME_SPATIAL; }
    
    virtual const char* getVersion() const override { return IDX_VERSION_SPATIAL; }
    
    virtual bool supportsDataType(int field_type) const override;
    
    virtual bool supportsIndexFlags(USHORT flags) const override;
    
    virtual USHORT getOptimalPageSize(USHORT avg_key_length, ULONG cardinality) const override;
    
    virtual ULONG estimateStorageSize(ULONG num_keys, USHORT avg_key_length) const override;
    
    //----------------------------
    // Spatial-specific interface
    //----------------------------
    
    // Spatial query methods
    bool spatialLookup(Jrd::thread_db* tdbb, SpatialQueryType queryType, 
                      const ExtendedMBR& queryMBR, SpatialRetrieval* retrieval);
    
    bool spatialLookup(Jrd::thread_db* tdbb, SpatialQueryType queryType, 
                      const Geometry& queryGeometry, SpatialRetrieval* retrieval);
    
    bool withinDistanceQuery(Jrd::thread_db* tdbb, const Coordinate& center, 
                           double maxDistance, SpatialRetrieval* retrieval);
    
    bool kNearestNeighbors(Jrd::thread_db* tdbb, const Coordinate& center, 
                         ULONG k, SpatialRetrieval* retrieval);
    
    // Bulk operations
    Jrd::index_error_t bulkInsert(Jrd::thread_db* tdbb, 
                                 const std::vector<std::pair<Geometry*, Jrd::RecordNumber>>& geometries,
                                 Jrd::jrd_tra* transaction);
    
    Jrd::index_error_t bulkDelete(Jrd::thread_db* tdbb,
                                 const std::vector<std::pair<Geometry*, Jrd::RecordNumber>>& geometries,
                                 Jrd::jrd_tra* transaction);
    
    // Index maintenance
    Jrd::index_error_t optimize(Jrd::thread_db* tdbb);
    Jrd::index_error_t rebalance(Jrd::thread_db* tdbb);
    double calculateQuality(Jrd::thread_db* tdbb);
    
    // Spatial reference system management
    bool setSRID(SRID srid);
    SRID getSRID() const { return spatialSRID; }
    SpatialReferenceSystem* getSRS() const;
    bool transformGeometry(const Geometry& input, SRID targetSRID, Geometry*& output) const;
    
    // Statistics and introspection
    const SpatialIndexStatistics& getSpatialStatistics(Jrd::thread_db* tdbb) const;
    void updateStatistics(Jrd::thread_db* tdbb) const;
    void resetStatistics();
    
    ExtendedMBR getBoundingBox(Jrd::thread_db* tdbb) const;
    ULONG getGeometryCount(Jrd::thread_db* tdbb) const;
    USHORT getTreeHeight(Jrd::thread_db* tdbb) const;
    double getPageUtilization(Jrd::thread_db* tdbb) const;
    
    // Configuration
    void setSplitStrategy(RTreeSplitStrategy strategy) { splitStrategy = strategy; }
    RTreeSplitStrategy getSplitStrategy() const { return splitStrategy; }
    void setDimensions(USHORT dims) { dimensions = dims; }
    USHORT getDimensions() const { return dimensions; }
    
protected:
    //----------------------------
    // Helper methods
    //----------------------------
    
    // Geometry processing
    Geometry* extractGeometry(const dsc* key) const;
    ExtendedMBR extractMBR(const dsc* key) const;
    bool validateGeometry(const Geometry* geometry) const;
    bool validateMBR(const ExtendedMBR& mbr) const;
    
    // Key conversion
    virtual Jrd::index_error_t convertSpatialKey(const dsc* key, ExtendedMBR& mbr, Geometry*& geometry) const;
    ByteChunk* geometryToWKB(const Geometry& geometry) const;
    Geometry* wkbToGeometry(const ByteChunk& wkb) const;
    
    // Coordinate system operations
    bool transformMBR(const ExtendedMBR& input, SRID targetSRID, ExtendedMBR& output) const;
    bool validateSRID(SRID srid) const;
    
    // Statistics helpers
    void collectStatistics(Jrd::thread_db* tdbb, SpatialIndexStatistics& stats) const;
    void invalidateStatistics() const { statisticsValid = false; }
    
    // R-Tree integration
    RTreeSearchParams createSearchParams(SpatialQueryType queryType, const ExtendedMBR& queryMBR, 
                                        double maxDistance = 0.0, ULONG k = 0) const;
    void processSearchResults(const std::vector<RTreeSearchResult>& rtreeResults, 
                            SpatialRetrieval* retrieval) const;
    
    // Error handling
    Jrd::index_error_t handleSpatialError(const string& operation, const string& error) const;
    void logSpatialOperation(const string& operation, bool success, double duration = 0.0) const;
    
private:
    // Internal initialization
    bool initializeRTree(Jrd::thread_db* tdbb);
    bool initializeSRS(Jrd::thread_db* tdbb);
    bool loadIndexMetadata(Jrd::thread_db* tdbb);
    bool saveIndexMetadata(Jrd::thread_db* tdbb);
    
    // Page management integration
    bool setupPageStructures(Jrd::thread_db* tdbb);
    bool setupStorageManager(Jrd::thread_db* tdbb);
    
    // Query optimization
    bool optimizeQuery(SpatialQueryType queryType, ExtendedMBR& queryMBR) const;
    double estimateQueryCost(SpatialQueryType queryType, const ExtendedMBR& queryMBR) const;
    bool shouldUseIndex(SpatialQueryType queryType, const ExtendedMBR& queryMBR) const;
};

//----------------------------
// SpatialIndexFactory - Factory for creating spatial index instances
//----------------------------
class SpatialIndexFactory : public Jrd::IndexTypeFactory
{
private:
    MemoryPool& pool;
    
public:
    SpatialIndexFactory(MemoryPool& p) : pool(p) {}
    virtual ~SpatialIndexFactory() = default;
    
    virtual Jrd::IndexType* createIndex(Jrd::thread_db* tdbb, Jrd::Database* database,
                                       Jrd::jrd_rel* relation, const Jrd::index_desc* desc) override;
    
    virtual const char* getTypeName() const override { return IDX_TYPE_NAME_SPATIAL; }
    
    virtual int getTypeId() const override { return IDX_TYPE_ID_SPATIAL; }
    
    // Factory-specific methods
    bool validateSpatialIndexCreation(const Jrd::index_desc* desc) const;
    USHORT recommendPageSize(USHORT avgGeometrySize, ULONG estimatedCount) const;
    RTreeSplitStrategy recommendSplitStrategy(const Jrd::index_desc* desc) const;
};

//----------------------------
// Spatial Query Processor - Advanced spatial query operations
//----------------------------
class SpatialQueryProcessor
{
private:
    SpatialIndex& spatialIndex;
    MemoryPool& pool;
    SpatialReferenceSystem* defaultSRS;
    
public:
    SpatialQueryProcessor(SpatialIndex& index, MemoryPool& p);
    ~SpatialQueryProcessor();
    
    // Complex spatial queries
    bool processIntersectionQuery(Jrd::thread_db* tdbb, const Geometry& queryGeom, 
                                SpatialRetrieval* retrieval);
    
    bool processContainmentQuery(Jrd::thread_db* tdbb, const Geometry& queryGeom, 
                               SpatialRetrieval* retrieval);
    
    bool processDistanceQuery(Jrd::thread_db* tdbb, const Geometry& queryGeom, 
                            double maxDistance, SpatialRetrieval* retrieval);
    
    bool processKNNQuery(Jrd::thread_db* tdbb, const Geometry& queryGeom, 
                        ULONG k, SpatialRetrieval* retrieval);
    
    // Spatial joins
    bool processSpatialJoin(Jrd::thread_db* tdbb, SpatialIndex& otherIndex, 
                          SpatialQueryType joinType, std::vector<std::pair<Jrd::RecordNumber, Jrd::RecordNumber>>& results);
    
    // Advanced analytics
    bool calculateConvexHull(Jrd::thread_db* tdbb, const std::vector<Jrd::RecordNumber>& records, 
                           Polygon*& convexHull);
    
    bool calculateCentroid(Jrd::thread_db* tdbb, const std::vector<Jrd::RecordNumber>& records, 
                         Point*& centroid);
    
    double calculateTotalArea(Jrd::thread_db* tdbb, const std::vector<Jrd::RecordNumber>& records);
    
    // Query optimization
    void optimizeQueryPlan(SpatialQueryType queryType, ExtendedMBR& queryMBR);
    double estimateSelectivity(SpatialQueryType queryType, const ExtendedMBR& queryMBR);
    
private:
    // Geometric algorithms
    bool preciseIntersectionTest(const Geometry& geom1, const Geometry& geom2);
    bool preciseContainmentTest(const Geometry& container, const Geometry& contained);
    double preciseDistance(const Geometry& geom1, const Geometry& geom2);
    
    // Filter strategies
    bool applyMBRFilter(const ExtendedMBR& queryMBR, const ExtendedMBR& candidateMBR, SpatialQueryType queryType);
    bool applyGeometryFilter(const Geometry& queryGeom, const Geometry& candidateGeom, SpatialQueryType queryType);
    
    // Performance optimization
    void cacheQueryResults(const string& querySignature, const std::vector<RTreeSearchResult>& results);
    bool getCachedResults(const string& querySignature, std::vector<RTreeSearchResult>& results);
};

//----------------------------
// Utility functions for spatial operations
//----------------------------
namespace SpatialIndexUtils
{
    // Data type validation
    bool isSpatialDataType(int fieldType);
    bool isGeometryBlob(const dsc* descriptor);
    USHORT getGeometryDataSize(const Geometry& geometry);
    
    // Conversion utilities
    string geometryToWKT(const Geometry& geometry);
    Geometry* wktToGeometry(const string& wkt, MemoryPool& pool);
    ByteChunk* geometryToWKB(const Geometry& geometry, MemoryPool& pool);
    Geometry* wkbToGeometry(const ByteChunk& wkb, MemoryPool& pool);
    
    // Spatial analysis
    bool geometriesIntersect(const Geometry& geom1, const Geometry& geom2);
    bool geometryContains(const Geometry& container, const Geometry& contained);
    double geometryDistance(const Geometry& geom1, const Geometry& geom2);
    
    // Index optimization
    RTreeSplitStrategy chooseBestSplitStrategy(const std::vector<ExtendedMBR>& mbrs);
    USHORT calculateOptimalPageSize(USHORT avgGeometrySize, ULONG fanout);
    double assessIndexQuality(const SpatialIndexStatistics& stats);
    
    // Performance monitoring
    void logSpatialQuery(SpatialQueryType queryType, const ExtendedMBR& queryMBR, 
                        ULONG resultCount, double executionTime);
    
    string generateSpatialIndexReport(const SpatialIndexStatistics& stats);
    void analyzeSpatialDistribution(const std::vector<ExtendedMBR>& mbrs, string& analysis);
}

} // namespace ScratchBird

#endif // JRD_SPATIAL_INDEX_H