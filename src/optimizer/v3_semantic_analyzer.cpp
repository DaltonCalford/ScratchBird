#include "scratchbird/optimizer/v3_semantic_analyzer.h"

#include "scratchbird/core/debug.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>

namespace scratchbird::optimizer
{
    namespace
    {
        using ColumnInfo = core::CatalogManager::ColumnInfo;
        using IndexInfo = core::CatalogManager::IndexInfo;
        using SchemaInfo = core::CatalogManager::SchemaInfo;
        using TableInfo = core::CatalogManager::TableInfo;

        struct ColumnToken
        {
            std::string qualifier;
            std::string column_name;
        };

        auto toLowerAscii(std::string value) -> std::string
        {
            std::transform(value.begin(),
                           value.end(),
                           value.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return value;
        }

        auto toUpperAscii(std::string value) -> std::string
        {
            std::transform(value.begin(),
                           value.end(),
                           value.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
            return value;
        }

        auto isZeroId(const core::ID &id) -> bool
        {
            return id == core::ID{};
        }

        auto renderSchemaPath(const parser::v3::SchemaPath &path,
                              const parser::v3::StringPool &pool) -> std::string
        {
            if (path.components.empty())
            {
                return {};
            }

            std::ostringstream out;
            for (size_t i = 0; i < path.components.size(); ++i)
            {
                if (i > 0)
                {
                    out << '.';
                }
                out << pool.get(path.components[i]);
            }
            return out.str();
        }

        auto defaultAliasForTable(const parser::v3::TableRefNode *table_ref,
                                  const parser::v3::StringPool &pool) -> std::string
        {
            if (table_ref == nullptr)
            {
                return {};
            }

            if (table_ref->has_alias &&
                table_ref->alias != parser::v3::StringPool::INVALID_ID)
            {
                return std::string(pool.get(table_ref->alias));
            }

            if (table_ref->ref_type == parser::v3::TableRefNode::Type::TABLE &&
                !table_ref->table_path.components.empty())
            {
                return std::string(pool.get(table_ref->table_path.components.back()));
            }

            return {};
        }

        auto relationNameForLookup(const ResolvedRelation &relation) -> std::string
        {
            if (!relation.alias.empty())
            {
                return relation.alias;
            }
            if (!relation.table_info.table_name.empty())
            {
                return relation.table_info.table_name;
            }
            return relation.table_path;
        }

        auto addSchemaCandidate(core::CatalogManager *catalog,
                                const std::string &schema_name,
                                std::vector<core::ID> &schema_ids) -> void
        {
            if (catalog == nullptr || schema_name.empty())
            {
                return;
            }

            SchemaInfo info;
            core::ErrorContext schema_ctx;
            if (catalog->getSchema(schema_name, info, &schema_ctx) != core::Status::OK)
            {
                return;
            }

            if (std::find(schema_ids.begin(), schema_ids.end(), info.schema_id) == schema_ids.end())
            {
                schema_ids.push_back(info.schema_id);
            }
        }

        auto collectColumnToken(const parser::v3::Expression *expr,
                                const parser::v3::StringPool &pool,
                                ColumnToken &token_out) -> bool
        {
            if (expr == nullptr)
            {
                return false;
            }

            if (expr->kind() == parser::v3::ASTKind::CastExpr)
            {
                const auto *cast_expr = static_cast<const parser::v3::CastExpr *>(expr);
                return collectColumnToken(cast_expr->expr, pool, token_out);
            }

            if (expr->kind() != parser::v3::ASTKind::ColumnRefExpr)
            {
                return false;
            }

            const auto *column_ref = static_cast<const parser::v3::ColumnRefExpr *>(expr);
            if (column_ref->column.column_name == parser::v3::StringPool::INVALID_ID)
            {
                return false;
            }

            token_out.column_name = std::string(pool.get(column_ref->column.column_name));
            if (column_ref->column.has_table_qualifier &&
                !column_ref->column.table_path.components.empty())
            {
                token_out.qualifier = std::string(
                    pool.get(column_ref->column.table_path.components.back()));
            }
            return true;
        }

        auto expressionToString(const parser::v3::Expression *expr,
                                const parser::v3::StringPool &pool) -> std::string
        {
            if (expr == nullptr)
            {
                return {};
            }

            switch (expr->kind())
            {
                case parser::v3::ASTKind::LiteralExpr: {
                    const auto *literal = static_cast<const parser::v3::LiteralExpr *>(expr);
                    switch (literal->literal_type)
                    {
                        case parser::v3::LiteralType::INTEGER:
                            return std::to_string(literal->int_value);
                        case parser::v3::LiteralType::FLOAT: {
                            std::ostringstream out;
                            out << literal->float_value;
                            return out.str();
                        }
                        case parser::v3::LiteralType::STRING:
                            return "'" + std::string(pool.get(literal->string_value)) + "'";
                        case parser::v3::LiteralType::BOOLEAN:
                            return literal->bool_value ? "TRUE" : "FALSE";
                        case parser::v3::LiteralType::NULL_VALUE:
                            return "NULL";
                        default:
                            return "LITERAL";
                    }
                }
                case parser::v3::ASTKind::ColumnRefExpr: {
                    const auto *column_ref = static_cast<const parser::v3::ColumnRefExpr *>(expr);
                    std::ostringstream out;
                    if (column_ref->column.has_table_qualifier &&
                        !column_ref->column.table_path.components.empty())
                    {
                        out << renderSchemaPath(column_ref->column.table_path, pool) << '.';
                    }
                    out << pool.get(column_ref->column.column_name);
                    return out.str();
                }
                case parser::v3::ASTKind::BinaryExpr: {
                    const auto *binary = static_cast<const parser::v3::BinaryExpr *>(expr);
                    const char *op_name = "?";
                    switch (binary->op)
                    {
                        case parser::v3::BinaryOp::EQ: op_name = "="; break;
                        case parser::v3::BinaryOp::NE: op_name = "!="; break;
                        case parser::v3::BinaryOp::LT: op_name = "<"; break;
                        case parser::v3::BinaryOp::LE: op_name = "<="; break;
                        case parser::v3::BinaryOp::GT: op_name = ">"; break;
                        case parser::v3::BinaryOp::GE: op_name = ">="; break;
                        case parser::v3::BinaryOp::AND: op_name = "AND"; break;
                        case parser::v3::BinaryOp::OR: op_name = "OR"; break;
                        case parser::v3::BinaryOp::ADD: op_name = "+"; break;
                        case parser::v3::BinaryOp::SUB: op_name = "-"; break;
                        case parser::v3::BinaryOp::MUL: op_name = "*"; break;
                        case parser::v3::BinaryOp::DIV: op_name = "/"; break;
                        case parser::v3::BinaryOp::CONCAT: op_name = "||"; break;
                        default: break;
                    }
                    return "(" + expressionToString(binary->left, pool) + " " +
                           std::string(op_name) + " " +
                           expressionToString(binary->right, pool) + ")";
                }
                case parser::v3::ASTKind::LikeExpr: {
                    const auto *like_expr = static_cast<const parser::v3::LikeExpr *>(expr);
                    return expressionToString(like_expr->expr, pool) + " LIKE " +
                           expressionToString(like_expr->pattern, pool);
                }
                case parser::v3::ASTKind::BetweenExpr: {
                    const auto *between_expr = static_cast<const parser::v3::BetweenExpr *>(expr);
                    return expressionToString(between_expr->expr, pool) + " BETWEEN " +
                           expressionToString(between_expr->low, pool) + " AND " +
                           expressionToString(between_expr->high, pool);
                }
                case parser::v3::ASTKind::UnaryExpr: {
                    const auto *unary = static_cast<const parser::v3::UnaryExpr *>(expr);
                    switch (unary->op)
                    {
                        case parser::v3::UnaryOp::NOT:
                            return "NOT " + expressionToString(unary->operand, pool);
                        case parser::v3::UnaryOp::NEGATE:
                            return "-" + expressionToString(unary->operand, pool);
                        case parser::v3::UnaryOp::IS_NULL:
                            return expressionToString(unary->operand, pool) + " IS NULL";
                        case parser::v3::UnaryOp::IS_NOT_NULL:
                            return expressionToString(unary->operand, pool) + " IS NOT NULL";
                        default:
                            return "UNARY";
                    }
                }
                case parser::v3::ASTKind::FunctionCallExpr: {
                    const auto *func = static_cast<const parser::v3::FunctionCallExpr *>(expr);
                    std::ostringstream out;
                    out << renderSchemaPath(func->function_path, pool) << '(';
                    for (size_t i = 0; i < func->arguments.size(); ++i)
                    {
                        if (i > 0)
                        {
                            out << ", ";
                        }
                        out << expressionToString(func->arguments[i], pool);
                    }
                    out << ')';
                    return out.str();
                }
                default:
                    return parser::v3::astKindToString(expr->kind());
            }
        }

        auto relationIndexForToken(const std::vector<ResolvedRelation> &relations,
                                   const ColumnToken &token,
                                   core::ID &column_id_out,
                                   std::string &column_name_out) -> std::optional<size_t>
        {
            const std::string wanted_column = toLowerAscii(token.column_name);
            const std::string wanted_qualifier = toLowerAscii(token.qualifier);

            std::optional<size_t> match;
            for (size_t index = 0; index < relations.size(); ++index)
            {
                const auto &relation = relations[index];
                const std::string alias = toLowerAscii(relation.alias);
                const std::string table_name = toLowerAscii(relation.table_info.table_name);
                const std::string table_path = toLowerAscii(relation.table_path);

                if (!wanted_qualifier.empty() &&
                    wanted_qualifier != alias &&
                    wanted_qualifier != table_name &&
                    wanted_qualifier != table_path)
                {
                    continue;
                }

                for (const auto &column : relation.columns)
                {
                    if (toLowerAscii(column.column_name) != wanted_column)
                    {
                        continue;
                    }

                    if (match.has_value())
                    {
                        return std::nullopt;
                    }

                    match = index;
                    column_id_out = column.column_id;
                    column_name_out = column.column_name;
                }
            }
            return match;
        }

        auto literalToText(const parser::v3::Expression *expr,
                           const parser::v3::StringPool &pool,
                           std::string &kind_out,
                           std::string &text_out) -> bool
        {
            if (expr == nullptr || expr->kind() != parser::v3::ASTKind::LiteralExpr)
            {
                return false;
            }

            const auto *literal = static_cast<const parser::v3::LiteralExpr *>(expr);
            switch (literal->literal_type)
            {
                case parser::v3::LiteralType::INTEGER:
                    kind_out = "INT64";
                    text_out = std::to_string(literal->int_value);
                    return true;
                case parser::v3::LiteralType::FLOAT: {
                    kind_out = "DOUBLE";
                    std::ostringstream out;
                    out << literal->float_value;
                    text_out = out.str();
                    return true;
                }
                case parser::v3::LiteralType::STRING:
                    kind_out = "STRING";
                    text_out = std::string(pool.get(literal->string_value));
                    return true;
                case parser::v3::LiteralType::BOOLEAN:
                    kind_out = "BOOLEAN";
                    text_out = literal->bool_value ? "TRUE" : "FALSE";
                    return true;
                default:
                    return false;
            }
        }

        auto predicateKindForBinary(parser::v3::BinaryOp op) -> ResolvedPredicateKind
        {
            switch (op)
            {
                case parser::v3::BinaryOp::EQ:
                    return ResolvedPredicateKind::EQUALITY;
                case parser::v3::BinaryOp::LT:
                case parser::v3::BinaryOp::LE:
                case parser::v3::BinaryOp::GT:
                case parser::v3::BinaryOp::GE:
                    return ResolvedPredicateKind::RANGE;
                default:
                    return ResolvedPredicateKind::NONE;
            }
        }

        auto renderBinaryOp(parser::v3::BinaryOp op) -> std::string
        {
            switch (op)
            {
                case parser::v3::BinaryOp::EQ: return "=";
                case parser::v3::BinaryOp::LT: return "<";
                case parser::v3::BinaryOp::LE: return "<=";
                case parser::v3::BinaryOp::GT: return ">";
                case parser::v3::BinaryOp::GE: return ">=";
                default: return {};
            }
        }

        auto chooseIndexForColumn(const ResolvedRelation &relation,
                                  const core::ID &column_id,
                                  IndexInfo &index_out) -> bool
        {
            for (const auto &index : relation.indexes)
            {
                if (index.index_type != core::CatalogManager::IndexType::BTREE ||
                    index.column_ids.empty())
                {
                    continue;
                }
                if (index.column_ids.front() == column_id)
                {
                    index_out = index;
                    return true;
                }
            }
            return false;
        }

        auto extractSimplePredicate(const parser::v3::Expression *expr,
                                    const parser::v3::StringPool &pool,
                                    std::vector<ResolvedRelation> &relations,
                                    ResolvedScanPredicate &predicate_out) -> bool
        {
            if (expr == nullptr)
            {
                return false;
            }

            if (expr->kind() == parser::v3::ASTKind::BinaryExpr)
            {
                const auto *binary = static_cast<const parser::v3::BinaryExpr *>(expr);
                if (binary->op == parser::v3::BinaryOp::AND)
                {
                    return extractSimplePredicate(binary->left, pool, relations, predicate_out) ||
                           extractSimplePredicate(binary->right, pool, relations, predicate_out);
                }

                const ResolvedPredicateKind kind = predicateKindForBinary(binary->op);
                if (kind == ResolvedPredicateKind::NONE)
                {
                    return false;
                }

                ColumnToken token;
                std::string literal_kind;
                std::string literal_text;
                bool column_on_left = collectColumnToken(binary->left, pool, token) &&
                                      literalToText(binary->right, pool, literal_kind, literal_text);
                if (!column_on_left)
                {
                    column_on_left = collectColumnToken(binary->right, pool, token) &&
                                     literalToText(binary->left, pool, literal_kind, literal_text);
                }
                if (!column_on_left)
                {
                    return false;
                }

                core::ID column_id{};
                std::string resolved_column_name;
                const auto relation_index =
                    relationIndexForToken(relations, token, column_id, resolved_column_name);
                if (!relation_index.has_value())
                {
                    return false;
                }

                predicate_out.kind = kind;
                predicate_out.relation_index = *relation_index;
                predicate_out.column_id = column_id;
                predicate_out.column_name = resolved_column_name.empty()
                    ? token.column_name
                    : resolved_column_name;
                predicate_out.operator_name = renderBinaryOp(binary->op);
                predicate_out.literal_kind = literal_kind;
                predicate_out.literal_text = literal_text;
                predicate_out.predicate_text = expressionToString(expr, pool);
                predicate_out.expression = expr;
                IndexInfo matched_index;
                if (chooseIndexForColumn(relations[*relation_index], column_id, matched_index))
                {
                    predicate_out.has_index_match = true;
                    predicate_out.matched_index = matched_index;
                }
                return true;
            }

            if (expr->kind() == parser::v3::ASTKind::LikeExpr)
            {
                const auto *like_expr = static_cast<const parser::v3::LikeExpr *>(expr);
                ColumnToken token;
                std::string literal_kind;
                std::string literal_text;
                if (!collectColumnToken(like_expr->expr, pool, token) ||
                    !literalToText(like_expr->pattern, pool, literal_kind, literal_text))
                {
                    return false;
                }

                if (literal_kind != "STRING" || literal_text.empty() ||
                    literal_text.front() == '%' ||
                    literal_text.find('%') == std::string::npos)
                {
                    return false;
                }

                core::ID column_id{};
                std::string resolved_column_name;
                const auto relation_index =
                    relationIndexForToken(relations, token, column_id, resolved_column_name);
                if (!relation_index.has_value())
                {
                    return false;
                }

                predicate_out.kind = ResolvedPredicateKind::LIKE_PREFIX;
                predicate_out.relation_index = *relation_index;
                predicate_out.column_id = column_id;
                predicate_out.column_name = resolved_column_name.empty()
                    ? token.column_name
                    : resolved_column_name;
                predicate_out.operator_name = "LIKE";
                predicate_out.literal_kind = literal_kind;
                predicate_out.literal_text = literal_text;
                predicate_out.predicate_text = expressionToString(expr, pool);
                predicate_out.expression = expr;
                IndexInfo matched_index;
                if (chooseIndexForColumn(relations[*relation_index], column_id, matched_index))
                {
                    predicate_out.has_index_match = true;
                    predicate_out.matched_index = matched_index;
                }
                return true;
            }

            return false;
        }

        auto resolveTableLikeRelation(core::CatalogManager *catalog,
                                      StatisticsManager *stats_manager,
                                      const parser::v3::TableRefNode *table_ref,
                                      const parser::v3::StringPool &pool,
                                      core::ConnectionContext *conn_ctx,
                                      const core::ID &current_schema_id,
                                      size_t relation_index,
                                      ResolvedRelation &relation_out) -> core::Status
        {
            relation_out.source_relation_index = relation_index;
            relation_out.table_ref = table_ref;
            relation_out.alias = defaultAliasForTable(table_ref, pool);

            if (table_ref == nullptr)
            {
                return core::Status::OK;
            }

            if (table_ref->ref_type != parser::v3::TableRefNode::Type::TABLE)
            {
                relation_out.derived = true;
                relation_out.table_path = relation_out.alias.empty() ? "<derived>" : relation_out.alias;
                relation_out.estimated_rows = 1000;
                relation_out.estimated_pages = 10;
                return core::Status::OK;
            }

            relation_out.table_path = renderSchemaPath(table_ref->table_path, pool);
            const std::string table_name =
                table_ref->table_path.components.empty()
                    ? std::string()
                    : std::string(pool.get(table_ref->table_path.components.back()));

            if (table_name.empty() || catalog == nullptr)
            {
                relation_out.estimated_rows = 1000;
                relation_out.estimated_pages = 10;
                return core::Status::OK;
            }

            std::vector<core::ID> schema_candidates;
            if (table_ref->table_path.components.size() > 1)
            {
                std::ostringstream schema_name;
                for (size_t i = 0; i + 1 < table_ref->table_path.components.size(); ++i)
                {
                    if (i > 0)
                    {
                        schema_name << '.';
                    }
                    schema_name << pool.get(table_ref->table_path.components[i]);
                }
                addSchemaCandidate(catalog, schema_name.str(), schema_candidates);
            }

            if (!isZeroId(current_schema_id))
            {
                schema_candidates.push_back(current_schema_id);
            }

            if (conn_ctx != nullptr)
            {
                if (!isZeroId(conn_ctx->getCurrentSchemaId()))
                {
                    schema_candidates.push_back(conn_ctx->getCurrentSchemaId());
                }
                for (const auto &entry : conn_ctx->search_path())
                {
                    addSchemaCandidate(catalog, entry, schema_candidates);
                }
            }

            addSchemaCandidate(catalog, "users.public", schema_candidates);
            addSchemaCandidate(catalog, "public", schema_candidates);

            TableInfo table_info;
            core::ErrorContext table_ctx;
            bool resolved = false;
            for (const auto &schema_id : schema_candidates)
            {
                if (catalog->getTable(schema_id, table_name, table_info, &table_ctx) == core::Status::OK)
                {
                    resolved = true;
                    break;
                }
            }

            if (!resolved)
            {
                std::vector<SchemaInfo> schemas;
                if (catalog->listSchemas(schemas, &table_ctx) == core::Status::OK)
                {
                    TableInfo only_match;
                    size_t match_count = 0;
                    const std::string table_name_lower = toLowerAscii(table_name);
                    for (const auto &schema : schemas)
                    {
                        std::vector<TableInfo> tables;
                        if (catalog->listTables(schema.schema_id, tables, &table_ctx) != core::Status::OK)
                        {
                            continue;
                        }
                        for (const auto &table : tables)
                        {
                            if (toLowerAscii(table.table_name) == table_name_lower)
                            {
                                only_match = table;
                                ++match_count;
                            }
                        }
                    }
                    if (match_count == 1)
                    {
                        table_info = only_match;
                        resolved = true;
                    }
                }
            }

            relation_out.resolved = resolved;
            relation_out.table_info = table_info;
            relation_out.estimated_rows = table_info.row_count == 0 ? 1000 : table_info.row_count;
            relation_out.estimated_pages = std::max<uint64_t>(1, relation_out.estimated_rows / 100);
            if (resolved)
            {
                (void)catalog->getColumns(table_info.table_id, relation_out.columns, nullptr);
                (void)catalog->listIndexesForTable(table_info.table_id, relation_out.indexes, nullptr);

                if (stats_manager != nullptr)
                {
                    TableStatistics stats;
                    if (stats_manager->getTableStatistics(table_info.table_id, stats, nullptr) ==
                        core::Status::OK)
                    {
                        relation_out.estimated_rows =
                            stats.num_rows == 0 ? relation_out.estimated_rows : stats.num_rows;
                        relation_out.estimated_pages =
                            stats.num_pages == 0 ? relation_out.estimated_pages : stats.num_pages;
                    }
                }
            }
            else
            {
                relation_out.table_info.table_name = table_name;
            }

            if (relation_out.alias.empty())
            {
                relation_out.alias = relation_out.table_info.table_name.empty()
                    ? table_name
                    : relation_out.table_info.table_name;
            }
            return core::Status::OK;
        }

        auto extractJoinKeyFromEquality(const parser::v3::Expression *condition,
                                        const parser::v3::StringPool &pool,
                                        const std::vector<ResolvedRelation> &relations,
                                        ResolvedJoin &join_out) -> bool
        {
            if (condition == nullptr || condition->kind() != parser::v3::ASTKind::BinaryExpr)
            {
                return false;
            }

            const auto *binary = static_cast<const parser::v3::BinaryExpr *>(condition);
            if (binary->op != parser::v3::BinaryOp::EQ)
            {
                return false;
            }

            ColumnToken left_token;
            ColumnToken right_token;
            if (!collectColumnToken(binary->left, pool, left_token) ||
                !collectColumnToken(binary->right, pool, right_token))
            {
                return false;
            }

            core::ID left_column_id{};
            core::ID right_column_id{};
            std::string left_column_name;
            std::string right_column_name;
            const auto left_relation =
                relationIndexForToken(relations, left_token, left_column_id, left_column_name);
            const auto right_relation =
                relationIndexForToken(relations, right_token, right_column_id, right_column_name);
            if (!left_relation.has_value() ||
                !right_relation.has_value() ||
                *left_relation == *right_relation)
            {
                return false;
            }

            join_out.left_relation_index = *left_relation;
            join_out.right_relation_index = *right_relation;
            join_out.equi_join = true;
            join_out.left_hash_qualifier =
                left_token.qualifier.empty()
                    ? relationNameForLookup(relations[*left_relation])
                    : left_token.qualifier;
            join_out.left_hash_column = left_column_name.empty()
                ? left_token.column_name
                : left_column_name;
            join_out.right_hash_qualifier =
                right_token.qualifier.empty()
                    ? relationNameForLookup(relations[*right_relation])
                    : right_token.qualifier;
            join_out.right_hash_column = right_column_name.empty()
                ? right_token.column_name
                : right_column_name;
            join_out.left_hash_column_id = left_column_id;
            join_out.right_hash_column_id = right_column_id;
            join_out.has_hash_column_ids = !isZeroId(left_column_id) &&
                                           !isZeroId(right_column_id);
            return true;
        }
    } // namespace

    auto V3SemanticAnalyzer::resolveSelect(const parser::v3::SelectStmt *stmt,
                                           const parser::v3::StringPool &pool,
                                           ResolvedSelectQuery &out,
                                           core::ConnectionContext *conn_ctx,
                                           const core::ID &current_schema_id,
                                           core::ErrorContext *ctx) -> core::Status
    {
        (void)ctx;

        out = ResolvedSelectQuery{};
        out.stmt = stmt;
        out.string_pool = &pool;

        if (stmt == nullptr)
        {
            return core::Status::INVALID_ARGUMENT;
        }

        if (stmt->from != nullptr)
        {
            ResolvedRelation base_relation;
            resolveTableLikeRelation(db_ != nullptr ? db_->catalog_manager() : nullptr,
                                     stats_manager_,
                                     stmt->from,
                                     pool,
                                     conn_ctx,
                                     current_schema_id,
                                     0,
                                     base_relation);
            if (!base_relation.resolved &&
                stmt->from->ref_type != parser::v3::TableRefNode::Type::TABLE)
            {
                out.has_unsupported_relation = true;
            }
            out.relations.push_back(std::move(base_relation));
        }

        for (size_t index = 0; index < stmt->joins.size(); ++index)
        {
            const auto *join = stmt->joins[index];
            if (join == nullptr)
            {
                continue;
            }

            ResolvedRelation relation;
            resolveTableLikeRelation(db_ != nullptr ? db_->catalog_manager() : nullptr,
                                     stats_manager_,
                                     join->right,
                                     pool,
                                     conn_ctx,
                                     current_schema_id,
                                     out.relations.size(),
                                     relation);
            if (!relation.resolved &&
                join->right != nullptr &&
                join->right->ref_type != parser::v3::TableRefNode::Type::TABLE)
            {
                out.has_unsupported_relation = true;
            }
            out.relations.push_back(std::move(relation));

            ResolvedJoin resolved_join;
            resolved_join.source_join_index = index;
            resolved_join.left_relation_index = index;
            resolved_join.right_relation_index = index + 1;
            resolved_join.join_type = join->join_type;
            resolved_join.condition = join->on_condition;
            resolved_join.condition_text = expressionToString(join->on_condition, pool);
            resolved_join.natural =
                join->join_type == parser::JoinType::NATURAL ||
                join->join_type == parser::JoinType::NATURAL_LEFT ||
                join->join_type == parser::JoinType::NATURAL_RIGHT ||
                join->join_type == parser::JoinType::NATURAL_FULL;
            if (join->has_using)
            {
                for (auto column_id : join->using_columns)
                {
                    if (column_id != parser::v3::StringPool::INVALID_ID)
                    {
                        resolved_join.using_columns.push_back(std::string(pool.get(column_id)));
                    }
                }
            }

            switch (join->join_type)
            {
                case parser::JoinType::INNER:
                case parser::JoinType::CROSS:
                case parser::JoinType::NATURAL:
                    break;
                default:
                    out.contains_outer_join = true;
                    out.all_joins_inner = false;
                    break;
            }

            if (!resolved_join.using_columns.empty())
            {
                resolved_join.equi_join = true;
                if (out.relations.size() > 1)
                {
                    const auto &left_relation = out.relations[resolved_join.left_relation_index];
                    const auto &right_relation = out.relations[resolved_join.right_relation_index];
                    resolved_join.left_hash_qualifier = relationNameForLookup(left_relation);
                    resolved_join.right_hash_qualifier = relationNameForLookup(right_relation);
                    resolved_join.left_hash_column = resolved_join.using_columns.front();
                    resolved_join.right_hash_column = resolved_join.using_columns.front();

                    auto resolve_using_column_id =
                        [](const ResolvedRelation &relation,
                           const std::string &column_name) -> core::ID {
                            for (const auto &column : relation.columns)
                            {
                                if (core::IdentifierUtils::namesMatch(column.column_name,
                                                                      false,
                                                                      column_name,
                                                                      false))
                                {
                                    return column.column_id;
                                }
                            }
                            return core::ID{};
                        };

                    resolved_join.left_hash_column_id =
                        resolve_using_column_id(left_relation, resolved_join.using_columns.front());
                    resolved_join.right_hash_column_id =
                        resolve_using_column_id(right_relation, resolved_join.using_columns.front());
                    resolved_join.has_hash_column_ids =
                        !isZeroId(resolved_join.left_hash_column_id) &&
                        !isZeroId(resolved_join.right_hash_column_id);
                }
            }
            else
            {
                (void)extractJoinKeyFromEquality(join->on_condition, pool, out.relations, resolved_join);
            }

            out.joins.push_back(std::move(resolved_join));
        }

        if (!out.contains_outer_join)
        {
            ResolvedScanPredicate predicate;
            if (extractSimplePredicate(stmt->where, pool, out.relations, predicate) &&
                predicate.relation_index < out.relations.size())
            {
                out.relations[predicate.relation_index].local_predicate = predicate;
            }
        }

        DEBUG_LOG_DB("V3SemanticAnalyzer resolved " +
                     std::to_string(out.relations.size()) + " relation(s) and " +
                     std::to_string(out.joins.size()) + " join(s)");
        return core::Status::OK;
    }

} // namespace scratchbird::optimizer
