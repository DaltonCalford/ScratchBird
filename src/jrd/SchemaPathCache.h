/*
 * Schema Path Cache - High-performance schema path parsing and caching
 * Provides efficient hierarchical schema operations for ScratchBird
 */

#ifndef JRD_SCHEMA_PATH_CACHE_H
#define JRD_SCHEMA_PATH_CACHE_H

#include "firebird.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/array.h"
#include "../common/classes/GenericMap.h"
#include "../common/ThreadData.h"
#include "../jrd/constants.h"
#include <vector>
#include <mutex>

namespace Jrd {

// Forward declarations
class thread_db;

// Schema component structure for efficient parsing
struct SchemaComponent
{
    ScratchBird::string name;
    size_t hash;
    bool isValid;
    
    SchemaComponent() : hash(0), isValid(false) {}
    SchemaComponent(const ScratchBird::string& n) : name(n), hash(0), isValid(true) 
    {
        // Simple hash for quick comparison
        hash = std::hash<std::string>{}(name.c_str());
    }
};

// Parsed schema path structure
struct ParsedSchemaPath
{
    std::vector<SchemaComponent> components;
    size_t depth;
    ScratchBird::string fullPath;
    size_t pathHash;
    bool isValid;
    
    ParsedSchemaPath() : depth(0), pathHash(0), isValid(false) {}
    
    void calculateHash() 
    {
        pathHash = 0;
        for (const auto& comp : components) {
            pathHash ^= comp.hash + 0x9e3779b9 + (pathHash << 6) + (pathHash >> 2);
        }
    }
};

// High-performance schema path cache
class SchemaPathCache
{
private:
    ScratchBird::MemoryPool& pool;
    
    // Cache structures
    ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::string, ParsedSchemaPath*>> pathCache;
    ScratchBird::GenericMap<ScratchBird::Pair<size_t, ParsedSchemaPath*>> hashCache;
    
    // Thread safety
    mutable std::shared_mutex cacheMutex;
    
    // Statistics
    mutable size_t hitCount;
    mutable size_t missCount;
    mutable size_t maxDepthSeen;
    
    // Internal methods
    ParsedSchemaPath* parseSchemaPathInternal(const ScratchBird::string& path);
    bool validateSchemaComponent(const ScratchBird::string& component) const;
    void insertIntoCache(const ScratchBird::string& path, ParsedSchemaPath* parsed);

public:
    explicit SchemaPathCache(ScratchBird::MemoryPool& p);
    ~SchemaPathCache();

    // Core functionality
    ParsedSchemaPath* parseSchemaPath(const ScratchBird::string& path);
    bool isValidPath(const ScratchBird::string& path) const;
    size_t getSchemaDepth(const ScratchBird::string& path);
    
    // Component extraction
    ScratchBird::string getSchemaComponent(const ScratchBird::string& path, size_t index);
    ScratchBird::string getParentSchema(const ScratchBird::string& path);
    ScratchBird::string getLeafSchema(const ScratchBird::string& path);
    
    // Path operations
    ScratchBird::string joinSchemaPath(const std::vector<ScratchBird::string>& components);
    bool isSubSchema(const ScratchBird::string& child, const ScratchBird::string& parent);
    
    // Cache management
    void clearCache();
    void trimCache(size_t maxEntries = 1000);
    
    // Statistics and diagnostics
    void getCacheStats(size_t& hits, size_t& misses, size_t& entries, size_t& maxDepth) const;
    void dumpCacheContents() const;
    
    // Validation
    static bool isValidSchemaName(const ScratchBird::string& name);
    static const char SCHEMA_SEPARATOR = '.';
    static const size_t MAX_COMPONENT_LENGTH = 63;
};

} // namespace Jrd

#endif // JRD_SCHEMA_PATH_CACHE_H