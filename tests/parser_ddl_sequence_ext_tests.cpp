#include "scratchbird/engine/parser_ddl.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    {
        auto ast = parse_ddl_sequence("ALTER SEQUENCE s RESTART WITH 10");
        assert(ast.kind == NodeKind::DdlSequence);
        assert(ast.ddlSequence.name == "s");
        assert(ast.ddlSequence.action.find("RESTART") != std::string::npos);
    }
    {
        auto ast = parse_ddl_sequence("ALTER SEQUENCE s RESTART WITH");
        bool saw = false;
        for (auto& w : ast.warnings)
            if (w.find("missing a numeric value") != std::string::npos)
                saw = true;
        assert(saw);
    }
    return 0;
}
