#include "RTreeAlgorithms.h"
#include "RTreeIndex.h"
#include "common/StatusArg.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <stack>
#include <random>
#include <chrono>

using namespace ScratchBird;

//============================================================================
// RTreeInsertContext Implementation
//============================================================================

RTreeInsertContext::RTreeInsertContext(RTreeIndex& idx, MemoryPool& p)
    : index(idx), pool(p), needsSplit(false)
{
    pathPages.reserve(RTREE_MAX_LEVELS);
    pathIndices.reserve(RTREE_MAX_LEVELS);
}

RTreeInsertContext::~RTreeInsertContext()
{
}

void RTreeInsertContext::addToPath(ULONG pageNumber, USHORT entryIndex)
{
    pathPages.push_back(pageNumber);
    pathIndices.push_back(entryIndex);
}

void RTreeInsertContext::clearPath()
{
    pathPages.clear();
    pathIndices.clear();
    needsSplit = false;
}

void RTreeInsertContext::propagateSplit(RTreePage* newPage1, RTreePage* newPage2, USHORT level)
{
    if (pathPages.empty()) {
        // Need to create new root
        ULONG newRootPage = index.createNewRoot(newPage1, newPage2);
        return;
    }
    
    // Get parent page
    ULONG parentPageNum = pathPages[pathPages.size() - 1];
    RTreePage* parentPage = index.readPage(parentPageNum);
    
    if (!parentPage) return;
    
    // Update parent entry for first page
    USHORT parentEntryIndex = pathIndices[pathIndices.size() - 1];
    RTreeEntry* parentEntry = parentPage->getEntry(parentEntryIndex);
    if (parentEntry) {
        parentEntry->setMBR(newPage1->getPageMBR());
    }
    
    // Add new entry for second page
    RTreeEntry* newEntry = index.getPageFactory().createInternalEntry(
        newPage2->getPageMBR(), index.getPageNumber(newPage2));
    
    if (parentPage->hasSpace(*newEntry)) {
        parentPage->addEntry(newEntry);
    } else {
        // Parent needs to split too
        needsSplit = true;
        // Remove current level from path and propagate up
        pathPages.pop_back();
        pathIndices.pop_back();
        
        // Split parent and propagate recursively
        std::pair<RTreePage*, RTreePage*> splitPages = parentPage->split();
        propagateSplit(splitPages.first, splitPages.second, level + 1);
    }
}

void RTreeInsertContext::updateMBRs()
{
    // Update MBRs from leaf to root
    for (int i = pathPages.size() - 1; i >= 0; i--) {
        RTreePage* page = index.readPage(pathPages[i]);
        if (page) {
            page->updatePageMBR();
            index.writePage(pathPages[i], page);
        }
    }
}

void RTreeInsertContext::adjustTree()
{
    updateMBRs();
    
    if (needsSplit) {
        // Handle any pending splits
        needsSplit = false;
    }
}

//============================================================================
// RTreeDeleteContext Implementation
//============================================================================

RTreeDeleteContext::RTreeDeleteContext(RTreeIndex& idx, MemoryPool& p)
    : index(idx), pool(p)
{
    underflowPages.reserve(RTREE_MAX_LEVELS);
    orphanedEntries.reserve(RTREE_MAX_ENTRIES_PER_PAGE);
}

RTreeDeleteContext::~RTreeDeleteContext()
{
    // Clean up orphaned entries
    for (RTreeEntry* entry : orphanedEntries) {
        delete entry;
    }
}

void RTreeDeleteContext::addUnderflowPage(ULONG pageNumber)
{
    underflowPages.push_back(pageNumber);
}

void RTreeDeleteContext::addOrphanedEntry(RTreeEntry* entry)
{
    if (entry) {
        orphanedEntries.push_back(entry);
    }
}

void RTreeDeleteContext::handleUnderflow()
{
    for (ULONG pageNumber : underflowPages) {
        RTreePage* page = index.readPage(pageNumber);
        if (!page) continue;
        
        // Try to merge with sibling or redistribute entries
        ULONG siblingPage = page->getLeftSibling();
        if (siblingPage == 0) {
            siblingPage = page->getRightSibling();
        }
        
        if (siblingPage != 0) {
            RTreePage* sibling = index.readPage(siblingPage);
            if (sibling && sibling->getEntryCount() + page->getEntryCount() <= RTREE_MAX_ENTRIES_PER_PAGE) {
                // Merge pages
                const ObjectsArray<RTreeEntry>& entries = page->getEntries();
                for (ULONG i = 0; i < entries.getCount(); i++) {
                    sibling->addEntry(const_cast<RTreeEntry*>(&entries[i]));
                }
                
                // Deallocate the underflow page
                index.deallocatePage(pageNumber);
            }
        }
    }
}

void RTreeDeleteContext::reinsertOrphans()
{
    RTreeAlgorithms algorithms(index, pool);
    
    for (RTreeEntry* entry : orphanedEntries) {
        if (entry->getType() == RTREE_ENTRY_LEAF) {
            algorithms.insert(entry->getMBR(), entry->getRecordNumber());
        } else {
            // Handle internal entries - would need more complex logic
        }
    }
    
    // Clear orphaned entries after reinsertion
    orphanedEntries.clear();
}

void RTreeDeleteContext::condenseTree()
{
    handleUnderflow();
    reinsertOrphans();
}

//============================================================================
// RTreeAlgorithms Implementation
//============================================================================

RTreeAlgorithms::RTreeAlgorithms(RTreeIndex& idx, MemoryPool& p)
    : index(idx), pool(p)
{
}

RTreeAlgorithms::~RTreeAlgorithms()
{
}

bool RTreeAlgorithms::insert(const ExtendedMBR& mbr, RecordNumber recordNumber, const Geometry* geometry)
{
    RTreeInsertContext context(index, pool);
    
    try {
        // Choose leaf node for insertion
        ULONG leafPageNum = chooseLeaf(mbr);
        RTreePage* leafPage = index.readPage(leafPageNum);
        
        if (!leafPage) return false;
        
        // Insert into leaf
        bool success = insertIntoLeaf(leafPage, mbr, recordNumber, geometry);
        
        if (success) {
            // Adjust tree structure if needed
            adjustTree(context);
        }
        
        return success;
        
    } catch (...) {
        return false;
    }
}

bool RTreeAlgorithms::remove(const ExtendedMBR& mbr, RecordNumber recordNumber)
{
    RTreeDeleteContext context(index, pool);
    
    try {
        // Find leaf containing the entry
        RTreePage* leafPage = findLeaf(mbr, recordNumber);
        if (!leafPage) return false;
        
        // Find and remove the entry
        const ObjectsArray<RTreeEntry>& entries = leafPage->getEntries();
        for (USHORT i = 0; i < entries.getCount(); i++) {
            const RTreeEntry& entry = entries[i];
            if (entry.getRecordNumber() == recordNumber && entry.getMBR().equals(mbr)) {
                leafPage->removeEntry(i);
                
                // Handle underflow if necessary
                if (leafPage->getEntryCount() < RTREE_MIN_ENTRIES_PER_PAGE) {
                    ULONG leafPageNum = index.getPageNumber(leafPage);
                    condenseTree(context, leafPageNum);
                }
                
                return true;
            }
        }
        
        return false;
        
    } catch (...) {
        return false;
    }
}

std::vector<RTreeSearchResult> RTreeAlgorithms::search(const RTreeSearchParams& params)
{
    std::vector<RTreeSearchResult> results;
    
    try {
        switch (params.queryType) {
            case RTREE_QUERY_INTERSECTS:
                return intersectionSearch(params.queryMBR);
                
            case RTREE_QUERY_CONTAINS:
                return containmentSearch(params.queryMBR);
                
            case RTREE_QUERY_WITHIN_DISTANCE:
                return withinDistanceSearch(params.queryMBR, params.maxDistance);
                
            case RTREE_QUERY_KNN:
                return kNearestNeighbors(params.queryMBR, params.k);
                
            default:
                searchRecursive(index.getRootPage(), params, results);
                break;
        }
        
        // Apply result limits
        if (params.maxResults > 0 && results.size() > params.maxResults) {
            results.resize(params.maxResults);
        }
        
        // Sort by distance if requested
        if (params.sortByDistance) {
            std::sort(results.begin(), results.end(),
                [](const RTreeSearchResult& a, const RTreeSearchResult& b) {
                    return a.distance < b.distance;
                });
        }
        
    } catch (...) {
        results.clear();
    }
    
    return results;
}

std::vector<RTreeSearchResult> RTreeAlgorithms::intersectionSearch(const ExtendedMBR& queryMBR)
{
    std::vector<RTreeSearchResult> results;
    
    RTreeSearchParams params;
    params.queryType = RTREE_QUERY_INTERSECTS;
    params.queryMBR = queryMBR;
    
    searchRecursive(index.getRootPage(), params, results);
    
    return results;
}

std::vector<RTreeSearchResult> RTreeAlgorithms::containmentSearch(const ExtendedMBR& queryMBR)
{
    std::vector<RTreeSearchResult> results;
    
    RTreeSearchParams params;
    params.queryType = RTREE_QUERY_CONTAINS;
    params.queryMBR = queryMBR;
    
    searchRecursive(index.getRootPage(), params, results);
    
    return results;
}

std::vector<RTreeSearchResult> RTreeAlgorithms::withinDistanceSearch(const ExtendedMBR& queryMBR, double maxDistance)
{
    std::vector<RTreeSearchResult> results;
    
    RTreeSearchParams params;
    params.queryType = RTREE_QUERY_WITHIN_DISTANCE;
    params.queryMBR = queryMBR;
    params.maxDistance = maxDistance;
    
    searchRecursive(index.getRootPage(), params, results);
    
    return results;
}

std::vector<RTreeSearchResult> RTreeAlgorithms::kNearestNeighbors(const ExtendedMBR& queryMBR, ULONG k)
{
    std::vector<RTreeSearchResult> results;
    std::priority_queue<std::pair<double, RTreeSearchResult>> knnQueue;
    
    knnSearchRecursive(index.getRootPage(), queryMBR, k, knnQueue);
    
    // Extract results from priority queue
    while (!knnQueue.empty() && results.size() < k) {
        results.push_back(knnQueue.top().second);
        knnQueue.pop();
    }
    
    // Reverse to get closest first
    std::reverse(results.begin(), results.end());
    
    return results;
}

bool RTreeAlgorithms::bulkInsert(const std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries)
{
    try {
        RTreeBulkLoader loader(index, pool);
        std::vector<std::pair<ExtendedMBR, RecordNumber>> sortedEntries = entries;
        
        return loader.loadFromUnsortedData(sortedEntries);
        
    } catch (...) {
        return false;
    }
}

bool RTreeAlgorithms::bulkDelete(const std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries)
{
    bool allSuccess = true;
    
    for (const auto& entry : entries) {
        if (!remove(entry.first, entry.second)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

void RTreeAlgorithms::rebuild()
{
    try {
        // Collect all entries
        std::vector<std::pair<ExtendedMBR, RecordNumber>> allEntries;
        collectSubtreeEntries(index.getRootPage(), allEntries);
        
        // Clear the index
        index.clear();
        
        // Bulk load with collected entries
        bulkInsert(allEntries);
        
    } catch (...) {
        // Handle rebuild failure
    }
}

void RTreeAlgorithms::optimize()
{
    try {
        TreeStatistics stats = getStatistics();
        
        // Decide if rebuild is beneficial
        if (stats.averagePageUtilization < 0.5 || stats.totalOverlap > stats.totalArea * 0.3) {
            rebuild();
        } else {
            // Perform incremental optimization
            rebalanceSubtree(index.getRootPage());
        }
        
    } catch (...) {
        // Handle optimization failure
    }
}

double RTreeAlgorithms::calculateTreeQuality()
{
    TreeStatistics stats = getStatistics();
    
    double quality = 1.0;
    
    // Penalize for poor space utilization
    if (stats.averagePageUtilization < 0.7) {
        quality -= (0.7 - stats.averagePageUtilization) * 0.5;
    }
    
    // Penalize for high overlap
    if (stats.totalArea > 0) {
        double overlapRatio = stats.totalOverlap / stats.totalArea;
        quality -= overlapRatio * 0.4;
    }
    
    // Penalize for unbalanced tree
    if (stats.maxLevel > 0) {
        double balanceRatio = stats.averageFanout / RTREE_FANOUT_TARGET;
        if (balanceRatio < 0.5 || balanceRatio > 2.0) {
            quality -= 0.2;
        }
    }
    
    return std::max(0.0, quality);
}

void RTreeAlgorithms::balanceTree()
{
    optimize();
}

RTreeAlgorithms::TreeStatistics RTreeAlgorithms::getStatistics()
{
    TreeStatistics stats;
    memset(&stats, 0, sizeof(TreeStatistics));
    
    try {
        // Initialize counters
        std::stack<std::pair<ULONG, USHORT>> pageStack;
        pageStack.push(std::make_pair(index.getRootPage(), 0));
        
        double totalFanout = 0.0;
        ULONG internalPageCount = 0;
        
        while (!pageStack.empty()) {
            auto current = pageStack.top();
            pageStack.pop();
            
            ULONG pageNum = current.first;
            USHORT level = current.second;
            
            RTreePage* page = index.readPage(pageNum);
            if (!page) continue;
            
            stats.totalPages++;
            stats.totalEntries += page->getEntryCount();
            stats.maxLevel = std::max(stats.maxLevel, level);
            
            if (page->isLeaf()) {
                stats.leafPages++;
            } else {
                stats.internalPages++;
                totalFanout += page->getEntryCount();
                internalPageCount++;
                
                // Add child pages to stack
                const ObjectsArray<RTreeEntry>& entries = page->getEntries();
                for (ULONG i = 0; i < entries.getCount(); i++) {
                    const RTreeEntry& entry = entries[i];
                    if (entry.getType() == RTREE_ENTRY_INTERNAL) {
                        pageStack.push(std::make_pair(entry.getChildPage(), level + 1));
                    }
                }
            }
            
            // Accumulate statistics
            stats.totalArea += page->getTotalArea();
            stats.totalOverlap += page->getTotalOverlap();
            
            double utilization = static_cast<double>(page->getEntryCount()) / RTREE_MAX_ENTRIES_PER_PAGE;
            stats.averagePageUtilization += utilization;
        }
        
        // Calculate averages
        if (stats.totalPages > 0) {
            stats.averagePageUtilization /= stats.totalPages;
        }
        
        if (internalPageCount > 0) {
            stats.averageFanout = totalFanout / internalPageCount;
        }
        
        // Calculate storage efficiency
        ULONG usedSpace = stats.totalEntries * sizeof(rtree_entry);
        ULONG totalSpace = stats.totalPages * RTREE_PAGE_SIZE;
        if (totalSpace > 0) {
            stats.storageEfficiency = static_cast<double>(usedSpace) / totalSpace;
        }
        
        // Estimate average search cost (simplified)
        if (stats.maxLevel > 0) {
            stats.averageSearchCost = std::log(stats.totalEntries) / std::log(stats.averageFanout);
        }
        
    } catch (...) {
        // Return default statistics on error
    }
    
    return stats;
}

void RTreeAlgorithms::analyzePerformance(const std::vector<RTreeSearchParams>& queries, string& report)
{
    std::ostringstream oss;
    
    oss << "R-Tree Performance Analysis Report\n";
    oss << "==================================\n\n";
    
    TreeStatistics stats = getStatistics();
    
    oss << "Tree Statistics:\n";
    oss << "  Total Pages: " << stats.totalPages << "\n";
    oss << "  Leaf Pages: " << stats.leafPages << "\n";
    oss << "  Internal Pages: " << stats.internalPages << "\n";
    oss << "  Total Entries: " << stats.totalEntries << "\n";
    oss << "  Max Level: " << stats.maxLevel << "\n";
    oss << "  Average Fanout: " << std::fixed << std::setprecision(2) << stats.averageFanout << "\n";
    oss << "  Page Utilization: " << (stats.averagePageUtilization * 100) << "%\n";
    oss << "  Storage Efficiency: " << (stats.storageEfficiency * 100) << "%\n";
    oss << "  Tree Quality: " << calculateTreeQuality() << "\n\n";
    
    // Analyze queries
    if (!queries.empty()) {
        oss << "Query Analysis:\n";
        
        std::map<RTreeQueryType, ULONG> queryTypeCounts;
        double totalSelectivity = 0.0;
        
        for (const RTreeSearchParams& params : queries) {
            queryTypeCounts[params.queryType]++;
            
            // Estimate selectivity
            double queryArea = params.queryMBR.area();
            double totalArea = stats.totalArea;
            if (totalArea > 0) {
                totalSelectivity += queryArea / totalArea;
            }
        }
        
        for (const auto& pair : queryTypeCounts) {
            oss << "  Query Type " << pair.first << ": " << pair.second << " queries\n";
        }
        
        if (queries.size() > 0) {
            oss << "  Average Selectivity: " << (totalSelectivity / queries.size()) << "\n";
        }
    }
    
    report = oss.str();
}

//============================================================================
// Private Helper Methods
//============================================================================

ULONG RTreeAlgorithms::chooseLeaf(const ExtendedMBR& mbr)
{
    ULONG currentPage = index.getRootPage();
    
    while (true) {
        RTreePage* page = index.readPage(currentPage);
        if (!page) break;
        
        if (page->isLeaf()) {
            return currentPage;
        }
        
        // Choose child with minimum enlargement
        const ObjectsArray<RTreeEntry>& entries = page->getEntries();
        USHORT bestChild = selectBestChild(page, mbr);
        
        if (bestChild < entries.getCount()) {
            currentPage = entries[bestChild].getChildPage();
        } else {
            break;
        }
    }
    
    return currentPage;
}

bool RTreeAlgorithms::insertIntoLeaf(RTreePage* leafPage, const ExtendedMBR& mbr, RecordNumber recordNumber, const Geometry* geometry)
{
    if (!leafPage) return false;
    
    // Create new leaf entry
    RTreeEntry* newEntry = index.getPageFactory().createLeafEntry(mbr, recordNumber, geometry);
    
    if (leafPage->hasSpace(*newEntry)) {
        return leafPage->addEntry(newEntry);
    } else {
        // Need to split the leaf
        RTreeInsertContext context(index, pool);
        splitNode(leafPage, context);
        
        // Try to insert into one of the split pages
        // This is simplified - would need more sophisticated logic
        return leafPage->addEntry(newEntry);
    }
}

void RTreeAlgorithms::splitNode(RTreePage* page, RTreeInsertContext& context)
{
    if (!page) return;
    
    std::pair<RTreePage*, RTreePage*> splitPages = page->split();
    
    context.setNeedsSplit(true);
    context.propagateSplit(splitPages.first, splitPages.second, page->getLevel());
}

void RTreeAlgorithms::adjustTree(RTreeInsertContext& context)
{
    context.adjustTree();
}

void RTreeAlgorithms::searchRecursive(ULONG pageNumber, const RTreeSearchParams& params, std::vector<RTreeSearchResult>& results)
{
    RTreePage* page = index.readPage(pageNumber);
    if (!page) return;
    
    const ObjectsArray<RTreeEntry>& entries = page->getEntries();
    
    for (ULONG i = 0; i < entries.getCount(); i++) {
        const RTreeEntry& entry = entries[i];
        
        if (testSpatialRelation(entry.getMBR(), params)) {
            if (page->isLeaf()) {
                // Add leaf entry to results
                RTreeSearchResult result(entry.getRecordNumber(), entry.getMBR());
                if (params.queryType == RTREE_QUERY_WITHIN_DISTANCE || params.queryType == RTREE_QUERY_KNN) {
                    result.distance = entry.getMBR().distance(params.queryMBR);
                }
                if (params.loadGeometry) {
                    result.geometry = entry.getGeometry();
                }
                results.push_back(result);
            } else {
                // Recurse into child page
                searchRecursive(entry.getChildPage(), params, results);
            }
        }
    }
}

bool RTreeAlgorithms::testSpatialRelation(const ExtendedMBR& entryMBR, const RTreeSearchParams& params)
{
    switch (params.queryType) {
        case RTREE_QUERY_INTERSECTS:
            return entryMBR.intersects(params.queryMBR);
            
        case RTREE_QUERY_CONTAINS:
            return params.queryMBR.contains(entryMBR);
            
        case RTREE_QUERY_CONTAINED:
            return entryMBR.contains(params.queryMBR);
            
        case RTREE_QUERY_TOUCHES:
            return entryMBR.touchesBoundary(params.queryMBR);
            
        case RTREE_QUERY_CROSSES:
            return entryMBR.intersects(params.queryMBR) && 
                   !entryMBR.contains(params.queryMBR) && 
                   !params.queryMBR.contains(entryMBR);
            
        case RTREE_QUERY_OVERLAPS:
            return entryMBR.overlapsArea(params.queryMBR);
            
        case RTREE_QUERY_WITHIN_DISTANCE:
            return entryMBR.isWithinDistance(params.queryMBR, params.maxDistance);
            
        case RTREE_QUERY_KNN:
            return true; // KNN considers all entries
            
        default:
            return entryMBR.intersects(params.queryMBR);
    }
}

void RTreeAlgorithms::knnSearchRecursive(ULONG pageNumber, const ExtendedMBR& queryMBR, ULONG k, 
                                        std::priority_queue<std::pair<double, RTreeSearchResult>>& results)
{
    RTreePage* page = index.readPage(pageNumber);
    if (!page) return;
    
    // Priority queue for sorting children by distance
    std::priority_queue<std::pair<double, ULONG>, std::vector<std::pair<double, ULONG>>, std::greater<std::pair<double, ULONG>>> childQueue;
    
    const ObjectsArray<RTreeEntry>& entries = page->getEntries();
    
    if (page->isLeaf()) {
        // Process leaf entries
        for (ULONG i = 0; i < entries.getCount(); i++) {
            const RTreeEntry& entry = entries[i];
            double distance = entry.getMBR().distance(queryMBR);
            
            RTreeSearchResult result(entry.getRecordNumber(), entry.getMBR(), distance);
            
            if (results.size() < k) {
                results.push(std::make_pair(distance, result));
            } else if (distance < results.top().first) {
                results.pop();
                results.push(std::make_pair(distance, result));
            }
        }
    } else {
        // Add children to priority queue sorted by distance
        for (ULONG i = 0; i < entries.getCount(); i++) {
            const RTreeEntry& entry = entries[i];
            double distance = entry.getMBR().distance(queryMBR);
            childQueue.push(std::make_pair(distance, entry.getChildPage()));
        }
        
        // Process children in order of increasing distance
        while (!childQueue.empty()) {
            auto child = childQueue.top();
            childQueue.pop();
            
            // Prune if we have k results and this child is farther than the farthest result
            if (results.size() >= k && child.first > results.top().first) {
                break;
            }
            
            knnSearchRecursive(child.second, queryMBR, k, results);
        }
    }
}

RTreePage* RTreeAlgorithms::findLeaf(const ExtendedMBR& mbr, RecordNumber recordNumber)
{
    std::stack<ULONG> pageStack;
    pageStack.push(index.getRootPage());
    
    while (!pageStack.empty()) {
        ULONG pageNum = pageStack.top();
        pageStack.pop();
        
        RTreePage* page = index.readPage(pageNum);
        if (!page) continue;
        
        if (page->isLeaf()) {
            // Check if this leaf contains the target entry
            const ObjectsArray<RTreeEntry>& entries = page->getEntries();
            for (ULONG i = 0; i < entries.getCount(); i++) {
                const RTreeEntry& entry = entries[i];
                if (entry.getRecordNumber() == recordNumber && entry.getMBR().equals(mbr)) {
                    return page;
                }
            }
        } else {
            // Add intersecting children to stack
            const ObjectsArray<RTreeEntry>& entries = page->getEntries();
            for (ULONG i = 0; i < entries.getCount(); i++) {
                const RTreeEntry& entry = entries[i];
                if (entry.getMBR().intersects(mbr)) {
                    pageStack.push(entry.getChildPage());
                }
            }
        }
    }
    
    return nullptr;
}

void RTreeAlgorithms::condenseTree(RTreeDeleteContext& context, ULONG leafPage)
{
    context.condenseTree();
}

bool RTreeAlgorithms::mergeNodes(RTreePage* page1, RTreePage* page2)
{
    if (!page1 || !page2) return false;
    
    // Check if merge is possible
    if (page1->getEntryCount() + page2->getEntryCount() > RTREE_MAX_ENTRIES_PER_PAGE) {
        return false;
    }
    
    // Move all entries from page2 to page1
    const ObjectsArray<RTreeEntry>& entries = page2->getEntries();
    for (ULONG i = 0; i < entries.getCount(); i++) {
        page1->addEntry(const_cast<RTreeEntry*>(&entries[i]));
    }
    
    page2->removeAllEntries();
    return true;
}

std::pair<RTreePage*, RTreePage*> RTreeAlgorithms::linearSplit(RTreePage* page)
{
    return page->linearSplit();
}

std::pair<RTreePage*, RTreePage*> RTreeAlgorithms::quadraticSplit(RTreePage* page)
{
    return page->quadraticSplit();
}

std::pair<RTreePage*, RTreePage*> RTreeAlgorithms::rStarSplit(RTreePage* page)
{
    return page->rStarSplit();
}

void RTreeAlgorithms::updateMBR(ULONG pageNumber, USHORT entryIndex, const ExtendedMBR& newMBR)
{
    RTreePage* page = index.readPage(pageNumber);
    if (!page) return;
    
    RTreeEntry* entry = page->getEntry(entryIndex);
    if (entry) {
        entry->setMBR(newMBR);
        page->updatePageMBR();
        index.writePage(pageNumber, page);
    }
}

ULONG RTreeAlgorithms::createNewRoot(RTreePage* leftChild, RTreePage* rightChild)
{
    return index.createNewRoot(leftChild, rightChild);
}

void RTreeAlgorithms::deleteRoot()
{
    // Implementation would delete current root and promote child
}

std::vector<RTreePage*> RTreeAlgorithms::bulkLoadBottomUp(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries)
{
    RTreeBulkLoader loader(index, pool);
    loader.loadFromSortedData(entries);
    
    // Return empty vector for now - would return created pages
    return std::vector<RTreePage*>();
}

void RTreeAlgorithms::sortTileRecursive(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries, ULONG fanout)
{
    RTreeBulkLoader loader(index, pool, fanout);
    loader.sortTileRecursive(entries);
}

void RTreeAlgorithms::hilbertSort(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries)
{
    RTreeBulkLoader loader(index, pool);
    loader.sortByHilbert(entries);
}

void RTreeAlgorithms::rebalanceSubtree(ULONG rootPageNumber)
{
    // Collect all entries in subtree
    std::vector<std::pair<ExtendedMBR, RecordNumber>> entries;
    collectSubtreeEntries(rootPageNumber, entries);
    
    // Rebuild subtree with bulk loading
    if (!entries.empty()) {
        bulkLoadBottomUp(entries);
    }
}

void RTreeAlgorithms::eliminateDeadSpace(RTreePage* page)
{
    if (!page) return;
    
    page->updatePageMBR();
    page->optimize();
}

void RTreeAlgorithms::minimizeOverlap(RTreePage* page)
{
    if (!page) return;
    
    // Reorder entries to minimize overlap
    page->reorderEntries();
}

double RTreeAlgorithms::calculateEnlargement(const ExtendedMBR& existingMBR, const ExtendedMBR& newMBR)
{
    return existingMBR.enlargement(newMBR);
}

double RTreeAlgorithms::calculateOverlapIncrease(RTreePage* page, const ExtendedMBR& newMBR, USHORT excludeIndex)
{
    if (!page) return 0.0;
    
    double totalOverlapIncrease = 0.0;
    ExtendedMBR expandedMBR = newMBR;
    
    const ObjectsArray<RTreeEntry>& entries = page->getEntries();
    for (USHORT i = 0; i < entries.getCount(); i++) {
        if (i != excludeIndex) {
            ExtendedMBR entryMBR = entries[i].getMBR();
            double oldOverlap = expandedMBR.overlapArea(entryMBR);
            
            ExtendedMBR combinedMBR = expandedMBR + entryMBR;
            double newOverlap = combinedMBR.overlapArea(entryMBR);
            
            totalOverlapIncrease += (newOverlap - oldOverlap);
        }
    }
    
    return totalOverlapIncrease;
}

USHORT RTreeAlgorithms::selectBestChild(RTreePage* page, const ExtendedMBR& mbr)
{
    if (!page || page->getEntryCount() == 0) return 0;
    
    const ObjectsArray<RTreeEntry>& entries = page->getEntries();
    USHORT bestChild = 0;
    double bestEnlargement = INFINITY;
    
    for (USHORT i = 0; i < entries.getCount(); i++) {
        double enlargement = calculateEnlargement(entries[i].getMBR(), mbr);
        
        if (enlargement < bestEnlargement) {
            bestEnlargement = enlargement;
            bestChild = i;
        } else if (enlargement == bestEnlargement) {
            // Tie-breaking: choose entry with smaller area
            if (entries[i].getMBR().area() < entries[bestChild].getMBR().area()) {
                bestChild = i;
            }
        }
    }
    
    return bestChild;
}

bool RTreeAlgorithms::isUnderflow(RTreePage* page)
{
    if (!page) return true;
    return page->getEntryCount() < RTREE_MIN_ENTRIES_PER_PAGE;
}

void RTreeAlgorithms::collectSubtreeEntries(ULONG pageNumber, std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries)
{
    RTreePage* page = index.readPage(pageNumber);
    if (!page) return;
    
    const ObjectsArray<RTreeEntry>& pageEntries = page->getEntries();
    
    if (page->isLeaf()) {
        // Collect leaf entries
        for (ULONG i = 0; i < pageEntries.getCount(); i++) {
            const RTreeEntry& entry = pageEntries[i];
            entries.emplace_back(entry.getMBR(), entry.getRecordNumber());
        }
    } else {
        // Recurse into children
        for (ULONG i = 0; i < pageEntries.getCount(); i++) {
            const RTreeEntry& entry = pageEntries[i];
            if (entry.getType() == RTREE_ENTRY_INTERNAL) {
                collectSubtreeEntries(entry.getChildPage(), entries);
            }
        }
    }
}

//============================================================================
// RTreeBulkLoader Implementation
//============================================================================

RTreeBulkLoader::RTreeBulkLoader(RTreeIndex& idx, MemoryPool& p, ULONG fanout)
    : index(idx), pool(p), targetFanout(fanout)
{
}

RTreeBulkLoader::~RTreeBulkLoader()
{
}

bool RTreeBulkLoader::loadFromSortedData(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries)
{
    if (entries.empty()) return true;
    
    try {
        // Calculate STR parameters
        STRParams params = calculateSTRParams(entries.size());
        
        // Create leaf level
        std::vector<RTreePage*> leafPages = createLeafLevel(entries);
        
        // Build internal levels bottom-up
        std::vector<RTreePage*> currentLevel = leafPages;
        USHORT level = 1;
        
        while (currentLevel.size() > 1) {
            currentLevel = createInternalLevel(currentLevel, level);
            level++;
        }
        
        // Set root
        if (!currentLevel.empty()) {
            currentLevel[0]->setFlag(RTREE_PAGE_ROOT);
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

bool RTreeBulkLoader::loadFromUnsortedData(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries)
{
    if (entries.empty()) return true;
    
    // Sort using STR method
    sortTileRecursive(entries);
    
    return loadFromSortedData(entries);
}

void RTreeBulkLoader::sortByHilbert(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries)
{
    std::sort(entries.begin(), entries.end(),
        [this](const std::pair<ExtendedMBR, RecordNumber>& a, const std::pair<ExtendedMBR, RecordNumber>& b) {
            ULONG64 hilbertA = calculateHilbertValue(a.first);
            ULONG64 hilbertB = calculateHilbertValue(b.first);
            return hilbertA < hilbertB;
        });
}

void RTreeBulkLoader::sortByZOrder(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries)
{
    std::sort(entries.begin(), entries.end(),
        [this](const std::pair<ExtendedMBR, RecordNumber>& a, const std::pair<ExtendedMBR, RecordNumber>& b) {
            ULONG64 zOrderA = calculateZOrderValue(a.first);
            ULONG64 zOrderB = calculateZOrderValue(b.first);
            return zOrderA < zOrderB;
        });
}

void RTreeBulkLoader::sortTileRecursive(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries)
{
    if (entries.empty()) return;
    
    // Calculate number of slices
    ULONG numSlices = static_cast<ULONG>(std::ceil(std::sqrt(entries.size() / targetFanout)));
    if (numSlices == 0) numSlices = 1;
    
    // Sort by X coordinate
    std::sort(entries.begin(), entries.end(),
        [](const std::pair<ExtendedMBR, RecordNumber>& a, const std::pair<ExtendedMBR, RecordNumber>& b) {
            return a.first.getCenter().x < b.first.getCenter().x;
        });
    
    // Process each slice
    ULONG sliceSize = entries.size() / numSlices;
    ULONG remainder = entries.size() % numSlices;
    
    ULONG offset = 0;
    for (ULONG i = 0; i < numSlices; i++) {
        ULONG currentSliceSize = sliceSize + (i < remainder ? 1 : 0);
        
        if (currentSliceSize > 0) {
            // Sort this slice by Y coordinate
            std::sort(entries.begin() + offset, entries.begin() + offset + currentSliceSize,
                [](const std::pair<ExtendedMBR, RecordNumber>& a, const std::pair<ExtendedMBR, RecordNumber>& b) {
                    return a.first.getCenter().y < b.first.getCenter().y;
                });
        }
        
        offset += currentSliceSize;
    }
}

ULONG RTreeBulkLoader::getOptimalFanout(ULONG totalEntries)
{
    if (totalEntries == 0) return targetFanout;
    
    // Calculate optimal fanout based on page utilization
    double optimalFanout = std::sqrt(static_cast<double>(totalEntries));
    
    return static_cast<ULONG>(std::max(static_cast<double>(RTREE_MIN_ENTRIES_PER_PAGE), 
                                      std::min(optimalFanout, static_cast<double>(RTREE_MAX_ENTRIES_PER_PAGE))));
}

RTreeBulkLoader::STRParams RTreeBulkLoader::calculateSTRParams(ULONG totalEntries)
{
    STRParams params;
    params.totalEntries = totalEntries;
    params.leafCapacity = targetFanout;
    params.numLeaves = (totalEntries + params.leafCapacity - 1) / params.leafCapacity;
    params.stripeCapacity = static_cast<ULONG>(std::ceil(std::sqrt(params.numLeaves)));
    params.numStripes = (params.numLeaves + params.stripeCapacity - 1) / params.stripeCapacity;
    
    return params;
}

std::vector<RTreePage*> RTreeBulkLoader::createLeafLevel(std::vector<std::pair<ExtendedMBR, RecordNumber>>& entries)
{
    std::vector<RTreePage*> leafPages;
    
    ULONG offset = 0;
    while (offset < entries.size()) {
        ULONG pageSize = std::min(targetFanout, static_cast<ULONG>(entries.size() - offset));
        
        RTreePage* leafPage = index.getPageFactory().createLeafPage();
        
        for (ULONG i = 0; i < pageSize; i++) {
            const auto& entry = entries[offset + i];
            RTreeEntry* leafEntry = index.getPageFactory().createLeafEntry(entry.first, entry.second);
            leafPage->addEntry(leafEntry);
        }
        
        leafPages.push_back(leafPage);
        offset += pageSize;
    }
    
    return leafPages;
}

std::vector<RTreePage*> RTreeBulkLoader::createInternalLevel(std::vector<RTreePage*>& childPages, USHORT level)
{
    std::vector<RTreePage*> internalPages;
    
    ULONG offset = 0;
    while (offset < childPages.size()) {
        ULONG pageSize = std::min(targetFanout, static_cast<ULONG>(childPages.size() - offset));
        
        RTreePage* internalPage = index.getPageFactory().createInternalPage(level);
        
        for (ULONG i = 0; i < pageSize; i++) {
            RTreePage* childPage = childPages[offset + i];
            ULONG childPageNum = index.getPageNumber(childPage);
            
            RTreeEntry* internalEntry = index.getPageFactory().createInternalEntry(
                childPage->getPageMBR(), childPageNum);
            internalPage->addEntry(internalEntry);
            
            // Set parent relationship
            childPage->setParentPage(index.getPageNumber(internalPage));
        }
        
        internalPages.push_back(internalPage);
        offset += pageSize;
    }
    
    return internalPages;
}

ULONG64 RTreeBulkLoader::calculateHilbertValue(const ExtendedMBR& mbr, USHORT order)
{
    Coordinate center = mbr.getCenter();
    
    // Normalize coordinates to [0, 2^order - 1]
    ULONG maxVal = (1UL << order) - 1;
    ULONG x = static_cast<ULONG>(center.x * maxVal / MAX_COORDINATE);
    ULONG y = static_cast<ULONG>(center.y * maxVal / MAX_COORDINATE);
    
    return hilbertCurve2D(x, y, order);
}

ULONG64 RTreeBulkLoader::hilbertCurve2D(ULONG x, ULONG y, USHORT order)
{
    ULONG64 hilbert = 0;
    ULONG s = 1UL << (order - 1);
    
    while (s > 0) {
        ULONG rx = (x & s) > 0 ? 1 : 0;
        ULONG ry = (y & s) > 0 ? 1 : 0;
        
        hilbert += s * s * ((3 * rx) ^ ry);
        
        // Rotate coordinates
        if (ry == 0) {
            if (rx == 1) {
                x = s - 1 - x;
                y = s - 1 - y;
            }
            std::swap(x, y);
        }
        
        s >>= 1;
    }
    
    return hilbert;
}

ULONG64 RTreeBulkLoader::calculateZOrderValue(const ExtendedMBR& mbr)
{
    Coordinate center = mbr.getCenter();
    
    // Normalize coordinates
    ULONG x = static_cast<ULONG>(center.x * 0xFFFFFFFF / MAX_COORDINATE);
    ULONG y = static_cast<ULONG>(center.y * 0xFFFFFFFF / MAX_COORDINATE);
    
    return interleaveBits(x, y);
}

ULONG64 RTreeBulkLoader::interleaveBits(ULONG x, ULONG y)
{
    ULONG64 result = 0;
    
    for (int i = 0; i < 32; i++) {
        result |= ((x & (1UL << i)) << i) | ((y & (1UL << i)) << (i + 1));
    }
    
    return result;
}

//============================================================================
// Additional utility classes would be implemented here:
// - RTreeQueryOptimizer
// - RTreeMaintenance  
// - RTreeConcurrency
// - RTreeProfiler
//============================================================================

// Placeholder implementations for remaining classes...

RTreeQueryOptimizer::RTreeQueryOptimizer(RTreeIndex& idx, MemoryPool& p)
    : index(idx), pool(p)
{
}

RTreeQueryOptimizer::~RTreeQueryOptimizer()
{
}

// Additional method implementations would follow...

RTreeMaintenance::RTreeMaintenance(RTreeIndex& idx, MemoryPool& p)
    : index(idx), pool(p)
{
}

RTreeMaintenance::~RTreeMaintenance()
{
}

// More implementations...

RTreeConcurrency::RTreeConcurrency(RTreeIndex& idx, MemoryPool& p)
    : index(idx), pool(p)
{
}

RTreeConcurrency::~RTreeConcurrency()
{
}

// Profiler namespace implementations...
namespace RTreeProfiler
{
    static bool profilingEnabled = false;
    static std::vector<OperationProfile> operationProfiles;
    static std::vector<QueryProfile> queryProfiles;
    
    void startProfiling()
    {
        profilingEnabled = true;
        resetProfile();
    }
    
    void stopProfiling()
    {
        profilingEnabled = false;
    }
    
    void resetProfile()
    {
        operationProfiles.clear();
        queryProfiles.clear();
    }
    
    void recordOperation(const string& operation, double duration)
    {
        if (!profilingEnabled) return;
        
        // Find existing profile or create new one
        for (OperationProfile& profile : operationProfiles) {
            if (profile.operationName == operation) {
                profile.callCount++;
                profile.totalTime += duration;
                profile.averageTime = profile.totalTime / profile.callCount;
                profile.minTime = std::min(profile.minTime, duration);
                profile.maxTime = std::max(profile.maxTime, duration);
                return;
            }
        }
        
        // Create new profile
        OperationProfile profile;
        profile.operationName = operation;
        profile.callCount = 1;
        profile.totalTime = duration;
        profile.averageTime = duration;
        profile.minTime = duration;
        profile.maxTime = duration;
        operationProfiles.push_back(profile);
    }
    
    void recordQuery(const QueryProfile& profile)
    {
        if (profilingEnabled) {
            queryProfiles.push_back(profile);
        }
    }
    
    std::vector<OperationProfile> getOperationProfiles()
    {
        return operationProfiles;
    }
    
    std::vector<QueryProfile> getQueryProfiles()
    {
        return queryProfiles;
    }
    
    string generatePerformanceReport()
    {
        std::ostringstream report;
        
        report << "R-Tree Performance Profile Report\n";
        report << "=================================\n\n";
        
        report << "Operation Profiles:\n";
        for (const OperationProfile& profile : operationProfiles) {
            report << "  " << profile.operationName << ":\n";
            report << "    Calls: " << profile.callCount << "\n";
            report << "    Total Time: " << profile.totalTime << "ms\n";
            report << "    Average Time: " << profile.averageTime << "ms\n";
            report << "    Min Time: " << profile.minTime << "ms\n";
            report << "    Max Time: " << profile.maxTime << "ms\n\n";
        }
        
        report << "Query Profiles: " << queryProfiles.size() << " queries executed\n";
        
        return report.str();
    }
    
    std::vector<string> identifyBottlenecks()
    {
        std::vector<string> bottlenecks;
        
        for (const OperationProfile& profile : operationProfiles) {
            if (profile.averageTime > 100.0) { // More than 100ms average
                bottlenecks.push_back("Slow operation: " + profile.operationName);
            }
            
            if (profile.callCount > 10000) { // Very frequent operation
                bottlenecks.push_back("Frequent operation: " + profile.operationName);
            }
        }
        
        return bottlenecks;
    }
    
    string analyzeQueryPerformance(const std::vector<QueryProfile>& profiles)
    {
        if (profiles.empty()) return "No query profiles available";
        
        std::ostringstream analysis;
        analysis << "Query Performance Analysis:\n";
        analysis << "Total Queries: " << profiles.size() << "\n";
        
        double totalTime = 0.0;
        ULONG totalPagesAccessed = 0;
        
        for (const QueryProfile& profile : profiles) {
            totalTime += profile.executionTime;
            totalPagesAccessed += profile.pagesAccessed;
        }
        
        analysis << "Average Execution Time: " << (totalTime / profiles.size()) << "ms\n";
        analysis << "Average Pages Accessed: " << (totalPagesAccessed / profiles.size()) << "\n";
        
        return analysis.str();
    }
    
    void suggestOptimizations(string& suggestions)
    {
        std::ostringstream oss;
        
        oss << "R-Tree Optimization Suggestions:\n";
        oss << "================================\n";
        
        std::vector<string> bottlenecks = identifyBottlenecks();
        if (!bottlenecks.empty()) {
            oss << "\nIdentified Bottlenecks:\n";
            for (const string& bottleneck : bottlenecks) {
                oss << "- " << bottleneck << "\n";
            }
        }
        
        oss << "\nGeneral Recommendations:\n";
        oss << "- Consider rebuilding index if page utilization is low\n";
        oss << "- Monitor overlap ratios and rebalance if necessary\n";
        oss << "- Use bulk loading for large datasets\n";
        oss << "- Adjust fanout based on query patterns\n";
        
        suggestions = oss.str();
    }
}