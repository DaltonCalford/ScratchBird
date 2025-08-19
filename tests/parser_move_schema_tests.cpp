#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    // ALTER PROCEDURE ... SET SCHEMA ...
    {
        auto ast = parse_sql("ALTER PROCEDURE p SET SCHEMA new_s");
        assert(ast.kind == NodeKind::DdlMove);
        assert(ast.ddlMove.object_type == "procedure");
        assert(ast.ddlMove.name == "p");
        assert(ast.ddlMove.new_schema == "new_s");
    }
    // ALTER FUNCTION ... SET SCHEMA ...
    {
        auto ast = parse_sql("ALTER FUNCTION f SET SCHEMA s2");
        assert(ast.kind == NodeKind::DdlMove);
        assert(ast.ddlMove.object_type == "function");
    }
    // ALTER PACKAGE ... SET SCHEMA ...
    {
        auto ast = parse_sql("ALTER PACKAGE pkg SET SCHEMA util");
        assert(ast.kind == NodeKind::DdlMove);
        assert(ast.ddlMove.object_type == "package");
    }
    return 0;
}
