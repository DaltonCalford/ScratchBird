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
// P3-19: Common Subexpression Elimination (CSE)
//
// Identifies and eliminates redundant expression evaluations in queries.
// When the same expression appears multiple times, compute it once and
// reuse the result.
//
// Example:
//   SELECT x + y, (x + y) * 2, (x + y) + z
// Becomes:
//   LET t1 = x + y IN SELECT t1, t1 * 2, t1 + z
//
// Benefits:
// - Reduces CPU cycles for complex expressions
// - Particularly effective for function calls and arithmetic
// - Works in SELECT, WHERE, GROUP BY, ORDER BY, HAVING
//
// November 25, 2025

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include <set>
#include <functional>

namespace scratchbird::optimizer {

// Forward declarations
class Expression;
class ExpressionNode;

// ============================================================================
// Expression Types
// ============================================================================

enum class ExpressionType : uint8_t {
    LITERAL,          // Constant value
    COLUMN_REF,       // Column reference
    BINARY_OP,        // a + b, a * b, etc.
    UNARY_OP,         // -a, NOT a
    FUNCTION_CALL,    // func(args...)
    CASE_WHEN,        // CASE WHEN ... END
    CAST,             // CAST(expr AS type)
    SUBQUERY,         // Scalar subquery
    AGGREGATE,        // SUM, COUNT, etc.
    TEMP_REF,         // Reference to temporary (CSE result)
};

// Binary operators
enum class BinaryOperator : uint8_t {
    ADD, SUB, MUL, DIV, MOD,
    EQ, NE, LT, LE, GT, GE,
    AND, OR,
    LIKE, IN, BETWEEN,
    CONCAT,
};

// Unary operators
enum class UnaryOperator : uint8_t {
    NEGATE,
    NOT,
    IS_NULL,
    IS_NOT_NULL,
    ABS,
};

// ============================================================================
// Expression Node
// ============================================================================

class ExpressionNode {
public:
    ExpressionType type;
    std::string canonical_form;       // Normalized string for comparison
    uint64_t hash = 0;                // Hash for quick comparison
    uint32_t ref_count = 1;           // How many times this expr is used
    uint32_t temp_id = 0;             // Assigned temporary ID (if CSE applied)
    bool is_deterministic = true;     // Can be safely cached
    bool has_side_effects = false;    // Cannot eliminate

    // Type-specific data
    std::string string_value;         // For LITERAL, COLUMN_REF, FUNCTION_CALL
    BinaryOperator binary_op;         // For BINARY_OP
    UnaryOperator unary_op;           // For UNARY_OP

    // Child expressions
    std::vector<std::shared_ptr<ExpressionNode>> children;

    // Parent tracking for replacement
    ExpressionNode* parent = nullptr;
    size_t parent_child_index = 0;

    ExpressionNode() = default;
    explicit ExpressionNode(ExpressionType t) : type(t) {}

    // Create canonical form for comparison
    std::string computeCanonicalForm() const;

    // Compute hash
    uint64_t computeHash() const;

    // Deep clone
    std::shared_ptr<ExpressionNode> clone() const;

    // Check equality (structural)
    bool equals(const ExpressionNode& other) const;
};

// ============================================================================
// CSE Statistics
// ============================================================================

struct CSEStats {
    uint32_t expressions_analyzed = 0;
    uint32_t common_expressions_found = 0;
    uint32_t temporaries_created = 0;
    uint32_t evaluations_saved = 0;
    double estimated_speedup = 1.0;

    // Per-expression type counts
    std::unordered_map<std::string, uint32_t> expression_counts;
};

// ============================================================================
// Expression Hash Table Entry
// ============================================================================

struct CSEEntry {
    std::string canonical_form;
    uint64_t hash;
    std::vector<ExpressionNode*> occurrences;  // All occurrences
    uint32_t temp_id = 0;                       // Assigned temporary
    bool worth_eliminating = false;             // Based on cost analysis
    float estimated_cost = 0.0f;                // Cost to evaluate once
};

// ============================================================================
// CSE Optimizer
// ============================================================================

class CSEOptimizer {
public:
    CSEOptimizer() = default;

    // ========================================================================
    // Main Optimization Entry Points
    // ========================================================================

    // Optimize an expression tree (modifies in place)
    core::Status optimize(std::shared_ptr<ExpressionNode>& root, core::ErrorContext* ctx = nullptr);

    // Analyze without modifying (returns potential savings)
    CSEStats analyze(const std::shared_ptr<ExpressionNode>& root) const;

    // ========================================================================
    // Configuration
    // ========================================================================

    // Minimum occurrences to eliminate (default: 2)
    void setMinOccurrences(uint32_t min) { min_occurrences_ = min; }

    // Minimum cost to eliminate (skip cheap expressions)
    void setMinCost(float cost) { min_cost_threshold_ = cost; }

    // Include/exclude expression types
    void excludeType(ExpressionType type) { excluded_types_.insert(type); }
    void includeAllTypes() { excluded_types_.clear(); }

    // ========================================================================
    // Statistics
    // ========================================================================

    const CSEStats& lastStats() const { return stats_; }
    void resetStats() { stats_ = CSEStats{}; }

private:
    uint32_t min_occurrences_ = 2;
    float min_cost_threshold_ = 1.0f;
    std::set<ExpressionType> excluded_types_;
    mutable CSEStats stats_;
    uint32_t next_temp_id_ = 1;

    // Expression hash table
    std::unordered_map<uint64_t, std::vector<CSEEntry>> hash_table_;

    // Phase 1: Collect all expressions
    void collectExpressions(ExpressionNode* node,
                            std::vector<ExpressionNode*>& expressions);

    // Phase 2: Build hash table and find duplicates
    void findCommonSubexpressions(const std::vector<ExpressionNode*>& expressions);

    // Phase 3: Cost analysis - determine if elimination is worthwhile
    void analyzeCosts();

    // Phase 4: Apply transformations
    void applyEliminations(std::shared_ptr<ExpressionNode>& root);

    // Helper: estimate expression evaluation cost
    float estimateCost(const ExpressionNode* node) const;

    // Helper: check if expression is deterministic
    bool isDeterministic(const ExpressionNode* node) const;

    // Helper: create a temporary reference node
    std::shared_ptr<ExpressionNode> createTempRef(uint32_t temp_id);
};

// ============================================================================
// Expression Builder (for testing/construction)
// ============================================================================

class ExpressionBuilder {
public:
    // Literals
    static std::shared_ptr<ExpressionNode> literal(const std::string& value);
    static std::shared_ptr<ExpressionNode> literalInt(int64_t value);
    static std::shared_ptr<ExpressionNode> literalFloat(double value);

    // Column reference
    static std::shared_ptr<ExpressionNode> column(const std::string& name);
    static std::shared_ptr<ExpressionNode> column(const std::string& table,
                                                   const std::string& name);

    // Binary operations
    static std::shared_ptr<ExpressionNode> add(std::shared_ptr<ExpressionNode> left,
                                                std::shared_ptr<ExpressionNode> right);
    static std::shared_ptr<ExpressionNode> sub(std::shared_ptr<ExpressionNode> left,
                                                std::shared_ptr<ExpressionNode> right);
    static std::shared_ptr<ExpressionNode> mul(std::shared_ptr<ExpressionNode> left,
                                                std::shared_ptr<ExpressionNode> right);
    static std::shared_ptr<ExpressionNode> div(std::shared_ptr<ExpressionNode> left,
                                                std::shared_ptr<ExpressionNode> right);

    // Comparison
    static std::shared_ptr<ExpressionNode> eq(std::shared_ptr<ExpressionNode> left,
                                               std::shared_ptr<ExpressionNode> right);
    static std::shared_ptr<ExpressionNode> lt(std::shared_ptr<ExpressionNode> left,
                                               std::shared_ptr<ExpressionNode> right);
    static std::shared_ptr<ExpressionNode> gt(std::shared_ptr<ExpressionNode> left,
                                               std::shared_ptr<ExpressionNode> right);

    // Logical
    static std::shared_ptr<ExpressionNode> andOp(std::shared_ptr<ExpressionNode> left,
                                                  std::shared_ptr<ExpressionNode> right);
    static std::shared_ptr<ExpressionNode> orOp(std::shared_ptr<ExpressionNode> left,
                                                 std::shared_ptr<ExpressionNode> right);
    static std::shared_ptr<ExpressionNode> notOp(std::shared_ptr<ExpressionNode> expr);

    // Function call
    static std::shared_ptr<ExpressionNode> func(const std::string& name,
                                                 std::vector<std::shared_ptr<ExpressionNode>> args);

    // Cast
    static std::shared_ptr<ExpressionNode> cast(std::shared_ptr<ExpressionNode> expr,
                                                 const std::string& target_type);

private:
    static std::shared_ptr<ExpressionNode> binaryOp(
        BinaryOperator op,
        std::shared_ptr<ExpressionNode> left,
        std::shared_ptr<ExpressionNode> right);
};

// ============================================================================
// CSE Pass Integration
// ============================================================================

// Apply CSE to a query plan (wrapper for plan-level integration)
class CSEPass {
public:
    // Apply to SELECT expressions
    core::Status optimizeSelectList(std::vector<std::shared_ptr<ExpressionNode>>& expressions,
                                   core::ErrorContext* ctx = nullptr);

    // Apply to WHERE clause
    core::Status optimizeWhereClause(std::shared_ptr<ExpressionNode>& predicate,
                                    core::ErrorContext* ctx = nullptr);

    // Apply to entire query (SELECT, WHERE, GROUP BY, HAVING, ORDER BY)
    core::Status optimizeQuery(
        std::vector<std::shared_ptr<ExpressionNode>>& select_list,
        std::shared_ptr<ExpressionNode>& where_clause,
        std::vector<std::shared_ptr<ExpressionNode>>& group_by,
        std::shared_ptr<ExpressionNode>& having,
        std::vector<std::shared_ptr<ExpressionNode>>& order_by,
        core::ErrorContext* ctx = nullptr);

    // Get statistics from last optimization
    CSEStats getStats() const { return optimizer_.lastStats(); }

private:
    CSEOptimizer optimizer_;

    // Merge multiple expression trees for joint optimization
    std::shared_ptr<ExpressionNode> mergeForAnalysis(
        const std::vector<std::shared_ptr<ExpressionNode>>& expressions);
};

// ============================================================================
// Non-deterministic Function Registry
// ============================================================================

class NonDeterministicFunctions {
public:
    static NonDeterministicFunctions& getInstance();

    // Check if function is non-deterministic
    bool isNonDeterministic(std::string_view func_name) const;

    // Register a non-deterministic function
    void registerNonDeterministic(std::string_view func_name);

    // Register a function with side effects
    void registerWithSideEffects(std::string_view func_name);

    // Check if function has side effects
    bool hasSideEffects(std::string_view func_name) const;

private:
    NonDeterministicFunctions();

    std::set<std::string> non_deterministic_;
    std::set<std::string> with_side_effects_;
};

} // namespace scratchbird::optimizer
