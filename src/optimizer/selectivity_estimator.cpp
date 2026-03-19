/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/optimizer/selectivity_estimator.h"
#include "scratchbird/parser/ast_v3.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/plain_value_reader.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <optional>

namespace scratchbird::optimizer
{
    namespace
    {
        struct ResolvedColumnRef
        {
            core::ID column_id{};
            std::string column_name;
            uint16_t data_type = 0;
        };

        struct EqualityPredicateTerm
        {
            ResolvedColumnRef column;
            std::vector<uint8_t> value;
        };

        struct JoinEqualityColumnPair
        {
            core::ID left_column_id{};
            core::ID right_column_id{};
        };

        auto clampSelectivity(double value) -> double
        {
            return std::max(0.0, std::min(1.0, value));
        }

        auto isZeroId(const core::ID &id) -> bool
        {
            return id == core::ID{};
        }

        auto matchesForeignKeyJoin(core::Database *db,
                                   const core::ID &left_table_id,
                                   const core::ID &right_table_id,
                                   const std::vector<JoinEqualityColumnPair> &join_pairs,
                                   core::CatalogManager::ForeignKeyInfo &matched_fk,
                                   bool &left_is_child,
                                   core::ErrorContext *ctx) -> bool
        {
            if (db == nullptr || db->catalog_manager() == nullptr || join_pairs.empty())
            {
                return false;
            }

            std::vector<core::CatalogManager::ColumnInfo> left_columns;
            std::vector<core::CatalogManager::ColumnInfo> right_columns;
            if (db->catalog_manager()->getColumns(left_table_id, left_columns, ctx) !=
                    core::Status::OK ||
                db->catalog_manager()->getColumns(right_table_id, right_columns, ctx) !=
                    core::Status::OK)
            {
                return false;
            }

            const auto resolve_column_name =
                [](const std::vector<core::CatalogManager::ColumnInfo> &columns,
                   const core::ID &column_id,
                   std::string &column_name) -> bool {
                    for (const auto &column : columns)
                    {
                        if (column.column_id == column_id)
                        {
                            column_name = column.column_name;
                            return true;
                        }
                    }
                    return false;
                };

            const auto relation_matches =
                [&](const core::CatalogManager::ForeignKeyInfo &fk,
                    bool child_on_left) -> bool {
                    if (!fk.is_enabled ||
                        fk.child_columns.size() != join_pairs.size() ||
                        fk.parent_columns.size() != join_pairs.size())
                    {
                        return false;
                    }

                    std::vector<bool> consumed(join_pairs.size(), false);
                    for (size_t fk_index = 0; fk_index < fk.child_columns.size(); ++fk_index)
                    {
                        bool matched = false;
                        for (size_t pair_index = 0; pair_index < join_pairs.size(); ++pair_index)
                        {
                            if (consumed[pair_index])
                            {
                                continue;
                            }

                            std::string child_name;
                            std::string parent_name;
                            const bool resolved =
                                child_on_left
                                    ? (resolve_column_name(left_columns,
                                                           join_pairs[pair_index].left_column_id,
                                                           child_name) &&
                                       resolve_column_name(right_columns,
                                                           join_pairs[pair_index].right_column_id,
                                                           parent_name))
                                    : (resolve_column_name(right_columns,
                                                           join_pairs[pair_index].right_column_id,
                                                           child_name) &&
                                       resolve_column_name(left_columns,
                                                           join_pairs[pair_index].left_column_id,
                                                           parent_name));
                            if (!resolved)
                            {
                                continue;
                            }

                            if (core::IdentifierUtils::namesMatch(child_name,
                                                                  false,
                                                                  fk.child_columns[fk_index],
                                                                  false) &&
                                core::IdentifierUtils::namesMatch(parent_name,
                                                                  false,
                                                                  fk.parent_columns[fk_index],
                                                                  false))
                            {
                                consumed[pair_index] = true;
                                matched = true;
                                break;
                            }
                        }

                        if (!matched)
                        {
                            return false;
                        }
                    }

                    return true;
                };

            std::vector<core::CatalogManager::ForeignKeyInfo> foreign_keys;
            if (db->catalog_manager()->getForeignKeysForTable(left_table_id, foreign_keys, ctx) ==
                core::Status::OK)
            {
                for (const auto &fk : foreign_keys)
                {
                    if (fk.parent_table_id == right_table_id && relation_matches(fk, true))
                    {
                        matched_fk = fk;
                        left_is_child = true;
                        return true;
                    }
                }
            }

            foreign_keys.clear();
            if (db->catalog_manager()->getForeignKeysForTable(right_table_id, foreign_keys, ctx) ==
                core::Status::OK)
            {
                for (const auto &fk : foreign_keys)
                {
                    if (fk.parent_table_id == left_table_id && relation_matches(fk, false))
                    {
                        matched_fk = fk;
                        left_is_child = false;
                        return true;
                    }
                }
            }

            return false;
        }

        auto unwrapCasts(const parser::v3::Expression *expr) -> const parser::v3::Expression *
        {
            const auto *current = expr;
            while (current != nullptr &&
                   current->kind() == parser::v3::ASTKind::CastExpr)
            {
                current = static_cast<const parser::v3::CastExpr *>(current)->expr;
            }
            return current;
        }

        auto encodeScalarValueToBytes(const void *value,
                                      size_t value_size,
                                      std::vector<uint8_t> &out) -> void
        {
            out.resize(value_size);
            std::memcpy(out.data(), value, value_size);
        }

        auto trimAsciiCopy(std::string_view text) -> std::string
        {
            size_t start = 0;
            while (start < text.size() &&
                   std::isspace(static_cast<unsigned char>(text[start])) != 0)
            {
                ++start;
            }

            size_t end = text.size();
            while (end > start &&
                   std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
            {
                --end;
            }
            return std::string(text.substr(start, end - start));
        }

        auto ltrimAsciiCopy(std::string_view text) -> std::string
        {
            size_t start = 0;
            while (start < text.size() &&
                   std::isspace(static_cast<unsigned char>(text[start])) != 0)
            {
                ++start;
            }
            return std::string(text.substr(start));
        }

        auto rtrimAsciiCopy(std::string_view text) -> std::string
        {
            size_t end = text.size();
            while (end > 0 &&
                   std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
            {
                --end;
            }
            return std::string(text.substr(0, end));
        }

        auto parseBoolText(std::string_view text, bool &value_out) -> bool
        {
            std::string normalized = core::IdentifierUtils::toUpper(trimAsciiCopy(text));
            if (normalized == "TRUE" || normalized == "T" || normalized == "1" ||
                normalized == "YES" || normalized == "ON")
            {
                value_out = true;
                return true;
            }
            if (normalized == "FALSE" || normalized == "F" || normalized == "0" ||
                normalized == "NO" || normalized == "OFF")
            {
                value_out = false;
                return true;
            }
            return false;
        }

        auto isLengthPrefixedType(core::DataType type) -> bool
        {
            switch (type)
            {
                case core::DataType::CHAR:
                case core::DataType::VARCHAR:
                case core::DataType::TEXT:
                case core::DataType::JSON:
                case core::DataType::JSONB:
                case core::DataType::XML:
                case core::DataType::BINARY:
                case core::DataType::VARBINARY:
                case core::DataType::BLOB:
                case core::DataType::BYTEA:
                case core::DataType::VECTOR:
                case core::DataType::BSON:
                case core::DataType::BLOB_SUB_TYPE_TEXT:
                    return true;
                default:
                    return false;
            }
        }

        template <typename T>
        auto readScalarValue(const std::vector<uint8_t> &value, T &out) -> bool
        {
            if (value.size() < sizeof(T))
            {
                return false;
            }
            std::memcpy(&out, value.data(), sizeof(T));
            return true;
        }

        auto encodeLengthPrefixedBytes(std::string_view text,
                                       std::vector<uint8_t> &value_out) -> void
        {
            value_out.resize(sizeof(uint32_t) + text.size());
            const uint32_t len = static_cast<uint32_t>(text.size());
            std::memcpy(value_out.data(), &len, sizeof(len));
            if (!text.empty())
            {
                std::memcpy(value_out.data() + sizeof(uint32_t), text.data(), text.size());
            }
        }

        auto decodeLengthPrefixedBytes(const std::vector<uint8_t> &value,
                                       std::vector<uint8_t> &payload_out) -> bool
        {
            if (value.empty())
            {
                payload_out.clear();
                return true;
            }

            size_t offset = 0;
            uint32_t len = 0;
            if (!core::readUint32LE(value.data(), value.size(), offset, len) ||
                offset + len > value.size())
            {
                return false;
            }

            payload_out.assign(value.begin() + static_cast<std::ptrdiff_t>(offset),
                               value.begin() + static_cast<std::ptrdiff_t>(offset + len));
            return true;
        }

        auto decodeLengthPrefixedString(const std::vector<uint8_t> &value,
                                        std::string &text_out) -> bool
        {
            std::vector<uint8_t> payload;
            if (!decodeLengthPrefixedBytes(value, payload))
            {
                return false;
            }
            text_out.assign(payload.begin(), payload.end());
            return true;
        }

        auto compareByteVectors(const std::vector<uint8_t> &lhs,
                                const std::vector<uint8_t> &rhs) -> int
        {
            const size_t shared = std::min(lhs.size(), rhs.size());
            for (size_t i = 0; i < shared; ++i)
            {
                if (lhs[i] < rhs[i])
                {
                    return -1;
                }
                if (lhs[i] > rhs[i])
                {
                    return 1;
                }
            }
            if (lhs.size() < rhs.size())
            {
                return -1;
            }
            if (lhs.size() > rhs.size())
            {
                return 1;
            }
            return 0;
        }

        auto comparatorFamilyForType(core::DataType type) -> StatisticsComparatorFamily
        {
            switch (type)
            {
                case core::DataType::INT8:
                case core::DataType::INT16:
                case core::DataType::INT32:
                case core::DataType::INT64:
                case core::DataType::INT128:
                case core::DataType::MEDIUMINT:
                    return StatisticsComparatorFamily::SIGNED_INTEGER;
                case core::DataType::UINT8:
                case core::DataType::UINT16:
                case core::DataType::UINT32:
                case core::DataType::UINT64:
                case core::DataType::UINT128:
                    return StatisticsComparatorFamily::UNSIGNED_INTEGER;
                case core::DataType::FLOAT32:
                case core::DataType::FLOAT64:
                case core::DataType::DECIMAL:
                case core::DataType::MONEY:
                case core::DataType::DECFLOAT16:
                case core::DataType::DECFLOAT34:
                    return StatisticsComparatorFamily::NUMERIC;
                case core::DataType::CHAR:
                case core::DataType::VARCHAR:
                case core::DataType::TEXT:
                case core::DataType::JSON:
                case core::DataType::JSONB:
                case core::DataType::XML:
                case core::DataType::BLOB_SUB_TYPE_TEXT:
                    return StatisticsComparatorFamily::STRING;
                case core::DataType::DATE:
                case core::DataType::TIME:
                case core::DataType::TIMESTAMP:
                case core::DataType::TIMESTAMP_WITH_ZONE:
                case core::DataType::TIME_WITH_ZONE:
                case core::DataType::DATETIME:
                case core::DataType::YEAR:
                    return StatisticsComparatorFamily::TEMPORAL;
                case core::DataType::UUID:
                    return StatisticsComparatorFamily::UUID;
                case core::DataType::BOOLEAN:
                case core::DataType::BIT:
                    return StatisticsComparatorFamily::BOOLEAN;
                case core::DataType::BINARY:
                case core::DataType::VARBINARY:
                case core::DataType::BLOB:
                case core::DataType::BYTEA:
                case core::DataType::VECTOR:
                case core::DataType::BSON:
                    return StatisticsComparatorFamily::BINARY;
                default:
                    return StatisticsComparatorFamily::UNKNOWN;
            }
        }

        auto valueEncodingForType(core::DataType type) -> StatisticsValueEncoding
        {
            if (type == core::DataType::UUID)
            {
                return StatisticsValueEncoding::UUID_BYTES;
            }
            if (isLengthPrefixedType(type))
            {
                return StatisticsValueEncoding::PLAIN_LENGTH_PREFIXED;
            }
            return StatisticsValueEncoding::PLAIN_FIXED;
        }

        auto effectiveComparatorFamily(const ColumnStatistics &stats)
            -> StatisticsComparatorFamily
        {
            if (stats.comparator_family != StatisticsComparatorFamily::UNKNOWN)
            {
                return stats.comparator_family;
            }
            return comparatorFamilyForType(stats.data_type);
        }

        auto effectiveValueEncoding(const ColumnStatistics &stats)
            -> StatisticsValueEncoding
        {
            if (stats.value_encoding != StatisticsValueEncoding::UNKNOWN)
            {
                return stats.value_encoding;
            }
            return valueEncodingForType(stats.data_type);
        }

        auto decodeComparableScalar(const std::vector<uint8_t> &value,
                                    core::DataType type,
                                    long double &out) -> bool
        {
            switch (type)
            {
                case core::DataType::INT8: {
                    int8_t decoded = 0;
                    if (!readScalarValue(value, decoded)) return false;
                    out = static_cast<long double>(decoded);
                    return true;
                }
                case core::DataType::INT16: {
                    int16_t decoded = 0;
                    if (!readScalarValue(value, decoded)) return false;
                    out = static_cast<long double>(decoded);
                    return true;
                }
                case core::DataType::INT32:
                case core::DataType::MEDIUMINT:
                case core::DataType::DATE:
                case core::DataType::YEAR: {
                    int32_t decoded = 0;
                    if (!readScalarValue(value, decoded)) return false;
                    out = static_cast<long double>(decoded);
                    return true;
                }
                case core::DataType::INT64:
                case core::DataType::TIME:
                case core::DataType::TIMESTAMP:
                case core::DataType::TIMESTAMP_WITH_ZONE:
                case core::DataType::TIME_WITH_ZONE:
                case core::DataType::DATETIME: {
                    int64_t decoded = 0;
                    if (!readScalarValue(value, decoded)) return false;
                    out = static_cast<long double>(decoded);
                    return true;
                }
                case core::DataType::UINT8:
                case core::DataType::BOOLEAN:
                case core::DataType::BIT: {
                    uint8_t decoded = 0;
                    if (!readScalarValue(value, decoded)) return false;
                    out = static_cast<long double>(decoded);
                    return true;
                }
                case core::DataType::UINT16: {
                    uint16_t decoded = 0;
                    if (!readScalarValue(value, decoded)) return false;
                    out = static_cast<long double>(decoded);
                    return true;
                }
                case core::DataType::UINT32: {
                    uint32_t decoded = 0;
                    if (!readScalarValue(value, decoded)) return false;
                    out = static_cast<long double>(decoded);
                    return true;
                }
                case core::DataType::UINT64: {
                    uint64_t decoded = 0;
                    if (!readScalarValue(value, decoded)) return false;
                    out = static_cast<long double>(decoded);
                    return true;
                }
                case core::DataType::FLOAT32: {
                    float decoded = 0.0f;
                    if (!readScalarValue(value, decoded)) return false;
                    out = static_cast<long double>(decoded);
                    return true;
                }
                case core::DataType::FLOAT64:
                case core::DataType::DECIMAL:
                case core::DataType::MONEY:
                case core::DataType::DECFLOAT16:
                case core::DataType::DECFLOAT34: {
                    if (value.size() >= sizeof(double))
                    {
                        double decoded = 0.0;
                        if (!readScalarValue(value, decoded)) return false;
                        out = static_cast<long double>(decoded);
                        return true;
                    }
                    if (value.size() >= sizeof(float))
                    {
                        float decoded = 0.0f;
                        if (!readScalarValue(value, decoded)) return false;
                        out = static_cast<long double>(decoded);
                        return true;
                    }
                    return false;
                }
                default:
                    return false;
            }
        }

        auto prefixRank(std::string_view text) -> long double
        {
            constexpr long double BASE = 257.0L;
            constexpr size_t LIMIT = 8;
            long double rank = 0.0L;
            long double factor = 1.0L;
            const size_t count = std::min(LIMIT, text.size());
            for (size_t i = 0; i < count; ++i)
            {
                factor /= BASE;
                rank += factor *
                        static_cast<long double>(
                            static_cast<unsigned char>(text[i]) + 1u);
            }
            factor /= BASE;
            rank += factor * static_cast<long double>(count);
            return rank;
        }

        auto prefixRank(const std::vector<uint8_t> &bytes) -> long double
        {
            constexpr long double BASE = 257.0L;
            constexpr size_t LIMIT = 8;
            long double rank = 0.0L;
            long double factor = 1.0L;
            const size_t count = std::min(LIMIT, bytes.size());
            for (size_t i = 0; i < count; ++i)
            {
                factor /= BASE;
                rank += factor * static_cast<long double>(bytes[i] + 1u);
            }
            factor /= BASE;
            rank += factor * static_cast<long double>(count);
            return rank;
        }

        auto parameterValueToBytes(const BoundParameterValue &binding,
                                   uint16_t data_type,
                                   std::vector<uint8_t> &value_out) -> bool
        {
            if (binding.is_null)
            {
                value_out.clear();
                return true;
            }

            const std::string text = trimAsciiCopy(binding.text);
            try
            {
                switch (static_cast<core::DataType>(data_type))
                {
                    case core::DataType::INT8: {
                        const auto value = static_cast<int8_t>(std::stoi(text));
                        encodeScalarValueToBytes(&value, sizeof(value), value_out);
                        return true;
                    }
                    case core::DataType::INT16: {
                        const auto value = static_cast<int16_t>(std::stoi(text));
                        encodeScalarValueToBytes(&value, sizeof(value), value_out);
                        return true;
                    }
                    case core::DataType::INT32:
                    case core::DataType::MEDIUMINT: {
                        const auto value = static_cast<int32_t>(std::stol(text));
                        encodeScalarValueToBytes(&value, sizeof(value), value_out);
                        return true;
                    }
                    case core::DataType::INT64: {
                        const auto value = static_cast<int64_t>(std::stoll(text));
                        encodeScalarValueToBytes(&value, sizeof(value), value_out);
                        return true;
                    }
                    case core::DataType::UINT8: {
                        const auto value = static_cast<uint8_t>(std::stoul(text));
                        encodeScalarValueToBytes(&value, sizeof(value), value_out);
                        return true;
                    }
                    case core::DataType::UINT16: {
                        const auto value = static_cast<uint16_t>(std::stoul(text));
                        encodeScalarValueToBytes(&value, sizeof(value), value_out);
                        return true;
                    }
                    case core::DataType::UINT32: {
                        const auto value = static_cast<uint32_t>(std::stoul(text));
                        encodeScalarValueToBytes(&value, sizeof(value), value_out);
                        return true;
                    }
                    case core::DataType::UINT64: {
                        const auto value = static_cast<uint64_t>(std::stoull(text));
                        encodeScalarValueToBytes(&value, sizeof(value), value_out);
                        return true;
                    }
                    case core::DataType::FLOAT32: {
                        const auto value = static_cast<float>(std::stof(text));
                        encodeScalarValueToBytes(&value, sizeof(value), value_out);
                        return true;
                    }
                    case core::DataType::FLOAT64:
                    case core::DataType::DECIMAL:
                    case core::DataType::MONEY: {
                        const auto value = static_cast<double>(std::stod(text));
                        encodeScalarValueToBytes(&value, sizeof(value), value_out);
                        return true;
                    }
                    case core::DataType::BOOLEAN: {
                        bool value = false;
                        if (!parseBoolText(text, value))
                        {
                            return false;
                        }
                        value_out = {static_cast<uint8_t>(value ? 1 : 0)};
                        return true;
                    }
                    default:
                        if (isLengthPrefixedType(static_cast<core::DataType>(data_type)))
                        {
                            encodeLengthPrefixedBytes(text, value_out);
                        }
                        else
                        {
                            value_out.assign(text.begin(), text.end());
                        }
                        return true;
                }
            }
            catch (...)
            {
                return false;
            }
        }

        auto parameterExprToBytes(const parser::v3::Expression *expr,
                                  const ParameterBindings *bindings,
                                  uint16_t data_type,
                                  std::vector<uint8_t> &value_out) -> bool
        {
            if (bindings == nullptr)
            {
                return false;
            }

            const auto *current = unwrapCasts(expr);
            if (current == nullptr ||
                current->kind() != parser::v3::ASTKind::ParameterExpr)
            {
                return false;
            }

            const auto *param = static_cast<const parser::v3::ParameterExpr *>(current);
            if (param->is_named)
            {
                return false;
            }

            BoundParameterValue value;
            if (!bindings->getPositional(param->index, value))
            {
                return false;
            }
            return parameterValueToBytes(value, data_type, value_out);
        }

        auto literalExprToBytes(const parser::v3::Expression *expr,
                                const parser::v3::StringPool *pool,
                                const ParameterBindings *bindings,
                                uint16_t data_type,
                                std::vector<uint8_t> &value_out) -> bool
        {
            const auto *current = unwrapCasts(expr);
            if (current == nullptr)
            {
                return false;
            }

            if (parameterExprToBytes(current, bindings, data_type, value_out))
            {
                return true;
            }

            switch (current->kind())
            {
                case parser::v3::ASTKind::LiteralExpr: {
                    const auto *literal = static_cast<const parser::v3::LiteralExpr *>(current);
                    switch (literal->literal_type)
                    {
                        case parser::v3::LiteralType::INTEGER:
                            encodeScalarValueToBytes(&literal->int_value,
                                                     sizeof(literal->int_value),
                                                     value_out);
                            return true;
                        case parser::v3::LiteralType::FLOAT:
                            encodeScalarValueToBytes(&literal->float_value,
                                                     sizeof(literal->float_value),
                                                     value_out);
                            return true;
                        case parser::v3::LiteralType::BOOLEAN:
                            value_out = {static_cast<uint8_t>(literal->bool_value ? 1 : 0)};
                            return true;
                        case parser::v3::LiteralType::STRING:
                            if (pool == nullptr ||
                                literal->string_value == parser::v3::StringPool::INVALID_ID)
                            {
                                return false;
                            }
                            {
                                const std::string text(pool->get(literal->string_value));
                                if (isLengthPrefixedType(static_cast<core::DataType>(data_type)))
                                {
                                    encodeLengthPrefixedBytes(text, value_out);
                                }
                                else
                                {
                                    value_out.assign(text.begin(), text.end());
                                }
                            }
                            return true;
                        case parser::v3::LiteralType::NULL_VALUE:
                            value_out.clear();
                            return true;
                        default:
                            return false;
                    }
                }
                case parser::v3::ASTKind::LiteralInt8Expr: {
                    const auto value =
                        static_cast<const parser::v3::LiteralInt8Expr *>(current)->value;
                    encodeScalarValueToBytes(&value, sizeof(value), value_out);
                    return true;
                }
                case parser::v3::ASTKind::LiteralInt16Expr: {
                    const auto value =
                        static_cast<const parser::v3::LiteralInt16Expr *>(current)->value;
                    encodeScalarValueToBytes(&value, sizeof(value), value_out);
                    return true;
                }
                case parser::v3::ASTKind::LiteralUInt8Expr: {
                    const auto value =
                        static_cast<const parser::v3::LiteralUInt8Expr *>(current)->value;
                    encodeScalarValueToBytes(&value, sizeof(value), value_out);
                    return true;
                }
                case parser::v3::ASTKind::LiteralUInt16Expr: {
                    const auto value =
                        static_cast<const parser::v3::LiteralUInt16Expr *>(current)->value;
                    encodeScalarValueToBytes(&value, sizeof(value), value_out);
                    return true;
                }
                case parser::v3::ASTKind::LiteralUInt32Expr: {
                    const auto value =
                        static_cast<const parser::v3::LiteralUInt32Expr *>(current)->value;
                    encodeScalarValueToBytes(&value, sizeof(value), value_out);
                    return true;
                }
                case parser::v3::ASTKind::LiteralUInt64Expr: {
                    const auto value =
                        static_cast<const parser::v3::LiteralUInt64Expr *>(current)->value;
                    encodeScalarValueToBytes(&value, sizeof(value), value_out);
                    return true;
                }
                case parser::v3::ASTKind::LiteralFloat32Expr: {
                    const auto value =
                        static_cast<const parser::v3::LiteralFloat32Expr *>(current)->value;
                    encodeScalarValueToBytes(&value, sizeof(value), value_out);
                    return true;
                }
                default:
                    return false;
            }
        }

        auto literalExprToString(const parser::v3::Expression *expr,
                                 const parser::v3::StringPool *pool,
                                 const ParameterBindings *bindings,
                                 std::string &value_out) -> bool
        {
            const auto *current = unwrapCasts(expr);
            if (current == nullptr)
            {
                return false;
            }
            if (current->kind() == parser::v3::ASTKind::ParameterExpr)
            {
                if (bindings == nullptr)
                {
                    return false;
                }
                const auto *param = static_cast<const parser::v3::ParameterExpr *>(current);
                if (param->is_named)
                {
                    return false;
                }
                BoundParameterValue value;
                if (!bindings->getPositional(param->index, value) || value.is_null)
                {
                    return false;
                }
                value_out = value.text;
                return true;
            }
            if (current->kind() != parser::v3::ASTKind::LiteralExpr || pool == nullptr)
            {
                return false;
            }
            const auto *literal = static_cast<const parser::v3::LiteralExpr *>(current);
            if (literal->literal_type != parser::v3::LiteralType::STRING ||
                literal->string_value == parser::v3::StringPool::INVALID_ID)
            {
                return false;
            }
            value_out = pool->get(literal->string_value);
            return true;
        }

        auto expressionStatsDescriptor(const parser::v3::Expression *expr,
                                       const parser::v3::StringPool *pool)
            -> std::optional<ExpressionStatsDescriptor>
        {
            const auto *current = unwrapCasts(expr);
            if (current == nullptr || pool == nullptr ||
                current->kind() != parser::v3::ASTKind::FunctionCallExpr)
            {
                return std::nullopt;
            }

            const auto *func = static_cast<const parser::v3::FunctionCallExpr *>(current);
            if (func->arguments.size() != 1 || func->function_path.components.empty())
            {
                return std::nullopt;
            }

            const auto *arg = unwrapCasts(func->arguments.front());
            if (arg == nullptr || arg->kind() != parser::v3::ASTKind::ColumnRefExpr)
            {
                return std::nullopt;
            }

            const auto *column_ref = static_cast<const parser::v3::ColumnRefExpr *>(arg);
            if (column_ref->column.column_name == parser::v3::StringPool::INVALID_ID)
            {
                return std::nullopt;
            }

            const std::string column_name =
                core::IdentifierUtils::toUpper(
                    std::string(pool->get(column_ref->column.column_name)));
            return buildExpressionStatsDescriptor(
                pool->get(func->function_path.components.back()),
                column_name);
        }

        auto resolveColumnRef(core::Database *db,
                              const core::ID &table_id,
                              const parser::v3::Expression *expr,
                              const parser::v3::StringPool *pool,
                              core::ErrorContext *ctx) -> std::optional<ResolvedColumnRef>
        {
            if (db == nullptr || db->catalog_manager() == nullptr ||
                pool == nullptr || isZeroId(table_id))
            {
                return std::nullopt;
            }

            const auto *current = unwrapCasts(expr);
            if (current == nullptr ||
                current->kind() != parser::v3::ASTKind::ColumnRefExpr)
            {
                return std::nullopt;
            }

            const auto *column_ref = static_cast<const parser::v3::ColumnRefExpr *>(current);
            if (column_ref->column.column_name == parser::v3::StringPool::INVALID_ID)
            {
                return std::nullopt;
            }

            const std::string wanted(pool->get(column_ref->column.column_name));
            std::vector<core::CatalogManager::ColumnInfo> columns;
            if (db->catalog_manager()->getColumns(table_id, columns, ctx) != core::Status::OK)
            {
                return std::nullopt;
            }

            for (const auto &column : columns)
            {
                if (core::IdentifierUtils::namesMatch(column.column_name,
                                                      false,
                                                      wanted,
                                                      false))
                {
                    return ResolvedColumnRef{column.column_id,
                                             column.column_name,
                                             column.data_type};
                }
            }

            return std::nullopt;
        }

        auto resolveColumnPairForJoin(core::Database *db,
                                      const core::ID &left_table_id,
                                      const core::ID &right_table_id,
                                      const parser::v3::Expression *left_expr,
                                      const parser::v3::Expression *right_expr,
                                      const parser::v3::StringPool *pool,
                                      core::ID &left_column_id_out,
                                      core::ID &right_column_id_out,
                                      core::ErrorContext *ctx) -> bool
        {
            auto left_column = resolveColumnRef(db, left_table_id, left_expr, pool, ctx);
            auto right_column = resolveColumnRef(db, right_table_id, right_expr, pool, ctx);
            if (left_column.has_value() && right_column.has_value())
            {
                left_column_id_out = left_column->column_id;
                right_column_id_out = right_column->column_id;
                return true;
            }

            left_column = resolveColumnRef(db, left_table_id, right_expr, pool, ctx);
            right_column = resolveColumnRef(db, right_table_id, left_expr, pool, ctx);
            if (left_column.has_value() && right_column.has_value())
            {
                left_column_id_out = left_column->column_id;
                right_column_id_out = right_column->column_id;
                return true;
            }

            return false;
        }

        auto normalizeLiteralForExpression(std::vector<uint8_t> &literal,
                                           const ExpressionStatsDescriptor &descriptor)
            -> bool
        {
            if (literal.empty())
            {
                return true;
            }

            std::string text;
            if (!decodeLengthPrefixedString(literal, text))
            {
                return false;
            }

            switch (descriptor.function)
            {
                case ExpressionStatsFunction::LOWER:
                    std::transform(text.begin(),
                                   text.end(),
                                   text.begin(),
                                   [](unsigned char ch) {
                                       return static_cast<char>(std::tolower(ch));
                                   });
                    break;
                case ExpressionStatsFunction::UPPER:
                    text = core::IdentifierUtils::toUpper(text);
                    break;
                case ExpressionStatsFunction::TRIM:
                    text = trimAsciiCopy(text);
                    break;
                case ExpressionStatsFunction::LTRIM:
                    text = ltrimAsciiCopy(text);
                    break;
                case ExpressionStatsFunction::RTRIM:
                    text = rtrimAsciiCopy(text);
                    break;
                case ExpressionStatsFunction::UNKNOWN:
                default:
                    break;
            }
            encodeLengthPrefixedBytes(text, literal);
            return true;
        }

        auto resolveEqualityPredicateTerm(core::Database *db,
                                          const core::ID &table_id,
                                          const parser::v3::Expression *expr,
                                          const parser::v3::StringPool *pool,
                                          const ParameterBindings *bindings,
                                          EqualityPredicateTerm &out,
                                          core::ErrorContext *ctx) -> bool
        {
            const auto *current = unwrapCasts(expr);
            if (current == nullptr ||
                current->kind() != parser::v3::ASTKind::BinaryExpr)
            {
                return false;
            }

            const auto *binary = static_cast<const parser::v3::BinaryExpr *>(current);
            if (binary->op != parser::v3::BinaryOp::EQ)
            {
                return false;
            }

            auto try_resolve =
                [&](const parser::v3::Expression *column_expr,
                    const parser::v3::Expression *literal_expr) -> bool {
                    auto column = resolveColumnRef(db, table_id, column_expr, pool, ctx);
                    if (!column.has_value())
                    {
                        return false;
                    }

                    std::vector<uint8_t> encoded_value;
                    if (!literalExprToBytes(literal_expr,
                                            pool,
                                            bindings,
                                            column->data_type,
                                            encoded_value))
                    {
                        return false;
                    }

                    out.column = *column;
                    out.value = std::move(encoded_value);
                    return true;
                };

            return try_resolve(binary->left, binary->right) ||
                   try_resolve(binary->right, binary->left);
        }

        auto collectEquiJoinPairs(core::Database *db,
                                  const core::ID &left_table_id,
                                  const core::ID &right_table_id,
                                  const parser::v3::Expression *expr,
                                  const parser::v3::StringPool *pool,
                                  std::vector<JoinEqualityColumnPair> &pairs_out,
                                  core::ErrorContext *ctx) -> bool
        {
            const auto *current = unwrapCasts(expr);
            if (current == nullptr)
            {
                return false;
            }

            if (current->kind() != parser::v3::ASTKind::BinaryExpr)
            {
                return false;
            }

            const auto *binary = static_cast<const parser::v3::BinaryExpr *>(current);
            if (binary->op == parser::v3::BinaryOp::AND)
            {
                const size_t baseline = pairs_out.size();
                if (!collectEquiJoinPairs(db,
                                          left_table_id,
                                          right_table_id,
                                          binary->left,
                                          pool,
                                          pairs_out,
                                          ctx) ||
                    !collectEquiJoinPairs(db,
                                          left_table_id,
                                          right_table_id,
                                          binary->right,
                                          pool,
                                          pairs_out,
                                          ctx))
                {
                    pairs_out.resize(baseline);
                    return false;
                }
                return pairs_out.size() > baseline;
            }

            if (binary->op != parser::v3::BinaryOp::EQ)
            {
                return false;
            }

            JoinEqualityColumnPair pair;
            if (!resolveColumnPairForJoin(db,
                                          left_table_id,
                                          right_table_id,
                                          binary->left,
                                          binary->right,
                                          pool,
                                          pair.left_column_id,
                                          pair.right_column_id,
                                          ctx))
            {
                return false;
            }

            pairs_out.push_back(pair);
            return true;
        }

        auto reorderMultivariateValues(const std::vector<core::ID> &source_column_ids,
                                       const std::vector<std::vector<uint8_t>> &source_values,
                                       const std::vector<core::ID> &target_column_ids,
                                       std::vector<std::vector<uint8_t>> &values_out) -> bool
        {
            if (source_column_ids.size() != source_values.size())
            {
                return false;
            }

            values_out.clear();
            values_out.reserve(target_column_ids.size());
            for (const auto &target_column_id : target_column_ids)
            {
                const auto it =
                    std::find(source_column_ids.begin(), source_column_ids.end(), target_column_id);
                if (it == source_column_ids.end())
                {
                    return false;
                }
                const size_t index =
                    static_cast<size_t>(std::distance(source_column_ids.begin(), it));
                values_out.push_back(source_values[index]);
            }
            return true;
        }

        auto valuesMatch(const std::vector<std::vector<uint8_t>> &left_values,
                         const std::vector<std::vector<uint8_t>> &right_values) -> bool
        {
            if (left_values.size() != right_values.size())
            {
                return false;
            }
            for (size_t index = 0; index < left_values.size(); ++index)
            {
                if (left_values[index] != right_values[index])
                {
                    return false;
                }
            }
            return true;
        }

        auto bestDependencyStrength(const MultivariateStatistics &stats,
                                    const core::ID &first_column_id,
                                    const core::ID &second_column_id) -> double
        {
            double strength = 0.0;
            for (const auto &dependency : stats.dependencies)
            {
                if (dependency.determinant_column_ids.size() != 1 ||
                    dependency.dependent_column_ids.size() != 1)
                {
                    const bool forward =
                        std::find(dependency.determinant_column_ids.begin(),
                                  dependency.determinant_column_ids.end(),
                                  first_column_id) != dependency.determinant_column_ids.end() &&
                        std::find(dependency.dependent_column_ids.begin(),
                                  dependency.dependent_column_ids.end(),
                                  second_column_id) != dependency.dependent_column_ids.end();
                    const bool reverse =
                        std::find(dependency.determinant_column_ids.begin(),
                                  dependency.determinant_column_ids.end(),
                                  second_column_id) != dependency.determinant_column_ids.end() &&
                        std::find(dependency.dependent_column_ids.begin(),
                                  dependency.dependent_column_ids.end(),
                                  first_column_id) != dependency.dependent_column_ids.end();
                    if (forward || reverse)
                    {
                        strength = std::max(strength, dependency.strength);
                    }
                    continue;
                }

                const core::ID determinant = dependency.determinant_column_ids.front();
                const core::ID dependent = dependency.dependent_column_ids.front();
                if ((determinant == first_column_id && dependent == second_column_id) ||
                    (determinant == second_column_id && dependent == first_column_id))
                {
                    strength = std::max(strength, dependency.strength);
                }
            }
            return std::max(0.0, std::min(1.0, strength));
        }

        auto averageRemainingFrequency(const MultivariateStatistics &stats) -> double
        {
            double mcv_total = 0.0;
            for (const auto &entry : stats.mcv_list)
            {
                mcv_total += entry.frequency;
            }

            if (stats.ndistinct.num_distinct == 0 ||
                stats.ndistinct.num_distinct <= stats.mcv_list.size())
            {
                return 0.0;
            }

            const uint64_t remaining_distinct =
                stats.ndistinct.num_distinct - static_cast<uint64_t>(stats.mcv_list.size());
            if (remaining_distinct == 0)
            {
                return 0.0;
            }

            const double remaining_mass = std::max(0.0, 1.0 - mcv_total);
            return remaining_mass / static_cast<double>(remaining_distinct);
        }

        auto singleColumnMcvOverlapSelectivity(const ColumnStatistics &left_stats,
                                               const ColumnStatistics &right_stats) -> double
        {
            double overlap = 0.0;
            double left_mcv_total = 0.0;
            double right_mcv_total = 0.0;

            for (const auto &left_entry : left_stats.mcv_list)
            {
                left_mcv_total += left_entry.frequency;
                for (const auto &right_entry : right_stats.mcv_list)
                {
                    if (left_entry.value_data == right_entry.value_data)
                    {
                        overlap += static_cast<double>(left_entry.frequency) *
                                   static_cast<double>(right_entry.frequency);
                    }
                }
            }

            for (const auto &right_entry : right_stats.mcv_list)
            {
                right_mcv_total += right_entry.frequency;
            }

            const double left_remaining_mass =
                std::max(0.0, 1.0 - left_mcv_total - left_stats.null_fraction);
            const double right_remaining_mass =
                std::max(0.0, 1.0 - right_mcv_total - right_stats.null_fraction);

            const uint64_t left_remaining_distinct =
                left_stats.num_distinct > left_stats.mcv_list.size()
                    ? (left_stats.num_distinct -
                       static_cast<uint64_t>(left_stats.mcv_list.size()))
                    : 0;
            const uint64_t right_remaining_distinct =
                right_stats.num_distinct > right_stats.mcv_list.size()
                    ? (right_stats.num_distinct -
                       static_cast<uint64_t>(right_stats.mcv_list.size()))
                    : 0;

            double unseen = 0.0;
            if (left_remaining_distinct > 0 && right_remaining_distinct > 0)
            {
                unseen = (left_remaining_mass * right_remaining_mass) /
                         static_cast<double>(
                             std::max(left_remaining_distinct, right_remaining_distinct));
            }

            return std::max(0.0, std::min(1.0, overlap + unseen));
        }

        auto multivariateMcvOverlapSelectivity(const MultivariateStatistics &left_stats,
                                               const std::vector<core::ID> &left_join_columns,
                                               const MultivariateStatistics &right_stats,
                                               const std::vector<core::ID> &right_join_columns)
            -> double
        {
            double overlap = 0.0;
            double left_mcv_total = 0.0;
            double right_mcv_total = 0.0;

            std::vector<std::vector<uint8_t>> left_values;
            std::vector<std::vector<uint8_t>> right_values;

            for (const auto &left_entry : left_stats.mcv_list)
            {
                left_mcv_total += left_entry.frequency;
                if (!reorderMultivariateValues(left_stats.column_ids,
                                               left_entry.values,
                                               left_join_columns,
                                               left_values))
                {
                    continue;
                }

                for (const auto &right_entry : right_stats.mcv_list)
                {
                    if (!reorderMultivariateValues(right_stats.column_ids,
                                                   right_entry.values,
                                                   right_join_columns,
                                                   right_values))
                    {
                        continue;
                    }
                    if (valuesMatch(left_values, right_values))
                    {
                        overlap += static_cast<double>(left_entry.frequency) *
                                   static_cast<double>(right_entry.frequency);
                    }
                }
            }

            for (const auto &right_entry : right_stats.mcv_list)
            {
                right_mcv_total += right_entry.frequency;
            }

            const uint64_t left_remaining_distinct =
                left_stats.ndistinct.num_distinct > left_stats.mcv_list.size()
                    ? (left_stats.ndistinct.num_distinct -
                       static_cast<uint64_t>(left_stats.mcv_list.size()))
                    : 0;
            const uint64_t right_remaining_distinct =
                right_stats.ndistinct.num_distinct > right_stats.mcv_list.size()
                    ? (right_stats.ndistinct.num_distinct -
                       static_cast<uint64_t>(right_stats.mcv_list.size()))
                    : 0;

            double unseen = 0.0;
            if (left_remaining_distinct > 0 && right_remaining_distinct > 0)
            {
                unseen =
                    (std::max(0.0, 1.0 - left_mcv_total) *
                     std::max(0.0, 1.0 - right_mcv_total)) /
                    static_cast<double>(
                        std::max(left_remaining_distinct, right_remaining_distinct));
            }

            return std::max(0.0, std::min(1.0, overlap + unseen));
        }

        auto multivariateEqualitySelectivity(const MultivariateStatistics &stats,
                                             const std::vector<core::ID> &requested_columns,
                                             const std::vector<std::vector<uint8_t>> &requested_values,
                                             const std::vector<double> &individual_selectivities)
            -> std::optional<double>
        {
            if (stats.ndistinct.num_distinct == 0 && stats.mcv_list.empty())
            {
                return std::nullopt;
            }

            std::vector<std::vector<uint8_t>> ordered_values;
            if (!reorderMultivariateValues(requested_columns,
                                           requested_values,
                                           stats.column_ids,
                                           ordered_values))
            {
                return std::nullopt;
            }

            for (const auto &entry : stats.mcv_list)
            {
                if (valuesMatch(entry.values, ordered_values))
                {
                    return clampSelectivity(static_cast<double>(entry.frequency));
                }
            }

            if (stats.ndistinct.num_distinct > 0 &&
                stats.ndistinct.num_distinct <= stats.mcv_list.size())
            {
                return 0.0;
            }

            double selectivity = averageRemainingFrequency(stats);
            if (stats.ndistinct.num_distinct > 0)
            {
                selectivity = std::max(
                    selectivity,
                    1.0 / static_cast<double>(stats.ndistinct.num_distinct));
            }

            double upper_bound = 1.0;
            for (double term_selectivity : individual_selectivities)
            {
                upper_bound = std::min(upper_bound, term_selectivity);
            }

            return clampSelectivity(std::min(upper_bound, selectivity));
        }
    } // namespace

    auto SelectivityEstimator::hasSampledRefresh(const core::ID &table_id) const -> bool
    {
        return std::find(sampled_refresh_tables_.begin(),
                         sampled_refresh_tables_.end(),
                         table_id) != sampled_refresh_tables_.end();
    }

    auto SelectivityEstimator::rememberSampledRefresh(const core::ID &table_id) -> void
    {
        if (!hasSampledRefresh(table_id))
        {
            sampled_refresh_tables_.push_back(table_id);
        }
    }

    auto SelectivityEstimator::maybeRefreshTableSample(const core::ID &table_id,
                                                       const ColumnStatistics &stats,
                                                       core::ErrorContext *ctx)
        -> core::Status
    {
        if (stats_manager_ == nullptr || isZeroId(table_id) || hasSampledRefresh(table_id))
        {
            return core::Status::OK;
        }

        const bool needs_refresh =
            stats.last_analyzed_time == 0 ||
            stats.staleness_class == StatisticsStalenessClass::STALE ||
            stats.staleness_class == StatisticsStalenessClass::EXPIRED ||
            stats.confidence_class == StatisticsConfidenceClass::LOW;
        if (!needs_refresh)
        {
            return core::Status::OK;
        }

        core::ErrorContext refresh_ctx;
        const core::Status refresh_status =
            stats_manager_->analyzeTable(table_id, 0.25f, &refresh_ctx);
        if (refresh_status != core::Status::OK)
        {
            if (ctx != nullptr && ctx->message.empty())
            {
                ctx->message = refresh_ctx.message;
            }
            return refresh_status;
        }

        rememberSampledRefresh(table_id);
        return core::Status::OK;
    }

    auto SelectivityEstimator::ensureColumnStatisticsForEstimation(const core::ID &table_id,
                                                                   const core::ID &column_id,
                                                                   ColumnStatistics &stats,
                                                                   core::ErrorContext *ctx)
        -> core::Status
    {
        if (stats_manager_ == nullptr)
        {
            return core::Status::INVALID_ARGUMENT;
        }

        core::Status status =
            stats_manager_->getColumnStatistics(table_id, column_id, stats, ctx);
        if (status != core::Status::OK)
        {
            return status;
        }

        const bool needs_refresh =
            stats.last_analyzed_time == 0 ||
            stats.staleness_class == StatisticsStalenessClass::STALE ||
            stats.staleness_class == StatisticsStalenessClass::EXPIRED ||
            stats.confidence_class == StatisticsConfidenceClass::LOW;
        if (!needs_refresh)
        {
            return core::Status::OK;
        }

        status = maybeRefreshTableSample(table_id, stats, ctx);
        if (status != core::Status::OK || !hasSampledRefresh(table_id))
        {
            return status;
        }

        return stats_manager_->getColumnStatistics(table_id, column_id, stats, ctx);
    }

    auto SelectivityEstimator::estimateDependencyAwareConjunction(
        const parser::v3::Expression *left_expr,
        const parser::v3::Expression *right_expr,
        const core::ID &table_id,
        const parser::v3::StringPool *pool,
        core::ErrorContext *ctx) -> std::optional<double>
    {
        if (stats_manager_ == nullptr || db_ == nullptr)
        {
            return std::nullopt;
        }

        std::vector<EqualityPredicateTerm> terms;
        const auto collect_terms =
            [&](const auto &self, const parser::v3::Expression *expr) -> bool {
                const auto *current = unwrapCasts(expr);
                if (current == nullptr)
                {
                    return false;
                }

                if (current->kind() == parser::v3::ASTKind::BinaryExpr)
                {
                    const auto *binary = static_cast<const parser::v3::BinaryExpr *>(current);
                    if (binary->op == parser::v3::BinaryOp::AND)
                    {
                        return self(self, binary->left) && self(self, binary->right);
                    }
                }

                EqualityPredicateTerm term;
                if (!resolveEqualityPredicateTerm(db_,
                                                  table_id,
                                                  current,
                                                  pool,
                                                  parameter_bindings_,
                                                  term,
                                                  ctx))
                {
                    return false;
                }

                terms.push_back(std::move(term));
                return true;
            };

        if (!collect_terms(collect_terms, left_expr) ||
            !collect_terms(collect_terms, right_expr))
        {
            return std::nullopt;
        }

        std::vector<EqualityPredicateTerm> unique_terms;
        unique_terms.reserve(terms.size());
        for (auto &term : terms)
        {
            auto existing_it =
                std::find_if(unique_terms.begin(),
                             unique_terms.end(),
                             [&](const EqualityPredicateTerm &existing) {
                                 return existing.column.column_id == term.column.column_id;
                             });
            if (existing_it != unique_terms.end())
            {
                if (existing_it->value != term.value)
                {
                    return 0.0;
                }
                continue;
            }
            unique_terms.push_back(std::move(term));
        }

        if (unique_terms.size() < 2)
        {
            return std::nullopt;
        }

        std::vector<double> term_selectivities;
        term_selectivities.reserve(unique_terms.size());
        double base_selectivity = 1.0;
        for (const auto &term : unique_terms)
        {
            ColumnStatistics column_stats;
            if (ensureColumnStatisticsForEstimation(table_id,
                                                    term.column.column_id,
                                                    column_stats,
                                                    ctx) != core::Status::OK)
            {
                return std::nullopt;
            }

            const double selectivity =
                estimateEquality(table_id, term.column.column_id, term.value, ctx);
            term_selectivities.push_back(selectivity);
            base_selectivity *= selectivity;
        }

        double best_selectivity = clampSelectivity(base_selectivity);
        bool found_enhanced_model = false;
        std::vector<size_t> subset_indices;

        const auto consider_subset =
            [&](const std::vector<size_t> &indices) -> void {
                std::vector<core::ID> subset_columns;
                std::vector<std::vector<uint8_t>> subset_values;
                std::vector<double> subset_term_selectivities;
                subset_columns.reserve(indices.size());
                subset_values.reserve(indices.size());
                subset_term_selectivities.reserve(indices.size());

                double uncovered_product = 1.0;
                for (size_t term_index = 0; term_index < unique_terms.size(); ++term_index)
                {
                    if (std::find(indices.begin(), indices.end(), term_index) !=
                        indices.end())
                    {
                        subset_columns.push_back(unique_terms[term_index].column.column_id);
                        subset_values.push_back(unique_terms[term_index].value);
                        subset_term_selectivities.push_back(term_selectivities[term_index]);
                    }
                    else
                    {
                        uncovered_product *= term_selectivities[term_index];
                    }
                }

                MultivariateStatistics subset_stats;
                if (stats_manager_->getMultivariateStatistics(table_id,
                                                              subset_columns,
                                                              subset_stats,
                                                              ctx) != core::Status::OK)
                {
                    return;
                }

                auto subset_selectivity =
                    multivariateEqualitySelectivity(subset_stats,
                                                    subset_columns,
                                                    subset_values,
                                                    subset_term_selectivities);
                if (!subset_selectivity.has_value())
                {
                    return;
                }

                if (indices.size() == 2)
                {
                    const double dependency_strength =
                        bestDependencyStrength(subset_stats,
                                               subset_columns[0],
                                               subset_columns[1]);
                    if (dependency_strength > 0.0)
                    {
                        const double pair_base =
                            subset_term_selectivities[0] * subset_term_selectivities[1];
                        double dependency_selectivity =
                            pair_base + dependency_strength *
                                            (std::min(subset_term_selectivities[0],
                                                      subset_term_selectivities[1]) -
                                             pair_base);
                        const double average_remaining =
                            averageRemainingFrequency(subset_stats);
                        if (average_remaining > 0.0)
                        {
                            dependency_selectivity =
                                std::max(dependency_selectivity, average_remaining);
                        }
                        subset_selectivity = std::max(
                            *subset_selectivity,
                            clampSelectivity(std::min(std::min(subset_term_selectivities[0],
                                                               subset_term_selectivities[1]),
                                                      dependency_selectivity)));
                    }
                }

                best_selectivity = std::max(
                    best_selectivity,
                    clampSelectivity((*subset_selectivity) * uncovered_product));
                found_enhanced_model = true;
            };

        const auto enumerate_subsets =
            [&](const auto &self, size_t start, size_t target_size) -> void {
                if (subset_indices.size() == target_size)
                {
                    consider_subset(subset_indices);
                    return;
                }

                for (size_t index = start; index < unique_terms.size(); ++index)
                {
                    subset_indices.push_back(index);
                    self(self, index + 1, target_size);
                    subset_indices.pop_back();
                }
            };

        for (size_t target_size = std::min<size_t>(3, unique_terms.size());
             target_size >= 2;
             --target_size)
        {
            enumerate_subsets(enumerate_subsets, 0, target_size);
            if (target_size == 2)
            {
                break;
            }
        }

        if (!found_enhanced_model)
        {
            return std::nullopt;
        }

        return clampSelectivity(best_selectivity);
    }

    auto SelectivityEstimator::estimateMultiColumnJoinSelectivity(
        const parser::v3::Expression *join_condition,
        const core::ID &left_table_id,
        const core::ID &right_table_id,
        const parser::v3::StringPool *pool,
        core::ErrorContext *ctx) -> std::optional<double>
    {
        if (stats_manager_ == nullptr || db_ == nullptr)
        {
            return std::nullopt;
        }

        std::vector<JoinEqualityColumnPair> join_pairs;
        if (!collectEquiJoinPairs(db_,
                                  left_table_id,
                                  right_table_id,
                                  join_condition,
                                  pool,
                                  join_pairs,
                                  ctx) ||
            join_pairs.size() < 2)
        {
            return std::nullopt;
        }

        std::vector<core::ID> left_join_columns;
        std::vector<core::ID> right_join_columns;
        left_join_columns.reserve(join_pairs.size());
        right_join_columns.reserve(join_pairs.size());
        for (const auto &pair : join_pairs)
        {
            left_join_columns.push_back(pair.left_column_id);
            right_join_columns.push_back(pair.right_column_id);
        }

        std::vector<ColumnStatistics> left_column_stats(join_pairs.size());
        std::vector<ColumnStatistics> right_column_stats(join_pairs.size());
        for (size_t index = 0; index < left_join_columns.size(); ++index)
        {
            if (ensureColumnStatisticsForEstimation(left_table_id,
                                                    left_join_columns[index],
                                                    left_column_stats[index],
                                                    ctx) != core::Status::OK)
            {
                return std::nullopt;
            }
        }

        for (size_t index = 0; index < right_join_columns.size(); ++index)
        {
            if (ensureColumnStatisticsForEstimation(right_table_id,
                                                    right_join_columns[index],
                                                    right_column_stats[index],
                                                    ctx) != core::Status::OK)
            {
                return std::nullopt;
            }
        }

        std::vector<double> pair_selectivities;
        pair_selectivities.reserve(join_pairs.size());
        double independence_product = 1.0;
        for (const auto &pair : join_pairs)
        {
            const double pair_selectivity =
                estimateEquiJoinSelectivity(pair.left_column_id,
                                            pair.right_column_id,
                                            left_table_id,
                                            right_table_id,
                                            ctx);
            pair_selectivities.push_back(pair_selectivity);
            independence_product *= pair_selectivity;
        }

        double best_selectivity = clampSelectivity(independence_product);
        bool found_enhanced_model = false;
        std::vector<size_t> subset_indices;

        const auto consider_subset =
            [&](const std::vector<size_t> &indices) -> void {
                std::vector<core::ID> left_subset;
                std::vector<core::ID> right_subset;
                left_subset.reserve(indices.size());
                right_subset.reserve(indices.size());

                double uncovered_product = 1.0;
                for (size_t pair_index = 0; pair_index < join_pairs.size(); ++pair_index)
                {
                    if (std::find(indices.begin(), indices.end(), pair_index) !=
                        indices.end())
                    {
                        left_subset.push_back(join_pairs[pair_index].left_column_id);
                        right_subset.push_back(join_pairs[pair_index].right_column_id);
                    }
                    else
                    {
                        uncovered_product *= pair_selectivities[pair_index];
                    }
                }

                MultivariateStatistics left_subset_stats;
                MultivariateStatistics right_subset_stats;
                if (stats_manager_->getMultivariateStatistics(left_table_id,
                                                              left_subset,
                                                              left_subset_stats,
                                                              ctx) != core::Status::OK ||
                    stats_manager_->getMultivariateStatistics(right_table_id,
                                                              right_subset,
                                                              right_subset_stats,
                                                              ctx) != core::Status::OK)
                {
                    return;
                }

                if ((left_subset_stats.ndistinct.num_distinct == 0 &&
                     left_subset_stats.mcv_list.empty()) ||
                    (right_subset_stats.ndistinct.num_distinct == 0 &&
                     right_subset_stats.mcv_list.empty()))
                {
                    return;
                }

                best_selectivity = std::max(
                    best_selectivity,
                    clampSelectivity(
                        multivariateMcvOverlapSelectivity(left_subset_stats,
                                                          left_subset,
                                                          right_subset_stats,
                                                          right_subset) *
                        uncovered_product));
                found_enhanced_model = true;
            };

        const auto enumerate_subsets =
            [&](const auto &self, size_t start, size_t target_size) -> void {
                if (subset_indices.size() == target_size)
                {
                    consider_subset(subset_indices);
                    return;
                }

                for (size_t index = start; index < join_pairs.size(); ++index)
                {
                    subset_indices.push_back(index);
                    self(self, index + 1, target_size);
                    subset_indices.pop_back();
                }
            };

        for (size_t target_size = std::min<size_t>(3, join_pairs.size());
             target_size >= 2;
             --target_size)
        {
            enumerate_subsets(enumerate_subsets, 0, target_size);
            if (target_size == 2)
            {
                break;
            }
        }

        core::CatalogManager::ForeignKeyInfo matched_fk;
        bool left_is_child = false;
        if (matchesForeignKeyJoin(db_,
                                  left_table_id,
                                  right_table_id,
                                  join_pairs,
                                  matched_fk,
                                  left_is_child,
                                  ctx))
        {
            std::vector<core::ID> parent_columns;
            parent_columns.reserve(join_pairs.size());
            double child_non_null_fraction = 1.0;

            for (size_t index = 0; index < join_pairs.size(); ++index)
            {
                const ColumnStatistics &child_stats =
                    left_is_child ? left_column_stats[index] : right_column_stats[index];
                parent_columns.push_back(left_is_child ? join_pairs[index].right_column_id
                                                       : join_pairs[index].left_column_id);
                child_non_null_fraction *=
                    std::max(0.0, 1.0 - child_stats.null_fraction);
            }

            uint64_t parent_distinct = 0;
            if (parent_columns.size() == 1)
            {
                parent_distinct =
                    left_is_child ? right_column_stats.front().num_distinct
                                  : left_column_stats.front().num_distinct;
            }
            else
            {
                MultivariateStatistics parent_stats;
                const core::ID &parent_table_id =
                    left_is_child ? right_table_id : left_table_id;
                if (stats_manager_->getMultivariateStatistics(parent_table_id,
                                                              parent_columns,
                                                              parent_stats,
                                                              ctx) == core::Status::OK)
                {
                    parent_distinct = parent_stats.ndistinct.num_distinct;
                }
            }

            if (parent_distinct > 0)
            {
                best_selectivity = std::max(
                    best_selectivity,
                    clampSelectivity(
                        child_non_null_fraction /
                        static_cast<double>(parent_distinct)));
                found_enhanced_model = true;
            }
        }

        if (!found_enhanced_model)
        {
            return std::nullopt;
        }

        return clampSelectivity(best_selectivity);
    }

    auto SelectivityEstimator::estimateWhereClause(
        const parser::v3::Expression *where_clause,
        const core::ID &table_id,
        const parser::v3::StringPool *pool,
        core::ErrorContext *ctx)
        -> double
    {
        if (!where_clause)
        {
            // No WHERE clause → all rows match
            return 1.0;
        }

        const auto *expr = unwrapCasts(where_clause);
        if (expr == nullptr)
        {
            return DEFAULT_RANGE_SEL;
        }

        if (expr->kind() == parser::v3::ASTKind::BinaryExpr)
        {
            const auto *binary = static_cast<const parser::v3::BinaryExpr *>(expr);
            if (binary->op == parser::v3::BinaryOp::AND)
            {
                double left_sel = estimateWhereClause(binary->left, table_id, pool, ctx);
                double right_sel = estimateWhereClause(binary->right, table_id, pool, ctx);
                double combined = estimateAnd(left_sel, right_sel);

                if (auto dependency_aware =
                        estimateDependencyAwareConjunction(binary->left,
                                                           binary->right,
                                                           table_id,
                                                           pool,
                                                           ctx);
                    dependency_aware.has_value())
                {
                    return *dependency_aware;
                }

                if (stats_manager_ != nullptr)
                {
                    auto left_column = resolveColumnRef(db_, table_id, binary->left, pool, ctx);
                    auto right_column = resolveColumnRef(db_, table_id, binary->right, pool, ctx);
                    if (left_column.has_value() && right_column.has_value())
                    {
                        ColumnCorrelationStatistics corr;
                        if (stats_manager_->getColumnCorrelation(table_id,
                                                                 left_column->column_id,
                                                                 right_column->column_id,
                                                                 corr,
                                                                 nullptr) == core::Status::OK)
                        {
                            const double magnitude = std::min(1.0, std::abs(corr.coefficient));
                            if (corr.coefficient >= 0.0)
                            {
                                combined += magnitude *
                                            (std::min(left_sel, right_sel) - combined);
                            }
                            else
                            {
                                combined *= std::max(0.1, 1.0 - magnitude * 0.5);
                            }
                            combined = std::max(0.0, std::min(1.0, combined));
                        }
                    }
                }
                return combined;
            }
            if (binary->op == parser::v3::BinaryOp::OR)
            {
                return estimateOr(estimateWhereClause(binary->left, table_id, pool, ctx),
                                  estimateWhereClause(binary->right, table_id, pool, ctx));
            }

            auto estimate_binary_predicate =
                [&](const parser::v3::Expression *column_expr,
                    const parser::v3::Expression *literal_expr,
                    bool reversed_range) -> std::optional<double> {
                    auto column = resolveColumnRef(db_, table_id, column_expr, pool, ctx);
                    if (!column.has_value())
                    {
                        auto expr_descriptor =
                            expressionStatsDescriptor(column_expr, pool);
                        if (!expr_descriptor.has_value() ||
                            binary->op != parser::v3::BinaryOp::EQ ||
                            stats_manager_ == nullptr)
                        {
                            return std::nullopt;
                        }

                        std::vector<uint8_t> expr_value;
                        if (!literalExprToBytes(
                                literal_expr,
                                pool,
                                parameter_bindings_,
                                static_cast<uint16_t>(
                                    expr_descriptor->result_data_type),
                                expr_value))
                        {
                            return std::nullopt;
                        }

                        ExpressionStatistics expr_stats;
                        if (stats_manager_->getExpressionStatistics(table_id,
                                                                    canonicalExpressionStatsKey(
                                                                        *expr_descriptor),
                                                                    expr_stats,
                                                                    ctx) != core::Status::OK)
                        {
                            return std::nullopt;
                        }

                        if (!normalizeLiteralForExpression(expr_value,
                                                           *expr_descriptor))
                        {
                            return std::nullopt;
                        }

                        const auto &stats = expr_stats.stats;
                        if (expr_value.empty())
                        {
                            return static_cast<double>(stats.null_fraction);
                        }
                        for (const auto &mcv : stats.mcv_list)
                        {
                            if (valueEquals(mcv.value_data, expr_value))
                            {
                                return static_cast<double>(mcv.frequency);
                            }
                        }
                        if (stats.num_distinct == 0)
                        {
                            return DEFAULT_EQUALITY_SEL;
                        }
                        double mcv_total = 0.0;
                        for (const auto &mcv : stats.mcv_list)
                        {
                            mcv_total += mcv.frequency;
                        }
                        const uint64_t remaining_distinct =
                            stats.num_distinct > stats.mcv_list.size()
                                ? stats.num_distinct - stats.mcv_list.size()
                                : 0;
                        if (remaining_distinct == 0)
                        {
                            return 0.0;
                        }
                        const double remaining =
                            std::max(0.0, 1.0 - mcv_total - stats.null_fraction);
                        return remaining / static_cast<double>(remaining_distinct);
                    }

                    std::vector<uint8_t> literal_bytes;
                    if (!literalExprToBytes(literal_expr,
                                            pool,
                                            parameter_bindings_,
                                            column->data_type,
                                            literal_bytes))
                    {
                        return std::nullopt;
                    }

                    switch (binary->op)
                    {
                        case parser::v3::BinaryOp::EQ:
                            return estimateEquality(table_id, column->column_id, literal_bytes, ctx);
                        case parser::v3::BinaryOp::LT:
                            return estimateRange(table_id,
                                                 column->column_id,
                                                 reversed_range ? ">" : "<",
                                                 literal_bytes,
                                                 ctx);
                        case parser::v3::BinaryOp::LE:
                            return estimateRange(table_id,
                                                 column->column_id,
                                                 reversed_range ? ">=" : "<=",
                                                 literal_bytes,
                                                 ctx);
                        case parser::v3::BinaryOp::GT:
                            return estimateRange(table_id,
                                                 column->column_id,
                                                 reversed_range ? "<" : ">",
                                                 literal_bytes,
                                                 ctx);
                        case parser::v3::BinaryOp::GE:
                            return estimateRange(table_id,
                                                 column->column_id,
                                                 reversed_range ? "<=" : ">=",
                                                 literal_bytes,
                                                 ctx);
                        default:
                            return std::nullopt;
                    }
                };

            if (auto sel = estimate_binary_predicate(binary->left, binary->right, false))
            {
                return *sel;
            }
            if (auto sel = estimate_binary_predicate(binary->right, binary->left, true))
            {
                return *sel;
            }
        }

        if (expr->kind() == parser::v3::ASTKind::UnaryExpr)
        {
            const auto *unary = static_cast<const parser::v3::UnaryExpr *>(expr);
            if (unary->op == parser::v3::UnaryOp::NOT)
            {
                return estimateNot(estimateWhereClause(unary->operand, table_id, pool, ctx));
            }
            if (unary->op == parser::v3::UnaryOp::IS_NULL ||
                unary->op == parser::v3::UnaryOp::IS_NOT_NULL)
            {
                auto column = resolveColumnRef(db_, table_id, unary->operand, pool, ctx);
                if (!column.has_value() || stats_manager_ == nullptr)
                {
                    return unary->op == parser::v3::UnaryOp::IS_NOT_NULL
                               ? (1.0 - DEFAULT_EQUALITY_SEL)
                               : DEFAULT_EQUALITY_SEL;
                }

                ColumnStatistics col_stats;
                if (stats_manager_->getColumnStatistics(table_id, column->column_id, col_stats, ctx) !=
                    core::Status::OK)
                {
                    return unary->op == parser::v3::UnaryOp::IS_NOT_NULL
                               ? (1.0 - DEFAULT_EQUALITY_SEL)
                               : DEFAULT_EQUALITY_SEL;
                }
                return unary->op == parser::v3::UnaryOp::IS_NOT_NULL
                           ? (1.0 - col_stats.null_fraction)
                           : col_stats.null_fraction;
            }
        }

        if (expr->kind() == parser::v3::ASTKind::BetweenExpr)
        {
            const auto *between = static_cast<const parser::v3::BetweenExpr *>(expr);
            auto column = resolveColumnRef(db_, table_id, between->expr, pool, ctx);
            std::vector<uint8_t> lower_value;
            std::vector<uint8_t> upper_value;
            if (!column.has_value() ||
                !literalExprToBytes(between->low,
                                    pool,
                                    parameter_bindings_,
                                    column->data_type,
                                    lower_value) ||
                !literalExprToBytes(between->high,
                                    pool,
                                    parameter_bindings_,
                                    column->data_type,
                                    upper_value))
            {
                return DEFAULT_RANGE_SEL;
            }
            double sel = estimateBetween(table_id, column->column_id, lower_value, upper_value, ctx);
            return between->negated ? estimateNot(sel) : sel;
        }

        if (expr->kind() == parser::v3::ASTKind::LikeExpr)
        {
            const auto *like = static_cast<const parser::v3::LikeExpr *>(expr);
            auto column = resolveColumnRef(db_, table_id, like->expr, pool, ctx);
            std::string pattern;
            if (!column.has_value() ||
                !literalExprToString(like->pattern,
                                     pool,
                                     parameter_bindings_,
                                     pattern))
            {
                return DEFAULT_LIKE_CONTAINS_SEL;
            }

            double sel = estimateLike(table_id, column->column_id, pattern, ctx);
            return like->negated ? estimateNot(sel) : sel;
        }

        if (expr->kind() == parser::v3::ASTKind::InExpr)
        {
            const auto *in_expr = static_cast<const parser::v3::InExpr *>(expr);
            auto column = resolveColumnRef(db_, table_id, in_expr->expr, pool, ctx);
            if (!column.has_value() || in_expr->has_subquery)
            {
                return DEFAULT_EQUALITY_SEL;
            }

            std::vector<std::vector<uint8_t>> values;
            for (const auto *entry : in_expr->values)
            {
                std::vector<uint8_t> bytes;
                if (literalExprToBytes(entry,
                                       pool,
                                       parameter_bindings_,
                                       column->data_type,
                                       bytes))
                {
                    values.push_back(std::move(bytes));
                }
            }
            if (values.empty())
            {
                return DEFAULT_EQUALITY_SEL;
            }

            double sel = estimateIn(table_id, column->column_id, values, ctx);
            return in_expr->negated ? estimateNot(sel) : sel;
        }

        DEBUG_LOG_DB("Using default WHERE clause selectivity: " +
                     std::to_string(DEFAULT_RANGE_SEL));
        return DEFAULT_RANGE_SEL;
    }

    auto SelectivityEstimator::estimateEquality(
        const core::ID &table_id,
        const core::ID &column_id,
        const std::vector<uint8_t> &value,
        core::ErrorContext *ctx)
        -> double
    {
        DEBUG_LOG_DB("Estimating equality selectivity for column " +
                     column_id.toString());

        // Get column statistics
        ColumnStatistics col_stats;
        core::Status status =
            ensureColumnStatisticsForEstimation(table_id, column_id, col_stats, ctx);

        if (status != core::Status::OK)
        {
            DEBUG_LOG_DB("No statistics available, using default: " +
                         std::to_string(DEFAULT_EQUALITY_SEL));
            return DEFAULT_EQUALITY_SEL;
        }

        // Check if value is NULL
        if (value.empty())
        {
            DEBUG_LOG_DB("NULL value, selectivity = null_fraction: " +
                         std::to_string(col_stats.null_fraction));
            return col_stats.null_fraction;
        }

        // Check MCVs first (most common values have exact frequencies)
        for (const auto &mcv : col_stats.mcv_list)
        {
            if (valueEquals(mcv.value_data, value))
            {
                DEBUG_LOG_DB("Value found in MCV list, frequency: " +
                             std::to_string(mcv.frequency));
                return mcv.frequency;
            }
        }

        // Value not in MCV list
        // Distribute remaining probability uniformly among remaining values
        if (col_stats.num_distinct == 0)
        {
            DEBUG_LOG_DB("num_distinct is 0, using default");
            return DEFAULT_EQUALITY_SEL;
        }

        if (col_stats.num_distinct <= col_stats.mcv_list.size())
        {
            // All distinct values are in MCV list, value not found
            DEBUG_LOG_DB("All distinct values in MCV list, value not found");
            return 0.0;
        }

        // Calculate total MCV frequency
        double mcv_total_freq = 0.0;
        for (const auto &mcv : col_stats.mcv_list)
        {
            mcv_total_freq += mcv.frequency;
        }

        // Remaining probability for non-MCV values
        double remaining_freq = 1.0 - mcv_total_freq - col_stats.null_fraction;
        if (remaining_freq < 0.0)
        {
            remaining_freq = 0.0;
        }

        // Number of non-MCV distinct values
        uint64_t remaining_distinct = col_stats.num_distinct - col_stats.mcv_list.size();
        if (remaining_distinct == 0)
        {
            DEBUG_LOG_DB("No remaining distinct values");
            return 0.0;
        }

        // Uniform distribution among remaining values
        double selectivity = remaining_freq / static_cast<double>(remaining_distinct);

        DEBUG_LOG_DB("Non-MCV value, selectivity: " + std::to_string(selectivity) +
                     " (remaining_freq=" + std::to_string(remaining_freq) +
                     ", remaining_distinct=" + std::to_string(remaining_distinct) + ")");

        return std::max(0.0, std::min(1.0, selectivity));
    }

    auto SelectivityEstimator::estimateRange(
        const core::ID &table_id,
        const core::ID &column_id,
        const std::string &op,
        const std::vector<uint8_t> &value,
        core::ErrorContext *ctx)
        -> double
    {
        DEBUG_LOG_DB("Estimating range selectivity for column " +
                     column_id.toString() + " with operator " + op);

        // Get column statistics
        ColumnStatistics col_stats;
        core::Status status =
            ensureColumnStatisticsForEstimation(table_id, column_id, col_stats, ctx);

        if (status != core::Status::OK || col_stats.histogram_buckets.empty())
        {
            DEBUG_LOG_DB("No histogram available, using default: " +
                         std::to_string(DEFAULT_RANGE_SEL));
            return DEFAULT_RANGE_SEL;
        }

        double selectivity = 0.0;

        // Iterate through histogram buckets
        for (const auto &bucket : col_stats.histogram_buckets)
        {
            int cmp_lower = compareValues(col_stats, value, bucket.lower_bound);
            int cmp_upper = compareValues(col_stats, value, bucket.upper_bound);

            if (op == ">")
            {
                // Query: col > value
                // Include bucket if any values in bucket are > value

                if (cmp_lower >= 0)
                {
                    // value >= bucket.lower_bound
                    if (cmp_upper >= 0)
                    {
                        // value >= bucket.upper_bound → no values in bucket are > value
                        continue;
                    }
                    else
                    {
                        // bucket.lower_bound <= value < bucket.upper_bound
                        // Interpolate: fraction of bucket that is > value
                        double fraction = interpolateBucket(
                            col_stats, value, bucket.lower_bound, bucket.upper_bound);
                        selectivity += bucket.frequency * fraction;
                    }
                }
                else
                {
                    // value < bucket.lower_bound → entire bucket is > value
                    selectivity += bucket.frequency;
                }
            }
            else if (op == ">=")
            {
                // Query: col >= value
                if (cmp_lower > 0)
                {
                    // value > bucket.lower_bound
                    if (cmp_upper >= 0)
                    {
                        // value >= bucket.upper_bound → no values in bucket are >= value
                        continue;
                    }
                    else
                    {
                        // bucket.lower_bound < value < bucket.upper_bound
                        // Interpolate: fraction of bucket that is >= value
                        double fraction = interpolateBucket(
                            col_stats, value, bucket.lower_bound, bucket.upper_bound);
                        selectivity += bucket.frequency * fraction;
                    }
                }
                else
                {
                    // value <= bucket.lower_bound → entire bucket is >= value
                    selectivity += bucket.frequency;
                }
            }
            else if (op == "<")
            {
                // Query: col < value
                if (cmp_upper <= 0)
                {
                    // value <= bucket.upper_bound
                    if (cmp_lower <= 0)
                    {
                        // value <= bucket.lower_bound → no values in bucket are < value
                        continue;
                    }
                    else
                    {
                        // bucket.lower_bound < value <= bucket.upper_bound
                        // Interpolate: fraction of bucket that is < value
                        double fraction = 1.0 - interpolateBucket(
                            col_stats, value, bucket.lower_bound, bucket.upper_bound);
                        selectivity += bucket.frequency * fraction;
                    }
                }
                else
                {
                    // value > bucket.upper_bound → entire bucket is < value
                    selectivity += bucket.frequency;
                }
            }
            else if (op == "<=")
            {
                // Query: col <= value
                if (cmp_upper < 0)
                {
                    // value < bucket.upper_bound
                    if (cmp_lower <= 0)
                    {
                        // value <= bucket.lower_bound → no values in bucket are <= value
                        continue;
                    }
                    else
                    {
                        // bucket.lower_bound < value < bucket.upper_bound
                        // Interpolate: fraction of bucket that is <= value
                        double fraction = 1.0 - interpolateBucket(
                            col_stats, value, bucket.lower_bound, bucket.upper_bound);
                        selectivity += bucket.frequency * fraction;
                    }
                }
                else
                {
                    // value >= bucket.upper_bound → entire bucket is <= value
                    selectivity += bucket.frequency;
                }
            }
        }

        selectivity = std::max(0.0, std::min(1.0, selectivity));

        DEBUG_LOG_DB("Range selectivity: " + std::to_string(selectivity));

        return selectivity;
    }

    auto SelectivityEstimator::estimateBetween(
        const core::ID &table_id,
        const core::ID &column_id,
        const std::vector<uint8_t> &lower_value,
        const std::vector<uint8_t> &upper_value,
        core::ErrorContext *ctx)
        -> double
    {
        DEBUG_LOG_DB("Estimating BETWEEN selectivity");

        // BETWEEN a AND b = (col >= a) - (col > b)
        double sel_ge_lower = estimateRange(table_id, column_id, ">=", lower_value, ctx);
        double sel_gt_upper = estimateRange(table_id, column_id, ">", upper_value, ctx);

        double selectivity = sel_ge_lower - sel_gt_upper;
        selectivity = std::max(0.0, std::min(1.0, selectivity));

        DEBUG_LOG_DB("BETWEEN selectivity: " + std::to_string(selectivity) +
                     " (>= lower: " + std::to_string(sel_ge_lower) +
                     ", > upper: " + std::to_string(sel_gt_upper) + ")");

        return selectivity;
    }

    auto SelectivityEstimator::estimateLike(
        const core::ID &table_id,
        const core::ID &column_id,
        const std::string &pattern,
        core::ErrorContext *ctx)
        -> double
    {
        DEBUG_LOG_DB("Estimating LIKE selectivity for pattern: " + pattern);

        // Match all
        if (pattern == "%")
        {
            return 1.0;
        }

        bool starts_with_wildcard = (pattern.empty() ? false : pattern[0] == '%');
        bool ends_with_wildcard = (pattern.empty() ? false : pattern.back() == '%');

        if (!starts_with_wildcard && ends_with_wildcard)
        {
            // Prefix match: 'John%'
            // Could use histogram range estimation for string prefix
            // For now, use heuristic
            DEBUG_LOG_DB("Prefix LIKE, using default: " +
                         std::to_string(DEFAULT_LIKE_PREFIX_SEL));
            return DEFAULT_LIKE_PREFIX_SEL;
        }
        else if (starts_with_wildcard && !ends_with_wildcard)
        {
            // Suffix match: '%Smith'
            // Cannot use index
            DEBUG_LOG_DB("Suffix LIKE, using default: " +
                         std::to_string(DEFAULT_LIKE_SUFFIX_SEL));
            return DEFAULT_LIKE_SUFFIX_SEL;
        }
        else if (starts_with_wildcard && ends_with_wildcard)
        {
            // Contains match: '%John%'
            // Cannot use index
            DEBUG_LOG_DB("Contains LIKE, using default: " +
                         std::to_string(DEFAULT_LIKE_CONTAINS_SEL));
            return DEFAULT_LIKE_CONTAINS_SEL;
        }
        else
        {
            // Exact match: 'John' (no wildcards)
            // Treat as equality
            DEBUG_LOG_DB("Exact LIKE, treating as equality");
            std::vector<uint8_t> value;
            ColumnStatistics col_stats;
            if (stats_manager_ != nullptr &&
                stats_manager_->getColumnStatistics(table_id, column_id, col_stats, ctx) ==
                    core::Status::OK &&
                effectiveValueEncoding(col_stats) == StatisticsValueEncoding::PLAIN_LENGTH_PREFIXED)
            {
                encodeLengthPrefixedBytes(pattern, value);
            }
            else
            {
                value.assign(pattern.begin(), pattern.end());
            }
            return estimateEquality(table_id, column_id, value, ctx);
        }
    }

    auto SelectivityEstimator::estimateIn(
        const core::ID &table_id,
        const core::ID &column_id,
        const std::vector<std::vector<uint8_t>> &values,
        core::ErrorContext *ctx)
        -> double
    {
        DEBUG_LOG_DB("Estimating IN selectivity for " +
                     std::to_string(values.size()) + " values");

        if (values.empty())
        {
            return 0.0;
        }

        double total_sel = 0.0;

        for (const auto &value : values)
        {
            double sel = estimateEquality(table_id, column_id, value, ctx);
            total_sel += sel;
        }

        // Cap at 1.0
        total_sel = std::min(1.0, total_sel);

        DEBUG_LOG_DB("IN selectivity: " + std::to_string(total_sel));

        return total_sel;
    }

    auto SelectivityEstimator::estimateAnd(double sel1, double sel2) const -> double
    {
        // Independence assumption: P(A AND B) = P(A) * P(B)
        double selectivity = sel1 * sel2;

        DEBUG_LOG_DB("AND selectivity: " + std::to_string(sel1) +
                     " * " + std::to_string(sel2) +
                     " = " + std::to_string(selectivity));

        return selectivity;
    }

    auto SelectivityEstimator::estimateOr(double sel1, double sel2) const -> double
    {
        // P(A OR B) = P(A) + P(B) - P(A AND B)
        //           = P(A) + P(B) - P(A) * P(B)
        double selectivity = sel1 + sel2 - (sel1 * sel2);

        DEBUG_LOG_DB("OR selectivity: " + std::to_string(sel1) +
                     " + " + std::to_string(sel2) +
                     " - (" + std::to_string(sel1) + " * " + std::to_string(sel2) + ")" +
                     " = " + std::to_string(selectivity));

        return selectivity;
    }

    auto SelectivityEstimator::estimateNot(double sel) const -> double
    {
        // P(NOT A) = 1 - P(A)
        double selectivity = 1.0 - sel;

        DEBUG_LOG_DB("NOT selectivity: 1.0 - " + std::to_string(sel) +
                     " = " + std::to_string(selectivity));

        return selectivity;
    }

    // Private helper methods

    auto SelectivityEstimator::compareValues(
        const ColumnStatistics &stats,
        const std::vector<uint8_t> &v1,
        const std::vector<uint8_t> &value_two) const
        -> int
    {
        const StatisticsComparatorFamily family = effectiveComparatorFamily(stats);
        const StatisticsValueEncoding encoding = effectiveValueEncoding(stats);

        if (family == StatisticsComparatorFamily::STRING)
        {
            std::string lhs;
            std::string rhs;
            if (decodeLengthPrefixedString(v1, lhs) && decodeLengthPrefixedString(value_two, rhs))
            {
                if (lhs < rhs) return -1;
                if (lhs > rhs) return 1;
                return 0;
            }
        }
        else if (family == StatisticsComparatorFamily::BINARY &&
                 encoding == StatisticsValueEncoding::PLAIN_LENGTH_PREFIXED)
        {
            std::vector<uint8_t> lhs;
            std::vector<uint8_t> rhs;
            if (decodeLengthPrefixedBytes(v1, lhs) && decodeLengthPrefixedBytes(value_two, rhs))
            {
                return compareByteVectors(lhs, rhs);
            }
        }
        else if (family == StatisticsComparatorFamily::UUID ||
                 family == StatisticsComparatorFamily::BINARY)
        {
            return compareByteVectors(v1, value_two);
        }
        else
        {
            long double lhs_scalar = 0.0L;
            long double rhs_scalar = 0.0L;
            if (decodeComparableScalar(v1, stats.data_type, lhs_scalar) &&
                decodeComparableScalar(value_two, stats.data_type, rhs_scalar))
            {
                if (lhs_scalar < rhs_scalar) return -1;
                if (lhs_scalar > rhs_scalar) return 1;
                return 0;
            }
        }

        return compareByteVectors(v1, value_two);
    }

    auto SelectivityEstimator::valueEquals(
        const std::vector<uint8_t> &v1,
        const std::vector<uint8_t> &value_two) const
        -> bool
    {
        return v1 == value_two;
    }

    auto SelectivityEstimator::interpolateBucket(
        const ColumnStatistics &stats,
        const std::vector<uint8_t> &value,
        const std::vector<uint8_t> &bucket_min,
        const std::vector<uint8_t> &bucket_max) const
        -> double
    {
        const StatisticsComparatorFamily family = effectiveComparatorFamily(stats);
        if (family == StatisticsComparatorFamily::STRING)
        {
            std::string current;
            std::string lower;
            std::string upper;
            if (decodeLengthPrefixedString(value, current) &&
                decodeLengthPrefixedString(bucket_min, lower) &&
                decodeLengthPrefixedString(bucket_max, upper))
            {
                const long double lower_rank = prefixRank(lower);
                const long double upper_rank = prefixRank(upper);
                const long double current_rank = prefixRank(current);
                if (upper_rank <= lower_rank)
                {
                    return 0.5;
                }
                const long double fraction =
                    (upper_rank - current_rank) / (upper_rank - lower_rank);
                return std::max(0.0, std::min(1.0, static_cast<double>(fraction)));
            }
        }
        else if (family == StatisticsComparatorFamily::UUID ||
                 family == StatisticsComparatorFamily::BINARY)
        {
            const long double lower_rank = prefixRank(bucket_min);
            const long double upper_rank = prefixRank(bucket_max);
            const long double current_rank = prefixRank(value);
            if (upper_rank <= lower_rank)
            {
                return 0.5;
            }
            const long double fraction =
                (upper_rank - current_rank) / (upper_rank - lower_rank);
            return std::max(0.0, std::min(1.0, static_cast<double>(fraction)));
        }

        long double current_value = 0.0L;
        long double min_value = 0.0L;
        long double max_value = 0.0L;
        if (!decodeComparableScalar(value, stats.data_type, current_value) ||
            !decodeComparableScalar(bucket_min, stats.data_type, min_value) ||
            !decodeComparableScalar(bucket_max, stats.data_type, max_value) ||
            max_value <= min_value)
        {
            return 0.5;
        }

        const long double fraction =
            (max_value - current_value) / (max_value - min_value);
        return std::max(0.0, std::min(1.0, static_cast<double>(fraction)));
    }

    auto SelectivityEstimator::estimateJoinSelectivity(
        const parser::v3::Expression* join_condition,
        const core::ID& left_table_id,
        const core::ID& right_table_id,
        const parser::v3::StringPool *pool,
        core::ErrorContext* ctx)
        -> double
    {
        if (!join_condition)
        {
            // CROSS JOIN or NATURAL JOIN without condition
            // Return 1.0 for now (Cartesian product)
            DEBUG_LOG_DB("No join condition, using selectivity 1.0");
            return 1.0;
        }

        const auto *expr = unwrapCasts(join_condition);
        if (auto* binary_expr = dynamic_cast<const parser::v3::BinaryExpr*>(expr))
        {
            if (binary_expr->op == parser::v3::BinaryOp::AND)
            {
                if (auto multivariate_sel =
                        estimateMultiColumnJoinSelectivity(join_condition,
                                                           left_table_id,
                                                           right_table_id,
                                                           pool,
                                                           ctx);
                    multivariate_sel.has_value())
                {
                    return *multivariate_sel;
                }
            }

            // Check for equality (equi-join)
            if (binary_expr->op == parser::v3::BinaryOp::EQ)
            {
                core::ID left_column_id{};
                core::ID right_column_id{};
                if (resolveColumnPairForJoin(db_,
                                             left_table_id,
                                             right_table_id,
                                             binary_expr->left,
                                             binary_expr->right,
                                             pool,
                                             left_column_id,
                                             right_column_id,
                                             ctx))
                {
                    return estimateEquiJoinSelectivity(left_column_id,
                                                       right_column_id,
                                                       left_table_id,
                                                       right_table_id,
                                                       ctx);
                }
                DEBUG_LOG_DB("Equi-join detected without resolved column IDs, using default selectivity: " +
                             std::to_string(DEFAULT_EQUALITY_SEL));
                return DEFAULT_EQUALITY_SEL;
            }
            // Range join (>, <, >=, <=)
            else if (binary_expr->op == parser::v3::BinaryOp::GT ||
                    binary_expr->op == parser::v3::BinaryOp::LT ||
                    binary_expr->op == parser::v3::BinaryOp::GE ||
                    binary_expr->op == parser::v3::BinaryOp::LE)
            {
                DEBUG_LOG_DB("Range join detected, using default selectivity: " +
                           std::to_string(DEFAULT_RANGE_SEL));
                return DEFAULT_RANGE_SEL;
            }
            // AND - multiply selectivities (independence assumption)
            else if (binary_expr->op == parser::v3::BinaryOp::AND)
            {
                double left_sel = estimateJoinSelectivity(
                    binary_expr->left, left_table_id, right_table_id, pool, ctx);
                double right_sel = estimateJoinSelectivity(
                    binary_expr->right, left_table_id, right_table_id, pool, ctx);

                double combined = left_sel * right_sel;
                DEBUG_LOG_DB("AND join condition: " + std::to_string(left_sel) +
                           " * " + std::to_string(right_sel) +
                           " = " + std::to_string(combined));
                return combined;
            }
            // OR - add selectivities and subtract overlap
            else if (binary_expr->op == parser::v3::BinaryOp::OR)
            {
                double left_sel = estimateJoinSelectivity(
                    binary_expr->left, left_table_id, right_table_id, pool, ctx);
                double right_sel = estimateJoinSelectivity(
                    binary_expr->right, left_table_id, right_table_id, pool, ctx);

                // sel(A OR B) = sel(A) + sel(B) - sel(A) * sel(B)
                double combined = left_sel + right_sel - (left_sel * right_sel);
                DEBUG_LOG_DB("OR join condition: " + std::to_string(left_sel) +
                           " + " + std::to_string(right_sel) +
                           " - overlap = " + std::to_string(combined));
                return combined;
            }
        }

        // Default for unknown join conditions
        DEBUG_LOG_DB("Unknown join condition type, using default: " +
                   std::to_string(DEFAULT_RANGE_SEL));
        return DEFAULT_RANGE_SEL;
    }

    auto SelectivityEstimator::estimateEquiJoinSelectivity(
        const core::ID& left_col_id,
        const core::ID& right_col_id,
        const core::ID& left_table_id,
        const core::ID& right_table_id,
        core::ErrorContext* ctx)
        -> double
    {
        DEBUG_LOG_DB("Estimating equi-join selectivity");

        // Get statistics for both columns
        ColumnStatistics left_stats, right_stats;

        core::Status left_status =
            ensureColumnStatisticsForEstimation(left_table_id, left_col_id, left_stats, ctx);
        core::Status right_status =
            ensureColumnStatisticsForEstimation(right_table_id, right_col_id, right_stats, ctx);

        if (left_status != core::Status::OK || right_status != core::Status::OK)
        {
            // No statistics available, use default
            DEBUG_LOG_DB("No statistics for join columns, using default: " +
                       std::to_string(DEFAULT_EQUALITY_SEL));
            return DEFAULT_EQUALITY_SEL;
        }

        double selectivity = 0.0;
        if (left_stats.num_distinct > 0 || right_stats.num_distinct > 0 ||
            !left_stats.mcv_list.empty() || !right_stats.mcv_list.empty())
        {
            selectivity = singleColumnMcvOverlapSelectivity(left_stats, right_stats);
        }
        else
        {
            // Equi-join selectivity formula fallback:
            // selectivity = 1 / MAX(n_distinct(left_col), n_distinct(right_col))
            uint64_t max_distinct = std::max(left_stats.num_distinct, right_stats.num_distinct);

            if (max_distinct == 0)
            {
                // Avoid division by zero
                DEBUG_LOG_DB("n_distinct is 0, using default: " +
                           std::to_string(DEFAULT_EQUALITY_SEL));
                selectivity = DEFAULT_EQUALITY_SEL;
            }
            else
            {
                selectivity = 1.0 / static_cast<double>(max_distinct);
            }
        }

        core::CatalogManager::ForeignKeyInfo matched_fk;
        bool left_is_child = false;
        if (matchesForeignKeyJoin(db_,
                                  left_table_id,
                                  right_table_id,
                                  {{left_col_id, right_col_id}},
                                  matched_fk,
                                  left_is_child,
                                  ctx))
        {
            const ColumnStatistics &child_stats = left_is_child ? left_stats : right_stats;
            const ColumnStatistics &parent_stats = left_is_child ? right_stats : left_stats;
            if (parent_stats.num_distinct > 0)
            {
                selectivity = std::max(
                    selectivity,
                    clampSelectivity(
                        std::max(0.0, 1.0 - child_stats.null_fraction) /
                        static_cast<double>(parent_stats.num_distinct)));
            }
        }

        DEBUG_LOG_DB("Equi-join selectivity: 1 / MAX(" +
                   std::to_string(left_stats.n_distinct) + ", " +
                   std::to_string(right_stats.n_distinct) + ") = " +
                   std::to_string(selectivity));

        return clampSelectivity(selectivity);
    }

} // namespace scratchbird::optimizer
