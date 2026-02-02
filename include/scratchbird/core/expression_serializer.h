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

#include "scratchbird/core/expression.h"
#include <vector>
#include <cstdint>
#include <memory>

/**
 * Expression Serializer for Task 17: Expression and Filtered Indexes
 *
 * Serializes expression AST trees to binary format for storage in catalog TOAST tables.
 * Deserializes binary data back to expression AST for evaluation.
 *
 * Binary Format:
 * [Version: 1 byte] - Format version (currently 1)
 * [Node Type: 1 byte] - Expression node type
 * [Flags: 1 byte] - Reserved for future use
 * [Data Length: 4 bytes] - Length of node-specific data
 * [Node Data: variable] - Node-specific data (depends on type)
 * [Child Count: 1 byte] - Number of child expressions
 * [Children: recursive] - Serialized child expressions
 */

namespace scratchbird::core
{
    class ExpressionSerializer
    {
    public:
        /**
         * Serialize a single expression to binary format
         */
        static std::vector<uint8_t> serialize(const Expression *expr);

        /**
         * Serialize a list of expressions to binary format
         */
        static std::vector<uint8_t> serializeList(const std::vector<Expression *> &expressions);

        /**
         * Deserialize binary data to a single expression
         * @param data Binary data
         * @param len Length of data
         * @return Deserialized expression (automatic memory management)
         */
        static std::unique_ptr<Expression> deserialize(const uint8_t *data, size_t len);

        /**
         * Deserialize binary data to a list of expressions
         */
        static std::vector<std::unique_ptr<Expression>> deserializeList(const uint8_t *data, size_t len);

    private:
        // Serialization helpers
        static void serializeNode(const Expression *expr, std::vector<uint8_t> &buffer);
        static void writeU8(std::vector<uint8_t> &buffer, uint8_t value);
        static void writeU16(std::vector<uint8_t> &buffer, uint16_t value);
        static void writeU32(std::vector<uint8_t> &buffer, uint32_t value);
        static void writeU64(std::vector<uint8_t> &buffer, uint64_t value);
        static void writeString(std::vector<uint8_t> &buffer, const std::string &str);
        // Deserialization helpers
        static std::unique_ptr<Expression> deserializeNode(const uint8_t *&ptr, const uint8_t *end);
        static uint8_t readU8(const uint8_t *&ptr, const uint8_t *end);
        static uint16_t readU16(const uint8_t *&ptr, const uint8_t *end);
        static uint32_t readU32(const uint8_t *&ptr, const uint8_t *end);
        static uint64_t readU64(const uint8_t *&ptr, const uint8_t *end);
        static int64_t readI64(const uint8_t *&ptr, const uint8_t *end);
        static double readF64(const uint8_t *&ptr, const uint8_t *end);
        static std::string readString(const uint8_t *&ptr, const uint8_t *end);
        static void writeI64(std::vector<uint8_t> &buffer, int64_t value);
        static void writeF64(std::vector<uint8_t> &buffer, double value);

        // Type-specific serialization
        static void serializeLiteral(const LiteralExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeIdentifier(const IdentifierExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeBinaryOp(const BinaryOpExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeFunctionCall(const FunctionCallExpr *expr,
                                          std::vector<uint8_t> &buffer);
        static void serializeCast(const CastExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeCase(const CaseExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeAggregate(const AggregateExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeCoalesce(const CoalesceExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeNullIf(const NullIfExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeExtract(const ExtractExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeAlterElement(const AlterElementExpr *expr, std::vector<uint8_t> &buffer);

        // Type-specific deserialization
        static std::unique_ptr<Expression> deserializeLiteral(const uint8_t *&ptr, const uint8_t *end);
        static std::unique_ptr<Expression> deserializeIdentifier(const uint8_t *&ptr, const uint8_t *end);
        static std::unique_ptr<Expression> deserializeBinaryOp(const uint8_t *&ptr, const uint8_t *end);
        static std::unique_ptr<Expression> deserializeFunctionCall(const uint8_t *&ptr, const uint8_t *end);
        static std::unique_ptr<Expression> deserializeCast(const uint8_t *&ptr, const uint8_t *end);
        static std::unique_ptr<Expression> deserializeCase(const uint8_t *&ptr, const uint8_t *end);
        static std::unique_ptr<Expression> deserializeAggregate(const uint8_t *&ptr, const uint8_t *end);
        static std::unique_ptr<Expression> deserializeCoalesce(const uint8_t *&ptr, const uint8_t *end);
        static std::unique_ptr<Expression> deserializeNullIf(const uint8_t *&ptr, const uint8_t *end);
        static std::unique_ptr<Expression> deserializeExtract(const uint8_t *&ptr, const uint8_t *end);
        static std::unique_ptr<Expression> deserializeAlterElement(const uint8_t *&ptr, const uint8_t *end);

        // Node type enum for serialization
        enum class SerializedNodeType : uint8_t
        {
            LITERAL = 1,
            IDENTIFIER = 2,
            BINARY_OP = 3,
            FUNCTION_CALL = 4,
            CAST = 5,
            CASE = 6,
            AGGREGATE = 7,
            COALESCE = 8,
            NULLIF = 9,
            EXTRACT = 10,
            ALTER_ELEMENT = 11,
            // Note: Subqueries not supported in expression indexes (PostgreSQL limitation)
        };

        static constexpr uint8_t FORMAT_VERSION = 2;
    };

} // namespace scratchbird::core
