/*
 * Database Link Nodes - Schema-aware database link DDL operations
 * Full implementation for hierarchical schema support
 */

#ifndef DSQL_DATABASE_LINK_NODES_H
#define DSQL_DATABASE_LINK_NODES_H

#include "DdlNodes.h"
#include "../jrd/MetaName.h"
#include "../jrd/DatabaseLink.h"
#include "../common/classes/fb_string.h"

using namespace Jrd;
using namespace DatabaseLinks;

namespace Jrd {

// Schema resolution mode enumeration
enum SchemaLinkMode {
    LINK_SCHEMA_NONE = 0,
    LINK_SCHEMA_FIXED = 1,
    LINK_SCHEMA_CONTEXT_AWARE = 2,
    LINK_SCHEMA_HIERARCHICAL = 3,
    LINK_SCHEMA_MIRROR = 4
};

// CREATE DATABASE LINK DDL node
class CreateDatabaseLinkNode : public DdlNode
{
public:
    CreateDatabaseLinkNode(MemoryPool& pool, const MetaName& aName)
        : DdlNode(pool), 
          name(pool, aName), 
          createIfNotExistsOnly(false),
          serverName(pool),
          databasePath(pool),
          userName(pool),
          password(pool),
          localSchema(pool),
          remoteSchema(pool),
          schemaMode(LINK_SCHEMA_NONE),
          schemaDepth(0)
    {
    }

    virtual ScratchBird::string internalPrint(NodePrinter& printer) const override
    {
        ScratchBird::string result = "CREATE DATABASE LINK ";
        if (createIfNotExistsOnly)
            result += "IF NOT EXISTS ";
        result += name.toString();
        result += " TO '";
        result += serverName;
        result += ":";
        result += databasePath;
        result += "'";
        
        if (!userName.isEmpty()) {
            result += " USER '";
            result += userName;
            result += "'";
        }
        
        if (!password.isEmpty()) {
            result += " PASSWORD '***'"; // Don't expose password in print
        }
        
        if (schemaMode != LINK_SCHEMA_NONE) {
            result += " SCHEMA_MODE ";
            switch (schemaMode) {
                case LINK_SCHEMA_FIXED: result += "FIXED"; break;
                case LINK_SCHEMA_CONTEXT_AWARE: result += "CONTEXT_AWARE"; break;
                case LINK_SCHEMA_HIERARCHICAL: result += "HIERARCHICAL"; break;
                case LINK_SCHEMA_MIRROR: result += "MIRROR"; break;
                default: result += "NONE"; break;
            }
        }
        
        if (!localSchema.isEmpty()) {
            result += " LOCAL_SCHEMA '";
            result += localSchema;
            result += "'";
        }
        
        if (!remoteSchema.isEmpty()) {
            result += " REMOTE_SCHEMA '";
            result += remoteSchema;
            result += "'";
        }
        
        return result;
    }

    virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction) override;
    virtual CreateDatabaseLinkNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;

    // Link parameters
    MetaName name;
    bool createIfNotExistsOnly;
    ScratchBird::string serverName;
    ScratchBird::string databasePath;
    ScratchBird::string userName;
    ScratchBird::string password;
    ScratchBird::string localSchema;
    ScratchBird::string remoteSchema;
    SchemaLinkMode schemaMode;
    SSHORT schemaDepth;
    
    // Setters for parsing
    void setServerDatabase(const ScratchBird::string& server, const ScratchBird::string& database) {
        serverName = server;
        databasePath = database;
    }
    
    void setCredentials(const ScratchBird::string& user, const ScratchBird::string& pass) {
        userName = user;
        password = pass;
    }
    
    void setSchemaMode(SchemaLinkMode mode, const ScratchBird::string& local = ScratchBird::string(), 
                      const ScratchBird::string& remote = ScratchBird::string()) {
        schemaMode = mode;
        localSchema = local;
        remoteSchema = remote;
        
        // Calculate schema depth for optimization
        if (!remoteSchema.isEmpty()) {
            schemaDepth = calculateSchemaDepth(remoteSchema);
        }
    }
    
private:
    SSHORT calculateSchemaDepth(const ScratchBird::string& path) {
        if (path.isEmpty()) return 0;
        SSHORT depth = 1;
        for (size_t i = 0; i < path.length(); i++) {
            if (path[i] == '.') depth++;
        }
        return depth;
    }
};

// DROP DATABASE LINK DDL node
class DropDatabaseLinkNode : public DdlNode
{
public:
    DropDatabaseLinkNode(MemoryPool& pool, const MetaName& aName, bool aSilent = false)
        : DdlNode(pool), name(pool, aName), silent(aSilent)
    {
    }

    virtual ScratchBird::string internalPrint(NodePrinter& printer) const override
    {
        ScratchBird::string text = "DROP DATABASE LINK ";
        if (silent)
            text += "IF EXISTS ";
        text += name.toString();
        return text;
    }

    virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction) override;
    virtual DropDatabaseLinkNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;

    MetaName name;
    bool silent;
};

// ALTER DATABASE LINK DDL node
class AlterDatabaseLinkNode : public DdlNode
{
public:
    AlterDatabaseLinkNode(MemoryPool& pool, const MetaName& aName)
        : DdlNode(pool), 
          name(pool, aName),
          newServerName(pool),
          newDatabasePath(pool),
          newUserName(pool),
          newPassword(pool),
          newLocalSchema(pool),
          newRemoteSchema(pool),
          newSchemaMode(LINK_SCHEMA_NONE),
          alterServer(false),
          alterCredentials(false),
          alterSchema(false)
    {
    }

    virtual ScratchBird::string internalPrint(NodePrinter& printer) const override
    {
        ScratchBird::string result = "ALTER DATABASE LINK ";
        result += name.toString();
        
        if (alterServer) {
            result += " TO '";
            result += newServerName;
            result += ":";
            result += newDatabasePath;
            result += "'";
        }
        
        if (alterCredentials) {
            if (!newUserName.isEmpty()) {
                result += " USER '";
                result += newUserName;
                result += "'";
            }
            if (!newPassword.isEmpty()) {
                result += " PASSWORD '***'"; // Don't expose password
            }
        }
        
        if (alterSchema) {
            result += " SCHEMA_MODE ";
            switch (newSchemaMode) {
                case LINK_SCHEMA_FIXED: result += "FIXED"; break;
                case LINK_SCHEMA_CONTEXT_AWARE: result += "CONTEXT_AWARE"; break;
                case LINK_SCHEMA_HIERARCHICAL: result += "HIERARCHICAL"; break;
                case LINK_SCHEMA_MIRROR: result += "MIRROR"; break;
                default: result += "NONE"; break;
            }
            
            if (!newLocalSchema.isEmpty()) {
                result += " LOCAL_SCHEMA '";
                result += newLocalSchema;
                result += "'";
            }
            
            if (!newRemoteSchema.isEmpty()) {
                result += " REMOTE_SCHEMA '";
                result += newRemoteSchema;
                result += "'";
            }
        }
        
        return result;
    }

    virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction) override;
    virtual AlterDatabaseLinkNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override;

    MetaName name;
    
    // Alteration parameters
    ScratchBird::string newServerName;
    ScratchBird::string newDatabasePath;
    ScratchBird::string newUserName;
    ScratchBird::string newPassword;
    ScratchBird::string newLocalSchema;
    ScratchBird::string newRemoteSchema;
    SchemaLinkMode newSchemaMode;
    
    // Alteration flags
    bool alterServer;
    bool alterCredentials;
    bool alterSchema;
    
    // Setters for parsing
    void setNewServerDatabase(const ScratchBird::string& server, const ScratchBird::string& database) {
        newServerName = server;
        newDatabasePath = database;
        alterServer = true;
    }
    
    void setNewCredentials(const ScratchBird::string& user, const ScratchBird::string& pass) {
        newUserName = user;
        newPassword = pass;
        alterCredentials = true;
    }
    
    void setNewSchemaMode(SchemaLinkMode mode, const ScratchBird::string& local = ScratchBird::string(), 
                         const ScratchBird::string& remote = ScratchBird::string()) {
        newSchemaMode = mode;
        newLocalSchema = local;
        newRemoteSchema = remote;
        alterSchema = true;
    }
};

} // namespace Jrd

#endif // DSQL_DATABASE_LINK_NODES_H