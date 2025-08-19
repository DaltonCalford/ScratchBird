#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    {
        auto ast = parse_sql("CREATE PUBLICATION pub FOR TABLE t1, t2 WITH (streaming=true)");
        assert(ast.kind == NodeKind::DdlPublication);
        assert(ast.ddlPublication.name == "pub");
    }
    {
        auto ast = parse_sql("ALTER PUBLICATION pub SET (enabled=false)");
        assert(ast.kind == NodeKind::DdlPublication);
    }
    {
        auto ast = parse_sql("DROP PUBLICATION pub");
        assert(ast.kind == NodeKind::DdlPublication);
    }
    {
        auto ast =
            parse_sql("CREATE SUBSCRIPTION sub CONNECTION 'dsn' PUBLICATION pub WITH (apply=true)");
        assert(ast.kind == NodeKind::DdlSubscription);
        assert(ast.ddlSubscription.name == "sub");
    }
    {
        auto ast = parse_sql("ALTER SUBSCRIPTION sub SET (enabled=true)");
        assert(ast.kind == NodeKind::DdlSubscription);
    }
    {
        auto ast = parse_sql("DROP SUBSCRIPTION sub");
        assert(ast.kind == NodeKind::DdlSubscription);
    }
    // Session-level control already covered in admin surfaces: PAUSE/RESUME SUBSCRIPTION
    return 0;
}
