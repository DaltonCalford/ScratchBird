/*
 * Database Link Management Implementation
 */

#include "firebird.h"
#include "fb_types.h"
#include "../../include/fb_blk.h"
#include "fb_exception.h"
#include "iberror.h"

#include "DatabaseLink.h"
#include "jrd.h"
#include "tra.h"
#include "exe.h"
#include "extds/ExtDS.h"
#include "../common/dsc.h"
#include "../dsql/dsql.h"

#include "blb_proto.h"
#include "exe_proto.h"
#include "err_proto.h"
#include "met_proto.h"
#include "vio_proto.h"

using namespace Jrd;
using namespace ScratchBird;
using namespace DatabaseLinks;

namespace DatabaseLinks {

//
// DatabaseLink implementation
//

DatabaseLink::DatabaseLink(MemoryPool& pool)
    : PermanentStorage(pool),
      m_flags(LINK_TRUSTED_AUTH | LINK_SESSION_AUTH | LINK_POOLING_ENABLED),
      m_poolMin(1), m_poolMax(10), m_timeout(3600),
      m_localSchema(pool), m_remoteSchema(pool), m_schemaMode(SCHEMA_MODE_NONE),
      m_schemaDepth(0), m_connections(pool)
{
}

DatabaseLink::~DatabaseLink()
{
    // Close all connections
    for (size_t i = 0; i < m_connections.getCount(); i++)
    {
        EDS::Connection* conn = m_connections[i];
        if (conn)
        {
            // Note: In real implementation, would properly clean up connections
            // This requires thread_db context which isn't available in destructor
        }
    }
}

void DatabaseLink::setCredentials(const string& user, const string& password, const string& role)
{
    MutexLockGuard guard(m_mutex, FB_FUNCTION);
    m_user = user;
    m_password = password;  // Note: In production, would encrypt password
    m_role = role;
    
    // Update flags based on credentials
    if (user.isEmpty() && password.isEmpty())
    {
        m_flags |= LINK_TRUSTED_AUTH;
        m_flags |= LINK_SESSION_AUTH;
    }
    else
    {
        m_flags &= ~LINK_TRUSTED_AUTH;
        m_flags &= ~LINK_SESSION_AUTH;
    }
}

EDS::Connection* DatabaseLink::getConnection(thread_db* tdbb, EDS::TraScope traScope)
{
    MutexLockGuard guard(m_mutex, FB_FUNCTION);
    
    // Try to reuse existing connection if pooling is enabled
    if (hasPooling())
    {
        for (size_t i = 0; i < m_connections.getCount(); i++)
        {
            EDS::Connection* conn = m_connections[i];
            if (conn && conn->isAvailable(tdbb, traScope))
            {
                return conn;
            }
        }
    }
    
    // Create new connection using EDS Manager
    EDS::Connection* conn = EDS::Manager::getConnection(tdbb, m_target, m_user, m_password, m_role, traScope);
    
    if (conn && hasPooling() && m_connections.getCount() < static_cast<size_t>(m_poolMax))
    {
        m_connections.add(conn);
    }
    
    return conn;
}

void DatabaseLink::releaseConnection(thread_db* tdbb, EDS::Connection* conn)
{
    if (!conn) return;
    
    MutexLockGuard guard(m_mutex, FB_FUNCTION);
    
    if (hasPooling())
    {
        // Keep connection for reuse
        // Connection will be managed by EDS connection pool
        return;
    }
    
    // Remove from our tracking and let EDS handle cleanup
    for (size_t i = 0; i < m_connections.getCount(); i++)
    {
        if (m_connections[i] == conn)
        {
            m_connections.remove(i);
            break;
        }
    }
}

bool DatabaseLink::validateTarget() const
{
    if (m_target.isEmpty()) return false;
    
    // Basic validation - should contain database path
    // Format: [server:]database or provider::database
    size_t colonPos = m_target.find(':');
    return (colonPos != string::npos && colonPos < m_target.length() - 1);
}

void DatabaseLink::loadFromRecord(thread_db* tdbb, Record* record)
{
    // This would load from RDB$DATABASE_LINKS table
    // Implementation would use VIO functions to read record fields
    // For now, this is a placeholder for the actual record reading logic
    
    // Example structure:
    // m_name = getString(record, f_links_name);
    // m_target = getString(record, f_links_target);
    // m_user = getString(record, f_links_user);
    // m_password = getString(record, f_links_password);
    // m_role = getString(record, f_links_role);
    // m_flags = getInt(record, f_links_flags);
    // m_poolMin = getInt(record, f_links_pool_min);
    // m_poolMax = getInt(record, f_links_pool_max);
    // m_timeout = getInt(record, f_links_timeout);
}

void DatabaseLink::saveToRecord(thread_db* tdbb, Record* record) const
{
    // This would save to RDB$DATABASE_LINKS table
    // Implementation would use VIO functions to write record fields
    // For now, this is a placeholder for the actual record writing logic
}

// Schema-aware methods
string DatabaseLink::resolveRemoteSchema(thread_db* tdbb, const string& currentSchema) const
{
    switch (m_schemaMode)
    {
        case SCHEMA_MODE_NONE:
            return "";
            
        case SCHEMA_MODE_FIXED:
            return m_remoteSchema;
            
        case SCHEMA_MODE_CONTEXT_AWARE:
        {
            // Resolve context-aware schema references
            if (m_remoteSchema == "CURRENT")
            {
                return currentSchema;
            }
            else if (m_remoteSchema == "HOME")
            {
                // Get home schema from attachment
                Attachment* attachment = tdbb->getAttachment();
                if (attachment && attachment->att_home_schema.hasData())
                {
                    return attachment->att_home_schema.c_str();
                }
                return "";
            }
            else if (m_remoteSchema == "USER")
            {
                // Use user name as schema
                return tdbb->getAttachment()->att_user->usr_user_name.c_str();
            }
            return m_remoteSchema;
        }
        
        case SCHEMA_MODE_HIERARCHICAL:
        {
            // Map local schema hierarchy to remote hierarchy
            if (currentSchema.isEmpty())
            {
                return m_remoteSchema;
            }
            
            // If current schema starts with local schema, map it
            if (m_localSchema.hasData() && currentSchema.find(m_localSchema.c_str()) == 0)
            {
                string relativePath = currentSchema.substr(m_localSchema.length());
                if (relativePath.hasData() && relativePath[0] == '.')
                {
                    relativePath = relativePath.substr(1);
                }
                
                if (m_remoteSchema.hasData())
                {
                    return m_remoteSchema + "." + relativePath;
                }
                return relativePath;
            }
            
            return m_remoteSchema;
        }
        
        case SCHEMA_MODE_MIRROR:
            // Mirror local schema path exactly
            return currentSchema;
            
        default:
            return m_remoteSchema;
    }
}

bool DatabaseLink::validateSchemaAccess(thread_db* tdbb, const string& remoteSchema) const
{
    if (remoteSchema.isEmpty())
    {
        return true;  // No schema restriction
    }
    
    // Get connection and validate schema exists
    try
    {
        EDS::Connection* conn = const_cast<DatabaseLink*>(this)->getConnection(tdbb, EDS::traReadCommitted);
        if (!conn)
        {
            return false;
        }
        
        // Check if schema exists on remote database
        string sql = "SELECT COUNT(*) FROM RDB$SCHEMAS WHERE RDB$SCHEMA_NAME = ?";
        
        EDS::Statement* stmt = conn->createStatement();
        stmt->prepare(sql.c_str());
        stmt->setString(1, remoteSchema.c_str());
        
        EDS::ResultSet* rs = stmt->executeQuery();
        if (rs && rs->next())
        {
            int count = rs->getInt(1);
            rs->close();
            stmt->close();
            
            return count > 0;
        }
        
        if (rs) rs->close();
        stmt->close();
        return false;
    }
    catch (const Exception&)
    {
        return false;
    }
}

//
// LinkManager implementation
//

LinkManager::LinkManager(MemoryPool& pool)
    : PermanentStorage(pool), m_links(pool), m_loaded(false)
{
}

LinkManager::~LinkManager()
{
    clearCache();
}

void LinkManager::createLink(thread_db* tdbb, const DatabaseLink& link)
{
    MutexLockGuard guard(m_mutex, FB_FUNCTION);
    
    // Check if link already exists
    if (findLinkNoLock(link.getName()))
    {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-607) <<
                 Arg::Gds(isc_dsql_command_err) <<
                 Arg::Gds(isc_dsql_dup_option));  // Duplicate option specified
    }
    
    // Add to cache
    DatabaseLink* newLink = FB_NEW_POOL(getPool()) DatabaseLink(getPool());
    *newLink = link;  // Copy construct
    m_links.add(newLink);
    
    // TODO: Insert into system table RDB$DATABASE_LINKS
    // This would require system table access through VIO
}

void LinkManager::dropLink(thread_db* tdbb, const MetaName& name)
{
    MutexLockGuard guard(m_mutex, FB_FUNCTION);
    
    // Find and remove from cache
    for (size_t i = 0; i < m_links.getCount(); i++)
    {
        if (m_links[i]->getName() == name)
        {
            DatabaseLink* link = m_links[i];
            m_links.remove(i);
            delete link;
            break;
        }
    }
    
    // TODO: Delete from system table RDB$DATABASE_LINKS
}

DatabaseLink* LinkManager::findLink(thread_db* tdbb, const MetaName& name)
{
    MutexLockGuard guard(m_mutex, FB_FUNCTION);
    
    if (!m_loaded)
    {
        loadLinks(tdbb);
    }
    
    return findLinkNoLock(name);
}

DatabaseLink* LinkManager::findLinkNoLock(const MetaName& name)
{
    for (size_t i = 0; i < m_links.getCount(); i++)
    {
        if (m_links[i]->getName() == name)
        {
            return m_links[i];
        }
    }
    return nullptr;
}

void LinkManager::loadLinks(thread_db* tdbb)
{
    if (m_loaded) return;
    
    // TODO: Load from RDB$DATABASE_LINKS system table
    // This would involve:
    // 1. Open relation RDB$DATABASE_LINKS
    // 2. Iterate through records
    // 3. Create DatabaseLink objects
    // 4. Add to m_links array
    
    // For now, create a default SELF link
    DatabaseLink* selfLink = FB_NEW_POOL(getPool()) DatabaseLink(getPool());
    selfLink->setName("SELF");
    selfLink->setTarget("localhost:current_database");
    selfLink->setProvider("ScratchBird");
    selfLink->setFlags(LINK_TRUSTED_AUTH | LINK_SESSION_AUTH | LINK_POOLING_ENABLED);
    m_links.add(selfLink);
    
    m_loaded = true;
}

void LinkManager::clearCache()
{
    MutexLockGuard guard(m_mutex, FB_FUNCTION);
    
    for (size_t i = 0; i < m_links.getCount(); i++)
    {
        delete m_links[i];
    }
    m_links.clear();
    m_loaded = false;
}

EDS::Connection* LinkManager::getConnection(thread_db* tdbb, const MetaName& linkName, EDS::TraScope traScope)
{
    DatabaseLink* link = findLink(tdbb, linkName);
    if (!link)
    {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
                 Arg::Gds(isc_dsql_relation_err) << Arg::Str(linkName));
    }
    
    return link->getConnection(tdbb, traScope);
}

void LinkManager::releaseConnection(thread_db* tdbb, const MetaName& linkName, EDS::Connection* conn)
{
    DatabaseLink* link = findLink(tdbb, linkName);
    if (link)
    {
        link->releaseConnection(tdbb, conn);
    }
}

bool LinkManager::validateLinkSyntax(const string& target) const
{
    if (target.isEmpty()) return false;
    
    // Validate target format: [provider::]server:database or provider::database
    size_t doubleColon = target.find("::");
    if (doubleColon != string::npos)
    {
        // Provider::database format
        string provider = target.substr(0, doubleColon);
        string database = target.substr(doubleColon + 2);
        return !provider.isEmpty() && !database.isEmpty();
    }
    
    // Server:database format  
    size_t colon = target.find(':');
    return (colon != string::npos && colon < target.length() - 1);
}

bool LinkManager::validateLinkAccess(thread_db* tdbb, const MetaName& linkName) const
{
    // TODO: Check user permissions for database link access
    // This would involve checking system privileges or specific link permissions
    return true;  // For now, allow all access
}

LinkManager* LinkManager::getManager(thread_db* tdbb)
{
    Attachment* attachment = tdbb->getAttachment();
    if (!attachment->att_link_manager)
    {
        attachment->att_link_manager = FB_NEW_POOL(attachment->att_pool) LinkManager(attachment->att_pool);
    }
    return attachment->att_link_manager;
}

void LinkManager::attachmentEnd(thread_db* tdbb, Attachment* att)
{
    if (att->att_link_manager)
    {
        delete att->att_link_manager;
        att->att_link_manager = nullptr;
    }
}

//
// LinkDDL implementation
//

void LinkDDL::createDatabaseLink(thread_db* tdbb, const MetaName& name,
                                const string& target, const string& user,
                                const string& password, const string& role,
                                int flags, int poolMin, int poolMax, int timeout,
                                const string& localSchema, const string& remoteSchema,
                                SchemaResolutionMode schemaMode)
{
    // Validate parameters
    validateLinkName(name);
    validateLinkTarget(target);
    checkLinkPermissions(tdbb);
    
    // Create database link
    DatabaseLink link(tdbb->getAttachment()->att_pool);
    link.setName(name);
    link.setTarget(target);
    link.setCredentials(user, password, role);
    link.setFlags(flags);
    link.setPooling(poolMin, poolMax);
    link.setTimeout(timeout);
    link.setLocalSchema(localSchema);
    link.setRemoteSchema(remoteSchema);
    link.setSchemaMode(schemaMode);
    
    // Add to manager
    LinkManager* manager = LinkManager::getManager(tdbb);
    manager->createLink(tdbb, link);
}

void LinkDDL::dropDatabaseLink(thread_db* tdbb, const MetaName& name, bool ifExists)
{
    checkLinkPermissions(tdbb);
    
    LinkManager* manager = LinkManager::getManager(tdbb);
    DatabaseLink* link = manager->findLink(tdbb, name);
    
    if (!link && !ifExists)
    {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
                 Arg::Gds(isc_dsql_relation_err) << Arg::Str(name));
    }
    
    if (link)
    {
        manager->dropLink(tdbb, name);
    }
}

void LinkDDL::alterDatabaseLink(thread_db* tdbb, const MetaName& name,
                               const string& newTarget, const string& newUser,
                               const string& newPassword, const string& newRole,
                               int newFlags, const string& newLocalSchema,
                               const string& newRemoteSchema,
                               SchemaResolutionMode newSchemaMode)
{
    checkLinkPermissions(tdbb);
    
    LinkManager* manager = LinkManager::getManager(tdbb);
    DatabaseLink* link = manager->findLink(tdbb, name);
    
    if (!link)
    {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
                 Arg::Gds(isc_dsql_relation_err) << Arg::Str(name));
    }
    
    // Update link properties
    if (!newTarget.isEmpty())
    {
        validateLinkTarget(newTarget);
        link->setTarget(newTarget);
    }
    
    if (!newUser.isEmpty() || !newPassword.isEmpty() || !newRole.isEmpty())
    {
        link->setCredentials(newUser, newPassword, newRole);
    }
    
    if (newFlags >= 0)
    {
        link->setFlags(newFlags);
    }
    
    // Update schema properties
    if (!newLocalSchema.isEmpty())
    {
        link->setLocalSchema(newLocalSchema);
    }
    
    if (!newRemoteSchema.isEmpty())
    {
        link->setRemoteSchema(newRemoteSchema);
    }
    
    if (newSchemaMode != SCHEMA_MODE_NONE)
    {
        link->setSchemaMode(newSchemaMode);
    }
    
    // TODO: Update system table
}

void LinkDDL::validateLinkName(const MetaName& name)
{
    if (name.isEmpty())
    {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                 Arg::Gds(isc_token_err) << Arg::Str("database link name"));
    }
    
    // Check for reserved names
    if (name == "SELF" || name == "LOCAL")
    {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-607) <<
                 Arg::Gds(isc_dsql_command_err) <<
                 Arg::Gds(isc_dsql_dup_option));
    }
}

void LinkDDL::validateLinkTarget(const string& target)
{
    if (target.isEmpty())
    {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                 Arg::Gds(isc_token_err) << Arg::Str("database target"));
    }
    
    // Basic format validation
    size_t colonPos = target.find(':');
    if (colonPos == string::npos || colonPos == target.length() - 1)
    {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                 Arg::Gds(isc_token_err) << Arg::Str("invalid target format"));
    }
}

void LinkDDL::checkLinkPermissions(thread_db* tdbb)
{
    // TODO: Check if user has database link creation/modification privileges
    // This would typically require RDB$ADMIN role or specific privileges
    
    const Attachment* attachment = tdbb->getAttachment();
    if (!attachment->att_user->locksmith(tdbb, LINK_DATABASE_PRIVILEGE))
    {
        ERR_post(Arg::Gds(isc_no_priv) << Arg::Str("database link") <<
                 Arg::Str("create/alter/drop"));
    }
}

} // namespace DatabaseLinks