/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/mga_backout_engine.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/storage_engine.h"

namespace scratchbird::core
{
    namespace
    {
        auto retireToastValues(ToastManager *toast_mgr,
                               const std::vector<ID> &toast_values_to_delete,
                               uint64_t rollback_xid,
                               const char *warning_context,
                               ErrorContext *ctx) -> Status
        {
            if (toast_mgr == nullptr)
            {
                return Status::OK;
            }

            for (const ID &toast_value_id : toast_values_to_delete)
            {
                Status cleanup_status = toast_mgr->deleteToastValue(toast_value_id, rollback_xid, ctx);
                if (cleanup_status != Status::OK && cleanup_status != Status::NOT_FOUND)
                {
                    LOG_WARNING(STORAGE,
                                "Failed to retire TOAST value during %s (status=%d)",
                                warning_context,
                                static_cast<int>(cleanup_status));
                }
            }

            return Status::OK;
        }
    } // namespace

    MgaBackoutEngine::MgaBackoutEngine(Database *db)
        : db_(db)
    {
    }

    MgaBackoutEngine::~MgaBackoutEngine() = default;

    auto MgaBackoutEngine::applySavepointBackout(
        const std::vector<SavepointBackoutAction> &actions,
        uint64_t rollback_xid,
        ErrorContext *ctx) -> Status
    {
        StorageEngine *storage = (db_ != nullptr) ? db_->storage_engine() : nullptr;
        if (storage == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Storage engine not available for backout");
            return Status::IO_ERROR;
        }

        for (const auto &action : actions)
        {
            Status status = storage->applyStableHeadBackout(action, rollback_xid, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        return Status::OK;
    }

    auto MgaBackoutEngine::applyStableHeadAncillaryBackout(
        const SavepointBackoutAction &action,
        uint16_t tablespace_id,
        const uint8_t *current_tuple_data,
        uint32_t current_tuple_size,
        const uint8_t *prior_tuple_data,
        uint32_t prior_tuple_size,
        const std::vector<std::vector<uint8_t>> &transient_tuple_images,
        uint64_t rollback_xid,
        ErrorContext *ctx) -> Status
    {
        StorageEngine *storage = (db_ != nullptr) ? db_->storage_engine() : nullptr;
        if (storage == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Storage engine not available for backout");
            return Status::IO_ERROR;
        }

        const bool prior_row_present = action.restoresPriorState();
        Status status = storage->applyStableTidIndexBackout(action.table_id,
                                                            tablespace_id,
                                                            action.stable_page_id,
                                                            action.stable_item_id,
                                                            current_tuple_data,
                                                            current_tuple_size,
                                                            prior_tuple_data,
                                                            prior_tuple_size,
                                                            prior_row_present,
                                                            rollback_xid,
                                                            ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<ID> toast_values_to_delete;
        ID restored_toast_value{};
        const bool restored_has_toast =
            prior_row_present &&
            ToastManager::extractReferencedToastValueId(prior_tuple_data,
                                                        prior_tuple_size,
                                                        &restored_toast_value);

        ToastManager::queueReferencedToastValueForRetirement(current_tuple_data,
                                                             current_tuple_size,
                                                             restored_has_toast ? &restored_toast_value
                                                                                : nullptr,
                                                             &toast_values_to_delete);
        for (const auto &chain_image : transient_tuple_images)
        {
            ToastManager::queueReferencedToastValueForRetirement(
                chain_image.data(),
                static_cast<uint32_t>(chain_image.size()),
                restored_has_toast ? &restored_toast_value : nullptr,
                &toast_values_to_delete);
        }

        ToastManager *toast_mgr = (action.table_id == ID{})
                                      ? nullptr
                                      : storage->getOrCreateToastManager(action.table_id, ctx);
        return retireToastValues(toast_mgr,
                                 toast_values_to_delete,
                                 rollback_xid,
                                 prior_row_present ? "restored-row native backout"
                                                   : "purged-row native backout",
                                 ctx);
    }
} // namespace scratchbird::core
