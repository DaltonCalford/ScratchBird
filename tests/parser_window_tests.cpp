#include "scratchbird/engine/parser_select.h"

#include <cassert>
#include <cstdlib>

using namespace scratchbird::engine;

int main()
{
    // Minimal: single window def with partition/order
    {
        auto q = parse_select_minimal("SELECT a FROM t WINDOW w AS (PARTITION BY a ORDER BY b)");
        assert(!q.windows.empty());
        const auto& w = q.windows[0];
        assert(w.partition_by.find("a") != std::string::npos);
        assert(w.order_by.find("b") != std::string::npos);
    }

    // Frame unit detection
    {
        auto q = parse_select_minimal(
            "SELECT a FROM t WINDOW w AS (ORDER BY a ROWS BETWEEN 1 PRECEDING AND 2 FOLLOWING)");
        assert(!q.windows.empty());
        assert(q.windows[0].frame.unit == "ROWS");
        assert(!q.windows[0].frame.start_kind.empty());
        assert(!q.windows[0].frame.end_kind.empty());
    }

    // Named window reference
    {
        auto q = parse_select_minimal("SELECT a FROM t WINDOW x AS (y)");
        bool saw_ref = false;
        for (auto& w : q.windows)
            if (!w.ref_name.empty())
                saw_ref = true;
        assert(saw_ref);
    }

    // Multiple window defs separated by comma
    {
        auto q = parse_select_minimal(
            "SELECT a FROM t WINDOW w1 AS (PARTITION BY a), w2 AS (ORDER BY b)");
        assert(q.windows.size() == 2);
    }

    // RANGE without ORDER BY should warn
    {
        auto q = parse_select_minimal(
            "SELECT a FROM t WINDOW w AS (RANGE BETWEEN 1 PRECEDING AND CURRENT ROW)");
        bool saw = false;
        for (auto& w : q.warnings)
            if (w.find("RANGE frame used without ORDER BY") != std::string::npos)
                saw = true;
        assert(saw);
    }

    // Negative bounds heuristic warning
    {
        auto q = parse_select_minimal(
            "SELECT a FROM t WINDOW w AS (ORDER BY a ROWS BETWEEN -1 PRECEDING AND -2 FOLLOWING)");
        bool saw = false;
        for (auto& w : q.warnings)
            if (w.find("negative bound") != std::string::npos)
                saw = true;
        assert(saw);
    }

    std::_Exit(0);
}
