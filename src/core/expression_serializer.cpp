/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/expression_serializer.h"
#include <cstring>
#include <stdexcept>

namespace scratchbird::core
{
    // ========================================================================
    // Helper Functions - Write
    // ========================================================================

    void ExpressionSerializer::writeU8(std::vector<uint8_t> &buffer, uint8_t value)
    {
        buffer.push_back(value);
    }

    void ExpressionSerializer::writeU16(std::vector<uint8_t> &buffer, uint16_t value)
    {
        buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    }

    void ExpressionSerializer::writeU32(std::vector<uint8_t> &buffer, uint32_t value)
    {
        buffer.push_back((value >> 24) & 0xFF);
        buffer.push_back((value >> 16) & 0xFF);
        buffer.push_back((value >> 8) & 0xFF);
        buffer.push_back(value & 0xFF);
    }

    void ExpressionSerializer::writeU64(std::vector<uint8_t> &buffer, uint64_t value)
    {
        buffer.push_back((value >> 56) & 0xFF);
        buffer.push_back((value >> 48) & 0xFF);
        buffer.push_back((value >> 40) & 0xFF);
        buffer.push_back((value >> 32) & 0xFF);
        buffer.push_back((value >> 24) & 0xFF);
        buffer.push_back((value >> 16) & 0xFF);
        buffer.push_back((value >> 8) & 0xFF);
        buffer.push_back(value & 0xFF);
    }

    void ExpressionSerializer::writeString(std::vector<uint8_t> &buffer, const std::string &str)
    {
        writeU32(buffer, static_cast<uint32_t>(str.length()));
        buffer.insert(buffer.end(), str.begin(), str.end());
    }

    void ExpressionSerializer::writeI64(std::vector<uint8_t> &buffer, int64_t value)
    {
        writeU64(buffer, static_cast<uint64_t>(value));
    }

    void ExpressionSerializer::writeF64(std::vector<uint8_t> &buffer, double value)
    {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(double));
        writeU64(buffer, bits);
    }

    // ========================================================================
    // Helper Functions - Read
    // ========================================================================

    uint8_t ExpressionSerializer::readU8(const uint8_t *&ptr, const uint8_t *end)
    {
        if (ptr >= end)
            throw std::runtime_error("Unexpected end of expression data");
        return *ptr++;
    }

    uint16_t ExpressionSerializer::readU16(const uint8_t *&ptr, const uint8_t *end)
    {
        if (ptr + 2 > end)
            throw std::runtime_error("Unexpected end of expression data");
        uint16_t value = (static_cast<uint16_t>(ptr[0]) << 8) |
                         static_cast<uint16_t>(ptr[1]);
        ptr += 2;
        return value;
    }

    uint32_t ExpressionSerializer::readU32(const uint8_t *&ptr, const uint8_t *end)
    {
        if (ptr + 4 > end)
            throw std::runtime_error("Unexpected end of expression data");
        uint32_t value = (static_cast<uint32_t>(ptr[0]) << 24) |
                         (static_cast<uint32_t>(ptr[1]) << 16) |
                         (static_cast<uint32_t>(ptr[2]) << 8) | static_cast<uint32_t>(ptr[3]);
        ptr += 4;
        return value;
    }

    uint64_t ExpressionSerializer::readU64(const uint8_t *&ptr, const uint8_t *end)
    {
        if (ptr + 8 > end)
            throw std::runtime_error("Unexpected end of expression data");
        uint64_t value = (static_cast<uint64_t>(ptr[0]) << 56) |
                         (static_cast<uint64_t>(ptr[1]) << 48) |
                         (static_cast<uint64_t>(ptr[2]) << 40) |
                         (static_cast<uint64_t>(ptr[3]) << 32) |
                         (static_cast<uint64_t>(ptr[4]) << 24) |
                         (static_cast<uint64_t>(ptr[5]) << 16) |
                         (static_cast<uint64_t>(ptr[6]) << 8) | static_cast<uint64_t>(ptr[7]);
        ptr += 8;
        return value;
    }

    std::string ExpressionSerializer::readString(const uint8_t *&ptr, const uint8_t *end)
    {
        uint32_t len = readU32(ptr, end);
        if (ptr + len > end)
            throw std::runtime_error("String length exceeds available data");
        std::string str(reinterpret_cast<const char *>(ptr), len);
        ptr += len;
        return str;
    }

    int64_t ExpressionSerializer::readI64(const uint8_t *&ptr, const uint8_t *end)
    {
        uint64_t uvalue = readU64(ptr, end);
        return static_cast<int64_t>(uvalue);
    }

    double ExpressionSerializer::readF64(const uint8_t *&ptr, const uint8_t *end)
    {
        uint64_t bits = readU64(ptr, end);
        double value;
        std::memcpy(&value, &bits, sizeof(double));
        return value;
    }

    // ========================================================================
    // Main Serialization
    // ========================================================================

    std::vector<uint8_t> ExpressionSerializer::serialize(const Expression *expr)
    {
        std::vector<uint8_t> buffer;
        writeU8(buffer, FORMAT_VERSION);
        serializeNode(expr, buffer);
        return buffer;
    }

    std::vector<uint8_t> ExpressionSerializer::serializeList(
        const std::vector<Expression *> &expressions)
    {
        std::vector<uint8_t> buffer;
        writeU8(buffer, FORMAT_VERSION);
        writeU32(buffer, static_cast<uint32_t>(expressions.size()));
        for (auto *expr : expressions)
        {
            serializeNode(expr, buffer);
        }
        return buffer;
    }

    void ExpressionSerializer::serializeNode(const Expression *expr, std::vector<uint8_t> &buffer)
    {
        if (!expr)
        {
            writeU8(buffer, 0);
            return;
        }

        switch (expr->kind())
        {
        case ExprKind::LITERAL:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::LITERAL));
            serializeLiteral(static_cast<const LiteralExpr *>(expr), buffer);
            break;

        case ExprKind::IDENTIFIER:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::IDENTIFIER));
            serializeIdentifier(static_cast<const IdentifierExpr *>(expr), buffer);
            break;

        case ExprKind::BINARY_OP:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::BINARY_OP));
            serializeBinaryOp(static_cast<const BinaryOpExpr *>(expr), buffer);
            break;

        case ExprKind::FUNCTION_CALL:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::FUNCTION_CALL));
            serializeFunctionCall(static_cast<const FunctionCallExpr *>(expr), buffer);
            break;

        case ExprKind::CAST:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::CAST));
            serializeCast(static_cast<const CastExpr *>(expr), buffer);
            break;

        case ExprKind::CASE:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::CASE));
            serializeCase(static_cast<const CaseExpr *>(expr), buffer);
            break;

        case ExprKind::AGGREGATE:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::AGGREGATE));
            serializeAggregate(static_cast<const AggregateExpr *>(expr), buffer);
            break;

        case ExprKind::COALESCE:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::COALESCE));
            serializeCoalesce(static_cast<const CoalesceExpr *>(expr), buffer);
            break;

        case ExprKind::NULLIF:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::NULLIF));
            serializeNullIf(static_cast<const NullIfExpr *>(expr), buffer);
            break;

        case ExprKind::EXTRACT:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::EXTRACT));
            serializeExtract(static_cast<const ExtractExpr *>(expr), buffer);
            break;

        case ExprKind::ALTER_ELEMENT:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::ALTER_ELEMENT));
            serializeAlterElement(static_cast<const AlterElementExpr *>(expr), buffer);
            break;

        default:
            throw std::runtime_error("Unsupported expression type for serialization");
        }
    }

    // ========================================================================
    // Type-Specific Serialization
    // ========================================================================

    void ExpressionSerializer::serializeLiteral(const LiteralExpr *expr,
                                                std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0);
        writeU8(buffer, static_cast<uint8_t>(expr->literalType()));

        switch (expr->literalType())
        {
        case LiteralExpr::LiteralType::INTEGER:
            writeI64(buffer, expr->intValue());
            break;
        case LiteralExpr::LiteralType::FLOAT:
            writeF64(buffer, expr->floatValue());
            break;
        case LiteralExpr::LiteralType::STRING:
            writeString(buffer, expr->stringValue());
            break;
        case LiteralExpr::LiteralType::NULL_LITERAL:
            break;
        case LiteralExpr::LiteralType::RANGE:
            writeString(buffer, expr->rangeValue());
            break;
        }
    }

    void ExpressionSerializer::serializeIdentifier(const IdentifierExpr *expr,
                                                   std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0);
        writeString(buffer, expr->name());
        writeU8(buffer, expr->isQualified() ? 1 : 0);
        if (expr->isQualified())
        {
            writeString(buffer, expr->qualifier());
        }
    }

    void ExpressionSerializer::serializeBinaryOp(const BinaryOpExpr *expr,
                                                 std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0);
        writeU8(buffer, static_cast<uint8_t>(expr->op()));
        serializeNode(expr->left(), buffer);
        serializeNode(expr->right(), buffer);
    }

    void ExpressionSerializer::serializeFunctionCall(const FunctionCallExpr *expr,
                                                     std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0);
        writeString(buffer, expr->name());

        const auto &args = expr->args();
        writeU8(buffer, static_cast<uint8_t>(args.size()));
        for (const auto &arg : args)
        {
            serializeNode(arg.get(), buffer);
        }
    }

    void ExpressionSerializer::serializeCast(const CastExpr *expr, std::vector<uint8_t> &buffer)
    {
        uint8_t flags = 0;
        if (expr->isTryCast())
        {
            flags |= 0x01;
        }
        writeU8(buffer, flags);
        writeU8(buffer, static_cast<uint8_t>(expr->format()));

        const TypeInfo &target = expr->targetType();
        writeU16(buffer, static_cast<uint16_t>(target.type));
        writeU32(buffer, target.precision);
        writeU32(buffer, target.scale);
        writeU16(buffer, static_cast<uint16_t>(target.element_type));
        writeU8(buffer, target.with_timezone ? 1 : 0);
        writeU16(buffer, target.timezone_hint);

        serializeNode(expr->expr(), buffer);
    }

    void ExpressionSerializer::serializeCase(const CaseExpr *expr, std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0);

        const auto &whens = expr->whenClauses();
        writeU8(buffer, static_cast<uint8_t>(whens.size()));
        for (const auto &when : whens)
        {
            serializeNode(when.condition.get(), buffer);
            serializeNode(when.result.get(), buffer);
        }

        serializeNode(expr->elseResult(), buffer);
    }

    void ExpressionSerializer::serializeAggregate(const AggregateExpr *expr,
                                                  std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0);
        writeU8(buffer, static_cast<uint8_t>(expr->func()));
        writeU8(buffer, expr->distinct() ? 1 : 0);
        serializeNode(expr->arg(), buffer);
    }

    void ExpressionSerializer::serializeCoalesce(const CoalesceExpr *expr,
                                                 std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0);

        const auto &args = expr->args();
        writeU8(buffer, static_cast<uint8_t>(args.size()));
        for (const auto &arg : args)
        {
            serializeNode(arg.get(), buffer);
        }
    }

    void ExpressionSerializer::serializeNullIf(const NullIfExpr *expr,
                                               std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0);
        serializeNode(expr->expr1(), buffer);
        serializeNode(expr->expr2(), buffer);
    }

    void ExpressionSerializer::serializeExtract(const ExtractExpr *expr,
                                                std::vector<uint8_t> &buffer)
    {
        uint8_t flags = expr->args().empty() ? 0 : 0x01;
        writeU8(buffer, flags);
        writeU8(buffer, expr->fieldId());
        writeString(buffer, expr->fieldName());
        if (flags & 0x01)
        {
            const auto &args = expr->args();
            writeU8(buffer, static_cast<uint8_t>(args.size()));
            for (const auto &arg : args)
            {
                serializeNode(arg.get(), buffer);
            }
        }
        serializeNode(expr->source(), buffer);
    }

    void ExpressionSerializer::serializeAlterElement(const AlterElementExpr *expr,
                                                     std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0);
        writeU8(buffer, expr->fieldId());
        writeString(buffer, expr->fieldName());
        const auto &args = expr->args();
        writeU8(buffer, static_cast<uint8_t>(args.size()));
        for (const auto &arg : args)
        {
            serializeNode(arg.get(), buffer);
        }
        serializeNode(expr->source(), buffer);
        serializeNode(expr->newValue(), buffer);
    }

    // ========================================================================
    // Main Deserialization
    // ========================================================================

    std::unique_ptr<Expression> ExpressionSerializer::deserialize(const uint8_t *data, size_t len)
    {
        const uint8_t *ptr = data;
        const uint8_t *end = data + len;

        uint8_t version = readU8(ptr, end);
        if (version != FORMAT_VERSION)
        {
            throw std::runtime_error("Unsupported expression format version");
        }

        return deserializeNode(ptr, end);
    }

    std::vector<std::unique_ptr<Expression>> ExpressionSerializer::deserializeList(const uint8_t *data, size_t len)
    {
        const uint8_t *ptr = data;
        const uint8_t *end = data + len;

        uint8_t version = readU8(ptr, end);
        if (version != FORMAT_VERSION)
        {
            throw std::runtime_error("Unsupported expression format version");
        }

        uint32_t count = readU32(ptr, end);
        std::vector<std::unique_ptr<Expression>> expressions;
        expressions.reserve(count);

        for (uint32_t i = 0; i < count; i++)
        {
            expressions.push_back(deserializeNode(ptr, end));
        }

        return expressions;
    }

    std::unique_ptr<Expression> ExpressionSerializer::deserializeNode(const uint8_t *&ptr, const uint8_t *end)
    {
        uint8_t type_byte = readU8(ptr, end);
        if (type_byte == 0)
        {
            return nullptr;
        }

        SerializedNodeType type = static_cast<SerializedNodeType>(type_byte);

        switch (type)
        {
        case SerializedNodeType::LITERAL:
            return deserializeLiteral(ptr, end);

        case SerializedNodeType::IDENTIFIER:
            return deserializeIdentifier(ptr, end);

        case SerializedNodeType::BINARY_OP:
            return deserializeBinaryOp(ptr, end);

        case SerializedNodeType::FUNCTION_CALL:
            return deserializeFunctionCall(ptr, end);

        case SerializedNodeType::CAST:
            return deserializeCast(ptr, end);

        case SerializedNodeType::CASE:
            return deserializeCase(ptr, end);

        case SerializedNodeType::AGGREGATE:
            return deserializeAggregate(ptr, end);

        case SerializedNodeType::COALESCE:
            return deserializeCoalesce(ptr, end);

        case SerializedNodeType::NULLIF:
            return deserializeNullIf(ptr, end);

        case SerializedNodeType::EXTRACT:
            return deserializeExtract(ptr, end);

        case SerializedNodeType::ALTER_ELEMENT:
            return deserializeAlterElement(ptr, end);

        default:
            throw std::runtime_error("Unknown expression node type");
        }
    }

    // ========================================================================
    // Type-Specific Deserialization
    // ========================================================================

    std::unique_ptr<Expression> ExpressionSerializer::deserializeLiteral(const uint8_t *&ptr, const uint8_t *end)
    {
        readU8(ptr, end); // flags

        auto type = static_cast<LiteralExpr::LiteralType>(readU8(ptr, end));
        auto expr = std::make_unique<LiteralExpr>(type);

        switch (type)
        {
        case LiteralExpr::LiteralType::INTEGER:
            expr->setIntValue(readI64(ptr, end));
            break;
        case LiteralExpr::LiteralType::FLOAT:
            expr->setFloatValue(readF64(ptr, end));
            break;
        case LiteralExpr::LiteralType::STRING:
            expr->setStringValue(readString(ptr, end));
            break;
        case LiteralExpr::LiteralType::NULL_LITERAL:
            break;
        case LiteralExpr::LiteralType::RANGE:
            expr->setRangeValue(readString(ptr, end));
            break;
        }

        return expr;
    }

    std::unique_ptr<Expression> ExpressionSerializer::deserializeIdentifier(const uint8_t *&ptr, const uint8_t *end)
    {
        readU8(ptr, end); // flags

        std::string name = readString(ptr, end);
        bool has_qualifier = readU8(ptr, end) != 0;
        if (has_qualifier)
        {
            std::string qualifier = readString(ptr, end);
            return std::make_unique<IdentifierExpr>(std::move(qualifier), std::move(name));
        }

        return std::make_unique<IdentifierExpr>(std::move(name));
    }

    std::unique_ptr<Expression> ExpressionSerializer::deserializeBinaryOp(const uint8_t *&ptr, const uint8_t *end)
    {
        readU8(ptr, end); // flags

        auto op = static_cast<BinaryOp>(readU8(ptr, end));
        auto left = deserializeNode(ptr, end);
        auto right = deserializeNode(ptr, end);
        return std::make_unique<BinaryOpExpr>(op, std::move(left), std::move(right));
    }

    std::unique_ptr<Expression> ExpressionSerializer::deserializeFunctionCall(const uint8_t *&ptr, const uint8_t *end)
    {
        readU8(ptr, end); // flags

        std::string func_name = readString(ptr, end);
        uint8_t arg_count = readU8(ptr, end);

        std::vector<std::unique_ptr<Expression>> args;
        args.reserve(arg_count);
        for (uint8_t i = 0; i < arg_count; i++)
        {
            args.push_back(deserializeNode(ptr, end));
        }

        return std::make_unique<FunctionCallExpr>(std::move(func_name), std::move(args));
    }

    std::unique_ptr<Expression> ExpressionSerializer::deserializeCast(const uint8_t *&ptr, const uint8_t *end)
    {
        uint8_t flags = readU8(ptr, end);
        auto format = static_cast<CastFormat>(readU8(ptr, end));
        bool is_try_cast = (flags & 0x01) != 0;

        TypeInfo target;
        target.type = static_cast<DataType>(readU16(ptr, end));
        target.precision = readU32(ptr, end);
        target.scale = readU32(ptr, end);
        target.element_type = static_cast<DataType>(readU16(ptr, end));
        target.with_timezone = readU8(ptr, end) != 0;
        target.timezone_hint = readU16(ptr, end);

        auto expr = deserializeNode(ptr, end);
        return std::make_unique<CastExpr>(std::move(expr), target, is_try_cast, format);
    }

    std::unique_ptr<Expression> ExpressionSerializer::deserializeCase(const uint8_t *&ptr, const uint8_t *end)
    {
        readU8(ptr, end); // flags

        uint8_t when_count = readU8(ptr, end);
        std::vector<CaseExpr::WhenClause> whens;
        whens.reserve(when_count);

        for (uint8_t i = 0; i < when_count; i++)
        {
            CaseExpr::WhenClause clause;
            clause.condition = deserializeNode(ptr, end);
            clause.result = deserializeNode(ptr, end);
            whens.push_back(std::move(clause));
        }

        auto else_expr = deserializeNode(ptr, end);
        return std::make_unique<CaseExpr>(std::move(whens), std::move(else_expr));
    }

    std::unique_ptr<Expression> ExpressionSerializer::deserializeAggregate(const uint8_t *&ptr, const uint8_t *end)
    {
        readU8(ptr, end); // flags

        auto func = static_cast<AggregateFunc>(readU8(ptr, end));
        bool distinct = readU8(ptr, end) != 0;
        auto arg = deserializeNode(ptr, end);
        return std::make_unique<AggregateExpr>(func, std::move(arg), distinct);
    }

    std::unique_ptr<Expression> ExpressionSerializer::deserializeCoalesce(const uint8_t *&ptr, const uint8_t *end)
    {
        readU8(ptr, end); // flags

        uint8_t arg_count = readU8(ptr, end);
        std::vector<std::unique_ptr<Expression>> args;
        args.reserve(arg_count);
        for (uint8_t i = 0; i < arg_count; i++)
        {
            args.push_back(deserializeNode(ptr, end));
        }

        return std::make_unique<CoalesceExpr>(std::move(args));
    }

    std::unique_ptr<Expression> ExpressionSerializer::deserializeNullIf(const uint8_t *&ptr, const uint8_t *end)
    {
        readU8(ptr, end); // flags

        auto expr1 = deserializeNode(ptr, end);
        auto expr2 = deserializeNode(ptr, end);
        return std::make_unique<NullIfExpr>(std::move(expr1), std::move(expr2));
    }

    std::unique_ptr<Expression> ExpressionSerializer::deserializeExtract(const uint8_t *&ptr, const uint8_t *end)
    {
        uint8_t flags = readU8(ptr, end);

        uint8_t field_id = readU8(ptr, end);
        std::string field_name = readString(ptr, end);
        std::vector<std::unique_ptr<Expression>> args;
        if (flags & 0x01)
        {
            uint8_t arg_count = readU8(ptr, end);
            args.reserve(arg_count);
            for (uint8_t i = 0; i < arg_count; i++)
            {
                args.push_back(deserializeNode(ptr, end));
            }
        }
        auto source = deserializeNode(ptr, end);
        return std::make_unique<ExtractExpr>(field_id, std::move(field_name),
                                             std::move(args), std::move(source));
    }

    std::unique_ptr<Expression> ExpressionSerializer::deserializeAlterElement(const uint8_t *&ptr,
                                                                              const uint8_t *end)
    {
        readU8(ptr, end); // flags

        uint8_t field_id = readU8(ptr, end);
        std::string field_name = readString(ptr, end);
        uint8_t arg_count = readU8(ptr, end);
        std::vector<std::unique_ptr<Expression>> args;
        args.reserve(arg_count);
        for (uint8_t i = 0; i < arg_count; i++)
        {
            args.push_back(deserializeNode(ptr, end));
        }
        auto source = deserializeNode(ptr, end);
        auto new_value = deserializeNode(ptr, end);
        return std::make_unique<AlterElementExpr>(field_id, std::move(field_name),
                                                  std::move(args), std::move(source),
                                                  std::move(new_value));
    }

} // namespace scratchbird::core
