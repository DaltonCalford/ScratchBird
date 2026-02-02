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

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{
    class Database;

    /**
     * Global uniqueness index for domains
     *
     * Tracks unique values across all columns using a domain with UNIQUENESS = TRUE.
     * Visibility is MGA-aware via transaction IDs and deletion markers.
     */
    class GlobalUniquenessIndex
    {
    public:
        explicit GlobalUniquenessIndex(Database *db);
        ~GlobalUniquenessIndex();

        Status checkUniqueness(const ID &domain_id,
                               const TypedValue &value,
                               uint64_t tx_id,
                               bool &is_unique_out,
                               ErrorContext *ctx = nullptr);

        Status insertValue(const ID &domain_id,
                           const ID &table_id,
                           const ID &column_id,
                           const TID &row_tid,
                           const TypedValue &value,
                           uint64_t tx_id,
                           ErrorContext *ctx = nullptr);

        Status deleteValue(const ID &domain_id,
                           const ID &table_id,
                           const ID &column_id,
                           const TID &row_tid,
                           const TypedValue &value,
                           uint64_t tx_id,
                           ErrorContext *ctx = nullptr);

        Status updateValue(const ID &domain_id,
                           const ID &table_id,
                           const ID &column_id,
                           const TID &row_tid,
                           const TypedValue &old_value,
                           const TypedValue &new_value,
                           uint64_t tx_id,
                           ErrorContext *ctx = nullptr);

        Status enableUniqueness(const ID &domain_id, ErrorContext *ctx = nullptr);
        Status disableUniqueness(const ID &domain_id, ErrorContext *ctx = nullptr);

    private:
        struct ValueKey
        {
            DataType type = DataType::UNKNOWN;
            bool is_null = false;
            std::vector<uint8_t> data;
        };

        struct ValueLocation
        {
            ID table_id;
            ID column_id;
            TID row_tid;
            uint64_t xmin = 0;
            uint64_t xmax = 0;
        };

        struct ValueKeyHash
        {
            size_t operator()(const ValueKey &key) const;
        };

        struct ValueKeyEqual
        {
            bool operator()(const ValueKey &lhs, const ValueKey &rhs) const;
        };

        using ValueMap = std::unordered_map<ValueKey, std::vector<ValueLocation>, ValueKeyHash, ValueKeyEqual>;

        Database *db_;
        std::unordered_map<ID, ValueMap, IDHash> index_;
        std::unordered_set<ID, IDHash> enabled_domains_;
        std::mutex mutex_;

        Status buildValueKey(const TypedValue &value, ValueKey &key_out, ErrorContext *ctx) const;
        bool isLocationVisible(const ValueLocation &loc, uint64_t current_xid) const;
        bool isEnabled(const ID &domain_id) const;
    };
} // namespace scratchbird::core
