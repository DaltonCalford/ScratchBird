#include "scratchbird/sblr/mysql_query_compiler.h"

namespace scratchbird {
namespace sblr {

namespace mysql = parser::mysql;

MySQLQueryCompiler::MySQLQueryCompiler(core::Database* db)
    : db_(db)
    , catalog_(db ? db->catalog_manager() : nullptr)
    , default_schema_("/remote/emulated/mysql/localhost/")
{
}

MySQLQueryCompiler::~MySQLQueryCompiler() = default;

MySQLCompilationResult MySQLQueryCompiler::compile(const std::string& sql) {
    return compileInternal(sql);
}

MySQLCompilationResult MySQLQueryCompiler::compileInternal(const std::string& sql) {
    MySQLCompilationResult result;
    MySQLCompilationStats stats;

    auto total_start = std::chrono::steady_clock::now();

    // Parsing + bytecode generation is handled by the MySQL parser itself
    auto parse_start = std::chrono::steady_clock::now();
    mysql::Parser parser(sql, db_, default_schema_);
    mysql::ParseResult parse_result = parser.parseStatement();
    auto parse_end = std::chrono::steady_clock::now();

    stats.parser_time = std::chrono::duration_cast<std::chrono::microseconds>(
        parse_end - parse_start);

    if (!parse_result.success()) {
        for (const auto& err : parse_result.errors()) {
            result.addError("Parse error at line " + std::to_string(err.location.line) +
                            ", column " + std::to_string(err.location.column) +
                            ": " + err.message);
        }
        return result;
    }

    result.setBytecode(parse_result.bytecode());
    stats.bytecode_size = parse_result.bytecode().size();

    auto total_end = std::chrono::steady_clock::now();
    stats.total_time = std::chrono::duration_cast<std::chrono::microseconds>(
        total_end - total_start);

    if (stats_enabled_) {
        result.setStats(stats);
    }

    return result;
}

} // namespace sblr
} // namespace scratchbird
