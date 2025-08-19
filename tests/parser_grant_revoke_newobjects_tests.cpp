#include "scratchbird/engine/parser.h"

#include <algorithm>
#include <cassert>

using namespace scratchbird::engine;

static bool has_warning(const Ast& ast, const char* needle)
{
    for (auto& w : ast.warnings) {
        if (w.find(needle) != std::string::npos)
            return true;
    }
    return false;
}

int main()
{
    {
        auto ast = parse_sql("GRANT USAGE ON FOREIGN SERVER srv TO role_r");
        assert(ast.kind == NodeKind::DdlGrant);
        assert(!ast.grantStmt.privilege_list.empty());
    }
    {
        auto ast = parse_sql("GRANT CREATE IN ON TABLESPACE fast TO user1");
        assert(ast.kind == NodeKind::DdlGrant);
        assert(std::find(ast.grantStmt.privilege_list.begin(), ast.grantStmt.privilege_list.end(),
                         "CREATE IN") != ast.grantStmt.privilege_list.end());
    }
    {
        auto ast = parse_sql("GRANT MANAGE ON TRACE PROFILE slowlog TO admin_r");
        assert(ast.kind == NodeKind::DdlGrant);
    }
    {
        auto ast = parse_sql("GRANT EXECUTE ON JOB etl TO etl_r");
        assert(ast.kind == NodeKind::DdlGrant);
    }
    {
        // Invalid privilege should warn
        auto ast = parse_sql("GRANT SELECT ON TRACE PROFILE slowlog TO u");
        assert(has_warning(ast, "invalid privilege"));
    }
    return 0;
}
