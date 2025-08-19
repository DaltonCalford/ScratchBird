#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    {
        auto ast = parse_sql("CREATE TABLESPACE fast LOCATION '/mnt/nvme1'");
        assert(ast.kind == NodeKind::DdlTablespace);
        assert(ast.ddlTablespace.action == "create");
        assert(ast.ddlTablespace.name == "fast");
    }
    {
        auto ast =
            parse_sql("ALTER TABLESPACE fast ADD FILE '/mnt/nvme1/ts02' SIZE 100G AUTOEXTEND");
        assert(ast.kind == NodeKind::DdlTablespace);
        assert(ast.ddlTablespace.action == "alter");
    }
    {
        auto ast = parse_sql("DROP TABLESPACE fast");
        assert(ast.kind == NodeKind::DdlTablespace);
        assert(ast.ddlTablespace.action == "drop");
    }
    {
        auto ast = parse_sql("DETACH TABLESPACE cold KEEP FILES");
        assert(ast.kind == NodeKind::DdlTablespace);
        assert(ast.ddlTablespace.action == "detach");
    }
    {
        auto ast = parse_sql("ATTACH TABLESPACE cold FROM '/mnt/cold' OPTIONS (ro=true)");
        assert(ast.kind == NodeKind::DdlTablespace);
        assert(ast.ddlTablespace.action == "attach");
    }
    return 0;
}
