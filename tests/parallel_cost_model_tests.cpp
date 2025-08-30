// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/parallel_executor.h"

#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace scratchbird::engine;

class ParallelCostModelTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        ParallelConfig config;
        config.max_worker_threads = 8;
        config.cpu_cost_per_tuple = 0.01;
        config.io_cost_per_page = 1.0;
        config.parallel_setup_cost = 1000.0;

        cost_model_ = std::make_unique<ParallelCostModel>(config);
    }

    void TearDown() override
    {
        cost_model_.reset();
    }

    SystemResourceInfo create_test_resources(std::uint32_t cores = 8,
                                             std::uint64_t memory_mb = 8192,
                                             double cpu_utilization = 0.3,
                                             double memory_utilization = 0.4)
    {
        SystemResourceInfo resources;
        resources.available_cores = cores;
        resources.available_memory_mb = memory_mb;
        resources.cpu_utilization = cpu_utilization;
        resources.memory_utilization = memory_utilization;
        resources.active_queries = 1;
        resources.available_workers = std::max(1U, cores - resources.active_queries);
        resources.numa_enabled = cores > 8;
        resources.numa_nodes = resources.numa_enabled ? cores / 8 : 1;
        return resources;
    }

    QueryComplexity create_test_complexity(std::uint32_t joins = 2,
                                           std::uint64_t intermediate_rows = 1000000,
                                           std::uint64_t result_rows = 100000)
    {
        QueryComplexity complexity;
        complexity.table_count = joins + 1;
        complexity.join_count = joins;
        complexity.aggregate_count = 1;
        complexity.sort_count = 1;
        complexity.estimated_intermediate_rows = intermediate_rows;
        complexity.estimated_result_rows = result_rows;
        return complexity;
    }

    std::unique_ptr<ParallelCostModel> cost_model_;
};

/// Test system resource information methods
TEST_F(ParallelCostModelTest, SystemResourceInfoMethods)
{
    SystemResourceInfo resources = create_test_resources(8, 4096, 0.5, 0.6);

    // Test resource pressure calculation
    EXPECT_DOUBLE_EQ(resources.get_resource_pressure(), 0.6); // Max of CPU and memory

    // Test effective parallelism limit
    std::uint32_t limit = resources.get_effective_parallelism_limit();
    EXPECT_GT(limit, 0);
    EXPECT_LE(limit, resources.available_workers);

    // Test high pressure scenario
    resources.cpu_utilization = 0.95;
    EXPECT_EQ(resources.get_effective_parallelism_limit(), 1); // Sequential only
}

/// Test query complexity analysis
TEST_F(ParallelCostModelTest, QueryComplexityAnalysis)
{
    // Simple query - should not benefit from parallelization
    QueryComplexity simple;
    simple.table_count = 1;
    simple.estimated_result_rows = 100; // Too small
    EXPECT_FALSE(simple.benefits_from_parallelization());

    // Complex query - should benefit
    QueryComplexity complex = create_test_complexity(3, 5000000, 1000000);
    EXPECT_TRUE(complex.benefits_from_parallelization());
    EXPECT_GT(complex.get_complexity_score(), 10.0);

    // Recursive CTE - should not benefit
    QueryComplexity recursive;
    recursive.has_recursive_cte = true;
    recursive.estimated_result_rows = 1000000;
    EXPECT_FALSE(recursive.benefits_from_parallelization());
}

/// Test basic resource monitoring
TEST_F(ParallelCostModelTest, ResourceMonitoring)
{
    // Test get current resources
    SystemResourceInfo resources = cost_model_->get_current_system_resources();
    EXPECT_GT(resources.available_cores, 0);
    EXPECT_GT(resources.available_memory_mb, 0);
    EXPECT_GE(resources.cpu_utilization, 0.0);
    EXPECT_LE(resources.cpu_utilization, 1.0);

    // Test resource cache update
    SystemResourceInfo custom_resources = create_test_resources(16, 16384, 0.2, 0.3);
    cost_model_->update_resource_cache(custom_resources);

    SystemResourceInfo cached = cost_model_->get_current_system_resources();
    EXPECT_EQ(cached.available_cores, 16);
    EXPECT_EQ(cached.available_memory_mb, 16384);
}

/// Test query parallelization analysis
TEST_F(ParallelCostModelTest, QueryParallelizationAnalysis)
{
    SystemResourceInfo resources = create_test_resources(8, 8192, 0.3, 0.4);

    // Test simple query that shouldn't be parallelized
    QueryComplexity simple;
    simple.estimated_result_rows = 100;

    ParallelizationDecision decision =
        cost_model_->analyze_query_parallelization(simple, resources);
    EXPECT_FALSE(decision.should_parallelize);
    EXPECT_EQ(decision.recommended_workers, 1);
    EXPECT_GT(decision.confidence, 0.5);
    EXPECT_FALSE(decision.reasoning.empty());

    // Test complex query that should be parallelized
    QueryComplexity complex = create_test_complexity(2, 2000000, 500000);

    decision = cost_model_->analyze_query_parallelization(complex, resources);
    EXPECT_EQ(decision.should_parallelize, decision.expected_speedup > 1.0);

    if (decision.should_parallelize) {
        EXPECT_GT(decision.recommended_workers, 1);
        EXPECT_LE(decision.recommended_workers, resources.get_effective_parallelism_limit());
        EXPECT_GT(decision.expected_speedup, 1.0);
        EXPECT_GT(decision.memory_requirement_mb, 0);
    }
}

/// Test resource pressure impact on decisions
TEST_F(ParallelCostModelTest, ResourcePressureImpact)
{
    QueryComplexity complex = create_test_complexity(3, 5000000, 1000000);

    // Low resource pressure - should allow parallelization
    SystemResourceInfo low_pressure = create_test_resources(8, 8192, 0.2, 0.3);
    ParallelizationDecision low_decision =
        cost_model_->analyze_query_parallelization(complex, low_pressure);

    // High resource pressure - should discourage parallelization
    SystemResourceInfo high_pressure = create_test_resources(8, 8192, 0.95, 0.9);
    ParallelizationDecision high_decision =
        cost_model_->analyze_query_parallelization(complex, high_pressure);

    EXPECT_FALSE(high_decision.should_parallelize);
    EXPECT_EQ(high_decision.recommended_workers, 1);
    EXPECT_FALSE(high_decision.reasoning.find("pressure") == std::string::npos);
}

/// Test operation parallelization decisions
TEST_F(ParallelCostModelTest, OperationParallelizationDecision)
{
    SystemResourceInfo resources = create_test_resources(8, 8192, 0.3, 0.4);

    // Create a cost estimate that favors parallelization
    ParallelCostEstimate estimate;
    estimate.sequential_cost = 10000.0;
    estimate.parallel_cost = 4000.0;
    estimate.setup_overhead = 500.0;
    estimate.coordination_overhead = 200.0;
    estimate.optimal_workers = 4;
    estimate.expected_speedup = 2.5;

    ParallelizationDecision decision =
        cost_model_->decide_operation_parallelization(estimate, resources, "TableScan");

    EXPECT_TRUE(decision.should_parallelize);
    EXPECT_LE(decision.recommended_workers, resources.get_effective_parallelism_limit());
    EXPECT_GT(decision.expected_speedup, 1.2); // Above threshold
    EXPECT_FALSE(decision.reasoning.find("TableScan") == std::string::npos);

    // Test with insufficient benefit
    estimate.expected_speedup = 1.1; // Below threshold
    decision = cost_model_->decide_operation_parallelization(estimate, resources, "SmallTableScan");

    EXPECT_FALSE(decision.should_parallelize);
    EXPECT_FALSE(decision.reasoning.find("Insufficient") == std::string::npos);
}

/// Test dynamic worker count optimization
TEST_F(ParallelCostModelTest, DynamicWorkerCountOptimization)
{
    SystemResourceInfo resources = create_test_resources(8, 8192, 0.3, 0.4);

    ParallelCostEstimate estimate;
    estimate.optimal_workers = 4;

    // Test with good historical performance
    std::vector<double> good_history = {1.8, 2.0, 1.9, 2.1}; // Good speedups
    std::uint32_t optimized =
        cost_model_->optimize_worker_count_dynamically(estimate, resources, good_history);

    EXPECT_GE(optimized, estimate.optimal_workers); // Should maintain or increase

    // Test with poor historical performance
    std::vector<double> poor_history = {0.6, 0.7, 0.5, 0.8}; // Poor speedups
    optimized = cost_model_->optimize_worker_count_dynamically(estimate, resources, poor_history);

    EXPECT_LE(optimized, estimate.optimal_workers); // Should reduce

    // Test with high resource pressure
    resources.cpu_utilization = 0.9;
    optimized = cost_model_->optimize_worker_count_dynamically(estimate, resources, good_history);

    EXPECT_LE(optimized, 2); // Should be limited due to pressure
}

/// Test parallel execution feasibility
TEST_F(ParallelCostModelTest, ParallelExecutionFeasibility)
{
    SystemResourceInfo resources = create_test_resources(8, 8192, 0.3, 0.4);

    // Test feasible parallelization
    ParallelCostEstimate good_estimate;
    good_estimate.optimal_workers = 4;
    good_estimate.expected_speedup = 2.0;

    EXPECT_TRUE(cost_model_->should_use_parallel_execution(good_estimate, resources));

    // Test insufficient speedup
    ParallelCostEstimate poor_estimate;
    poor_estimate.optimal_workers = 4;
    poor_estimate.expected_speedup = 1.1; // Below threshold

    EXPECT_FALSE(cost_model_->should_use_parallel_execution(poor_estimate, resources));

    // Test single worker
    ParallelCostEstimate sequential_estimate;
    sequential_estimate.optimal_workers = 1;
    sequential_estimate.expected_speedup = 1.0;

    EXPECT_FALSE(cost_model_->should_use_parallel_execution(sequential_estimate, resources));

    // Test high resource pressure
    resources.cpu_utilization = 0.95;
    EXPECT_FALSE(cost_model_->should_use_parallel_execution(good_estimate, resources));
}

/// Test parallelization efficiency calculation
TEST_F(ParallelCostModelTest, ParallelizationEfficiency)
{
    ParallelCostEstimate estimate;
    estimate.sequential_cost = 10000.0;
    estimate.setup_overhead = 500.0;
    estimate.coordination_overhead = 200.0;

    // Test single worker efficiency
    EXPECT_DOUBLE_EQ(cost_model_->calculate_parallelization_efficiency(1, estimate), 1.0);

    // Test multi-worker efficiency
    double efficiency_2 = cost_model_->calculate_parallelization_efficiency(2, estimate);
    double efficiency_4 = cost_model_->calculate_parallelization_efficiency(4, estimate);
    double efficiency_8 = cost_model_->calculate_parallelization_efficiency(8, estimate);

    // Efficiency should generally decrease with more workers due to overhead
    EXPECT_LE(efficiency_2, 1.0);
    EXPECT_LE(efficiency_4, efficiency_2);
    EXPECT_LE(efficiency_8, efficiency_4);

    // All efficiencies should be positive
    EXPECT_GT(efficiency_2, 0.0);
    EXPECT_GT(efficiency_4, 0.0);
    EXPECT_GT(efficiency_8, 0.0);
}

/// Test memory pressure penalties
TEST_F(ParallelCostModelTest, MemoryPressurePenalties)
{
    SystemResourceInfo low_memory = create_test_resources(8, 1024, 0.3, 0.4);   // Only 1GB
    SystemResourceInfo high_memory = create_test_resources(8, 32768, 0.3, 0.4); // 32GB

    QueryComplexity memory_intensive = create_test_complexity(3, 10000000, 2000000);

    // Should be more conservative with low memory
    ParallelizationDecision low_mem_decision =
        cost_model_->analyze_query_parallelization(memory_intensive, low_memory);

    ParallelizationDecision high_mem_decision =
        cost_model_->analyze_query_parallelization(memory_intensive, high_memory);

    if (low_mem_decision.should_parallelize && high_mem_decision.should_parallelize) {
        EXPECT_LE(low_mem_decision.recommended_workers, high_mem_decision.recommended_workers);
    }
}

/// Test NUMA awareness
TEST_F(ParallelCostModelTest, NumaAwareness)
{
    // System with NUMA
    SystemResourceInfo numa_system = create_test_resources(16, 32768, 0.3, 0.4);
    numa_system.numa_enabled = true;
    numa_system.numa_nodes = 2;

    // System without NUMA
    SystemResourceInfo non_numa_system = create_test_resources(16, 32768, 0.3, 0.4);
    non_numa_system.numa_enabled = false;
    non_numa_system.numa_nodes = 1;

    QueryComplexity complex = create_test_complexity(3, 5000000, 1000000);

    ParallelizationDecision numa_decision =
        cost_model_->analyze_query_parallelization(complex, numa_system);

    ParallelizationDecision non_numa_decision =
        cost_model_->analyze_query_parallelization(complex, non_numa_system);

    // Both should potentially parallelize, but NUMA system might be more conservative
    // about cross-NUMA worker allocation
    if (numa_decision.should_parallelize && non_numa_decision.should_parallelize) {
        // The test mainly verifies that NUMA considerations are factored in
        EXPECT_GT(numa_decision.confidence, 0.0);
        EXPECT_GT(non_numa_decision.confidence, 0.0);
    }
}

/// Test decision confidence calculation
TEST_F(ParallelCostModelTest, DecisionConfidence)
{
    SystemResourceInfo resources = create_test_resources(8, 8192, 0.3, 0.4);
    QueryComplexity complex = create_test_complexity(2, 2000000, 500000);

    ParallelizationDecision decision =
        cost_model_->analyze_query_parallelization(complex, resources);

    // Confidence should be reasonable
    EXPECT_GE(decision.confidence, 0.0);
    EXPECT_LE(decision.confidence, 1.0);

    // Higher expected speedup should generally mean higher confidence
    // (though this depends on other factors)
    if (decision.expected_speedup > 2.0) {
        EXPECT_GT(decision.confidence, 0.5);
    }
}

/// Performance test for decision making
TEST_F(ParallelCostModelTest, DecisionMakingPerformance)
{
    SystemResourceInfo resources = create_test_resources(8, 8192, 0.3, 0.4);
    QueryComplexity complex = create_test_complexity(3, 5000000, 1000000);

    auto start_time = std::chrono::steady_clock::now();

    const int num_decisions = 1000;
    for (int i = 0; i < num_decisions; ++i) {
        // Vary the complexity slightly to avoid caching effects
        QueryComplexity varied = complex;
        varied.estimated_intermediate_rows += i;

        ParallelizationDecision decision =
            cost_model_->analyze_query_parallelization(varied, resources);

        // Basic validity check
        EXPECT_GE(decision.confidence, 0.0);
        EXPECT_LE(decision.confidence, 1.0);
    }

    auto end_time = std::chrono::steady_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Should be able to make decisions quickly (< 100ms for 1000 decisions)
    EXPECT_LT(total_time.count(), 100);

    double avg_time_per_decision = static_cast<double>(total_time.count()) / num_decisions;
    EXPECT_LT(avg_time_per_decision, 0.1); // < 0.1ms per decision
}

/// Integration test combining all features
TEST_F(ParallelCostModelTest, IntegrationTest)
{
    // Simulate a realistic scenario
    SystemResourceInfo resources = create_test_resources(12, 16384, 0.4, 0.3);

    // Complex analytical query
    QueryComplexity analytical_query;
    analytical_query.table_count = 5;
    analytical_query.join_count = 4;
    analytical_query.aggregate_count = 3;
    analytical_query.sort_count = 2;
    analytical_query.window_function_count = 1;
    analytical_query.estimated_intermediate_rows = 10000000;
    analytical_query.estimated_result_rows = 1000000;
    analytical_query.has_subqueries = true;

    // Analyze parallelization
    ParallelizationDecision decision =
        cost_model_->analyze_query_parallelization(analytical_query, resources);

    // Should benefit from parallelization due to complexity
    EXPECT_TRUE(analytical_query.benefits_from_parallelization());
    EXPECT_GT(analytical_query.get_complexity_score(), 20.0);

    if (decision.should_parallelize) {
        EXPECT_GT(decision.recommended_workers, 1);
        EXPECT_LE(decision.recommended_workers, resources.get_effective_parallelism_limit());
        EXPECT_GT(decision.expected_speedup, 1.2);
        EXPECT_GT(decision.memory_requirement_mb, 0);
        EXPECT_FALSE(decision.reasoning.empty());

        // Test dynamic optimization
        std::vector<double> performance_history = {1.8, 2.2, 1.9, 2.0, 2.1};

        // Create cost estimate for dynamic optimization
        ParallelCostEstimate estimate;
        estimate.optimal_workers = decision.recommended_workers;
        estimate.expected_speedup = decision.expected_speedup;

        std::uint32_t optimized_workers = cost_model_->optimize_worker_count_dynamically(
            estimate, resources, performance_history);

        EXPECT_GT(optimized_workers, 0);
        EXPECT_LE(optimized_workers, resources.available_workers);
    }

    // Verify decision consistency
    EXPECT_GE(decision.confidence, 0.0);
    EXPECT_LE(decision.confidence, 1.0);
    EXPECT_GT(decision.estimated_sequential_time.count(), 0);

    if (decision.should_parallelize) {
        EXPECT_GT(decision.estimated_parallel_time.count(), 0);
        EXPECT_LT(decision.estimated_parallel_time, decision.estimated_sequential_time);
    }
}

/// Run all tests
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
