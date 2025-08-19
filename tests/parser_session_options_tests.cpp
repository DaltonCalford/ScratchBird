#include "scratchbird/engine/ast.h"
#include "scratchbird/engine/parser_session.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    {
        auto ast = parse_session_stmt(
            "CREATE DATABASE '/tmp/db.fdb' PAGE_SIZE 8192 DEFAULT CHARACTER SET UTF8 DIALECT 3");
        assert(ast.kind == NodeKind::SessionStmt);
        assert(ast.session.kind == SessionKind::CreateDb);
        assert(ast.session.dbopts.page_size == "8192" || ast.session.dbopts.page_size == "8192;");
        assert(ast.session.dbopts.default_charset == "UTF8");
        assert(ast.session.dbopts.dialect == "3");
    }
    {
        auto ast =
            parse_session_stmt("ALTER DATABASE PAGE_SIZE 4096 DEFAULT CHARACTER SET WIN1252");
        assert(ast.session.kind == SessionKind::AlterDb);
        assert(ast.session.dbopts.page_size == "4096");
        assert(ast.session.dbopts.default_charset == "WIN1252");
    }
    {
        auto ast = parse_session_stmt("CREATE DATABASE 'db.fdb' FILE '/tmp/seg1.fdb' FILE "
                                      "'/tmp/seg2.fdb' SHADOW '/tmp/sh1.fdb'");
        assert(ast.session.dbopts.files.size() >= 2);
        assert(ast.session.dbopts.shadows.size() >= 1);
    }
    {
        auto ast = parse_session_stmt("SET LOCK TIMEOUT 10");
        assert(ast.session.kind == SessionKind::SetOption);
        assert(ast.session.setopts.lock_timeout.find("10") != std::string::npos);
    }
    {
        auto ast = parse_session_stmt("SET NO WAIT");
        assert(ast.session.kind == SessionKind::SetOption);
        assert(ast.session.setopts.lock_timeout == "NO WAIT");
    }
    {
        auto ast = parse_session_stmt("SET STATISTICS ON");
        assert(ast.session.kind == SessionKind::SetOption);
        assert(ast.session.setopts.stats.find("STATISTICS") != std::string::npos);
    }
    {
        auto ast =
            parse_session_stmt("SET TRANSACTION READ COMMITTED READ WRITE WAIT RECORD_VERSION");
        assert(ast.session.kind == SessionKind::SetTxn);
        assert(ast.session.setopts.isolation == "READ COMMITTED");
        assert(ast.session.setopts.access == "READ WRITE");
        assert(ast.session.setopts.wait == "WAIT");
    }
    {
        auto ast = parse_session_stmt(
            "CREATE DATABASE 'db.fdb' PAGE CACHE 10000 SWEEP INTERVAL 200 RESERVE SPACE");
        assert(ast.session.dbopts.page_cache == "10000");
        assert(ast.session.dbopts.sweep_interval == "200");
        assert(!ast.session.dbopts.reserve_space.empty());
    }
    // New: SNAPSHOT TABLE STABILITY and NO WAIT
    {
        auto ast = parse_session_stmt("SET TRANSACTION SNAPSHOT TABLE STABILITY NO WAIT");
        assert(ast.session.kind == SessionKind::SetTxn);
        assert(ast.session.setopts.isolation == "SNAPSHOT");
        assert(ast.session.setopts.snapshot_table_stability == true);
        assert(ast.session.setopts.wait == "NO WAIT");
    }
    // New: RESERVING list with mode
    {
        auto ast = parse_session_stmt("SET TRANSACTION RESERVING T1, T2 FOR SHARED READ");
        assert(ast.session.kind == SessionKind::SetTxn);
        assert(ast.session.setopts.table_reservations.size() >= 2);
    }
    return 0;
}
