#include "RTreePageStructures.h"
#include "WKBParser.h"
#include "common/StatusArg.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>

using namespace ScratchBird;

//============================================================================
// RTreeEntry Implementation
//============================================================================

RTreeEntry::RTreeEntry(MemoryPool& p) : entry(nullptr), pool(p), ownsData(false)
{
}

RTreeEntry::RTreeEntry(rtree_entry* entryData, MemoryPool& p, bool owns)
    : entry(entryData), pool(p), ownsData(owns)
{
}

RTreeEntry::RTreeEntry(const ExtendedMBR& mbr, RTreeEntryType type, MemoryPool& p)
    : entry(nullptr), pool(p), ownsData(true)
{
    allocateEntry(0); // No geometry data initially
    entry->type = type;
    entry->flags = 0;
    setMBR(mbr);
    updateLength();
}

RTreeEntry::~RTreeEntry()
{
    if (ownsData && entry) {
        // Memory managed by pool - will be cleaned up automatically
        entry = nullptr;
    }
}

ExtendedMBR RTreeEntry::getMBR() const
{
    if (!entry) return ExtendedMBR();
    
    return ExtendedMBR(entry->mbr_min_x, entry->mbr_min_y,
                      entry->mbr_max_x, entry->mbr_max_y);
}

void RTreeEntry::setMBR(const ExtendedMBR& mbr)
{
    if (!entry) return;
    
    entry->mbr_min_x = mbr.minX;
    entry->mbr_min_y = mbr.minY;
    entry->mbr_max_x = mbr.maxX;
    entry->mbr_max_y = mbr.maxY;
    
    // Initialize Z and M coordinates to 0 for now
    entry->mbr_min_z = 0.0;
    entry->mbr_max_z = 0.0;
    entry->mbr_min_m = 0.0;
    entry->mbr_max_m = 0.0;
}

ULONG RTreeEntry::getGeometryDataSize() const
{
    if (!entry) return 0;
    return entry->entry_length - sizeof(rtree_entry) + 1; // Subtract base size, add back geometry_data[1]
}

void RTreeEntry::setGeometryData(const UCHAR* data, ULONG size)
{
    if (!entry || !data) return;
    
    // Reallocate if needed
    ULONG currentSize = getGeometryDataSize();
    if (size > currentSize) {
        allocateEntry(size);
    }
    
    memcpy(entry->geometry_data, data, size);
    updateLength();
}

Geometry* RTreeEntry::getGeometry() const
{
    if (!entry || getGeometryDataSize() == 0) return nullptr;
    
    try {
        return WKBUtils::fromWKB(entry->geometry_data, getGeometryDataSize(), pool);
    }
    catch (...) {
        return nullptr;
    }
}

void RTreeEntry::setGeometry(const Geometry& geometry)
{
    ByteChunk* wkb = WKBUtils::toWKB(geometry, pool);
    if (wkb) {
        setGeometryData(wkb->getBuffer(), wkb->getCount());
        delete wkb;
    }
}

ULONG RTreeEntry::calculateStorageSize() const
{
    return sizeof(rtree_entry) - 1 + getGeometryDataSize();
}

ULONG RTreeEntry::calculateStorageSize(const ExtendedMBR& mbr, const Geometry* geometry)
{
    ULONG size = sizeof(rtree_entry) - 1; // Base size without geometry_data[1]
    
    if (geometry) {
        size += WKBUtils::getWKBSize(*geometry);
    }
    
    return size;
}

bool RTreeEntry::intersects(const ExtendedMBR& queryMBR) const
{
    return getMBR().intersects(queryMBR);
}

bool RTreeEntry::contains(const ExtendedMBR& queryMBR) const
{
    return getMBR().contains(queryMBR);
}

double RTreeEntry::distance(const ExtendedMBR& queryMBR) const
{
    return getMBR().distance(queryMBR);
}

double RTreeEntry::enlargement(const ExtendedMBR& newMBR) const
{
    return getMBR().enlargement(newMBR);
}

bool RTreeEntry::isValid() const
{
    if (!entry) return false;
    
    ExtendedMBR mbr = getMBR();
    if (!mbr.isValid()) return false;
    
    if (entry->type < RTREE_ENTRY_INTERNAL || entry->type > RTREE_ENTRY_OVERFLOW) {
        return false;
    }
    
    if (entry->entry_length < sizeof(rtree_entry) - 1) {
        return false;
    }
    
    return true;
}

void RTreeEntry::allocateEntry(ULONG geometrySize)
{
    ULONG totalSize = sizeof(rtree_entry) - 1 + geometrySize;
    entry = (rtree_entry*)pool.allocate(totalSize);
    memset(entry, 0, totalSize);
    ownsData = true;
}

void RTreeEntry::updateLength()
{
    if (entry) {
        entry->entry_length = calculateStorageSize();
    }
}

//============================================================================
// RTreePage Implementation
//============================================================================

RTreePage::RTreePage(MemoryPool& p) 
    : page(nullptr), pool(p), ownsData(true), entries(p), entriesLoaded(false)
{
    page = (rtree_page*)pool.allocate(RTREE_PAGE_SIZE);
    initializePage();
}

RTreePage::RTreePage(rtree_page* pageData, MemoryPool& p, bool owns)
    : page(pageData), pool(p), ownsData(owns), entries(p), entriesLoaded(false)
{
}

RTreePage::~RTreePage()
{
    if (ownsData && page) {
        // Memory managed by pool - will be cleaned up automatically
        page = nullptr;
    }
}

ExtendedMBR RTreePage::getPageMBR() const
{
    if (!page) return ExtendedMBR();
    
    return ExtendedMBR(page->header.page_mbr_min_x, page->header.page_mbr_min_y,
                      page->header.page_mbr_max_x, page->header.page_mbr_max_y);
}

void RTreePage::setPageMBR(const ExtendedMBR& mbr)
{
    if (!page) return;
    
    page->header.page_mbr_min_x = mbr.minX;
    page->header.page_mbr_min_y = mbr.minY;
    page->header.page_mbr_max_x = mbr.maxX;
    page->header.page_mbr_max_y = mbr.maxY;
}

void RTreePage::updatePageMBR()
{
    if (!entriesLoaded) {
        loadEntries();
    }
    
    if (entries.getCount() == 0) {
        setPageMBR(ExtendedMBR()); // Empty MBR
        return;
    }
    
    ExtendedMBR pageMBR = entries[0].getMBR();
    for (ULONG i = 1; i < entries.getCount(); i++) {
        pageMBR.expand(entries[i].getMBR());
    }
    
    setPageMBR(pageMBR);
}

void RTreePage::loadEntries()
{
    if (entriesLoaded || !page) return;
    
    entries.clear();
    
    const UCHAR* entryData = page->entry_data;
    ULONG offset = 0;
    
    for (USHORT i = 0; i < page->header.rtree_entry_count; i++) {
        if (offset >= RTREE_PAGE_SIZE - RTREE_HEADER_SIZE) break;
        
        rtree_entry* entry = (rtree_entry*)(entryData + offset);
        RTreeEntry rtreeEntry(entry, pool, false);
        entries.add(rtreeEntry);
        
        offset += entry->entry_length;
    }
    
    entriesLoaded = true;
}

const ObjectsArray<RTreeEntry>& RTreePage::getEntries()
{
    if (!entriesLoaded) {
        loadEntries();
    }
    return entries;
}

RTreeEntry* RTreePage::getEntry(USHORT index)
{
    if (!entriesLoaded) {
        loadEntries();
    }
    
    if (index >= entries.getCount()) {
        return nullptr;
    }
    
    return &entries[index];
}

bool RTreePage::addEntry(RTreeEntry* entry)
{
    if (!entry || !hasSpace(*entry)) {
        return false;
    }
    
    if (!entriesLoaded) {
        loadEntries();
    }
    
    entries.add(*entry);
    page->header.rtree_entry_count++;
    
    // Update free space
    USHORT requiredSpace = calculateRequiredSpace(*entry);
    page->header.rtree_free_space -= requiredSpace;
    
    updatePageMBR();
    updateStatistics();
    setFlag(RTREE_PAGE_DIRTY);
    
    return true;
}

bool RTreePage::removeEntry(USHORT index)
{
    if (!entriesLoaded) {
        loadEntries();
    }
    
    if (index >= entries.getCount()) {
        return false;
    }
    
    RTreeEntry& entry = entries[index];
    USHORT entrySpace = calculateRequiredSpace(entry);
    
    entries.remove(index);
    page->header.rtree_entry_count--;
    page->header.rtree_free_space += entrySpace;
    
    updatePageMBR();
    updateStatistics();
    setFlag(RTREE_PAGE_DIRTY);
    
    return true;
}

void RTreePage::removeAllEntries()
{
    entries.clear();
    page->header.rtree_entry_count = 0;
    page->header.rtree_free_space = RTREE_PAGE_SIZE - RTREE_HEADER_SIZE;
    
    // Reset MBR
    setPageMBR(ExtendedMBR());
    
    updateStatistics();
    setFlag(RTREE_PAGE_DIRTY);
}

bool RTreePage::hasSpace(const RTreeEntry& entry) const
{
    USHORT requiredSpace = calculateRequiredSpace(entry);
    return page->header.rtree_free_space >= requiredSpace;
}

USHORT RTreePage::calculateRequiredSpace(const RTreeEntry& entry) const
{
    return entry.calculateStorageSize() + RTREE_ENTRY_OVERHEAD;
}

std::vector<USHORT> RTreePage::findIntersecting(const ExtendedMBR& queryMBR) const
{
    std::vector<USHORT> results;
    
    if (!entriesLoaded) {
        const_cast<RTreePage*>(this)->loadEntries();
    }
    
    for (ULONG i = 0; i < entries.getCount(); i++) {
        if (entries[i].intersects(queryMBR)) {
            results.push_back(i);
        }
    }
    
    return results;
}

std::vector<USHORT> RTreePage::findContained(const ExtendedMBR& queryMBR) const
{
    std::vector<USHORT> results;
    
    if (!entriesLoaded) {
        const_cast<RTreePage*>(this)->loadEntries();
    }
    
    for (ULONG i = 0; i < entries.getCount(); i++) {
        if (queryMBR.contains(entries[i].getMBR())) {
            results.push_back(i);
        }
    }
    
    return results;
}

std::vector<USHORT> RTreePage::findContaining(const ExtendedMBR& queryMBR) const
{
    std::vector<USHORT> results;
    
    if (!entriesLoaded) {
        const_cast<RTreePage*>(this)->loadEntries();
    }
    
    for (ULONG i = 0; i < entries.getCount(); i++) {
        if (entries[i].contains(queryMBR)) {
            results.push_back(i);
        }
    }
    
    return results;
}

std::pair<RTreePage*, RTreePage*> RTreePage::split(RTreeSplitStrategy strategy)
{
    switch (strategy) {
        case RTREE_SPLIT_LINEAR:
            return linearSplit();
        case RTREE_SPLIT_QUADRATIC:
            return quadraticSplit();
        case RTREE_SPLIT_R_STAR:
            return rStarSplit();
        default:
            return quadraticSplit(); // Default to quadratic
    }
}

std::pair<RTreePage*, RTreePage*> RTreePage::quadraticSplit()
{
    if (!entriesLoaded) {
        loadEntries();
    }
    
    // Create two new pages
    RTreePage* page1 = FB_NEW_POOL(pool) RTreePage(pool);
    RTreePage* page2 = FB_NEW_POOL(pool) RTreePage(pool);
    
    page1->setLevel(getLevel());
    page2->setLevel(getLevel());
    page1->setSRID(getSRID());
    page2->setSRID(getSRID());
    page1->setDimensions(getDimensions());
    page2->setDimensions(getDimensions());
    
    // Pick seeds using quadratic method
    std::pair<USHORT, USHORT> seeds = pickSeeds(RTREE_SPLIT_QUADRATIC);
    
    // Add seeds to respective pages
    page1->addEntry(&entries[seeds.first]);
    page2->addEntry(&entries[seeds.second]);
    
    // Assign remaining entries
    std::vector<USHORT> assigned(entries.getCount(), 0); // 0 = unassigned, 1 = page1, 2 = page2
    assigned[seeds.first] = 1;
    assigned[seeds.second] = 2;
    
    for (ULONG i = 0; i < entries.getCount(); i++) {
        if (assigned[i] != 0) continue; // Already assigned
        
        ExtendedMBR mbr1 = page1->getPageMBR();
        ExtendedMBR mbr2 = page2->getPageMBR();
        
        double enlargement1 = mbr1.enlargement(entries[i].getMBR());
        double enlargement2 = mbr2.enlargement(entries[i].getMBR());
        
        if (enlargement1 < enlargement2) {
            page1->addEntry(&entries[i]);
            assigned[i] = 1;
        } else if (enlargement2 < enlargement1) {
            page2->addEntry(&entries[i]);
            assigned[i] = 2;
        } else {
            // Tie: choose page with smaller area
            if (mbr1.area() <= mbr2.area()) {
                page1->addEntry(&entries[i]);
                assigned[i] = 1;
            } else {
                page2->addEntry(&entries[i]);
                assigned[i] = 2;
            }
        }
    }
    
    return std::make_pair(page1, page2);
}

std::pair<RTreePage*, RTreePage*> RTreePage::linearSplit()
{
    // Simplified linear split implementation
    return quadraticSplit(); // Fall back to quadratic for now
}

std::pair<RTreePage*, RTreePage*> RTreePage::rStarSplit()
{
    // Simplified R*-Tree split implementation
    return quadraticSplit(); // Fall back to quadratic for now
}

std::pair<USHORT, USHORT> RTreePage::pickSeeds(RTreeSplitStrategy strategy) const
{
    if (entries.getCount() < 2) {
        return std::make_pair(0, 0);
    }
    
    double maxWaste = -1.0;
    USHORT seed1 = 0, seed2 = 1;
    
    // Find the pair that would waste the most area if grouped together
    for (ULONG i = 0; i < entries.getCount(); i++) {
        for (ULONG j = i + 1; j < entries.getCount(); j++) {
            ExtendedMBR mbr1 = entries[i].getMBR();
            ExtendedMBR mbr2 = entries[j].getMBR();
            ExtendedMBR combined = mbr1 + mbr2;
            
            double waste = combined.area() - mbr1.area() - mbr2.area();
            
            if (waste > maxWaste) {
                maxWaste = waste;
                seed1 = i;
                seed2 = j;
            }
        }
    }
    
    return std::make_pair(seed1, seed2);
}

bool RTreePage::isValid() const
{
    if (!page) return false;
    
    // Check basic page structure
    if (page->header.rtree_entry_count > RTREE_MAX_ENTRIES_PER_PAGE) {
        return false;
    }
    
    // Check MBR validity
    ExtendedMBR pageMBR = getPageMBR();
    if (!pageMBR.isValid()) {
        return false;
    }
    
    // Check level constraints
    if (page->header.rtree_level > RTREE_MAX_LEVELS) {
        return false;
    }
    
    return true;
}

void RTreePage::initializePage()
{
    if (!page) return;
    
    memset(page, 0, RTREE_PAGE_SIZE);
    
    // Initialize page header
    page->header.page_header.pag_type = isLeaf() ? pag_rtree_leaf : pag_rtree_internal;
    page->header.page_header.pag_flags = 0;
    page->header.page_header.pag_checksum = 0;
    
    // Initialize R-Tree specific fields
    page->header.rtree_level = 0;
    page->header.rtree_entry_count = 0;
    page->header.rtree_free_space = RTREE_PAGE_SIZE - RTREE_HEADER_SIZE;
    page->header.rtree_split_strategy = RTREE_SPLIT_QUADRATIC;
    
    page->header.rtree_parent_page = 0;
    page->header.rtree_left_sibling = 0;
    page->header.rtree_right_sibling = 0;
    
    // Initialize page MBR to empty
    setPageMBR(ExtendedMBR());
    
    // Initialize statistics
    page->header.rtree_entry_insertions = 0;
    page->header.rtree_entry_deletions = 0;
    page->header.rtree_total_area = 0.0;
    page->header.rtree_total_overlap = 0.0;
    
    // Initialize coordinate system
    page->header.coordinate_srid = DEFAULT_SRID;
    page->header.coordinate_dimensions = 2;
    
    page->header.rtree_flags = 0;
}

void RTreePage::updateStatistics()
{
    if (!page) return;
    
    if (!entriesLoaded) {
        loadEntries();
    }
    
    // Calculate total area
    double totalArea = 0.0;
    for (ULONG i = 0; i < entries.getCount(); i++) {
        totalArea += entries[i].getMBR().area();
    }
    page->header.rtree_total_area = totalArea;
    
    // Calculate total overlap
    double totalOverlap = 0.0;
    for (ULONG i = 0; i < entries.getCount(); i++) {
        for (ULONG j = i + 1; j < entries.getCount(); j++) {
            totalOverlap += entries[i].getMBR().overlapArea(entries[j].getMBR());
        }
    }
    page->header.rtree_total_overlap = totalOverlap;
}

//============================================================================
// RTreePageFactory Implementation
//============================================================================

RTreePageFactory::RTreePageFactory(MemoryPool& p) : pool(p)
{
}

RTreePage* RTreePageFactory::createLeafPage(SRID srid, USHORT dimensions)
{
    RTreePage* page = FB_NEW_POOL(pool) RTreePage(pool);
    page->setLevel(0);
    page->setSRID(srid);
    page->setDimensions(dimensions);
    page->setFlag(RTREE_PAGE_LEAF);
    return page;
}

RTreePage* RTreePageFactory::createInternalPage(USHORT level, SRID srid, USHORT dimensions)
{
    RTreePage* page = FB_NEW_POOL(pool) RTreePage(pool);
    page->setLevel(level);
    page->setSRID(srid);
    page->setDimensions(dimensions);
    return page;
}

RTreePage* RTreePageFactory::createRootPage(SRID srid, USHORT dimensions)
{
    RTreePage* page = createLeafPage(srid, dimensions); // Root starts as leaf
    page->setFlag(RTREE_PAGE_ROOT);
    return page;
}

RTreeEntry* RTreePageFactory::createLeafEntry(const ExtendedMBR& mbr, RecordNumber recordNum, const Geometry* geometry)
{
    RTreeEntry* entry = FB_NEW_POOL(pool) RTreeEntry(mbr, RTREE_ENTRY_LEAF, pool);
    entry->setRecordNumber(recordNum);
    
    if (geometry) {
        entry->setGeometry(*geometry);
    }
    
    return entry;
}

RTreeEntry* RTreePageFactory::createInternalEntry(const ExtendedMBR& mbr, ULONG childPage)
{
    RTreeEntry* entry = FB_NEW_POOL(pool) RTreeEntry(mbr, RTREE_ENTRY_INTERNAL, pool);
    entry->setChildPage(childPage);
    return entry;
}

//============================================================================
// RTreePageCache Implementation
//============================================================================

RTreePageCache::RTreePageCache(ULONG maxSize, MemoryPool& p) 
    : maxCacheSize(maxSize), accessCounter(0), pool(p), 
      totalRequests(0), cacheHits(0), cacheMisses(0)
{
    cache.reserve(maxSize);
}

RTreePageCache::~RTreePageCache()
{
    clear();
}

RTreePage* RTreePageCache::get(ULONG pageNumber)
{
    totalRequests++;
    
    CacheEntry* entry = findEntry(pageNumber);
    if (entry) {
        updateAccessTime(*entry);
        cacheHits++;
        return entry->page;
    }
    
    cacheMisses++;
    return nullptr;
}

void RTreePageCache::put(ULONG pageNumber, RTreePage* page)
{
    if (!page) return;
    
    // Check if already cached
    CacheEntry* existing = findEntry(pageNumber);
    if (existing) {
        existing->page = page;
        updateAccessTime(*existing);
        return;
    }
    
    // Evict if cache is full
    if (cache.size() >= maxCacheSize) {
        evictLRU();
    }
    
    // Add new entry
    CacheEntry newEntry;
    newEntry.pageNumber = pageNumber;
    newEntry.page = page;
    newEntry.lastAccess = ++accessCounter;
    newEntry.dirty = false;
    newEntry.accessCount = 1;
    
    cache.push_back(newEntry);
}

void RTreePageCache::remove(ULONG pageNumber)
{
    auto it = std::find_if(cache.begin(), cache.end(),
        [pageNumber](const CacheEntry& entry) {
            return entry.pageNumber == pageNumber;
        });
    
    if (it != cache.end()) {
        cache.erase(it);
    }
}

void RTreePageCache::clear()
{
    cache.clear();
    accessCounter = 0;
    totalRequests = 0;
    cacheHits = 0;
    cacheMisses = 0;
}

double RTreePageCache::getHitRate() const
{
    if (totalRequests == 0) return 0.0;
    return static_cast<double>(cacheHits) / totalRequests;
}

RTreePageCache::CacheEntry* RTreePageCache::findEntry(ULONG pageNumber)
{
    for (CacheEntry& entry : cache) {
        if (entry.pageNumber == pageNumber) {
            return &entry;
        }
    }
    return nullptr;
}

void RTreePageCache::updateAccessTime(CacheEntry& entry)
{
    entry.lastAccess = ++accessCounter;
    entry.accessCount++;
}

void RTreePageCache::evictLRU()
{
    if (cache.empty()) return;
    
    CacheEntry* lru = findLRUEntry();
    if (lru) {
        auto it = std::find_if(cache.begin(), cache.end(),
            [lru](const CacheEntry& entry) {
                return &entry == lru;
            });
        
        if (it != cache.end()) {
            cache.erase(it);
        }
    }
}

RTreePageCache::CacheEntry* RTreePageCache::findLRUEntry()
{
    if (cache.empty()) return nullptr;
    
    CacheEntry* lru = &cache[0];
    for (CacheEntry& entry : cache) {
        if (entry.lastAccess < lru->lastAccess) {
            lru = &entry;
        }
    }
    
    return lru;
}

//============================================================================
// FileRTreeStorageManager Implementation
//============================================================================

FileRTreeStorageManager::FileRTreeStorageManager(MemoryPool& p, ULONG cacheSize)
    : pool(p), pageCache(nullptr), nextPageNumber(1)
{
    pageCache = FB_NEW_POOL(pool) RTreePageCache(cacheSize, pool);
}

FileRTreeStorageManager::~FileRTreeStorageManager()
{
    if (pageCache) {
        pageCache->flush();
        delete pageCache;
    }
}

RTreePage* FileRTreeStorageManager::readPage(ULONG pageNumber)
{
    // Try cache first
    if (pageCache) {
        RTreePage* cachedPage = pageCache->get(pageNumber);
        if (cachedPage) {
            return cachedPage;
        }
    }
    
    // Load from disk
    RTreePage* page = loadPageFromDisk(pageNumber);
    
    // Cache the page
    if (page && pageCache) {
        pageCache->put(pageNumber, page);
    }
    
    return page;
}

void FileRTreeStorageManager::writePage(ULONG pageNumber, RTreePage* page)
{
    if (!page) return;
    
    // Update cache
    if (pageCache) {
        pageCache->put(pageNumber, page);
        pageCache->markDirty(pageNumber);
    }
    
    // Write to disk
    savePageToDisk(pageNumber, page);
}

ULONG FileRTreeStorageManager::allocatePage()
{
    if (!freePages.empty()) {
        return getFromFreeList();
    }
    
    return nextPageNumber++;
}

void FileRTreeStorageManager::deallocatePage(ULONG pageNumber)
{
    addToFreeList(pageNumber);
    
    // Remove from cache
    if (pageCache) {
        pageCache->remove(pageNumber);
    }
}

std::vector<RTreePage*> FileRTreeStorageManager::readPages(const std::vector<ULONG>& pageNumbers)
{
    std::vector<RTreePage*> pages;
    pages.reserve(pageNumbers.size());
    
    for (ULONG pageNumber : pageNumbers) {
        pages.push_back(readPage(pageNumber));
    }
    
    return pages;
}

void FileRTreeStorageManager::writePages(const std::map<ULONG, RTreePage*>& pages)
{
    for (const auto& pair : pages) {
        writePage(pair.first, pair.second);
    }
}

RTreePage* FileRTreeStorageManager::loadPageFromDisk(ULONG pageNumber)
{
    // Simplified implementation - in practice would read from actual file
    RTreePage* page = FB_NEW_POOL(pool) RTreePage(pool);
    return page;
}

void FileRTreeStorageManager::savePageToDisk(ULONG pageNumber, RTreePage* page)
{
    // Simplified implementation - in practice would write to actual file
}

void FileRTreeStorageManager::addToFreeList(ULONG pageNumber)
{
    if (std::find(freePages.begin(), freePages.end(), pageNumber) == freePages.end()) {
        freePages.push_back(pageNumber);
    }
}

ULONG FileRTreeStorageManager::getFromFreeList()
{
    if (freePages.empty()) return 0;
    
    ULONG pageNumber = freePages.back();
    freePages.pop_back();
    return pageNumber;
}

void FileRTreeStorageManager::defragment()
{
    // Implementation would defragment storage
}

void FileRTreeStorageManager::compact()
{
    // Implementation would compact storage
}

double FileRTreeStorageManager::getFragmentationRatio() const
{
    if (nextPageNumber <= 1) return 0.0;
    return static_cast<double>(freePages.size()) / (nextPageNumber - 1);
}

//============================================================================
// RTreePageUtils Implementation
//============================================================================

namespace RTreePageUtils
{
    bool isPageValid(const RTreePage& page)
    {
        return page.isValid();
    }
    
    double calculatePageQuality(const RTreePage& page)
    {
        // Simple quality metric based on area coverage and overlap
        double quality = 1.0;
        
        // Penalize for overlap
        double totalOverlap = page.getTotalOverlap();
        double totalArea = page.getTotalArea();
        
        if (totalArea > 0) {
            double overlapRatio = totalOverlap / totalArea;
            quality -= overlapRatio * 0.5; // Up to 50% penalty
        }
        
        // Penalize for poor space utilization
        double spaceUtilization = static_cast<double>(page.getEntryCount()) / RTREE_MAX_ENTRIES_PER_PAGE;
        if (spaceUtilization < 0.5) {
            quality -= (0.5 - spaceUtilization) * 0.3; // Up to 15% penalty
        }
        
        return std::max(0.0, quality);
    }
    
    string generatePageReport(const RTreePage& page)
    {
        std::ostringstream report;
        
        report << "R-Tree Page Report\n";
        report << "==================\n";
        report << "Level: " << page.getLevel() << "\n";
        report << "Entry Count: " << page.getEntryCount() << "\n";
        report << "Free Space: " << page.getFreeSpace() << " bytes\n";
        report << "Is Leaf: " << (page.isLeaf() ? "Yes" : "No") << "\n";
        report << "Is Root: " << (page.isRoot() ? "Yes" : "No") << "\n";
        report << "SRID: " << page.getSRID() << "\n";
        report << "Dimensions: " << page.getDimensions() << "\n";
        
        ExtendedMBR mbr = page.getPageMBR();
        report << "Page MBR: (" << std::fixed << std::setprecision(6)
               << mbr.minX << "," << mbr.minY << "," << mbr.maxX << "," << mbr.maxY << ")\n";
        
        report << "Total Area: " << page.getTotalArea() << "\n";
        report << "Total Overlap: " << page.getTotalOverlap() << "\n";
        report << "Quality Score: " << calculatePageQuality(page) << "\n";
        
        return report.str();
    }
    
    ULONG estimatePageMemoryUsage(USHORT entryCount, USHORT averageGeometrySize)
    {
        ULONG basePageSize = sizeof(rtree_page_header);
        ULONG entrySize = sizeof(rtree_entry) + averageGeometrySize;
        
        return basePageSize + (entryCount * entrySize);
    }
    
    ULONG estimateTreeMemoryUsage(ULONG totalEntries, USHORT averageGeometrySize, USHORT fanout)
    {
        if (totalEntries == 0 || fanout == 0) return 0;
        
        ULONG leafPages = (totalEntries + fanout - 1) / fanout; // Ceiling division
        ULONG internalEntries = leafPages;
        ULONG internalPages = 0;
        
        // Calculate internal pages (geometric series)
        while (internalEntries > 1) {
            internalPages += (internalEntries + fanout - 1) / fanout;
            internalEntries = (internalEntries + fanout - 1) / fanout;
        }
        
        ULONG totalPages = leafPages + internalPages;
        ULONG averagePageSize = estimatePageMemoryUsage(fanout, averageGeometrySize);
        
        return totalPages * averagePageSize;
    }
}

//============================================================================
// Static utility functions
//============================================================================

RTreeEntry* RTreeEntry::deserialize(const UCHAR* buffer, MemoryPool& pool)
{
    if (!buffer) return nullptr;
    
    const rtree_entry* entryData = reinterpret_cast<const rtree_entry*>(buffer);
    
    // Allocate memory for the entry
    ULONG totalSize = entryData->entry_length;
    rtree_entry* newEntry = (rtree_entry*)pool.allocate(totalSize);
    memcpy(newEntry, buffer, totalSize);
    
    return FB_NEW_POOL(pool) RTreeEntry(newEntry, pool, true);
}

void RTreeEntry::serialize(UCHAR* buffer) const
{
    if (!entry || !buffer) return;
    
    memcpy(buffer, entry, entry->entry_length);
}