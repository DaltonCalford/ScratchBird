/*
 * Database Link DDL Nodes Implementation
 */

#include "firebird.h"
#include "DatabaseLinkNodes.h"
#include "dsql.h"
#include "../jrd/DatabaseLink.h"
#include "../jrd/jrd.h"
#include "../jrd/tra.h"

using namespace Jrd;
using namespace ScratchBird;
using namespace DatabaseLinks;

//
// CreateDatabaseLinkNode implementation
//

void CreateDatabaseLinkNode::print(string& text, Array<dsql_nod*>& nodes) const
{
    text += "CREATE DATABASE LINK ";
    text += name.c_str();
    text += " CONNECT TO '";
    text += target.c_str();
    text += "'";
    
    if (!user.isEmpty())
    {
        text += " AS USER '";
        text += user.c_str();
        text += "'";
        
        if (!password.isEmpty())
        {
            text += " PASSWORD '***'";
        }
        
        if (!role.isEmpty())
        {
            text += " ROLE '";
            text += role.c_str();
            text += "'";
        }
    }
    else if (flags & LINK_TRUSTED_AUTH)
    {
        text += " USING TRUSTED AUTHENTICATION";
    }
    
    if (flags & LINK_POOLING_ENABLED)
    {
        text += " WITH CONNECTION POOL (MIN=";
        text += string(poolMin);
        text += ", MAX=";
        text += string(poolMax);
        text += ")";
    }
    
    if (timeout != 3600)  // Default timeout
    {
        text += " SESSION TIMEOUT ";
        text += string(timeout);
    }
}

CreateDatabaseLinkNode* CreateDatabaseLinkNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
    return this;
}

void CreateDatabaseLinkNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
{
    // Check permissions
    if (!tdbb->getAttachment()->att_user->locksmith(tdbb, CREATE_DATABASE_LINK_PRIVILEGE))
    {
        ERR_post(Arg::Gds(isc_no_priv) << 
                 Arg::Str("database link") << 
                 Arg::Str("create"));
    }
    
    try 
    {
        // Create database link using DDL interface
        LinkDDL::createDatabaseLink(tdbb, name, target, user, password, role, 
                                   flags, poolMin, poolMax, timeout);
    }
    catch (const Exception& ex)
    {
        if (createIfNotExistsOnly && ex.containsErrorCode(isc_dsql_dup_option))
        {
            // IF NOT EXISTS specified and link already exists - ignore error
            return;
        }
        throw;
    }
}

//
// DropDatabaseLinkNode implementation
//

void DropDatabaseLinkNode::print(string& text, Array<dsql_nod*>& nodes) const
{
    text += "DROP DATABASE LINK ";
    if (silent)
        text += "IF EXISTS ";
    text += name.c_str();
}

DropDatabaseLinkNode* DropDatabaseLinkNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
    return this;
}

void DropDatabaseLinkNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
{
    // Check permissions
    if (!tdbb->getAttachment()->att_user->locksmith(tdbb, DROP_DATABASE_LINK_PRIVILEGE))
    {
        ERR_post(Arg::Gds(isc_no_priv) << 
                 Arg::Str("database link") << 
                 Arg::Str("drop"));
    }
    
    try
    {
        LinkDDL::dropDatabaseLink(tdbb, name, silent);
    }
    catch (const Exception& ex)
    {
        if (silent && ex.containsErrorCode(isc_dsql_relation_err))
        {
            // IF EXISTS specified and link doesn't exist - ignore error
            return;
        }
        throw;
    }
}

//
// AlterDatabaseLinkNode implementation  
//

void AlterDatabaseLinkNode::print(string& text, Array<dsql_nod*>& nodes) const
{
    text += "ALTER DATABASE LINK ";
    text += name.c_str();
    
    if (!newTarget.isEmpty())
    {
        text += " CONNECT TO '";
        text += newTarget.c_str();
        text += "'";
    }
    
    if (!newUser.isEmpty())
    {
        text += " AS USER '";
        text += newUser.c_str();
        text += "'";
        
        if (!newPassword.isEmpty())
        {
            text += " PASSWORD '***'";
        }
        
        if (!newRole.isEmpty())
        {
            text += " ROLE '";
            text += newRole.c_str();
            text += "'";
        }
    }
}

AlterDatabaseLinkNode* AlterDatabaseLinkNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
    return this;
}

void AlterDatabaseLinkNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
{
    // Check permissions
    if (!tdbb->getAttachment()->att_user->locksmith(tdbb, ALTER_DATABASE_LINK_PRIVILEGE))
    {
        ERR_post(Arg::Gds(isc_no_priv) << 
                 Arg::Str("database link") << 
                 Arg::Str("alter"));
    }
    
    LinkDDL::alterDatabaseLink(tdbb, name, newTarget, newUser, newPassword, newRole, newFlags);
}

//
// RemoteTableNode implementation
//

void RemoteTableNode::print(string& text, Array<dsql_nod*>& nodes) const
{
    text += tableName.c_str();
    text += "@";
    text += linkName.c_str();
    
    if (!alias.isEmpty())
    {
        text += " ";
        text += alias.c_str();
    }
}

RemoteTableNode* RemoteTableNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
    // Validate that the database link exists
    LinkManager* manager = LinkManager::getManager(dsqlScratch->getAttachment()->att_tdbb);
    DatabaseLink* link = manager->findLink(dsqlScratch->getAttachment()->att_tdbb, linkName);
    
    if (!link)
    {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
                 Arg::Gds(isc_dsql_relation_err) << Arg::Str(linkName));
    }
    
    // Check access permissions
    if (!manager->validateLinkAccess(dsqlScratch->getAttachment()->att_tdbb, linkName))
    {
        ERR_post(Arg::Gds(isc_no_priv) << 
                 Arg::Str("database link") << 
                 Arg::Str(linkName.c_str()));
    }
    
    return this;
}

RecordSource* RemoteTableNode::compile(thread_db* tdbb, OptimizerBlk* opt, bool innerSubStream)
{
    // Get connection to remote database
    LinkManager* manager = LinkManager::getManager(tdbb);
    connection = manager->getConnection(tdbb, linkName, EDS::traCommon);
    
    if (!connection)
    {
        ERR_post(Arg::Gds(isc_unavailable) << 
                 Arg::Str("database link connection") << 
                 Arg::Str(linkName.c_str()));
    }
    
    // Create remote record source
    // This would integrate with the EDS (External Data Sources) system
    // to create a record source that fetches data from the remote table
    
    // For now, return a placeholder - full implementation would require
    // creating a specialized RecordSource subclass for remote tables
    return nullptr;  // TODO: Implement RemoteRecordSource
}

// Utility functions for privilege checking
namespace {
    const UCHAR CREATE_DATABASE_LINK_PRIVILEGE = 1;
    const UCHAR DROP_DATABASE_LINK_PRIVILEGE = 2; 
    const UCHAR ALTER_DATABASE_LINK_PRIVILEGE = 3;
}