/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/parser/ast_v3.h"
#include "scratchbird/parser/parser_v3.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

namespace {

using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::parser::v3::ParseResult;
using scratchbird::parser::v3::Parser;
using scratchbird::parser::v3::ParserOptions;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::testing::TestDatabaseFile;

struct SuccessScopeRow {
    std::string syntax_contract_id;
    std::string native_feature_key;
    std::string priority;
    bool mandatory_scope = false;
    std::string sql_probe;
};

struct RejectScopeRow {
    std::string case_id;
    std::string sql_probe;
    std::string expected_error_code;
    std::set<std::string> disabled_feature_keys;
};

struct BindingReportRow {
    std::string row_type;
    std::string row_id;
    std::string syntax_contract_id;
    std::string native_feature_key;
    std::string expected_mode;
    std::string status;
    std::string expected_code;
    std::string actual_code;
    std::string ast_kind;
    std::string root_opcode_symbol;
    std::string deterministic_hash_match;
    std::string notes;
};

std::vector<std::string> splitTabLine(const std::string& line) {
    std::vector<std::string> out;
    std::string cell;
    std::istringstream in(line);
    while (std::getline(in, cell, '\t')) {
        out.push_back(cell);
    }
    return out;
}

std::string trim(std::string value) {
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::set<std::string> splitCsvSet(const std::string& input) {
    std::set<std::string> out;
    std::string token;
    std::istringstream in(input);
    while (std::getline(in, token, ',')) {
        std::string normalized = trim(token);
        if (!normalized.empty()) {
            out.insert(std::move(normalized));
        }
    }
    return out;
}

std::filesystem::path resolvePath(const char* env_name, const std::filesystem::path& fallback) {
    const char* value = std::getenv(env_name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    return std::filesystem::path(value);
}

std::vector<SuccessScopeRow> loadSuccessScopeRows(const std::filesystem::path& map_path) {
    std::ifstream input(map_path);
    if (!input.is_open()) {
        ADD_FAILURE() << "Unable to open SYN13 scope file: " << map_path;
        return {};
    }

    std::vector<SuccessScopeRow> rows;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        if (line.rfind("syntax_contract_id\t", 0) == 0) {
            continue;
        }
        const std::vector<std::string> cols = splitTabLine(line);
        if (cols.size() < 5) {
            ADD_FAILURE() << "Malformed SYN13 scope row: " << line;
            continue;
        }

        SuccessScopeRow row;
        row.syntax_contract_id = cols[0];
        row.native_feature_key = cols[1];
        row.priority = cols[2];
        row.mandatory_scope = (cols[3] == "1");
        row.sql_probe = cols[4];
        rows.push_back(std::move(row));
    }

    return rows;
}

std::vector<RejectScopeRow> loadRejectScopeRows(const std::filesystem::path& map_path) {
    std::ifstream input(map_path);
    if (!input.is_open()) {
        ADD_FAILURE() << "Unable to open reject scope file: " << map_path;
        return {};
    }

    std::vector<RejectScopeRow> rows;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        if (line.rfind("case_id\t", 0) == 0) {
            continue;
        }
        const std::vector<std::string> cols = splitTabLine(line);
        if (cols.size() < 3) {
            ADD_FAILURE() << "Malformed reject scope row: " << line;
            continue;
        }

        RejectScopeRow row;
        row.case_id = cols[0];
        row.sql_probe = cols[1];
        row.expected_error_code = cols[2];
        if (cols.size() >= 4) {
            row.disabled_feature_keys = splitCsvSet(cols[3]);
        }
        rows.push_back(std::move(row));
    }

    return rows;
}

bool hasParseErrorCode(const ParseResult& result, std::string_view expected_code) {
    for (const auto& error : result.errors()) {
        if (error.message.find(expected_code) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string extractParseErrorCode(const ParseResult& result) {
    static const std::regex kCodePattern("([A-Z]{3}_[0-9]{4})");
    for (const auto& error : result.errors()) {
        std::smatch match;
        if (std::regex_search(error.message, match, kCodePattern) && match.size() >= 2) {
            return match[1].str();
        }
    }
    return "";
}

std::string extractTraceErrorCode(const QueryCompilerV3::TraceResult& result) {
    static const std::regex kCodePattern("([A-Z]{3}_[0-9]{4})");
    for (const auto& error : result.errors()) {
        std::smatch match;
        if (std::regex_search(error, match, kCodePattern) && match.size() >= 2) {
            return match[1].str();
        }
    }
    return "";
}

std::string sanitizeCsvCell(std::string value) {
    for (char& ch : value) {
        if (ch == ',' || ch == '\n' || ch == '\r') {
            ch = ' ';
        }
    }
    if (value.empty()) {
        return "-";
    }
    return value;
}

bool writeBindingReport(const std::filesystem::path& out_path,
                        const std::vector<BindingReportRow>& rows) {
    std::filesystem::create_directories(out_path.parent_path());
    std::ofstream out(out_path);
    if (!out.is_open()) {
        return false;
    }

    out << "row_type,row_id,syntax_contract_id,native_feature_key,expected_mode,status,"
           "expected_code,actual_code,ast_kind,root_opcode_symbol,deterministic_hash_match,notes\n";
    for (const auto& row : rows) {
        out << sanitizeCsvCell(row.row_type) << ","
            << sanitizeCsvCell(row.row_id) << ","
            << sanitizeCsvCell(row.syntax_contract_id) << ","
            << sanitizeCsvCell(row.native_feature_key) << ","
            << sanitizeCsvCell(row.expected_mode) << ","
            << sanitizeCsvCell(row.status) << ","
            << sanitizeCsvCell(row.expected_code) << ","
            << sanitizeCsvCell(row.actual_code) << ","
            << sanitizeCsvCell(row.ast_kind) << ","
            << sanitizeCsvCell(row.root_opcode_symbol) << ","
            << sanitizeCsvCell(row.deterministic_hash_match) << ","
            << sanitizeCsvCell(row.notes) << "\n";
    }
    return true;
}

}  // namespace

TEST(NativeSqlAstSblrBindingCoverageMeterTest, MandatoryScopeAndRejectMatrixAreDeterministic) {
    const std::filesystem::path success_scope_path =
        resolvePath("SB_AST_SBLR_BINDING_SCOPE",
                    std::filesystem::path(__FILE__).parent_path() / "data" /
                        "native_sql_syn13_registration_scope.tsv");
    const std::filesystem::path reject_scope_path =
        resolvePath("SB_AST_SBLR_REJECT_SCOPE",
                    std::filesystem::path(__FILE__).parent_path() / "data" /
                        "native_sql_reject_consistency_scope.tsv");
    const std::filesystem::path report_out_path =
        resolvePath("SB_AST_SBLR_BINDING_REPORT_OUT",
                    std::filesystem::path("/tmp/native_sql_ast_sblr_binding_report.csv"));

    const std::vector<SuccessScopeRow> success_rows = loadSuccessScopeRows(success_scope_path);
    const std::vector<RejectScopeRow> reject_rows = loadRejectScopeRows(reject_scope_path);
    ASSERT_FALSE(success_rows.empty()) << "SYN13 scope rows required for AST/SBLR binding audit";
    ASSERT_FALSE(reject_rows.empty()) << "reject scope rows required for reject matrix audit";

    auto db_file = std::make_unique<TestDatabaseFile>("native_sql_ast_sblr_binding_meter");
    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_file->path(), 16384, &ctx), Status::OK) << ctx.message;

    auto db = std::make_unique<Database>();
    ASSERT_EQ(db->open(db_file->path(), &ctx), Status::OK) << ctx.message;

    QueryCompilerV3 compiler(db.get());
    std::vector<BindingReportRow> report_rows;
    size_t failure_count = 0;

    for (const SuccessScopeRow& row : success_rows) {
        if (!row.mandatory_scope) {
            continue;
        }

        BindingReportRow report_row;
        report_row.row_type = "mandatory_scope";
        report_row.row_id = row.syntax_contract_id;
        report_row.syntax_contract_id = row.syntax_contract_id;
        report_row.native_feature_key = row.native_feature_key;
        report_row.expected_mode = "mandatory";
        report_row.expected_code = "";

        Parser parser(row.sql_probe);
        ParseResult parse_result = parser.parseStatement();
        report_row.ast_kind = parse_result.success()
                                  ? scratchbird::parser::v3::astKindToString(parse_result.statement()->kind())
                                  : "";

        QueryCompilerV3::TraceResult trace_first;
        QueryCompilerV3::TraceResult trace_second;
        bool trace_success = false;
        bool deterministic_hash_match = false;
        if (parse_result.success()) {
            trace_first = compiler.compileTrace(row.sql_probe);
            trace_second = compiler.compileTrace(row.sql_probe);
            trace_success = trace_first.success() && trace_second.success();
            if (trace_success) {
                deterministic_hash_match =
                    trace_first.digest().ast_hash == trace_second.digest().ast_hash &&
                    trace_first.digest().sblr_hash == trace_second.digest().sblr_hash &&
                    trace_first.digest().root_opcode_symbol == trace_second.digest().root_opcode_symbol &&
                    !trace_first.digest().ast_hash.empty() &&
                    !trace_first.digest().sblr_hash.empty() &&
                    !trace_first.digest().root_opcode_symbol.empty();
                report_row.root_opcode_symbol = trace_first.digest().root_opcode_symbol;
            } else {
                report_row.actual_code = extractTraceErrorCode(trace_first);
            }
        } else {
            report_row.actual_code = extractParseErrorCode(parse_result);
        }

        report_row.deterministic_hash_match = deterministic_hash_match ? "1" : "0";

        const bool pass = parse_result.success() && trace_success && deterministic_hash_match;
        if (!pass) {
            ++failure_count;
            if (!parse_result.success()) {
                report_row.notes = "parse_reject_for_mandatory_scope";
            } else if (!trace_success) {
                report_row.notes = "compile_trace_reject_for_mandatory_scope";
            } else {
                report_row.notes = "non_deterministic_ast_or_sblr_hash";
            }
            report_row.status = "fail";
        } else {
            report_row.notes = "binding_closed";
            report_row.status = "pass";
        }

        report_rows.push_back(std::move(report_row));
    }

    for (const RejectScopeRow& row : reject_rows) {
        BindingReportRow report_row;
        report_row.row_type = "reject_scope";
        report_row.row_id = row.case_id;
        report_row.syntax_contract_id = "";
        report_row.native_feature_key = "";
        report_row.expected_mode = "reject";
        report_row.expected_code = row.expected_error_code;

        ParserOptions options;
        options.disabled_feature_keys = row.disabled_feature_keys;
        Parser parser(row.sql_probe, std::move(options));
        ParseResult parse_result = parser.parseStatement();

        report_row.ast_kind = parse_result.success()
                                  ? scratchbird::parser::v3::astKindToString(parse_result.statement()->kind())
                                  : "";
        report_row.actual_code = extractParseErrorCode(parse_result);
        report_row.root_opcode_symbol = "";
        report_row.deterministic_hash_match = "1";

        const bool has_expected_reject =
            !parse_result.success() && hasParseErrorCode(parse_result, row.expected_error_code);
        if (!has_expected_reject) {
            ++failure_count;
            report_row.status = "fail";
            report_row.notes = "reject_code_mismatch";
        } else {
            report_row.status = "pass";
            report_row.notes = "deterministic_reject_match";
        }
        report_rows.push_back(std::move(report_row));
    }

    ASSERT_TRUE(writeBindingReport(report_out_path, report_rows))
        << "failed to write AST/SBLR binding report to " << report_out_path;
    EXPECT_EQ(failure_count, 0u);
}
