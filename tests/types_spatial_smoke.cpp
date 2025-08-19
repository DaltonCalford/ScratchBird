#include "scratchbird/engine/parser.h"
#include "scratchbird/engine/parser_select.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    // Accept spatial type names in table DDL (smoke via columns_raw presence)
    {
        auto ast = parse_sql("CREATE TABLE geo (id INT, p POINT)");
        assert(ast.kind == NodeKind::DdlTable);
        assert(ast.ddlTable.columns_raw.find("POINT") != std::string::npos);
    }
    // Accept ST_Intersects syntactically in SELECT (as raw string)
    {
        auto q = parse_select_minimal("SELECT ST_Intersects(a,b) FROM t");
        (void)q; // smoke: no crash
    }
    return 0;
}
