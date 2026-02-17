#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

#include "scratchbird/core/telemetry.h"
#include "scratchbird/optimizer/multi_track_classifier.h"

using scratchbird::optimizer::MultiTrackClassifier;
using scratchbird::optimizer::OperatorClass;
using scratchbird::optimizer::QueryTrack;
using scratchbird::optimizer::StorageFamily;
using scratchbird::optimizer::TrackClassificationInput;

namespace
{
auto metricCounterValue(const std::string& metric_name,
                        const std::vector<std::string>& labels) -> double
{
    auto* metric = scratchbird::core::MetricsRegistry::getInstance().get(metric_name);
    if (metric == nullptr)
    {
        return 0.0;
    }
    auto* counter = dynamic_cast<scratchbird::core::Counter*>(metric);
    if (counter == nullptr)
    {
        return 0.0;
    }
    return counter->get(labels);
}

auto commonBackendOps() -> std::vector<std::string>
{
    return {
        "scan", "filter", "join", "group", "sort",
        "doc_scan", "path_filter", "doc_project",
        "ts_chunk_scan", "time_bucket_agg",
        "col_scan", "zone_prune", "vector_agg",
        "postings_scan", "scorer_eval", "docvalues_fetch",
        "ann_search", "vector_rerank",
        "bridge_exchange", "bridge_materialize"};
}
} // namespace

TEST(OptimizerMultiTrackClassificationTest, RelationalClassifiesDeterministically)
{
    TrackClassificationInput input;
    input.storage_family = StorageFamily::RELATIONAL;
    input.operator_classes = {OperatorClass::RELATIONAL_OP, OperatorClass::RELATIONAL_OP};
    input.statement_operators = {"scan", "filter", "join", "group", "sort"};
    input.backend_available_operators = commonBackendOps();

    auto result = MultiTrackClassifier::classify(input);
    ASSERT_TRUE(result.ok) << result.error_code << ": " << result.error_message;
    EXPECT_EQ(QueryTrack::RELATIONAL_TRACK, result.selected_track);
}

TEST(OptimizerMultiTrackClassificationTest, SearchVectorTieUsesPrecedenceDeterministically)
{
    TrackClassificationInput input;
    input.storage_family = StorageFamily::SEARCH;
    input.operator_classes = {OperatorClass::SEARCH_OP, OperatorClass::VECTOR_OP};
    input.statement_operators = {"postings_scan", "scorer_eval", "docvalues_fetch"};
    input.backend_available_operators = commonBackendOps();

    auto result = MultiTrackClassifier::classify(input);
    ASSERT_TRUE(result.ok) << result.error_code << ": " << result.error_message;
    EXPECT_EQ(QueryTrack::SEARCH_TRACK, result.selected_track);
}

TEST(OptimizerMultiTrackClassificationTest, IncompatibleSearchRelationalUsesHybridTrack)
{
    TrackClassificationInput input;
    input.storage_family = StorageFamily::RELATIONAL;
    input.operator_classes = {OperatorClass::SEARCH_OP, OperatorClass::RELATIONAL_OP,
                              OperatorClass::RELATIONAL_OP};
    input.statement_operators = {"bridge_exchange", "bridge_materialize"};
    input.backend_available_operators = commonBackendOps();

    auto result = MultiTrackClassifier::classify(input);
    ASSERT_TRUE(result.ok) << result.error_code << ": " << result.error_message;
    EXPECT_EQ(QueryTrack::HYBRID_TRACK, result.selected_track);
}

TEST(OptimizerMultiTrackClassificationTest, UnboundedTimeseriesRelationalUsesHybridTrack)
{
    TrackClassificationInput input;
    input.storage_family = StorageFamily::TIMESERIES;
    input.operator_classes = {OperatorClass::TIMESERIES_OP, OperatorClass::RELATIONAL_OP,
                              OperatorClass::RELATIONAL_OP};
    input.bounded_time_constraint = false;
    input.statement_operators = {"bridge_exchange", "bridge_materialize"};
    input.backend_available_operators = commonBackendOps();

    auto result = MultiTrackClassifier::classify(input);
    ASSERT_TRUE(result.ok) << result.error_code << ": " << result.error_message;
    EXPECT_EQ(QueryTrack::HYBRID_TRACK, result.selected_track);
}

TEST(OptimizerMultiTrackClassificationTest, MissingCandidatesRejectsWithOPT0301)
{
    TrackClassificationInput input;
    input.storage_family = StorageFamily::UNKNOWN;
    input.backend_available_operators = commonBackendOps();

    auto result = MultiTrackClassifier::classify(input);
    ASSERT_FALSE(result.ok);
    EXPECT_EQ("OPT_0301", result.error_code);
}

TEST(OptimizerMultiTrackClassificationTest, ForbiddenOperatorRejectsWithOPT0303)
{
    TrackClassificationInput input;
    input.storage_family = StorageFamily::RELATIONAL;
    input.operator_classes = {OperatorClass::RELATIONAL_OP, OperatorClass::RELATIONAL_OP};
    input.statement_operators = {"scan", "filter", "join", "group", "sort", "search_dsl_eval"};
    input.backend_available_operators = commonBackendOps();
    input.allow_hybrid_fallback = false;

    auto result = MultiTrackClassifier::classify(input);
    ASSERT_FALSE(result.ok);
    EXPECT_EQ("OPT_0303", result.error_code);
}

TEST(OptimizerMultiTrackClassificationTest, MissingBackendOperatorRejectsWithOPT0305)
{
    TrackClassificationInput input;
    input.storage_family = StorageFamily::VECTOR;
    input.operator_classes = {OperatorClass::VECTOR_OP, OperatorClass::VECTOR_OP};
    input.statement_operators = {"ann_search", "vector_rerank"};
    input.backend_available_operators = {"vector_rerank"};
    input.allow_hybrid_fallback = false;

    auto result = MultiTrackClassifier::classify(input);
    ASSERT_FALSE(result.ok);
    EXPECT_EQ("OPT_0305", result.error_code);
}

TEST(OptimizerMultiTrackClassificationTest, IllegalHybridPathRejectsWithOPT0304)
{
    TrackClassificationInput input;
    input.storage_family = StorageFamily::RELATIONAL;
    input.operator_classes = {OperatorClass::SEARCH_OP, OperatorClass::RELATIONAL_OP,
                              OperatorClass::RELATIONAL_OP};
    input.statement_operators = {"scan", "filter", "join", "group", "sort"};
    input.backend_available_operators = commonBackendOps();
    input.allow_hybrid_fallback = true;

    auto result = MultiTrackClassifier::classify(input);
    ASSERT_FALSE(result.ok);
    EXPECT_EQ("OPT_0304", result.error_code);
}

TEST(OptimizerMultiTrackClassificationTest, MetricsEmissionTracksClassifyOutcomes)
{
    const std::string metric = "scratchbird_vnext_optimizer_events_total";
    const double ok_before = metricCounterValue(metric, {"track_classify", "ok", "NONE"});
    const double reject_before = metricCounterValue(metric, {"track_classify", "reject", "OPT_0301"});

    TrackClassificationInput success_input;
    success_input.storage_family = StorageFamily::RELATIONAL;
    success_input.operator_classes = {OperatorClass::RELATIONAL_OP, OperatorClass::RELATIONAL_OP};
    success_input.statement_operators = {"scan", "filter", "join", "group", "sort"};
    success_input.backend_available_operators = commonBackendOps();
    auto success_result = MultiTrackClassifier::classify(success_input);
    ASSERT_TRUE(success_result.ok);

    TrackClassificationInput reject_input;
    reject_input.storage_family = StorageFamily::UNKNOWN;
    reject_input.backend_available_operators = commonBackendOps();
    auto reject_result = MultiTrackClassifier::classify(reject_input);
    ASSERT_FALSE(reject_result.ok);
    EXPECT_EQ("OPT_0301", reject_result.error_code);

    EXPECT_EQ(ok_before + 1.0, metricCounterValue(metric, {"track_classify", "ok", "NONE"}));
    EXPECT_EQ(reject_before + 1.0,
              metricCounterValue(metric, {"track_classify", "reject", "OPT_0301"}));
}
