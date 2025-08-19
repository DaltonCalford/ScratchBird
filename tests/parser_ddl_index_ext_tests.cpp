#include "scratchbird/engine/parser.h"

#include <cassert>
#include <string>

using namespace scratchbird::engine;

int main()
{
    // USING GIN
    {
        auto ast = parse_sql("CREATE INDEX ix ON t USING GIN (a)");
        assert(ast.kind == NodeKind::DdlIndex);
        assert(ast.ddlIndex.method.find("GIN") != std::string::npos);
    }
    // PARTIAL HASH INDEX with WHERE
    {
        auto ast = parse_sql("CREATE PARTIAL HASH INDEX ix_ph ON t (a) WHERE a IS NOT NULL");
        assert(ast.kind == NodeKind::DdlIndex);
        assert(ast.ddlIndex.method == std::string("PARTIAL_HASH"));
        assert(!ast.ddlIndex.where_raw.empty());
    }
    // USING RTREE
    {
        auto ast = parse_sql("CREATE INDEX ix_r ON t USING RTREE (geom)");
        assert(ast.kind == NodeKind::DdlIndex);
        assert(ast.ddlIndex.method.find("RTREE") != std::string::npos);
    }
    // INCLUDE
    {
        auto ast = parse_sql("CREATE INDEX ix_cov ON t (a) INCLUDE (b,c)");
        assert(ast.kind == NodeKind::DdlIndex);
        // options may record INCLUDE marker
        assert(ast.ddlIndex.options.find("INCLUDE") != std::string::npos);
    }
    // Multi-column with ASC/DESC, COLLATE
    {
        auto ast =
            parse_sql("CREATE UNIQUE INDEX ix_multi ON t (a DESC, b ASC COLLATE UNICODE, c)");
        assert(ast.kind == NodeKind::DdlIndex);
        assert(ast.ddlIndex.unique);
        assert(ast.ddlIndex.columns.size() == 3);
        bool has_desc = false, has_asc = false, has_coll = false;
        for (auto& p : ast.ddlIndex.column_directions) {
            if (p.second == "DESC")
                has_desc = true;
            else if (p.second == "ASC")
                has_asc = true;
        }
        for (auto& c : ast.ddlIndex.column_collates)
            if (!c.second.empty())
                has_coll = true;
        assert(has_desc && has_asc && has_coll);
    }
    return 0;
}
