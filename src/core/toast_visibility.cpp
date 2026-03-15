/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/toast_visibility.h"
#include "scratchbird/core/transaction_manager.h"

namespace scratchbird::core
{

    auto ToastVisibility::evaluateChunkLifecycle(uint64_t chunk_xmin, uint64_t chunk_xmax,
                                                 uint64_t current_xid, uint64_t reclaim_horizon,
                                                 TransactionManager *tm)
        -> ToastChunkLifecycleDecision
    {
        ToastChunkLifecycleDecision decision{};
        if (tm == nullptr)
        {
            return decision;
        }

        // GC/audit callers need durable transaction-state classification even when
        // a delete XID is newer than the current reclaim horizon. Use an unbounded
        // reader XID for maintenance-state inspection and gate physical reclaim
        // separately with reclaim_horizon below.
        const uint64_t maintenance_xid =
            (reclaim_horizon != 0) ? UINT64_MAX : current_xid;

        decision.visibility = tm->evaluateRecordVisibility(chunk_xmin,
                                                           chunk_xmax,
                                                           current_xid,
                                                           VisibilityMode::READ_CURRENT_VERSION,
                                                           nullptr);
        decision.visible = decision.visibility.visible;
        decision.create_horizon = tm->evaluateTransactionVisibility(
            chunk_xmin, maintenance_xid, VisibilityMode::READ_CURRENT_VERSION, nullptr);

        if (decision.create_horizon.state == TransactionState::ABORTED ||
            decision.create_horizon.reason == VisibilityReason::INVALID_XID)
        {
            decision.state = ToastChunkLifecycleState::RECLAIMABLE_ABORTED_CREATE;
            decision.reclaimable = true;
            decision.visible = false;
            return decision;
        }

        if (chunk_xmax == 0)
        {
            decision.state = decision.visible ? ToastChunkLifecycleState::LIVE_VISIBLE
                                              : ToastChunkLifecycleState::CREATE_INVISIBLE;
            return decision;
        }

        decision.delete_horizon = tm->evaluateTransactionVisibility(
            chunk_xmax, maintenance_xid, VisibilityMode::READ_CURRENT_VERSION, nullptr);

        if (decision.delete_horizon.state == TransactionState::ABORTED ||
            decision.delete_horizon.reason == VisibilityReason::INVALID_XID)
        {
            decision.state = ToastChunkLifecycleState::CLEAR_DELETE_MARKER;
            decision.clear_delete_marker = true;
            return decision;
        }

        const bool horizon_allows_reclaim =
            maintenance_xid == UINT64_MAX || chunk_xmax < maintenance_xid;
        if (decision.delete_horizon.visible && horizon_allows_reclaim)
        {
            decision.state = ToastChunkLifecycleState::RECLAIMABLE_DELETED;
            decision.reclaimable = true;
            return decision;
        }

        decision.state = decision.visible ? ToastChunkLifecycleState::DELETE_PENDING
                                          : ToastChunkLifecycleState::CREATE_INVISIBLE;
        return decision;
    }

    bool ToastVisibility::isChunkVisible(uint64_t chunk_xmin, uint64_t chunk_xmax,
                                         uint64_t current_xid, TransactionManager *tm)
    {
        if (tm == nullptr)
        {
            return false;
        }

        return tm->isRuntimeRecordVisible(chunk_xmin, chunk_xmax, current_xid);
    }

    bool ToastVisibility::isOwnChunk(uint64_t chunk_xmin, uint64_t current_xid)
    {
        return chunk_xmin == current_xid;
    }

    bool ToastVisibility::isChunkDeleted(uint64_t chunk_xmax, uint64_t current_xid,
                                         TransactionManager *tm)
    {
        if (chunk_xmax == 0)
        {
            return false;
        }

        if (tm == nullptr)
        {
            return false;
        }

        return tm->isRuntimeTransactionVisible(chunk_xmax, current_xid);
    }

} // namespace scratchbird::core
