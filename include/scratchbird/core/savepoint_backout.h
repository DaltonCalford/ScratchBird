#pragma once

#include "scratchbird/core/uuidv7.h"
#include <cstdint>
#include <vector>

namespace scratchbird::core
{
    using SavepointBackoutID = UuidV7Bytes;

    enum class SavepointBackoutActionKind : uint8_t
    {
        INSERT = 0,
        DELETE = 1,
        UPDATE = 2
    };

    struct SavepointBackoutAction
    {
        SavepointBackoutActionKind kind = SavepointBackoutActionKind::INSERT;
        SavepointBackoutID table_id{};
        uint32_t page_id = 0;
        uint16_t item_id = 0;
        std::vector<uint8_t> old_tuple_image;
    };
} // namespace scratchbird::core
