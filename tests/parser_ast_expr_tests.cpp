#include "scratchbird/engine/parser_expr.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    {
        auto e = parse_expression_to_ast("a + 1");
        assert(!e.text.empty());
        assert(e.span.end >= e.span.start);
    }
    {
        auto e = parse_expression_to_ast("NOT (b)");
        assert(!e.text.empty());
    }
    return 0;
}
