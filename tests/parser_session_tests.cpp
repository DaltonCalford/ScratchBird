#include "scratchbird/engine/parser.h"

#include <cassert>
#include <string>

using namespace scratchbird::engine;

int main()
{
    {
        auto ast = parse_sql("CREATE DATABASE '/tmp/db.fdb'");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.kind == SessionKind::CreateDb);
        assert(!ast.session.a.empty());
    }
    {
        auto ast = parse_sql("ALTER DATABASE ADD DIFFERENCE FILE '/tmp/df'");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.kind == SessionKind::AlterDb);
    }
    {
        auto ast = parse_sql("DROP DATABASE");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.kind == SessionKind::DropDb);
    }
    {
        auto ast = parse_sql("CONNECT '/tmp/db.fdb'");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.kind == SessionKind::Connect);
        assert(!ast.session.a.empty());
    }
    {
        auto ast = parse_sql("DISCONNECT");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.kind == SessionKind::Disconnect);
    }
    {
        auto ast = parse_sql("SET NAMES UTF8");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.kind == SessionKind::SetNames);
        assert(ast.session.a == "UTF8");
    }
    {
        auto ast = parse_sql("SET ROLE r1");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.kind == SessionKind::SetRole);
        assert(ast.session.a == "r1");
    }
    {
        auto ast = parse_sql("SET SQL DIALECT 3");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.kind == SessionKind::SetDialect);
    }
    {
        auto ast = parse_sql("SET TRANSACTION READ COMMITTED WAIT");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.kind == SessionKind::SetTxn);
        assert(ast.session.a.find("READ COMMITTED") != std::string::npos);
    }
    {
        auto ast = parse_sql("COMMIT WORK");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.kind == SessionKind::Commit);
    }
    {
        auto ast = parse_sql("ROLLBACK WORK");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.kind == SessionKind::Rollback);
    }
    {
        auto ast = parse_sql("SAVEPOINT s1");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.kind == SessionKind::Savepoint);
        assert(ast.session.a == "s1");
    }
    {
        auto ast = parse_sql("RELEASE SAVEPOINT s1");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.kind == SessionKind::Release);
        assert(ast.session.a == "s1");
    }
    {
        auto ast = parse_sql("SET TIME ZONE 'UTC'");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.setopts.time_zone.size() > 0);
    }
    {
        auto ast = parse_sql("SET BIND mybind");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.setopts.bind == "mybind");
    }
    {
        auto ast = parse_sql("SET OPTIMIZE foo=bar");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.setopts.optimize.find("foo") != std::string::npos);
    }
    {
        auto ast = parse_sql("SET SEARCH PATH schema1, schema2");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.setopts.search_path.find("schema1") != std::string::npos);
    }
    {
        auto ast = parse_sql("SET DEBUG OPTION TRACE");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.setopts.debug_option.find("TRACE") != std::string::npos);
    }
    {
        auto ast = parse_sql("SET DECFLOAT ROUND CEILING");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.setopts.decfloat_round.find("CEILING") != std::string::npos);
    }
    {
        auto ast = parse_sql("SET DECFLOAT TRAPS INVALID, DIVBYZERO");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.setopts.decfloat_traps.find("INVALID") != std::string::npos);
    }
    {
        auto ast = parse_sql("SESSION RESET");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.setopts.session_reset);
    }
    return 0;
}
