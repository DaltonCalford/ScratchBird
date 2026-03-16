#include <gtest/gtest.h>

#include "scratchbird/optimizer/cost_model.h"

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using scratchbird::optimizer::CostEstimate;
using scratchbird::optimizer::CostFormulaProfile;
using scratchbird::optimizer::CostModel;

namespace
{
    struct CalibrationCase
    {
        uint64_t table_pages;
        uint64_t table_rows;
        uint64_t index_pages;
        uint64_t index_rows;
        double correlation;
    };

    auto calibrationCorpus() -> std::vector<CalibrationCase>
    {
        return {
            {128, 12000, 8, 12, 0.95},
            {256, 18000, 16, 48, 0.80},
            {512, 64000, 24, 120, 0.55},
            {1024, 125000, 48, 320, 0.20},
            {2048, 250000, 64, 1200, 0.10},
        };
    }

    auto makeProfile(const std::string &profile_id,
                     const std::string &calibration_id,
                     const std::string &workload_profile,
                     double random_page_cost,
                     double cpu_tuple_cost,
                     uint64_t work_mem_bytes) -> CostFormulaProfile
    {
        CostFormulaProfile profile;
        profile.profile_id = profile_id;
        profile.profile_version = 1;
        profile.calibration_profile_id = calibration_id;
        profile.storage_profile = "heap_btree";
        profile.workload_profile = workload_profile;
        profile.parameters.seq_page_cost = 1.0;
        profile.parameters.random_page_cost = random_page_cost;
        profile.parameters.cpu_tuple_cost = cpu_tuple_cost;
        profile.parameters.cpu_index_tuple_cost = 0.005;
        profile.parameters.cpu_operator_cost = 0.0025;
        profile.parameters.work_mem_bytes = work_mem_bytes;
        profile.parameters.effective_cache_size = 1024.0;
        return profile;
    }

    auto csvEscape(const std::string& value) -> std::string
    {
        std::string escaped = "\"";
        escaped.reserve(value.size() + 2);
        for (const char ch : value)
        {
            if (ch == '"')
            {
                escaped += "\"\"";
            }
            else
            {
                escaped.push_back(ch);
            }
        }
        escaped.push_back('"');
        return escaped;
    }

    auto csvRow(const std::vector<std::string>& columns) -> std::string
    {
        std::ostringstream out;
        for (size_t i = 0; i < columns.size(); ++i)
        {
            if (i > 0)
            {
                out << ',';
            }
            out << csvEscape(columns[i]);
        }
        return out.str();
    }

    auto writeDelimitedLines(const std::filesystem::path& path,
                             const std::vector<std::string>& lines) -> bool
    {
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }
        std::ofstream out(path);
        if (!out.is_open())
        {
            return false;
        }
        for (const auto& line : lines)
        {
            out << line << '\n';
        }
        return true;
    }
} // namespace

TEST(OptimizerCostCalibrationBenchmarkTest,
     FixedSeedCorpusProducesStableProfileDrivenCostEvidence)
{
    const auto oltp_profile = makeProfile("sb_cost_formula/benchmark_oltp",
                                          "sb_cost_calibration/benchmark_oltp_v1",
                                          "oltp",
                                          4.0,
                                          0.01,
                                          4 * 1024 * 1024);
    const auto analytic_profile = makeProfile("sb_cost_formula/benchmark_analytic",
                                              "sb_cost_calibration/benchmark_analytic_v1",
                                              "analytic_scan",
                                              2.2,
                                              0.018,
                                              16 * 1024 * 1024);

    CostModel oltp_model(oltp_profile);
    CostModel analytic_model(analytic_profile);
    const auto corpus = calibrationCorpus();

    size_t oltp_prefers_index = 0;
    size_t analytic_prefers_index = 0;
    std::vector<std::string> benchmark_rows = {
        csvRow({"profile_id",
                "workload_profile",
                "case_id",
                "table_pages",
                "table_rows",
                "index_pages",
                "index_rows",
                "correlation",
                "seq_total_cost",
                "index_total_cost",
                "cost_delta",
                "index_prefers",
                "formula_profile_id",
                "calibration_profile_id"})};

    for (size_t case_index = 0; case_index < corpus.size(); ++case_index)
    {
        const auto& entry = corpus[case_index];
        const CostEstimate oltp_seq =
            oltp_model.costSeqScan(entry.table_pages, entry.table_rows, 0.0);
        const CostEstimate oltp_index = oltp_model.costIndexScan(3,
                                                                 entry.index_pages,
                                                                 entry.index_rows,
                                                                 entry.index_pages,
                                                                 entry.index_rows,
                                                                 0.0,
                                                                 entry.correlation);
        const CostEstimate analytic_seq =
            analytic_model.costSeqScan(entry.table_pages, entry.table_rows, 0.0);
        const CostEstimate analytic_index = analytic_model.costIndexScan(3,
                                                                         entry.index_pages,
                                                                         entry.index_rows,
                                                                         entry.index_pages,
                                                                         entry.index_rows,
                                                                         0.0,
                                                                         entry.correlation);

        EXPECT_EQ(oltp_index.formula_profile_id, oltp_profile.profile_id);
        EXPECT_EQ(oltp_index.calibration_profile_id,
                  oltp_profile.calibration_profile_id);
        EXPECT_EQ(analytic_index.formula_profile_id, analytic_profile.profile_id);
        EXPECT_EQ(analytic_index.calibration_profile_id,
                  analytic_profile.calibration_profile_id);
        EXPECT_FALSE(oltp_index.expanded_terms.empty());
        EXPECT_FALSE(analytic_index.expanded_terms.empty());

        if (oltp_index.total_cost < oltp_seq.total_cost)
        {
            ++oltp_prefers_index;
        }
        if (analytic_index.total_cost < analytic_seq.total_cost)
        {
            ++analytic_prefers_index;
        }

        benchmark_rows.push_back(
            csvRow({oltp_profile.profile_id,
                    oltp_profile.workload_profile,
                    "case_" + std::to_string(case_index + 1),
                    std::to_string(entry.table_pages),
                    std::to_string(entry.table_rows),
                    std::to_string(entry.index_pages),
                    std::to_string(entry.index_rows),
                    std::to_string(entry.correlation),
                    std::to_string(oltp_seq.total_cost),
                    std::to_string(oltp_index.total_cost),
                    std::to_string(oltp_seq.total_cost - oltp_index.total_cost),
                    oltp_index.total_cost < oltp_seq.total_cost ? "true" : "false",
                    oltp_index.formula_profile_id,
                    oltp_index.calibration_profile_id}));
        benchmark_rows.push_back(
            csvRow({analytic_profile.profile_id,
                    analytic_profile.workload_profile,
                    "case_" + std::to_string(case_index + 1),
                    std::to_string(entry.table_pages),
                    std::to_string(entry.table_rows),
                    std::to_string(entry.index_pages),
                    std::to_string(entry.index_rows),
                    std::to_string(entry.correlation),
                    std::to_string(analytic_seq.total_cost),
                    std::to_string(analytic_index.total_cost),
                    std::to_string(analytic_seq.total_cost -
                                   analytic_index.total_cost),
                    analytic_index.total_cost < analytic_seq.total_cost ? "true"
                                                                        : "false",
                    analytic_index.formula_profile_id,
                    analytic_index.calibration_profile_id}));
    }

    std::cout << "[Benchmark] OPW-013 profile=oltp index_prefers="
              << oltp_prefers_index << "/" << corpus.size() << "\n";
    std::cout << "[Benchmark] OPW-013 profile=analytic index_prefers="
              << analytic_prefers_index << "/" << corpus.size() << "\n";

    EXPECT_GT(oltp_prefers_index, 0u);
    EXPECT_GT(analytic_prefers_index, 0u);
    EXPECT_LE(analytic_prefers_index, oltp_prefers_index);

    if (const char* path = std::getenv("SB_OPTIMIZER_COST_BENCHMARK_CSV"))
    {
        ASSERT_TRUE(writeDelimitedLines(path, benchmark_rows)) << path;
    }
}
