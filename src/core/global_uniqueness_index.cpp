/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/global_uniqueness_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/transaction_manager.h"
#include <limits>

namespace scratchbird::core
{
    namespace
    {
        size_t hashBytes(const uint8_t *data, size_t length, size_t seed)
        {
            constexpr size_t kPrime = 1099511628211ULL;
            size_t hash = seed ^ 1469598103934665603ULL;
            for (size_t i = 0; i < length; ++i)
            {
                hash ^= static_cast<size_t>(data[i]);
                hash *= kPrime;
            }
            return hash;
        }
    } // namespace

    GlobalUniquenessIndex::GlobalUniquenessIndex(Database *db)
        : db_(db)
    {
    }

    GlobalUniquenessIndex::~GlobalUniquenessIndex() = default;

    size_t GlobalUniquenessIndex::ValueKeyHash::operator()(const ValueKey &key) const
    {
        size_t hash = 0;
        uint16_t type_val = static_cast<uint16_t>(key.type);
        hash = hashBytes(reinterpret_cast<const uint8_t *>(&type_val), sizeof(type_val), hash);

        uint8_t null_flag = key.is_null ? 1 : 0;
        hash = hashBytes(&null_flag, sizeof(null_flag), hash);

        if (!key.data.empty())
        {
            hash = hashBytes(key.data.data(), key.data.size(), hash);
        }
        return hash;
    }

    bool GlobalUniquenessIndex::ValueKeyEqual::operator()(const ValueKey &lhs,
                                                          const ValueKey &rhs) const
    {
        return lhs.type == rhs.type &&
               lhs.is_null == rhs.is_null &&
               lhs.data == rhs.data;
    }

    bool GlobalUniquenessIndex::isEnabled(const ID &domain_id) const
    {
        return enabled_domains_.find(domain_id) != enabled_domains_.end();
    }

    Status GlobalUniquenessIndex::buildValueKey(const TypedValue &value,
                                                ValueKey &key_out,
                                                ErrorContext *ctx) const
    {
        if (value.isEncrypted())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Value must be decrypted");
            return Status::INVALID_ARGUMENT;
        }

        key_out.type = value.type();
        key_out.is_null = value.isNull();
        key_out.data.clear();

        if (value.isNull())
        {
            return Status::OK;
        }

        Status status = value.serializePlainValue(key_out.data, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        return Status::OK;
    }

    bool GlobalUniquenessIndex::isLocationVisible(const ValueLocation &loc,
                                                  uint64_t current_xid) const
    {
        if (db_ == nullptr)
        {
            return true;
        }

        if (TransactionManager *txn_mgr = db_->transaction_manager(); txn_mgr != nullptr)
        {
            // Domain uniqueness checks must use transaction inventory truth directly and
            // must not inherit connection snapshot state.
            return txn_mgr->isInventoryRecordVisible(loc.xmin, loc.xmax, current_xid);
        }

        if (db_->storage_engine() == nullptr)
        {
            return true;
        }

        return db_->storage_engine()->isVisible(loc.xmin, loc.xmax, current_xid);
    }

    Status GlobalUniquenessIndex::checkUniqueness(const ID &domain_id,
                                                  const TypedValue &value,
                                                  uint64_t tx_id,
                                                  bool &is_unique_out,
                                                  ErrorContext *ctx)
    {
        is_unique_out = true;

        if (tx_id == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Transaction ID required");
            return Status::INVALID_ARGUMENT;
        }

        if (value.isNull())
        {
            return Status::OK;
        }

        ValueKey key;
        Status status = buildValueKey(value, key, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (!isEnabled(domain_id))
        {
            return Status::OK;
        }

        auto domain_it = index_.find(domain_id);
        if (domain_it == index_.end())
        {
            return Status::OK;
        }

        auto value_it = domain_it->second.find(key);
        if (value_it == domain_it->second.end())
        {
            return Status::OK;
        }

        for (const auto &loc : value_it->second)
        {
            if (isLocationVisible(loc, tx_id))
            {
                is_unique_out = false;
                return Status::OK;
            }
        }

        return Status::OK;
    }

    Status GlobalUniquenessIndex::insertValue(const ID &domain_id,
                                              const ID &table_id,
                                              const ID &column_id,
                                              const TID &row_tid,
                                              const TypedValue &value,
                                              uint64_t tx_id,
                                              ErrorContext *ctx)
    {
        if (tx_id == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Transaction ID required");
            return Status::INVALID_ARGUMENT;
        }

        if (value.isNull())
        {
            return Status::OK;
        }

        ValueKey key;
        Status status = buildValueKey(value, key, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (!isEnabled(domain_id))
        {
            return Status::OK;
        }

        auto &domain_map = index_[domain_id];
        auto &locations = domain_map[key];

        for (const auto &loc : locations)
        {
            if (loc.table_id == table_id &&
                loc.column_id == column_id &&
                loc.row_tid == row_tid &&
                loc.xmax == 0 &&
                loc.xmin == tx_id)
            {
                return Status::OK;
            }

            if (isLocationVisible(loc, tx_id))
            {
                SET_ERROR_CONTEXT(ctx, Status::UNIQUE_VIOLATION,
                                  "Domain uniqueness violation");
                return Status::UNIQUE_VIOLATION;
            }
        }

        ValueLocation loc;
        loc.table_id = table_id;
        loc.column_id = column_id;
        loc.row_tid = row_tid;
        loc.xmin = tx_id;
        loc.xmax = 0;
        locations.push_back(std::move(loc));

        return Status::OK;
    }

    Status GlobalUniquenessIndex::deleteValue(const ID &domain_id,
                                              const ID &table_id,
                                              const ID &column_id,
                                              const TID &row_tid,
                                              const TypedValue &value,
                                              uint64_t tx_id,
                                              ErrorContext *ctx)
    {
        if (tx_id == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Transaction ID required");
            return Status::INVALID_ARGUMENT;
        }

        if (value.isNull())
        {
            return Status::OK;
        }

        ValueKey key;
        Status status = buildValueKey(value, key, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (!isEnabled(domain_id))
        {
            return Status::OK;
        }

        auto domain_it = index_.find(domain_id);
        if (domain_it == index_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found in uniqueness index");
            return Status::NOT_FOUND;
        }

        auto value_it = domain_it->second.find(key);
        if (value_it == domain_it->second.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Value not found in uniqueness index");
            return Status::NOT_FOUND;
        }

        for (auto &loc : value_it->second)
        {
            if (loc.table_id == table_id &&
                loc.column_id == column_id &&
                loc.row_tid == row_tid)
            {
                if (loc.xmax == 0)
                {
                    loc.xmax = tx_id;
                    return Status::OK;
                }
                if (loc.xmax == tx_id)
                {
                    return Status::OK;
                }
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Value location not found for delete");
        return Status::NOT_FOUND;
    }

    Status GlobalUniquenessIndex::updateValue(const ID &domain_id,
                                              const ID &table_id,
                                              const ID &column_id,
                                              const TID &row_tid,
                                              const TypedValue &old_value,
                                              const TypedValue &new_value,
                                              uint64_t tx_id,
                                              ErrorContext *ctx)
    {
        if (tx_id == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Transaction ID required");
            return Status::INVALID_ARGUMENT;
        }

        if (old_value.isNull() && new_value.isNull())
        {
            return Status::OK;
        }

        if (old_value.isNull())
        {
            return insertValue(domain_id, table_id, column_id, row_tid, new_value, tx_id, ctx);
        }

        if (new_value.isNull())
        {
            return deleteValue(domain_id, table_id, column_id, row_tid, old_value, tx_id, ctx);
        }

        ValueKey old_key;
        ValueKey new_key;
        Status status = buildValueKey(old_value, old_key, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = buildValueKey(new_value, new_key, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (ValueKeyEqual{}(old_key, new_key))
        {
            return Status::OK;
        }

        status = deleteValue(domain_id, table_id, column_id, row_tid, old_value, tx_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        return insertValue(domain_id, table_id, column_id, row_tid, new_value, tx_id, ctx);
    }

    Status GlobalUniquenessIndex::enableUniqueness(const ID &domain_id, ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_domains_.insert(domain_id);
        index_.try_emplace(domain_id, ValueMap{});
        return Status::OK;
    }

    Status GlobalUniquenessIndex::disableUniqueness(const ID &domain_id, ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_domains_.erase(domain_id);
        index_.erase(domain_id);
        return Status::OK;
    }
} // namespace scratchbird::core
