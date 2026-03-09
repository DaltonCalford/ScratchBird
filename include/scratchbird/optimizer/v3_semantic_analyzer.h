#pragma once

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/status.h"
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/parser/ast_v3.h"

#include <optional>
#include <string>
#include <vector>

namespace scratchbird::optimizer
{

    enum class ResolvedPredicateKind : uint8_t
    {
        NONE = 0,
        EQUALITY = 1,
        RANGE = 2,
        LIKE_PREFIX = 3
    };

    struct ResolvedScanPredicate
    {
        ResolvedPredicateKind kind = ResolvedPredicateKind::NONE;
        size_t relation_index = 0;
        core::ID column_id{};
        std::string column_name;
        std::string operator_name;
        std::string literal_kind;
        std::string literal_text;
        std::string predicate_text;
        const parser::v3::Expression *expression = nullptr;
        bool has_index_match = false;
        core::CatalogManager::IndexInfo matched_index{};
    };

    struct ResolvedRelation
    {
        size_t source_relation_index = 0;
        std::string table_path;
        std::string alias;
        bool resolved = false;
        bool derived = false;
        const parser::v3::TableRefNode *table_ref = nullptr;
        core::CatalogManager::TableInfo table_info{};
        std::vector<core::CatalogManager::ColumnInfo> columns;
        std::vector<core::CatalogManager::IndexInfo> indexes;
        uint64_t estimated_rows = 1000;
        uint64_t estimated_pages = 10;
        std::optional<ResolvedScanPredicate> local_predicate;
    };

    struct ResolvedJoin
    {
        size_t source_join_index = 0;
        size_t left_relation_index = 0;
        size_t right_relation_index = 0;
        parser::JoinType join_type = parser::JoinType::INNER;
        const parser::v3::Expression *condition = nullptr;
        std::vector<std::string> using_columns;
        bool natural = false;
        bool equi_join = false;
        std::string condition_text;
        std::string left_hash_qualifier;
        std::string left_hash_column;
        std::string right_hash_qualifier;
        std::string right_hash_column;
        core::ID left_hash_column_id{};
        core::ID right_hash_column_id{};
        bool has_hash_column_ids = false;
    };

    struct ResolvedSelectQuery
    {
        const parser::v3::SelectStmt *stmt = nullptr;
        const parser::v3::StringPool *string_pool = nullptr;
        std::vector<ResolvedRelation> relations;
        std::vector<ResolvedJoin> joins;
        bool all_joins_inner = true;
        bool contains_outer_join = false;
        bool has_unsupported_relation = false;
    };

    class V3SemanticAnalyzer
    {
    public:
        explicit V3SemanticAnalyzer(core::Database *db,
                                    StatisticsManager *stats_manager = nullptr)
            : db_(db), stats_manager_(stats_manager)
        {
        }

        auto resolveSelect(const parser::v3::SelectStmt *stmt,
                           const parser::v3::StringPool &pool,
                           ResolvedSelectQuery &out,
                           core::ConnectionContext *conn_ctx = nullptr,
                           const core::ID &current_schema_id = core::ID{},
                           core::ErrorContext *ctx = nullptr) -> core::Status;

    private:
        core::Database *db_;
        StatisticsManager *stats_manager_;
    };

} // namespace scratchbird::optimizer
