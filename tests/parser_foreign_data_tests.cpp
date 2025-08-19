#include "scratchbird/engine/parser.h"

#include <cassert>

using namespace scratchbird::engine;

int main()
{
    {
        auto ast = parse_sql("CREATE FOREIGN SERVER srv OPTIONS (dsn='foo', driver='odbc')");
        assert(ast.kind == NodeKind::DdlForeignServer);
        assert(ast.ddlForeignServer.name == "srv");
    }
    {
        auto ast = parse_sql("CREATE USER MAPPING FOR alice SERVER srv OPTIONS (user='a')");
        assert(ast.kind == NodeKind::DdlUserMapping);
        assert(ast.ddlUserMapping.user_name.find("alice") != std::string::npos);
        assert(ast.ddlUserMapping.server_name == "srv");
    }
    {
        auto ast = parse_sql("CREATE FOREIGN TABLE ft(a int) SERVER srv OPTIONS (table='X')");
        assert(ast.kind == NodeKind::DdlForeignTable);
        assert(ast.ddlForeignTable.name == "ft");
        assert(ast.ddlForeignTable.server_name == "srv");
        assert(ast.ddlForeignTable.columns_raw.find("a int") != std::string::npos);
    }
    {
        auto ast = parse_sql("IMPORT FOREIGN SCHEMA remote FROM SERVER srv INTO public");
        assert(ast.kind == NodeKind::DdlImportForeignSchema);
        assert(ast.ddlImportForeignSchema.remote_schema.find("remote") != std::string::npos);
        assert(ast.ddlImportForeignSchema.server_name.find("srv") != std::string::npos);
        assert(ast.ddlImportForeignSchema.into_schema.find("public") != std::string::npos);
    }
    return 0;
}
