/*
 * Schema Path Cache - Stub for compilation compatibility
 * Temporarily disabled for core build completion
 */

#ifndef JRD_SCHEMA_PATH_CACHE_H
#define JRD_SCHEMA_PATH_CACHE_H

#include "firebird.h"
#include "../common/classes/fb_string.h"

namespace Jrd {

// Stub class for schema path cache
class SchemaPathCache
{
public:
    explicit SchemaPathCache(ScratchBird::MemoryPool& p) {}
    ~SchemaPathCache() {}

    // Minimal interface for compilation compatibility
    bool isValidPath(const ScratchBird::string& path) const { return true; }
    void clearCache() {}
};

} // namespace Jrd

#endif // JRD_SCHEMA_PATH_CACHE_H