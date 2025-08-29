#include "scratchbird/engine.h"
#include "scratchbird/engine/ddl/tablespace_ddl.h"
#include "scratchbird/engine/parser_ddl.h"
#include "scratchbird/engine/tablespace_manager.h"
#include "test_db_utils.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>

using namespace scratchbird;
using namespace scratchbird::engine;

static void test_default_tablespace_bootstrap()
{
    scratchbird::tests::TestDatabaseRAII test_db("ts_bootstrap", true);
    Status st{};
    auto db = open_database(test_db.path().c_str(), st);
    assert(st.code == StatusCode::Ok);
    // Load tablespace manager; ensure default exists
    TablespaceManager tsm(test_db.path(), /*page_size*/ 4096);
    bool ok = tsm.load_from_catalog();
    assert(ok);
    auto def = tsm.get_by_name("SDB$DEFAULT");
    // If catalog not yet materialized, ensure_default will synthesize one
    if (!def) {
        ok = tsm.ensure_default();
        assert(ok);
        def = tsm.get_by_name("SDB$DEFAULT");
    }
    assert(def.has_value());
    close_database(db);
}

static void test_create_tablespace_noop()
{
    scratchbird::tests::TestDatabaseRAII test_db("ts_create", true);
    Ast ast =
        parse_ddl_tablespace("CREATE TABLESPACE SDB$TS1 LOCATION '/tmp' WITH (auto_extend=true)");
    std::string err;
    bool ok = execute_tablespace_ddl(ast, test_db.path(), err);
    assert(ok);
}

int main()
{
    std::cout << "=== Tablespace Basic Tests ===" << std::endl;
    test_default_tablespace_bootstrap();
    test_create_tablespace_noop();
    std::cout << "\n✓ Tablespace basic tests passed" << std::endl;
    return 0;
}
