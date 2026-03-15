#pragma once

#include "scratchbird/core/savepoint_backout.h"
#include "scratchbird/core/status.h"
#include <cstdint>
#include <vector>

namespace scratchbird::core
{
    class Database;
    class StorageEngine;
    struct ErrorContext;

    class MgaBackoutEngine
    {
    public:
        explicit MgaBackoutEngine(Database *db);
        ~MgaBackoutEngine();

        MgaBackoutEngine(const MgaBackoutEngine &) = delete;
        MgaBackoutEngine &operator=(const MgaBackoutEngine &) = delete;
        MgaBackoutEngine(MgaBackoutEngine &&) noexcept = delete;
        MgaBackoutEngine &operator=(MgaBackoutEngine &&) noexcept = delete;

        auto applySavepointBackout(const std::vector<SavepointBackoutAction> &actions,
                                   uint64_t rollback_xid,
                                   ErrorContext *ctx = nullptr) -> Status;

    private:
        friend class StorageEngine;

        auto applyStableHeadAncillaryBackout(
            const SavepointBackoutAction &action,
            uint16_t tablespace_id,
            const uint8_t *current_tuple_data,
            uint32_t current_tuple_size,
            const uint8_t *prior_tuple_data,
            uint32_t prior_tuple_size,
            const std::vector<std::vector<uint8_t>> &transient_tuple_images,
            uint64_t rollback_xid,
            ErrorContext *ctx) -> Status;

        Database *db_;
    };
} // namespace scratchbird::core
