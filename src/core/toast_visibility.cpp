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
        return tm->evaluateRecordVisibility(chunk_xmin,
                                            chunk_xmax,
                                            current_xid,
                                            VisibilityMode::READ_CURRENT_VERSION,
                                            nullptr)
            .visible;
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

        return tm->evaluateTransactionVisibility(chunk_xmax,
                                                 current_xid,
                                                 VisibilityMode::READ_CURRENT_VERSION,
                                                 nullptr)
            .visible;
    }

} // namespace scratchbird::core
