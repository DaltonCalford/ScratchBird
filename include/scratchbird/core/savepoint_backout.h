#pragma once

#include "scratchbird/core/uuidv7.h"
#include <cstdint>
#include <vector>

namespace scratchbird::core
{
    using SavepointBackoutID = UuidV7Bytes;

    enum class SavepointBackoutActionKind : uint8_t
    {
        REMOVE_ROW = 0,
        RESTORE_ROW = 1
    };

    struct SavepointBackoutAction
    {
        SavepointBackoutActionKind kind = SavepointBackoutActionKind::REMOVE_ROW;
        SavepointBackoutID table_id{};
        uint32_t stable_page_id = 0;
        uint16_t stable_item_id = 0;
        std::vector<uint8_t> restore_tuple_image;

        [[nodiscard]] auto matches(const SavepointBackoutID &candidate_table_id,
                                   uint32_t candidate_page_id,
                                   uint16_t candidate_item_id) const -> bool
        {
            return table_id == candidate_table_id && stable_page_id == candidate_page_id &&
                   stable_item_id == candidate_item_id;
        }
    };
} // namespace scratchbird::core
