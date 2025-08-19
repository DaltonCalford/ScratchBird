#include "isql/meta.h"
#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"
#include "scratchbird/engine/parser_select.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace scratchbird::tools::isql;
using scratchbird::engine::ExecutionResult;

static void print_rows(const ExecutionResult& r)
{
    if (!r.columns.empty()) {
        for (size_t i = 0; i < r.columns.size(); ++i) {
            std::printf("%s%s", r.columns[i].c_str(), (i + 1 == r.columns.size() ? "\n" : "\t"));
        }
    }
    for (const auto& row : r.rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            std::printf("%s%s", row[i].c_str(), (i + 1 == row.size() ? "\n" : "\t"));
        }
    }
}

int main(int argc, char** argv)
{
    std::string db_path;
    if (argc > 1)
        db_path = argv[1];
    scratchbird::engine::set_executor_db_path(db_path);

    char* lineptr = nullptr;
    size_t n = 0;
    while (true) {
        std::printf("isql> ");
        ssize_t r = getline(&lineptr, &n, stdin);
        if (r <= 0)
            break;
        std::string line(lineptr, static_cast<size_t>(r));
        auto mc = parse_meta_command(line);
        if (mc.kind == MetaCmdKind::Connect) {
            db_path = mc.arg;
            scratchbird::engine::set_executor_db_path(db_path);
            std::printf("Connected to %s\n", db_path.c_str());
            continue;
        }
        if (mc.kind == MetaCmdKind::Show) {
            std::string arg = mc.arg;
            if (arg == "CATALOG" || arg == "catalog") {
                scratchbird::engine::CatalogManager cm(db_path);
                auto ver = cm.current_version();
                std::printf("Catalog %u.%u\n", unsigned(ver.major), unsigned(ver.minor));
                auto schemas = cm.list_schemas();
                std::printf("Schemas (%zu):\n", schemas.size());
                for (auto& [oid, name] : schemas) {
                    std::printf("  %s\n", name.c_str());
                }
                continue;
            }
            if (arg.rfind("RELATIONS", 0) == 0 || arg.rfind("relations", 0) == 0) {
                std::string schema;
                auto sp = arg.find(' ');
                if (sp != std::string::npos)
                    schema = arg.substr(sp + 1);
                scratchbird::engine::CatalogManager cm(db_path);
                std::optional<scratchbird::engine::UuidBytes> soid;
                if (!schema.empty())
                    soid = cm.lookup_schema_oid_by_name(schema);
                auto rels = cm.list_relations(soid);
                if (!schema.empty())
                    std::printf("Relations in %s (%zu):\n", schema.c_str(), rels.size());
                else
                    std::printf("Relations (%zu):\n", rels.size());
                for (auto& [oid, name] : rels) {
                    std::printf("  %s\n", name.c_str());
                }
                continue;
            }
            if (arg.rfind("INDEXES", 0) == 0 || arg.rfind("indexes", 0) == 0) {
                // Minimal: list index objects by scanning SDB$OBJECT type=INDEX
                std::string schema;
                auto sp = arg.find(' ');
                if (sp != std::string::npos)
                    schema = arg.substr(sp + 1);
                scratchbird::engine::CatalogManager cm(db_path);
                std::optional<scratchbird::engine::UuidBytes> soid;
                if (!schema.empty())
                    soid = cm.lookup_schema_oid_by_name(schema);
                auto objs = cm.list_objects_in_schema(soid);
                std::printf("Indexes%s:\n", schema.empty() ? "" : (" in " + schema).c_str());
                for (auto& [oid, type, name] : objs) {
                    if (type == "INDEX")
                        std::printf("  %s\n", name.c_str());
                }
                continue;
            }
            if (arg.rfind("COLUMNS", 0) == 0 || arg.rfind("columns", 0) == 0) {
                std::string rest;
                auto sp = arg.find(' ');
                if (sp != std::string::npos)
                    rest = arg.substr(sp + 1);
                std::string schema = "public";
                std::string rel = rest;
                auto dot = rest.find('.');
                if (dot != std::string::npos) {
                    schema = rest.substr(0, dot);
                    rel = rest.substr(dot + 1);
                }
                scratchbird::engine::CatalogManager cm(db_path);
                auto soid = cm.lookup_schema_oid_by_name(schema);
                if (!soid) {
                    std::printf("Schema not found: %s\n", schema.c_str());
                    continue;
                }
                auto roid = cm.lookup_object_oid(soid, "RELATION", rel);
                if (!roid)
                    roid = cm.lookup_object_oid(soid, "TABLE", rel);
                if (!roid) {
                    std::printf("Relation not found: %s.%s\n", schema.c_str(), rel.c_str());
                    continue;
                }
                auto cols = cm.list_columns(*roid);
                std::printf("Columns of %s.%s (%zu):\n", schema.c_str(), rel.c_str(), cols.size());
                for (auto& [pos, name] : cols) {
                    std::printf("  %lld: %s\n", (long long)pos, name.c_str());
                }
                continue;
            }
            if (arg == "DOMAINS" || arg == "domains") {
                scratchbird::engine::CatalogManager cm(db_path);
                auto doms = cm.list_domains();
                std::printf("Domains (%zu):\n", doms.size());
                for (auto& t : doms) {
                    std::printf("  %s %s(%lld,%lld,%lld)\n", std::get<0>(t).c_str(),
                                std::get<1>(t).c_str(), (long long)std::get<2>(t),
                                (long long)std::get<3>(t), (long long)std::get<4>(t));
                }
                continue;
            }
            if (arg.rfind("VIEWS", 0) == 0 || arg.rfind("views", 0) == 0) {
                std::string schema;
                auto sp = arg.find(' ');
                if (sp != std::string::npos)
                    schema = arg.substr(sp + 1);
                scratchbird::engine::CatalogManager cm(db_path);
                std::optional<scratchbird::engine::UuidBytes> soid;
                if (!schema.empty())
                    soid = cm.lookup_schema_oid_by_name(schema);
                auto views = cm.list_views(soid);
                if (!schema.empty())
                    std::printf("Views in %s (%zu):\n", schema.c_str(), views.size());
                else
                    std::printf("Views (%zu):\n", views.size());
                for (auto& [oid, name] : views) {
                    std::printf("  %s\n", name.c_str());
                }
                continue;
            }
            if (arg.rfind("VIEWDEF", 0) == 0 || arg.rfind("viewdef", 0) == 0) {
                std::string qual;
                auto sp = arg.find(' ');
                if (sp != std::string::npos)
                    qual = arg.substr(sp + 1);
                if (qual.empty()) {
                    std::printf("usage: SHOW VIEWDEF schema.view\n");
                    continue;
                }
                std::string schema = "public";
                std::string view = qual;
                auto dot = qual.find('.');
                if (dot != std::string::npos) {
                    schema = qual.substr(0, dot);
                    view = qual.substr(dot + 1);
                }
                scratchbird::engine::CatalogManager cm(db_path);
                auto soid = cm.lookup_schema_oid_by_name(schema);
                if (!soid) {
                    std::printf("Schema not found: %s\n", schema.c_str());
                    continue;
                }
                auto def = cm.get_view_definition(soid, view);
                if (def.empty())
                    std::printf("No definition found for %s.%s\n", schema.c_str(), view.c_str());
                else
                    std::printf("%s\n", def.c_str());
                continue;
            }
            if (arg.rfind("CONSTRAINTS", 0) == 0 || arg.rfind("constraints", 0) == 0) {
                std::string qual;
                auto sp = arg.find(' ');
                if (sp != std::string::npos)
                    qual = arg.substr(sp + 1);
                if (qual.empty()) {
                    std::printf("usage: SHOW CONSTRAINTS schema.table\n");
                    continue;
                }
                std::string schema = "public";
                std::string rel = qual;
                auto dot = qual.find('.');
                if (dot != std::string::npos) {
                    schema = qual.substr(0, dot);
                    rel = qual.substr(dot + 1);
                }
                scratchbird::engine::CatalogManager cm(db_path);
                auto soid = cm.lookup_schema_oid_by_name(schema);
                if (!soid) {
                    std::printf("Schema not found: %s\n", schema.c_str());
                    continue;
                }
                auto cons = cm.list_relation_constraints_by_name(soid, rel);
                std::printf("Constraints of %s.%s (%zu):\n", schema.c_str(), rel.c_str(),
                            cons.size());
                for (const auto& c : cons) {
                    std::printf("  %s %s deferrable=%s initially=%s cols=[", c.name.c_str(),
                                c.type.c_str(), c.deferrable ? "TRUE" : "FALSE",
                                c.initially_deferred ? "DEFERRED" : "IMMEDIATE");
                    for (size_t i = 0; i < c.columns.size(); ++i) {
                        std::printf("%s%s", c.columns[i].c_str(),
                                    (i + 1 == c.columns.size() ? "" : ","));
                    }
                    std::printf("]");
                    if (!c.check_expr.empty())
                        std::printf(" check=(%s)", c.check_expr.c_str());
                    if (!c.ref_relation.empty()) {
                        std::printf(" -> %s (", c.ref_relation.c_str());
                        for (size_t i = 0; i < c.ref_columns.size(); ++i) {
                            std::printf("%s%s", c.ref_columns[i].c_str(),
                                        (i + 1 == c.ref_columns.size() ? "" : ","));
                        }
                        std::printf(") on_update=%s on_delete=%s", c.on_update.c_str(),
                                    c.on_delete.c_str());
                    }
                    std::printf("\n");
                }
                continue;
            }
            if (arg.rfind("TRIGGERS", 0) == 0 || arg.rfind("triggers", 0) == 0) {
                std::string qual;
                auto sp = arg.find(' ');
                if (sp != std::string::npos)
                    qual = arg.substr(sp + 1);
                if (qual.empty()) {
                    std::printf("usage: SHOW TRIGGERS schema.table\n");
                    continue;
                }
                std::string schema = "public";
                std::string rel = qual;
                auto dot = qual.find('.');
                if (dot != std::string::npos) {
                    schema = qual.substr(0, dot);
                    rel = qual.substr(dot + 1);
                }
                scratchbird::engine::CatalogManager cm(db_path);
                auto soid = cm.lookup_schema_oid_by_name(schema);
                if (!soid) {
                    std::printf("Schema not found: %s\n", schema.c_str());
                    continue;
                }
                auto tr = cm.list_relation_triggers_by_name(soid, rel);
                std::printf("Triggers of %s.%s (%zu):\n", schema.c_str(), rel.c_str(), tr.size());
                for (const auto& t : tr) {
                    std::printf("  %s %s %s position=%d\n", t.name.c_str(), t.timing.c_str(),
                                t.events.c_str(), t.position);
                }
                continue;
            }
        }
        // Otherwise treat as SQL: EXPLAIN handled in executor; simple SELECTs run via executor
        auto ast = scratchbird::engine::parse_sql(line);
        scratchbird::engine::ExecutionResult res;
        if (line.size() >= 7 && (line.rfind("EXPLAIN", 0) == 0 || line.rfind("explain", 0) == 0)) {
            // Support EXPLAIN ANALYZE SELECT ...
            std::string rest = line.substr(7);
            // trim leading spaces
            while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t'))
                rest.erase(rest.begin());
            bool analyze = false;
            if (rest.rfind("ANALYZE", 0) == 0 || rest.rfind("analyze", 0) == 0) {
                analyze = true;
                rest = rest.substr(7);
                while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t'))
                    rest.erase(rest.begin());
            }
            if (analyze)
                res = scratchbird::engine::explain_analyze_select_sql_multiline(rest);
            else {
                // Use optimizer plan (multiline)
                scratchbird::engine::SelectQuery q =
                    scratchbird::engine::parse_select_minimal(rest);
                auto lines = scratchbird::engine::optimize_select_plan_multiline(q, false);
                res.columns = {"Plan"};
                res.rows.clear();
                for (auto& ln : lines)
                    res.rows.push_back({ln});
            }
        } else if (line.size() >= 6 &&
                   (line.rfind("SELECT", 0) == 0 || line.rfind("select", 0) == 0)) {
            res = scratchbird::engine::execute_select_sql(line);
        } else if (line.size() >= 6 &&
                   (line.rfind("INSERT", 0) == 0 || line.rfind("insert", 0) == 0)) {
            res = scratchbird::engine::execute_insert_sql(line);
        } else if (line.size() >= 6 &&
                   (line.rfind("UPDATE", 0) == 0 || line.rfind("update", 0) == 0)) {
            res = scratchbird::engine::execute_update_sql(line);
        } else if (line.size() >= 6 &&
                   (line.rfind("DELETE", 0) == 0 || line.rfind("delete", 0) == 0)) {
            res = scratchbird::engine::execute_delete_sql(line);
        } else if (line.find(" UNION ") != std::string::npos ||
                   line.find(" INTERSECT ") != std::string::npos ||
                   line.find(" EXCEPT ") != std::string::npos) {
            // Fallback: route through SELECT path (parser detects compound)
            res = scratchbird::engine::execute_select_sql(line);
        } else {
            // Quick SET CONSTRAINTS toggle (session-wide; minimal)
            std::string low = line;
            for (auto& ch : low)
                ch = char(std::tolower((unsigned char)ch));
            if (low.rfind("set constraints", 0) == 0) {
                bool defer = (low.find(" deferred") != std::string::npos);
                // capture names between keyword and mode, supporting ALL
                std::string tail = low.substr(std::string("set constraints").size());
                while (!tail.empty() && (tail[0] == ' ' || tail[0] == '\t'))
                    tail.erase(tail.begin());
                std::vector<std::string> names;
                if (tail.rfind("all", 0) == 0) {
                    if (defer)
                        scratchbird::engine::set_constraints_deferred_all(true);
                    else
                        scratchbird::engine::set_constraints_immediate_all(true);
                } else {
                    // names comma-separated until mode token
                    auto mode_pos = tail.find(defer ? " deferred" : " immediate");
                    std::string list =
                        (mode_pos == std::string::npos) ? tail : tail.substr(0, mode_pos);
                    size_t p = 0;
                    while (p < list.size()) {
                        auto comma = list.find(',', p);
                        std::string tok = list.substr(
                            p, comma == std::string::npos ? std::string::npos : comma - p);
                        // trim
                        while (!tok.empty() && (tok.front() == ' ' || tok.front() == '\t'))
                            tok.erase(tok.begin());
                        while (!tok.empty() && (tok.back() == ' ' || tok.back() == '\t'))
                            tok.pop_back();
                        if (!tok.empty())
                            names.push_back(tok);
                        if (comma == std::string::npos)
                            break;
                        p = comma + 1;
                    }
                    if (defer)
                        scratchbird::engine::set_constraints_deferred_list(names, true);
                    else
                        scratchbird::engine::set_constraints_immediate_list(names, true);
                }
                res.columns = {"ok"};
                res.rows = {{defer ? "constraints deferred" : "constraints immediate"}};
            } else if (low == "commit") {
                auto err = scratchbird::engine::executor_commit();
                if (!err.empty()) {
                    res.columns = {"error"};
                    res.rows = {{err}};
                } else {
                    res.columns = {"ok"};
                    res.rows = {{"commit"}};
                }
            } else if (low == "rollback") {
                scratchbird::engine::executor_rollback();
                res.columns = {"ok"};
                res.rows = {{"rollback"}};
            } else {
                res = scratchbird::engine::execute_ast(ast);
            }
        }
        print_rows(res);
    }
    free(lineptr);
    return 0;
}
