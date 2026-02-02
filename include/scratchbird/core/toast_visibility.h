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

#include "scratchbird/core/transaction_manager.h"
#include <cstdint>

namespace scratchbird::core
{

    /**
     * ToastVisibility - TIP-based visibility helper for TOAST chunks
     *
     * Implements Firebird MGA visibility rules for TOAST chunks using
     * Transaction Inventory Pages (TIP) instead of PostgreSQL-style snapshots.
     *
     * This class encapsulates TOAST chunk visibility logic to ensure:
     * - MGA Rule 0: Uses TIP-based visibility, NOT snapshot-based
     * - MGA Rule 3: Own changes always visible (xmin == current_xid)
     * - Soft deletes: Chunks marked deleted via xmax, not physically removed
     */
    class ToastVisibility
    {
    public:
        /**
         * Check if TOAST chunk is visible to current transaction
         *
         * Uses TIP-based visibility (Firebird MGA):
         * - Checks transaction state via TIP (not snapshot arrays)
         * - Own changes always visible (MGA Rule 3)
         * - Respects soft deletes (xmax set and visible)
         *
         * @param chunk_xmin Transaction that created this chunk
         * @param chunk_xmax Transaction that deleted this chunk (0 if active)
         * @param current_xid Current transaction ID viewing the chunk
         * @param tm Transaction manager for TIP lookups
         * @return true if chunk is visible to current_xid
         */
        static bool isChunkVisible(uint64_t chunk_xmin, uint64_t chunk_xmax,
                                   uint64_t current_xid, TransactionManager *tm);

        /**
         * Check if chunk was created by current transaction (always visible)
         *
         * Implements MGA Rule 3: Own changes always visible
         *
         * @param chunk_xmin Transaction that created this chunk
         * @param current_xid Current transaction ID
         * @return true if current transaction created this chunk
         */
        static bool isOwnChunk(uint64_t chunk_xmin, uint64_t current_xid);

        /**
         * Check if chunk is deleted (xmax set and visible)
         *
         * Uses TIP to check if deleting transaction is visible:
         * - If we deleted it (xmax == current_xid), it's deleted
         * - Otherwise check TIP state of xmax transaction
         *
         * @param chunk_xmax Transaction that deleted this chunk (0 if not deleted)
         * @param current_xid Current transaction ID
         * @param tm Transaction manager for TIP lookups
         * @return true if chunk is deleted and deletion is visible
         */
        static bool isChunkDeleted(uint64_t chunk_xmax, uint64_t current_xid,
                                   TransactionManager *tm);
    };

} // namespace scratchbird::core
