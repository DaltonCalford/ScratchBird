#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    {
        auto ast = parse_sql("CREATE TRACE PROFILE slowlog OPTIONS (min_duration_ms=100)");
        assert(ast.kind == NodeKind::DdlTraceProfile);
        assert(ast.ddlTraceProfile.name == "slowlog");
    }
    {
        auto ast = parse_sql("ALTER TRACE PROFILE slowlog OPTIONS (sample=0.1)");
        assert(ast.kind == NodeKind::DdlTraceProfile);
    }
    {
        auto ast = parse_sql("DROP TRACE PROFILE slowlog");
        assert(ast.kind == NodeKind::DdlTraceProfile);
    }
    {
        auto ast = parse_sql("CREATE AUDIT POLICY auth_fail ACTIONS (LOGIN_FAILURE)");
        assert(ast.kind == NodeKind::DdlAuditPolicy);
        assert(ast.ddlAuditPolicy.name == "auth_fail");
    }
    {
        auto ast = parse_sql("ALTER AUDIT POLICY auth_fail SET (enabled=true)");
        assert(ast.kind == NodeKind::DdlAuditPolicy);
    }
    {
        auto ast = parse_sql("DROP AUDIT POLICY auth_fail");
        assert(ast.kind == NodeKind::DdlAuditPolicy);
    }
    // AUDIT/NOAUDIT are session-level admin commands, covered via admin surfaces/specs
    return 0;
}
