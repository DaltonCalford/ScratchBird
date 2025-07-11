/*
 * Database Link DDL Nodes
 * AST nodes for database link DDL operations
 */

#ifndef DSQL_DATABASE_LINK_NODES_H
#define DSQL_DATABASE_LINK_NODES_H

#include "DdlNodes.h"
#include "../jrd/DatabaseLink.h"

namespace Jrd {

// CREATE DATABASE LINK node
class CreateDatabaseLinkNode : public DdlNode
{
public:
    explicit CreateDatabaseLinkNode(MemoryPool& pool, const MetaName& aName)
        : DdlNode(pool),
          name(pool, aName),
          target(pool),
          user(pool),
          password(pool),
          role(pool),
          provider(pool),
          flags(DatabaseLinks::LINK_TRUSTED_AUTH | DatabaseLinks::LINK_SESSION_AUTH | DatabaseLinks::LINK_POOLING_ENABLED),
          poolMin(1),
          poolMax(10),
          timeout(3600),
          createIfNotExistsOnly(false)
    {
    }

    virtual void print(ScratchBird::string& text, ScratchBird::Array<dsql_nod*>& nodes) const;
    virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
    virtual CreateDatabaseLinkNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);

public:
    MetaName name;
    ScratchBird::string target;
    ScratchBird::string user;
    ScratchBird::string password;
    ScratchBird::string role;
    ScratchBird::string provider;
    int flags;
    int poolMin;
    int poolMax;
    int timeout;
    bool createIfNotExistsOnly;
    
    // Schema-aware properties
    ScratchBird::string localSchema;
    ScratchBird::string remoteSchema;
    DatabaseLinks::SchemaResolutionMode schemaMode;
};

// DROP DATABASE LINK node
class DropDatabaseLinkNode : public DdlNode
{
public:
    explicit DropDatabaseLinkNode(MemoryPool& pool, const MetaName& aName)
        : DdlNode(pool),
          name(pool, aName),
          silent(false)
    {
    }

    virtual void print(ScratchBird::string& text, ScratchBird::Array<dsql_nod*>& nodes) const;
    virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
    virtual DropDatabaseLinkNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);

public:
    MetaName name;
    bool silent;  // IF EXISTS clause
};

// ALTER DATABASE LINK node
class AlterDatabaseLinkNode : public DdlNode
{
public:
    explicit AlterDatabaseLinkNode(MemoryPool& pool, const MetaName& aName)
        : DdlNode(pool),
          name(pool, aName),
          newTarget(pool),
          newUser(pool),
          newPassword(pool),
          newRole(pool),
          newFlags(-1)  // -1 means not specified
    {
    }

    virtual void print(ScratchBird::string& text, ScratchBird::Array<dsql_nod*>& nodes) const;
    virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
    virtual AlterDatabaseLinkNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);

public:
    MetaName name;
    ScratchBird::string newTarget;
    ScratchBird::string newUser;
    ScratchBird::string newPassword;
    ScratchBird::string newRole;
    int newFlags;
    
    // Schema-aware properties
    ScratchBird::string newLocalSchema;
    ScratchBird::string newRemoteSchema;
    DatabaseLinks::SchemaResolutionMode newSchemaMode;
};

// Remote table reference node for table@link syntax
class RemoteTableNode : public RecSourceNode
{
public:
    explicit RemoteTableNode(MemoryPool& pool, const MetaName& aTableName, const MetaName& aLinkName)
        : RecSourceNode(pool),
          tableName(pool, aTableName),
          linkName(pool, aLinkName),
          alias(pool),
          connection(nullptr)
    {
    }

    virtual void print(ScratchBird::string& text, ScratchBird::Array<dsql_nod*>& nodes) const;
    virtual RemoteTableNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
    virtual RecordSource* compile(thread_db* tdbb, OptimizerBlk* opt, bool innerSubStream);

public:
    MetaName tableName;
    MetaName linkName;
    MetaName alias;
    EDS::Connection* connection;  // Set during compilation
};

} // namespace Jrd

#endif // DSQL_DATABASE_LINK_NODES_H