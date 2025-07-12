/*
 * Database Link Nodes - Stub for compilation compatibility  
 * Temporarily disabled advanced database link functionality
 */

#ifndef DSQL_DATABASE_LINK_NODES_H
#define DSQL_DATABASE_LINK_NODES_H

#include "DdlNodes.h"
#include "../jrd/MetaName.h"

using namespace Jrd;

namespace Jrd {

// Minimal stub for database link DDL nodes
class CreateDatabaseLinkNode : public DdlNode
{
public:
    CreateDatabaseLinkNode(MemoryPool& pool, const MetaName& aName)
        : DdlNode(pool), name(pool, aName), createIfNotExistsOnly(false)
    {
    }

    virtual ScratchBird::string internalPrint(NodePrinter& printer) const override
    {
        return ScratchBird::string("CREATE DATABASE LINK ") + name.toString();
    }

    virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction) override
    {
        // Stub implementation - database links not fully implemented
    }

    virtual CreateDatabaseLinkNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
    {
        return this;
    }

    MetaName name;
    bool createIfNotExistsOnly;
};

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

    virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction) override
    {
        // Stub implementation - database links not fully implemented
    }

    virtual DropDatabaseLinkNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
    {
        return this;
    }

    MetaName name;
    bool silent;
};

class AlterDatabaseLinkNode : public DdlNode
{
public:
    AlterDatabaseLinkNode(MemoryPool& pool, const MetaName& aName)
        : DdlNode(pool), name(pool, aName)
    {
    }

    virtual ScratchBird::string internalPrint(NodePrinter& printer) const override
    {
        return ScratchBird::string("ALTER DATABASE LINK ") + name.toString();
    }

    virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction) override
    {
        // Stub implementation - database links not fully implemented
    }

    virtual AlterDatabaseLinkNode* dsqlPass(DsqlCompilerScratch* dsqlScratch) override
    {
        return this;
    }

    MetaName name;
};

} // namespace Jrd

#endif // DSQL_DATABASE_LINK_NODES_H