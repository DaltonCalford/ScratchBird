#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    // Basic table rename
    {
        auto ast = parse_sql("ALTER TABLE t RENAME TO t2");
        assert(ast.kind == NodeKind::DdlRename);
        assert(ast.ddlRename.object_type == "table");
        assert(ast.ddlRename.old_name == "t");
        assert(ast.ddlRename.new_name == "t2");
    }
    return 0;
}
