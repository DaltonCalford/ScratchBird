#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "scratchbird/core/telemetry.h"

using namespace scratchbird::core;

namespace
{
auto labelsKey(const std::vector<MetricLabel>& labels) -> std::string
{
    std::string key;
    for (size_t i = 0; i < labels.size(); ++i)
    {
        if (i > 0)
        {
            key += "|";
        }
        key += labels[i].name + "=" + labels[i].value;
    }
    return key;
}
} // namespace

TEST(TelemetryReportingContractTest, SnapshotAndPrometheusExportAreDeterministic)
{
    auto& registry = MetricsRegistry::getInstance();
    registry.clear();

    auto* z_counter = registry.registerCounter("z_metric_total", "z counter", {"slot"});
    auto* a_gauge = registry.registerGauge("a_metric", "a gauge", {"slot"});

    ASSERT_NE(z_counter, nullptr);
    ASSERT_NE(a_gauge, nullptr);

    a_gauge->set(2.0, {"b"});
    a_gauge->set(1.0, {"a"});
    z_counter->inc(3.0, {"c"});
    z_counter->inc(1.0, {"a"});

    const std::string prometheus_a = registry.exportPrometheus();
    const std::string prometheus_b = registry.exportPrometheus();
    EXPECT_EQ(prometheus_a, prometheus_b);

    auto rows = registry.snapshotSamples();
    ASSERT_EQ(rows.size(), 4u);

    EXPECT_EQ(rows[0].metric_name, "a_metric");
    EXPECT_EQ(labelsKey(rows[0].labels), "slot=a");
    EXPECT_EQ(rows[1].metric_name, "a_metric");
    EXPECT_EQ(labelsKey(rows[1].labels), "slot=b");
    EXPECT_EQ(rows[2].metric_name, "z_metric_total");
    EXPECT_EQ(labelsKey(rows[2].labels), "slot=a");
    EXPECT_EQ(rows[3].metric_name, "z_metric_total");
    EXPECT_EQ(labelsKey(rows[3].labels), "slot=c");
}

TEST(TelemetryReportingContractTest, CanonicalJsonShapeAndReplayAreDeterministic)
{
    auto& registry = MetricsRegistry::getInstance();
    registry.clear();

    auto* counter = registry.registerCounter("scratchbird_vnext_optimizer_events_total",
                                             "optimizer events",
                                             {"event", "outcome", "code"});
    ASSERT_NE(counter, nullptr);

    counter->inc(1.0, {"plan_select", "ok", "NONE"});
    counter->inc(2.0, {"plan_score", "reject", "OPT_0306"});

    const std::string json_a = registry.exportCanonicalJson(1700000000);
    const std::string json_b = registry.exportCanonicalJson(1700000000);
    EXPECT_EQ(json_a, json_b);

    const auto parsed = nlohmann::json::parse(json_a);
    ASSERT_TRUE(parsed.contains("schema"));
    ASSERT_TRUE(parsed.contains("sample_count"));
    ASSERT_TRUE(parsed.contains("samples"));
    EXPECT_EQ(parsed.at("schema"), "ScratchBirdMetricSnapshotV1");
    EXPECT_EQ(parsed.at("generated_at_epoch_ms"), 1700000000);
    EXPECT_EQ(parsed.at("sample_count"), 2);
    EXPECT_EQ(parsed.at("samples").size(), 2u);
}

TEST(TelemetryReportingContractTest, SloEvaluationUsesDeterministicWorstCaseByComparator)
{
    const std::vector<SloObservation> observations{
        {"Transaction commit", "p95 latency", 4.5, "ms"},
        {"Parser latency", "p95", 2.0, "ms"},
        {"Parser latency", "p95", 5.5, "ms"},
        {"Columnar scan", "throughput", 1.4, "GB/s"},
        {"Columnar scan", "throughput", 0.8, "GB/s"},
    };

    std::vector<SloBaselineEvaluation> evaluations;
    ConformanceTelemetry::evaluateSloBaselines(observations, evaluations);

    auto find_eval = [&evaluations](const std::string& domain, const std::string& metric)
        -> const SloBaselineEvaluation* {
        for (const auto& eval : evaluations)
        {
            if (eval.domain == domain && eval.metric == metric)
            {
                return &eval;
            }
        }
        return nullptr;
    };

    const auto* parser_eval = find_eval("Parser latency", "p95");
    ASSERT_NE(parser_eval, nullptr);
    EXPECT_TRUE(parser_eval->has_observed_value);
    EXPECT_DOUBLE_EQ(parser_eval->observed_value, 5.5);
    EXPECT_EQ(parser_eval->status, SloBaselineStatus::FAIL);

    const auto* columnar_eval = find_eval("Columnar scan", "throughput");
    ASSERT_NE(columnar_eval, nullptr);
    EXPECT_TRUE(columnar_eval->has_observed_value);
    EXPECT_DOUBLE_EQ(columnar_eval->observed_value, 0.8);
    EXPECT_EQ(columnar_eval->status, SloBaselineStatus::FAIL);

    const auto* vector_eval = find_eval("Vector query", "p95 latency");
    ASSERT_NE(vector_eval, nullptr);
    EXPECT_FALSE(vector_eval->has_observed_value);
    EXPECT_EQ(vector_eval->status, SloBaselineStatus::NO_DATA);
}

TEST(TelemetryReportingContractTest, BaselineReportReplayAndHashAreStable)
{
    const std::vector<SloObservation> observations_a{
        {"Transaction commit", "p95 latency", 5.0, "ms"},
        {"Transaction rollback", "p95 latency", 3.0, "ms"},
        {"Optimizer planning", "p95 latency single-track", 10.0, "ms"},
        {"Parser latency", "p95", 2.5, "ms"},
        {"Time-series ingest", "throughput", 150000.0, "points/s"},
        {"Columnar scan", "throughput", 1.2, "GB/s"},
        {"Search query", "p95 latency", 20.0, "ms"},
        {"Vector query", "p95 latency", 30.0, "ms"},
    };

    auto observations_b = observations_a;
    std::reverse(observations_b.begin(), observations_b.end());

    const std::string report_a =
        ConformanceTelemetry::buildBaselineReportJson(observations_a, "REF-HW-V1", 1700000100);
    const std::string report_b =
        ConformanceTelemetry::buildBaselineReportJson(observations_b, "REF-HW-V1", 1700000100);
    EXPECT_EQ(report_a, report_b);

    const std::string hash_a = ConformanceTelemetry::sha256Hex(report_a);
    const std::string hash_b = ConformanceTelemetry::sha256Hex(report_b);
    EXPECT_EQ(hash_a, hash_b);
    EXPECT_EQ(hash_a.size(), 64u);

    const auto parsed = nlohmann::json::parse(report_a);
    EXPECT_EQ(parsed.at("overall_status"), "PASS");
    EXPECT_EQ(parsed.at("reference_hardware_profile_id"), "REF-HW-V1");
}
