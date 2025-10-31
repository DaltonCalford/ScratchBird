#pragma once

#include "scratchbird/parser/ast.h"
#include "scratchbird/parser/token.h"
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
    using namespace scratchbird::parser;

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
         * @param pool String pool for allocating string IDs
         * @return Deserialized expression (caller owns memory)
         */
        static Expression *deserialize(const uint8_t *data, size_t len, StringPool &pool);

        /**
         * Deserialize binary data to a list of expressions
         */
        static std::vector<Expression *> deserializeList(const uint8_t *data, size_t len,
                                                         StringPool &pool);

    private:
        // Serialization helpers
        static void serializeNode(const Expression *expr, std::vector<uint8_t> &buffer);
        static void writeU8(std::vector<uint8_t> &buffer, uint8_t value);
        static void writeU32(std::vector<uint8_t> &buffer, uint32_t value);
        static void writeU64(std::vector<uint8_t> &buffer, uint64_t value);
        static void writeString(std::vector<uint8_t> &buffer, const std::string &str);
        static void writeStringId(std::vector<uint8_t> &buffer, StringPool::StringId id);

        // Deserialization helpers
        static Expression *deserializeNode(const uint8_t *&ptr, const uint8_t *end,
                                           StringPool &pool);
        static uint8_t readU8(const uint8_t *&ptr, const uint8_t *end);
        static uint32_t readU32(const uint8_t *&ptr, const uint8_t *end);
        static uint64_t readU64(const uint8_t *&ptr, const uint8_t *end);
        static std::string readString(const uint8_t *&ptr, const uint8_t *end);
        static StringPool::StringId readStringId(const uint8_t *&ptr, const uint8_t *end,
                                                 StringPool &pool);

        // Type-specific serialization
        static void serializeLiteral(const LiteralExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeIdentifier(const IdentifierExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeBinaryOp(const BinaryOpExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeFunctionCall(const FunctionCallExpr *expr,
                                          std::vector<uint8_t> &buffer);
        static void serializeCast(const CastExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeCase(const CaseExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeAggregate(const AggregateExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeWindowFunc(const WindowFuncExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeJSONFunc(const JSONFuncExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeCoalesce(const CoalesceExpr *expr, std::vector<uint8_t> &buffer);
        static void serializeNullIf(const NullIfExpr *expr, std::vector<uint8_t> &buffer);

        // Window spec serialization helpers
        static void serializeWindowSpec(const WindowSpec *spec, std::vector<uint8_t> &buffer);
        static void serializeFrameBoundary(const FrameBoundary &boundary,
                                           std::vector<uint8_t> &buffer);

        // Type-specific deserialization
        static Expression *deserializeLiteral(const uint8_t *&ptr, const uint8_t *end,
                                              StringPool &pool);
        static Expression *deserializeIdentifier(const uint8_t *&ptr, const uint8_t *end,
                                                 StringPool &pool);
        static Expression *deserializeBinaryOp(const uint8_t *&ptr, const uint8_t *end,
                                               StringPool &pool);
        static Expression *deserializeFunctionCall(const uint8_t *&ptr, const uint8_t *end,
                                                   StringPool &pool);
        static Expression *deserializeCast(const uint8_t *&ptr, const uint8_t *end,
                                           StringPool &pool);
        static Expression *deserializeCase(const uint8_t *&ptr, const uint8_t *end,
                                           StringPool &pool);
        static Expression *deserializeAggregate(const uint8_t *&ptr, const uint8_t *end,
                                                StringPool &pool);
        static Expression *deserializeWindowFunc(const uint8_t *&ptr, const uint8_t *end,
                                                 StringPool &pool);
        static Expression *deserializeJSONFunc(const uint8_t *&ptr, const uint8_t *end,
                                               StringPool &pool);
        static Expression *deserializeCoalesce(const uint8_t *&ptr, const uint8_t *end,
                                               StringPool &pool);
        static Expression *deserializeNullIf(const uint8_t *&ptr, const uint8_t *end,
                                             StringPool &pool);

        // Window spec deserialization helpers
        static WindowSpec *deserializeWindowSpec(const uint8_t *&ptr, const uint8_t *end,
                                                 StringPool &pool);
        static FrameBoundary deserializeFrameBoundary(const uint8_t *&ptr, const uint8_t *end,
                                                      StringPool &pool);

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
            WINDOW_FUNC = 8,
            COALESCE = 9,
            NULLIF = 10,
            JSON_FUNC = 11,
            // Note: Subqueries not supported in expression indexes (PostgreSQL limitation)
        };

        static constexpr uint8_t FORMAT_VERSION = 1;
    };

} // namespace scratchbird::core
