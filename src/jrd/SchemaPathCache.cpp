/*
 * Schema Path Cache Implementation
 * High-performance schema path parsing and validation for database links
 */

#include "firebird.h"
#include "SchemaPathCache.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/locks.h"

using namespace ScratchBird;
using namespace Jrd;

namespace Jrd {

//
// SchemaPathCache implementation
//

SchemaPathCache::~SchemaPathCache()
{
    clearCache();
}

const SchemaPathComponents* SchemaPathCache::getSchemaPath(const string& path)
{
    if (path.isEmpty())
        return nullptr;
    
    // Try read lock first for cache hit
    ReadLockGuard readGuard(cacheLock, FB_FUNCTION);
    
    SchemaPathComponents** found = pathCache.get(path);
    if (found && *found)
    {
        return *found;
    }
    
    // Cache miss - upgrade to write lock and parse
    readGuard.release();
    WriteLockGuard writeGuard(cacheLock, FB_FUNCTION);
    
    // Double check after acquiring write lock
    found = pathCache.get(path);
    if (found && *found)
    {
        return *found;
    }
    
    // Parse the path
    SchemaPathComponents* components = parseSchemaPath(path);
    if (components)
    {
        pathCache.put(path, components);
    }
    
    return components;
}

SchemaPathComponents* SchemaPathCache::parseSchemaPath(const string& path)
{
    if (path.isEmpty())
        return nullptr;
    
    SchemaPathComponents* components = FB_NEW_POOL(pool) SchemaPathComponents(pool);
    
    // Split path by separator
    string currentPath = path;
    USHORT depth = 0;
    
    while (!currentPath.isEmpty() && depth < MAX_SCHEMA_DEPTH)
    {
        size_t separatorPos = currentPath.find(SCHEMA_PATH_SEPARATOR);
        
        string component;
        if (separatorPos != string::npos)
        {
            component = currentPath.substr(0, separatorPos);
            currentPath = currentPath.substr(separatorPos + 1);
        }
        else
        {
            component = currentPath;
            currentPath = "";
        }
        
        // Validate component
        if (!validateSchemaComponent(component))
        {
            delete components;
            return nullptr;
        }
        
        components->components.add(component);
        depth++;
    }
    
    // Check for remaining path (too deep)
    if (!currentPath.isEmpty())
    {
        delete components;
        return nullptr;
    }
    
    components->depth = depth;
    components->isValid = (depth > 0);
    
    return components;
}

bool SchemaPathCache::validateSchemaComponent(const string& component) const
{
    if (component.isEmpty())
        return false;
    
    // Check length limit (63 characters max for identifier)
    if (component.length() > METADATA_IDENTIFIER_CHAR_LEN)
        return false;
    
    // Basic identifier validation
    // Must start with letter or underscore
    char firstChar = component[0];
    if (!((firstChar >= 'A' && firstChar <= 'Z') ||
          (firstChar >= 'a' && firstChar <= 'z') ||
          (firstChar == '_')))
    {
        return false;
    }
    
    // Remaining characters must be alphanumeric or underscore
    for (size_t i = 1; i < component.length(); i++)
    {
        char c = component[i];
        if (!((c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              (c == '_')))
        {
            return false;
        }
    }
    
    return true;
}

bool SchemaPathCache::validateSchemaPath(const string& path)
{
    const SchemaPathComponents* components = getSchemaPath(path);
    return components && components->isValid;
}

USHORT SchemaPathCache::getSchemaDepth(const string& path)
{
    if (path.isEmpty())
        return 0;
    
    const SchemaPathComponents* components = getSchemaPath(path);
    return components ? components->depth : 0;
}

string SchemaPathCache::getParentSchemaPath(const string& path)
{
    if (path.isEmpty())
        return "";
    
    const SchemaPathComponents* components = getSchemaPath(path);
    if (!components || components->depth <= 1)
        return "";
    
    return components->getParentPath();
}

string SchemaPathCache::buildChildSchemaPath(const string& parent, const string& child)
{
    if (parent.isEmpty())
        return child;
    
    if (child.isEmpty())
        return parent;
    
    // Validate both components
    if (!validateSchemaPath(parent) || !SchemaPathUtils::isValidSchemaPath(child))
        return "";
    
    // Check depth limit
    USHORT parentDepth = getSchemaDepth(parent);
    USHORT childDepth = SchemaPathUtils::countSchemaDepth(child);
    
    if (parentDepth + childDepth > MAX_SCHEMA_DEPTH)
        return "";
    
    return parent + SCHEMA_PATH_SEPARATOR + child;
}

void SchemaPathCache::clearCache()
{
    WriteLockGuard guard(cacheLock, FB_FUNCTION);
    
    // Delete all cached components
    for (PathCacheMap::Iterator it(pathCache); it.hasData(); ++it)
    {
        delete it.current().second;
    }
    
    pathCache.clear();
}

void SchemaPathCache::removeCachedPath(const string& path)
{
    WriteLockGuard guard(cacheLock, FB_FUNCTION);
    
    SchemaPathComponents** found = pathCache.get(path);
    if (found && *found)
    {
        delete *found;
        pathCache.remove(path);
    }
}

SchemaPathCache::CacheStats SchemaPathCache::getCacheStats() const
{
    ReadLockGuard guard(cacheLock, FB_FUNCTION);
    
    CacheStats stats;
    stats.hitCount = 0;      // Would need instrumentation to track
    stats.missCount = 0;     // Would need instrumentation to track
    stats.parseCount = 0;    // Would need instrumentation to track
    stats.cacheSize = pathCache.count();
    
    return stats;
}

//
// SchemaPathUtils implementation
//

bool SchemaPathUtils::isValidSchemaPath(const string& path)
{
    if (path.isEmpty())
        return false;
    
    // Quick validation without full parsing
    USHORT depth = 0;
    size_t start = 0;
    
    while (start < path.length() && depth < MAX_SCHEMA_DEPTH)
    {
        size_t separatorPos = path.find(SCHEMA_PATH_SEPARATOR, start);
        size_t end = (separatorPos != string::npos) ? separatorPos : path.length();
        
        // Check component length
        if (end - start == 0 || end - start > METADATA_IDENTIFIER_CHAR_LEN)
            return false;
        
        // Basic identifier validation
        char firstChar = path[start];
        if (!((firstChar >= 'A' && firstChar <= 'Z') ||
              (firstChar >= 'a' && firstChar <= 'z') ||
              (firstChar == '_')))
        {
            return false;
        }
        
        // Validate remaining characters
        for (size_t i = start + 1; i < end; i++)
        {
            char c = path[i];
            if (!((c >= 'A' && c <= 'Z') ||
                  (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') ||
                  (c == '_')))
            {
                return false;
            }
        }
        
        depth++;
        start = end + 1;
    }
    
    // Check if we consumed the entire path
    return (start > path.length()) && (depth > 0);
}

USHORT SchemaPathUtils::countSchemaDepth(const string& path)
{
    if (path.isEmpty())
        return 0;
    
    USHORT depth = 1;
    for (size_t i = 0; i < path.length(); i++)
    {
        if (path[i] == SCHEMA_PATH_SEPARATOR)
            depth++;
    }
    
    return depth;
}

string SchemaPathUtils::extractSchemaComponent(const string& path, USHORT index)
{
    if (path.isEmpty() || index >= MAX_SCHEMA_DEPTH)
        return "";
    
    USHORT currentIndex = 0;
    size_t start = 0;
    
    while (start < path.length() && currentIndex <= index)
    {
        size_t separatorPos = path.find(SCHEMA_PATH_SEPARATOR, start);
        size_t end = (separatorPos != string::npos) ? separatorPos : path.length();
        
        if (currentIndex == index)
        {
            return path.substr(start, end - start);
        }
        
        currentIndex++;
        start = end + 1;
    }
    
    return "";
}

string SchemaPathUtils::extractParentPath(const string& path)
{
    if (path.isEmpty())
        return "";
    
    size_t lastSeparator = path.rfind(SCHEMA_PATH_SEPARATOR);
    if (lastSeparator == string::npos)
        return "";
    
    return path.substr(0, lastSeparator);
}

bool SchemaPathUtils::isAbsolutePath(const string& path)
{
    // For now, all paths are considered relative to current schema context
    // In the future, could support absolute paths starting with special prefix
    return false;
}

string SchemaPathUtils::normalizeSchemaPath(const string& path)
{
    if (path.isEmpty())
        return "";
    
    string result;
    result.reserve(path.length());
    
    bool lastWasSeparator = false;
    for (size_t i = 0; i < path.length(); i++)
    {
        char c = path[i];
        
        if (c == SCHEMA_PATH_SEPARATOR)
        {
            if (!lastWasSeparator && !result.isEmpty())
            {
                result += c;
                lastWasSeparator = true;
            }
        }
        else
        {
            result += c;
            lastWasSeparator = false;
        }
    }
    
    // Remove trailing separator
    if (!result.isEmpty() && result[result.length() - 1] == SCHEMA_PATH_SEPARATOR)
    {
        result.resize(result.length() - 1);
    }
    
    return result;
}

} // namespace Jrd