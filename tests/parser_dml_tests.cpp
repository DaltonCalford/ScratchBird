#include "scratchbird/engine/parser_dml.h"

#include <cassert>

int main()
{
    using namespace scratchbird::engine;
    {
        auto st = parse_insert_minimal("INSERT INTO t(a,b) VALUES(1,2) RETURNING a, b");
        assert(st.target == "t");
        assert(st.columns.size() == 2);
        assert(st.values.size() >= 2);
        assert(st.has_returning);
        assert(st.returning.size() == 2);
    }
    // Multi-row VALUES (tuples + flat)
    {
        auto st = parse_insert_minimal("INSERT INTO t(a,b) VALUES(1,2),(3,4)");
        assert(st.values.size() >= 4);
        assert(st.values_tuples.size() == 2);
        assert(st.values_tuples[0].size() == 2);
        assert(st.values_tuples[1].size() == 2);
    }
    // INSERT ... SELECT
    {
        auto st = parse_insert_minimal("INSERT INTO t(a) SELECT a FROM u WHERE a > 0");
        assert(st.target == "t");
        assert(!st.select_raw.empty());
    }
    {
        auto st = parse_insert_minimal("INSERT INTO t DEFAULT VALUES RETURNING id");
        assert(st.default_values);
        assert(st.has_returning);
        // No mismatch warning
        assert(st.warnings.empty());
    }
    {
        auto st = parse_update_minimal("UPDATE t SET a=1, b=2 WHERE a > 0 RETURNING a");
        assert(st.target == "t");
        assert(st.assignments.size() == 2);
        assert(!st.where_expr.empty());
        assert(st.has_returning);
        assert(st.returning.size() == 1);
    }
    {
        auto st = parse_update_minimal("UPDATE t SET a=1, b=2 FOR UPDATE RETURNING *");
        assert(st.target == "t");
        assert(st.assignments.size() == 2);
        assert(st.has_for_update);
        assert(st.has_returning);
    }
    // UPDATE FROM
    {
        auto st = parse_update_minimal("UPDATE t SET a=1 FROM u WHERE t.id = u.id RETURNING a");
        assert(st.target == "t");
        assert(st.from_raw.find("u") != std::string::npos);
        assert(st.has_returning);
    }
    {
        auto st = parse_delete_minimal("DELETE FROM t WHERE id=1 RETURNING id, a");
        assert(st.target == "t");
        assert(st.has_returning);
        assert(st.returning.size() == 2);
        assert(!st.where_expr.empty());
    }
    // DELETE USING
    {
        auto st = parse_delete_minimal("DELETE FROM t USING u WHERE t.id = u.id");
        assert(st.using_raw.find("u") != std::string::npos);
    }
    // WHERE CURRENT OF for UPDATE/DELETE
    {
        auto st = parse_update_minimal("UPDATE t SET a=1 WHERE CURRENT OF c1");
        assert(st.where_current_cursor == "c1");
        assert(st.warnings.empty());
    }
    {
        auto st = parse_delete_minimal("DELETE FROM t WHERE CURRENT OF c2");
        assert(st.where_current_cursor == "c2");
    }
    {
        auto st = parse_merge_minimal("MERGE INTO t USING u ON t.id = u.id WHEN MATCHED THEN "
                                      "UPDATE SET a=1 WHEN NOT MATCHED THEN INSERT (a) VALUES (1)");
        assert(st.target == "t");
        assert(!st.using_source.empty());
        assert(!st.on_match.empty());
        assert(!st.actions.empty());
    }
    {
        auto st = parse_update_minimal("UPDATE t SET WHERE id=1");
        bool saw = false;
        for (auto& m : st.warnings)
            if (m.find("empty SET") != std::string::npos)
                saw = true;
        assert(saw);
    }
    {
        auto st = parse_delete_minimal("DELETE FROM t USING WHERE id=1");
        bool saw = false;
        for (auto& m : st.warnings)
            if (m.find("no identifiers") != std::string::npos)
                saw = true;
        assert(saw);
    }
    {
        auto st = parse_upsert_minimal("UPDATE OR INSERT INTO t(a,b) VALUES(1,2) MATCHING(c)");
        bool saw = false;
        for (auto& m : st.warnings)
            if (m.find("MATCHING") != std::string::npos)
                saw = true;
        assert(saw);
    }
    {
        auto st = parse_insert_minimal("INSERT INTO t(a,b) VALUES(1)");
        bool saw = false;
        for (auto& m : st.warnings)
            if (m.find("column count") != std::string::npos)
                saw = true;
        assert(saw);
    }
    return 0;
}
