/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/index_key_extractor.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/error_context.h"
#include <cstring>

namespace scratchbird::core
{

IndexKeyExtractor::IndexKeyExtractor()
{
}

IndexKeyExtractor::~IndexKeyExtractor()
{
    clearCache();
}

auto IndexKeyExtractor::extractKey(
    const uint8_t* tuple_data,
    size_t tuple_size,
    const std::vector<size_t>& column_offsets,
    const std::vector<size_t>& column_sizes,
    const std::vector<uint16_t>& column_indices,
    ToastManager* toast_mgr,
    uint64_t xid,
    std::vector<uint8_t>* key_out,
    ErrorContext* ctx) -> Status
{
    if (tuple_data == nullptr || key_out == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          "Null pointer in extractKey");
        return Status::INVALID_ARGUMENT;
    }

    key_out->clear();

    // Extract each column for the index
    for (uint16_t col_idx : column_indices)
    {
        // Validate column index
        if (col_idx >= column_offsets.size() || col_idx >= column_sizes.size())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Column index out of range");
            return Status::INVALID_ARGUMENT;
        }

        size_t col_offset = column_offsets[col_idx];
        size_t col_size = column_sizes[col_idx];

        // Validate offset + size doesn't exceed tuple
        if (col_offset + col_size > tuple_size)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Column offset/size exceeds tuple size");
            return Status::INVALID_ARGUMENT;
        }

        const uint8_t* col_data = tuple_data + col_offset;

        // Get column value (detoasting if needed)
        std::vector<uint8_t> col_value;
        Status status = getColumnValue(col_data, col_size, col_idx,
                                        toast_mgr, xid, &col_value, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Append to key
        key_out->insert(key_out->end(), col_value.begin(), col_value.end());
    }

    return Status::OK;
}

auto IndexKeyExtractor::extractKeyForUpdate(
    const uint8_t* old_tuple_data,
    size_t old_tuple_size,
    const std::vector<size_t>& old_column_offsets,
    const std::vector<size_t>& old_column_sizes,
    const uint8_t* new_tuple_data,
    size_t new_tuple_size,
    const std::vector<size_t>& new_column_offsets,
    const std::vector<size_t>& new_column_sizes,
    const std::vector<uint16_t>& column_indices,
    ToastManager* toast_mgr,
    uint64_t xid,
    std::vector<uint8_t>* old_key_out,
    std::vector<uint8_t>* new_key_out,
    ErrorContext* ctx) -> Status
{
    if (old_tuple_data == nullptr || new_tuple_data == nullptr ||
        old_key_out == nullptr || new_key_out == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          "Null pointer in extractKeyForUpdate");
        return Status::INVALID_ARGUMENT;
    }

    // Extract old key
    Status status = extractKey(
        old_tuple_data, old_tuple_size,
        old_column_offsets, old_column_sizes,
        column_indices, toast_mgr, xid,
        old_key_out, ctx);

    if (status != Status::OK)
    {
        return status;
    }

    // Extract new key
    status = extractKey(
        new_tuple_data, new_tuple_size,
        new_column_offsets, new_column_sizes,
        column_indices, toast_mgr, xid,
        new_key_out, ctx);

    if (status != Status::OK)
    {
        return status;
    }

    return Status::OK;
}

void IndexKeyExtractor::clearCache()
{
    detoast_cache_.clear();
}

auto IndexKeyExtractor::getColumnValue(
    const uint8_t* column_data,
    size_t column_size,
    uint16_t column_index,
    ToastManager* toast_mgr,
    uint64_t xid,
    std::vector<uint8_t>* value_out,
    ErrorContext* ctx) -> Status
{
    // Check cache first
    auto it = detoast_cache_.find(column_index);
    if (it != detoast_cache_.end())
    {
        *value_out = it->second;
        return Status::OK;
    }

    // Check if column is a TOAST pointer
    if (toast_mgr != nullptr &&
        ToastManager::isToastPointer(column_data, column_size))
    {
        // Detoast value
        std::vector<uint8_t> detoasted;
        Status status = toast_mgr->detoastIfNeeded(
            column_data, column_size, &detoasted, xid, ctx);

        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status,
                              "Failed to detoast column for indexing");
            return status;
        }

        // Cache detoasted value
        detoast_cache_[column_index] = detoasted;
        *value_out = std::move(detoasted);
    }
    else
    {
        // Not a TOAST pointer, use inline value as-is
        value_out->assign(column_data, column_data + column_size);

        // Cache inline value too (for consistency)
        detoast_cache_[column_index] = *value_out;
    }

    return Status::OK;
}

} // namespace scratchbird::core
