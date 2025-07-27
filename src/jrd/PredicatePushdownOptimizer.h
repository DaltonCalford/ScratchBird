/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		PredicatePushdownOptimizer.h
 *	DESCRIPTION:	Predicate pushdown optimization for bitmap table scans
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * 2025.07.23 - ScratchBird Predicate Pushdown Optimization
 */

#ifndef JRD_PREDICATE_PUSHDOWN_OPTIMIZER_H
#define JRD_PREDICATE_PUSHDOWN_OPTIMIZER_H

#include "../jrd/constants.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "BitmapIndex.h"
#include "../jrd/optimizer/opt_proto.h"
#include <vector>
#include <memory>
#include <map>
#include <set>

namespace Jrd {

// Forward declarations
class thread_db;
class MemoryPool;
class CompilerScratch;
class jrd_rel;
struct index_desc;
class BoolExprNode;
class ValueExprNode;
class CompareNode;
class ArithmeticNode;
class RecordSource;
class CompressedBitmap;

//----------------------------
// Predicate Types for Pushdown
//----------------------------

enum PredicateType : UCHAR
{
    PREDICATE_COMPARISON = 0,           // =, <>, <, <=, >, >=
    PREDICATE_RANGE = 1,                // BETWEEN, IN range
    PREDICATE_IN_LIST = 2,              // IN (val1, val2, ...)
    PREDICATE_LIKE = 3,                 // LIKE pattern matching
    PREDICATE_NULL_CHECK = 4,           // IS NULL, IS NOT NULL
    PREDICATE_EXISTENCE = 5,            // EXISTS subquery
    PREDICATE_ARITHMETIC = 6,           // Mathematical expressions
    PREDICATE_LOGICAL = 7,              // AND, OR, NOT combinations
    PREDICATE_BITMAP_OPERATION = 8,     // Bitmap-specific operations
    PREDICATE_FUNCTION_CALL = 9         // Function-based predicates
};

//----------------------------
// Pushdown Applicability
//----------------------------

enum PushdownApplicability : UCHAR
{
    PUSHDOWN_FULLY_APPLICABLE = 0,      // Can push entire predicate
    PUSHDOWN_PARTIALLY_APPLICABLE = 1,  // Can push part of predicate
    PUSHDOWN_NOT_APPLICABLE = 2,        // Cannot push predicate
    PUSHDOWN_BENEFICIAL = 3,            // Pushdown provides significant benefit
    PUSHDOWN_NEUTRAL = 4,               // Pushdown has no significant impact
    PUSHDOWN_DETRIMENTAL = 5            // Pushdown would hurt performance
};

//----------------------------
// Predicate Analysis Information
//----------------------------

struct PredicateAnalysis
{
    BoolExprNode* predicate_node;       // Original predicate expression
    PredicateType predicate_type;       // Classified predicate type
    jrd_rel* target_relation;           // Table this predicate applies to
    USHORT field_id;                    // Field involved in predicate
    const index_desc* applicable_index; // Bitmap index that can handle predicate
    
    PushdownApplicability applicability; // Whether pushdown is applicable
    double selectivity_estimate;        // Estimated selectivity (0.0 - 1.0)
    ULONG estimated_result_size;        // Estimated result cardinality
    double pushdown_benefit_score;      // Benefit score for pushing down
    
    // Complexity analysis
    ULONG computation_cost;             // Cost of evaluating predicate
    bool is_deterministic;              // True if predicate is deterministic
    bool has_side_effects;              // True if predicate has side effects
    bool requires_full_record;          // True if needs complete record data
    
    // Dependencies
    std::vector<USHORT> dependent_field_ids; // Fields this predicate depends on
    std::vector<jrd_rel*> dependent_relations; // Relations this predicate depends on
    bool has_subquery;                  // True if contains subquery
    bool has_user_function;             // True if contains user-defined function
    
    PredicateAnalysis()
        : predicate_node(nullptr), predicate_type(PREDICATE_COMPARISON),
          target_relation(nullptr), field_id(0), applicable_index(nullptr),
          applicability(PUSHDOWN_NOT_APPLICABLE), selectivity_estimate(1.0),
          estimated_result_size(0), pushdown_benefit_score(0.0),
          computation_cost(0), is_deterministic(true), has_side_effects(false),
          requires_full_record(false), has_subquery(false), has_user_function(false)
    {
    }
};

//----------------------------
// Pushdown Strategy
//----------------------------

enum PushdownStrategy : UCHAR
{
    PUSHDOWN_STRATEGY_EAGER = 0,        // Push down as many predicates as possible
    PUSHDOWN_STRATEGY_SELECTIVE = 1,    // Push down only high-benefit predicates
    PUSHDOWN_STRATEGY_CONSERVATIVE = 2, // Push down only safe predicates
    PUSHDOWN_STRATEGY_ADAPTIVE = 3,     // Adapt strategy based on query characteristics
    PUSHDOWN_STRATEGY_COST_BASED = 4    // Use cost-based decision making
};

//----------------------------
// Bitmap Predicate Implementation
//----------------------------

/**
 * Represents a predicate that can be evaluated using bitmap operations
 */
class BitmapPredicate
{
public:
    explicit BitmapPredicate(MemoryPool* pool, const PredicateAnalysis& analysis);
    virtual ~BitmapPredicate();

    // Predicate evaluation
    virtual CompressedBitmap* evaluatePredicate(thread_db* tdbb, CompilerScratch* csb) = 0;
    virtual bool canEvaluateWithBitmap() const = 0;
    virtual double getSelectivityEstimate() const;
    
    // Predicate properties
    PredicateType getPredicateType() const { return m_analysis.predicate_type; }
    jrd_rel* getTargetRelation() const { return m_analysis.target_relation; }
    USHORT getFieldId() const { return m_analysis.field_id; }
    const index_desc* getApplicableIndex() const { return m_analysis.applicable_index; }
    
    // Cost estimation
    virtual ULONG getEvaluationCost() const;
    virtual ULONG getEstimatedResultSize() const;
    
    // Optimization hints
    virtual bool benefitsFromPushdown() const;
    virtual bool requiresIndexScan() const;
    virtual bool supportsBitmapCaching() const;
    
    // String representation for debugging
    virtual ScratchBird::string toString() const;

protected:
    MemoryPool* m_pool;
    PredicateAnalysis m_analysis;
    
    // Helper methods
    CompressedBitmap* createEmptyBitmap(ULONG size) const;
    CompressedBitmap* createFullBitmap(ULONG size) const;
    
    // Bitmap operations
    CompressedBitmap* intersectBitmaps(CompressedBitmap* left, CompressedBitmap* right) const;
    CompressedBitmap* unionBitmaps(CompressedBitmap* left, CompressedBitmap* right) const;
    CompressedBitmap* negateBitmap(CompressedBitmap* bitmap) const;
};

//----------------------------
// Specific Predicate Implementations
//----------------------------

/**
 * Comparison predicate (=, <>, <, <=, >, >=)
 */
class ComparisonBitmapPredicate : public BitmapPredicate
{
public:
    ComparisonBitmapPredicate(MemoryPool* pool, const PredicateAnalysis& analysis,
                             CompareNode* comparison_node);
    
    virtual CompressedBitmap* evaluatePredicate(thread_db* tdbb, CompilerScratch* csb) override;
    virtual bool canEvaluateWithBitmap() const override;
    
private:
    CompareNode* m_comparison_node;
    
    // Comparison-specific evaluation
    CompressedBitmap* evaluateEquality(thread_db* tdbb, CompilerScratch* csb);
    CompressedBitmap* evaluateInequality(thread_db* tdbb, CompilerScratch* csb);
    CompressedBitmap* evaluateLessThan(thread_db* tdbb, CompilerScratch* csb);
    CompressedBitmap* evaluateGreaterThan(thread_db* tdbb, CompilerScratch* csb);
};

/**
 * Range predicate (BETWEEN, range IN)
 */
class RangeBitmapPredicate : public BitmapPredicate
{
public:
    RangeBitmapPredicate(MemoryPool* pool, const PredicateAnalysis& analysis,
                        ValueExprNode* min_value, ValueExprNode* max_value,
                        bool inclusive_min = true, bool inclusive_max = true);
    
    virtual CompressedBitmap* evaluatePredicate(thread_db* tdbb, CompilerScratch* csb) override;
    virtual bool canEvaluateWithBitmap() const override;
    
private:
    ValueExprNode* m_min_value;
    ValueExprNode* m_max_value;
    bool m_inclusive_min;
    bool m_inclusive_max;
};

/**
 * IN list predicate
 */
class InListBitmapPredicate : public BitmapPredicate
{
public:
    InListBitmapPredicate(MemoryPool* pool, const PredicateAnalysis& analysis,
                         const std::vector<ValueExprNode*>& value_list);
    
    virtual CompressedBitmap* evaluatePredicate(thread_db* tdbb, CompilerScratch* csb) override;
    virtual bool canEvaluateWithBitmap() const override;
    
private:
    std::vector<ValueExprNode*> m_value_list;
    
    // Optimization for large IN lists
    bool shouldUseHashOptimization() const;
    CompressedBitmap* evaluateWithHash(thread_db* tdbb, CompilerScratch* csb);
    CompressedBitmap* evaluateWithBitmapUnion(thread_db* tdbb, CompilerScratch* csb);
};

/**
 * NULL check predicate
 */
class NullCheckBitmapPredicate : public BitmapPredicate
{
public:
    NullCheckBitmapPredicate(MemoryPool* pool, const PredicateAnalysis& analysis, bool is_null_check);
    
    virtual CompressedBitmap* evaluatePredicate(thread_db* tdbb, CompilerScratch* csb) override;
    virtual bool canEvaluateWithBitmap() const override;
    
private:
    bool m_is_null_check; // true for IS NULL, false for IS NOT NULL
};

/**
 * Logical combination predicate (AND, OR, NOT)
 */
class LogicalBitmapPredicate : public BitmapPredicate
{
public:
    enum LogicalOperator : UCHAR {
        LOGICAL_AND = 0,
        LOGICAL_OR = 1,
        LOGICAL_NOT = 2
    };
    
    LogicalBitmapPredicate(MemoryPool* pool, const PredicateAnalysis& analysis,
                          LogicalOperator op, const std::vector<BitmapPredicate*>& operands);
    
    virtual CompressedBitmap* evaluatePredicate(thread_db* tdbb, CompilerScratch* csb) override;
    virtual bool canEvaluateWithBitmap() const override;
    
private:
    LogicalOperator m_operator;
    std::vector<BitmapPredicate*> m_operands;
    
    // Optimization for logical operations
    std::vector<BitmapPredicate*> optimizeOperandOrder() const;
    bool shouldUseShortCircuitEvaluation() const;
};

//----------------------------
// Predicate Pushdown Optimizer
//----------------------------

/**
 * Main optimizer for predicate pushdown in bitmap table scans
 */
class PredicatePushdownOptimizer
{
public:
    explicit PredicatePushdownOptimizer(MemoryPool* pool);
    ~PredicatePushdownOptimizer();

    // Main optimization interface
    std::vector<BitmapPredicate*> optimizePredicates(thread_db* tdbb, CompilerScratch* csb,
                                                     jrd_rel* relation,
                                                     const std::vector<BoolExprNode*>& predicates,
                                                     PushdownStrategy strategy = PUSHDOWN_STRATEGY_ADAPTIVE);
    
    // Predicate analysis
    PredicateAnalysis analyzePredicateForPushdown(thread_db* tdbb, CompilerScratch* csb,
                                                  jrd_rel* relation, BoolExprNode* predicate) const;
    
    std::vector<PredicateAnalysis> analyzePredicateSet(thread_db* tdbb, CompilerScratch* csb,
                                                       jrd_rel* relation,
                                                       const std::vector<BoolExprNode*>& predicates) const;
    
    // Pushdown decision making
    bool shouldPushdownPredicate(const PredicateAnalysis& analysis, PushdownStrategy strategy) const;
    
    double calculatePushdownBenefit(const PredicateAnalysis& analysis) const;
    
    // Predicate transformation
    BitmapPredicate* createBitmapPredicate(const PredicateAnalysis& analysis) const;
    
    std::vector<BitmapPredicate*> transformPredicates(const std::vector<PredicateAnalysis>& analyses) const;
    
    // Optimization strategies
    std::vector<BitmapPredicate*> applyEagerStrategy(const std::vector<PredicateAnalysis>& analyses) const;
    std::vector<BitmapPredicate*> applySelectiveStrategy(const std::vector<PredicateAnalysis>& analyses) const;
    std::vector<BitmapPredicate*> applyConservativeStrategy(const std::vector<PredicateAnalysis>& analyses) const;
    std::vector<BitmapPredicate*> applyAdaptiveStrategy(const std::vector<PredicateAnalysis>& analyses) const;
    std::vector<BitmapPredicate*> applyCostBasedStrategy(const std::vector<PredicateAnalysis>& analyses) const;
    
    // Execution order optimization
    std::vector<BitmapPredicate*> optimizeExecutionOrder(const std::vector<BitmapPredicate*>& predicates) const;
    
    // Performance estimation
    struct PushdownPerformanceEstimate
    {
        double estimated_selectivity;      // Overall selectivity after pushdown
        ULONG estimated_result_size;       // Estimated result cardinality
        ULONG bitmap_operation_cost;       // Cost of bitmap operations
        ULONG predicate_evaluation_cost;   // Cost of predicate evaluation
        double total_cost_reduction;       // Expected cost reduction
        bool is_beneficial;                // True if pushdown is beneficial
        
        PushdownPerformanceEstimate()
            : estimated_selectivity(1.0), estimated_result_size(0),
              bitmap_operation_cost(0), predicate_evaluation_cost(0),
              total_cost_reduction(0.0), is_beneficial(false)
        {
        }
    };
    
    PushdownPerformanceEstimate estimatePerformance(const std::vector<BitmapPredicate*>& predicates) const;
    
    // Configuration
    void setPushdownStrategy(PushdownStrategy strategy);
    PushdownStrategy getPushdownStrategy() const;
    
    void setSelectivityThreshold(double threshold);
    double getSelectivityThreshold() const;
    
    void setBenefitThreshold(double threshold);
    double getBenefitThreshold() const;

private:
    MemoryPool* m_pool;
    
    // Configuration
    PushdownStrategy m_default_strategy;
    double m_selectivity_threshold;
    double m_benefit_threshold;
    
    // Predicate analysis helpers
    PredicateType classifyPredicate(BoolExprNode* predicate) const;
    
    bool isComparisonPredicate(BoolExprNode* predicate) const;
    bool isRangePredicate(BoolExprNode* predicate) const;
    bool isInListPredicate(BoolExprNode* predicate) const;
    bool isNullCheckPredicate(BoolExprNode* predicate) const;
    bool isLogicalPredicate(BoolExprNode* predicate) const;
    
    // Field and relation analysis
    std::vector<USHORT> extractDependentFields(BoolExprNode* predicate) const;
    std::vector<jrd_rel*> extractDependentRelations(BoolExprNode* predicate) const;
    
    jrd_rel* identifyPrimaryRelation(BoolExprNode* predicate,
                                    const std::vector<jrd_rel*>& available_relations) const;
    
    // Index suitability analysis
    const index_desc* findBestBitmapIndex(thread_db* tdbb, jrd_rel* relation,
                                         const PredicateAnalysis& analysis) const;
    
    std::vector<const index_desc*> findApplicableBitmapIndexes(thread_db* tdbb, jrd_rel* relation,
                                                               USHORT field_id) const;
    
    bool isIndexSuitableForPredicate(const index_desc* index, const PredicateAnalysis& analysis) const;
    
    // Selectivity estimation
    double estimatePredicateSelectivity(thread_db* tdbb, CompilerScratch* csb,
                                       const PredicateAnalysis& analysis) const;
    
    double estimateComparisonSelectivity(thread_db* tdbb, const PredicateAnalysis& analysis) const;
    double estimateRangeSelectivity(thread_db* tdbb, const PredicateAnalysis& analysis) const;
    double estimateInListSelectivity(thread_db* tdbb, const PredicateAnalysis& analysis) const;
    
    // Cost modeling
    ULONG estimatePredicateEvaluationCost(const PredicateAnalysis& analysis) const;
    ULONG estimateBitmapOperationCost(const PredicateAnalysis& analysis) const;
    
    // Decision making
    double calculateBenefitScore(const PredicateAnalysis& analysis) const;
    
    bool isPredicateSafeForPushdown(const PredicateAnalysis& analysis) const;
    bool hasAcceptableComplexity(const PredicateAnalysis& analysis) const;
    
    // Strategy-specific decision logic
    bool shouldPushdownWithEagerStrategy(const PredicateAnalysis& analysis) const;
    bool shouldPushdownWithSelectiveStrategy(const PredicateAnalysis& analysis) const;
    bool shouldPushdownWithConservativeStrategy(const PredicateAnalysis& analysis) const;
    bool shouldPushdownWithAdaptiveStrategy(const PredicateAnalysis& analysis) const;
    bool shouldPushdownWithCostBasedStrategy(const PredicateAnalysis& analysis) const;
    
    // Execution order optimization
    struct PredicateOrderingCriteria
    {
        double selectivity;                 // Lower selectivity = higher priority
        ULONG evaluation_cost;              // Lower cost = higher priority
        bool requires_index;                // Index-based predicates first
        bool is_deterministic;              // Deterministic predicates first
        
        PredicateOrderingCriteria()
            : selectivity(1.0), evaluation_cost(0), requires_index(false), is_deterministic(true)
        {
        }
    };
    
    PredicateOrderingCriteria analyzePredicateOrdering(const BitmapPredicate* predicate) const;
    
    static bool comparePredicateOrder(const std::pair<BitmapPredicate*, PredicateOrderingCriteria>& a,
                                     const std::pair<BitmapPredicate*, PredicateOrderingCriteria>& b);
};

//----------------------------
// Bitmap Table Scan with Pushdown
//----------------------------

/**
 * Enhanced bitmap table scan that supports predicate pushdown
 */
class BitmapTableScanWithPushdown : public RecordSource
{
public:
    BitmapTableScanWithPushdown(CompilerScratch* csb, jrd_rel* relation,
                               const std::vector<BitmapPredicate*>& pushed_predicates,
                               const std::vector<BoolExprNode*>& remaining_predicates);
    
    virtual ~BitmapTableScanWithPushdown();

    // RecordSource interface
    virtual void open(thread_db* tdbb) override;
    virtual void close(thread_db* tdbb) override;
    virtual bool getRecord(thread_db* tdbb) override;
    virtual bool refetchRecord(thread_db* tdbb) override;
    virtual WriteLockResult lockRecord(thread_db* tdbb, bool skipLocked = false) override;
    virtual void markRecursive() override;
    virtual void invalidateRecords(jrd_req* request) override;
    
    // Cost and cardinality
    virtual double getCost() const override;
    virtual ULONG getCardinality() const override;
    
    // Pushdown-specific methods
    void setPushdownStrategy(PushdownStrategy strategy);
    
    struct PushdownMetrics
    {
        ULONG records_eliminated_by_pushdown;   // Records filtered by pushdown
        ULONG bitmap_operations_performed;      // Bitmap operations executed
        ULONG predicate_evaluations_saved;     // Predicate evaluations avoided
        double pushdown_selectivity;            // Actual pushdown selectivity
        ULONG pushdown_execution_time_ms;      // Time spent on pushdown operations
        
        PushdownMetrics()
            : records_eliminated_by_pushdown(0), bitmap_operations_performed(0),
              predicate_evaluations_saved(0), pushdown_selectivity(1.0),
              pushdown_execution_time_ms(0)
        {
        }
    };
    
    PushdownMetrics getPushdownMetrics() const;

private:
    CompilerScratch* m_csb;
    jrd_rel* m_relation;
    std::vector<BitmapPredicate*> m_pushed_predicates;
    std::vector<BoolExprNode*> m_remaining_predicates;
    
    // Execution state
    CompressedBitmap* m_filter_bitmap;
    CompressedBitmap::BitIterator m_bitmap_iterator;
    bool m_is_open;
    ULONG m_current_position;
    
    // Performance tracking
    PushdownMetrics m_pushdown_metrics;
    
    // Internal operations
    void initializeBitmapFilter(thread_db* tdbb);
    CompressedBitmap* evaluatePushedPredicates(thread_db* tdbb);
    bool evaluateRemainingPredicates(thread_db* tdbb);
    bool fetchNextRecordFromBitmap(thread_db* tdbb);
    
    void updatePushdownMetrics();
    void recordBitmapOperation();
    void recordPredicateEvaluationSaved(ULONG count);
};

//----------------------------
// Integration with Query Optimizer
//----------------------------

/**
 * Integration layer for predicate pushdown optimization
 */
class PredicatePushdownIntegration
{
public:
    // Optimizer hooks
    static void registerPushdownOptimizer();
    static void unregisterPushdownOptimizer();
    
    // Query plan enhancement
    static RecordSource* enhanceTableScanWithPushdown(thread_db* tdbb, CompilerScratch* csb,
                                                      jrd_rel* relation,
                                                      const std::vector<BoolExprNode*>& predicates);
    
    static bool shouldUsePushdownOptimization(thread_db* tdbb, CompilerScratch* csb,
                                             jrd_rel* relation,
                                             const std::vector<BoolExprNode*>& predicates);
    
    // Cost model integration
    static double calculatePushdownCostBenefit(thread_db* tdbb, CompilerScratch* csb,
                                              jrd_rel* relation,
                                              const std::vector<BoolExprNode*>& predicates);
    
    // Statistics collection
    static void updatePushdownStatistics(const BitmapTableScanWithPushdown::PushdownMetrics& metrics);
    static BitmapTableScanWithPushdown::PushdownMetrics getGlobalPushdownStatistics();

private:
    static std::unique_ptr<PredicatePushdownOptimizer> s_global_optimizer;
    static BitmapTableScanWithPushdown::PushdownMetrics s_global_metrics;
    static ScratchBird::Mutex s_integration_mutex;
};

//----------------------------
// Utility Functions
//----------------------------

// Predicate analysis utilities
bool canPredicateUseBitmapIndex(thread_db* tdbb, BoolExprNode* predicate, jrd_rel* relation);
std::vector<const index_desc*> findBitmapIndexesForPredicate(thread_db* tdbb, BoolExprNode* predicate, jrd_rel* relation);

// Selectivity estimation utilities
double estimatePredicateSelectivity(thread_db* tdbb, BoolExprNode* predicate, jrd_rel* relation);
ULONG estimatePredicateResultSize(thread_db* tdbb, BoolExprNode* predicate, jrd_rel* relation);

// Pushdown benefit analysis
double calculatePushdownBenefit(thread_db* tdbb, const std::vector<BoolExprNode*>& predicates, jrd_rel* relation);
bool isPushdownBeneficial(thread_db* tdbb, const std::vector<BoolExprNode*>& predicates, jrd_rel* relation,
                         double benefit_threshold = 0.1);

// Predicate complexity analysis
ULONG calculatePredicateComplexity(BoolExprNode* predicate);
bool isPredicateDeterministic(BoolExprNode* predicate);
bool doesPredicateHaveSideEffects(BoolExprNode* predicate);

} // namespace Jrd

#endif // JRD_PREDICATE_PUSHDOWN_OPTIMIZER_H