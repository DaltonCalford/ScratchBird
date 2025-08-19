#include "scratchbird/engine/parser_ddl.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    // VIEW column list + WITH CHECK OPTION flag
    {
        auto ast = parse_ddl_view("CREATE VIEW v(a,b) AS SELECT a,b FROM t WITH CHECK OPTION");
        assert(ast.kind == NodeKind::DdlView);
        assert(ast.ddlView.name == "v");
        assert(!ast.ddlView.columns_raw.empty());
        assert(ast.ddlView.with_check_option);
    }
    // DOMAIN attributes
    {
        auto ast = parse_ddl_domain("CREATE DOMAIN d AS VARCHAR(10) COLLATE UNICODE");
        assert(ast.kind == NodeKind::DdlDomain);
        assert(ast.ddlDomain.collate.find("UNICODE") != std::string::npos);
    }
    return 0;
}
