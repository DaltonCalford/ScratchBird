/*
 * Database Link Management
 * Handles database link definitions, connections, and operations
 */

#ifndef JRD_DATABASE_LINK_H
#define JRD_DATABASE_LINK_H

#include "../common/classes/fb_string.h"
#include "../common/classes/array.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/locks.h"
#include "../common/classes/ClumpletWriter.h"
#include "../jrd/MetaName.h"
#include "extds/ExtDS.h"

namespace Jrd
{
    class thread_db;
    class Attachment;
    class jrd_tra;
}

namespace DatabaseLinks {

// Database link flags
enum LinkFlags {
    LINK_TRUSTED_AUTH    = 0x01,  // Use trusted authentication
    LINK_SESSION_AUTH    = 0x02,  // Use session-based authentication  
    LINK_POOLING_ENABLED = 0x04,  // Enable connection pooling
    LINK_READ_ONLY       = 0x08,  // Read-only access
    LINK_AUTO_RECONNECT  = 0x10,  // Automatic reconnection
    LINK_SCHEMA_AWARE    = 0x20,  // Schema-aware link
    LINK_INHERIT_CONTEXT = 0x40   // Inherit schema context
};

// Schema resolution modes
enum SchemaResolutionMode {
    SCHEMA_MODE_NONE = 0,          // No schema awareness (legacy mode)
    SCHEMA_MODE_FIXED = 1,         // Fixed remote schema
    SCHEMA_MODE_CONTEXT_AWARE = 2, // Context-aware schema resolution
    SCHEMA_MODE_HIERARCHICAL = 3,  // Hierarchical mapping
    SCHEMA_MODE_MIRROR = 4         // Mirror mode
};

// Database link definition
class DatabaseLink : public ScratchBird::PermanentStorage
{
public:
    explicit DatabaseLink(ScratchBird::MemoryPool& pool);
    ~DatabaseLink();

    // Properties
    const Jrd::MetaName& getName() const { return m_name; }
    const ScratchBird::string& getTarget() const { return m_target; }
    const ScratchBird::string& getUser() const { return m_user; }
    const ScratchBird::string& getPassword() const { return m_password; }
    const ScratchBird::string& getRole() const { return m_role; }
    const ScratchBird::string& getProvider() const { return m_provider; }
    
    int getFlags() const { return m_flags; }
    int getPoolMin() const { return m_poolMin; }
    int getPoolMax() const { return m_poolMax; }
    int getTimeout() const { return m_timeout; }
    
    // Schema-aware properties
    const ScratchBird::string& getLocalSchema() const { return m_localSchema; }
    const ScratchBird::string& getRemoteSchema() const { return m_remoteSchema; }
    SchemaResolutionMode getSchemaMode() const { return m_schemaMode; }
    USHORT getSchemaDepth() const { return m_schemaDepth; }
    
    bool hasTrustedAuth() const { return (m_flags & LINK_TRUSTED_AUTH) != 0; }
    bool hasSessionAuth() const { return (m_flags & LINK_SESSION_AUTH) != 0; }
    bool hasPooling() const { return (m_flags & LINK_POOLING_ENABLED) != 0; }
    bool isReadOnly() const { return (m_flags & LINK_READ_ONLY) != 0; }
    bool hasAutoReconnect() const { return (m_flags & LINK_AUTO_RECONNECT) != 0; }
    bool isSchemaAware() const { return (m_flags & LINK_SCHEMA_AWARE) != 0; }
    bool inheritsContext() const { return (m_flags & LINK_INHERIT_CONTEXT) != 0; }

    // Setters
    void setName(const Jrd::MetaName& name) { m_name = name; }
    void setTarget(const ScratchBird::string& target) { m_target = target; }
    void setCredentials(const ScratchBird::string& user, const ScratchBird::string& password, 
                       const ScratchBird::string& role);
    void setProvider(const ScratchBird::string& provider) { m_provider = provider; }
    void setFlags(int flags) { m_flags = flags; }
    void setPooling(int minConn, int maxConn) { m_poolMin = minConn; m_poolMax = maxConn; }
    void setTimeout(int timeout) { m_timeout = timeout; }
    
    // Schema-aware setters
    void setLocalSchema(const ScratchBird::string& schema) { m_localSchema = schema; }
    void setRemoteSchema(const ScratchBird::string& schema) { m_remoteSchema = schema; }
    void setSchemaMode(SchemaResolutionMode mode) { m_schemaMode = mode; }

    // Operations
    EDS::Connection* getConnection(Jrd::thread_db* tdbb, EDS::TraScope traScope);
    void releaseConnection(Jrd::thread_db* tdbb, EDS::Connection* conn);
    bool validateTarget() const;
    
    // Schema-aware operations
    ScratchBird::string resolveRemoteSchema(Jrd::thread_db* tdbb, const ScratchBird::string& currentSchema) const;
    bool validateSchemaAccess(Jrd::thread_db* tdbb, const ScratchBird::string& remoteSchema) const;
    
    // Serialization for system table storage
    void loadFromRecord(Jrd::thread_db* tdbb, Jrd::Record* record);
    void saveToRecord(Jrd::thread_db* tdbb, Jrd::Record* record) const;

private:
    Jrd::MetaName m_name;
    ScratchBird::string m_target;
    ScratchBird::string m_user;
    ScratchBird::string m_password;
    ScratchBird::string m_role;
    ScratchBird::string m_provider;
    int m_flags;
    int m_poolMin;
    int m_poolMax;
    int m_timeout;
    
    // Schema-aware properties
    ScratchBird::string m_localSchema;
    ScratchBird::string m_remoteSchema;
    SchemaResolutionMode m_schemaMode;
    USHORT m_schemaDepth;               // Cached schema depth for optimization
    
    // Connection management
    mutable ScratchBird::Mutex m_mutex;
    ScratchBird::Array<EDS::Connection*> m_connections;
};

// Database link manager
class LinkManager : public ScratchBird::PermanentStorage
{
public:
    explicit LinkManager(ScratchBird::MemoryPool& pool);
    ~LinkManager();

    // Link management
    void createLink(Jrd::thread_db* tdbb, const DatabaseLink& link);
    void dropLink(Jrd::thread_db* tdbb, const Jrd::MetaName& name);
    DatabaseLink* findLink(Jrd::thread_db* tdbb, const Jrd::MetaName& name);
    void loadLinks(Jrd::thread_db* tdbb);
    void clearCache();

    // Connection operations
    EDS::Connection* getConnection(Jrd::thread_db* tdbb, const Jrd::MetaName& linkName, 
                                  EDS::TraScope traScope);
    void releaseConnection(Jrd::thread_db* tdbb, const Jrd::MetaName& linkName, 
                          EDS::Connection* conn);

    // Validation
    bool validateLinkSyntax(const ScratchBird::string& target) const;
    bool validateLinkAccess(Jrd::thread_db* tdbb, const Jrd::MetaName& linkName) const;

    // Attachment management
    static LinkManager* getManager(Jrd::thread_db* tdbb);
    static void attachmentEnd(Jrd::thread_db* tdbb, Jrd::Attachment* att);

private:
    typedef ScratchBird::ObjectsArray<DatabaseLink> LinkArray;
    
    LinkArray m_links;
    mutable ScratchBird::Mutex m_mutex;
    bool m_loaded;
    
    DatabaseLink* findLinkNoLock(const Jrd::MetaName& name);
    void loadLinksFromSystemTable(Jrd::thread_db* tdbb);
};

// Remote table reference for SQL parsing
class RemoteTableRef
{
public:
    RemoteTableRef(const Jrd::MetaName& tableName, const Jrd::MetaName& linkName)
        : m_tableName(tableName), m_linkName(linkName) {}

    const Jrd::MetaName& getTableName() const { return m_tableName; }
    const Jrd::MetaName& getLinkName() const { return m_linkName; }
    
    bool isRemote() const { return !m_linkName.isEmpty(); }
    
    ScratchBird::string getFullName() const {
        if (isRemote()) {
            return ScratchBird::string(m_tableName.c_str()) + "@" + ScratchBird::string(m_linkName.c_str());
        }
        return ScratchBird::string(m_tableName.c_str());
    }

private:
    Jrd::MetaName m_tableName;
    Jrd::MetaName m_linkName;
};

// DDL operations
class LinkDDL
{
public:
    // CREATE DATABASE LINK
    static void createDatabaseLink(Jrd::thread_db* tdbb, const Jrd::MetaName& name,
                                  const ScratchBird::string& target, const ScratchBird::string& user,
                                  const ScratchBird::string& password, const ScratchBird::string& role,
                                  int flags, int poolMin, int poolMax, int timeout,
                                  const ScratchBird::string& localSchema = "",
                                  const ScratchBird::string& remoteSchema = "",
                                  SchemaResolutionMode schemaMode = SCHEMA_MODE_NONE);

    // DROP DATABASE LINK  
    static void dropDatabaseLink(Jrd::thread_db* tdbb, const Jrd::MetaName& name,
                                bool ifExists = false);

    // ALTER DATABASE LINK
    static void alterDatabaseLink(Jrd::thread_db* tdbb, const Jrd::MetaName& name,
                                 const ScratchBird::string& newTarget, const ScratchBird::string& newUser,
                                 const ScratchBird::string& newPassword, const ScratchBird::string& newRole,
                                 int newFlags, const ScratchBird::string& newLocalSchema = "",
                                 const ScratchBird::string& newRemoteSchema = "",
                                 SchemaResolutionMode newSchemaMode = SCHEMA_MODE_NONE);

private:
    static void validateLinkName(const Jrd::MetaName& name);
    static void validateLinkTarget(const ScratchBird::string& target);
    static void checkLinkPermissions(Jrd::thread_db* tdbb);
};

} // namespace DatabaseLinks

#endif // JRD_DATABASE_LINK_H