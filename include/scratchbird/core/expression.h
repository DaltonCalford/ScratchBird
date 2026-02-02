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
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace scratchbird::core
{
    enum class ExprKind : uint8_t
    {
        LITERAL,
        IDENTIFIER,
        BINARY_OP,
        FUNCTION_CALL,
        CAST,
        CASE,
        AGGREGATE,
        COALESCE,
        NULLIF,
        EXTRACT,
        ALTER_ELEMENT
    };

    class Expression
    {
    public:
        explicit Expression(ExprKind kind) : kind_(kind) {}
        virtual ~Expression() = default;

        ExprKind kind() const { return kind_; }

    private:
        ExprKind kind_;
    };

    class LiteralExpr : public Expression
    {
    public:
        enum class LiteralType : uint8_t
        {
            INTEGER,
            FLOAT,
            STRING,
            NULL_LITERAL,
            RANGE
        };

        explicit LiteralExpr(LiteralType type)
            : Expression(ExprKind::LITERAL), literal_type_(type)
        {
        }

        LiteralType literalType() const { return literal_type_; }

        int64_t intValue() const { return int_value_; }
        double floatValue() const { return float_value_; }
        const std::string &stringValue() const { return string_value_; }
        const std::string &rangeValue() const { return range_value_; }

        void setIntValue(int64_t value) { int_value_ = value; }
        void setFloatValue(double value) { float_value_ = value; }
        void setStringValue(std::string value) { string_value_ = std::move(value); }
        void setRangeValue(std::string value) { range_value_ = std::move(value); }

    private:
        LiteralType literal_type_;
        int64_t int_value_ = 0;
        double float_value_ = 0.0;
        std::string string_value_;
        std::string range_value_;
    };

    class IdentifierExpr : public Expression
    {
    public:
        explicit IdentifierExpr(std::string name)
            : Expression(ExprKind::IDENTIFIER), name_(std::move(name))
        {
        }

        IdentifierExpr(std::string qualifier, std::string name)
            : Expression(ExprKind::IDENTIFIER),
              name_(std::move(name)),
              qualifier_(std::move(qualifier))
        {
        }

        const std::string &name() const { return name_; }
        const std::string &qualifier() const { return qualifier_; }
        bool isQualified() const { return !qualifier_.empty(); }

    private:
        std::string name_;
        std::string qualifier_;
    };

    enum class BinaryOp : uint8_t
    {
        ADD,
        SUBTRACT,
        MULTIPLY,
        DIVIDE,
        MODULO,
        EQ,
        NE,
        LT,
        GT,
        LE,
        GE,
        AND,
        OR,
        LIKE,
        ILIKE,
        IN,
        NOT_IN,
        ARRAY_OVERLAP,
        ARRAY_CONTAINS,
        ARRAY_CONTAINED_BY,
        REGEX_MATCH,
        REGEX_MATCH_CI,
        REGEX_NOT_MATCH,
        REGEX_NOT_MATCH_CI,
        RANGE_STRICTLY_LEFT,
        RANGE_STRICTLY_RIGHT,
        RANGE_ADJACENT
    };

    class BinaryOpExpr : public Expression
    {
    public:
        BinaryOpExpr(BinaryOp op, std::unique_ptr<Expression> left,
                     std::unique_ptr<Expression> right)
            : Expression(ExprKind::BINARY_OP),
              op_(op),
              left_(std::move(left)),
              right_(std::move(right))
        {
        }

        BinaryOp op() const { return op_; }
        Expression *left() const { return left_.get(); }
        Expression *right() const { return right_.get(); }

    private:
        BinaryOp op_;
        std::unique_ptr<Expression> left_;
        std::unique_ptr<Expression> right_;
    };

    class FunctionCallExpr : public Expression
    {
    public:
        FunctionCallExpr(std::string name, std::vector<std::unique_ptr<Expression>> args)
            : Expression(ExprKind::FUNCTION_CALL),
              name_(std::move(name)),
              args_(std::move(args))
        {
        }

        const std::string &name() const { return name_; }
        const std::vector<std::unique_ptr<Expression>> &args() const { return args_; }

    private:
        std::string name_;
        std::vector<std::unique_ptr<Expression>> args_;
    };

    class CastExpr : public Expression
    {
    public:
        CastExpr(std::unique_ptr<Expression> expr, const TypeInfo &target_type,
                 bool is_try_cast = false,
                 CastFormat format = CastFormat::DEFAULT)
            : Expression(ExprKind::CAST),
              expr_(std::move(expr)),
              target_type_(target_type),
              is_try_cast_(is_try_cast),
              format_(format)
        {
        }

        Expression *expr() const { return expr_.get(); }
        const TypeInfo &targetType() const { return target_type_; }
        bool isTryCast() const { return is_try_cast_; }
        CastFormat format() const { return format_; }

    private:
        std::unique_ptr<Expression> expr_;
        TypeInfo target_type_;
        bool is_try_cast_ = false;
        CastFormat format_ = CastFormat::DEFAULT;
    };

    class CaseExpr : public Expression
    {
    public:
        struct WhenClause
        {
            std::unique_ptr<Expression> condition;
            std::unique_ptr<Expression> result;
        };

        CaseExpr(std::vector<WhenClause> when_clauses,
                 std::unique_ptr<Expression> else_result = nullptr)
            : Expression(ExprKind::CASE),
              when_clauses_(std::move(when_clauses)),
              else_result_(std::move(else_result))
        {
        }

        const std::vector<WhenClause> &whenClauses() const { return when_clauses_; }
        Expression *elseResult() const { return else_result_.get(); }

    private:
        std::vector<WhenClause> when_clauses_;
        std::unique_ptr<Expression> else_result_;
    };

    enum class AggregateFunc : uint8_t
    {
        COUNT,
        SUM,
        AVG,
        MIN,
        MAX,
        ARRAY_AGG,
        STRING_AGG
    };

    class AggregateExpr : public Expression
    {
    public:
        AggregateExpr(AggregateFunc func, std::unique_ptr<Expression> arg,
                      bool distinct = false)
            : Expression(ExprKind::AGGREGATE),
              func_(func),
              arg_(std::move(arg)),
              distinct_(distinct)
        {
        }

        AggregateFunc func() const { return func_; }
        Expression *arg() const { return arg_.get(); }
        bool distinct() const { return distinct_; }

    private:
        AggregateFunc func_;
        std::unique_ptr<Expression> arg_;
        bool distinct_ = false;
    };

    class CoalesceExpr : public Expression
    {
    public:
        explicit CoalesceExpr(std::vector<std::unique_ptr<Expression>> args)
            : Expression(ExprKind::COALESCE), args_(std::move(args))
        {
        }

        const std::vector<std::unique_ptr<Expression>> &args() const { return args_; }

    private:
        std::vector<std::unique_ptr<Expression>> args_;
    };

    class NullIfExpr : public Expression
    {
    public:
        NullIfExpr(std::unique_ptr<Expression> expr1, std::unique_ptr<Expression> expr2)
            : Expression(ExprKind::NULLIF),
              expr1_(std::move(expr1)),
              expr2_(std::move(expr2))
        {
        }

        Expression *expr1() const { return expr1_.get(); }
        Expression *expr2() const { return expr2_.get(); }

    private:
        std::unique_ptr<Expression> expr1_;
        std::unique_ptr<Expression> expr2_;
    };

    class ExtractExpr : public Expression
    {
    public:
        ExtractExpr(uint8_t field_id, std::string field_name,
                    std::unique_ptr<Expression> source)
            : Expression(ExprKind::EXTRACT),
              field_id_(field_id),
              field_name_(std::move(field_name)),
              source_(std::move(source))
        {
        }

        ExtractExpr(uint8_t field_id, std::string field_name,
                    std::vector<std::unique_ptr<Expression>> args,
                    std::unique_ptr<Expression> source)
            : Expression(ExprKind::EXTRACT),
              field_id_(field_id),
              field_name_(std::move(field_name)),
              args_(std::move(args)),
              source_(std::move(source))
        {
        }

        uint8_t fieldId() const { return field_id_; }
        const std::string &fieldName() const { return field_name_; }
        const std::vector<std::unique_ptr<Expression>> &args() const { return args_; }
        Expression *source() const { return source_.get(); }

    private:
        uint8_t field_id_ = 0;
        std::string field_name_;
        std::vector<std::unique_ptr<Expression>> args_;
        std::unique_ptr<Expression> source_;
    };

    class AlterElementExpr : public Expression
    {
    public:
        AlterElementExpr(uint8_t field_id, std::string field_name,
                         std::vector<std::unique_ptr<Expression>> args,
                         std::unique_ptr<Expression> source,
                         std::unique_ptr<Expression> new_value)
            : Expression(ExprKind::ALTER_ELEMENT),
              field_id_(field_id),
              field_name_(std::move(field_name)),
              args_(std::move(args)),
              source_(std::move(source)),
              new_value_(std::move(new_value))
        {
        }

        uint8_t fieldId() const { return field_id_; }
        const std::string &fieldName() const { return field_name_; }
        const std::vector<std::unique_ptr<Expression>> &args() const { return args_; }
        Expression *source() const { return source_.get(); }
        Expression *newValue() const { return new_value_.get(); }

    private:
        uint8_t field_id_ = 0;
        std::string field_name_;
        std::vector<std::unique_ptr<Expression>> args_;
        std::unique_ptr<Expression> source_;
        std::unique_ptr<Expression> new_value_;
    };
} // namespace scratchbird::core
