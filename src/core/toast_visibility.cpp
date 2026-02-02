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

    bool ToastVisibility::isChunkVisible(uint64_t chunk_xmin, uint64_t chunk_xmax,
                                         uint64_t current_xid, TransactionManager *tm)
    {
        // MGA Rule 3: Own changes always visible
        // If we created this chunk, it's visible to us regardless of commit state
        if (chunk_xmin == current_xid)
        {
            return true;
        }

        // Firebird MGA: Check if creating transaction is visible via TIP
        // Uses isVersionVisible() which performs TIP lookup (O(1))
        if (!tm->isVersionVisible(chunk_xmin, current_xid))
        {
            return false; // Creating transaction not visible (not committed or too new)
        }

        // Check if chunk is deleted
        if (chunk_xmax != 0)
        {
            // If we deleted it, not visible to us
            if (chunk_xmax == current_xid)
            {
                return false;
            }

            // Firebird MGA: Check if deletion is visible via TIP
            if (tm->isVersionVisible(chunk_xmax, current_xid))
            {
                return false; // Deletion visible, chunk not visible
            }
        }

        // Chunk visible: created by visible transaction, not deleted or deletion not visible
        return true;
    }

    bool ToastVisibility::isOwnChunk(uint64_t chunk_xmin, uint64_t current_xid)
    {
        // MGA Rule 3: Own changes always visible
        return chunk_xmin == current_xid;
    }

    bool ToastVisibility::isChunkDeleted(uint64_t chunk_xmax, uint64_t current_xid,
                                         TransactionManager *tm)
    {
        if (chunk_xmax == 0)
        {
            return false; // Not deleted
        }

        if (chunk_xmax == current_xid)
        {
            return true; // We deleted it
        }

        // Firebird MGA: Check if deletion is visible via TIP
        return tm->isVersionVisible(chunk_xmax, current_xid);
    }

} // namespace scratchbird::core
