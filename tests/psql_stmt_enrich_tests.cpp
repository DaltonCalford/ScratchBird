#include "scratchbird/engine/parser_psql.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    // WHILE/LEAVE/CONTINUE/EXCEPTION/DECLARE/CURSOR ops classification and FOR SELECT INTO
    {
        auto ast = parse_psql_block("EXECUTE BLOCK AS BEGIN DECLARE x INT; FOR SELECT a FROM t "
                                    "INTO x; WHILE (1=1) DO BEGIN LEAVE; END FETCH c INTO x; "
                                    "CLOSE c; EXCEPTION WHEN ANY DO BEGIN END; CONTINUE; END");
        bool saw_while = false, saw_leave = false, saw_fetch = false, saw_close = false,
             saw_exc = false, saw_for = false, saw_decl = false;
        for (auto& st : ast.psqlBlock.body) {
            if (st.kind == Ast::PsqlStmtKind::While)
                saw_while = true;
            if (st.kind == Ast::PsqlStmtKind::Leave)
                saw_leave = true;
            if (st.kind == Ast::PsqlStmtKind::FetchCursor)
                saw_fetch = true;
            if (st.kind == Ast::PsqlStmtKind::CloseCursor)
                saw_close = true;
            if (st.kind == Ast::PsqlStmtKind::Exception)
                saw_exc = true;
            if (st.kind == Ast::PsqlStmtKind::ForSelect)
                saw_for = true;
            if (st.kind == Ast::PsqlStmtKind::Declare)
                saw_decl = true;
        }
        assert(saw_while && saw_leave && saw_fetch && saw_close && saw_exc && saw_for && saw_decl);
    }
    // DECLARE variable missing type warning
    {
        auto ast = parse_psql_block("EXECUTE BLOCK AS BEGIN DECLARE y ; END");
        bool warned = false;
        for (const auto& w : ast.warnings)
            if (w.find("DECLARE missing type") != std::string::npos) {
                warned = true;
                break;
            }
        assert(warned);
    }
    // Nested block splitting (BEGIN...END inside block)
    {
        auto ast = parse_psql_block("EXECUTE BLOCK AS BEGIN BEGIN x = 1; END; RETURN; END");
        bool saw_return = false, saw_nested_assign = false;
        for (const auto& st : ast.psqlBlock.body) {
            if (st.kind == Ast::PsqlStmtKind::Return) {
                saw_return = true;
            }
            if (!st.nested.empty()) {
                for (const auto& ns : st.nested) {
                    if (ns.raw.find("x = 1") != std::string::npos) {
                        saw_nested_assign = true;
                        break;
                    }
                }
            }
        }
        assert(saw_return && saw_nested_assign);
    }
    // FOR SELECT ... INTO var list capture
    {
        auto ast = parse_psql_block("EXECUTE BLOCK AS BEGIN FOR SELECT a,b FROM t INTO x, y; END");
        bool captured = false;
        for (const auto& st : ast.psqlBlock.body)
            if (st.kind == Ast::PsqlStmtKind::ForSelect) {
                captured =
                    (st.into_vars.size() == 2) && st.for_query_raw.find("a") != std::string::npos;
                break;
            }
        assert(captured);
    }
    // FOR SELECT INTO mismatch warns (heuristic)
    {
        auto ast = parse_psql_block("EXECUTE BLOCK AS BEGIN FOR SELECT a,b FROM t INTO x; END");
        bool warned = false;
        for (const auto& w : ast.warnings)
            if (w.find("variable count mismatch") != std::string::npos) {
                warned = true;
                break;
            }
        assert(warned);
    }
    // DECLARE typed variable parsed via TypeDescriptor
    {
        auto ast = parse_psql_block(
            "EXECUTE BLOCK AS BEGIN DECLARE z VARCHAR(20) CHARACTER SET UTF8; END");
        bool saw = false;
        for (const auto& st : ast.psqlBlock.body)
            if (st.kind == Ast::PsqlStmtKind::Declare && !st.decl_type.name.empty()) {
                saw = true;
                break;
            }
        assert(saw);
    }
    // DECLARE with COLLATE and arrays (lite parser captures)
    {
        auto ast =
            parse_psql_block("EXECUTE BLOCK AS BEGIN DECLARE a INTEGER[][] COLLATE UNICODE; END");
        bool saw = false;
        for (const auto& st : ast.psqlBlock.body)
            if (st.kind == Ast::PsqlStmtKind::Declare && st.decl_type.array_rank == 2) {
                saw = true;
                break;
            }
        assert(saw);
    }
    // DML RETURNING INTO capture and mismatch warn
    {
        auto ast = parse_psql_block(
            "EXECUTE BLOCK AS BEGIN INSERT INTO t(a,b) VALUES(1,2) RETURNING a,b INTO x,y; END");
        bool has_into = false;
        for (const auto& st : ast.psqlBlock.body) {
            if (!st.into_vars.empty()) {
                has_into = true;
                break;
            }
        }
        assert(has_into);
    }
    {
        auto ast =
            parse_psql_block("EXECUTE BLOCK AS BEGIN UPDATE t SET a=1 RETURNING a,b INTO x; END");
        bool warned = false;
        for (const auto& w : ast.warnings)
            if (w.find("RETURNING INTO variable count mismatch") != std::string::npos) {
                warned = true;
                break;
            }
        assert(warned);
        // spans present
        assert(!ast.warning_spans.empty());
        assert(ast.warning_spans[0].end > ast.warning_spans[0].start);
    }
    return 0;
}
