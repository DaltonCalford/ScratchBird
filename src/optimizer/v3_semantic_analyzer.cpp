#include "scratchbird/optimizer/v3_semantic_analyzer.h"

#include "scratchbird/core/debug.h"
#include "scratchbird/parser/parser_v3.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
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

        auto unwrapSimpleExpression(const parser::v3::Expression *expr)
            -> const parser::v3::Expression *
        {
            const auto *current = expr;
            while (current != nullptr &&
                   current->kind() == parser::v3::ASTKind::CastExpr)
            {
                current = static_cast<const parser::v3::CastExpr *>(current)->expr;
            }
            return current;
        }

        auto predicateValueToText(const parser::v3::Expression *expr,
                                  const parser::v3::StringPool &pool,
                                  std::string &kind_out,
                                  std::string &text_out) -> bool
        {
            const auto *current = unwrapSimpleExpression(expr);
            if (current == nullptr)
            {
                return false;
            }

            if (current->kind() == parser::v3::ASTKind::ParameterExpr)
            {
                const auto *param = static_cast<const parser::v3::ParameterExpr *>(current);
                if (param->is_named)
                {
                    if (param->name == parser::v3::StringPool::INVALID_ID)
                    {
                        return false;
                    }
                    kind_out = "PARAMETER";
                    text_out = ":" + std::string(pool.get(param->name));
                    return true;
                }

                if (param->index == 0)
                {
                    return false;
                }
                kind_out = "PARAMETER";
                text_out = "$" + std::to_string(param->index);
                return true;
            }

            if (current->kind() != parser::v3::ASTKind::LiteralExpr)
            {
                return false;
            }

            const auto *literal = static_cast<const parser::v3::LiteralExpr *>(current);
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

        auto predicateMatchShape(ResolvedPredicateKind kind)
            -> PredicateMatchShape
        {
            switch (kind)
            {
                case ResolvedPredicateKind::EQUALITY:
                    return PredicateMatchShape::EQUALITY;
                case ResolvedPredicateKind::RANGE:
                    return PredicateMatchShape::RANGE;
                case ResolvedPredicateKind::LIKE_PREFIX:
                    return PredicateMatchShape::LIKE_PREFIX;
                case ResolvedPredicateKind::NONE:
                default:
                    return PredicateMatchShape::NONE;
            }
        }

        auto chooseIndexForColumn(const ResolvedRelation &relation,
                                  core::CatalogManager *catalog,
                                  StatisticsManager *stats_manager,
                                  const core::ID &column_id,
                                  ResolvedPredicateKind predicate_kind,
                                  const std::string &operator_name,
                                  IndexInfo &index_out,
                                  PlannerFamilyLoweringResult &lowering_out) -> bool
        {
            auto exactness_rank =
                [](AccessPathExactnessClass exactness_class) -> int {
                    switch (exactness_class)
                    {
                        case AccessPathExactnessClass::EXACT_ROW:
                            return 5;
                        case AccessPathExactnessClass::EXACT_KEY:
                            return 4;
                        case AccessPathExactnessClass::LOWER_BOUND_ORDERED:
                            return 3;
                        case AccessPathExactnessClass::CANDIDATE_REGION:
                            return 2;
                        case AccessPathExactnessClass::APPROX_TOPK:
                            return 1;
                        case AccessPathExactnessClass::UNKNOWN:
                        default:
                            return 0;
                    }
                };
            auto queryability_rank =
                [](AccessPathQueryabilityState queryability_state) -> int {
                    switch (queryability_state)
                    {
                        case AccessPathQueryabilityState::QUERYABLE:
                            return 3;
                        case AccessPathQueryabilityState::LIMITED:
                            return 2;
                        case AccessPathQueryabilityState::UNKNOWN:
                            return 1;
                        case AccessPathQueryabilityState::INVALID:
                        default:
                            return 0;
                    }
                };

            bool found = false;
            int best_score = std::numeric_limits<int>::min();
            double best_recheck_ratio = std::numeric_limits<double>::infinity();
            double best_coverage_fraction = -1.0;
            double best_correlation = -1.0;

            for (const auto &index : relation.indexes)
            {
                if (index.column_ids.empty())
                {
                    continue;
                }
                if (index.column_ids.front() == column_id)
                {
                    const PlannerFamilyLoweringRequest lowering_request =
                        buildPlannerFamilyLoweringRequest(catalog,
                                                          index,
                                                          predicateMatchShape(predicate_kind),
                                                          operator_name);
                    lowering_out = lowerPlannerFamily(lowering_request);
                    if (lowering_out.queryability_state ==
                        AccessPathQueryabilityState::INVALID)
                    {
                        continue;
                    }

                    IndexFamilyMetricsPacket metrics_packet;
                    const bool have_metrics =
                        stats_manager != nullptr &&
                        !isZeroId(index.index_id) &&
                        stats_manager->getIndexFamilyMetrics(index.index_id,
                                                             metrics_packet,
                                                             nullptr) ==
                            core::Status::OK;

                    int candidate_score =
                        (queryability_rank(lowering_out.queryability_state) * 100) +
                        (exactness_rank(lowering_out.exactness_class) * 20);
                    if (!lowering_out.requires_recheck)
                    {
                        candidate_score += 12;
                    }
                    if (lowering_out.supports_ordering &&
                        (predicate_kind == ResolvedPredicateKind::RANGE ||
                         predicate_kind == ResolvedPredicateKind::LIKE_PREFIX))
                    {
                        candidate_score += 6;
                    }
                    if (lowering_out.supports_covering)
                    {
                        candidate_score += 2;
                    }
                    if (have_metrics)
                    {
                        candidate_score +=
                            static_cast<int>(std::round(
                                std::clamp(metrics_packet.coverage_fraction,
                                           0.0,
                                           1.0) * 10.0));
                    }

                    const double candidate_recheck_ratio =
                        have_metrics ? metrics_packet.recheck_ratio_est
                                     : (lowering_out.requires_recheck ? 1.0 : 0.0);
                    const double candidate_coverage_fraction =
                        have_metrics ? metrics_packet.coverage_fraction : 0.0;
                    const double candidate_correlation =
                        have_metrics ? std::abs(metrics_packet.correlation) : 0.0;

                    const bool better_candidate =
                        !found ||
                        candidate_score > best_score ||
                        (candidate_score == best_score &&
                         candidate_recheck_ratio < best_recheck_ratio) ||
                        (candidate_score == best_score &&
                         candidate_recheck_ratio == best_recheck_ratio &&
                         candidate_coverage_fraction > best_coverage_fraction) ||
                        (candidate_score == best_score &&
                         candidate_recheck_ratio == best_recheck_ratio &&
                         candidate_coverage_fraction == best_coverage_fraction &&
                         candidate_correlation > best_correlation) ||
                        (candidate_score == best_score &&
                         candidate_recheck_ratio == best_recheck_ratio &&
                         candidate_coverage_fraction == best_coverage_fraction &&
                         candidate_correlation == best_correlation &&
                         index.index_name < index_out.index_name);
                    if (!better_candidate)
                    {
                        continue;
                    }

                    found = true;
                    best_score = candidate_score;
                    best_recheck_ratio = candidate_recheck_ratio;
                    best_coverage_fraction = candidate_coverage_fraction;
                    best_correlation = candidate_correlation;
                    index_out = index;
                }
            }
            return found;
        }

        auto extractSimplePredicate(const parser::v3::Expression *expr,
                                    core::CatalogManager *catalog,
                                    StatisticsManager *stats_manager,
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
                if (binary->op == parser::v3::BinaryOp::AND ||
                    binary->op == parser::v3::BinaryOp::OR)
                {
                    return false;
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
                                      predicateValueToText(binary->right,
                                                           pool,
                                                           literal_kind,
                                                           literal_text);
                if (!column_on_left)
                {
                    column_on_left = collectColumnToken(binary->right, pool, token) &&
                                     predicateValueToText(binary->left,
                                                          pool,
                                                          literal_kind,
                                                          literal_text);
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
                PlannerFamilyLoweringResult lowering;
                if (chooseIndexForColumn(relations[*relation_index],
                                         catalog,
                                         stats_manager,
                                         column_id,
                                         kind,
                                         predicate_out.operator_name,
                                         matched_index,
                                         lowering))
                {
                    predicate_out.has_index_match = true;
                    predicate_out.matched_index = matched_index;
                    predicate_out.matched_family = lowering.family;
                    predicate_out.matched_path_name = lowering.path_name;
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
                    !predicateValueToText(like_expr->pattern, pool, literal_kind, literal_text))
                {
                    return false;
                }

                if (literal_kind == "STRING")
                {
                    if (literal_text.empty() ||
                        literal_text.front() == '%' ||
                        literal_text.find('%') == std::string::npos)
                    {
                        return false;
                    }
                }
                else if (literal_kind != "PARAMETER")
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
                PlannerFamilyLoweringResult lowering;
                if (chooseIndexForColumn(relations[*relation_index],
                                         catalog,
                                         stats_manager,
                                         column_id,
                                         ResolvedPredicateKind::LIKE_PREFIX,
                                         predicate_out.operator_name,
                                         matched_index,
                                         lowering))
                {
                    predicate_out.has_index_match = true;
                    predicate_out.matched_index = matched_index;
                    predicate_out.matched_family = lowering.family;
                    predicate_out.matched_path_name = lowering.path_name;
                }
                return true;
            }

            return false;
        }

        auto collectSimplePredicates(const parser::v3::Expression *expr,
                                     core::CatalogManager *catalog,
                                     StatisticsManager *stats_manager,
                                     const parser::v3::StringPool &pool,
                                     std::vector<ResolvedRelation> &relations,
                                     std::vector<ResolvedScanPredicate> &predicates_out) -> bool
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
                    const bool left = collectSimplePredicates(binary->left,
                                                              catalog,
                                                              stats_manager,
                                                              pool,
                                                              relations,
                                                              predicates_out);
                    const bool right = collectSimplePredicates(binary->right,
                                                               catalog,
                                                               stats_manager,
                                                               pool,
                                                               relations,
                                                               predicates_out);
                    return left || right;
                }
            }

            ResolvedScanPredicate predicate;
            if (!extractSimplePredicate(expr,
                                        catalog,
                                        stats_manager,
                                        pool,
                                        relations,
                                        predicate))
            {
                return false;
            }
            predicates_out.push_back(std::move(predicate));
            return true;
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
            relation_out.lateral = table_ref != nullptr && table_ref->lateral;

            if (table_ref == nullptr)
            {
                return core::Status::OK;
            }

            if (table_ref->ref_type != parser::v3::TableRefNode::Type::TABLE)
            {
                if (table_ref->ref_type == parser::v3::TableRefNode::Type::SUBQUERY &&
                    table_ref->subquery != nullptr &&
                    table_ref->subquery->kind() == parser::v3::ASTKind::SelectStmt)
                {
                    const auto *subquery =
                        static_cast<const parser::v3::SelectStmt *>(table_ref->subquery);
                    const bool pass_through =
                        subquery->with == nullptr &&
                        !subquery->distinct &&
                        !subquery->all &&
                        subquery->joins.empty() &&
                        subquery->where == nullptr &&
                        subquery->group_by.empty() &&
                        subquery->having == nullptr &&
                        subquery->windows.empty() &&
                        subquery->order_by.empty() &&
                        subquery->limit == nullptr &&
                        subquery->offset == nullptr &&
                        subquery->set_op == parser::v3::SetOpType::NONE &&
                        subquery->from != nullptr &&
                        subquery->items.size() == 1 &&
                        (subquery->items.front()->item_type == parser::v3::SelectItem::Type::STAR ||
                         subquery->items.front()->item_type ==
                             parser::v3::SelectItem::Type::TABLE_STAR);
                    if (pass_through)
                    {
                        ResolvedRelation flattened;
                        auto status = resolveTableLikeRelation(catalog,
                                                               stats_manager,
                                                               subquery->from,
                                                               pool,
                                                               conn_ctx,
                                                               current_schema_id,
                                                               relation_index,
                                                               flattened);
                        if (status == core::Status::OK && flattened.resolved)
                        {
                            relation_out = std::move(flattened);
                            relation_out.table_ref = table_ref;
                            relation_out.derived = false;
                            relation_out.flattened_derived = true;
                            relation_out.table_path =
                                relation_out.alias.empty() ? "<derived>" : relation_out.alias;
                            relation_out.physical_table_path =
                                relation_out.physical_table_path.empty()
                                    ? relation_out.table_path
                                    : relation_out.physical_table_path;
                            return core::Status::OK;
                        }
                    }
                }

                relation_out.derived = true;
                relation_out.table_path = relation_out.alias.empty() ? "<derived>" : relation_out.alias;
                relation_out.physical_table_path = relation_out.table_path;
                relation_out.estimated_rows = 1000;
                relation_out.estimated_pages = 10;
                return core::Status::OK;
            }

            relation_out.table_path = renderSchemaPath(table_ref->table_path, pool);
            relation_out.physical_table_path = relation_out.table_path;
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
            if (!resolved && catalog != nullptr)
            {
                core::CatalogManager::ViewInfo view_info;
                for (const auto &schema_id : schema_candidates)
                {
                    if (catalog->getView(schema_id, table_name, view_info, nullptr) ==
                        core::Status::OK)
                    {
                        resolved = true;
                        relation_out.flattened_derived = true;
                        relation_out.derived = false;
                        relation_out.table_path = renderSchemaPath(table_ref->table_path, pool);
                        relation_out.alias = relation_out.alias.empty()
                            ? table_name
                            : relation_out.alias;

                        if (view_info.materialized &&
                            view_info.materialized_table_id != core::ID{} &&
                            catalog->getTable(view_info.materialized_table_id,
                                              relation_out.table_info,
                                              nullptr) == core::Status::OK)
                        {
                            relation_out.resolved = true;
                            relation_out.physical_table_path = relation_out.table_info.table_name;
                            (void)catalog->getColumns(relation_out.table_info.table_id,
                                                      relation_out.columns,
                                                      nullptr);
                            (void)catalog->listIndexesForTable(relation_out.table_info.table_id,
                                                               relation_out.indexes,
                                                               nullptr);
                            break;
                        }

                        parser::v3::Parser parser(view_info.definition);
                        auto parse_result = parser.parseStatement();
                        if (!parse_result.success() ||
                            parse_result.statement() == nullptr ||
                            parse_result.statement()->kind() != parser::v3::ASTKind::SelectStmt)
                        {
                            resolved = false;
                            break;
                        }

                        const auto *view_select =
                            static_cast<const parser::v3::SelectStmt *>(parse_result.statement());
                        const bool pass_through =
                            view_select->with == nullptr &&
                            !view_select->distinct &&
                            !view_select->all &&
                            view_select->joins.empty() &&
                            view_select->where == nullptr &&
                            view_select->group_by.empty() &&
                            view_select->having == nullptr &&
                            view_select->windows.empty() &&
                            view_select->order_by.empty() &&
                            view_select->limit == nullptr &&
                            view_select->offset == nullptr &&
                            view_select->set_op == parser::v3::SetOpType::NONE &&
                            view_select->from != nullptr &&
                            view_select->items.size() == 1 &&
                            (view_select->items.front()->item_type ==
                                 parser::v3::SelectItem::Type::STAR ||
                             view_select->items.front()->item_type ==
                                 parser::v3::SelectItem::Type::TABLE_STAR);
                        if (!pass_through)
                        {
                            resolved = false;
                            break;
                        }

                        ResolvedRelation flattened;
                        if (resolveTableLikeRelation(catalog,
                                                     stats_manager,
                                                     view_select->from,
                                                     parser.stringPool(),
                                                     conn_ctx,
                                                     current_schema_id,
                                                     relation_index,
                                                     flattened) != core::Status::OK ||
                            !flattened.resolved)
                        {
                            resolved = false;
                            break;
                        }

                        relation_out = std::move(flattened);
                        relation_out.source_relation_index = relation_index;
                        relation_out.table_ref = table_ref;
                        relation_out.alias = defaultAliasForTable(table_ref, pool);
                        relation_out.table_path = renderSchemaPath(table_ref->table_path, pool);
                        relation_out.physical_table_path =
                            relation_out.physical_table_path.empty()
                                ? relation_out.table_path
                                : relation_out.physical_table_path;
                        relation_out.flattened_derived = true;
                        relation_out.derived = false;
                        relation_out.resolved = true;
                        break;
                    }
                }
            }

            if (resolved)
            {
                if (isZeroId(relation_out.table_info.table_id))
                {
                    relation_out.table_info = table_info;
                }
                (void)catalog->getColumns(relation_out.table_info.table_id,
                                          relation_out.columns,
                                          nullptr);
                (void)catalog->listIndexesForTable(relation_out.table_info.table_id,
                                                   relation_out.indexes,
                                                   nullptr);

                if (stats_manager != nullptr)
                {
                    TableStatistics stats;
                    if (stats_manager->getTableStatistics(relation_out.table_info.table_id,
                                                          stats,
                                                          nullptr) ==
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
        auto *catalog = db_ != nullptr ? db_->catalog_manager() : nullptr;

        if (stmt == nullptr)
        {
            return core::Status::INVALID_ARGUMENT;
        }

        if (stmt->from != nullptr)
        {
            ResolvedRelation base_relation;
            resolveTableLikeRelation(catalog,
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
            resolveTableLikeRelation(catalog,
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

            const auto legality = classifyJoinLegality(
                resolved_join.join_type,
                resolved_join.natural,
                !resolved_join.using_columns.empty());
            resolved_join.legality_class = legality.legality_class;
            resolved_join.reorderable = legality.reorderable;
            resolved_join.preserves_left_rows = legality.preserves_left_rows;
            resolved_join.preserves_right_rows = legality.preserves_right_rows;
            resolved_join.null_introduces_left = legality.null_introduces_left;
            resolved_join.null_introduces_right = legality.null_introduces_right;
            resolved_join.requires_original_order = legality.requires_original_order;

            switch (join->join_type)
            {
                case parser::JoinType::INNER:
                case parser::JoinType::CROSS:
                    break;
                default:
                    out.all_joins_inner = false;
                    break;
            }
            switch (join->join_type)
            {
                case parser::JoinType::LEFT:
                case parser::JoinType::RIGHT:
                case parser::JoinType::FULL:
                case parser::JoinType::NATURAL_LEFT:
                case parser::JoinType::NATURAL_RIGHT:
                case parser::JoinType::NATURAL_FULL:
                    out.contains_outer_join = true;
                    break;
                default:
                    break;
            }
            if (!resolved_join.reorderable)
            {
                out.has_join_reorder_barrier = true;
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
            std::vector<ResolvedScanPredicate> predicates;
            if (collectSimplePredicates(stmt->where,
                                        catalog,
                                        stats_manager_,
                                        pool,
                                        out.relations,
                                        predicates))
            {
                for (const auto &predicate : predicates)
                {
                    if (predicate.relation_index >= out.relations.size())
                    {
                        continue;
                    }
                    auto &relation = out.relations[predicate.relation_index];
                    relation.local_predicates.push_back(predicate);
                    if (!relation.local_predicate.has_value())
                    {
                        relation.local_predicate = predicate;
                    }
                }
            }
        }

        DEBUG_LOG_DB("V3SemanticAnalyzer resolved " +
                     std::to_string(out.relations.size()) + " relation(s) and " +
                     std::to_string(out.joins.size()) + " join(s)");
        return core::Status::OK;
    }

} // namespace scratchbird::optimizer
