#include "scratchbird/engine/parser_ddl.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    // DECLARE EXTERNAL FUNCTION
    {
        auto ast = parse_ddl_udf("DECLARE EXTERNAL FUNCTION f ENTRY_POINT 'ep' MODULE_NAME 'mod'");
        assert(ast.kind == NodeKind::DdlUdf);
        assert(ast.ddlUdf.name == "f");
    }
    // CREATE FUNCTION ... EXTERNAL NAME ... ENGINE UDR
    {
        auto ast =
            parse_ddl_udr("CREATE FUNCTION f(x int) RETURNS INT EXTERNAL NAME 'pkg.f' ENGINE UDR");
        assert(ast.kind == NodeKind::DdlUdr);
        assert(ast.ddlUdr.name == "f");
        assert(ast.ddlUdr.external_name.find("pkg.f") != std::string::npos);
        assert(ast.ddlUdr.engine.find("UDR") != std::string::npos);
    }
    // DECLARE/CREATE BLOB FILTER
    {
        auto ast = parse_ddl_blob_filter(
            "DECLARE FILTER flt INPUT_TYPE 1 OUTPUT_TYPE 2 ENTRY_POINT 'ep' MODULE_NAME 'mod'");
        assert(ast.kind == NodeKind::DdlBlobFilter);
        assert(ast.ddlBlobFilter.name == "flt");
        assert(ast.ddlBlobFilter.input_type == "1");
        assert(ast.ddlBlobFilter.output_type == "2");
        assert(ast.ddlBlobFilter.entry_point == "ep");
        assert(ast.ddlBlobFilter.module_name == "mod");
    }
    {
        auto ast = parse_ddl_blob_filter(
            "CREATE FILTER flt INPUT_TYPE 3 OUTPUT_TYPE 4 ENTRY_POINT 'E' MODULE_NAME 'X'");
        assert(ast.kind == NodeKind::DdlBlobFilter);
        assert(ast.ddlBlobFilter.name == "flt");
    }
    {
        auto ast = parse_ddl_blob_filter("DROP FILTER flt");
        assert(ast.kind == NodeKind::DdlBlobFilter);
        assert(ast.ddlBlobFilter.name == "flt");
    }
    // CREATE MAPPING
    {
        auto ast = parse_ddl_mapping("CREATE MAPPING m USING plugin FROM ldap TO srp");
        assert(ast.kind == NodeKind::DdlMapping);
        assert(ast.ddlMapping.name == "m");
    }
    // CREATE GLOBAL TEMPORARY TABLE
    {
        auto ast = parse_ddl_gtt("CREATE GLOBAL TEMPORARY TABLE g(a int) ON COMMIT PRESERVE ROWS");
        assert(ast.kind == NodeKind::DdlGtt);
        assert(ast.ddlGtt.name == "g");
        assert(ast.ddlGtt.on_commit == "PRESERVE ROWS");
        assert(!ast.ddlGtt.columns_raw.empty());
    }
    return 0;
}
