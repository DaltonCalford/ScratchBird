#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    {
        auto ast = parse_sql("GRANT SELECT, UPDATE ON TABLE t TO USER alice WITH GRANT OPTION");
        assert(ast.kind == NodeKind::DdlGrant);
        assert(ast.grantStmt.privilege_list.size() == 2);
        assert(ast.grantStmt.privilege_list[0].find("SELECT") != std::string::npos);
        assert(ast.grantStmt.with_grant_option);
    }
    // GRANT tail malformed still parses object and grantees
    {
        auto ast = parse_sql("GRANT SELECT ON TABLE t TO user1, user2, ");
        assert(ast.kind == NodeKind::DdlGrant);
        assert(ast.grantStmt.object_type == "table");
        assert(ast.grantStmt.object_name == "t");
    }
    // System privileges (no ON clause): accept raw
    {
        auto ast = parse_sql("GRANT CREATE USER TO USER admin");
        assert(ast.kind == NodeKind::DdlGrant);
        assert(!ast.grantStmt.privilege_list.empty());
    }
    {
        auto ast = parse_sql("GRANT BACKUP DATABASE TO USER backup");
        assert(ast.kind == NodeKind::DdlGrant);
        assert(!ast.grantStmt.privilege_list.empty());
    }
    {
        // Invalid privilege/object combo should warn and be filtered
        auto ast = parse_sql("GRANT EXECUTE ON TABLE t TO alice");
        bool warned = false;
        for (auto& w : ast.warnings)
            if (w.find("invalid privilege") != std::string::npos)
                warned = true;
        assert(warned);
    }
    {
        auto ast = parse_sql("GRANT EXECUTE ON PACKAGE pkg TO bob");
        assert(ast.kind == NodeKind::DdlGrant);
        assert(ast.grantStmt.privilege_list.size() == 1);
    }
    // Extended matrix samples and object-name context in warnings
    {
        auto ast = parse_sql("GRANT UPDATE, TRIGGER ON VIEW v TO carol");
        assert(ast.kind == NodeKind::DdlGrant);
        bool ctx = false;
        for (auto& w : ast.warnings)
            if (w.find("'v'") != std::string::npos) {
                ctx = true;
                break;
            }
        assert(ctx);
    }
    {
        auto ast = parse_sql("REVOKE ALTER, USAGE ON SEQUENCE s FROM dave");
        assert(ast.kind == NodeKind::DdlRevoke);
        // USAGE and ALTER valid; no warnings expected
    }
    {
        auto ast = parse_sql("GRANT EXECUTE, INSERT ON FUNCTION f TO eve");
        bool warned = false;
        for (auto& w : ast.warnings)
            if (w.find("function") != std::string::npos) {
                warned = true;
                break;
            }
        assert(warned);
    }
    {
        auto ast = parse_sql("GRANT USAGE, UPDATE ON SEQUENCE s TO PUBLIC");
        assert(ast.kind == NodeKind::DdlGrant);
        assert(ast.grantStmt.privilege_list.size() == 2);
    }
    {
        auto ast = parse_sql("REVOKE GRANT OPTION FOR SELECT ON TABLE t FROM alice");
        assert(ast.kind == NodeKind::DdlRevoke);
        assert(ast.grantStmt.revoke_grant_option);
    }
    {
        auto ast = parse_sql("GRANT r1 TO USER alice WITH ADMIN OPTION");
        assert(ast.kind == NodeKind::DdlGrant);
        // role grant: no ON clause, object fields may be empty; admin option set
        assert(ast.grantStmt.admin_option);
    }
    {
        auto ast = parse_sql("REVOKE SELECT, INSERT ON TABLE t FROM ROLE r1");
        assert(ast.kind == NodeKind::DdlRevoke);
        assert(ast.grantStmt.privilege_list.size() == 2);
        assert(ast.grantStmt.object_type == "table");
    }
    {
        // system privilege style (treated as raw single item)
        auto ast = parse_sql("GRANT CREATE USER TO USER admin");
        assert(ast.kind == NodeKind::DdlGrant);
        assert(!ast.grantStmt.privilege_list.empty());
    }
    return 0;
}
