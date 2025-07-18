/*
 * Schema Path Cache - High-performance schema path parsing and caching
 * Provides efficient hierarchical schema operations for ScratchBird
 */

#include "SchemaPathCache.h"
#include "../common/classes/init.h"
#include "../jrd/err_proto.h"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace Jrd {

SchemaPathCache::SchemaPathCache(ScratchBird::MemoryPool& p) 
    : pool(p), 
      pathCache(pool),
      hashCache(pool),
      hitCount(0), 
      missCount(0), 
      maxDepthSeen(0)
{
}

SchemaPathCache::~SchemaPathCache()
{
    clearCache();
}

ParsedSchemaPath* SchemaPathCache::parseSchemaPath(const ScratchBird::string& path)
{
    if (path.empty()) {
        return nullptr;
    }

    // Check cache first (read lock)
    {
        std::shared_lock<std::shared_mutex> lock(cacheMutex);
        auto it = pathCache.begin();
        while (it != pathCache.end()) {
            if ((*it).first == path) {
                hitCount++;
                return (*it).second;
            }
            ++it;
        }
    }

    // Cache miss - parse and cache (write lock)
    {
        std::unique_lock<std::shared_mutex> lock(cacheMutex);
        missCount++;
        
        // Double-check in case another thread added it
        auto it = pathCache.begin();
        while (it != pathCache.end()) {
            if ((*it).first == path) {
                return (*it).second;
            }
            ++it;
        }
        
        // Parse the path
        ParsedSchemaPath* parsed = parseSchemaPathInternal(path);
        if (parsed && parsed->isValid) {
            insertIntoCache(path, parsed);
            if (parsed->depth > maxDepthSeen) {
                maxDepthSeen = parsed->depth;
            }
        }
        
        return parsed;
    }
}

ParsedSchemaPath* SchemaPathCache::parseSchemaPathInternal(const ScratchBird::string& path)
{
    ParsedSchemaPath* parsed = FB_NEW(pool) ParsedSchemaPath();
    parsed->fullPath = path;
    
    // Split by separator
    std::string pathStr(path.c_str());
    std::stringstream ss(pathStr);
    std::string component;
    
    while (std::getline(ss, component, SCHEMA_SEPARATOR)) {
        if (component.empty()) {
            // Empty component (e.g., ".." in path)
            FB_DELETE(pool, parsed);
            return nullptr;
        }
        
        if (!validateSchemaComponent(ScratchBird::string(component.c_str()))) {
            FB_DELETE(pool, parsed);
            return nullptr;
        }
        
        parsed->components.emplace_back(ScratchBird::string(component.c_str()));
    }
    
    parsed->depth = parsed->components.size();
    
    // Check depth limits
    if (parsed->depth == 0 || parsed->depth > MAX_SCHEMA_DEPTH) {
        FB_DELETE(pool, parsed);
        return nullptr;
    }
    
    parsed->calculateHash();
    parsed->isValid = true;
    
    return parsed;
}

bool SchemaPathCache::validateSchemaComponent(const ScratchBird::string& component) const
{
    if (component.empty() || component.length() > MAX_COMPONENT_LENGTH) {
        return false;
    }
    
    // Must start with letter or underscore
    const char* str = component.c_str();
    if (!std::isalpha(str[0]) && str[0] != '_') {
        return false;
    }
    
    // Rest must be alphanumeric or underscore
    for (size_t i = 1; i < component.length(); i++) {
        if (!std::isalnum(str[i]) && str[i] != '_') {
            return false;
        }
    }
    
    return true;
}

void SchemaPathCache::insertIntoCache(const ScratchBird::string& path, ParsedSchemaPath* parsed)
{
    // Insert into path cache
    pathCache.put(ScratchBird::Pair<ScratchBird::string, ParsedSchemaPath*>(path, parsed));
    
    // Insert into hash cache for fast lookup
    hashCache.put(ScratchBird::Pair<size_t, ParsedSchemaPath*>(parsed->pathHash, parsed));
    
    // Trim cache if it gets too large
    if (pathCache.count() > 2000) {
        trimCache(1000);
    }
}

bool SchemaPathCache::isValidPath(const ScratchBird::string& path) const
{
    if (path.empty()) {
        return false;
    }
    
    // Quick check against cache
    {
        std::shared_lock<std::shared_mutex> lock(cacheMutex);
        auto it = pathCache.begin();
        while (it != pathCache.end()) {
            if ((*it).first == path) {
                return (*it).second && (*it).second->isValid;
            }
            ++it;
        }
    }
    
    // Parse to validate
    const_cast<SchemaPathCache*>(this)->parseSchemaPath(path);
    
    // Check again
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    auto it = pathCache.begin();
    while (it != pathCache.end()) {
        if ((*it).first == path) {
            return (*it).second && (*it).second->isValid;
        }
        ++it;
    }
    
    return false;
}

size_t SchemaPathCache::getSchemaDepth(const ScratchBird::string& path)
{
    ParsedSchemaPath* parsed = parseSchemaPath(path);
    return parsed ? parsed->depth : 0;
}

ScratchBird::string SchemaPathCache::getSchemaComponent(const ScratchBird::string& path, size_t index)
{
    ParsedSchemaPath* parsed = parseSchemaPath(path);
    if (!parsed || index >= parsed->components.size()) {
        return ScratchBird::string();
    }
    
    return parsed->components[index].name;
}

ScratchBird::string SchemaPathCache::getParentSchema(const ScratchBird::string& path)
{
    ParsedSchemaPath* parsed = parseSchemaPath(path);
    if (!parsed || parsed->components.size() <= 1) {
        return ScratchBird::string();
    }
    
    // Build parent path
    std::vector<ScratchBird::string> parentComponents;
    for (size_t i = 0; i < parsed->components.size() - 1; i++) {
        parentComponents.push_back(parsed->components[i].name);
    }
    
    return joinSchemaPath(parentComponents);
}

ScratchBird::string SchemaPathCache::getLeafSchema(const ScratchBird::string& path)
{
    ParsedSchemaPath* parsed = parseSchemaPath(path);
    if (!parsed || parsed->components.empty()) {
        return ScratchBird::string();
    }
    
    return parsed->components.back().name;
}

ScratchBird::string SchemaPathCache::joinSchemaPath(const std::vector<ScratchBird::string>& components)
{
    if (components.empty()) {
        return ScratchBird::string();
    }
    
    ScratchBird::string result = components[0];
    for (size_t i = 1; i < components.size(); i++) {
        result += SCHEMA_SEPARATOR;
        result += components[i];
    }
    
    return result;
}

bool SchemaPathCache::isSubSchema(const ScratchBird::string& child, const ScratchBird::string& parent)
{
    ParsedSchemaPath* childParsed = parseSchemaPath(child);
    ParsedSchemaPath* parentParsed = parseSchemaPath(parent);
    
    if (!childParsed || !parentParsed || childParsed->depth <= parentParsed->depth) {
        return false;
    }
    
    // Check if parent components match
    for (size_t i = 0; i < parentParsed->components.size(); i++) {
        if (childParsed->components[i].name != parentParsed->components[i].name) {
            return false;
        }
    }
    
    return true;
}

void SchemaPathCache::clearCache()
{
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    
    // Delete all cached entries
    auto it = pathCache.begin();
    while (it != pathCache.end()) {
        FB_DELETE(pool, (*it).second);
        ++it;
    }
    
    pathCache.clear();
    hashCache.clear();
    hitCount = 0;
    missCount = 0;
    maxDepthSeen = 0;
}

void SchemaPathCache::trimCache(size_t maxEntries)
{
    if (pathCache.count() <= maxEntries) {
        return;
    }
    
    // Simple LRU approximation - remove entries until we're under the limit
    size_t toRemove = pathCache.count() - maxEntries;
    size_t removed = 0;
    
    auto it = pathCache.begin();
    while (it != pathCache.end() && removed < toRemove) {
        ParsedSchemaPath* parsed = (*it).second;
        size_t hash = parsed->pathHash;
        
        // Remove from both caches
        pathCache.remove((*it).first);
        hashCache.remove(hash);
        FB_DELETE(pool, parsed);
        
        removed++;
        it = pathCache.begin(); // Restart iteration after removal
    }
}

void SchemaPathCache::getCacheStats(size_t& hits, size_t& misses, size_t& entries, size_t& maxDepth) const
{
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    hits = hitCount;
    misses = missCount;
    entries = pathCache.count();
    maxDepth = maxDepthSeen;
}

void SchemaPathCache::dumpCacheContents() const
{
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    
    printf("SchemaPathCache Contents:\n");
    printf("  Entries: %zu\n", pathCache.count());
    printf("  Hits: %zu, Misses: %zu\n", hitCount, missCount);
    printf("  Max Depth Seen: %zu\n", maxDepthSeen);
    
    auto it = pathCache.begin();
    while (it != pathCache.end()) {
        ParsedSchemaPath* parsed = (*it).second;
        printf("  Path: '%s' (depth: %zu, hash: %zu)\n", 
               (*it).first.c_str(), parsed->depth, parsed->pathHash);
        ++it;
    }
}

bool SchemaPathCache::isValidSchemaName(const ScratchBird::string& name)
{
    if (name.empty() || name.length() > MAX_COMPONENT_LENGTH) {
        return false;
    }
    
    const char* str = name.c_str();
    if (!std::isalpha(str[0]) && str[0] != '_') {
        return false;
    }
    
    for (size_t i = 1; i < name.length(); i++) {
        if (!std::isalnum(str[i]) && str[i] != '_') {
            return false;
        }
    }
    
    return true;
}

} // namespace Jrd