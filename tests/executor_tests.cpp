#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    auto ast = parse_sql("select 1");
    auto res = execute_ast(ast);
    assert(res.columns.size() == 1);
    assert(res.rows.size() == 1);
    assert(res.rows[0].size() == 1);
    assert(res.rows[0][0] == std::string("1"));
    return 0;
}
