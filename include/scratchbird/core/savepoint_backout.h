#pragma once

#include "scratchbird/core/uuidv7.h"
#include <cstdint>
#include <vector>

namespace scratchbird::core
{
    using SavepointBackoutID = UuidV7Bytes;

    struct SavepointBackoutAction
    {
        SavepointBackoutID table_id{};
        uint32_t stable_page_id = 0;
        uint16_t stable_item_id = 0;
        std::vector<uint8_t> prior_tuple_image;

        [[nodiscard]] auto matches(const SavepointBackoutID &candidate_table_id,
                                   uint32_t candidate_page_id,
                                   uint16_t candidate_item_id) const -> bool
        {
            return table_id == candidate_table_id && stable_page_id == candidate_page_id &&
                   stable_item_id == candidate_item_id;
        }

        [[nodiscard]] auto restoresPriorState() const -> bool
        {
            return !prior_tuple_image.empty();
        }
    };
} // namespace scratchbird::core
