#include "scratchbird/engine/parser.h"
#include "scratchbird/engine/parser_select.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    // CREATE DATABASE LINK
    {
        auto ast = parse_sql("CREATE DATABASE LINK fin_link USING 'dsn=finance'");
        assert(ast.kind == NodeKind::DdlDbLink);
        assert(ast.ddlDbLink.name.find("fin_link") != std::string::npos);
    }
    // table@link in FROM
    {
        auto q = parse_select_minimal("SELECT * FROM customers@fin_link c");
        bool saw = false;
        for (const auto& fi : q.from_items) {
            if (fi.table.find("@") != std::string::npos) {
                saw = true;
                break;
            }
        }
        assert(saw);
    }
    return 0;
}
