/*
 * Database Link - Stub for compilation compatibility
 * Temporarily disabled advanced database link functionality
 */

#ifndef JRD_DATABASE_LINK_H
#define JRD_DATABASE_LINK_H

#include "firebird.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/array.h"

namespace Jrd {
    class thread_db;
    class Attachment;
}

// Stub namespace for database links
namespace DatabaseLinks {

// Database link manager stub
class LinkManager
{
public:
    LinkManager() {}
    ~LinkManager() {}

    // Minimal interface for compilation compatibility
    void clearLinks() {}
    void* findLink(const ScratchBird::string& name) { return nullptr; }
};

// Stub class for database links
class DatabaseLink
{
public:
    DatabaseLink() {}
    ~DatabaseLink() {}

    // Minimal interface for compilation compatibility
    bool isValid() const { return false; }
    ScratchBird::string getName() const { return ScratchBird::string(); }
};

} // namespace DatabaseLinks

#endif // JRD_DATABASE_LINK_H