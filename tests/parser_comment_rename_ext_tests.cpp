#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    // COMMENT ON multi-word object types
    {
        auto ast = parse_sql("COMMENT ON FOREIGN SERVER srv IS 'Remote'");
        assert(ast.kind == NodeKind::DdlComment);
        assert(ast.ddlComment.object_type == "foreign server");
        assert(ast.ddlComment.object_name.find("srv") != std::string::npos);
    }
    {
        auto ast = parse_sql("COMMENT ON TRACE PROFILE slowlog IS 'Slow queries'");
        assert(ast.kind == NodeKind::DdlComment);
        assert(ast.ddlComment.object_type == "trace profile");
    }
    {
        auto ast = parse_sql("COMMENT ON TABLESPACE fast IS 'NVMe'");
        assert(ast.kind == NodeKind::DdlComment);
        assert(ast.ddlComment.object_type == "tablespace");
    }
    {
        auto ast = parse_sql("COMMENT ON PUBLICATION pub IS 'All tables'");
        assert(ast.kind == NodeKind::DdlComment);
        assert(ast.ddlComment.object_type == "publication");
    }
    // ALTER ... RENAME TO multi-word object types
    {
        auto ast = parse_sql("ALTER FOREIGN SERVER srv RENAME TO srv2");
        assert(ast.kind == NodeKind::DdlRename);
        assert(ast.ddlRename.object_type == "foreign server");
        assert(ast.ddlRename.old_name.find("srv") != std::string::npos);
        assert(ast.ddlRename.new_name.find("srv2") != std::string::npos);
    }
    {
        auto ast = parse_sql("ALTER TRACE PROFILE slowlog RENAME TO slowlog2");
        assert(ast.kind == NodeKind::DdlRename);
        assert(ast.ddlRename.object_type == "trace profile");
    }
    {
        auto ast = parse_sql("ALTER DATABASE LINK rem RENAME TO rem2");
        assert(ast.kind == NodeKind::DdlRename);
        assert(ast.ddlRename.object_type == "database link");
    }
    return 0;
}
