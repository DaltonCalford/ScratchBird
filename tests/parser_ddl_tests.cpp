#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    // TABLE
    {
        auto ast = parse_sql("CREATE TABLE t (id INT NOT NULL, x VARCHAR(20) DEFAULT 'a', "
                             "CONSTRAINT pk PRIMARY KEY(id))");
        assert(ast.kind == NodeKind::DdlTable);
        assert(ast.ddlTable.name == "t");
        assert(ast.ddlTable.columns_raw.find("id INT") != std::string::npos);
        // Structured constraint capture
        bool saw_pk = false;
        for (const auto& c : ast.ddlTable.table_constraints) {
            if (c.kind == "PRIMARY KEY" && !c.columns.empty()) {
                saw_pk = true;
                break;
            }
        }
        assert(saw_pk);
    }
    // COMPUTED BY capture
    {
        auto ast = parse_sql("CREATE TABLE t2 (sum_val INT COMPUTED BY (a + b), a INT, b INT)");
        assert(ast.kind == NodeKind::DdlTable);
        bool found = false;
        for (auto& kv : ast.ddlTable.computed_by)
            if (kv.first == "sum_val") {
                found = true;
                break;
            }
        assert(found);
    }
    // INDEX
    {
        auto ast = parse_sql("CREATE UNIQUE INDEX ix_t_id ON t (id) ACTIVE");
        assert(ast.kind == NodeKind::DdlIndex);
        assert(ast.ddlIndex.unique == true);
        assert(ast.ddlIndex.on_table == "t");
        assert(ast.ddlIndex.expr_raw.find("id") != std::string::npos);
        assert(ast.ddlIndex.options.find("ACTIVE") != std::string::npos);
    }
    // INDEX with ASC/DESC per entry
    {
        auto ast = parse_sql("CREATE INDEX ix_t_name ON t (name ASC, id DESC)");
        assert(ast.kind == NodeKind::DdlIndex);
        assert(ast.ddlIndex.column_directions.size() == 2);
        assert(ast.ddlIndex.column_directions[0].second == "ASC");
        assert(ast.ddlIndex.column_directions[1].second == "DESC");
    }
    // INDEX with COLLATE on entry
    {
        auto ast = parse_sql("CREATE INDEX ix_t_c ON t (name COLLATE UNICODE_CI ASC)");
        assert(ast.kind == NodeKind::DdlIndex);
        bool saw = false;
        for (auto& p : ast.ddlIndex.column_collates)
            if (p.second == "UNICODE_CI") {
                saw = true;
                break;
            }
        assert(saw);
    }
    // Expression index with direction
    {
        auto ast = parse_sql("CREATE INDEX ix_t_upper_name ON t ( UPPER ( \"Name\" )  DESC)");
        assert(ast.kind == NodeKind::DdlIndex);
        assert(ast.ddlIndex.column_directions.size() == 1);
        assert(ast.ddlIndex.column_directions[0].second == "DESC");
        // Base should include expression
        assert(ast.ddlIndex.column_directions[0].first.find("UPPER(") != std::string::npos);
    }
    // Computed by index form
    {
        auto ast = parse_sql("CREATE INDEX ix_comp ON t COMPUTED BY (a + b)");
        assert(ast.kind == NodeKind::DdlIndex);
        assert(ast.ddlIndex.expr_raw.find("a + b") != std::string::npos);
    }
    // ALTER INDEX REBUILD and SET STATISTICS
    {
        auto ast = parse_sql("ALTER INDEX ix REBUILD");
        assert(ast.kind == NodeKind::DdlIndex);
        assert(ast.ddlIndex.rebuild == true);
    }
    {
        auto ast = parse_sql("ALTER INDEX ix SET STATISTICS 75");
        assert(ast.kind == NodeKind::DdlIndex);
        assert(ast.ddlIndex.statistics.find("75") != std::string::npos);
    }
    // SEQUENCE
    {
        auto ast = parse_sql("ALTER SEQUENCE s RESTART WITH 100 INCREMENT BY 5 CYCLE");
        assert(ast.kind == NodeKind::DdlSequence);
        assert(ast.ddlSequence.name == "s");
        assert(ast.ddlSequence.action.find("RESTART") != std::string::npos);
        assert(ast.ddlSequence.increment_by == 5);
        assert(ast.ddlSequence.cycle == true);
    }
    // ALTER TABLE minimal capture
    {
        auto ast = parse_sql("ALTER TABLE t ALTER COLUMN x TYPE VARCHAR(50)");
        assert(ast.kind == NodeKind::DdlTable);
        assert(ast.ddlTable.name == "t");
        assert(ast.ddlTable.table_attrs_raw.find("ALTER COLUMN") != std::string::npos);
        assert(!ast.ddlTable.alter_ops.empty());
        assert(ast.ddlTable.alter_ops[0].kind == "ALTER");
    }
    // Per-column charset/collate capture
    {
        auto ast = parse_sql("CREATE TABLE t3 (c1 VARCHAR(20) CHARACTER SET UTF8 COLLATE UNICODE)");
        assert(ast.kind == NodeKind::DdlTable);
        bool saw_cs = false, saw_co = false;
        for (auto& p : ast.ddlTable.column_charsets)
            if (p.first == "c1") {
                saw_cs = true;
                break;
            }
        for (auto& p : ast.ddlTable.column_collates)
            if (p.first == "c1") {
                saw_co = true;
                break;
            }
        assert(saw_cs && saw_co);
    }
    // NOT NULL and IDENTITY capture
    {
        auto ast =
            parse_sql("CREATE TABLE t4 (id INT GENERATED ALWAYS AS IDENTITY, n INT NOT NULL)");
        assert(ast.kind == NodeKind::DdlTable);
        bool saw_nn = false, saw_ident = false;
        for (auto& c : ast.ddlTable.not_null_columns)
            if (c == "n") {
                saw_nn = true;
                break;
            }
        for (auto& p : ast.ddlTable.identity_columns)
            if (p.first == "id") {
                saw_ident = true;
                break;
            }
        assert(saw_nn && saw_ident);
    }
    // GENERATED ALWAYS AS (expr) treated as computed-by; VIRTUAL warned
    {
        auto ast = parse_sql("CREATE TABLE t5 (c INT GENERATED ALWAYS AS (a + b) VIRTUAL)");
        assert(ast.kind == NodeKind::DdlTable);
        bool saw_comp = false, warned = false;
        for (auto& kv : ast.ddlTable.computed_by)
            if (kv.first == "t5" || kv.first == "c") {
                saw_comp = true;
                break;
            }
        for (auto& w : ast.warnings)
            if (w.find("VIRTUAL") != std::string::npos) {
                warned = true;
                break;
            }
        assert(saw_comp && warned);
    }
    // DOMAIN
    {
        auto ast = parse_sql(
            "CREATE DOMAIN d AS VARCHAR(10) DEFAULT 'x' CHECK (value <> '') COLLATE UTF8");
        assert(ast.kind == NodeKind::DdlDomain);
        assert(ast.ddlDomain.name == "d");
        assert(ast.ddlDomain.type_raw.find("VARCHAR") != std::string::npos);
        assert(!ast.ddlDomain.default_raw.empty());
        assert(ast.ddlDomain.check_raw.find("CHECK") != std::string::npos);
    }
    // DOMAIN CHECK missing parentheses warning
    {
        auto ast = parse_sql("CREATE DOMAIN d AS INT CHECK value > 0");
        assert(ast.kind == NodeKind::DdlDomain);
        bool saw_warning = false;
        for (const auto& w : ast.warnings) {
            if (w.find("DOMAIN CHECK missing parentheses") != std::string::npos) {
                saw_warning = true;
                break;
            }
        }
        assert(saw_warning);
        // spans present
        assert(ast.warning_spans.empty() ||
               (ast.warning_spans[0].end > ast.warning_spans[0].start));
    }
    return 0;
}
