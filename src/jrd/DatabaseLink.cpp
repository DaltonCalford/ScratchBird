/*
 * Database Link - Schema-aware database link implementation
 * Enhanced for hierarchical schema support and remote database access
 */

#include "DatabaseLink.h"
#include "Attachment.h"
#include "thread_db.h"
#include "../common/classes/init.h"
#include "../common/StatusArg.h"
#include "../jrd/err_proto.h"

namespace DatabaseLinks {

// DatabaseLink implementation

DatabaseLink::DatabaseLink() : 
    schemaMode(SCHEMA_MODE_NONE), 
    schemaDepth(0), 
    isValid_(false)
{
}

DatabaseLink::DatabaseLink(ScratchBird::MemoryPool& pool) : 
    schemaMode(SCHEMA_MODE_NONE), 
    schemaDepth(0), 
    isValid_(false)
{
}

DatabaseLink::DatabaseLink(const ScratchBird::string& name, const ScratchBird::string& server, 
                          const ScratchBird::string& database, const ScratchBird::string& user, 
                          const ScratchBird::string& pass) :
    linkName(name),
    serverName(server),
    databasePath(database),
    userName(user),
    password(pass),
    schemaMode(SCHEMA_MODE_NONE),
    schemaDepth(0),
    isValid_(true)
{
}

DatabaseLink::~DatabaseLink()
{
    disconnect();
}

ScratchBird::string DatabaseLink::resolveRemoteSchema(const ScratchBird::string& localPath, Jrd::thread_db* tdbb) const
{
    if (!tdbb || !isValid_) {
        return ScratchBird::string();
    }
    
    switch (schemaMode) {
    case SCHEMA_MODE_NONE:
        return ScratchBird::string(); // No schema mapping
        
    case SCHEMA_MODE_FIXED:
        return remoteSchema; // Fixed remote schema
        
    case SCHEMA_MODE_CONTEXT_AWARE:
        // Handle context-aware schema references
        if (remoteSchema == "CURRENT") {
            // Use current schema from attachment
            // TODO: Get current schema from attachment context
            return ScratchBird::string("PUBLIC"); // Default fallback
        } else if (remoteSchema == "HOME") {
            // Use home schema from attachment
            // TODO: Get home schema from attachment context  
            return ScratchBird::string("PUBLIC"); // Default fallback
        } else if (remoteSchema == "USER") {
            // Use user's default schema
            // TODO: Get user schema from attachment context
            return ScratchBird::string("PUBLIC"); // Default fallback
        }
        return remoteSchema;
        
    case SCHEMA_MODE_HIERARCHICAL:
        {
            // Hierarchical mapping: replace local prefix with remote prefix
            if (localPath.hasData() && localSchema.hasData()) {
                if (localPath.substr(0, localSchema.length()) == localSchema) {
                    // Replace local schema prefix with remote schema prefix
                    ScratchBird::string suffix = localPath.substr(localSchema.length());
                    return remoteSchema + suffix;
                }
            }
            return remoteSchema; // Fallback to remote schema
        }
        
    case SCHEMA_MODE_MIRROR:
        return localPath; // Mirror local schema path exactly
        
    default:
        return ScratchBird::string();
    }
}

bool DatabaseLink::validateSchemaAccess(const ScratchBird::string& schemaPath, Jrd::thread_db* tdbb) const
{
    if (!tdbb || schemaPath.isEmpty()) {
        return false;
    }
    
    // Check schema path depth limits
    SSHORT depth = getSchemaPathDepth(schemaPath);
    if (depth > MAX_SCHEMA_DEPTH) {
        return false;
    }
    
    // Validate schema path format
    if (!isValidSchemaPath(schemaPath)) {
        return false;
    }
    
    // TODO: Add actual remote schema existence validation
    // This would require establishing a connection and querying RDB$SCHEMAS
    
    return true;
}

ScratchBird::string DatabaseLink::resolveTargetSchema(const ScratchBird::string& contextSchema, Jrd::thread_db* tdbb) const
{
    ScratchBird::string resolved = resolveRemoteSchema(contextSchema, tdbb);
    
    if (!validateSchemaAccess(resolved, tdbb)) {
        return ScratchBird::string(); // Invalid schema path
    }
    
    return resolved;
}

bool DatabaseLink::validateConnection(Jrd::thread_db* tdbb) const
{
    if (!isValid_ || serverName.isEmpty() || databasePath.isEmpty()) {
        return false;
    }
    
    // TODO: Implement actual connection validation
    // This would establish a test connection to verify accessibility
    
    return true;
}

bool DatabaseLink::connect(Jrd::thread_db* tdbb)
{
    if (!validateConnection(tdbb)) {
        return false;
    }
    
    // TODO: Implement actual connection establishment
    // This would create an EDS connection to the remote database
    
    return true;
}

void DatabaseLink::disconnect()
{
    // TODO: Implement connection cleanup
    // This would close any active EDS connections
}

bool DatabaseLink::isValidSchemaPath(const ScratchBird::string& path)
{
    if (path.isEmpty()) {
        return true; // Empty path is valid (means no schema)
    }
    
    if (path.length() > MAX_SCHEMA_PATH_LENGTH) {
        return false;
    }
    
    // Check for valid identifier characters and proper dot separation
    // TODO: Implement comprehensive schema path validation
    
    return true;
}

SSHORT DatabaseLink::getSchemaPathDepth(const ScratchBird::string& path)
{
    if (path.isEmpty()) {
        return 0;
    }
    
    SSHORT depth = 1;
    for (size_t i = 0; i < path.length(); i++) {
        if (path[i] == '.') {
            depth++;
        }
    }
    
    return depth;
}

// LinkManager implementation

LinkManager::LinkManager() : linksLock()
{
}

LinkManager::LinkManager(ScratchBird::MemoryPool& pool) : linksLock()
{
}

LinkManager::~LinkManager()
{
    clearLinks();
}

bool LinkManager::addLink(DatabaseLink* link)
{
    if (!link || !link->isValid()) {
        return false;
    }
    
    ScratchBird::WriteLockGuard guard(linksLock, "LinkManager::addLink");
    
    ScratchBird::string name = link->getName();
    if (links.exist(name)) {
        return false; // Link already exists
    }
    
    links.put(name, link);
    return true;
}

bool LinkManager::removeLink(const ScratchBird::string& name)
{
    ScratchBird::WriteLockGuard guard(linksLock, "LinkManager::removeLink");
    
    DatabaseLink** linkPtr = links.get(name);
    if (!linkPtr) {
        return false;
    }
    
    DatabaseLink* link = *linkPtr;
    links.remove(name);
    delete link;
    
    return true;
}

DatabaseLink* LinkManager::findLink(const ScratchBird::string& name)
{
    ScratchBird::ReadLockGuard guard(linksLock, "LinkManager::findLink");
    
    DatabaseLink** linkPtr = links.get(name);
    return linkPtr ? *linkPtr : nullptr;
}

void LinkManager::clearLinks()
{
    ScratchBird::WriteLockGuard guard(linksLock, "LinkManager::clearLinks");
    
    // Delete all cached database links
    auto it = links.begin();
    while (it != links.end()) {
        DatabaseLink* link = (*it).second;
        if (link) {
            link->disconnect();
            delete link;
        }
        ++it;
    }
    
    links.clear();
}

DatabaseLink* LinkManager::findLinkBySchema(const ScratchBird::string& schemaPath)
{
    ScratchBird::ReadLockGuard guard(linksLock, "LinkManager::findLinkBySchema");
    
    // Search for links that match the schema path
    auto it = links.begin();
    while (it != links.end()) {
        DatabaseLink* link = (*it).second;
        if (link && link->isValid()) {
            // Check if this link's local schema matches or is a parent of the requested path
            const ScratchBird::string& linkSchema = link->getLocalSchema();
            
            if (!linkSchema.isEmpty()) {
                // Exact match
                if (schemaPath == linkSchema) {
                    return link;
                }
                
                // Check if schema path starts with link schema (hierarchical match)
                if (schemaPath.length() > linkSchema.length() && 
                    schemaPath.substr(0, linkSchema.length()) == linkSchema &&
                    schemaPath[linkSchema.length()] == '.') {
                    return link;
                }
            }
        }
        ++it;
    }
    
    return nullptr;
}

ScratchBird::string LinkManager::resolveRemotePath(const ScratchBird::string& linkName, 
                                                  const ScratchBird::string& localPath, 
                                                  Jrd::thread_db* tdbb)
{
    DatabaseLink* link = findLink(linkName);
    if (!link) {
        return ScratchBird::string();
    }
    
    return link->resolveTargetSchema(localPath, tdbb);
}

bool LinkManager::validateAllLinks(Jrd::thread_db* tdbb)
{
    ScratchBird::ReadLockGuard guard(linksLock, "LinkManager::validateAllLinks");
    
    bool allValid = true;
    auto it = links.begin();
    
    while (it != links.end()) {
        DatabaseLink* link = (*it).second;
        if (link) {
            // Validate connection parameters
            if (!link->validateConnection(tdbb)) {
                allValid = false;
                continue;
            }
            
            // Validate schema access if schema mode is configured
            if (link->getSchemaMode() != SCHEMA_MODE_NONE) {
                const ScratchBird::string& remoteSchema = link->getRemoteSchema();
                if (!remoteSchema.isEmpty() && !link->validateSchemaAccess(remoteSchema, tdbb)) {
                    allValid = false;
                    continue;
                }
            }
        } else {
            allValid = false;
        }
        ++it;
    }
    
    return allValid;
}

void LinkManager::refreshSchemaCache()
{
    // TODO: Implement schema cache refresh
    // This would update cached schema information for all links
}

// LinkIterator implementation

LinkManager::LinkIterator::LinkIterator(
    ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<ScratchBird::string, DatabaseLink*>>>::Iterator begin,
    ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<ScratchBird::string, DatabaseLink*>>>::Iterator end) :
    it(begin), end(end)
{
}

bool LinkManager::LinkIterator::hasNext() const
{
    // TODO: Implement proper iterator comparison
    return false;
}

DatabaseLink* LinkManager::LinkIterator::next()
{
    // TODO: Implement proper iterator access
    return nullptr;
}

LinkManager::LinkIterator LinkManager::getIterator()
{
    ScratchBird::ReadLockGuard guard(linksLock, "LinkManager::getIterator");
    return LinkIterator(links.begin(), links.end());
}

} // namespace DatabaseLinks