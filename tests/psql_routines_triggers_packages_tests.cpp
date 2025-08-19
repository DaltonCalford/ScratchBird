#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    // PROCEDURE
    {
        auto ast = parse_sql("CREATE PROCEDURE p (a INT) RETURNS (b INT) AS BEGIN b = a; END");
        assert(ast.kind == NodeKind::PsqlRoutine);
        assert(ast.psqlRoutine.kind == "PROCEDURE");
        assert(ast.psqlRoutine.name == "p");
        assert(ast.psqlRoutine.params_in.find("a INT") != std::string::npos);
        assert(ast.psqlRoutine.returns.find("b INT") != std::string::npos);
        assert(ast.psqlRoutine.body_raw.find("b = a") != std::string::npos);
        assert(!ast.psqlRoutine.params.empty());
    }
    // Routine header malformed (missing right paren) recovers
    {
        auto ast = parse_sql("CREATE PROCEDURE bad_proc (a INT RETURNS (b INT) AS BEGIN END");
        assert(ast.kind == NodeKind::PsqlRoutine);
        bool warned = false;
        for (const auto& w : ast.warnings)
            if (w.find("ROUTINE header malformed; recovered") != std::string::npos) {
                warned = true;
                break;
            }
        assert(warned);
    }
    // FUNCTION
    {
        auto ast = parse_sql("CREATE FUNCTION f (IN x INT) RETURNS TABLE (y INT) DETERMINISTIC SQL "
                             "SECURITY DEFINER AS BEGIN RETURN x; END");
        assert(ast.kind == NodeKind::PsqlRoutine);
        assert(ast.psqlRoutine.kind == "FUNCTION");
        assert(ast.psqlRoutine.name == "f");
        assert(ast.psqlRoutine.attributes_raw.find("DETERMINISTIC") != std::string::npos);
        assert(ast.psqlRoutine.attributes_raw.find("SQL SECURITY") != std::string::npos);
        assert(ast.psqlRoutine.returns.find("y INT") != std::string::npos);
        bool saw_in = false;
        for (auto& p : ast.psqlRoutine.params) {
            if (p.first.find("IN x") != std::string::npos) {
                saw_in = true;
                break;
            }
        }
        assert(saw_in);
        // param_types populated
        assert(!ast.psqlRoutine.param_types.empty() &&
               ast.psqlRoutine.param_types[0].name.find("INT") != std::string::npos);
    }
    // RECREATE PROCEDURE/FUNCTION dispatch, and UDR routing
    {
        auto ast = parse_sql("RECREATE PROCEDURE p AS BEGIN END");
        assert(ast.kind == NodeKind::PsqlRoutine);
        assert(ast.psqlRoutine.name == "p");
    }
    {
        auto ast =
            parse_sql("RECREATE FUNCTION f2 EXTERNAL NAME 'mod!fn' ENGINE UDR RETURNS INTEGER");
        assert(ast.kind == NodeKind::DdlUdr);
        assert(ast.ddlUdr.external_name.find("mod!fn") != std::string::npos);
        assert(ast.ddlUdr.engine.find("UDR") != std::string::npos);
    }
    // TRIGGER
    {
        auto ast = parse_sql(
            "CREATE TRIGGER tr ACTIVE BEFORE INSERT OR UPDATE OF col1,col2 ON t FOR EACH ROW "
            "AS BEGIN SUSPEND; END");
        assert(ast.kind == NodeKind::PsqlTrigger);
        assert(ast.psqlTrigger.name == "tr");
        assert(ast.psqlTrigger.table == "t");
        assert(ast.psqlTrigger.timing == "BEFORE");
        assert(ast.psqlTrigger.for_each == "ROW");
        assert(ast.psqlTrigger.active);
        assert(ast.psqlTrigger.update_of_columns.size() == 2);
    }
    // PACKAGE / PACKAGE BODY
    {
        auto ast = parse_sql("CREATE PACKAGE pkg AS PROCEDURE p; END");
        assert(ast.kind == NodeKind::PsqlPackage);
        assert(ast.psqlPackage.name == "pkg");
        assert(!ast.psqlPackage.is_body);
    }
    {
        auto ast = parse_sql("CREATE PACKAGE BODY pkg AS BEGIN END");
        assert(ast.kind == NodeKind::PsqlPackage);
        assert(ast.psqlPackage.name == "pkg");
        assert(ast.psqlPackage.is_body);
    }
    return 0;
}
