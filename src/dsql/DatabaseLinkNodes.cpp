/*
 * Database Link Nodes Implementation - Schema-aware database link DDL operations
 * Full implementation for hierarchical schema support
 */

#include "DatabaseLinkNodes.h"
#include "../jrd/Attachment.h"
#include "../jrd/thread_db.h"
#include "../jrd/tra.h"
#include "../jrd/constants.h"
#include "../jrd/DatabaseLink.h"
#include "../dsql/dsql.h"
#include "../common/StatusArg.h"

using namespace Jrd;
using namespace DatabaseLinks;

namespace Jrd {

//
// CreateDatabaseLinkNode implementation
//

void CreateDatabaseLinkNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
{
    if (!tdbb || !dsqlScratch || !transaction) {
        return;
    }
    
    Attachment* attachment = tdbb->getAttachment();
    if (!attachment) {
        return;
    }
    
    // Validate link name
    if (name.isEmpty()) {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                 Arg::Gds(isc_token_err) << Arg::Str("DATABASE LINK name required"));
    }
    
    // Validate server and database path
    if (serverName.isEmpty() || databasePath.isEmpty()) {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                 Arg::Gds(isc_token_err) << Arg::Str("Server and database path required"));
    }
    
    // Check if link already exists
    LinkManager* linkManager = attachment->att_link_manager;
    if (linkManager) {
        DatabaseLink* existingLink = linkManager->findLink(name.toString());
        if (existingLink) {
            if (createIfNotExistsOnly) {
                return; // Silent success
            } else {
                ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-607) <<
                         Arg::Gds(isc_dsql_duplicate_spec) << Arg::Str("DATABASE LINK") << Arg::Str(name.c_str()));
            }
        }
    }
    
    // Validate schema parameters
    if (schemaMode != LINK_SCHEMA_NONE) {
        // Check schema depth limits
        if (schemaDepth > MAX_SCHEMA_DEPTH) {
            ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                     Arg::Gds(isc_token_err) << Arg::Str("Schema depth exceeds maximum"));
        }
        
        // Validate schema path lengths
        if (!localSchema.isEmpty() && localSchema.length() > MAX_SCHEMA_PATH_LENGTH) {
            ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                     Arg::Gds(isc_token_err) << Arg::Str("Local schema path too long"));
        }
        
        if (!remoteSchema.isEmpty() && remoteSchema.length() > MAX_SCHEMA_PATH_LENGTH) {
            ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                     Arg::Gds(isc_token_err) << Arg::Str("Remote schema path too long"));
        }
        
        // Validate required schema parameters for each mode
        switch (schemaMode) {
            case LINK_SCHEMA_FIXED:
                if (remoteSchema.isEmpty()) {
                    ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                             Arg::Gds(isc_token_err) << Arg::Str("FIXED mode requires REMOTE_SCHEMA"));
                }
                break;
                
            case LINK_SCHEMA_HIERARCHICAL:
                if (localSchema.isEmpty() || remoteSchema.isEmpty()) {
                    ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                             Arg::Gds(isc_token_err) << Arg::Str("HIERARCHICAL mode requires both LOCAL_SCHEMA and REMOTE_SCHEMA"));
                }
                break;
                
            case LINK_SCHEMA_CONTEXT_AWARE:
                if (remoteSchema.isEmpty()) {
                    ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                             Arg::Gds(isc_token_err) << Arg::Str("CONTEXT_AWARE mode requires REMOTE_SCHEMA"));
                }
                // Validate context keywords
                if (remoteSchema != "CURRENT" && remoteSchema != "HOME" && remoteSchema != "USER") {
                    // Treat as literal schema path for context-aware mode
                }
                break;
                
            default:
                break;
        }
    }
    
    // Create the database link
    DatabaseLink* newLink = new DatabaseLink(name.toString(), serverName, databasePath, userName, password);
    if (!newLink) {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
                 Arg::Gds(isc_imp_exc) << Arg::Str("Failed to create database link"));
    }
    
    // Configure schema mode
    if (schemaMode != LINK_SCHEMA_NONE) {
        SchemaMode mode = static_cast<SchemaMode>(schemaMode);
        newLink->setSchemaMode(mode);
        newLink->setLocalSchema(localSchema);
        newLink->setRemoteSchema(remoteSchema);
        newLink->setSchemaDepth(schemaDepth);
    }
    
    // Validate the link connection
    if (!newLink->validateConnection(tdbb)) {
        delete newLink;
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
                 Arg::Gds(isc_imp_exc) << Arg::Str("Database link connection validation failed"));
    }
    
    // Add to link manager
    if (!linkManager) {
        // Create new link manager if needed
        linkManager = new LinkManager();
        attachment->att_link_manager = linkManager;
    }
    
    if (!linkManager->addLink(newLink)) {
        delete newLink;
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
                 Arg::Gds(isc_imp_exc) << Arg::Str("Failed to register database link"));
    }
    
    // TODO: Persist to RDB$DATABASE_LINKS system table when runtime supports it
    // For now, links are session-only until full database link infrastructure is available
}

CreateDatabaseLinkNode* CreateDatabaseLinkNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
    return this;
}

//
// DropDatabaseLinkNode implementation
//

void DropDatabaseLinkNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
{
    if (!tdbb || !dsqlScratch || !transaction) {
        return;
    }
    
    Attachment* attachment = tdbb->getAttachment();
    if (!attachment) {
        return;
    }
    
    // Validate link name
    if (name.isEmpty()) {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                 Arg::Gds(isc_token_err) << Arg::Str("DATABASE LINK name required"));
    }
    
    LinkManager* linkManager = attachment->att_link_manager;
    if (!linkManager) {
        if (silent) {
            return; // Silent success
        } else {
            ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-607) <<
                     Arg::Gds(isc_dsql_spec_not_found) << Arg::Str("DATABASE LINK") << Arg::Str(name.c_str()));
        }
    }
    
    // Check if link exists
    DatabaseLink* existingLink = linkManager->findLink(name.toString());
    if (!existingLink) {
        if (silent) {
            return; // Silent success
        } else {
            ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-607) <<
                     Arg::Gds(isc_dsql_spec_not_found) << Arg::Str("DATABASE LINK") << Arg::Str(name.c_str()));
        }
    }
    
    // Remove the link
    if (!linkManager->removeLink(name.toString())) {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
                 Arg::Gds(isc_imp_exc) << Arg::Str("Failed to remove database link"));
    }
    
    // TODO: Remove from RDB$DATABASE_LINKS system table when runtime supports it
}

DropDatabaseLinkNode* DropDatabaseLinkNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
    return this;
}

//
// AlterDatabaseLinkNode implementation
//

void AlterDatabaseLinkNode::execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
{
    if (!tdbb || !dsqlScratch || !transaction) {
        return;
    }
    
    Attachment* attachment = tdbb->getAttachment();
    if (!attachment) {
        return;
    }
    
    // Validate link name
    if (name.isEmpty()) {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                 Arg::Gds(isc_token_err) << Arg::Str("DATABASE LINK name required"));
    }
    
    LinkManager* linkManager = attachment->att_link_manager;
    if (!linkManager) {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-607) <<
                 Arg::Gds(isc_dsql_spec_not_found) << Arg::Str("DATABASE LINK") << Arg::Str(name.c_str()));
    }
    
    // Find existing link
    DatabaseLink* existingLink = linkManager->findLink(name.toString());
    if (!existingLink) {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-607) <<
                 Arg::Gds(isc_dsql_spec_not_found) << Arg::Str("DATABASE LINK") << Arg::Str(name.c_str()));
    }
    
    // Apply alterations
    if (alterServer) {
        if (newServerName.isEmpty() || newDatabasePath.isEmpty()) {
            ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                     Arg::Gds(isc_token_err) << Arg::Str("Server and database path required"));
        }
        
        existingLink->setServerName(newServerName);
        existingLink->setDatabasePath(newDatabasePath);
    }
    
    if (alterCredentials) {
        existingLink->setUserName(newUserName);
        existingLink->setPassword(newPassword);
    }
    
    if (alterSchema) {
        // Validate new schema parameters
        if (newSchemaMode != LINK_SCHEMA_NONE) {
            switch (newSchemaMode) {
                case LINK_SCHEMA_FIXED:
                    if (newRemoteSchema.isEmpty()) {
                        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                                 Arg::Gds(isc_token_err) << Arg::Str("FIXED mode requires REMOTE_SCHEMA"));
                    }
                    break;
                    
                case LINK_SCHEMA_HIERARCHICAL:
                    if (newLocalSchema.isEmpty() || newRemoteSchema.isEmpty()) {
                        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                                 Arg::Gds(isc_token_err) << Arg::Str("HIERARCHICAL mode requires both LOCAL_SCHEMA and REMOTE_SCHEMA"));
                    }
                    break;
                    
                case LINK_SCHEMA_CONTEXT_AWARE:
                    if (newRemoteSchema.isEmpty()) {
                        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
                                 Arg::Gds(isc_token_err) << Arg::Str("CONTEXT_AWARE mode requires REMOTE_SCHEMA"));
                    }
                    break;
                    
                default:
                    break;
            }
        }
        
        SchemaMode mode = static_cast<SchemaMode>(newSchemaMode);
        existingLink->setSchemaMode(mode);
        existingLink->setLocalSchema(newLocalSchema);
        existingLink->setRemoteSchema(newRemoteSchema);
        
        // Recalculate schema depth
        SSHORT newDepth = 0;
        if (!newRemoteSchema.isEmpty()) {
            newDepth = 1;
            for (size_t i = 0; i < newRemoteSchema.length(); i++) {
                if (newRemoteSchema[i] == '.') newDepth++;
            }
        }
        existingLink->setSchemaDepth(newDepth);
    }
    
    // Validate the updated link
    if (!existingLink->validateConnection(tdbb)) {
        ERR_post(Arg::Gds(isc_sqlerr) << Arg::Num(-901) <<
                 Arg::Gds(isc_imp_exc) << Arg::Str("Database link connection validation failed after alteration"));
    }
    
    // TODO: Update RDB$DATABASE_LINKS system table when runtime supports it
}

AlterDatabaseLinkNode* AlterDatabaseLinkNode::dsqlPass(DsqlCompilerScratch* dsqlScratch)
{
    return this;
}

} // namespace Jrd