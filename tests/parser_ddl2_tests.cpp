#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    // VIEW
    {
        auto ast = parse_sql("CREATE VIEW v AS SELECT 1 WITH CHECK OPTION");
        assert(ast.kind == NodeKind::DdlView);
        assert(ast.ddlView.name == "v");
        assert(ast.ddlView.with_check_option);
        assert(ast.ddlView.body_raw.find("SELECT 1") != std::string::npos);
    }
    // EXTERNAL TABLE attributes
    {
        auto ast = parse_sql("CREATE TABLE ext_t (a INT) EXTERNAL FILE '/var/tmp/data.dat'");
        assert(ast.kind == NodeKind::DdlTable);
        assert(ast.ddlTable.external_file.find("data.dat") != std::string::npos);
    }
    // USER attributes
    {
        auto ast = parse_sql("CREATE USER alice PASSWORD 'pw' FIRSTNAME 'A' LASTNAME 'L' ACTIVE");
        assert(ast.kind == NodeKind::DdlUser);
        assert(ast.ddlUser.name == "alice");
        assert(ast.ddlUser.password == "pw");
        assert(ast.ddlUser.first_name == "A");
        assert(ast.ddlUser.last_name == "L");
        assert(ast.ddlUser.active == true);
    }
    {
        auto ast = parse_sql("ALTER USER bob PASSWORD 'x' INACTIVE");
        assert(ast.kind == NodeKind::DdlUser);
        assert(ast.ddlUser.name == "bob");
        assert(ast.ddlUser.active == false);
    }
    // ROLE active/inactive
    {
        auto ast = parse_sql("ALTER ROLE r INACTIVE");
        assert(ast.kind == NodeKind::DdlRole);
        assert(ast.ddlRole.name == "r");
        assert(ast.ddlRole.active == false);
    }
    // RECREATE VIEW
    {
        auto ast = parse_sql("RECREATE VIEW v AS SELECT 1");
        assert(ast.kind == NodeKind::DdlView);
        assert(ast.ddlView.name == "v");
    }
    // VIEW column count vs select list heuristic
    {
        auto ast = parse_sql("CREATE VIEW v2(a,b) AS SELECT 1");
        assert(ast.kind == NodeKind::DdlView);
        bool warned = false;
        for (const auto& w : ast.warnings) {
            if (w.find("VIEW column list count does not match SELECT list count") !=
                std::string::npos) {
                warned = true;
                break;
            }
        }
        assert(warned);
    }
    // COLLATION
    {
        auto ast = parse_sql("CREATE COLLATION c_utf8 FOR UTF8 FROM EXTERNAL 'ucase'");
        assert(ast.kind == NodeKind::DdlCollation);
        assert(ast.ddlCollation.name.find("c_utf8") != std::string::npos);
    }
    // CHARSET
    {
        auto ast = parse_sql("CREATE CHARACTER SET cs");
        assert(ast.kind == NodeKind::DdlCharset);
        assert(ast.ddlCharset.name == "cs");
    }
    // EXCEPTION
    {
        auto ast = parse_sql("CREATE EXCEPTION ex 'oops'");
        assert(ast.kind == NodeKind::DdlException);
        assert(ast.ddlException.name == "ex");
        assert(ast.ddlException.message.find("oops") != std::string::npos);
    }
    // COMMENT ON
    {
        auto ast = parse_sql("COMMENT ON TABLE t IS 'hello'");
        assert(ast.kind == NodeKind::DdlComment);
        assert(ast.ddlComment.object_type == "table");
        assert(ast.ddlComment.object_name == "t");
    }
    // RENAME
    {
        auto ast = parse_sql("ALTER TABLE t RENAME TO t2");
        assert(ast.kind == NodeKind::DdlRename);
        assert(ast.ddlRename.old_name == "t");
        assert(ast.ddlRename.new_name == "t2");
    }
    // ROLE
    {
        auto ast = parse_sql("CREATE ROLE r");
        assert(ast.kind == NodeKind::DdlRole);
        assert(ast.ddlRole.name == "r");
    }
    // RECREATE PACKAGE/PACKAGE BODY routed to package parser
    {
        auto ast = parse_sql("RECREATE PACKAGE pkg AS PROCEDURE p; END");
        assert(ast.kind == NodeKind::PsqlPackage);
        assert(ast.psqlPackage.name == "pkg");
    }
    // USER
    {
        auto ast = parse_sql("CREATE USER u");
        assert(ast.kind == NodeKind::DdlUser);
        assert(ast.ddlUser.name == "u");
    }
    return 0;
}
