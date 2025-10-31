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

    void ExpressionSerializer::writeStringId(std::vector<uint8_t> &buffer,
                                             StringPool::StringId id)
    {
        writeU32(buffer, id);
    }

    void ExpressionSerializer::writeI64(std::vector<uint8_t> &buffer, int64_t value)
    {
        // Write as unsigned, bit pattern preserved
        writeU64(buffer, static_cast<uint64_t>(value));
    }

    void ExpressionSerializer::writeF64(std::vector<uint8_t> &buffer, double value)
    {
        // Write double as uint64_t bit pattern
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

    StringPool::StringId ExpressionSerializer::readStringId(const uint8_t *&ptr,
                                                            const uint8_t *end,
                                                            StringPool &pool)
    {
        uint32_t id = readU32(ptr, end);
        // Note: In deserialization, we need to map old string IDs to new ones
        // For now, we'll store the string itself and re-intern it
        return id;
    }

    int64_t ExpressionSerializer::readI64(const uint8_t *&ptr, const uint8_t *end)
    {
        // Read as unsigned, then reinterpret as signed
        uint64_t uvalue = readU64(ptr, end);
        return static_cast<int64_t>(uvalue);
    }

    double ExpressionSerializer::readF64(const uint8_t *&ptr, const uint8_t *end)
    {
        // Read uint64 bit pattern, then reinterpret as double
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
            writeU8(buffer, 0); // null marker
            return;
        }

        // Determine node type and serialize
        switch (expr->kind())
        {
        case ASTKind::LITERAL:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::LITERAL));
            serializeLiteral(static_cast<const LiteralExpr *>(expr), buffer);
            break;

        case ASTKind::IDENTIFIER:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::IDENTIFIER));
            serializeIdentifier(static_cast<const IdentifierExpr *>(expr), buffer);
            break;

        case ASTKind::BINARY_OP:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::BINARY_OP));
            serializeBinaryOp(static_cast<const BinaryOpExpr *>(expr), buffer);
            break;

        case ASTKind::FUNCTION_CALL:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::FUNCTION_CALL));
            serializeFunctionCall(static_cast<const FunctionCallExpr *>(expr), buffer);
            break;

        case ASTKind::CAST:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::CAST));
            serializeCast(static_cast<const CastExpr *>(expr), buffer);
            break;

        case ASTKind::CASE:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::CASE));
            serializeCase(static_cast<const CaseExpr *>(expr), buffer);
            break;

        case ASTKind::AGGREGATE_FUNC:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::AGGREGATE));
            serializeAggregate(static_cast<const AggregateExpr *>(expr), buffer);
            break;

        case ASTKind::WINDOW_FUNC:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::WINDOW_FUNC));
            serializeWindowFunc(static_cast<const WindowFuncExpr *>(expr), buffer);
            break;

        case ASTKind::JSON_FUNC:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::JSON_FUNC));
            serializeJSONFunc(static_cast<const JSONFuncExpr *>(expr), buffer);
            break;

        case ASTKind::COALESCE:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::COALESCE));
            serializeCoalesce(static_cast<const CoalesceExpr *>(expr), buffer);
            break;

        case ASTKind::NULLIF:
            writeU8(buffer, static_cast<uint8_t>(SerializedNodeType::NULLIF));
            serializeNullIf(static_cast<const NullIfExpr *>(expr), buffer);
            break;

        case ASTKind::SUBQUERY:
            // Subqueries are not supported in expression indexes
            // This is a PostgreSQL limitation
            throw std::runtime_error("Subquery expressions cannot be used in expression indexes");

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
        writeU8(buffer, 0); // flags (unused)

        // Write literal type
        writeU8(buffer, static_cast<uint8_t>(expr->literalType()));

        // Write literal value based on type
        switch (expr->literalType())
        {
        case LiteralExpr::LiteralType::INTEGER:
            writeI64(buffer, expr->intValue());
            break;
        case LiteralExpr::LiteralType::FLOAT:
            writeF64(buffer, expr->floatValue());
            break;
        case LiteralExpr::LiteralType::STRING:
            writeStringId(buffer, expr->stringValue());
            break;
        case LiteralExpr::LiteralType::NULL_LITERAL:
            // No value to write for NULL
            break;
        case LiteralExpr::LiteralType::RANGE:
            writeStringId(buffer, expr->rangeValue());
            break;
        }
    }

    void ExpressionSerializer::serializeIdentifier(const IdentifierExpr *expr,
                                                   std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0); // flags

        // Write identifier name ID
        writeStringId(buffer, expr->name());

        // Write qualifier (table/alias) if present
        bool has_qualifier = expr->isQualified();
        writeU8(buffer, has_qualifier ? 1 : 0);
        if (has_qualifier)
        {
            writeStringId(buffer, expr->qualifier());
        }
    }

    void ExpressionSerializer::serializeBinaryOp(const BinaryOpExpr *expr,
                                                 std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0); // flags

        // Write operator type
        writeU8(buffer, static_cast<uint8_t>(expr->op()));

        // Write left and right expressions
        serializeNode(expr->left(), buffer);
        serializeNode(expr->right(), buffer);
    }

    void ExpressionSerializer::serializeFunctionCall(const FunctionCallExpr *expr,
                                                     std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0); // flags

        // Write function name
        writeStringId(buffer, expr->name());

        // Write arguments
        const auto &args = expr->args();
        writeU8(buffer, static_cast<uint8_t>(args.size()));
        for (auto *arg : args)
        {
            serializeNode(arg, buffer);
        }
    }

    void ExpressionSerializer::serializeCast(const CastExpr *expr, std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0); // flags

        // Write target type - serialize TypeName struct
        const TypeName &target = expr->targetType();
        writeU8(buffer, static_cast<uint8_t>(target.type));
        writeU32(buffer, target.precision);
        writeU32(buffer, target.scale);
        writeU8(buffer, target.with_timezone ? 1 : 0);

        // Write expression being cast
        serializeNode(expr->expr(), buffer);
    }

    void ExpressionSerializer::serializeCase(const CaseExpr *expr, std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0); // flags

        // Write WHEN clauses
        const auto &whens = expr->whenClauses();
        writeU8(buffer, static_cast<uint8_t>(whens.size()));
        for (const auto &when : whens)
        {
            serializeNode(when.condition, buffer);
            serializeNode(when.result, buffer);
        }

        // Write ELSE clause
        serializeNode(expr->elseResult(), buffer);
    }

    void ExpressionSerializer::serializeAggregate(const AggregateExpr *expr,
                                                  std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0); // flags

        // Write aggregate function type
        writeU8(buffer, static_cast<uint8_t>(expr->func()));

        // Write DISTINCT flag
        writeU8(buffer, expr->distinct() ? 1 : 0);

        // Write argument expression (nullptr for COUNT(*))
        serializeNode(expr->arg(), buffer);
    }

    void ExpressionSerializer::serializeWindowFunc(const WindowFuncExpr *expr,
                                                   std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0); // flags

        // Write window function type
        writeU8(buffer, static_cast<uint8_t>(expr->func()));

        // Write arguments
        const auto &args = expr->args();
        writeU8(buffer, static_cast<uint8_t>(args.size()));
        for (auto *arg : args)
        {
            serializeNode(arg, buffer);
        }

        // Write window spec
        serializeWindowSpec(expr->windowSpec(), buffer);
    }

    void ExpressionSerializer::serializeWindowSpec(const WindowSpec *spec,
                                                   std::vector<uint8_t> &buffer)
    {
        if (!spec)
        {
            writeU8(buffer, 0); // null marker
            return;
        }

        writeU8(buffer, 1); // has spec

        // Write PARTITION BY expressions
        const auto &partition_by = spec->partitionBy();
        writeU8(buffer, static_cast<uint8_t>(partition_by.size()));
        for (auto *expr : partition_by)
        {
            serializeNode(expr, buffer);
        }

        // Write ORDER BY expressions
        const auto &order_by = spec->orderBy();
        const auto &order_asc = spec->orderAscending();
        const auto &order_nulls = spec->orderNullsFirst();
        writeU8(buffer, static_cast<uint8_t>(order_by.size()));
        for (size_t i = 0; i < order_by.size(); i++)
        {
            serializeNode(order_by[i], buffer);
            writeU8(buffer, order_asc[i] ? 1 : 0);
            writeU8(buffer, order_nulls[i] ? 1 : 0);
        }

        // Write frame clause
        writeU8(buffer, spec->hasFrame() ? 1 : 0);
        if (spec->hasFrame())
        {
            writeU8(buffer, static_cast<uint8_t>(spec->frameMode()));
            serializeFrameBoundary(spec->frameStart(), buffer);
            serializeFrameBoundary(spec->frameEnd(), buffer);
        }
    }

    void ExpressionSerializer::serializeFrameBoundary(const FrameBoundary &boundary,
                                                      std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, static_cast<uint8_t>(boundary.type));
        serializeNode(boundary.offset, buffer);
    }

    void ExpressionSerializer::serializeJSONFunc(const JSONFuncExpr *expr,
                                                 std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0); // flags

        // Write JSON function type
        writeU8(buffer, static_cast<uint8_t>(expr->func()));

        // Write arguments
        const auto &args = expr->args();
        writeU8(buffer, static_cast<uint8_t>(args.size()));
        for (auto *arg : args)
        {
            serializeNode(arg, buffer);
        }
    }

    void ExpressionSerializer::serializeCoalesce(const CoalesceExpr *expr,
                                                 std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0); // flags

        // Write arguments
        const auto &args = expr->args();
        writeU8(buffer, static_cast<uint8_t>(args.size()));
        for (auto *arg : args)
        {
            serializeNode(arg, buffer);
        }
    }

    void ExpressionSerializer::serializeNullIf(const NullIfExpr *expr,
                                               std::vector<uint8_t> &buffer)
    {
        writeU8(buffer, 0); // flags

        // Write two expressions
        serializeNode(expr->expr1(), buffer);
        serializeNode(expr->expr2(), buffer);
    }

    // ========================================================================
    // Main Deserialization
    // ========================================================================

    Expression *ExpressionSerializer::deserialize(const uint8_t *data, size_t len,
                                                  StringPool &pool)
    {
        const uint8_t *ptr = data;
        const uint8_t *end = data + len;

        uint8_t version = readU8(ptr, end);
        if (version != FORMAT_VERSION)
        {
            throw std::runtime_error("Unsupported expression format version");
        }

        return deserializeNode(ptr, end, pool);
    }

    std::vector<Expression *> ExpressionSerializer::deserializeList(const uint8_t *data, size_t len,
                                                                     StringPool &pool)
    {
        const uint8_t *ptr = data;
        const uint8_t *end = data + len;

        uint8_t version = readU8(ptr, end);
        if (version != FORMAT_VERSION)
        {
            throw std::runtime_error("Unsupported expression format version");
        }

        uint32_t count = readU32(ptr, end);
        std::vector<Expression *> expressions;
        expressions.reserve(count);

        for (uint32_t i = 0; i < count; i++)
        {
            expressions.push_back(deserializeNode(ptr, end, pool));
        }

        return expressions;
    }

    Expression *ExpressionSerializer::deserializeNode(const uint8_t *&ptr, const uint8_t *end,
                                                      StringPool &pool)
    {
        uint8_t type_byte = readU8(ptr, end);
        if (type_byte == 0)
        {
            return nullptr; // null marker
        }

        SerializedNodeType type = static_cast<SerializedNodeType>(type_byte);

        switch (type)
        {
        case SerializedNodeType::LITERAL:
            return deserializeLiteral(ptr, end, pool);

        case SerializedNodeType::IDENTIFIER:
            return deserializeIdentifier(ptr, end, pool);

        case SerializedNodeType::BINARY_OP:
            return deserializeBinaryOp(ptr, end, pool);

        case SerializedNodeType::FUNCTION_CALL:
            return deserializeFunctionCall(ptr, end, pool);

        case SerializedNodeType::CAST:
            return deserializeCast(ptr, end, pool);

        case SerializedNodeType::CASE:
            return deserializeCase(ptr, end, pool);

        case SerializedNodeType::AGGREGATE:
            return deserializeAggregate(ptr, end, pool);

        case SerializedNodeType::WINDOW_FUNC:
            return deserializeWindowFunc(ptr, end, pool);

        case SerializedNodeType::JSON_FUNC:
            return deserializeJSONFunc(ptr, end, pool);

        case SerializedNodeType::COALESCE:
            return deserializeCoalesce(ptr, end, pool);

        case SerializedNodeType::NULLIF:
            return deserializeNullIf(ptr, end, pool);

        default:
            throw std::runtime_error("Unknown serialized node type");
        }
    }

    // ========================================================================
    // Type-Specific Deserialization
    // ========================================================================

    Expression *ExpressionSerializer::deserializeLiteral(const uint8_t *&ptr, const uint8_t *end,
                                                         StringPool &pool)
    {
        readU8(ptr, end); // flags (unused)

        auto lit_type = static_cast<parser::LiteralExpr::LiteralType>(readU8(ptr, end));

        SourceSpan span; // Dummy span for deserialized expressions
        LiteralExpr *lit_expr = new LiteralExpr(span, lit_type);

        // Read value based on type
        switch (lit_type)
        {
        case parser::LiteralExpr::LiteralType::INTEGER:
            lit_expr->setIntValue(readI64(ptr, end));
            break;
        case parser::LiteralExpr::LiteralType::FLOAT:
            lit_expr->setFloatValue(readF64(ptr, end));
            break;
        case parser::LiteralExpr::LiteralType::STRING:
            lit_expr->setStringValue(readStringId(ptr, end, pool));
            break;
        case parser::LiteralExpr::LiteralType::NULL_LITERAL:
            // No value to read for NULL
            break;
        case parser::LiteralExpr::LiteralType::RANGE:
            lit_expr->setRangeValue(readStringId(ptr, end, pool));
            break;
        }

        return lit_expr;
    }

    Expression *ExpressionSerializer::deserializeIdentifier(const uint8_t *&ptr, const uint8_t *end,
                                                            StringPool &pool)
    {
        readU8(ptr, end); // flags

        StringPool::StringId name_id = readStringId(ptr, end, pool);
        bool has_table = (readU8(ptr, end) != 0);

        SourceSpan span;
        if (has_table)
        {
            StringPool::StringId table_id = readStringId(ptr, end, pool);
            return new IdentifierExpr(span, name_id, table_id);
        }
        else
        {
            return new IdentifierExpr(span, name_id);
        }
    }

    Expression *ExpressionSerializer::deserializeBinaryOp(const uint8_t *&ptr, const uint8_t *end,
                                                          StringPool &pool)
    {
        readU8(ptr, end); // flags

        auto op = static_cast<BinaryOp>(readU8(ptr, end));
        Expression *left = deserializeNode(ptr, end, pool);
        Expression *right = deserializeNode(ptr, end, pool);

        SourceSpan span;
        return new BinaryOpExpr(span, op, left, right);
    }

    Expression *ExpressionSerializer::deserializeFunctionCall(const uint8_t *&ptr,
                                                              const uint8_t *end, StringPool &pool)
    {
        readU8(ptr, end); // flags

        StringPool::StringId func_name = readStringId(ptr, end, pool);
        uint8_t arg_count = readU8(ptr, end);

        std::vector<Expression *> args;
        args.reserve(arg_count);
        for (uint8_t i = 0; i < arg_count; i++)
        {
            args.push_back(deserializeNode(ptr, end, pool));
        }

        SourceSpan span;
        return new FunctionCallExpr(span, func_name, args);
    }

    Expression *ExpressionSerializer::deserializeCast(const uint8_t *&ptr, const uint8_t *end,
                                                      StringPool &pool)
    {
        readU8(ptr, end); // flags

        auto target_type = static_cast<DataType>(readU32(ptr, end));
        Expression *expr = deserializeNode(ptr, end, pool);

        SourceSpan span;
        return new CastExpr(span, expr, target_type);
    }

    Expression *ExpressionSerializer::deserializeCase(const uint8_t *&ptr, const uint8_t *end,
                                                      StringPool &pool)
    {
        readU8(ptr, end); // flags

        uint8_t when_count = readU8(ptr, end);
        std::vector<CaseExpr::WhenClause> whens;
        whens.reserve(when_count);

        for (uint8_t i = 0; i < when_count; i++)
        {
            Expression *condition = deserializeNode(ptr, end, pool);
            Expression *result = deserializeNode(ptr, end, pool);
            whens.push_back({condition, result});
        }

        Expression *else_result = deserializeNode(ptr, end, pool);

        SourceSpan span;
        return new CaseExpr(span, whens, else_result);
    }

    Expression *ExpressionSerializer::deserializeAggregate(const uint8_t *&ptr,
                                                           const uint8_t *end, StringPool &pool)
    {
        readU8(ptr, end); // flags

        auto func = static_cast<AggregateFunc>(readU8(ptr, end));
        bool distinct = (readU8(ptr, end) != 0);
        Expression *arg = deserializeNode(ptr, end, pool);

        SourceSpan span;
        return new AggregateExpr(span, func, arg, distinct);
    }

    Expression *ExpressionSerializer::deserializeWindowFunc(const uint8_t *&ptr,
                                                            const uint8_t *end, StringPool &pool)
    {
        readU8(ptr, end); // flags

        auto func = static_cast<WindowFunc>(readU8(ptr, end));

        uint8_t arg_count = readU8(ptr, end);
        std::vector<Expression *> args;
        args.reserve(arg_count);
        for (uint8_t i = 0; i < arg_count; i++)
        {
            args.push_back(deserializeNode(ptr, end, pool));
        }

        WindowSpec *window_spec = deserializeWindowSpec(ptr, end, pool);

        SourceSpan span;
        return new WindowFuncExpr(span, func, args, window_spec);
    }

    WindowSpec *ExpressionSerializer::deserializeWindowSpec(const uint8_t *&ptr,
                                                            const uint8_t *end, StringPool &pool)
    {
        uint8_t has_spec = readU8(ptr, end);
        if (has_spec == 0)
        {
            return nullptr;
        }

        SourceSpan span;
        WindowSpec *spec = new WindowSpec(span);

        // Read PARTITION BY expressions
        uint8_t partition_count = readU8(ptr, end);
        for (uint8_t i = 0; i < partition_count; i++)
        {
            Expression *expr = deserializeNode(ptr, end, pool);
            spec->addPartitionBy(expr);
        }

        // Read ORDER BY expressions
        uint8_t order_count = readU8(ptr, end);
        for (uint8_t i = 0; i < order_count; i++)
        {
            Expression *expr = deserializeNode(ptr, end, pool);
            bool ascending = (readU8(ptr, end) != 0);
            bool nulls_first = (readU8(ptr, end) != 0);
            spec->addOrderBy(expr, ascending, nulls_first);
        }

        // Read frame clause
        bool has_frame = (readU8(ptr, end) != 0);
        if (has_frame)
        {
            auto frame_mode = static_cast<FrameMode>(readU8(ptr, end));
            FrameBoundary start = deserializeFrameBoundary(ptr, end, pool);
            FrameBoundary end_bound = deserializeFrameBoundary(ptr, end, pool);
            spec->setFrame(frame_mode, start, end_bound);
        }

        return spec;
    }

    FrameBoundary ExpressionSerializer::deserializeFrameBoundary(const uint8_t *&ptr,
                                                                 const uint8_t *end,
                                                                 StringPool &pool)
    {
        auto type = static_cast<FrameBoundaryType>(readU8(ptr, end));
        Expression *offset = deserializeNode(ptr, end, pool);
        return FrameBoundary(type, offset);
    }

    Expression *ExpressionSerializer::deserializeJSONFunc(const uint8_t *&ptr, const uint8_t *end,
                                                          StringPool &pool)
    {
        readU8(ptr, end); // flags

        auto func = static_cast<JSONFunc>(readU8(ptr, end));

        uint8_t arg_count = readU8(ptr, end);
        std::vector<Expression *> args;
        args.reserve(arg_count);
        for (uint8_t i = 0; i < arg_count; i++)
        {
            args.push_back(deserializeNode(ptr, end, pool));
        }

        SourceSpan span;
        return new JSONFuncExpr(span, func, args);
    }

    Expression *ExpressionSerializer::deserializeCoalesce(const uint8_t *&ptr, const uint8_t *end,
                                                          StringPool &pool)
    {
        readU8(ptr, end); // flags

        uint8_t arg_count = readU8(ptr, end);
        std::vector<Expression *> args;
        args.reserve(arg_count);
        for (uint8_t i = 0; i < arg_count; i++)
        {
            args.push_back(deserializeNode(ptr, end, pool));
        }

        SourceSpan span;
        return new CoalesceExpr(span, args);
    }

    Expression *ExpressionSerializer::deserializeNullIf(const uint8_t *&ptr, const uint8_t *end,
                                                        StringPool &pool)
    {
        readU8(ptr, end); // flags

        Expression *expr1 = deserializeNode(ptr, end, pool);
        Expression *expr2 = deserializeNode(ptr, end, pool);

        SourceSpan span;
        return new NullIfExpr(span, expr1, expr2);
    }

} // namespace scratchbird::core
