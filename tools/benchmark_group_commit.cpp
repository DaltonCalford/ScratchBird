/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/error_context.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>

using namespace scratchbird::core;

struct BenchmarkConfig
{
    int num_threads = 32;
    int commits_per_thread = 100;
    bool group_commit_enabled = true;
    uint64_t group_commit_timeout_us = 10000;  // 10ms
    uint32_t group_commit_batch_size = 32;
    bool verbose = false;
};

struct BenchmarkResults
{
    int total_commits = 0;
    int successful_commits = 0;
    int failed_commits = 0;
    uint64_t elapsed_ms = 0;
    uint64_t group_commits_performed = 0;
    uint64_t total_xids_batched = 0;
    double throughput_cps = 0.0;         // commits per second
    double avg_batch_size = 0.0;
    double avg_latency_ms = 0.0;
};

void print_config(const BenchmarkConfig &config)
{
    std::cout << "\n========================================\n";
    std::cout << "Group Commit Benchmark Configuration\n";
    std::cout << "========================================\n";
    std::cout << "Threads:              " << config.num_threads << "\n";
    std::cout << "Commits per thread:   " << config.commits_per_thread << "\n";
    std::cout << "Total commits:        " << (config.num_threads * config.commits_per_thread)
              << "\n";
    std::cout << "Group commit:         " << (config.group_commit_enabled ? "ENABLED" : "DISABLED")
              << "\n";
    if (config.group_commit_enabled)
    {
        std::cout << "  Timeout:            " << config.group_commit_timeout_us << " us\n";
        std::cout << "  Batch size:         " << config.group_commit_batch_size << "\n";
    }
    std::cout << "========================================\n\n";
}

void print_results(const BenchmarkResults &results, const BenchmarkConfig &config)
{
    std::cout << "\n========================================\n";
    std::cout << "Benchmark Results\n";
    std::cout << "========================================\n";
    std::cout << "Total commits:        " << results.total_commits << "\n";
    std::cout << "Successful:           " << results.successful_commits << "\n";
    std::cout << "Failed:               " << results.failed_commits << "\n";
    std::cout << "Elapsed time:         " << results.elapsed_ms << " ms\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Throughput:           " << results.throughput_cps << " commits/sec\n";
    std::cout << "Avg latency:          " << results.avg_latency_ms << " ms\n";

    if (config.group_commit_enabled && results.group_commits_performed > 0)
    {
        std::cout << "\nGroup Commit Statistics:\n";
        std::cout << "  Group commits:      " << results.group_commits_performed << "\n";
        std::cout << "  Total XIDs batched: " << results.total_xids_batched << "\n";
        std::cout << "  Avg batch size:     " << results.avg_batch_size << "\n";
        std::cout << "  fsync reduction:    "
                  << (100.0 * (1.0 - static_cast<double>(results.group_commits_performed) /
                                         results.successful_commits))
                  << "%\n";
    }
    std::cout << "========================================\n\n";
}

BenchmarkResults run_benchmark(const BenchmarkConfig &config)
{
    BenchmarkResults results;
    results.total_commits = config.num_threads * config.commits_per_thread;

    // Create database
    const char *db_path = "benchmark_group_commit.db";
    std::remove(db_path);

    ErrorContext ctx;
    Status status = Database::create(db_path, 8192, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to create database: " << ctx.error_message << "\n";
        return results;
    }

    Database *db = nullptr;
    status = Database::open(db_path, &db, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Failed to open database: " << ctx.error_message << "\n";
        return results;
    }

    // Initialize ProcArray
    ProcArrayManager::initialize(config.num_threads + 10);

    auto *txn_mgr = db->transaction_manager();

    // Configure group commit
    txn_mgr->enableGroupCommit(config.group_commit_enabled);
    if (config.group_commit_enabled)
    {
        txn_mgr->setGroupCommitTimeout(config.group_commit_timeout_us);
        txn_mgr->setGroupCommitBatchSize(config.group_commit_batch_size);
    }

    // Get baseline statistics
    auto [gc_before, xids_before] = txn_mgr->getGroupCommitStats();

    // Atomic counters
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    std::vector<std::thread> threads;

    // Start benchmark
    auto start_time = std::chrono::steady_clock::now();

    for (int t = 0; t < config.num_threads; t++)
    {
        threads.emplace_back(
            [&txn_mgr, &config, t, &success_count, &failure_count]() {
                ErrorContext thread_ctx;
                uint32_t proc_id = t;

                for (int i = 0; i < config.commits_per_thread; i++)
                {
                    // Begin transaction
                    uint64_t xid;
                    Status status = txn_mgr->beginTransaction(proc_id, xid, &thread_ctx);
                    if (status != Status::OK)
                    {
                        failure_count.fetch_add(1);
                        continue;
                    }

                    // Commit transaction
                    status = txn_mgr->commitTransaction(proc_id, xid, &thread_ctx);
                    if (status == Status::OK)
                    {
                        success_count.fetch_add(1);
                    }
                    else
                    {
                        failure_count.fetch_add(1);
                        if (config.verbose)
                        {
                            std::cerr << "Commit failed for XID " << xid << ": "
                                      << thread_ctx.error_message << "\n";
                        }
                    }
                }
            });
    }

    // Wait for all threads to complete
    for (auto &thread : threads)
    {
        thread.join();
    }

    auto end_time = std::chrono::steady_clock::now();

    // Calculate results
    results.successful_commits = success_count.load();
    results.failed_commits = failure_count.load();
    results.elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (results.elapsed_ms > 0)
    {
        results.throughput_cps = (results.successful_commits * 1000.0) / results.elapsed_ms;
        results.avg_latency_ms =
            static_cast<double>(results.elapsed_ms) / results.successful_commits;
    }

    // Get group commit statistics
    auto [gc_after, xids_after] = txn_mgr->getGroupCommitStats();
    results.group_commits_performed = gc_after - gc_before;
    results.total_xids_batched = xids_after - xids_before;

    if (results.group_commits_performed > 0)
    {
        results.avg_batch_size =
            static_cast<double>(results.total_xids_batched) / results.group_commits_performed;
    }

    // Cleanup
    ProcArrayManager::shutdown();
    delete db;
    std::remove(db_path);

    return results;
}

void print_usage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " [OPTIONS]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --threads N          Number of concurrent threads (default: 32)\n";
    std::cout << "  --commits N          Commits per thread (default: 100)\n";
    std::cout << "  --no-group-commit    Disable group commit\n";
    std::cout << "  --timeout US         Group commit timeout in microseconds (default: 10000)\n";
    std::cout << "  --batch-size N       Group commit batch size (default: 32)\n";
    std::cout << "  --verbose            Enable verbose output\n";
    std::cout << "  --compare            Run both with and without group commit\n";
    std::cout << "  --help               Show this help message\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " --threads 50 --commits 200\n";
    std::cout << "  " << program_name << " --compare\n";
    std::cout << "  " << program_name << " --no-group-commit\n";
}

int main(int argc, char **argv)
{
    BenchmarkConfig config;
    bool compare_mode = false;

    // Parse command line arguments
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "--threads" && i + 1 < argc)
        {
            config.num_threads = std::atoi(argv[++i]);
        }
        else if (arg == "--commits" && i + 1 < argc)
        {
            config.commits_per_thread = std::atoi(argv[++i]);
        }
        else if (arg == "--no-group-commit")
        {
            config.group_commit_enabled = false;
        }
        else if (arg == "--timeout" && i + 1 < argc)
        {
            config.group_commit_timeout_us = std::atoi(argv[++i]);
        }
        else if (arg == "--batch-size" && i + 1 < argc)
        {
            config.group_commit_batch_size = std::atoi(argv[++i]);
        }
        else if (arg == "--verbose" || arg == "-v")
        {
            config.verbose = true;
        }
        else if (arg == "--compare")
        {
            compare_mode = true;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════╗\n";
    std::cout << "║  ScratchBird Group Commit Benchmark   ║\n";
    std::cout << "╚════════════════════════════════════════╝\n";

    if (compare_mode)
    {
        // Run without group commit
        std::cout << "\n[1/2] Running WITHOUT group commit...\n";
        BenchmarkConfig config_no_gc = config;
        config_no_gc.group_commit_enabled = false;
        print_config(config_no_gc);
        auto results_no_gc = run_benchmark(config_no_gc);
        print_results(results_no_gc, config_no_gc);

        // Run with group commit
        std::cout << "\n[2/2] Running WITH group commit...\n";
        BenchmarkConfig config_gc = config;
        config_gc.group_commit_enabled = true;
        print_config(config_gc);
        auto results_gc = run_benchmark(config_gc);
        print_results(results_gc, config_gc);

        // Print comparison
        std::cout << "\n========================================\n";
        std::cout << "Comparison Summary\n";
        std::cout << "========================================\n";
        std::cout << std::fixed << std::setprecision(2);

        double speedup = results_gc.throughput_cps / results_no_gc.throughput_cps;
        double fsync_reduction = 100.0 * (1.0 - static_cast<double>(results_gc.group_commits_performed) /
                                          results_no_gc.successful_commits);

        std::cout << "Throughput improvement:  " << speedup << "x\n";
        std::cout << "Without group commit:    " << results_no_gc.throughput_cps << " commits/sec\n";
        std::cout << "With group commit:       " << results_gc.throughput_cps << " commits/sec\n";
        std::cout << "\nfsync reduction:         " << fsync_reduction << "%\n";
        std::cout << "Without group commit:    " << results_no_gc.successful_commits << " fsyncs\n";
        std::cout << "With group commit:       " << results_gc.group_commits_performed << " fsyncs\n";
        std::cout << "\nAverage batch size:      " << results_gc.avg_batch_size << " commits/batch\n";
        std::cout << "========================================\n\n";
    }
    else
    {
        // Single run
        print_config(config);
        auto results = run_benchmark(config);
        print_results(results, config);
    }

    return 0;
}
