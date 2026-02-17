/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/optimizer/multi_track_classifier.h"
#include "scratchbird/core/vnext_metrics_event_model.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>
#include <unordered_set>

namespace scratchbird::optimizer
{
    namespace
    {
        constexpr size_t kPrimaryTrackCount = 6;

        auto toPrimaryIndex(QueryTrack track) -> std::optional<size_t>
        {
            switch (track)
            {
                case QueryTrack::RELATIONAL_TRACK: return 0;
                case QueryTrack::DOCUMENT_TRACK: return 1;
                case QueryTrack::TIMESERIES_TRACK: return 2;
                case QueryTrack::COLUMNAR_TRACK: return 3;
                case QueryTrack::SEARCH_TRACK: return 4;
                case QueryTrack::VECTOR_TRACK: return 5;
                case QueryTrack::HYBRID_TRACK: return std::nullopt;
            }
            return std::nullopt;
        }

        auto fromPrimaryIndex(size_t index) -> QueryTrack
        {
            switch (index)
            {
                case 0: return QueryTrack::RELATIONAL_TRACK;
                case 1: return QueryTrack::DOCUMENT_TRACK;
                case 2: return QueryTrack::TIMESERIES_TRACK;
                case 3: return QueryTrack::COLUMNAR_TRACK;
                case 4: return QueryTrack::SEARCH_TRACK;
                case 5: return QueryTrack::VECTOR_TRACK;
                default: return QueryTrack::RELATIONAL_TRACK;
            }
        }

        auto storageToTrack(StorageFamily family) -> std::optional<QueryTrack>
        {
            switch (family)
            {
                case StorageFamily::RELATIONAL: return QueryTrack::RELATIONAL_TRACK;
                case StorageFamily::DOCUMENT: return QueryTrack::DOCUMENT_TRACK;
                case StorageFamily::TIMESERIES: return QueryTrack::TIMESERIES_TRACK;
                case StorageFamily::COLUMNAR: return QueryTrack::COLUMNAR_TRACK;
                case StorageFamily::SEARCH: return QueryTrack::SEARCH_TRACK;
                case StorageFamily::VECTOR: return QueryTrack::VECTOR_TRACK;
                case StorageFamily::UNKNOWN: return std::nullopt;
            }
            return std::nullopt;
        }

        auto classToTrack(OperatorClass klass) -> QueryTrack
        {
            switch (klass)
            {
                case OperatorClass::SEARCH_OP: return QueryTrack::SEARCH_TRACK;
                case OperatorClass::VECTOR_OP: return QueryTrack::VECTOR_TRACK;
                case OperatorClass::TIMESERIES_OP: return QueryTrack::TIMESERIES_TRACK;
                case OperatorClass::COLUMNAR_OP: return QueryTrack::COLUMNAR_TRACK;
                case OperatorClass::DOCUMENT_OP: return QueryTrack::DOCUMENT_TRACK;
                case OperatorClass::RELATIONAL_OP: return QueryTrack::RELATIONAL_TRACK;
            }
            return QueryTrack::RELATIONAL_TRACK;
        }

        auto classWeight(OperatorClass klass) -> uint32_t
        {
            switch (klass)
            {
                case OperatorClass::SEARCH_OP: return 5;
                case OperatorClass::VECTOR_OP: return 5;
                case OperatorClass::TIMESERIES_OP: return 4;
                case OperatorClass::COLUMNAR_OP: return 3;
                case OperatorClass::DOCUMENT_OP: return 3;
                case OperatorClass::RELATIONAL_OP: return 2;
            }
            return 0;
        }

        auto precedenceRank(QueryTrack track) -> int
        {
            switch (track)
            {
                case QueryTrack::SEARCH_TRACK: return 0;
                case QueryTrack::VECTOR_TRACK: return 1;
                case QueryTrack::TIMESERIES_TRACK: return 2;
                case QueryTrack::COLUMNAR_TRACK: return 3;
                case QueryTrack::DOCUMENT_TRACK: return 4;
                case QueryTrack::RELATIONAL_TRACK: return 5;
                case QueryTrack::HYBRID_TRACK: return 6;
            }
            return 6;
        }

        auto normalizeOperatorName(const std::string &name) -> std::string
        {
            std::string normalized;
            normalized.reserve(name.size());
            for (char ch : name)
            {
                if (ch == '-' || std::isspace(static_cast<unsigned char>(ch)))
                {
                    normalized.push_back('_');
                }
                else
                {
                    normalized.push_back(
                        static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
                }
            }
            return normalized;
        }

        auto makeOperatorSet(const std::vector<std::string> &operators)
            -> std::unordered_set<std::string>
        {
            std::unordered_set<std::string> out;
            out.reserve(operators.size());
            for (const std::string &op : operators)
            {
                out.insert(normalizeOperatorName(op));
            }
            return out;
        }

        auto requiredOperators(QueryTrack track) -> std::vector<std::string_view>
        {
            switch (track)
            {
                case QueryTrack::RELATIONAL_TRACK:
                    return {"scan", "filter", "join", "group", "sort"};
                case QueryTrack::DOCUMENT_TRACK:
                    return {"doc_scan", "path_filter", "doc_project"};
                case QueryTrack::TIMESERIES_TRACK:
                    return {"ts_chunk_scan", "time_bucket_agg"};
                case QueryTrack::COLUMNAR_TRACK:
                    return {"col_scan", "zone_prune", "vector_agg"};
                case QueryTrack::SEARCH_TRACK:
                    return {"postings_scan", "scorer_eval", "docvalues_fetch"};
                case QueryTrack::VECTOR_TRACK:
                    return {"ann_search", "vector_rerank"};
                case QueryTrack::HYBRID_TRACK:
                    return {"bridge_exchange", "bridge_materialize"};
            }
            return {};
        }

        auto forbiddenOperators(QueryTrack track) -> std::vector<std::string_view>
        {
            switch (track)
            {
                case QueryTrack::RELATIONAL_TRACK:
                    return {"search_dsl_eval", "ann_search"};
                case QueryTrack::DOCUMENT_TRACK:
                    return {"ann_search"};
                case QueryTrack::TIMESERIES_TRACK:
                    return {"generic_hash_join_unbounded"};
                case QueryTrack::COLUMNAR_TRACK:
                    return {"row_store_update"};
                case QueryTrack::SEARCH_TRACK:
                    return {"relational_join_primary"};
                case QueryTrack::VECTOR_TRACK:
                    return {"full_table_scan_first"};
                case QueryTrack::HYBRID_TRACK:
                    return {"implicit_bridge_insertion"};
            }
            return {};
        }

        auto dominates(uint32_t score_a, uint32_t score_b) -> bool
        {
            const uint32_t dominance_threshold = (score_b * 5 + 3) / 4;
            return score_a >= dominance_threshold && score_a >= score_b + 3;
        }

        struct ValidationResult
        {
            bool ok = true;
            std::string code;
            std::string message;
        };

        auto validateTrack(QueryTrack track,
                           const std::unordered_set<std::string> &statement_ops,
                           const std::unordered_set<std::string> &backend_ops)
            -> ValidationResult
        {
            const std::vector<std::string_view> forbidden = forbiddenOperators(track);
            for (std::string_view op : forbidden)
            {
                if (statement_ops.find(std::string(op)) != statement_ops.end())
                {
                    return {false, "OPT_0303",
                            std::string("Unsupported operator for selected track: ") +
                                std::string(op)};
                }
            }

            const std::vector<std::string_view> required = requiredOperators(track);
            if (!statement_ops.empty())
            {
                for (std::string_view op : required)
                {
                    if (statement_ops.find(std::string(op)) == statement_ops.end())
                    {
                        return {false, "OPT_0303",
                                std::string("Missing required operator for selected track: ") +
                                    std::string(op)};
                    }
                }
            }

            if (!backend_ops.empty())
            {
                for (std::string_view op : required)
                {
                    if (backend_ops.find(std::string(op)) == backend_ops.end())
                    {
                        return {false, "OPT_0305",
                                std::string("Required operator backend unavailable: ") +
                                    std::string(op)};
                    }
                }
            }
            return {};
        }

        auto isIncompatiblePair(QueryTrack a, QueryTrack b, bool bounded_time_constraint) -> bool
        {
            if ((a == QueryTrack::SEARCH_TRACK && b == QueryTrack::RELATIONAL_TRACK) ||
                (a == QueryTrack::RELATIONAL_TRACK && b == QueryTrack::SEARCH_TRACK))
            {
                return true;
            }
            if ((a == QueryTrack::SEARCH_TRACK && b == QueryTrack::DOCUMENT_TRACK) ||
                (a == QueryTrack::DOCUMENT_TRACK && b == QueryTrack::SEARCH_TRACK))
            {
                return true;
            }
            if ((a == QueryTrack::VECTOR_TRACK && b == QueryTrack::RELATIONAL_TRACK) ||
                (a == QueryTrack::RELATIONAL_TRACK && b == QueryTrack::VECTOR_TRACK))
            {
                return true;
            }
            if ((a == QueryTrack::VECTOR_TRACK && b == QueryTrack::DOCUMENT_TRACK) ||
                (a == QueryTrack::DOCUMENT_TRACK && b == QueryTrack::VECTOR_TRACK))
            {
                return true;
            }
            if (!bounded_time_constraint &&
                ((a == QueryTrack::TIMESERIES_TRACK && b == QueryTrack::RELATIONAL_TRACK) ||
                 (a == QueryTrack::RELATIONAL_TRACK && b == QueryTrack::TIMESERIES_TRACK)))
            {
                return true;
            }
            return false;
        }

        auto bridgeOperatorCount(QueryTrack selected,
                                 const std::optional<QueryTrack> &storage_track) -> uint32_t
        {
            if (!storage_track || *storage_track == selected || selected == QueryTrack::HYBRID_TRACK)
            {
                return 0;
            }
            return 1;
        }

        auto chooseByTieBreak(const std::vector<QueryTrack> &candidates,
                              const std::optional<QueryTrack> &storage_track) -> QueryTrack
        {
            QueryTrack best = candidates.front();
            for (size_t i = 1; i < candidates.size(); ++i)
            {
                QueryTrack candidate = candidates[i];
                const int best_rank = precedenceRank(best);
                const int candidate_rank = precedenceRank(candidate);
                if (candidate_rank < best_rank)
                {
                    best = candidate;
                    continue;
                }
                if (candidate_rank > best_rank)
                {
                    continue;
                }

                const uint32_t best_bridge = bridgeOperatorCount(best, storage_track);
                const uint32_t candidate_bridge = bridgeOperatorCount(candidate, storage_track);
                if (candidate_bridge < best_bridge)
                {
                    best = candidate;
                    continue;
                }
                if (candidate_bridge > best_bridge)
                {
                    continue;
                }

                const std::string best_name = MultiTrackClassifier::trackToString(best);
                const std::string candidate_name = MultiTrackClassifier::trackToString(candidate);
                if (candidate_name < best_name)
                {
                    best = candidate;
                }
            }
            return best;
        }
    } // namespace

    auto MultiTrackClassifier::classify(const TrackClassificationInput &input)
        -> TrackClassificationResult
    {
        TrackClassificationResult result;

        for (OperatorClass klass : input.operator_classes)
        {
            const QueryTrack track = classToTrack(klass);
            const std::optional<size_t> index = toPrimaryIndex(track);
            if (!index)
            {
                continue;
            }
            result.primary_scores[*index] += classWeight(klass);
        }

        const std::optional<QueryTrack> storage_track = storageToTrack(input.storage_family);
        std::vector<QueryTrack> scored_tracks;
        scored_tracks.reserve(kPrimaryTrackCount);
        for (size_t i = 0; i < kPrimaryTrackCount; ++i)
        {
            if (result.primary_scores[i] > 0)
            {
                scored_tracks.push_back(fromPrimaryIndex(i));
            }
        }

        if (scored_tracks.empty())
        {
            if (storage_track)
            {
                result.candidates.push_back(*storage_track);
            }
            else
            {
                result.error_code = "OPT_0301";
                result.error_message = "No valid track classification candidates";
                core::VNextMetricsEventModel::recordOptimizerEvent(
                    "track_classify", "reject", result.error_code);
                return result;
            }
        }
        else
        {
            for (QueryTrack candidate : scored_tracks)
            {
                const size_t candidate_idx = *toPrimaryIndex(candidate);
                bool dominated = false;
                for (QueryTrack peer : scored_tracks)
                {
                    if (peer == candidate)
                    {
                        continue;
                    }
                    const size_t peer_idx = *toPrimaryIndex(peer);
                    if (dominates(result.primary_scores[peer_idx], result.primary_scores[candidate_idx]))
                    {
                        dominated = true;
                        break;
                    }
                }
                if (!dominated)
                {
                    result.candidates.push_back(candidate);
                }
            }

            if (storage_track)
            {
                const auto it = std::find(result.candidates.begin(), result.candidates.end(),
                                          *storage_track);
                if (it == result.candidates.end())
                {
                    result.candidates.push_back(*storage_track);
                }
            }
        }

        if (result.candidates.empty())
        {
            result.error_code = "OPT_0301";
            result.error_message = "No valid track classification candidates";
            core::VNextMetricsEventModel::recordOptimizerEvent(
                "track_classify", "reject", result.error_code);
            return result;
        }

        QueryTrack selected = result.candidates.front();
        if (result.candidates.size() > 1)
        {
            bool incompatible = false;
            for (size_t i = 0; i < result.candidates.size() && !incompatible; ++i)
            {
                for (size_t j = i + 1; j < result.candidates.size() && !incompatible; ++j)
                {
                    incompatible = isIncompatiblePair(result.candidates[i],
                                                      result.candidates[j],
                                                      input.bounded_time_constraint);
                }
            }

            if (incompatible)
            {
                if (!input.allow_hybrid_fallback)
                {
                    result.error_code = "OPT_0304";
                    result.error_message =
                        "Incompatible multi-primary classification without legal hybrid path";
                    core::VNextMetricsEventModel::recordOptimizerEvent(
                        "track_classify", "reject", result.error_code);
                    return result;
                }
                selected = QueryTrack::HYBRID_TRACK;
            }
            else
            {
                selected = chooseByTieBreak(result.candidates, storage_track);
            }
        }

        const std::unordered_set<std::string> statement_ops =
            makeOperatorSet(input.statement_operators);
        const std::unordered_set<std::string> backend_ops =
            makeOperatorSet(input.backend_available_operators);

        ValidationResult validation = validateTrack(selected, statement_ops, backend_ops);
        if (!validation.ok)
        {
            if (selected == QueryTrack::HYBRID_TRACK)
            {
                result.error_code = "OPT_0304";
                result.error_message =
                    "Incompatible multi-primary classification without legal hybrid path";
                return result;
            }

            if (!input.allow_hybrid_fallback)
            {
                result.error_code = validation.code;
                result.error_message = validation.message;
                core::VNextMetricsEventModel::recordOptimizerEvent(
                    "track_classify", "reject", result.error_code);
                return result;
            }

            ValidationResult hybrid_validation =
                validateTrack(QueryTrack::HYBRID_TRACK, statement_ops, backend_ops);
            if (!hybrid_validation.ok)
            {
                result.error_code = "OPT_0304";
                result.error_message =
                    "Incompatible multi-primary classification without legal hybrid path";
                core::VNextMetricsEventModel::recordOptimizerEvent(
                    "track_classify", "reject", result.error_code);
                return result;
            }
            selected = QueryTrack::HYBRID_TRACK;
        }

        result.ok = true;
        result.selected_track = selected;
        core::VNextMetricsEventModel::recordOptimizerEvent(
            "track_classify", "ok", "NONE");
        return result;
    }

    auto MultiTrackClassifier::trackToString(QueryTrack track) -> const char *
    {
        switch (track)
        {
            case QueryTrack::RELATIONAL_TRACK: return "RELATIONAL_TRACK";
            case QueryTrack::DOCUMENT_TRACK: return "DOCUMENT_TRACK";
            case QueryTrack::TIMESERIES_TRACK: return "TIMESERIES_TRACK";
            case QueryTrack::COLUMNAR_TRACK: return "COLUMNAR_TRACK";
            case QueryTrack::SEARCH_TRACK: return "SEARCH_TRACK";
            case QueryTrack::VECTOR_TRACK: return "VECTOR_TRACK";
            case QueryTrack::HYBRID_TRACK: return "HYBRID_TRACK";
        }
        return "RELATIONAL_TRACK";
    }

} // namespace scratchbird::optimizer
