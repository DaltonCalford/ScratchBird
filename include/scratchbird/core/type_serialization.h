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

#include "scratchbird/core/types.h"
#include "scratchbird/core/error_context.h"
#include <vector>
#include <cstdint>
#include <optional>

namespace scratchbird::core
{
    /**
     * Type Serialization and Deserialization
     *
     * Handles converting TypedValue objects to/from binary format
     * for storage on disk or transmission over network.
     */
    class TypeSerializer
    {
    public:
        /**
         * Serialize a TypedValue to binary format
         * Returns the serialized bytes
         */
        static auto serialize(const TypedValue &value) -> std::vector<uint8_t>;

        /**
         * Deserialize binary data to a TypedValue
         * Returns nullopt on error
         */
        static auto deserialize(DataType type, const uint8_t *data, uint32_t size,
                                ErrorContext *ctx = nullptr) -> std::optional<TypedValue>;

        /**
         * Get the size of the serialized representation without actually serializing
         * Returns 0 for NULL or unsupported types
         */
        static auto getSerializedSize(const TypedValue &value) -> uint32_t;
    };

} // namespace scratchbird::core
