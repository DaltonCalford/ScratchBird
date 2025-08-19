#include "scratchbird/engine/parser_dml.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    // UPDATE OR INSERT ... MATCHING (...)
    {
        auto st = parse_upsert_minimal("UPDATE OR INSERT INTO t(a,b) VALUES(1,2) MATCHING (a)");
        assert(st.target == "t");
        assert(st.columns.size() == 2);
        assert(st.values.size() == 2);
        assert(st.matching_cols.size() == 1);
    }
    // MERGE with multiple actions and guards
    {
        auto st = parse_merge_minimal(
            "MERGE INTO t USING u ON t.id = u.id WHEN MATCHED AND u.v > 0 THEN UPDATE SET a=u.a, "
            "b=u.b WHEN NOT MATCHED THEN INSERT (a,b) VALUES (u.a,u.b)");
        assert(st.target == "t");
        assert(!st.actions.empty());
        bool saw_update = false, saw_insert = false;
        for (auto& a : st.actions) {
            if (a.kind == MergeAction::Kind::Update) {
                saw_update = true;
                assert(!a.set.empty());
            }
            if (a.kind == MergeAction::Kind::Insert) {
                saw_insert = true;
                assert(!a.insert_cols.empty());
                assert(!a.insert_values.empty());
            }
        }
        assert(saw_update && saw_insert);
    }
    // MERGE with DELETE action and guard
    {
        auto st = parse_merge_minimal(
            "MERGE INTO t USING u ON t.id = u.id WHEN MATCHED AND u.v = 0 THEN DELETE");
        assert(!st.actions.empty());
    }
    // MERGE THEN DO NOTHING
    {
        auto st = parse_merge_minimal(
            "MERGE INTO t USING u ON t.id = u.id WHEN NOT MATCHED THEN DO NOTHING");
        bool saw = false;
        for (auto& a : st.actions)
            if (a.do_nothing) {
                saw = true;
                break;
            }
        assert(saw);
    }
    // EXECUTE PROCEDURE
    {
        auto st = parse_execproc_minimal("EXECUTE PROCEDURE p(1, 'x')");
        assert(st.name == "p");
        assert(!st.args.empty());
    }
    return 0;
}
