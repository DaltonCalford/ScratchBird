/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace scratchbird::optimizer
{

    enum class QueryTrack : uint8_t
    {
        RELATIONAL_TRACK = 0,
        DOCUMENT_TRACK = 1,
        TIMESERIES_TRACK = 2,
        COLUMNAR_TRACK = 3,
        SEARCH_TRACK = 4,
        VECTOR_TRACK = 5,
        HYBRID_TRACK = 6,
    };

    enum class OperatorClass : uint8_t
    {
        SEARCH_OP = 0,
        VECTOR_OP = 1,
        TIMESERIES_OP = 2,
        COLUMNAR_OP = 3,
        DOCUMENT_OP = 4,
        RELATIONAL_OP = 5,
    };

    enum class StorageFamily : uint8_t
    {
        UNKNOWN = 0,
        RELATIONAL = 1,
        DOCUMENT = 2,
        TIMESERIES = 3,
        COLUMNAR = 4,
        SEARCH = 5,
        VECTOR = 6,
    };

    struct TrackClassificationInput
    {
        StorageFamily storage_family = StorageFamily::UNKNOWN;
        std::vector<OperatorClass> operator_classes;
        std::vector<std::string> statement_operators;
        std::vector<std::string> backend_available_operators;
        bool bounded_time_constraint = true;
        bool allow_hybrid_fallback = true;
    };

    struct TrackClassificationResult
    {
        bool ok = false;
        QueryTrack selected_track = QueryTrack::RELATIONAL_TRACK;
        std::vector<QueryTrack> candidates;
        std::array<uint32_t, 6> primary_scores{};
        std::string error_code;
        std::string error_message;
    };

    class MultiTrackClassifier
    {
    public:
        static auto classify(const TrackClassificationInput &input)
            -> TrackClassificationResult;

        static auto trackToString(QueryTrack track)
            -> const char *;
    };

} // namespace scratchbird::optimizer

