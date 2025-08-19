#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    auto ast = parse_sql("select 1");
    assert(ast.kind == NodeKind::SelectLiteral);
    assert(ast.literal_value == 1);

    auto ast2 = parse_sql("select 2");
    assert(ast2.kind == NodeKind::Unknown);
    return 0;
}
