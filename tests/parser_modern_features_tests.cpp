#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    // Row-level security
    {
        auto ast = parse_sql("CREATE POLICY p1 ON table t USING (owner = CURRENT_USER)");
        assert(ast.kind == NodeKind::DdlRlsPolicy);
        assert(ast.ddlRlsPolicy.name == "p1");
    }
    {
        auto ast = parse_sql("ALTER POLICY p1 SET (enabled=false)");
        assert(ast.kind == NodeKind::DdlRlsPolicy);
    }
    {
        auto ast = parse_sql("DROP POLICY p1");
        assert(ast.kind == NodeKind::DdlRlsPolicy);
    }
    // Materialized views
    {
        auto ast = parse_sql("CREATE MATERIALIZED VIEW mv AS SELECT 1");
        assert(ast.kind == NodeKind::DdlMaterializedView);
        assert(ast.ddlMaterializedView.name == "mv");
    }
    {
        auto ast = parse_sql("REFRESH MATERIALIZED VIEW mv WITH DATA");
        assert(ast.kind == NodeKind::DdlMaterializedView);
        assert(ast.ddlMaterializedView.action == "refresh");
    }
    {
        auto ast = parse_sql("DROP MATERIALIZED VIEW mv");
        assert(ast.kind == NodeKind::DdlMaterializedView);
    }
    return 0;
}
