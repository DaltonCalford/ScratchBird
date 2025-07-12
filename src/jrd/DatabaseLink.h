/*
 * Database Link - Schema-aware database link implementation
 * Enhanced for hierarchical schema support and remote database access
 */

#ifndef JRD_DATABASE_LINK_H
#define JRD_DATABASE_LINK_H

#include "firebird.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/array.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/SyncObject.h"
#include "constants.h"

namespace Jrd {
    class thread_db;
    class Attachment;
}

namespace DatabaseLinks {

// Schema resolution modes for database links
enum SchemaMode {
    SCHEMA_MODE_NONE = 0,          // No schema awareness (legacy mode)
    SCHEMA_MODE_FIXED = 1,         // Fixed remote schema mapping
    SCHEMA_MODE_CONTEXT_AWARE = 2, // Context-aware resolution (CURRENT, HOME, USER)
    SCHEMA_MODE_HIERARCHICAL = 3,  // Hierarchical schema mapping
    SCHEMA_MODE_MIRROR = 4         // Mirror mode (local schema = remote schema)
};

// Schema-aware database link class
class DatabaseLink
{
private:
    ScratchBird::string linkName;
    ScratchBird::string serverName;
    ScratchBird::string databasePath;
    ScratchBird::string userName;
    ScratchBird::string password;
    ScratchBird::string localSchema;
    ScratchBird::string remoteSchema;
    SchemaMode schemaMode;
    SSHORT schemaDepth;  // Cached schema depth for optimization
    bool isValid_;
    
    // Schema path parsing and resolution
    ScratchBird::string resolveRemoteSchema(const ScratchBird::string& localPath, Jrd::thread_db* tdbb) const;
    bool validateSchemaAccess(const ScratchBird::string& schemaPath, Jrd::thread_db* tdbb) const;
    
public:
    DatabaseLink();
    DatabaseLink(const ScratchBird::string& name, const ScratchBird::string& server, 
                 const ScratchBird::string& database, const ScratchBird::string& user, 
                 const ScratchBird::string& pass);
    ~DatabaseLink();
    
    // Core properties
    bool isValid() const { return isValid_; }
    const ScratchBird::string& getName() const { return linkName; }
    const ScratchBird::string& getServerName() const { return serverName; }
    const ScratchBird::string& getDatabasePath() const { return databasePath; }
    
    // Schema-aware properties
    void setSchemaMode(SchemaMode mode) { schemaMode = mode; }
    SchemaMode getSchemaMode() const { return schemaMode; }
    void setLocalSchema(const ScratchBird::string& schema) { localSchema = schema; }
    void setRemoteSchema(const ScratchBird::string& schema) { remoteSchema = schema; }
    const ScratchBird::string& getLocalSchema() const { return localSchema; }
    const ScratchBird::string& getRemoteSchema() const { return remoteSchema; }
    SSHORT getSchemaDepth() const { return schemaDepth; }
    
    // Schema resolution methods
    ScratchBird::string resolveTargetSchema(const ScratchBird::string& contextSchema, Jrd::thread_db* tdbb) const;
    bool validateConnection(Jrd::thread_db* tdbb) const;
    
    // Connection management
    bool connect(Jrd::thread_db* tdbb);
    void disconnect();
    
    // Validation methods
    static bool isValidSchemaPath(const ScratchBird::string& path);
    static SSHORT getSchemaPathDepth(const ScratchBird::string& path);
};

// Database link manager class
class LinkManager
{
private:
    ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<ScratchBird::string, DatabaseLink*>>> links;
    ScratchBird::RWLock linksLock;
    
public:
    LinkManager();
    ~LinkManager();
    
    // Link management
    bool addLink(DatabaseLink* link);
    bool removeLink(const ScratchBird::string& name);
    DatabaseLink* findLink(const ScratchBird::string& name);
    void clearLinks();
    
    // Schema-aware operations
    DatabaseLink* findLinkBySchema(const ScratchBird::string& schemaPath);
    ScratchBird::string resolveRemotePath(const ScratchBird::string& linkName, 
                                         const ScratchBird::string& localPath, 
                                         Jrd::thread_db* tdbb);
    
    // Validation and maintenance
    bool validateAllLinks(Jrd::thread_db* tdbb);
    void refreshSchemaCache();
    
    // Iterator support for link enumeration
    class LinkIterator {
    private:
        ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<ScratchBird::string, DatabaseLink*>>>::iterator it;
        ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<ScratchBird::string, DatabaseLink*>>>::iterator end;
        
    public:
        LinkIterator(ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<ScratchBird::string, DatabaseLink*>>>::iterator begin,
                    ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::Left<ScratchBird::string, DatabaseLink*>>>::iterator end);
        
        bool hasNext() const;
        DatabaseLink* next();
    };
    
    LinkIterator getIterator();
};

} // namespace DatabaseLinks

#endif // JRD_DATABASE_LINK_H