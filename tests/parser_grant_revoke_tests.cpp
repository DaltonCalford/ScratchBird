#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    {
        auto ast = parse_sql("GRANT SELECT, UPDATE ON TABLE t TO ROLE r WITH GRANT OPTION");
        assert(ast.kind == NodeKind::DdlGrant);
        assert(ast.grantStmt.object_type == "table");
        assert(ast.grantStmt.object_name == "t");
        assert(ast.grantStmt.privileges.find("SELECT") != std::string::npos);
        assert(ast.grantStmt.with_grant_option);
    }
    {
        auto ast = parse_sql("REVOKE ALL PRIVILEGES ON TABLE t FROM USER alice RESTRICT");
        assert(ast.kind == NodeKind::DdlRevoke);
        assert(ast.grantStmt.object_type == "table");
        assert(ast.grantStmt.object_name == "t");
        assert(ast.grantStmt.privileges.find("ALL") != std::string::npos);
    }
    return 0;
}
