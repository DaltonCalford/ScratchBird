#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    {
        auto ast = parse_sql("CREATE CLUSTER c1 OPTIONS (auth='central')");
        assert(ast.kind == NodeKind::DdlCluster);
        assert(ast.ddlCluster.name == "c1");
    }
    {
        auto ast = parse_sql("CREATE CLUSTER NODE n1 OPTIONS (host='10.0.0.1')");
        assert(ast.kind == NodeKind::DdlClusterNode);
        assert(ast.ddlClusterNode.name == "n1");
    }
    {
        auto ast = parse_sql("CREATE CLUSTER SERVICE svc1 OPTIONS (port=3050)");
        assert(ast.kind == NodeKind::DdlClusterService);
        assert(ast.ddlClusterService.name == "svc1");
    }
    {
        auto ast = parse_sql("CREATE AUTH PROVIDER ldap OPTIONS (url='ldaps://host')");
        assert(ast.kind == NodeKind::DdlAuthProvider);
        assert(ast.ddlAuthProvider.name == "ldap");
    }
    return 0;
}
