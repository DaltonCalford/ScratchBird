#include "scratchbird/engine/parser.h"

#include <cassert>
#include <string>

using namespace scratchbird::engine;

int main()
{
    {
        auto ast = parse_sql("START TRACE slowlog");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("PAUSE SUBSCRIPTION sub1");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("START BACKGROUND TASK stats OPTIONS (interval='5m')");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("CREATE SCHEDULE nightly CRON '0 2 * * *'");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("RUN JOB backup NOW");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("BACKUP DATABASE TO '/backup/db.bak' WITH (compress='zstd')");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("BACKUP TABLESPACE fast TO '/backup/ts_fast.bak'");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("RESTORE DATABASE FROM '/backup/db.bak' WITH (replace=true)");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("SHOW BACKUP HISTORY FOR DATABASE");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("ALTER DATABASE SWEEP IMMEDIATE");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("SET SWEEP INTERVAL 300");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("SET PAGE CACHE 8192");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("SET RESERVE SPACE ON");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("SET READ CONSISTENCY OFF");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("TRUNCATE TABLE t CASCADE");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("EXPLAIN SELECT * FROM t");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("EXPLAIN ANALYZE SELECT * FROM t");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("ANALYZE t (a,b)");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("VACUUM FULL t");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("CREATE STATISTICS st_ab ON t (a,b)");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("DROP STATISTICS st_ab");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    {
        auto ast = parse_sql("SET CONSTRAINTS ALL DEFERRED");
        assert(ast.kind == NodeKind::SessionStmt);
    }
    return 0;
}
