/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-19: Common Subexpression Elimination Implementation
//
// November 25, 2025

#include "scratchbird/optimizer/cse.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace scratchbird::optimizer {

// ============================================================================
// ExpressionNode Implementation
// ============================================================================

std::string ExpressionNode::computeCanonicalForm() const {
    std::stringstream ss;

    switch (type) {
        case ExpressionType::LITERAL:
            ss << "LIT:" << string_value;
            break;

        case ExpressionType::COLUMN_REF:
            ss << "COL:" << string_value;
            break;

        case ExpressionType::BINARY_OP: {
            ss << "(";
            if (!children.empty() && children[0]) {
                ss << children[0]->computeCanonicalForm();
            }
            ss << " OP" << static_cast<int>(binary_op) << " ";
            if (children.size() > 1 && children[1]) {
                ss << children[1]->computeCanonicalForm();
            }
            ss << ")";
            break;
        }

        case ExpressionType::UNARY_OP: {
            ss << "UOP" << static_cast<int>(unary_op) << "(";
            if (!children.empty() && children[0]) {
                ss << children[0]->computeCanonicalForm();
            }
            ss << ")";
            break;
        }

        case ExpressionType::FUNCTION_CALL: {
            ss << "FN:" << string_value << "(";
            for (size_t i = 0; i < children.size(); ++i) {
                if (i > 0) ss << ",";
                if (children[i]) {
                    ss << children[i]->computeCanonicalForm();
                }
            }
            ss << ")";
            break;
        }

        case ExpressionType::CAST: {
            ss << "CAST(";
            if (!children.empty() && children[0]) {
                ss << children[0]->computeCanonicalForm();
            }
            ss << " AS " << string_value << ")";
            break;
        }

        case ExpressionType::TEMP_REF:
            ss << "TEMP:" << temp_id;
            break;

        default:
            ss << "EXPR:" << static_cast<int>(type);
            break;
    }

    return ss.str();
}

uint64_t ExpressionNode::computeHash() const {
    std::hash<std::string> hasher;
    return hasher(computeCanonicalForm());
}

std::shared_ptr<ExpressionNode> ExpressionNode::clone() const {
    auto copy = std::make_shared<ExpressionNode>();
    copy->type = type;
    copy->canonical_form = canonical_form;
    copy->hash = hash;
    copy->ref_count = 1;
    copy->temp_id = temp_id;
    copy->is_deterministic = is_deterministic;
    copy->has_side_effects = has_side_effects;
    copy->string_value = string_value;
    copy->binary_op = binary_op;
    copy->unary_op = unary_op;

    for (const auto& child : children) {
        if (child) {
            copy->children.push_back(child->clone());
        }
    }

    return copy;
}

bool ExpressionNode::equals(const ExpressionNode& other) const {
    if (type != other.type) return false;

    // Compare based on canonical form
    std::string this_form = computeCanonicalForm();
    std::string other_form = other.computeCanonicalForm();

    return this_form == other_form;
}

// ============================================================================
// CSEOptimizer Implementation
// ============================================================================

core::Status CSEOptimizer::optimize(std::shared_ptr<ExpressionNode>& root,
                                    core::ErrorContext* ctx) {
    if (!root) return core::Status::OK;

    stats_ = CSEStats{};
    hash_table_.clear();
    next_temp_id_ = 1;

    // Phase 1: Collect all expressions
    std::vector<ExpressionNode*> expressions;
    collectExpressions(root.get(), expressions);

    stats_.expressions_analyzed = expressions.size();

    // Phase 2: Find common subexpressions
    findCommonSubexpressions(expressions);

    // Phase 3: Analyze costs
    analyzeCosts();

    // Phase 4: Apply eliminations
    applyEliminations(root);

    return core::Status::OK;
}

CSEStats CSEOptimizer::analyze(const std::shared_ptr<ExpressionNode>& root) const {
    CSEStats stats;
    if (!root) return stats;

    // Collect expressions
    std::vector<ExpressionNode*> expressions;
    std::function<void(ExpressionNode*)> collect = [&](ExpressionNode* node) {
        if (!node) return;
        expressions.push_back(node);
        for (auto& child : node->children) {
            collect(child.get());
        }
    };
    collect(root.get());

    stats.expressions_analyzed = expressions.size();

    // Find duplicates
    std::unordered_map<std::string, uint32_t> counts;
    for (auto* expr : expressions) {
        std::string form = expr->computeCanonicalForm();
        counts[form]++;
    }

    for (const auto& [form, count] : counts) {
        if (count >= 2) {
            stats.common_expressions_found++;
            stats.evaluations_saved += count - 1;
        }
        stats.expression_counts[form] = count;
    }

    if (stats.expressions_analyzed > 0) {
        stats.estimated_speedup = 1.0 + (stats.evaluations_saved * 0.1);
    }

    return stats;
}

void CSEOptimizer::collectExpressions(ExpressionNode* node,
                                      std::vector<ExpressionNode*>& expressions) {
    if (!node) return;

    // Skip excluded types
    if (excluded_types_.find(node->type) != excluded_types_.end()) {
        // Still recurse into children
        for (auto& child : node->children) {
            collectExpressions(child.get(), expressions);
        }
        return;
    }

    // Skip non-deterministic or side-effect expressions
    if (!isDeterministic(node)) {
        node->is_deterministic = false;
        for (auto& child : node->children) {
            collectExpressions(child.get(), expressions);
        }
        return;
    }

    // Add this expression
    node->canonical_form = node->computeCanonicalForm();
    node->hash = node->computeHash();
    expressions.push_back(node);

    // Track parent-child relationships for replacement
    for (size_t i = 0; i < node->children.size(); ++i) {
        if (node->children[i]) {
            node->children[i]->parent = node;
            node->children[i]->parent_child_index = i;
            collectExpressions(node->children[i].get(), expressions);
        }
    }
}

void CSEOptimizer::findCommonSubexpressions(const std::vector<ExpressionNode*>& expressions) {
    hash_table_.clear();

    for (auto* expr : expressions) {
        // Skip trivial expressions (literals, column refs)
        if (expr->type == ExpressionType::LITERAL ||
            expr->type == ExpressionType::COLUMN_REF) {
            continue;
        }

        // Look for existing entry with same hash
        auto& bucket = hash_table_[expr->hash];

        bool found = false;
        for (auto& entry : bucket) {
            if (entry.canonical_form == expr->canonical_form) {
                entry.occurrences.push_back(expr);
                found = true;
                break;
            }
        }

        if (!found) {
            CSEEntry entry;
            entry.canonical_form = expr->canonical_form;
            entry.hash = expr->hash;
            entry.occurrences.push_back(expr);
            entry.estimated_cost = estimateCost(expr);
            bucket.push_back(std::move(entry));
        }
    }
}

void CSEOptimizer::analyzeCosts() {
    for (auto& [hash, bucket] : hash_table_) {
        for (auto& entry : bucket) {
            // Need at least min_occurrences to be worth eliminating
            if (entry.occurrences.size() >= min_occurrences_ &&
                entry.estimated_cost >= min_cost_threshold_) {

                entry.worth_eliminating = true;
                entry.temp_id = next_temp_id_++;

                stats_.common_expressions_found++;
                stats_.temporaries_created++;
                stats_.evaluations_saved += entry.occurrences.size() - 1;
            }
        }
    }

    if (stats_.expressions_analyzed > 0) {
        stats_.estimated_speedup =
            1.0 + (static_cast<double>(stats_.evaluations_saved) / stats_.expressions_analyzed);
    }
}

void CSEOptimizer::applyEliminations(std::shared_ptr<ExpressionNode>& root) {
    // For each CSE entry that's worth eliminating,
    // replace all but the first occurrence with a temp reference

    for (auto& [hash, bucket] : hash_table_) {
        for (auto& entry : bucket) {
            if (!entry.worth_eliminating) continue;

            // Assign temp_id to all occurrences
            for (auto* expr : entry.occurrences) {
                expr->temp_id = entry.temp_id;
            }

            // Replace occurrences (except first) with temp references
            // Skip first occurrence - it will be computed and stored
            for (size_t i = 1; i < entry.occurrences.size(); ++i) {
                auto* expr = entry.occurrences[i];
                if (expr->parent) {
                    auto temp_ref = createTempRef(entry.temp_id);
                    expr->parent->children[expr->parent_child_index] = temp_ref;
                }
            }
        }
    }
}

float CSEOptimizer::estimateCost(const ExpressionNode* node) const {
    // Base cost estimation
    float cost = 0.0f;

    switch (node->type) {
        case ExpressionType::LITERAL:
        case ExpressionType::COLUMN_REF:
        case ExpressionType::TEMP_REF:
            cost = 0.1f;  // Very cheap
            break;

        case ExpressionType::BINARY_OP:
            cost = 1.0f;
            // Multiply/divide slightly more expensive
            if (node->binary_op == BinaryOperator::MUL ||
                node->binary_op == BinaryOperator::DIV ||
                node->binary_op == BinaryOperator::MOD) {
                cost = 1.5f;
            }
            break;

        case ExpressionType::UNARY_OP:
            cost = 0.5f;
            break;

        case ExpressionType::FUNCTION_CALL:
            cost = 5.0f;  // Function calls are expensive
            // Some functions are more expensive
            if (node->string_value == "REGEXP_MATCH" ||
                node->string_value == "LIKE") {
                cost = 10.0f;
            }
            break;

        case ExpressionType::CAST:
            cost = 1.0f;
            break;

        case ExpressionType::CASE_WHEN:
            cost = 2.0f;
            break;

        case ExpressionType::SUBQUERY:
            cost = 100.0f;  // Subqueries are very expensive
            break;

        case ExpressionType::AGGREGATE:
            cost = 0.0f;  // Aggregates can't be CSE'd normally
            break;

        default:
            cost = 1.0f;
    }

    // Add child costs
    for (const auto& child : node->children) {
        if (child) {
            cost += estimateCost(child.get()) * 0.5f;  // Discount nested
        }
    }

    return cost;
}

bool CSEOptimizer::isDeterministic(const ExpressionNode* node) const {
    // Check if this specific node is deterministic
    if (node->type == ExpressionType::FUNCTION_CALL) {
        if (NonDeterministicFunctions::getInstance().isNonDeterministic(node->string_value)) {
            return false;
        }
        if (NonDeterministicFunctions::getInstance().hasSideEffects(node->string_value)) {
            return false;
        }
    }

    // Check children
    for (const auto& child : node->children) {
        if (child && !isDeterministic(child.get())) {
            return false;
        }
    }

    return true;
}

std::shared_ptr<ExpressionNode> CSEOptimizer::createTempRef(uint32_t temp_id) {
    auto node = std::make_shared<ExpressionNode>(ExpressionType::TEMP_REF);
    node->temp_id = temp_id;
    node->string_value = "t" + std::to_string(temp_id);
    node->is_deterministic = true;
    return node;
}

// ============================================================================
// ExpressionBuilder Implementation
// ============================================================================

std::shared_ptr<ExpressionNode> ExpressionBuilder::literal(const std::string& value) {
    auto node = std::make_shared<ExpressionNode>(ExpressionType::LITERAL);
    node->string_value = value;
    return node;
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::literalInt(int64_t value) {
    return literal(std::to_string(value));
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::literalFloat(double value) {
    std::ostringstream ss;
    ss << std::setprecision(17) << value;
    return literal(ss.str());
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::column(const std::string& name) {
    auto node = std::make_shared<ExpressionNode>(ExpressionType::COLUMN_REF);
    node->string_value = name;
    return node;
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::column(const std::string& table,
                                                          const std::string& name) {
    auto node = std::make_shared<ExpressionNode>(ExpressionType::COLUMN_REF);
    node->string_value = table + "." + name;
    return node;
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::binaryOp(
    BinaryOperator op,
    std::shared_ptr<ExpressionNode> left,
    std::shared_ptr<ExpressionNode> right) {

    auto node = std::make_shared<ExpressionNode>(ExpressionType::BINARY_OP);
    node->binary_op = op;
    node->children.push_back(std::move(left));
    node->children.push_back(std::move(right));
    return node;
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::add(
    std::shared_ptr<ExpressionNode> left,
    std::shared_ptr<ExpressionNode> right) {
    return binaryOp(BinaryOperator::ADD, std::move(left), std::move(right));
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::sub(
    std::shared_ptr<ExpressionNode> left,
    std::shared_ptr<ExpressionNode> right) {
    return binaryOp(BinaryOperator::SUB, std::move(left), std::move(right));
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::mul(
    std::shared_ptr<ExpressionNode> left,
    std::shared_ptr<ExpressionNode> right) {
    return binaryOp(BinaryOperator::MUL, std::move(left), std::move(right));
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::div(
    std::shared_ptr<ExpressionNode> left,
    std::shared_ptr<ExpressionNode> right) {
    return binaryOp(BinaryOperator::DIV, std::move(left), std::move(right));
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::eq(
    std::shared_ptr<ExpressionNode> left,
    std::shared_ptr<ExpressionNode> right) {
    return binaryOp(BinaryOperator::EQ, std::move(left), std::move(right));
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::lt(
    std::shared_ptr<ExpressionNode> left,
    std::shared_ptr<ExpressionNode> right) {
    return binaryOp(BinaryOperator::LT, std::move(left), std::move(right));
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::gt(
    std::shared_ptr<ExpressionNode> left,
    std::shared_ptr<ExpressionNode> right) {
    return binaryOp(BinaryOperator::GT, std::move(left), std::move(right));
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::andOp(
    std::shared_ptr<ExpressionNode> left,
    std::shared_ptr<ExpressionNode> right) {
    return binaryOp(BinaryOperator::AND, std::move(left), std::move(right));
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::orOp(
    std::shared_ptr<ExpressionNode> left,
    std::shared_ptr<ExpressionNode> right) {
    return binaryOp(BinaryOperator::OR, std::move(left), std::move(right));
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::notOp(
    std::shared_ptr<ExpressionNode> expr) {
    auto node = std::make_shared<ExpressionNode>(ExpressionType::UNARY_OP);
    node->unary_op = UnaryOperator::NOT;
    node->children.push_back(std::move(expr));
    return node;
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::func(
    const std::string& name,
    std::vector<std::shared_ptr<ExpressionNode>> args) {

    auto node = std::make_shared<ExpressionNode>(ExpressionType::FUNCTION_CALL);
    node->string_value = name;
    node->children = std::move(args);

    // Mark determinism based on function name
    node->is_deterministic =
        !NonDeterministicFunctions::getInstance().isNonDeterministic(name);

    return node;
}

std::shared_ptr<ExpressionNode> ExpressionBuilder::cast(
    std::shared_ptr<ExpressionNode> expr,
    const std::string& target_type) {

    auto node = std::make_shared<ExpressionNode>(ExpressionType::CAST);
    node->string_value = target_type;
    node->children.push_back(std::move(expr));
    return node;
}

// ============================================================================
// CSEPass Implementation
// ============================================================================

core::Status CSEPass::optimizeSelectList(
    std::vector<std::shared_ptr<ExpressionNode>>& expressions,
    core::ErrorContext* ctx) {

    for (auto& expr : expressions) {
        auto status = optimizer_.optimize(expr, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }
    return core::Status::OK;
}

core::Status CSEPass::optimizeWhereClause(
    std::shared_ptr<ExpressionNode>& predicate,
    core::ErrorContext* ctx) {

    return optimizer_.optimize(predicate, ctx);
}

core::Status CSEPass::optimizeQuery(
    std::vector<std::shared_ptr<ExpressionNode>>& select_list,
    std::shared_ptr<ExpressionNode>& where_clause,
    std::vector<std::shared_ptr<ExpressionNode>>& group_by,
    std::shared_ptr<ExpressionNode>& having,
    std::vector<std::shared_ptr<ExpressionNode>>& order_by,
    core::ErrorContext* ctx) {

    // Combine all expressions for joint analysis
    std::vector<std::shared_ptr<ExpressionNode>> all_exprs;
    all_exprs.insert(all_exprs.end(), select_list.begin(), select_list.end());
    if (where_clause) all_exprs.push_back(where_clause);
    all_exprs.insert(all_exprs.end(), group_by.begin(), group_by.end());
    if (having) all_exprs.push_back(having);
    all_exprs.insert(all_exprs.end(), order_by.begin(), order_by.end());

    // Merge into single tree for analysis
    auto merged = mergeForAnalysis(all_exprs);

    // Optimize
    auto status = optimizer_.optimize(merged, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    // Results are modified in place through shared pointers
    return core::Status::OK;
}

std::shared_ptr<ExpressionNode> CSEPass::mergeForAnalysis(
    const std::vector<std::shared_ptr<ExpressionNode>>& expressions) {

    // Create a pseudo-root that contains all expressions as children
    auto root = std::make_shared<ExpressionNode>(ExpressionType::FUNCTION_CALL);
    root->string_value = "__CSE_ROOT__";
    root->is_deterministic = true;

    for (const auto& expr : expressions) {
        if (expr) {
            root->children.push_back(expr);
        }
    }

    return root;
}

// ============================================================================
// NonDeterministicFunctions Implementation
// ============================================================================

NonDeterministicFunctions::NonDeterministicFunctions() {
    // Register common non-deterministic functions
    non_deterministic_ = {
        "RANDOM", "RAND",
        "NOW", "CURRENT_TIMESTAMP", "CURRENT_TIME", "CURRENT_DATE",
        "CLOCK_TIMESTAMP", "STATEMENT_TIMESTAMP", "TRANSACTION_TIMESTAMP",
        "LOCALTIME", "LOCALTIMESTAMP",
        "NEXTVAL", "CURRVAL",  // Sequence functions
        "UUID", "GEN_RANDOM_UUID",
        "SETSEED",
    };

    // Functions with side effects (should never be eliminated)
    with_side_effects_ = {
        "NEXTVAL",      // Modifies sequence
        "SETVAL",       // Modifies sequence
        "NOTIFY",       // Sends notification
        "PG_NOTIFY",    // Sends notification
        "RAISE",        // Raises exception
    };
}

NonDeterministicFunctions& NonDeterministicFunctions::getInstance() {
    static NonDeterministicFunctions instance;
    return instance;
}

bool NonDeterministicFunctions::isNonDeterministic(std::string_view func_name) const {
    std::string upper(func_name);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return non_deterministic_.find(upper) != non_deterministic_.end();
}

void NonDeterministicFunctions::registerNonDeterministic(std::string_view func_name) {
    std::string upper(func_name);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    non_deterministic_.insert(upper);
}

void NonDeterministicFunctions::registerWithSideEffects(std::string_view func_name) {
    std::string upper(func_name);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    with_side_effects_.insert(upper);
    non_deterministic_.insert(upper);  // Side effects imply non-deterministic
}

bool NonDeterministicFunctions::hasSideEffects(std::string_view func_name) const {
    std::string upper(func_name);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return with_side_effects_.find(upper) != with_side_effects_.end();
}

} // namespace scratchbird::optimizer
