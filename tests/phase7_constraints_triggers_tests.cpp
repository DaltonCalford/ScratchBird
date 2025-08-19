#include "scratchbird/capi.h"
#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"
#include "scratchbird/engine/parser.h"

#include "gtest/gtest.h"
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace scratchbird::engine;

static void run_ddl(const std::string& sql)
{
    auto ast = parse_sql(sql);
    execute_ast(ast);
}

static std::string tempdb()
{
    // Use project-local temp dir to avoid /tmp space constraints
    const char* root = "/home/dcalford/CliWork/ScratchBird/temp";
    mkdir(root, 0755);
    std::ostringstream oss;
    oss << root << "/db_" << getpid() << "_" << (unsigned long long)time(nullptr);
    return oss.str();
}

static void create_db_and_set_path(const std::string& base)
{
    SB_CreateDbOptions o{};
    o.page_size = 4096;
    SB_Database* db = nullptr;
    auto st = sb_create_database(base.c_str(), &o, &db);
    (void)st;
    if (db)
        sb_close_database(db);
    set_executor_db_path(base);
    // Ensure catalog roots are bootstrapped before any DDL
    CatalogManager cm(get_executor_db_path());
    cm.bootstrap_if_needed();
    // Ensure default schema 'public' exists for SELECT executor and trigger runners
    if (!cm.lookup_schema_oid_by_name("public")) {
        UuidBytes gen{};
        {
            std::hash<std::string> h;
            auto v = h(std::string("public"));
            std::memcpy(gen.data(), &v, std::min(sizeof(v), gen.size()));
        }
        cm.create_schema(gen, "public", std::nullopt, "USER");
    }
}

static std::string select_mgr_id_public_emp_where_id(int id)
{
    CatalogManager cm(get_executor_db_path());
    cm.bootstrap_if_needed();
    auto soid = cm.lookup_schema_oid_by_name("public");
    if (!soid)
        return "schema not found: public";
    auto root = cm.get_relation_root_page_by_name(soid, "emp");
    if (!root)
        return "relation not found";
    auto cols = cm.list_column_names_by_name(soid, "emp");
    std::unordered_map<std::string, size_t> pos;
    for (size_t i = 0; i < cols.size(); ++i)
        pos[cols[i]] = i;
    FileOptions fo{};
    fo.direct_io = false;
    auto fh = FileManager::open(get_executor_db_path() + ".seg0", fo, false);
    std::vector<std::uint8_t> hb(4096, 0);
    FileManager::pread(fh, hb.data(), hb.size(), 0);
    auto* hh = reinterpret_cast<const ods::PageHeader*>(hb.data());
    std::uint32_t ps = hh->page_size ? hh->page_size : 4096u;
    FileMap::Layout l{};
    l.page_size = ps;
    l.pages_per_segment = 262144;
    l.options.direct_io = false;
    FileMap fm(l);
    auto s = get_executor_db_path().find_last_of('/');
    std::string dir =
        (s == std::string::npos) ? std::string(".") : get_executor_db_path().substr(0, s);
    std::string base =
        (s == std::string::npos) ? get_executor_db_path() : get_executor_db_path().substr(s + 1);
    fm.set_base_path(dir, base);
    auto hrel = HeapRelation::open(std::move(fm), ps, *root, TupleLayout{});
    auto sc = hrel.open_scan();
    std::vector<Value> row;
    ods::RowId rid{};
    while (sc.next(row, &rid)) {
        if (pos.count("id") && pos.count("mgr_id") && pos["id"] < row.size()) {
            const auto& v = row[pos["id"]];
            if (!v.is_null && std::stoi(v.bytes) == id) {
                const auto& m = row[pos["mgr_id"]];
                return m.is_null ? std::string("NULL") : m.bytes;
            }
        }
    }
    return std::string();
}

static std::pair<std::string, std::string> select_ab_from_public_c()
{
    CatalogManager cm(get_executor_db_path());
    cm.bootstrap_if_needed();
    auto soid = cm.lookup_schema_oid_by_name("public");
    if (!soid)
        return {"schema not found: public", ""};
    auto root = cm.get_relation_root_page_by_name(soid, "c");
    if (!root)
        return {"relation not found", ""};
    auto cols = cm.list_column_names_by_name(soid, "c");
    std::unordered_map<std::string, size_t> pos;
    for (size_t i = 0; i < cols.size(); ++i)
        pos[cols[i]] = i;
    FileOptions fo{};
    fo.direct_io = false;
    auto fh = FileManager::open(get_executor_db_path() + ".seg0", fo, false);
    std::vector<std::uint8_t> hb(4096, 0);
    FileManager::pread(fh, hb.data(), hb.size(), 0);
    auto* hh = reinterpret_cast<const ods::PageHeader*>(hb.data());
    std::uint32_t ps = hh->page_size ? hh->page_size : 4096u;
    FileMap::Layout l{};
    l.page_size = ps;
    l.pages_per_segment = 262144;
    l.options.direct_io = false;
    FileMap fm(l);
    auto s = get_executor_db_path().find_last_of('/');
    std::string dir =
        (s == std::string::npos) ? std::string(".") : get_executor_db_path().substr(0, s);
    std::string base =
        (s == std::string::npos) ? get_executor_db_path() : get_executor_db_path().substr(s + 1);
    fm.set_base_path(dir, base);
    auto hrel = HeapRelation::open(std::move(fm), ps, *root, TupleLayout{});
    auto sc = hrel.open_scan();
    std::vector<Value> row;
    ods::RowId rid{};
    if (sc.next(row, &rid)) {
        std::string a = (pos.count("a") && pos["a"] < row.size() && !row[pos["a"]].is_null)
                            ? row[pos["a"]].bytes
                            : std::string();
        std::string b = (pos.count("b") && pos["b"] < row.size() && !row[pos["b"]].is_null)
                            ? row[pos["b"]].bytes
                            : std::string();
        return {a, b};
    }
    return {std::string(), std::string()};
}

static void cleanup_db(const std::string& base)
{
    // Best-effort: remove segment files base.seg0..base.seg15
    for (int i = 0; i < 16; ++i) {
        std::ostringstream oss;
        oss << base << ".seg" << i;
        unlink(oss.str().c_str());
    }
}

TEST(Phase7, DeferrableUniqAndSetConstraints)
{
    std::string db = tempdb();
    create_db_and_set_path(db);
    run_ddl("CREATE SCHEMA public");
    run_ddl("CREATE TABLE public.t (id INT, v INT, CONSTRAINT ux UNIQUE(id) DEFERRABLE INITIALLY "
            "DEFERRED)");
    auto r1 = execute_insert_sql("INSERT INTO public.t(id,v) VALUES(1,10)");
    ASSERT_EQ(r1.columns.size(), 1);
    auto r2 = execute_insert_sql("INSERT INTO public.t(id,v) VALUES(1,20)");
    ASSERT_EQ(r2.columns.size(), 1);
    set_constraints_immediate_all(true);
    auto r3 = execute_insert_sql("INSERT INTO public.t(id,v) VALUES(1,30)");
    ASSERT_EQ(r3.columns[0], std::string("error"));
    cleanup_db(db);
}

TEST(Phase7, FK_Cascade_SetNull_Default)
{
    std::string db = tempdb();
    create_db_and_set_path(db);
    run_ddl("CREATE SCHEMA public");
    run_ddl("CREATE TABLE public.p (id INT PRIMARY KEY, d INT DEFAULT 0)");
    run_ddl("CREATE TABLE public.c (pid INT, x INT, CONSTRAINT fk FOREIGN KEY(pid) REFERENCES "
            "public.p(id) ON UPDATE CASCADE ON DELETE SET NULL)");
    execute_insert_sql("INSERT INTO public.p(id) VALUES(1)");
    execute_insert_sql("INSERT INTO public.c(pid,x) VALUES(1,111)");
    auto ru = execute_update_sql("UPDATE public.p SET id=2 WHERE id=1");
    ASSERT_NE(ru.rows.size(), 0);
    auto rd = execute_delete_sql("DELETE FROM public.p WHERE id=2");
    ASSERT_NE(rd.rows.size(), 0);
    cleanup_db(db);
}

TEST(Phase7, Triggers_When_UpdateOf_Order)
{
    std::string db = tempdb();
    create_db_and_set_path(db);
    run_ddl("CREATE SCHEMA public");
    run_ddl("CREATE TABLE public.t (a INT, b INT)");
    run_ddl("CREATE TRIGGER trg_bfr BEFORE UPDATE ON public.t POSITION 0 AS \nWHEN NEW.a = "
            "5\nBEGIN\n  NEW.b = 99;\nEND");
    execute_insert_sql("INSERT INTO public.t(a,b) VALUES(1,10)");
    execute_update_sql("UPDATE public.t SET a=5 WHERE a=1");
    cleanup_db(db);
}

TEST(Phase7, DeferrableByNameList)
{
    std::string db = tempdb();
    create_db_and_set_path(db);
    run_ddl("CREATE SCHEMA public");
    run_ddl("CREATE TABLE public.u (a INT, CONSTRAINT ux UNIQUE(a) DEFERRABLE)");
    // Defer by name list
    set_constraints_deferred_list({"ux"}, true);
    auto r1 = execute_insert_sql("INSERT INTO public.u(a) VALUES(7)");
    auto r2 = execute_insert_sql("INSERT INTO public.u(a) VALUES(7)");
    ASSERT_EQ(r1.columns.size(), 1);
    ASSERT_EQ(r2.columns.size(), 1);
    // Switch to immediate by name -> next duplicate should error
    set_constraints_immediate_list({"ux"}, true);
    auto r3 = execute_insert_sql("INSERT INTO public.u(a) VALUES(7)");
    ASSERT_EQ(r3.columns[0], std::string("error"));
    cleanup_db(db);
}

TEST(Phase7, FK_SelfReferential_Cascade)
{
    std::string db = tempdb();
    create_db_and_set_path(db);
    run_ddl("CREATE SCHEMA public");
    run_ddl("CREATE TABLE public.emp (id INT PRIMARY KEY, mgr_id INT, CONSTRAINT fk_mgr FOREIGN "
            "KEY(mgr_id) REFERENCES public.emp(id) ON UPDATE CASCADE)");
    execute_insert_sql("INSERT INTO public.emp(id,mgr_id) VALUES(1,NULL)");
    execute_insert_sql("INSERT INTO public.emp(id,mgr_id) VALUES(2,1)");
    auto ru = execute_update_sql("UPDATE public.emp SET id=3 WHERE id=1");
    ASSERT_NE(ru.rows.size(), 0);
    // Verify child cascaded: expect mgr_id=3 for id=2
    {
        auto sel = execute_select_sql("SELECT mgr_id FROM public.emp WHERE id=2");
        ASSERT_FALSE(sel.rows.empty());
        ASSERT_EQ(sel.rows[0][0], std::string("3"));
    }
    cleanup_db(db);
}

TEST(Phase7, FK_MultiColumn_SetDefault)
{
    std::string db = tempdb();
    create_db_and_set_path(db);
    run_ddl("CREATE SCHEMA public");
    run_ddl("CREATE TABLE public.p (a INT DEFAULT 9, b INT DEFAULT 8, CONSTRAINT pk PRIMARY KEY "
            "(a,b))");
    run_ddl("CREATE TABLE public.c (a INT DEFAULT 0, b INT DEFAULT 0, CONSTRAINT fk FOREIGN "
            "KEY(a,b) REFERENCES public.p(a,b) ON DELETE SET DEFAULT)");
    execute_insert_sql("INSERT INTO public.p(a,b) VALUES(1,2)");
    execute_insert_sql("INSERT INTO public.c(a,b) VALUES(1,2)");
    auto rd = execute_delete_sql("DELETE FROM public.p WHERE a=1 AND b=2");
    ASSERT_NE(rd.rows.size(), 0);
    {
        auto sel = execute_select_sql("SELECT a,b FROM public.c");
        ASSERT_FALSE(sel.rows.empty());
        ASSERT_EQ(sel.rows[0][0], std::string("0"));
        ASSERT_EQ(sel.rows[0][1], std::string("0"));
    }
    cleanup_db(db);
}

TEST(Phase7, TriggerActiveInactiveAndRaise)
{
    std::string db = tempdb();
    create_db_and_set_path(db);
    run_ddl("CREATE SCHEMA public");
    run_ddl("CREATE TABLE public.t (a INT)");
    execute_insert_sql("INSERT INTO public.t(a) VALUES(0)");
    // Row trigger that raises
    run_ddl("CREATE TRIGGER trg_stmt BEFORE UPDATE ON public.t POSITION 0 FOR EACH ROW AS BEGIN "
            "RAISE SQLSTATE 'P0001' 'fail'; END");
    // Verify trigger exists and is ACTIVE
    {
        CatalogManager cm(get_executor_db_path());
        auto soid = cm.lookup_schema_oid_by_name("public");
        auto triglist = cm.list_relation_triggers_by_name(soid, "t");
        bool found = false, active = false;
        for (auto& ti : triglist) {
            if (ti.name == "trg_stmt") {
                found = true;
                active = ti.active;
                break;
            }
        }
        ASSERT_TRUE(found);
        ASSERT_TRUE(active);
    }
    // Inactivate via catalog API
    {
        CatalogManager cm(get_executor_db_path());
        auto soid = cm.lookup_schema_oid_by_name("public");
        cm.alter_trigger_active(soid, "trg_stmt", false);
    }
    execute_update_sql("UPDATE public.t SET a=1");
    // Confirm inactive (find the last/most recent trigger with this name)
    {
        CatalogManager cm(get_executor_db_path());
        auto soid = cm.lookup_schema_oid_by_name("public");
        auto triglist = cm.list_relation_triggers_by_name(soid, "t");
        bool active = true;
        for (auto& ti : triglist) {
            if (ti.name == "trg_stmt") {
                active = ti.active;
            }
        } // don't break, find last
        ASSERT_FALSE(active);
    }
    // Activate and expect error on update
    {
        CatalogManager cm(get_executor_db_path());
        auto soid = cm.lookup_schema_oid_by_name("public");
        cm.alter_trigger_active(soid, "trg_stmt", true);
    }
    // Confirm active again
    {
        CatalogManager cm(get_executor_db_path());
        auto soid = cm.lookup_schema_oid_by_name("public");
        auto triglist = cm.list_relation_triggers_by_name(soid, "t");
        bool active = false;
        for (auto& ti : triglist) {
            if (ti.name == "trg_stmt") {
                active = ti.active;
                break;
            }
        }
        ASSERT_TRUE(active);
    }
    auto result = execute_update_sql("UPDATE public.t SET a=1");
    ASSERT_EQ(result.columns.size(), 1);
    ASSERT_EQ(result.columns[0], "error");
    ASSERT_EQ(result.rows.size(), 1);
    EXPECT_TRUE(result.rows[0][0].find("trigger error:") == 0);
    cleanup_db(db);
}
