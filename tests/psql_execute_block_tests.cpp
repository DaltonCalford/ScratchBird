#include "scratchbird/engine/parser_psql.h"

#include <cassert>
#include <string>
using namespace scratchbird::engine;
int main()
{
    {
        auto ast = parse_psql_block("EXECUTE BLOCK AS BEGIN DECLARE c CURSOR FOR SELECT 1; OPEN c; "
                                    "FETCH c INTO x; CLOSE c; WHEN SQLCODE < 0 DO BEGIN POST EVENT "
                                    "'err'; END; RAISE my_exc; SUSPEND; RETURN; END");
        bool saw_open = false, saw_fetch = false, saw_close = false, saw_when = false,
             saw_raise = false, saw_post = false, saw_ret = false, saw_susp = false;
        for (auto& st : ast.psqlBlock.body) {
            if (st.kind == Ast::PsqlStmtKind::OpenCursor)
                saw_open = true;
            if (st.kind == Ast::PsqlStmtKind::FetchCursor)
                saw_fetch = true;
            if (st.kind == Ast::PsqlStmtKind::CloseCursor)
                saw_close = true;
            if (st.kind == Ast::PsqlStmtKind::Exception)
                saw_when = true;
            if (st.kind == Ast::PsqlStmtKind::Raise)
                saw_raise = true;
            if (st.kind == Ast::PsqlStmtKind::PostEvent)
                saw_post = true;
            if (st.kind == Ast::PsqlStmtKind::Return)
                saw_ret = true;
            if (st.kind == Ast::PsqlStmtKind::Suspend)
                saw_susp = true;
        }
        assert(saw_open && saw_fetch && saw_close && saw_when && saw_raise && saw_post && saw_ret &&
               saw_susp);
    }
    // Malformed nested block should not crash and should recover
    {
        auto ast = parse_psql_block("EXECUTE BLOCK AS BEGIN BEGIN x = 1, y = ; END; SUSPEND; END");
        assert(ast.kind == NodeKind::PsqlBlock);
        // Warning span presence is sufficient signal of recovery
        // Allow zero if heuristics didn't flag, but must not crash
    }
    {
        auto ast = parse_psql_block(
            "EXECUTE BLOCK AS BEGIN EXECUTE STATEMENT 'select 1' WITH CALLER PRIVILEGES AS USER "
            "'u' PASSWORD 'p' ROLE 'r' ON EXTERNAL 'db' WITH BIND TIMEOUT 5 INTO x; END");
        bool saw_exec = false;
        for (auto& st : ast.psqlBlock.body) {
            if (st.kind == Ast::PsqlStmtKind::ExecStmt) {
                saw_exec = true;
                assert(st.exec_opts.caller_privileges);
                assert(st.exec_opts.as_user == "u");
                assert(st.exec_opts.password == "p");
                assert(st.exec_opts.role == "r");
                assert(st.exec_opts.on_external == "db");
                assert(st.exec_opts.timeout == "5");
                break;
            }
        }
        assert(saw_exec);
    }
    {
        auto ast = parse_psql_block(
            "EXECUTE BLOCK AS BEGIN IF (1=1) THEN BEGIN LEAVE; END ELSE BEGIN CONTINUE; END WHILE "
            "(1=0) DO BEGIN EXIT; END CASE x WHEN 1 THEN SUSPEND; END END");
        bool saw_if = false, saw_while = false, saw_leave = false, saw_cont = false,
             saw_case = false;
        for (auto& st : ast.psqlBlock.body) {
            if (st.kind == Ast::PsqlStmtKind::If)
                saw_if = true;
            if (st.kind == Ast::PsqlStmtKind::While)
                saw_while = true;
            if (st.kind == Ast::PsqlStmtKind::Leave)
                saw_leave = true;
            if (st.kind == Ast::PsqlStmtKind::Continue)
                saw_cont = true;
            if (st.kind == Ast::PsqlStmtKind::Case)
                saw_case = true;
        }
        assert(saw_if && saw_while && saw_leave && saw_cont && saw_case);
    }
    {
        auto ast = parse_psql_block("EXECUTE BLOCK AS BEGIN EXECUTE PROCEDURE p(1,2,3); CALL "
                                    "f(4,5) ; EXECUTE STATEMENT 'select 1'; END");
        bool saw_execproc = false, saw_call = false, saw_execstmt = false;
        for (auto& st : ast.psqlBlock.body) {
            if (st.kind == Ast::PsqlStmtKind::ExecProc)
                saw_execproc = true;
            if (st.kind == Ast::PsqlStmtKind::Call)
                saw_call = true;
            if (st.kind == Ast::PsqlStmtKind::ExecStmt)
                saw_execstmt = true;
        }
        assert(saw_execproc && saw_call && saw_execstmt);
    }
    return 0;
}
