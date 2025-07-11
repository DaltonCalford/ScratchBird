/*
 * Schema Path Cache - Optimized schema path parsing and validation
 * Prevents repeated string parsing on every database link connection
 */

#ifndef JRD_SCHEMA_PATH_CACHE_H
#define JRD_SCHEMA_PATH_CACHE_H

#include "firebird.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/locks.h"

namespace Jrd {

// Schema depth and path constants
inline constexpr unsigned MAX_SCHEMA_DEPTH = 11;  // 10 user-createable + 1 for link mounting
inline constexpr unsigned MAX_SCHEMA_PATH_LENGTH = 703;  // 63 * 11 + 10 separators
inline constexpr char SCHEMA_PATH_SEPARATOR = '.';

// Pre-parsed schema path components for fast resolution
struct SchemaPathComponents
{
    ScratchBird::Array<ScratchBird::string> components;
    USHORT depth;
    bool isValid;
    
    SchemaPathComponents(ScratchBird::MemoryPool& pool) 
        : components(pool), depth(0), isValid(false) {}
    
    // Fast component access without string parsing
    const ScratchBird::string& getComponent(USHORT level) const
    {
        return (level < components.getCount()) ? components[level] : ScratchBird::string::EMPTY_STRING;
    }
    
    // Get parent path (all components except last)
    ScratchBird::string getParentPath() const
    {
        if (depth <= 1) return ScratchBird::string::EMPTY_STRING;
        
        ScratchBird::string result;
        for (USHORT i = 0; i < depth - 1; i++)
        {
            if (i > 0) result += SCHEMA_PATH_SEPARATOR;
            result += components[i];
        }
        return result;
    }
    
    // Get full path reconstruction
    ScratchBird::string getFullPath() const
    {
        ScratchBird::string result;
        for (USHORT i = 0; i < depth; i++)
        {
            if (i > 0) result += SCHEMA_PATH_SEPARATOR;
            result += components[i];
        }
        return result;
    }
};

// High-performance schema path cache
class SchemaPathCache
{
private:
    // Path string -> parsed components cache
    typedef ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::string, SchemaPathComponents*> > PathCacheMap;
    
    PathCacheMap pathCache;
    mutable ScratchBird::RWLock cacheLock;
    ScratchBird::MemoryPool& pool;
    
    // Parse schema path into components (internal)
    SchemaPathComponents* parseSchemaPath(const ScratchBird::string& path);
    
    // Validate individual schema component
    bool validateSchemaComponent(const ScratchBird::string& component) const;

public:
    explicit SchemaPathCache(ScratchBird::MemoryPool& p) : pathCache(p), pool(p) {}
    ~SchemaPathCache();
    
    // Get parsed schema path (cached)
    const SchemaPathComponents* getSchemaPath(const ScratchBird::string& path);
    
    // Fast schema path validation
    bool validateSchemaPath(const ScratchBird::string& path);
    
    // Get schema depth without full parsing
    USHORT getSchemaDepth(const ScratchBird::string& path);
    
    // Extract parent schema path efficiently
    ScratchBird::string getParentSchemaPath(const ScratchBird::string& path);
    
    // Build child schema path
    ScratchBird::string buildChildSchemaPath(const ScratchBird::string& parent, const ScratchBird::string& child);
    
    // Cache management
    void clearCache();
    void removeCachedPath(const ScratchBird::string& path);
    
    // Cache statistics
    struct CacheStats
    {
        ULONG hitCount;
        ULONG missCount;
        ULONG parseCount;
        ULONG cacheSize;
    };
    
    CacheStats getCacheStats() const;
};

// Schema path utility functions
class SchemaPathUtils
{
public:
    // Fast path validation without caching
    static bool isValidSchemaPath(const ScratchBird::string& path);
    
    // Count schema depth without parsing
    static USHORT countSchemaDepth(const ScratchBird::string& path);
    
    // Extract nth component without full parsing
    static ScratchBird::string extractSchemaComponent(const ScratchBird::string& path, USHORT index);
    
    // Quick parent path extraction
    static ScratchBird::string extractParentPath(const ScratchBird::string& path);
    
    // Check if path is absolute (starts with root)
    static bool isAbsolutePath(const ScratchBird::string& path);
    
    // Normalize schema path (remove redundant separators, etc.)
    static ScratchBird::string normalizeSchemaPath(const ScratchBird::string& path);
};

} // namespace Jrd

#endif // JRD_SCHEMA_PATH_CACHE_H