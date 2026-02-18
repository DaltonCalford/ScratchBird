/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "scratchbird/parser/parser_v3.h"

namespace {

using scratchbird::parser::v3::ParseResult;
using scratchbird::parser::v3::Parser;

struct ScopeRow {
    std::string syntax_contract_id;
    std::string native_feature_key;
    std::string priority;
    bool mandatory_scope = false;
    std::string sql_probe;
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

std::vector<ScopeRow> loadScopeRows() {
    const std::filesystem::path map_path =
        std::filesystem::path(__FILE__).parent_path() / "data" / "native_sql_syn13_registration_scope.tsv";
    std::ifstream input(map_path);
    if (!input.is_open()) {
        ADD_FAILURE() << "Unable to open coverage scope file: " << map_path;
        return {};
    }

    std::vector<ScopeRow> rows;
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
            ADD_FAILURE() << "Malformed scope row: " << line;
            continue;
        }

        ScopeRow row;
        row.syntax_contract_id = cols[0];
        row.native_feature_key = cols[1];
        row.priority = cols[2];
        row.mandatory_scope = (cols[3] == "1");
        row.sql_probe = cols[4];
        rows.push_back(std::move(row));
    }

    return rows;
}

bool parseSuccess(const std::string& sql, ParseResult& out_result) {
    Parser parser(sql);
    out_result = parser.parseStatement();
    return out_result.success();
}

}  // namespace

TEST(NativeSqlSyn13CoverageMeterTest, ScopeRowsAreUniqueAndNonEmpty) {
    const std::vector<ScopeRow> rows = loadScopeRows();
    ASSERT_FALSE(rows.empty()) << "coverage scope must contain at least one row";

    std::set<std::string> syntax_ids;
    std::set<std::string> feature_keys;
    size_t mandatory_count = 0;

    for (const ScopeRow& row : rows) {
        EXPECT_FALSE(row.syntax_contract_id.empty());
        EXPECT_FALSE(row.native_feature_key.empty());
        EXPECT_FALSE(row.priority.empty());
        EXPECT_FALSE(row.sql_probe.empty());

        EXPECT_TRUE(syntax_ids.insert(row.syntax_contract_id).second)
            << "Duplicate syntax contract id: " << row.syntax_contract_id;
        EXPECT_TRUE(feature_keys.insert(row.native_feature_key).second)
            << "Duplicate native feature key: " << row.native_feature_key;

        if (row.mandatory_scope) {
            ++mandatory_count;
        }
    }

    EXPECT_GT(mandatory_count, 0u) << "mandatory scope rows must be present";
}

TEST(NativeSqlSyn13CoverageMeterTest, MandatoryScopeSqlProbesParseSuccessfully) {
    const std::vector<ScopeRow> rows = loadScopeRows();
    ASSERT_FALSE(rows.empty());

    for (const ScopeRow& row : rows) {
        if (!row.mandatory_scope) {
            continue;
        }

        ParseResult result;
        const bool ok = parseSuccess(row.sql_probe, result);
        EXPECT_TRUE(ok)
            << row.syntax_contract_id << " / " << row.native_feature_key
            << " failed to parse probe SQL: " << row.sql_probe;
        if (!ok) {
            for (const auto& err : result.errors()) {
                ADD_FAILURE()
                    << "Parse error for " << row.syntax_contract_id
                    << " (" << row.native_feature_key << "): " << err.message;
            }
        }
    }
}
