/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/sblr/expression_evaluator.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/charset.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/firebird_datetime.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/timezone.h"
#include "scratchbird/core/type_extractor.h"
#include "scratchbird/sblr/extract_element_catalog.h"
#include "scratchbird/sblr/extract_element_ops.h"  // Spec: docs/specifications/EXTRACT_AND_ALTER_ELEMENT.md
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <regex>

namespace scratchbird::sblr
{
    using json = nlohmann::json;

    // Task 17 MGA Phase 1.4: Updated constructor to include transaction context
    ExpressionEvaluator::ExpressionEvaluator(const std::vector<core::CatalogManager::ColumnInfo> &columns,
                                             core::Database *db,
                                             uint64_t xid)
        : db_(db), xid_(xid)
    {
        // Build column position map for fast lookups
        for (size_t i = 0; i < columns.size(); i++)
        {
            column_positions_[normalizeIdentifier(columns[i].column_name)] = i;
        }
    }

    TypedValue ExpressionEvaluator::evaluate(const Expression *expr,
                                              const std::vector<TypedValue> &row)
    {
        if (!expr)
        {
            return TypedValue::makeNull();
        }

        switch (expr->kind())
        {
        case ExprKind::LITERAL:
            return evaluateLiteral(static_cast<const LiteralExpr *>(expr), row);

        case ExprKind::IDENTIFIER:
            return evaluateIdentifier(static_cast<const IdentifierExpr *>(expr), row);

        case ExprKind::BINARY_OP:
            return evaluateBinaryOp(static_cast<const BinaryOpExpr *>(expr), row);

        case ExprKind::FUNCTION_CALL:
            return evaluateFunctionCall(static_cast<const FunctionCallExpr *>(expr), row);

        case ExprKind::CAST:
            return evaluateCast(static_cast<const CastExpr *>(expr), row);

        case ExprKind::CASE:
            return evaluateCase(static_cast<const CaseExpr *>(expr), row);

        case ExprKind::AGGREGATE:
            return evaluateAggregate(static_cast<const AggregateExpr *>(expr), row);

        case ExprKind::COALESCE:
            return evaluateCoalesce(static_cast<const CoalesceExpr *>(expr), row);

        case ExprKind::NULLIF:
            return evaluateNullIf(static_cast<const NullIfExpr *>(expr), row);

        case ExprKind::EXTRACT:
            return evaluateExtract(static_cast<const ExtractExpr *>(expr), row);

        case ExprKind::ALTER_ELEMENT:
            return evaluateAlterElement(static_cast<const AlterElementExpr *>(expr), row);

        default:
            throw std::runtime_error("Unsupported expression type for evaluation");
        }
    }

    bool ExpressionEvaluator::evaluatePredicate(const Expression *predicate,
                                                 const std::vector<TypedValue> &row)
    {
        TypedValue result = evaluate(predicate, row);
        return isTruthy(result);
    }

    // ========================================================================
    // Expression Type Handlers
    // ========================================================================

    TypedValue ExpressionEvaluator::evaluateLiteral(const LiteralExpr *expr,
                                                     const std::vector<TypedValue> &row)
    {
        switch (expr->literalType())
        {
        case core::LiteralExpr::LiteralType::INTEGER:
        {
            int64_t int_val = expr->intValue();
            return TypedValue::makeInt64(int_val);
        }

        case core::LiteralExpr::LiteralType::FLOAT:
        {
            double float_val = expr->floatValue();
            return TypedValue::makeFloat64(float_val);
        }

        case core::LiteralExpr::LiteralType::STRING:
            return TypedValue::makeVarchar(expr->stringValue());

        case core::LiteralExpr::LiteralType::NULL_LITERAL:
            return TypedValue::makeNull();

        default:
            throw std::runtime_error("Unsupported literal type");
        }
    }

    TypedValue ExpressionEvaluator::evaluateIdentifier(const IdentifierExpr *expr,
                                                        const std::vector<TypedValue> &row)
    {
        // Look up column in position map
        auto it = column_positions_.find(normalizeIdentifier(expr->name()));
        if (it == column_positions_.end())
        {
            throw std::runtime_error("Column not found: " + expr->name());
        }

        size_t pos = it->second;
        if (pos >= row.size())
        {
            throw std::runtime_error("Row index out of bounds");
        }

        return row[pos];
    }

    // ========================================================================
    // P0-4: Safe Arithmetic Operations (Overflow Detection)
    // ========================================================================

    namespace {
        // Safe arithmetic operations using compiler intrinsics
        inline bool safeAdd(int64_t a, int64_t b, int64_t* result) {
            return !__builtin_add_overflow(a, b, result);
        }

        inline bool safeSubtract(int64_t a, int64_t b, int64_t* result) {
            return !__builtin_sub_overflow(a, b, result);
        }

        inline bool safeMultiply(int64_t a, int64_t b, int64_t* result) {
            return !__builtin_mul_overflow(a, b, result);
        }

        inline bool safeDivide(int64_t a, int64_t b, int64_t* result) {
            if (b == 0) {
                return false;
            }
            // Check for INT64_MIN / -1 overflow
            if (a == INT64_MIN && b == -1) {
                return false;
            }
            *result = a / b;
            return true;
        }

        inline bool safeModulo(int64_t a, int64_t b, int64_t* result) {
            if (b == 0) {
                return false;
            }
            // Check for INT64_MIN % -1 (which is also problematic on some platforms)
            if (a == INT64_MIN && b == -1) {
                *result = 0;
                return true;
            }
            *result = a % b;
            return true;
        }

        template <typename T>
        inline bool safeAdd128(T a, T b, T* result) {
#if defined(__GNUC__) || defined(__clang__)
            return !__builtin_add_overflow(a, b, result);
#else
            *result = a + b;
            return true;
#endif
        }

        template <typename T>
        inline bool safeSubtract128(T a, T b, T* result) {
#if defined(__GNUC__) || defined(__clang__)
            return !__builtin_sub_overflow(a, b, result);
#else
            *result = a - b;
            return true;
#endif
        }

        template <typename T>
        inline bool safeMultiply128(T a, T b, T* result) {
#if defined(__GNUC__) || defined(__clang__)
            return !__builtin_mul_overflow(a, b, result);
#else
            *result = a * b;
            return true;
#endif
        }

        inline core::int128_t int128MinValue() {
            return static_cast<core::int128_t>(core::uint128_t{1} << 127);
        }

        inline bool safeDivide128(core::int128_t a, core::int128_t b, core::int128_t* result) {
            if (b == 0) {
                return false;
            }
            if (a == int128MinValue() && b == -1) {
                return false;
            }
            *result = a / b;
            return true;
        }

        inline bool safeModulo128(core::int128_t a, core::int128_t b, core::int128_t* result) {
            if (b == 0) {
                return false;
            }
            if (a == int128MinValue() && b == -1) {
                *result = 0;
                return true;
            }
            *result = a % b;
            return true;
        }

        inline bool safeDivide128(core::uint128_t a, core::uint128_t b, core::uint128_t* result) {
            if (b == 0) {
                return false;
            }
            *result = a / b;
            return true;
        }

        inline bool safeModulo128(core::uint128_t a, core::uint128_t b, core::uint128_t* result) {
            if (b == 0) {
                return false;
            }
            *result = a % b;
            return true;
        }

        inline bool isSignedIntegerType(core::DataType type) {
            switch (type) {
                case core::DataType::INT8:
                case core::DataType::INT16:
                case core::DataType::INT32:
                case core::DataType::INT64:
                case core::DataType::INT128:
                    return true;
                default:
                    return false;
            }
        }

        inline bool isUnsignedIntegerType(core::DataType type) {
            switch (type) {
                case core::DataType::UINT8:
                case core::DataType::UINT16:
                case core::DataType::UINT32:
                case core::DataType::UINT64:
                case core::DataType::UINT128:
                    return true;
                default:
                    return false;
            }
        }

        inline bool isIntegerType(core::DataType type) {
            return isSignedIntegerType(type) || isUnsignedIntegerType(type);
        }

        inline core::int128_t toSigned128(const TypedValue& value) {
            if (value.type() == core::DataType::INT128) {
                return value.getInt128();
            }
            return static_cast<core::int128_t>(value.getInt64());
        }

        inline bool tryGetUnsigned128(const TypedValue& value, core::uint128_t& out) {
            switch (value.type()) {
                case core::DataType::UINT8:
                    out = value.getUInt8();
                    return true;
                case core::DataType::UINT16:
                    out = value.getUInt16();
                    return true;
                case core::DataType::UINT32:
                    out = value.getUInt32();
                    return true;
                case core::DataType::UINT64:
                    out = value.getUInt64();
                    return true;
                case core::DataType::UINT128:
                    out = value.getUInt128();
                    return true;
                case core::DataType::INT128: {
                    core::int128_t signed_val = value.getInt128();
                    if (signed_val < 0) {
                        return false;
                    }
                    out = static_cast<core::uint128_t>(signed_val);
                    return true;
                }
                case core::DataType::INT8:
                case core::DataType::INT16:
                case core::DataType::INT32:
                case core::DataType::INT64: {
                    int64_t signed_val = value.getInt64();
                    if (signed_val < 0) {
                        return false;
                    }
                    out = static_cast<core::uint128_t>(signed_val);
                    return true;
                }
                default:
                    return false;
            }
        }

        inline std::vector<uint8_t> encodeInt128(core::int128_t value) {
            std::vector<uint8_t> bytes(16);
            core::uint128_t uvalue = static_cast<core::uint128_t>(value);
            for (size_t i = 0; i < 16; ++i) {
                bytes[i] = static_cast<uint8_t>(uvalue & 0xFF);
                uvalue >>= 8;
            }
            return bytes;
        }

        inline std::vector<uint8_t> encodeUInt128(core::uint128_t value) {
            std::vector<uint8_t> bytes(16);
            core::uint128_t uvalue = value;
            for (size_t i = 0; i < 16; ++i) {
                bytes[i] = static_cast<uint8_t>(uvalue & 0xFF);
                uvalue >>= 8;
            }
            return bytes;
        }

        inline TypedValue makeInt128Value(core::int128_t value) {
            return TypedValue::makeInt128(encodeInt128(value));
        }

        inline TypedValue makeUInt128Value(core::uint128_t value) {
            return TypedValue::makeUInt128(encodeUInt128(value));
        }

        inline int compareIntegerValues(const TypedValue& left, const TypedValue& right) {
            const bool left_unsigned = isUnsignedIntegerType(left.type());
            const bool right_unsigned = isUnsignedIntegerType(right.type());
            if (left_unsigned || right_unsigned) {
                core::int128_t left_signed = 0;
                core::int128_t right_signed = 0;
                bool left_negative = false;
                bool right_negative = false;

                if (!left_unsigned) {
                    left_signed = toSigned128(left);
                    left_negative = left_signed < 0;
                }
                if (!right_unsigned) {
                    right_signed = toSigned128(right);
                    right_negative = right_signed < 0;
                }

                if (left_negative && !right_negative) {
                    return -1;
                }
                if (right_negative && !left_negative) {
                    return 1;
                }
                if (left_negative && right_negative) {
                    if (left_signed < right_signed) {
                        return -1;
                    }
                    if (left_signed > right_signed) {
                        return 1;
                    }
                    return 0;
                }

                core::uint128_t lhs = 0;
                core::uint128_t rhs = 0;
                if (!tryGetUnsigned128(left, lhs) || !tryGetUnsigned128(right, rhs)) {
                    return 0;
                }
                if (lhs < rhs) {
                    return -1;
                }
                if (lhs > rhs) {
                    return 1;
                }
                return 0;
            }

            core::int128_t lhs = toSigned128(left);
            core::int128_t rhs = toSigned128(right);
            if (lhs < rhs) {
                return -1;
            }
            if (lhs > rhs) {
                return 1;
            }
            return 0;
        }

        inline double coerceToDouble(const TypedValue& val) {
            return val.toDouble();
        }

        inline int64_t floorDiv(int64_t value, int64_t divisor) {
            int64_t quotient = value / divisor;
            int64_t remainder = value % divisor;
            if (remainder != 0 && ((remainder > 0) != (divisor > 0))) {
                --quotient;
            }
            return quotient;
        }

        inline int64_t defaultDateTimeMicros() {
            core::Config &cfg = core::Config::getInstance();
            std::string default_time = cfg.getString("server.time", "date_default_time",
                                                     "00:00:00");
            int hour = 0;
            int minute = 0;
            int second = 0;
            int micros = 0;
            std::string time_part = default_time;
            std::string frac_part;
            size_t dot_pos = default_time.find('.');
            if (dot_pos != std::string::npos)
            {
                time_part = default_time.substr(0, dot_pos);
                frac_part = default_time.substr(dot_pos + 1);
            }
            int parsed = std::sscanf(time_part.c_str(), "%d:%d:%d", &hour, &minute, &second);
            if (parsed < 2)
            {
                return 0;
            }
            if (parsed == 2)
            {
                second = 0;
            }
            if (!frac_part.empty())
            {
                if (frac_part.size() > 6)
                {
                    return 0;
                }
                int frac_value = 0;
                for (char ch : frac_part)
                {
                    if (ch < '0' || ch > '9')
                    {
                        return 0;
                    }
                    frac_value = frac_value * 10 + (ch - '0');
                }
                int scale = 6 - static_cast<int>(frac_part.size());
                for (int i = 0; i < scale; ++i)
                {
                    frac_value *= 10;
                }
                micros = frac_value;
            }
            if (hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
                second < 0 || second > 59)
            {
                return 0;
            }
            return (static_cast<int64_t>(hour) * 3600 +
                    static_cast<int64_t>(minute) * 60 +
                    static_cast<int64_t>(second)) * 1000000 +
                   micros;
        }

        inline core::TimezoneManager& timezoneManager() {
            return core::getThreadLocalTimezoneManager();
        }

        static bool matchSqlLike(const std::string& str, const std::string& pattern,
                                 char escape = '\\') {
            size_t s = 0, p = 0;
            size_t star_p = std::string::npos, star_s = 0;

            while (s < str.size()) {
                if (p < pattern.size()) {
                    if (escape != '\0' && pattern[p] == escape && p + 1 < pattern.size()) {
                        p++;
                        if (str[s] == pattern[p]) {
                            s++;
                            p++;
                            continue;
                        }
                        if (star_p != std::string::npos) {
                            p = star_p + 1;
                            s = ++star_s;
                            continue;
                        }
                        return false;
                    }

                    if (pattern[p] == '%') {
                        star_p = p++;
                        star_s = s;
                        continue;
                    }

                    if (pattern[p] == '_' || pattern[p] == str[s]) {
                        s++;
                        p++;
                        continue;
                    }
                }

                if (star_p != std::string::npos) {
                    p = star_p + 1;
                    s = ++star_s;
                    continue;
                }

                return false;
            }

            while (p < pattern.size() && pattern[p] == '%') {
                p++;
            }

            return p == pattern.size();
        }

        static bool matchSqlLikeCase(const std::string& str, const std::string& pattern,
                                     char escape, bool case_insensitive) {
            if (!case_insensitive) {
                return matchSqlLike(str, pattern, escape);
            }

            std::string lower_str = str;
            std::string lower_pattern = pattern;
            for (char& c : lower_str) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            for (char& c : lower_pattern) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }

            char esc = escape;
            if (esc != '\0') {
                esc = static_cast<char>(std::tolower(static_cast<unsigned char>(esc)));
            }
            return matchSqlLike(lower_str, lower_pattern, esc);
        }

        static bool matchRegex(const std::string &text, const std::string &pattern,
                               bool case_insensitive) {
            try {
                std::regex::flag_type flags = std::regex::ECMAScript;
                if (case_insensitive) {
                    flags |= std::regex::icase;
                }
                std::regex re(pattern, flags);
                return std::regex_search(text, re);
            } catch (const std::regex_error &) {
                throw std::runtime_error("Invalid regular expression");
            }
        }

        enum class ThreeValuedTruth : uint8_t {
            FALSE_VALUE = 0,
            TRUE_VALUE = 1,
            UNKNOWN_VALUE = 2,
        };

        static ThreeValuedTruth toThreeValuedTruth(const TypedValue& value) {
            if (value.isNull()) {
                return ThreeValuedTruth::UNKNOWN_VALUE;
            }
            if (value.type() == core::DataType::BOOLEAN) {
                return value.getBoolean() ? ThreeValuedTruth::TRUE_VALUE
                                          : ThreeValuedTruth::FALSE_VALUE;
            }
            return ThreeValuedTruth::TRUE_VALUE;
        }

        static TypedValue evaluateThreeValuedAnd(const TypedValue& left,
                                                 const TypedValue& right) {
            const ThreeValuedTruth left_truth = toThreeValuedTruth(left);
            const ThreeValuedTruth right_truth = toThreeValuedTruth(right);
            if (left_truth == ThreeValuedTruth::FALSE_VALUE ||
                right_truth == ThreeValuedTruth::FALSE_VALUE) {
                return TypedValue::makeBoolean(false);
            }
            if (left_truth == ThreeValuedTruth::UNKNOWN_VALUE ||
                right_truth == ThreeValuedTruth::UNKNOWN_VALUE) {
                return TypedValue::makeNull(core::DataType::BOOLEAN);
            }
            return TypedValue::makeBoolean(true);
        }

        static TypedValue evaluateThreeValuedOr(const TypedValue& left,
                                                const TypedValue& right) {
            const ThreeValuedTruth left_truth = toThreeValuedTruth(left);
            const ThreeValuedTruth right_truth = toThreeValuedTruth(right);
            if (left_truth == ThreeValuedTruth::TRUE_VALUE ||
                right_truth == ThreeValuedTruth::TRUE_VALUE) {
                return TypedValue::makeBoolean(true);
            }
            if (left_truth == ThreeValuedTruth::UNKNOWN_VALUE ||
                right_truth == ThreeValuedTruth::UNKNOWN_VALUE) {
                return TypedValue::makeNull(core::DataType::BOOLEAN);
            }
            return TypedValue::makeBoolean(false);
        }
    }

    TypedValue ExpressionEvaluator::evaluateBinaryOp(const BinaryOpExpr *expr,
                                                      const std::vector<TypedValue> &row)
    {
        TypedValue left = evaluate(expr->left(), row);
        TypedValue right = evaluate(expr->right(), row);

        using core::BinaryOp;
        if (expr->op() == BinaryOp::AND)
        {
            return evaluateThreeValuedAnd(left, right);
        }
        if (expr->op() == BinaryOp::OR)
        {
            return evaluateThreeValuedOr(left, right);
        }

        // Handle NULL propagation for most operators
        if (left.isNull() || right.isNull())
        {
            return TypedValue::makeNull();
        }

        const bool use_integer128 = isIntegerType(left.type()) && isIntegerType(right.type()) &&
                                    (left.type() == core::DataType::INT128 ||
                                     right.type() == core::DataType::INT128 ||
                                     isUnsignedIntegerType(left.type()) ||
                                     isUnsignedIntegerType(right.type()));

        auto eval_integer128 = [&](BinaryOp op) -> TypedValue {
            const bool use_unsigned = isUnsignedIntegerType(left.type()) ||
                                      isUnsignedIntegerType(right.type());
            if (use_unsigned) {
                core::uint128_t lhs = 0;
                core::uint128_t rhs = 0;
                if (!tryGetUnsigned128(left, lhs) || !tryGetUnsigned128(right, rhs)) {
                    throw std::runtime_error("Negative value in unsigned arithmetic");
                }
                core::uint128_t result = 0;
                switch (op) {
                    case BinaryOp::ADD:
                        if (!safeAdd128(lhs, rhs, &result)) {
                            throw std::runtime_error("Integer overflow in addition");
                        }
                        break;
                    case BinaryOp::SUBTRACT:
                        if (!safeSubtract128(lhs, rhs, &result)) {
                            throw std::runtime_error("Integer overflow in subtraction");
                        }
                        break;
                    case BinaryOp::MULTIPLY:
                        if (!safeMultiply128(lhs, rhs, &result)) {
                            throw std::runtime_error("Integer overflow in multiplication");
                        }
                        break;
                    case BinaryOp::DIVIDE:
                        if (!safeDivide128(lhs, rhs, &result)) {
                            throw std::runtime_error("Division by zero");
                        }
                        break;
                    case BinaryOp::MODULO:
                        if (!safeModulo128(lhs, rhs, &result)) {
                            throw std::runtime_error("Modulo by zero");
                        }
                        break;
                    default:
                        break;
                }
                return makeUInt128Value(result);
            }

            core::int128_t lhs = toSigned128(left);
            core::int128_t rhs = toSigned128(right);
            core::int128_t result = 0;
            switch (op) {
                case BinaryOp::ADD:
                    if (!safeAdd128(lhs, rhs, &result)) {
                        throw std::runtime_error("Integer overflow in addition");
                    }
                    break;
                case BinaryOp::SUBTRACT:
                    if (!safeSubtract128(lhs, rhs, &result)) {
                        throw std::runtime_error("Integer overflow in subtraction");
                    }
                    break;
                case BinaryOp::MULTIPLY:
                    if (!safeMultiply128(lhs, rhs, &result)) {
                        throw std::runtime_error("Integer overflow in multiplication");
                    }
                    break;
                case BinaryOp::DIVIDE:
                    if (!safeDivide128(lhs, rhs, &result)) {
                        throw std::runtime_error("Division by zero or integer overflow");
                    }
                    break;
                case BinaryOp::MODULO:
                    if (!safeModulo128(lhs, rhs, &result)) {
                        throw std::runtime_error("Modulo by zero");
                    }
                    break;
                default:
                    break;
            }
            return makeInt128Value(result);
        };

        switch (expr->op())
        {
        // Arithmetic
        case BinaryOp::ADD:
            if (left.type() == core::DataType::INT64 && right.type() == core::DataType::INT64)
            {
                int64_t result;
                if (!safeAdd(left.getInt64(), right.getInt64(), &result)) {
                    throw std::runtime_error("Integer overflow in addition");
                }
                return TypedValue::makeInt64(result);
            }
            else if (use_integer128)
                return eval_integer128(BinaryOp::ADD);
            else
                return TypedValue::makeFloat64(left.toDouble() + right.toDouble());

        case BinaryOp::SUBTRACT:
            if (left.type() == core::DataType::INT64 && right.type() == core::DataType::INT64)
            {
                int64_t result;
                if (!safeSubtract(left.getInt64(), right.getInt64(), &result)) {
                    throw std::runtime_error("Integer overflow in subtraction");
                }
                return TypedValue::makeInt64(result);
            }
            else if (use_integer128)
                return eval_integer128(BinaryOp::SUBTRACT);
            else
                return TypedValue::makeFloat64(left.toDouble() - right.toDouble());

        case BinaryOp::MULTIPLY:
            if (left.type() == core::DataType::INT64 && right.type() == core::DataType::INT64)
            {
                int64_t result;
                if (!safeMultiply(left.getInt64(), right.getInt64(), &result)) {
                    throw std::runtime_error("Integer overflow in multiplication");
                }
                return TypedValue::makeInt64(result);
            }
            else if (use_integer128)
                return eval_integer128(BinaryOp::MULTIPLY);
            else
                return TypedValue::makeFloat64(left.toDouble() * right.toDouble());

        case BinaryOp::DIVIDE:
            // For integer division
            if (left.type() == core::DataType::INT64 && right.type() == core::DataType::INT64)
            {
                int64_t result;
                if (!safeDivide(left.getInt64(), right.getInt64(), &result)) {
                    throw std::runtime_error("Division by zero or integer overflow");
                }
                return TypedValue::makeInt64(result);
            }
            if (use_integer128)
                return eval_integer128(BinaryOp::DIVIDE);
            // For floating point division
            if (right.toDouble() == 0.0)
                throw std::runtime_error("Division by zero");
            return TypedValue::makeFloat64(left.toDouble() / right.toDouble());

        case BinaryOp::MODULO:
            if (left.type() == core::DataType::INT64 && right.type() == core::DataType::INT64)
            {
                int64_t result;
                if (!safeModulo(left.getInt64(), right.getInt64(), &result)) {
                    throw std::runtime_error("Modulo by zero");
                }
                return TypedValue::makeInt64(result);
            }
            else if (use_integer128)
                return eval_integer128(BinaryOp::MODULO);
            else
            {
                if (right.toDouble() == 0.0)
                    throw std::runtime_error("Modulo by zero");
                return TypedValue::makeFloat64(std::fmod(left.toDouble(), right.toDouble()));
            }

        // Comparison
        case BinaryOp::EQ:
            return TypedValue::makeBoolean(compareValues(left, right) == 0);

        case BinaryOp::NE:
            return TypedValue::makeBoolean(compareValues(left, right) != 0);

        case BinaryOp::LT:
            return TypedValue::makeBoolean(compareValues(left, right) < 0);

        case BinaryOp::LE:
            return TypedValue::makeBoolean(compareValues(left, right) <= 0);

        case BinaryOp::GT:
            return TypedValue::makeBoolean(compareValues(left, right) > 0);

        case BinaryOp::GE:
            return TypedValue::makeBoolean(compareValues(left, right) >= 0);

        // Logical
        case BinaryOp::AND:
            return evaluateThreeValuedAnd(left, right);

        case BinaryOp::OR:
            return evaluateThreeValuedOr(left, right);

        // String
        case BinaryOp::LIKE:
            return TypedValue::makeBoolean(
                matchSqlLikeCase(left.toString(), right.toString(), '\\', false));

        case BinaryOp::ILIKE:
            return TypedValue::makeBoolean(
                matchSqlLikeCase(left.toString(), right.toString(), '\\', true));

        case BinaryOp::REGEX_MATCH:
            return TypedValue::makeBoolean(matchRegex(left.toString(), right.toString(), false));

        case BinaryOp::REGEX_MATCH_CI:
            return TypedValue::makeBoolean(matchRegex(left.toString(), right.toString(), true));

        case BinaryOp::REGEX_NOT_MATCH:
            return TypedValue::makeBoolean(!matchRegex(left.toString(), right.toString(), false));

        case BinaryOp::REGEX_NOT_MATCH_CI:
            return TypedValue::makeBoolean(!matchRegex(left.toString(), right.toString(), true));

        case BinaryOp::IN:
        case BinaryOp::NOT_IN:
        {
            bool negated = (expr->op() == BinaryOp::NOT_IN);
            bool found = false;

            if (right.type() == core::DataType::ARRAY)
            {
                const auto& array = right.getArray();
                for (const auto& elem : array)
                {
                    if (elem.isNull())
                    {
                        continue;
                    }
                    if (compareValues(left, elem) == 0)
                    {
                        found = true;
                        break;
                    }
                }
            }
            else
            {
                std::string right_str = right.toString();
                try
                {
                    json j_array = json::parse(right_str);
                    if (!j_array.is_array())
                    {
                        found = (left.toString() == right_str);
                    }
                    else
                    {
                        std::string test_str = left.toString();
                        for (const auto& elem : j_array)
                        {
                            if (elem.is_null())
                            {
                                continue;
                            }
                            std::string elem_str = elem.is_string()
                                                       ? elem.get<std::string>()
                                                       : elem.dump();
                            if (test_str == elem_str)
                            {
                                found = true;
                                break;
                            }
                            if (elem.is_number())
                            {
                                try
                                {
                                    double test_num = std::stod(test_str);
                                    double elem_num = elem.get<double>();
                                    if (test_num == elem_num)
                                    {
                                        found = true;
                                        break;
                                    }
                                }
                                catch (...)
                                {
                                }
                            }
                        }
                    }
                }
                catch (const json::exception&)
                {
                    found = (left.toString() == right.toString());
                }
            }

            return TypedValue::makeBoolean(negated ? !found : found);
        }

        default:
            throw std::runtime_error("Unsupported binary operator");
        }
    }

    TypedValue ExpressionEvaluator::evaluateFunctionCall(const FunctionCallExpr *expr,
                                                          const std::vector<TypedValue> &row)
    {
        std::string func_name(expr->name());
        std::transform(func_name.begin(), func_name.end(), func_name.begin(), ::toupper);

        const auto &args = expr->args();

        // Evaluate arguments
        std::vector<TypedValue> arg_values;
        for (const auto &arg : args)
        {
            arg_values.push_back(evaluate(arg.get(), row));
        }

        // String functions
        if (func_name == "LOWER")
        {
            if (arg_values.empty())
                throw std::runtime_error("LOWER requires 1 argument");
            std::string str = arg_values[0].toString();
            str = core::utf8::to_lower(str);
            return TypedValue::makeVarchar(str);
        }
        else if (func_name == "UPPER")
        {
            if (arg_values.empty())
                throw std::runtime_error("UPPER requires 1 argument");
            std::string str = arg_values[0].toString();
            str = core::utf8::to_upper(str);
            return TypedValue::makeVarchar(str);
        }
        else if (func_name == "TRIM")
        {
            if (arg_values.empty())
                throw std::runtime_error("TRIM requires 1 argument");
            std::string str = arg_values[0].toString();
            size_t start = 0;
            while (start < str.length() &&
                   std::isspace(static_cast<unsigned char>(str[start])))
            {
                start++;
            }
            size_t end = str.length();
            while (end > start &&
                   std::isspace(static_cast<unsigned char>(str[end - 1])))
            {
                end--;
            }
            return TypedValue::makeVarchar(str.substr(start, end - start));
        }
        else if (func_name == "LTRIM")
        {
            if (arg_values.empty())
                throw std::runtime_error("LTRIM requires 1 argument");
            std::string str = arg_values[0].toString();
            size_t start = 0;
            while (start < str.length() &&
                   std::isspace(static_cast<unsigned char>(str[start])))
            {
                start++;
            }
            return TypedValue::makeVarchar(str.substr(start));
        }
        else if (func_name == "RTRIM")
        {
            if (arg_values.empty())
                throw std::runtime_error("RTRIM requires 1 argument");
            std::string str = arg_values[0].toString();
            size_t end = str.length();
            while (end > 0 &&
                   std::isspace(static_cast<unsigned char>(str[end - 1])))
            {
                end--;
            }
            return TypedValue::makeVarchar(str.substr(0, end));
        }
        else if (func_name == "SUBSTRING" || func_name == "SUBSTR")
        {
            if (arg_values.size() != 3)
                throw std::runtime_error("SUBSTRING requires 3 arguments");
            std::string str = arg_values[0].toString();
            int32_t char_start = static_cast<int32_t>(arg_values[1].toInt64());
            int32_t char_length = static_cast<int32_t>(arg_values[2].toInt64());

            if (char_start < 1)
                char_start = 1;
            char_start--;

            const uint8_t *str_bytes = reinterpret_cast<const uint8_t *>(str.data());
            uint32_t total_chars = core::utf8::char_length(
                str_bytes, static_cast<uint32_t>(str.size()));

            if (char_start >= static_cast<int32_t>(total_chars) || char_length <= 0)
            {
                return TypedValue::makeVarchar("");
            }

            uint32_t byte_start = core::utf8::byte_length(str_bytes,
                                                          static_cast<uint32_t>(char_start));
            uint32_t remaining_chars = std::min(static_cast<uint32_t>(char_length),
                                                total_chars - static_cast<uint32_t>(char_start));
            uint32_t byte_length = core::utf8::byte_length(str_bytes + byte_start,
                                                           remaining_chars);
            return TypedValue::makeVarchar(str.substr(byte_start, byte_length));
        }
        else if (func_name == "LENGTH" || func_name == "LEN" || func_name == "CHAR_LENGTH")
        {
            if (arg_values.empty())
                throw std::runtime_error("LENGTH requires 1 argument");
            std::string str = arg_values[0].toString();
            uint32_t len = core::utf8::char_length(
                reinterpret_cast<const uint8_t *>(str.data()),
                static_cast<uint32_t>(str.size()));
            return TypedValue::makeInt32(static_cast<int32_t>(len));
        }
        else if (func_name == "OCTET_LENGTH")
        {
            if (arg_values.empty())
                throw std::runtime_error("OCTET_LENGTH requires 1 argument");
            std::string str = arg_values[0].toString();
            return TypedValue::makeInt32(static_cast<int32_t>(str.size()));
        }
        else if (func_name == "CONCAT")
        {
            if (arg_values.empty())
                throw std::runtime_error("CONCAT requires at least 1 argument");
            std::string result;
            for (const auto &arg : arg_values)
            {
                if (arg.isNull())
                {
                    return TypedValue::makeNull();
                }
                result += arg.toString();
            }
            return TypedValue::makeVarchar(result);
        }
        else if (func_name == "CONCAT_WS")
        {
            if (arg_values.size() < 2)
                throw std::runtime_error("CONCAT_WS requires at least 2 arguments");
            const auto &sep_val = arg_values[0];
            if (sep_val.isNull())
            {
                return TypedValue::makeNull();
            }
            std::string sep = sep_val.toString();
            std::string result;
            bool first = true;
            for (size_t i = 1; i < arg_values.size(); ++i)
            {
                if (arg_values[i].isNull())
                {
                    continue;
                }
                if (!first)
                {
                    result += sep;
                }
                result += arg_values[i].toString();
                first = false;
            }
            return TypedValue::makeVarchar(result);
        }
        else if (func_name == "ABS")
        {
            if (arg_values.empty())
                throw std::runtime_error("ABS requires 1 argument");
            if (arg_values[0].type() == core::DataType::INT64)
            {
                int64_t val = arg_values[0].getInt64();
                // Check for INT64_MIN overflow (abs(INT64_MIN) would overflow)
                if (val == INT64_MIN)
                    throw std::runtime_error("Integer overflow in ABS (INT64_MIN has no positive representation)");
                return TypedValue::makeInt64(std::abs(val));
            }
            if (arg_values[0].type() == core::DataType::INT32)
            {
                int32_t val = arg_values[0].getInt32();
                if (val == INT32_MIN)
                    throw std::runtime_error("Integer overflow in ABS (INT32_MIN has no positive representation)");
                return TypedValue::makeInt32(std::abs(val));
            }
            return TypedValue::makeFloat64(std::abs(arg_values[0].toDouble()));
        }
        else if (func_name == "ROUND")
        {
            if (arg_values.empty())
                throw std::runtime_error("ROUND requires at least 1 argument");
            double value = arg_values[0].toDouble();
            if (arg_values.size() > 1)
            {
                int64_t decimals = arg_values[1].toInt64();
                double multiplier = std::pow(10.0, static_cast<double>(decimals));
                return TypedValue::makeFloat64(std::round(value * multiplier) / multiplier);
            }
            else
            {
                return TypedValue::makeFloat64(std::round(value));
            }
        }
        else if (func_name == "SIGN")
        {
            if (arg_values.empty())
                throw std::runtime_error("SIGN requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            double val = coerceToDouble(arg_values[0]);
            int32_t sign = (val > 0.0) ? 1 : ((val < 0.0) ? -1 : 0);
            return TypedValue::makeInt32(sign);
        }
        else if (func_name == "CEIL" || func_name == "CEILING")
        {
            if (arg_values.empty())
                throw std::runtime_error("CEIL requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(std::ceil(coerceToDouble(arg_values[0])));
        }
        else if (func_name == "FLOOR")
        {
            if (arg_values.empty())
                throw std::runtime_error("FLOOR requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(std::floor(coerceToDouble(arg_values[0])));
        }
        else if (func_name == "TRUNC")
        {
            if (arg_values.empty())
                throw std::runtime_error("TRUNC requires at least 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            int64_t precision = 0;
            if (arg_values.size() > 1 && !arg_values[1].isNull())
            {
                precision = arg_values[1].toInt64();
            }
            double value = coerceToDouble(arg_values[0]);
            double multiplier = std::pow(10.0, static_cast<double>(precision));
            return TypedValue::makeFloat64(std::trunc(value * multiplier) / multiplier);
        }
        else if (func_name == "MOD")
        {
            if (arg_values.size() != 2)
                throw std::runtime_error("MOD requires 2 arguments");
            if (arg_values[0].isNull() || arg_values[1].isNull())
                return TypedValue::makeNull();
            double divisor = coerceToDouble(arg_values[1]);
            if (divisor == 0.0)
                throw std::runtime_error("Division by zero in MOD");
            double dividend = coerceToDouble(arg_values[0]);
            return TypedValue::makeFloat64(std::fmod(dividend, divisor));
        }
        else if (func_name == "POWER" || func_name == "POW")
        {
            if (arg_values.size() != 2)
                throw std::runtime_error("POWER requires 2 arguments");
            if (arg_values[0].isNull() || arg_values[1].isNull())
                return TypedValue::makeNull();
            double base = coerceToDouble(arg_values[0]);
            double exponent = coerceToDouble(arg_values[1]);
            double result = std::pow(base, exponent);
            if (std::isnan(result))
                throw std::runtime_error("POWER produced invalid result");
            return TypedValue::makeFloat64(result);
        }
        else if (func_name == "EXP")
        {
            if (arg_values.empty())
                throw std::runtime_error("EXP requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            double value = coerceToDouble(arg_values[0]);
            double result = std::exp(value);
            if (std::isnan(result))
                throw std::runtime_error("EXP produced invalid result");
            return TypedValue::makeFloat64(result);
        }
        else if (func_name == "LN")
        {
            if (arg_values.empty())
                throw std::runtime_error("LN requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            double value = coerceToDouble(arg_values[0]);
            if (value <= 0.0)
                throw std::runtime_error("LN argument must be positive");
            double result = std::log(value);
            if (std::isnan(result))
                throw std::runtime_error("LN produced invalid result");
            return TypedValue::makeFloat64(result);
        }
        else if (func_name == "LOG")
        {
            if (arg_values.empty() || arg_values.size() > 2)
                throw std::runtime_error("LOG requires 1 or 2 arguments");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            if (arg_values.size() == 1)
            {
                double value = coerceToDouble(arg_values[0]);
                if (value <= 0.0)
                    throw std::runtime_error("LOG argument must be positive");
                double result = std::log10(value);
                if (std::isnan(result))
                    throw std::runtime_error("LOG produced invalid result");
                return TypedValue::makeFloat64(result);
            }
            if (arg_values[1].isNull())
                return TypedValue::makeNull();
            double base = coerceToDouble(arg_values[0]);
            double value = coerceToDouble(arg_values[1]);
            if (base <= 0.0 || base == 1.0)
                throw std::runtime_error("LOG base must be positive and not equal to 1");
            if (value <= 0.0)
                throw std::runtime_error("LOG argument must be positive");
            double result = std::log(value) / std::log(base);
            if (std::isnan(result))
                throw std::runtime_error("LOG produced invalid result");
            return TypedValue::makeFloat64(result);
        }
        else if (func_name == "LOG10")
        {
            if (arg_values.empty())
                throw std::runtime_error("LOG10 requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            double value = coerceToDouble(arg_values[0]);
            if (value <= 0.0)
                throw std::runtime_error("LOG10 argument must be positive");
            double result = std::log10(value);
            if (std::isnan(result))
                throw std::runtime_error("LOG10 produced invalid result");
            return TypedValue::makeFloat64(result);
        }
        else if (func_name == "LOG2")
        {
            if (arg_values.empty())
                throw std::runtime_error("LOG2 requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            double value = coerceToDouble(arg_values[0]);
            if (value <= 0.0)
                throw std::runtime_error("LOG2 argument must be positive");
            double result = std::log2(value);
            if (std::isnan(result))
                throw std::runtime_error("LOG2 produced invalid result");
            return TypedValue::makeFloat64(result);
        }
        else if (func_name == "SQRT")
        {
            if (arg_values.empty())
                throw std::runtime_error("SQRT requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            double value = coerceToDouble(arg_values[0]);
            if (value < 0.0)
                throw std::runtime_error("SQRT argument must be non-negative");
            double result = std::sqrt(value);
            if (std::isnan(result))
                throw std::runtime_error("SQRT produced invalid result");
            return TypedValue::makeFloat64(result);
        }
        else if (func_name == "CBRT")
        {
            if (arg_values.empty())
                throw std::runtime_error("CBRT requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(std::cbrt(coerceToDouble(arg_values[0])));
        }
        else if (func_name == "SIN")
        {
            if (arg_values.empty())
                throw std::runtime_error("SIN requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(std::sin(coerceToDouble(arg_values[0])));
        }
        else if (func_name == "COS")
        {
            if (arg_values.empty())
                throw std::runtime_error("COS requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(std::cos(coerceToDouble(arg_values[0])));
        }
        else if (func_name == "TAN")
        {
            if (arg_values.empty())
                throw std::runtime_error("TAN requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(std::tan(coerceToDouble(arg_values[0])));
        }
        else if (func_name == "ASIN")
        {
            if (arg_values.empty())
                throw std::runtime_error("ASIN requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            double value = coerceToDouble(arg_values[0]);
            if (value < -1.0 || value > 1.0)
                throw std::runtime_error("ASIN argument must be in range [-1, 1]");
            return TypedValue::makeFloat64(std::asin(value));
        }
        else if (func_name == "ACOS")
        {
            if (arg_values.empty())
                throw std::runtime_error("ACOS requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            double value = coerceToDouble(arg_values[0]);
            if (value < -1.0 || value > 1.0)
                throw std::runtime_error("ACOS argument must be in range [-1, 1]");
            return TypedValue::makeFloat64(std::acos(value));
        }
        else if (func_name == "ATAN")
        {
            if (arg_values.empty())
                throw std::runtime_error("ATAN requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(std::atan(coerceToDouble(arg_values[0])));
        }
        else if (func_name == "ATAN2")
        {
            if (arg_values.size() != 2)
                throw std::runtime_error("ATAN2 requires 2 arguments");
            if (arg_values[0].isNull() || arg_values[1].isNull())
                return TypedValue::makeNull();
            double y = coerceToDouble(arg_values[0]);
            double x = coerceToDouble(arg_values[1]);
            return TypedValue::makeFloat64(std::atan2(y, x));
        }
        else if (func_name == "DEGREES")
        {
            if (arg_values.empty())
                throw std::runtime_error("DEGREES requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(
                coerceToDouble(arg_values[0]) * 180.0 / 3.141592653589793238462643383279502884);
        }
        else if (func_name == "RADIANS")
        {
            if (arg_values.empty())
                throw std::runtime_error("RADIANS requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(
                coerceToDouble(arg_values[0]) * 3.141592653589793238462643383279502884 / 180.0);
        }
        else if (func_name == "PI")
        {
            if (!arg_values.empty())
                throw std::runtime_error("PI requires 0 arguments");
            return TypedValue::makeFloat64(3.141592653589793238462643383279502884);
        }
        else if (func_name == "SINH")
        {
            if (arg_values.empty())
                throw std::runtime_error("SINH requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(std::sinh(coerceToDouble(arg_values[0])));
        }
        else if (func_name == "COSH")
        {
            if (arg_values.empty())
                throw std::runtime_error("COSH requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(std::cosh(coerceToDouble(arg_values[0])));
        }
        else if (func_name == "TANH")
        {
            if (arg_values.empty())
                throw std::runtime_error("TANH requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(std::tanh(coerceToDouble(arg_values[0])));
        }
        else if (func_name == "ASINH")
        {
            if (arg_values.empty())
                throw std::runtime_error("ASINH requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(std::asinh(coerceToDouble(arg_values[0])));
        }
        else if (func_name == "ACOSH")
        {
            if (arg_values.empty())
                throw std::runtime_error("ACOSH requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            double value = coerceToDouble(arg_values[0]);
            if (value < 1.0)
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(std::acosh(value));
        }
        else if (func_name == "ATANH")
        {
            if (arg_values.empty())
                throw std::runtime_error("ATANH requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            double value = coerceToDouble(arg_values[0]);
            if (std::abs(value) >= 1.0)
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(std::atanh(value));
        }
        else if (func_name == "COT")
        {
            if (arg_values.empty())
                throw std::runtime_error("COT requires 1 argument");
            if (arg_values[0].isNull())
                return TypedValue::makeNull();
            double tan_val = std::tan(coerceToDouble(arg_values[0]));
            if (tan_val == 0.0)
                return TypedValue::makeNull();
            return TypedValue::makeFloat64(1.0 / tan_val);
        }
        else if (func_name == "NOW")
        {
            if (!arg_values.empty())
                throw std::runtime_error("NOW requires 0 arguments");
            auto now = std::chrono::system_clock::now();
            auto gmt_micros =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    now.time_since_epoch()).count();
            uint16_t tz_id = db_ ? db_->getConnectionTimezone()
                                 : timezoneManager().getDefaultTimezone();
            core::TimezoneOffset offset = timezoneManager().getOffset(
                tz_id, static_cast<int64_t>(gmt_micros));
            int32_t offset_seconds = offset.offset_minutes * 60;
            return TypedValue::makeTimestamp(static_cast<int64_t>(gmt_micros), offset_seconds);
        }
        else if (func_name == "CURRENT_TIMESTAMP")
        {
            if (!arg_values.empty())
                throw std::runtime_error("CURRENT_TIMESTAMP requires 0 arguments");

            int64_t gmt_micros = 0;
            if (auto* conn_ctx = core::ConnectionContext::getCurrent())
            {
                gmt_micros = static_cast<int64_t>(conn_ctx->getTransactionStartTime().count());
            }
            if (gmt_micros == 0)
            {
                auto now = std::chrono::system_clock::now();
                gmt_micros =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        now.time_since_epoch()).count();
            }

            uint16_t tz_id = db_ ? db_->getConnectionTimezone()
                                 : timezoneManager().getDefaultTimezone();
            core::TimezoneOffset offset = timezoneManager().getOffset(
                tz_id, static_cast<int64_t>(gmt_micros));
            int32_t offset_seconds = offset.offset_minutes * 60;
            return TypedValue::makeTimestamp(static_cast<int64_t>(gmt_micros), offset_seconds);
        }
        else if (func_name == "CURRENT_DATE")
        {
            if (!arg_values.empty())
                throw std::runtime_error("CURRENT_DATE requires 0 arguments");
            auto now = std::chrono::system_clock::now();
            auto gmt_micros =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    now.time_since_epoch()).count();
            uint16_t tz_id = db_ ? db_->getConnectionTimezone()
                                 : timezoneManager().getDefaultTimezone();
            core::TimezoneOffset offset = timezoneManager().getOffset(
                tz_id, static_cast<int64_t>(gmt_micros));
            int32_t offset_seconds = offset.offset_minutes * 60;

            int64_t local_seconds = floorDiv(gmt_micros +
                                             static_cast<int64_t>(offset_seconds) * 1000000,
                                             1000000);
            int64_t default_micros = defaultDateTimeMicros();
            int64_t default_seconds = default_micros / 1000000;
            int64_t days = floorDiv(local_seconds - default_seconds,
                                     core::FirebirdDateTime::SECONDS_PER_DAY);
            return TypedValue::makeDate(days, offset_seconds);
        }
        else if (func_name == "CURRENT_TIME")
        {
            if (!arg_values.empty())
                throw std::runtime_error("CURRENT_TIME requires 0 arguments");
            auto now = std::chrono::system_clock::now();
            auto gmt_micros =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    now.time_since_epoch()).count();
            uint16_t tz_id = db_ ? db_->getConnectionTimezone()
                                 : timezoneManager().getDefaultTimezone();
            core::TimezoneOffset offset = timezoneManager().getOffset(
                tz_id, static_cast<int64_t>(gmt_micros));
            int32_t offset_seconds = offset.offset_minutes * 60;
            int64_t micros_per_day =
                static_cast<int64_t>(core::FirebirdDateTime::SECONDS_PER_DAY) * 1000000;
            int64_t utc_time_micros = gmt_micros % micros_per_day;
            if (utc_time_micros < 0)
            {
                utc_time_micros += micros_per_day;
            }
            return TypedValue::makeTime(utc_time_micros, offset_seconds);
        }
        else if (func_name == "DATE_ADD")
        {
            if (arg_values.size() != 2)
                throw std::runtime_error("DATE_ADD requires 2 arguments");
            if (arg_values[0].isNull() || arg_values[1].isNull())
                return TypedValue::makeNull();
            int64_t days = static_cast<int64_t>(coerceToDouble(arg_values[1]));
            const int64_t micros_per_day =
                static_cast<int64_t>(core::FirebirdDateTime::SECONDS_PER_DAY) * 1000000;

            const auto& date_val = arg_values[0];
            if (date_val.type() == core::DataType::DATE)
            {
                int64_t result_days = date_val.getDate() + days;
                return TypedValue::makeDate(result_days, date_val.getTimezoneOffsetSeconds());
            }
            if (date_val.type() == core::DataType::TIMESTAMP)
            {
                int64_t result_micros = date_val.getTimestamp() + days * micros_per_day;
                return TypedValue::makeTimestamp(result_micros, date_val.getTimezoneOffsetSeconds());
            }
            if (date_val.type() == core::DataType::TIME)
            {
                int64_t result_micros = date_val.getTime() + days * micros_per_day;
                result_micros %= micros_per_day;
                if (result_micros < 0)
                {
                    result_micros += micros_per_day;
                }
                return TypedValue::makeTime(result_micros, date_val.getTimezoneOffsetSeconds());
            }
            throw std::runtime_error("DATE_ADD expects DATE, TIME, or TIMESTAMP input");
        }
        else if (func_name == "DATE_SUB")
        {
            if (arg_values.size() != 2)
                throw std::runtime_error("DATE_SUB requires 2 arguments");
            if (arg_values[0].isNull() || arg_values[1].isNull())
                return TypedValue::makeNull();
            int64_t days = static_cast<int64_t>(coerceToDouble(arg_values[1]));
            const int64_t micros_per_day =
                static_cast<int64_t>(core::FirebirdDateTime::SECONDS_PER_DAY) * 1000000;

            const auto& date_val = arg_values[0];
            if (date_val.type() == core::DataType::DATE)
            {
                int64_t result_days = date_val.getDate() - days;
                return TypedValue::makeDate(result_days, date_val.getTimezoneOffsetSeconds());
            }
            if (date_val.type() == core::DataType::TIMESTAMP)
            {
                int64_t result_micros = date_val.getTimestamp() - days * micros_per_day;
                return TypedValue::makeTimestamp(result_micros, date_val.getTimezoneOffsetSeconds());
            }
            if (date_val.type() == core::DataType::TIME)
            {
                int64_t result_micros = date_val.getTime() - days * micros_per_day;
                result_micros %= micros_per_day;
                if (result_micros < 0)
                {
                    result_micros += micros_per_day;
                }
                return TypedValue::makeTime(result_micros, date_val.getTimezoneOffsetSeconds());
            }
            throw std::runtime_error("DATE_SUB expects DATE, TIME, or TIMESTAMP input");
        }
        else if (func_name == "DATE_DIFF" || func_name == "DATEDIFF")
        {
            if (arg_values.size() != 2)
                throw std::runtime_error("DATE_DIFF requires 2 arguments");
            if (arg_values[0].isNull() || arg_values[1].isNull())
                return TypedValue::makeNull();

            auto to_days = [&](const TypedValue& input) -> int64_t {
                if (input.type() == core::DataType::DATE)
                {
                    return input.getDate();
                }
                if (input.type() == core::DataType::TIMESTAMP)
                {
                    core::TypeInfo target(core::DataType::DATE);
                    TypedValue date_value;
                    core::ErrorContext ctx;
                    if (input.convertTo(target, date_value,
                                        core::CastFormat::DEFAULT, &ctx) != core::Status::OK)
                    {
                        throw std::runtime_error("DATE_DIFF failed to convert timestamp");
                    }
                    return date_value.getDate();
                }
                throw std::runtime_error("DATE_DIFF expects DATE or TIMESTAMP input");
            };

            int64_t days1 = to_days(arg_values[0]);
            int64_t days2 = to_days(arg_values[1]);
            return TypedValue::makeInt64(days1 - days2);
        }

        throw std::runtime_error("Unknown function: " + func_name);
    }

    TypedValue ExpressionEvaluator::evaluateCast(const CastExpr *expr,
                                                  const std::vector<TypedValue> &row)
    {
        TypedValue value = evaluate(expr->expr(), row);
        return castValue(value, expr->targetType(), expr->format());
    }

    TypedValue ExpressionEvaluator::evaluateCase(const CaseExpr *expr,
                                                  const std::vector<TypedValue> &row)
    {
        const auto &whens = expr->whenClauses();
        for (const auto &when_clause : whens)
        {
            TypedValue condition = evaluate(when_clause.condition.get(), row);
            if (isTruthy(condition))
            {
                return evaluate(when_clause.result.get(), row);
            }
        }

        // No WHEN matched, return ELSE
        if (expr->elseResult())
        {
            return evaluate(expr->elseResult(), row);
        }

        return TypedValue::makeNull();
    }

    TypedValue ExpressionEvaluator::evaluateAggregate(const AggregateExpr *expr,
                                                       const std::vector<TypedValue> &row)
    {
        // Aggregate functions cannot be evaluated on a single row
        // They require multiple rows and are not supported in expression indexes
        throw std::runtime_error("Aggregate functions not supported in expression indexes");
    }

    TypedValue ExpressionEvaluator::evaluateCoalesce(const CoalesceExpr *expr,
                                                      const std::vector<TypedValue> &row)
    {
        const auto &args = expr->args();
        for (const auto &arg : args)
        {
            TypedValue value = evaluate(arg.get(), row);
            if (!value.isNull())
            {
                return value;
            }
        }
        return TypedValue::makeNull();
    }

    TypedValue ExpressionEvaluator::evaluateNullIf(const NullIfExpr *expr,
                                                    const std::vector<TypedValue> &row)
    {
        TypedValue value1 = evaluate(expr->expr1(), row);
        TypedValue value2 = evaluate(expr->expr2(), row);

        if (compareValues(value1, value2) == 0)
        {
            return TypedValue::makeNull();
        }

        return value1;
    }

    TypedValue ExpressionEvaluator::evaluateExtract(const ExtractExpr *expr,
                                                     const std::vector<TypedValue> &row)
    {
        ExtractField field = static_cast<ExtractField>(expr->fieldId());
        if (!expr->fieldName().empty())
        {
            auto resolved = resolveExtractFieldName(expr->fieldName());
            if (!resolved.has_value())
            {
                throw std::runtime_error("Unknown EXTRACT element: " + expr->fieldName());
            }
            field = resolved.value();
        }

        std::vector<TypedValue> args;
        args.reserve(expr->args().size());
        for (const auto &arg : expr->args())
        {
            args.push_back(evaluate(arg.get(), row));
        }

        TypedValue source = evaluate(expr->source(), row);
        TypedValue out;
        std::string err;
        if (!extractElement(source, field, args, &out, &err))
        {
            throw std::runtime_error(err.empty() ? "EXTRACT failed" : err);
        }
        return out;
    }

    TypedValue ExpressionEvaluator::evaluateAlterElement(const AlterElementExpr *expr,
                                                         const std::vector<TypedValue> &row)
    {
        ExtractField field = static_cast<ExtractField>(expr->fieldId());
        if (!expr->fieldName().empty())
        {
            auto resolved = resolveExtractFieldName(expr->fieldName());
            if (!resolved.has_value())
            {
                throw std::runtime_error("Unknown ALTER_ELEMENT element: " + expr->fieldName());
            }
            field = resolved.value();
        }

        std::vector<TypedValue> args;
        args.reserve(expr->args().size());
        for (const auto &arg : expr->args())
        {
            args.push_back(evaluate(arg.get(), row));
        }

        TypedValue source = evaluate(expr->source(), row);
        TypedValue new_value = evaluate(expr->newValue(), row);
        TypedValue out;
        std::string err;
        if (!alterElement(source, field, args, new_value, &out, &err))
        {
            throw std::runtime_error(err.empty() ? "ALTER_ELEMENT failed" : err);
        }
        return out;
    }

    // ========================================================================
    // Helper Methods
    // ========================================================================

    TypedValue ExpressionEvaluator::castValue(const TypedValue &value,
                                              const TypeInfo& target_type,
                                              CastFormat format)
    {
        if (value.isNull())
        {
            return TypedValue::makeNull(target_type.type);
        }

        TypedValue result;
        ErrorContext ctx;
        Status status = value.convertTo(target_type, result, format, &ctx);
        if (status != Status::OK)
        {
            throw std::runtime_error(ctx.message.empty()
                                         ? "Type conversion failed"
                                         : ctx.message);
        }
        return result;
    }

    bool ExpressionEvaluator::isTruthy(const TypedValue &value)
    {
        if (value.isNull())
        {
            return false;
        }

        if (value.type() == core::DataType::BOOLEAN)
        {
            return value.getBoolean();
        }

        // Non-NULL, non-boolean values are truthy
        return true;
    }

    int ExpressionEvaluator::compareValues(const TypedValue &left, const TypedValue &right)
    {
        // NULL handling
        if (left.isNull() && right.isNull())
            return 0;
        if (left.isNull())
            return -1; // NULL is less than any value
        if (right.isNull())
            return 1;

        // Type-specific comparison
        if (isIntegerType(left.type()) && isIntegerType(right.type()))
        {
            return compareIntegerValues(left, right);
        }
        else if (left.type() == core::DataType::VARCHAR || left.type() == core::DataType::TEXT ||
            right.type() == core::DataType::VARCHAR || right.type() == core::DataType::TEXT)
        {
            std::string left_str = left.toString();
            std::string right_str = right.toString();
            if (left_str < right_str)
                return -1;
            if (left_str > right_str)
                return 1;
            return 0;
        }
        else if (left.type() == core::DataType::BOOLEAN || right.type() == core::DataType::BOOLEAN)
        {
            bool left_bool = left.toBoolean();
            bool right_bool = right.toBoolean();
            if (left_bool < right_bool)
                return -1;
            if (left_bool > right_bool)
                return 1;
            return 0;
        }
        else
        {
            // Numeric comparison
            double left_num = left.toDouble();
            double right_num = right.toDouble();
            if (left_num < right_num)
                return -1;
            if (left_num > right_num)
                return 1;
            return 0;
        }
    }

    std::string ExpressionEvaluator::normalizeIdentifier(std::string_view name)
    {
        std::string normalized(name);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::toupper);
        return normalized;
    }

    // ========================================================================
    // Task 17 MGA Phase 1.4: Transaction-Aware Evaluation
    // ========================================================================

    bool ExpressionEvaluator::evaluateForTuple(const Expression *expr,
                                               const core::TID &tid,
                                               TypedValue &result_out)
    {
        // WP-6 PARSE-4: Evaluate expression for a specific tuple (by TID)
        //
        // This method evaluates an expression using a tuple's data. The TID is used
        // to identify the row, but since we don't have direct table access here,
        // callers should use the overload that accepts column values directly.
        //
        // For now, this TID-based method is a placeholder that requires external
        // tuple fetching. The preferred approach is to fetch the tuple externally
        // and call evaluate() with the row values.

        (void)tid;  // TID parameter reserved for future direct tuple access

        // Check if we have database context
        if (!db_)
        {
            // Without database context, we can't fetch tuple data.
            // Return false to indicate evaluation was not performed.
            result_out = TypedValue::makeNull();
            return false;
        }

        // NOTE: Full tuple-fetch implementation would require:
        // 1. Table ID (to locate the correct table)
        // 2. Fetch tuple: db_->storage_engine()->getTuple(table_id, tid, &tuple, nullptr)
        // 3. Check visibility with xid_
        // 4. Deserialize tuple to TypedValue vector
        // 5. Call evaluate(expr, row_values)
        //
        // For now, callers should use the public evaluate() method directly
        // with pre-fetched row data, which is the common pattern used throughout
        // the codebase.

        // Without table context, cannot evaluate by TID
        result_out = TypedValue::makeNull();
        return false;
    }

    bool ExpressionEvaluator::evaluatePredicateForTuple(const Expression *predicate,
                                                        const core::TID &tid,
                                                        bool &result_out)
    {
        // WP-6 PARSE-5: Evaluate predicate for a specific tuple (by TID)
        //
        // Similar to evaluateForTuple, but for boolean predicates.
        // Returns true if evaluation succeeded, result_out contains the boolean result.

        (void)tid;  // TID parameter reserved for future direct tuple access

        // Check if we have database context
        if (!db_)
        {
            result_out = false;
            return false;
        }

        // NOTE: Full implementation would require table context (see evaluateForTuple).
        // For now, return false to indicate evaluation was not performed.
        // Callers should use evaluatePredicate() with pre-fetched row data.

        result_out = false;
        return false;
    }

    // WP-6 PARSE-4/5: Additional overloads that accept row data directly
    TypedValue ExpressionEvaluator::evaluateForRow(const Expression *expr,
                                                    const std::vector<std::string>& column_names,
                                                    const std::vector<TypedValue>& column_values)
    {
        // Set up column position map
        column_positions_.clear();
        for (size_t i = 0; i < column_names.size(); ++i)
        {
            column_positions_[normalizeIdentifier(column_names[i])] = i;
        }

        // Evaluate using existing method
        return evaluate(expr, column_values);
    }

    bool ExpressionEvaluator::evaluatePredicateForRow(const Expression *predicate,
                                                       const std::vector<std::string>& column_names,
                                                       const std::vector<TypedValue>& column_values)
    {
        // Set up column position map
        column_positions_.clear();
        for (size_t i = 0; i < column_names.size(); ++i)
        {
            column_positions_[normalizeIdentifier(column_names[i])] = i;
        }

        // Evaluate using existing method
        return evaluatePredicate(predicate, column_values);
    }

} // namespace scratchbird::sblr
