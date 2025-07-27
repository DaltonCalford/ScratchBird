#ifndef RTREE_PAGE_STRUCTURES_H
#define RTREE_PAGE_STRUCTURES_H

#include "SpatialDataTypes.h"
#include "MBROperations.h"
#include "jrd/ods.h"
#include "common/classes/alloc.h"
#include "common/classes/array.h"
#include <vector>

namespace ScratchBird {

// R-Tree specific page types (extending ODS page types)
const USHORT pag_rtree_root = 18;     // R-Tree root page
const USHORT pag_rtree_internal = 19; // R-Tree internal node page
const USHORT pag_rtree_leaf = 20;     // R-Tree leaf node page
const USHORT pag_rtree_overflow = 21; // R-Tree overflow page

// R-Tree configuration constants
const USHORT RTREE_MAX_ENTRIES_PER_PAGE = 64;   // Maximum entries per node
const USHORT RTREE_MIN_ENTRIES_PER_PAGE = 8;    // Minimum entries per node (for splits)
const USHORT RTREE_PAGE_SIZE = 8192;             // Standard page size
const USHORT RTREE_HEADER_SIZE = 64;             // Page header size
const USHORT RTREE_ENTRY_OVERHEAD = 32;          // Per-entry overhead
const USHORT RTREE_MAX_LEVELS = 16;              // Maximum tree depth
const USHORT RTREE_FANOUT_TARGET = 32;           // Target fanout for optimal performance

// R-Tree entry types
enum RTreeEntryType : UCHAR
{
    RTREE_ENTRY_INTERNAL = 1,    // Internal node entry (MBR + child page pointer)
    RTREE_ENTRY_LEAF = 2,        // Leaf node entry (MBR + record pointer)
    RTREE_ENTRY_OVERFLOW = 3     // Overflow entry (for large geometries)
};

// R-Tree node split strategies
enum RTreeSplitStrategy : UCHAR
{
    RTREE_SPLIT_LINEAR = 1,      // Linear split (R-Tree)
    RTREE_SPLIT_QUADRATIC = 2,   // Quadratic split (R-Tree)
    RTREE_SPLIT_R_STAR = 3,      // R*-Tree split
    RTREE_SPLIT_HILBERT = 4,     // Hilbert R-Tree split
    RTREE_SPLIT_PRIORITY = 5     // Priority R-Tree split
};

// Forward declarations
class RTreeNode;
class RTreeEntry;
class RTreePage;

// R-Tree entry structure (stored on pages)
struct rtree_entry
{
    RTreeEntryType type;         // Entry type
    UCHAR flags;                 // Entry flags
    USHORT entry_length;         // Total entry length
    ULONG child_page;            // Child page number (for internal entries)
    RecordNumber record_number;  // Record number (for leaf entries)
    
    // MBR coordinates (always stored)
    double mbr_min_x;
    double mbr_min_y;
    double mbr_max_x;
    double mbr_max_y;
    
    // Optional Z/M coordinates
    double mbr_min_z;
    double mbr_max_z;
    double mbr_min_m;
    double mbr_max_m;
    
    // Variable-length geometry data follows
    UCHAR geometry_data[1];      // Variable length WKB geometry data
};

// R-Tree page header structure
struct rtree_page_header
{
    pag page_header;             // Standard page header
    
    // R-Tree specific fields
    USHORT rtree_level;          // Node level (0 = leaf, >0 = internal)
    USHORT rtree_entry_count;    // Number of entries on this page
    USHORT rtree_free_space;     // Free space available on page
    USHORT rtree_split_strategy; // Split strategy used for this node
    
    ULONG rtree_parent_page;     // Parent page number
    ULONG rtree_left_sibling;    // Left sibling page number
    ULONG rtree_right_sibling;   // Right sibling page number
    
    // MBR of all entries on this page
    double page_mbr_min_x;
    double page_mbr_min_y;
    double page_mbr_max_x;
    double page_mbr_max_y;
    
    // Statistics and optimization data
    ULONG rtree_entry_insertions; // Total entries inserted
    ULONG rtree_entry_deletions;  // Total entries deleted
    double rtree_total_area;       // Total area of all entries
    double rtree_total_overlap;    // Total overlap between entries
    
    // Index metadata
    SRID coordinate_srid;          // Spatial reference system ID
    USHORT coordinate_dimensions;  // Number of coordinate dimensions (2, 3, or 4)
    
    UCHAR rtree_flags;            // Page flags
    UCHAR rtree_reserved[7];      // Reserved for future use
};

// R-Tree page flags
const UCHAR RTREE_PAGE_LEAF = 0x01;           // Page is a leaf node
const UCHAR RTREE_PAGE_ROOT = 0x02;           // Page is the root node
const UCHAR RTREE_PAGE_OVERFLOW = 0x04;       // Page contains overflow entries
const UCHAR RTREE_PAGE_DIRTY = 0x08;          // Page has been modified
const UCHAR RTREE_PAGE_COMPRESSED = 0x10;     // Page uses compression
const UCHAR RTREE_PAGE_BULK_LOADED = 0x20;    // Page was bulk loaded
const UCHAR RTREE_PAGE_OPTIMIZED = 0x40;      // Page has been optimized

// Complete R-Tree page structure
struct rtree_page
{
    rtree_page_header header;
    UCHAR entry_data[1];         // Variable length entry data
};

// R-Tree entry wrapper class for easier manipulation
class RTreeEntry
{
private:
    rtree_entry* entry;
    MemoryPool& pool;
    bool ownsData;
    
public:
    RTreeEntry(MemoryPool& p);
    RTreeEntry(rtree_entry* entryData, MemoryPool& p, bool owns = false);
    RTreeEntry(const ExtendedMBR& mbr, RTreeEntryType type, MemoryPool& p);
    ~RTreeEntry();
    
    // Entry properties
    RTreeEntryType getType() const { return entry->type; }
    void setType(RTreeEntryType type) { entry->type = type; }
    
    UCHAR getFlags() const { return entry->flags; }
    void setFlags(UCHAR flags) { entry->flags = flags; }
    
    USHORT getLength() const { return entry->entry_length; }
    
    // Child page access (for internal entries)
    ULONG getChildPage() const { return entry->child_page; }
    void setChildPage(ULONG pageNum) { entry->child_page = pageNum; }
    
    // Record access (for leaf entries)
    RecordNumber getRecordNumber() const { return entry->record_number; }
    void setRecordNumber(RecordNumber recNum) { entry->record_number = recNum; }
    
    // MBR access
    ExtendedMBR getMBR() const;
    void setMBR(const ExtendedMBR& mbr);
    
    // Geometry data access
    const UCHAR* getGeometryData() const { return entry->geometry_data; }
    ULONG getGeometryDataSize() const;
    void setGeometryData(const UCHAR* data, ULONG size);
    
    // Geometry object access
    Geometry* getGeometry() const;
    void setGeometry(const Geometry& geometry);
    
    // Size calculations
    ULONG calculateStorageSize() const;
    static ULONG calculateStorageSize(const ExtendedMBR& mbr, const Geometry* geometry = nullptr);
    
    // Serialization
    void serialize(UCHAR* buffer) const;
    static RTreeEntry* deserialize(const UCHAR* buffer, MemoryPool& pool);
    
    // Comparison and sorting
    bool intersects(const ExtendedMBR& queryMBR) const;
    bool contains(const ExtendedMBR& queryMBR) const;
    double distance(const ExtendedMBR& queryMBR) const;
    double enlargement(const ExtendedMBR& newMBR) const;
    
    // Entry validation
    bool isValid() const;
    
private:
    void allocateEntry(ULONG geometrySize);
    void updateLength();
};

// R-Tree page wrapper class
class RTreePage
{
private:
    rtree_page* page;
    MemoryPool& pool;
    bool ownsData;
    ObjectsArray<RTreeEntry> entries;
    bool entriesLoaded;
    
public:
    RTreePage(MemoryPool& p);
    RTreePage(rtree_page* pageData, MemoryPool& p, bool owns = false);
    ~RTreePage();
    
    // Page properties
    USHORT getLevel() const { return page->header.rtree_level; }
    void setLevel(USHORT level) { page->header.rtree_level = level; }
    
    bool isLeaf() const { return getLevel() == 0; }
    bool isRoot() const { return (page->header.rtree_flags & RTREE_PAGE_ROOT) != 0; }
    
    USHORT getEntryCount() const { return page->header.rtree_entry_count; }
    USHORT getFreeSpace() const { return page->header.rtree_free_space; }
    
    // Parent and sibling access
    ULONG getParentPage() const { return page->header.rtree_parent_page; }
    void setParentPage(ULONG pageNum) { page->header.rtree_parent_page = pageNum; }
    
    ULONG getLeftSibling() const { return page->header.rtree_left_sibling; }
    void setLeftSibling(ULONG pageNum) { page->header.rtree_left_sibling = pageNum; }
    
    ULONG getRightSibling() const { return page->header.rtree_right_sibling; }
    void setRightSibling(ULONG pageNum) { page->header.rtree_right_sibling = pageNum; }
    
    // Page MBR
    ExtendedMBR getPageMBR() const;
    void setPageMBR(const ExtendedMBR& mbr);
    void updatePageMBR();
    
    // Entry management
    void loadEntries();
    const ObjectsArray<RTreeEntry>& getEntries();
    RTreeEntry* getEntry(USHORT index);
    
    bool addEntry(RTreeEntry* entry);
    bool removeEntry(USHORT index);
    void removeAllEntries();
    
    // Space management
    bool hasSpace(const RTreeEntry& entry) const;
    USHORT calculateRequiredSpace(const RTreeEntry& entry) const;
    void compactPage();
    
    // Page operations
    bool isFull() const { return getEntryCount() >= RTREE_MAX_ENTRIES_PER_PAGE; }
    bool needsSplit() const { return isFull(); }
    bool canMerge(const RTreePage& other) const;
    
    // Split operations
    std::pair<RTreePage*, RTreePage*> split(RTreeSplitStrategy strategy = RTREE_SPLIT_QUADRATIC);
    std::pair<RTreePage*, RTreePage*> linearSplit();
    std::pair<RTreePage*, RTreePage*> quadraticSplit();
    std::pair<RTreePage*, RTreePage*> rStarSplit();
    
    // Search operations
    std::vector<USHORT> findIntersecting(const ExtendedMBR& queryMBR) const;
    std::vector<USHORT> findContained(const ExtendedMBR& queryMBR) const;
    std::vector<USHORT> findContaining(const ExtendedMBR& queryMBR) const;
    
    // Optimization operations
    void optimize();
    double calculateDeadSpace() const;
    double calculateOverlap() const;
    void reorderEntries();
    
    // Statistics
    ULONG getTotalInsertions() const { return page->header.rtree_entry_insertions; }
    ULONG getTotalDeletions() const { return page->header.rtree_entry_deletions; }
    double getTotalArea() const { return page->header.rtree_total_area; }
    double getTotalOverlap() const { return page->header.rtree_total_overlap; }
    
    void incrementInsertions() { page->header.rtree_entry_insertions++; }
    void incrementDeletions() { page->header.rtree_entry_deletions++; }
    
    // Coordinate system
    SRID getSRID() const { return page->header.coordinate_srid; }
    void setSRID(SRID srid) { page->header.coordinate_srid = srid; }
    
    USHORT getDimensions() const { return page->header.coordinate_dimensions; }
    void setDimensions(USHORT dims) { page->header.coordinate_dimensions = dims; }
    
    // Page flags
    bool hasFlag(UCHAR flag) const { return (page->header.rtree_flags & flag) != 0; }
    void setFlag(UCHAR flag) { page->header.rtree_flags |= flag; }
    void clearFlag(UCHAR flag) { page->header.rtree_flags &= ~flag; }
    
    // Serialization
    void serialize(UCHAR* buffer) const;
    static RTreePage* deserialize(const UCHAR* buffer, MemoryPool& pool);
    
    // Validation
    bool isValid() const;
    bool validateStructure() const;
    std::vector<string> validate() const;
    
    // Memory management
    ULONG getMemoryUsage() const;
    void releaseMemory();
    
private:
    void initializePage();
    void updateStatistics();
    void storeEntries();
    std::pair<USHORT, USHORT> pickSeeds(RTreeSplitStrategy strategy) const;
    void assignEntries(RTreePage* page1, RTreePage* page2, 
                      const std::vector<USHORT>& group1, 
                      const std::vector<USHORT>& group2);
};

// R-Tree page factory and management
class RTreePageFactory
{
private:
    MemoryPool& pool;
    
public:
    RTreePageFactory(MemoryPool& p) : pool(p) {}
    ~RTreePageFactory() {}
    
    // Page creation
    RTreePage* createLeafPage(SRID srid = DEFAULT_SRID, USHORT dimensions = 2);
    RTreePage* createInternalPage(USHORT level, SRID srid = DEFAULT_SRID, USHORT dimensions = 2);
    RTreePage* createRootPage(SRID srid = DEFAULT_SRID, USHORT dimensions = 2);
    
    // Entry creation
    RTreeEntry* createLeafEntry(const ExtendedMBR& mbr, RecordNumber recordNum, const Geometry* geometry = nullptr);
    RTreeEntry* createInternalEntry(const ExtendedMBR& mbr, ULONG childPage);
    
    // Page loading from storage
    RTreePage* loadPage(ULONG pageNumber);
    void savePage(RTreePage* page, ULONG pageNumber);
    
    // Bulk operations
    std::vector<RTreePage*> createPages(ULONG count, bool isLeaf = true);
    void destroyPages(std::vector<RTreePage*>& pages);
    
    // Memory optimization
    void optimizePageLayout(RTreePage* page);
    ULONG estimatePageCount(ULONG entryCount, USHORT averageEntrySize);
};

// R-Tree page cache for performance optimization
class RTreePageCache
{
private:
    struct CacheEntry
    {
        ULONG pageNumber;
        RTreePage* page;
        ULONG64 lastAccess;
        bool dirty;
        ULONG accessCount;
        
        CacheEntry() : pageNumber(0), page(nullptr), lastAccess(0), dirty(false), accessCount(0) {}
    };
    
    std::vector<CacheEntry> cache;
    ULONG maxCacheSize;
    ULONG64 accessCounter;
    MemoryPool& pool;
    
    // Cache statistics
    ULONG64 totalRequests;
    ULONG64 cacheHits;
    ULONG64 cacheMisses;
    
public:
    RTreePageCache(ULONG maxSize, MemoryPool& p);
    ~RTreePageCache();
    
    // Cache operations
    RTreePage* get(ULONG pageNumber);
    void put(ULONG pageNumber, RTreePage* page);
    void remove(ULONG pageNumber);
    void markDirty(ULONG pageNumber);
    void flush();
    void clear();
    
    // Cache management
    void evictLRU();
    void evictLFU();
    void resize(ULONG newSize);
    
    // Statistics
    double getHitRate() const;
    ULONG getCacheSize() const { return cache.size(); }
    ULONG getMaxSize() const { return maxCacheSize; }
    ULONG64 getTotalRequests() const { return totalRequests; }
    
    // Performance analysis
    void printStatistics() const;
    void resetStatistics();
    
private:
    CacheEntry* findEntry(ULONG pageNumber);
    void updateAccessTime(CacheEntry& entry);
    CacheEntry* findLRUEntry();
    CacheEntry* findLFUEntry();
};

// R-Tree storage manager interface
class RTreeStorageManager
{
public:
    virtual ~RTreeStorageManager() {}
    
    // Page I/O operations
    virtual RTreePage* readPage(ULONG pageNumber) = 0;
    virtual void writePage(ULONG pageNumber, RTreePage* page) = 0;
    virtual ULONG allocatePage() = 0;
    virtual void deallocatePage(ULONG pageNumber) = 0;
    
    // Bulk operations
    virtual std::vector<RTreePage*> readPages(const std::vector<ULONG>& pageNumbers) = 0;
    virtual void writePages(const std::map<ULONG, RTreePage*>& pages) = 0;
    
    // Storage statistics
    virtual ULONG getTotalPages() const = 0;
    virtual ULONG getUsedPages() const = 0;
    virtual ULONG getFreePages() const = 0;
    
    // Storage optimization
    virtual void defragment() = 0;
    virtual void compact() = 0;
    virtual double getFragmentationRatio() const = 0;
};

// File-based R-Tree storage implementation
class FileRTreeStorageManager : public RTreeStorageManager
{
private:
    MemoryPool& pool;
    RTreePageCache* pageCache;
    std::vector<ULONG> freePages;
    ULONG nextPageNumber;
    
public:
    FileRTreeStorageManager(MemoryPool& p, ULONG cacheSize = 1000);
    virtual ~FileRTreeStorageManager();
    
    // RTreeStorageManager interface
    virtual RTreePage* readPage(ULONG pageNumber) override;
    virtual void writePage(ULONG pageNumber, RTreePage* page) override;
    virtual ULONG allocatePage() override;
    virtual void deallocatePage(ULONG pageNumber) override;
    
    virtual std::vector<RTreePage*> readPages(const std::vector<ULONG>& pageNumbers) override;
    virtual void writePages(const std::map<ULONG, RTreePage*>& pages) override;
    
    virtual ULONG getTotalPages() const override { return nextPageNumber; }
    virtual ULONG getUsedPages() const override { return nextPageNumber - freePages.size(); }
    virtual ULONG getFreePages() const override { return freePages.size(); }
    
    virtual void defragment() override;
    virtual void compact() override;
    virtual double getFragmentationRatio() const override;
    
    // File management
    void setPageCache(RTreePageCache* cache) { pageCache = cache; }
    void sync();
    void close();
    
private:
    RTreePage* loadPageFromDisk(ULONG pageNumber);
    void savePageToDisk(ULONG pageNumber, RTreePage* page);
    void addToFreeList(ULONG pageNumber);
    ULONG getFromFreeList();
};

// Utility functions for R-Tree page operations
namespace RTreePageUtils
{
    // Page analysis
    bool isPageValid(const RTreePage& page);
    double calculatePageQuality(const RTreePage& page);
    string generatePageReport(const RTreePage& page);
    
    // Entry operations
    std::vector<RTreeEntry*> sortEntriesByArea(const std::vector<RTreeEntry*>& entries);
    std::vector<RTreeEntry*> sortEntriesByDistance(const std::vector<RTreeEntry*>& entries, const ExtendedMBR& reference);
    
    // Split analysis
    double evaluateSplitQuality(const RTreePage& page1, const RTreePage& page2);
    RTreeSplitStrategy chooseBestSplitStrategy(const RTreePage& page);
    
    // Memory estimation
    ULONG estimatePageMemoryUsage(USHORT entryCount, USHORT averageGeometrySize);
    ULONG estimateTreeMemoryUsage(ULONG totalEntries, USHORT averageGeometrySize, USHORT fanout);
    
    // Performance optimization
    void optimizePageOrdering(std::vector<RTreePage*>& pages);
    void balancePageSizes(std::vector<RTreePage*>& pages);
    
    // Debugging and diagnostics
    void dumpPageStructure(const RTreePage& page, string& output);
    bool validatePageChain(const std::vector<RTreePage*>& pages);
    void analyzePageDistribution(const std::vector<RTreePage*>& pages, string& analysis);
}

} // namespace ScratchBird

#endif // RTREE_PAGE_STRUCTURES_H