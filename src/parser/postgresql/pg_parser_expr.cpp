/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * PostgreSQL Parser - Expression Parsing
 *
 * Expression parsing with proper operator precedence for PostgreSQL.
 */

#include "scratchbird/parser/postgresql/pg_parser.h"
#include "scratchbird/core/types.h"
#include "scratchbird/sblr/extract_element_catalog.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace scratchbird::parser::postgresql {

parser::v3::Expression* Parser::makeBinary(parser::v3::BinaryOp op,
                                       parser::v3::Expression* left,
                                       parser::v3::Expression* right) {
    auto* expr = arena()->create<parser::v3::BinaryExpr>();
    expr->op = op;
    expr->left = left;
    expr->right = right;
    return expr;
}

parser::v3::Expression* Parser::makeUnary(parser::v3::UnaryOp op,
                                      parser::v3::Expression* operand) {
    auto* expr = arena()->create<parser::v3::UnaryExpr>();
    expr->op = op;
    expr->operand = operand;
    return expr;
}

parser::v3::Expression* Parser::makeLiteralInt(int64_t value) {
    auto* expr = arena()->create<parser::v3::LiteralExpr>();
    expr->literal_type = parser::v3::LiteralType::INTEGER;
    expr->int_value = value;
    return expr;
}

parser::v3::Expression* Parser::makeLiteralFloat(double value) {
    auto* expr = arena()->create<parser::v3::LiteralExpr>();
    expr->literal_type = parser::v3::LiteralType::FLOAT;
    expr->float_value = value;
    return expr;
}

parser::v3::Expression* Parser::makeLiteralString(const std::string& value) {
    auto* expr = arena()->create<parser::v3::LiteralExpr>();
    expr->literal_type = parser::v3::LiteralType::STRING;
    expr->string_value = string_pool_.intern(value);
    return expr;
}

parser::v3::Expression* Parser::makeLiteralBool(bool value) {
    auto* expr = arena()->create<parser::v3::LiteralExpr>();
    expr->literal_type = parser::v3::LiteralType::BOOLEAN;
    expr->bool_value = value;
    return expr;
}

parser::v3::Expression* Parser::makeLiteralNull() {
    auto* expr = arena()->create<parser::v3::LiteralExpr>();
    expr->literal_type = parser::v3::LiteralType::NULL_VALUE;
    return expr;
}

parser::v3::Expression* Parser::makeColumnRef(const std::vector<std::string>& parts) {
    auto* expr = arena()->create<parser::v3::ColumnRefExpr>();
    if (parts.empty()) {
        return expr;
    }
    auto col_id = string_pool_.intern(parts.back());
    if (parts.size() == 1) {
        expr->column = parser::v3::ColumnRef(col_id);
        return expr;
    }
    std::vector<parser::v3::StringPool::StringId> comps;
    comps.reserve(parts.size() - 1);
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        comps.push_back(string_pool_.intern(parts[i]));
    }
    parser::v3::SchemaPath path(parser::v3::PathType::UNQUALIFIED, std::move(comps));
    expr->column = parser::v3::ColumnRef(std::move(path), col_id);
    return expr;
}

std::vector<uint8_t> Parser::captureExpressionBytecode() {
    bool prev = emit_enabled_;
    emit_enabled_ = true;
    size_t start = bytecode_.size();
    parseExpression();
    std::vector<uint8_t> out;
    if (bytecode_.size() > start) {
        out.assign(bytecode_.begin() + static_cast<long>(start), bytecode_.end());
        bytecode_.resize(start);
    }
    emit_enabled_ = prev;
    return out;
}


std::string Parser::parseExpressionText() {
    // Consume an expression for DDL clauses that currently store text payloads.
    bool prev_emit = emit_enabled_;
    emit_enabled_ = false;
    (void)parseExpression();
    emit_enabled_ = prev_emit;
    return "";
}


// ============================================================================
// Expression Parsing (Operator Precedence)
// ============================================================================

parser::v3::Expression* Parser::parseExpression() {
    return parseOrExpr();
}

parser::v3::Expression* Parser::parseOrExpr() {
    auto* left = parseAndExpr();
    while (matchKeyword(TokenType::KW_OR)) {
        auto* right = parseAndExpr();
        left = makeBinary(parser::v3::BinaryOp::OR, left, right);
    }
    return left;
}

parser::v3::Expression* Parser::parseAndExpr() {
    auto* left = parseNotExpr();
    while (matchKeyword(TokenType::KW_AND)) {
        auto* right = parseNotExpr();
        left = makeBinary(parser::v3::BinaryOp::AND, left, right);
    }
    return left;
}

parser::v3::Expression* Parser::parseNotExpr() {
    if (matchKeyword(TokenType::KW_NOT)) {
        return makeUnary(parser::v3::UnaryOp::NOT, parseNotExpr());
    }
    return parseComparisonExpr();
}

parser::v3::Expression* Parser::parseComparisonExpr() {
    auto* left = parseIsExpr();
    if (match(TokenType::EQUAL)) return makeBinary(parser::v3::BinaryOp::EQ, left, parseIsExpr());
    if (match(TokenType::NOT_EQUAL) || match(TokenType::LESS_GREATER))
        return makeBinary(parser::v3::BinaryOp::NE, left, parseIsExpr());
    if (match(TokenType::LESS_THAN)) return makeBinary(parser::v3::BinaryOp::LT, left, parseIsExpr());
    if (match(TokenType::GREATER_THAN)) return makeBinary(parser::v3::BinaryOp::GT, left, parseIsExpr());
    if (match(TokenType::LESS_EQUAL)) return makeBinary(parser::v3::BinaryOp::LE, left, parseIsExpr());
    if (match(TokenType::GREATER_EQUAL)) return makeBinary(parser::v3::BinaryOp::GE, left, parseIsExpr());
    return left;
}

parser::v3::Expression* Parser::parseIsExpr() {
    auto* left = parseInExpr();
    if (matchKeyword(TokenType::KW_IS)) {
        bool is_not = matchKeyword(TokenType::KW_NOT);
        if (matchKeyword(TokenType::KW_NULL)) {
            auto* expr = arena()->create<parser::v3::IsNullExpr>();
            expr->expr = left;
            expr->negated = is_not;
            return expr;
        }
        if (matchKeyword(TokenType::KW_TRUE)) {
            auto* eq = makeBinary(parser::v3::BinaryOp::EQ, left, makeLiteralBool(true));
            return is_not ? makeUnary(parser::v3::UnaryOp::NOT, eq) : eq;
        }
        if (matchKeyword(TokenType::KW_FALSE)) {
            auto* eq = makeBinary(parser::v3::BinaryOp::EQ, left, makeLiteralBool(false));
            return is_not ? makeUnary(parser::v3::UnaryOp::NOT, eq) : eq;
        }
        if (matchKeyword(TokenType::KW_UNKNOWN)) {
            auto* expr = arena()->create<parser::v3::IsNullExpr>();
            expr->expr = left;
            expr->negated = is_not;
            return expr;
        }
        if (matchKeyword(TokenType::KW_DISTINCT)) {
            consumeKeyword(TokenType::KW_FROM, "Expected FROM after DISTINCT");
            auto* right = parseInExpr();
            auto* fn = arena()->create<parser::v3::FunctionCallExpr>();
            fn->function_path = parser::v3::SchemaPath(parser::v3::PathType::UNQUALIFIED,
                                                       {string_pool_.intern("is_distinct_from")});
            fn->arguments.push_back(left);
            fn->arguments.push_back(right);
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_NULL_SAFE_EQ));
            return is_not ? makeUnary(parser::v3::UnaryOp::NOT, fn) : fn;
        }
        error("Expected NULL, TRUE, FALSE, DISTINCT, or UNKNOWN after IS");
    }
    return left;
}

parser::v3::Expression* Parser::parseInExpr() {
    auto* expr = parseBetweenExpr();
    bool is_not = false;
    if (matchKeyword(TokenType::KW_NOT)) is_not = true;
    if (matchKeyword(TokenType::KW_IN)) {
        consume(TokenType::LEFT_PAREN, "Expected ( after IN");
        auto* in_expr = arena()->create<parser::v3::InExpr>();
        in_expr->expr = expr;
        in_expr->negated = is_not;
        if (check(TokenType::KW_SELECT)) {
            auto* sub = parseSubquery();
            in_expr->subquery = sub;
            in_expr->has_subquery = true;
        } else {
            do { in_expr->values.push_back(parseExpression()); } while (match(TokenType::COMMA));
        }
        consume(TokenType::RIGHT_PAREN, "Expected ) after IN list");
        return in_expr;
    }
    if (is_not) error("Expected IN after NOT");
    return expr;
}

parser::v3::Expression* Parser::parseBetweenExpr() {
    auto* expr = parseLikeExpr();
    bool is_not = false;
    if (matchKeyword(TokenType::KW_NOT)) is_not = true;
    if (matchKeyword(TokenType::KW_BETWEEN)) {
        auto* between = arena()->create<parser::v3::BetweenExpr>();
        between->expr = expr;
        between->negated = is_not;
        between->low = parseLikeExpr();
        consumeKeyword(TokenType::KW_AND, "Expected AND in BETWEEN");
        between->high = parseLikeExpr();
        return between;
    }
    if (is_not) error("Expected BETWEEN after NOT");
    return expr;
}

parser::v3::Expression* Parser::parseLikeExpr() {
    auto* expr = parseBitwiseOrExpr();
    bool is_not = false;
    if (matchKeyword(TokenType::KW_NOT)) is_not = true;
    if (matchKeyword(TokenType::KW_LIKE)) {
        auto* like = arena()->create<parser::v3::LikeExpr>();
        like->expr = expr;
        like->negated = is_not;
        like->match_kind = parser::v3::LikeMatchKind::LIKE;
        like->pattern = parseBitwiseOrExpr();
        if (matchKeyword(TokenType::KW_ESCAPE)) {
            like->escape = parseBitwiseOrExpr();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_LIKE_ESCAPE));
        }
        return like;
    }
    if (matchKeyword(TokenType::KW_ILIKE)) {
        auto* like = arena()->create<parser::v3::LikeExpr>();
        like->expr = expr;
        like->negated = is_not;
        like->case_insensitive = true;
        like->match_kind = parser::v3::LikeMatchKind::ILIKE;
        like->pattern = parseBitwiseOrExpr();
        if (matchKeyword(TokenType::KW_ESCAPE)) {
            like->escape = parseBitwiseOrExpr();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_LIKE_ESCAPE));
        }
        return like;
    }
    if (matchKeyword(TokenType::KW_SIMILAR)) {
        consumeKeyword(TokenType::KW_TO, "Expected TO after SIMILAR");
        auto* like = arena()->create<parser::v3::LikeExpr>();
        like->expr = expr;
        like->negated = is_not;
        like->match_kind = parser::v3::LikeMatchKind::SIMILAR;
        like->pattern = parseBitwiseOrExpr();
        if (matchKeyword(TokenType::KW_ESCAPE)) {
            like->escape = parseBitwiseOrExpr();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_LIKE_ESCAPE));
        }
        return like;
    }
    if (is_not) error("Expected LIKE, ILIKE, or SIMILAR after NOT");
    return expr;
}


static parser::v3::TypeName pgTypeToTypeName(const PgDataType& type, parser::v3::StringPool& pool) {
    parser::v3::TypeName out;
    auto intern = [&](std::string_view s) { return pool.intern(s); };
    switch (type.kind) {
        case PgDataType::Kind::SMALLINT: out.name = intern("smallint"); break;
        case PgDataType::Kind::INTEGER: out.name = intern("integer"); break;
        case PgDataType::Kind::BIGINT: out.name = intern("bigint"); break;
        case PgDataType::Kind::INT128: out.name = intern("int128"); break;
        case PgDataType::Kind::UINT128: out.name = intern("uint128"); break;
        case PgDataType::Kind::REAL: out.name = intern("real"); break;
        case PgDataType::Kind::DOUBLE_PRECISION: out.name = intern("double precision"); break;
        case PgDataType::Kind::DECIMAL: out.name = intern("decimal"); break;
        case PgDataType::Kind::NUMERIC: out.name = intern("numeric"); break;
        case PgDataType::Kind::MONEY: out.name = intern("money"); break;
        case PgDataType::Kind::SMALLSERIAL: out.name = intern("smallserial"); break;
        case PgDataType::Kind::SERIAL: out.name = intern("serial"); break;
        case PgDataType::Kind::BIGSERIAL: out.name = intern("bigserial"); break;
        case PgDataType::Kind::CHAR: out.name = intern("char"); break;
        case PgDataType::Kind::VARCHAR: out.name = intern("varchar"); break;
        case PgDataType::Kind::TEXT: out.name = intern("text"); break;
        case PgDataType::Kind::BYTEA: out.name = intern("bytea"); break;
        case PgDataType::Kind::DATE: out.name = intern("date"); break;
        case PgDataType::Kind::TIME: out.name = intern("time"); break;
        case PgDataType::Kind::TIMETZ: out.name = intern("timetz"); break;
        case PgDataType::Kind::TIMESTAMP: out.name = intern("timestamp"); break;
        case PgDataType::Kind::TIMESTAMPTZ: out.name = intern("timestamptz"); break;
        case PgDataType::Kind::INTERVAL: out.name = intern("interval"); break;
        case PgDataType::Kind::BOOLEAN: out.name = intern("boolean"); break;
        case PgDataType::Kind::UUID: out.name = intern("uuid"); break;
        case PgDataType::Kind::JSON: out.name = intern("json"); break;
        case PgDataType::Kind::JSONB: out.name = intern("jsonb"); break;
        case PgDataType::Kind::JSONPATH: out.name = intern("jsonpath"); break;
        case PgDataType::Kind::BIT: out.name = intern("bit"); break;
        case PgDataType::Kind::VARBIT: out.name = intern("varbit"); break;
        case PgDataType::Kind::TSVECTOR: out.name = intern("tsvector"); break;
        case PgDataType::Kind::TSQUERY: out.name = intern("tsquery"); break;
        case PgDataType::Kind::INT4RANGE: out.name = intern("int4range"); break;
        case PgDataType::Kind::INT8RANGE: out.name = intern("int8range"); break;
        case PgDataType::Kind::NUMRANGE: out.name = intern("numrange"); break;
        case PgDataType::Kind::DATERANGE: out.name = intern("daterange"); break;
        case PgDataType::Kind::TSRANGE: out.name = intern("tsrange"); break;
        case PgDataType::Kind::TSTZRANGE: out.name = intern("tstzrange"); break;
        case PgDataType::Kind::XML: out.name = intern("xml"); break;
        case PgDataType::Kind::ENUM:
        case PgDataType::Kind::DOMAIN:
        case PgDataType::Kind::COMPOSITE:
            out.name = intern(type.type_name);
            break;
        case PgDataType::Kind::ARRAY: {
            out.is_array = true;
            out.array_size = (type.array_size > 0) ? std::optional<int32_t>(type.array_size) : std::nullopt;
            if (!type.element_type.empty()) {
                out.name = intern(type.element_type);
            } else {
                PgDataType elem(type.element_kind);
                out.name = pgTypeToTypeName(elem, pool).name;
            }
            break;
        }
        default:
            out.name = intern("unknown");
            break;
    }
    if (type.length > 0) out.length = type.length;
    if (type.precision > 0) out.precision = type.precision;
    if (type.scale > 0) out.scale = type.scale;
    out.with_time_zone = type.with_time_zone;
    return out;
}

parser::v3::Expression* Parser::parseBitwiseOrExpr() {
    auto* left = parseBitwiseXorExpr();
    while (match(TokenType::PIPE)) {
        auto* right = parseBitwiseXorExpr();
        left = makeBinary(parser::v3::BinaryOp::BIT_OR, left, right);
    }
    return left;
}

parser::v3::Expression* Parser::parseBitwiseXorExpr() {
    auto* left = parseBitwiseAndExpr();
    while (match(TokenType::CARET)) {
        auto* right = parseBitwiseAndExpr();
        left = makeBinary(parser::v3::BinaryOp::BIT_XOR, left, right);
    }
    return left;
}

parser::v3::Expression* Parser::parseBitwiseAndExpr() {
    auto* left = parseShiftExpr();
    while (match(TokenType::AMPERSAND)) {
        auto* right = parseShiftExpr();
        left = makeBinary(parser::v3::BinaryOp::BIT_AND, left, right);
    }
    return left;
}

parser::v3::Expression* Parser::parseShiftExpr() {
    auto* left = parseAdditiveExpr();
    while (true) {
        if (match(TokenType::LEFT_SHIFT)) {
            left = makeBinary(parser::v3::BinaryOp::SHIFT_LEFT, left, parseAdditiveExpr());
        } else if (match(TokenType::RIGHT_SHIFT)) {
            left = makeBinary(parser::v3::BinaryOp::SHIFT_RIGHT, left, parseAdditiveExpr());
        } else {
            break;
        }
    }
    return left;
}

parser::v3::Expression* Parser::parseAdditiveExpr() {
    auto* left = parseMultiplicativeExpr();
    while (true) {
        if (match(TokenType::PLUS)) {
            left = makeBinary(parser::v3::BinaryOp::ADD, left, parseMultiplicativeExpr());
        } else if (match(TokenType::MINUS)) {
            left = makeBinary(parser::v3::BinaryOp::SUB, left, parseMultiplicativeExpr());
        } else if (match(TokenType::CONCAT)) {
            left = makeBinary(parser::v3::BinaryOp::CONCAT, left, parseMultiplicativeExpr());
        } else {
            break;
        }
    }
    return left;
}

parser::v3::Expression* Parser::parseMultiplicativeExpr() {
    auto* left = parseUnaryExpr();
    while (true) {
        if (match(TokenType::STAR)) {
            left = makeBinary(parser::v3::BinaryOp::MUL, left, parseUnaryExpr());
        } else if (match(TokenType::SLASH)) {
            left = makeBinary(parser::v3::BinaryOp::DIV, left, parseUnaryExpr());
        } else if (match(TokenType::PERCENT)) {
            left = makeBinary(parser::v3::BinaryOp::MOD, left, parseUnaryExpr());
        } else {
            break;
        }
    }
    return left;
}

parser::v3::Expression* Parser::parseUnaryExpr() {
    if (match(TokenType::MINUS)) return makeUnary(parser::v3::UnaryOp::NEGATE, parseUnaryExpr());
    if (match(TokenType::PLUS)) return parseUnaryExpr();
    if (match(TokenType::TILDE)) return makeUnary(parser::v3::UnaryOp::BIT_NOT, parseUnaryExpr());
    return parsePostfixExpr();
}

parser::v3::Expression* Parser::parsePostfixExpr() {
    auto* expr = parsePrimaryExpr();
    return parsePostfixTail(expr);
}

parser::v3::Expression* Parser::parsePostfixTail(parser::v3::Expression* base) {
    auto* expr = base;
    while (true) {
        if (match(TokenType::DOUBLE_COLON)) {
            expr = parseTypeCast(expr);
        } else if (match(TokenType::LEFT_BRACKET)) {
            auto* idx = parseExpression();
            consume(TokenType::RIGHT_BRACKET, "Expected ]");
            auto* fn = arena()->create<parser::v3::FunctionCallExpr>();
            fn->function_path = parser::v3::SchemaPath(parser::v3::PathType::UNQUALIFIED,
                                                       {string_pool_.intern("array_subscript")});
            fn->arguments.push_back(expr);
            fn->arguments.push_back(idx);
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ARRAY_SUBSCRIPT));
            expr = fn;
        } else if (match(TokenType::ARROW)) {
            expr = makeBinary(parser::v3::BinaryOp::JSON_EXTRACT, expr, parseExpression());
        } else if (match(TokenType::DOUBLE_ARROW)) {
            expr = makeBinary(parser::v3::BinaryOp::JSON_EXTRACT_TEXT, expr, parseExpression());
        } else if (match(TokenType::HASH_ARROW)) {
            expr = makeBinary(parser::v3::BinaryOp::JSON_HASH_EXTRACT, expr, parseExpression());
        } else if (match(TokenType::HASH_DOUBLE_ARROW)) {
            expr = makeBinary(parser::v3::BinaryOp::JSON_HASH_EXTRACT_TEXT, expr, parseExpression());
        } else if (match(TokenType::AT_GREATER)) {
            expr = makeBinary(parser::v3::BinaryOp::ARRAY_CONTAINS, expr, parsePrimaryExpr());
        } else if (match(TokenType::LESS_AT)) {
            expr = makeBinary(parser::v3::BinaryOp::ARRAY_CONTAINED_BY, expr, parsePrimaryExpr());
        } else if (match(TokenType::QUESTION)) {
            expr = makeBinary(parser::v3::BinaryOp::JSON_EXISTS, expr, parsePrimaryExpr());
        } else if (match(TokenType::QUESTION_PIPE)) {
            expr = makeBinary(parser::v3::BinaryOp::JSON_EXISTS_ANY, expr, parsePrimaryExpr());
        } else if (match(TokenType::QUESTION_AMPERSAND)) {
            expr = makeBinary(parser::v3::BinaryOp::JSON_EXISTS_ALL, expr, parsePrimaryExpr());
        } else if (match(TokenType::AT_AT)) {
            auto* fn = arena()->create<parser::v3::FunctionCallExpr>();
            fn->function_path = parser::v3::SchemaPath(parser::v3::PathType::UNQUALIFIED,
                                                       {string_pool_.intern("ts_match")});
            fn->arguments.push_back(expr);
            fn->arguments.push_back(parsePrimaryExpr());
            expr = fn;
        } else {
            break;
        }
    }
    return expr;
}

parser::v3::Expression* Parser::parsePrimaryExpr() {
    if (check(TokenType::INTEGER_LITERAL)) {
        auto* lit = makeLiteralInt(current_token_.value.int_value);
        advance();
        return lit;
    }
    if (check(TokenType::FLOAT_LITERAL)) {
        auto* lit = makeLiteralFloat(current_token_.value.float_value);
        advance();
        return lit;
    }
    if (check(TokenType::STRING_LITERAL) || check(TokenType::DOLLAR_STRING) ||
        check(TokenType::ESCAPE_STRING)) {
        uint32_t id = current_token_.value.string_id;
        std::string_view str = lexer_.stringPool().get(id);
        auto* lit = makeLiteralString(std::string(str));
        advance();
        return lit;
    }
    if (matchKeyword(TokenType::KW_NULL)) return makeLiteralNull();
    if (matchKeyword(TokenType::KW_TRUE)) return makeLiteralBool(true);
    if (matchKeyword(TokenType::KW_FALSE)) return makeLiteralBool(false);
    if (matchKeyword(TokenType::KW_DEFAULT)) {
        auto* lit = arena()->create<parser::v3::LiteralExpr>();
        lit->literal_type = parser::v3::LiteralType::DEFAULT;
        return lit;
    }

    if (check(TokenType::KW_CASE)) return parseCaseExpr();
    if (check(TokenType::KW_CAST)) return parseCastExpr();
    if (matchKeyword(TokenType::KW_EXTRACT)) return parseExtractExpr();
    if (matchKeyword(TokenType::KW_ALTER_ELEMENT)) return parseAlterElementExpr();
    if (check(TokenType::KW_ARRAY)) return parseArrayConstructor();

    if (check(TokenType::LEFT_PAREN)) {
        advance();
        if (check(TokenType::KW_SELECT)) {
            auto* sub = parseSubquery();
            consume(TokenType::RIGHT_PAREN, "Expected ) after subquery");
            auto* subexpr = arena()->create<parser::v3::SubqueryExpr>();
            subexpr->subquery = sub;
            return subexpr;
        }
        auto* expr = parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return expr;
    }

    if (matchKeyword(TokenType::KW_EXISTS)) {
        consume(TokenType::LEFT_PAREN, "Expected ( after EXISTS");
        auto* sub = parseSubquery();
        consume(TokenType::RIGHT_PAREN, "Expected ) after EXISTS subquery");
        auto* exists = arena()->create<parser::v3::ExistsExpr>();
        exists->negated = false;
        exists->subquery = sub;
        return exists;
    }

    if (check(TokenType::PARAMETER)) {
        auto* param = arena()->create<parser::v3::ParameterExpr>();
        int64_t position = current_token_.value.int_value;
        if (position <= 0 || position > static_cast<int64_t>(std::numeric_limits<uint16_t>::max())) {
            error("Parameter index out of range");
            advance();
            return makeLiteralNull();
        }
        param->is_named = false;
        param->index = static_cast<uint32_t>(position);
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_PLACEHOLDER));
        emitU16(static_cast<uint16_t>(position));
        advance();
        return param;
    }

    if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER) ||
        isNonReservedKeyword(current_token_.type)) {
        std::vector<std::string> parts;
        parts.push_back(parseIdentifier());
        if (match(TokenType::LEFT_PAREN)) {
            return parseFunctionCall(parts.front());
        }
        while (match(TokenType::DOT)) {
            if (parts.size() >= 3) {
                error("PostgreSQL column references support at most schema.table.column");
                // Consume remaining qualifier segments to keep parser in sync.
                parseIdentifier();
                while (match(TokenType::DOT)) {
                    parseIdentifier();
                }
                break;
            }
            if (match(TokenType::STAR)) {
                // SELECT table.* style wildcard reference.
                auto* expr = arena()->create<parser::v3::ColumnRefExpr>();
                std::vector<parser::v3::StringPool::StringId> path_parts;
                path_parts.reserve(parts.size());
                for (const auto& part : parts) {
                    path_parts.push_back(string_pool_.intern(part));
                }
                expr->column = parser::v3::ColumnRef(
                    parser::v3::SchemaPath(parser::v3::PathType::UNQUALIFIED, std::move(path_parts)),
                    string_pool_.intern("*"));
                return expr;
            }
            parts.push_back(parseIdentifier());
        }
        return makeColumnRef(parts);
    }

    if (matchKeyword(TokenType::KW_COUNT)) {
        if (match(TokenType::LEFT_PAREN)) {
            auto* fn = arena()->create<parser::v3::FunctionCallExpr>();
            fn->function_path = parser::v3::SchemaPath(parser::v3::PathType::UNQUALIFIED,
                                                       {string_pool_.intern("count")});
            if (!match(TokenType::STAR)) {
                fn->arguments.push_back(parseExpression());
            }
            consume(TokenType::RIGHT_PAREN, "Expected ) after COUNT");
            return fn;
        }
    }

    error("Expected expression");
    return makeLiteralNull();
}

parser::v3::Expression* Parser::parseFunctionCall(const std::string& name) {
    auto* fn = arena()->create<parser::v3::FunctionCallExpr>();
    fn->function_path = parser::v3::SchemaPath(parser::v3::PathType::UNQUALIFIED,
                                               {string_pool_.intern(name)});
    if (matchKeyword(TokenType::KW_DISTINCT)) {
        fn->distinct = true;
    }
    if (match(TokenType::RIGHT_PAREN)) {
        // no args
    } else if (match(TokenType::STAR)) {
        consume(TokenType::RIGHT_PAREN, "Expected ) after *");
    } else {
        fn->arguments.push_back(parseExpression());
        while (match(TokenType::COMMA)) {
            fn->arguments.push_back(parseExpression());
        }
        // ORDER BY within aggregate
        if (matchKeyword(TokenType::KW_ORDER)) {
            consumeKeyword(TokenType::KW_BY, "Expected BY after ORDER");
            do {
                auto* item = arena()->create<parser::v3::OrderByItem>();
                item->expr = parseExpression();
                if (matchKeyword(TokenType::KW_ASC)) {
                    item->ascending = true;
                } else if (matchKeyword(TokenType::KW_DESC)) {
                    item->ascending = false;
                }
                if (matchKeyword(TokenType::KW_NULLS)) {
                    item->has_nulls_spec = true;
                    if (matchKeyword(TokenType::KW_FIRST)) item->nulls_first = true;
                    else if (matchKeyword(TokenType::KW_LAST)) item->nulls_last = true;
                }
                fn->order_by.push_back(item);
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RIGHT_PAREN, "Expected ) after function arguments");
    }

    if (matchIdentifierKeyword("FILTER")) {
        consume(TokenType::LEFT_PAREN, "Expected ( after FILTER");
        consumeKeyword(TokenType::KW_WHERE, "Expected WHERE after FILTER (");
        fn->filter = parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected ) after FILTER");
    }

    if (matchKeyword(TokenType::KW_OVER)) {
        fn->is_window = true;
        if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER) || isNonReservedKeyword(current_token_.type)) {
            fn->window = arena()->create<parser::v3::WindowSpec>();
            fn->window->ref_name = parseIdentifierId();
            fn->window->has_ref = true;
        } else if (match(TokenType::LEFT_PAREN)) {
            auto* win = arena()->create<parser::v3::WindowSpec>();
            if (matchKeyword(TokenType::KW_PARTITION)) {
                consumeKeyword(TokenType::KW_BY, "Expected BY after PARTITION");
                do { win->partition_by.push_back(parseExpression()); } while (match(TokenType::COMMA));
            }
            if (matchKeyword(TokenType::KW_ORDER)) {
                consumeKeyword(TokenType::KW_BY, "Expected BY after ORDER");
                do {
                    auto* item = arena()->create<parser::v3::OrderByItem>();
                    item->expr = parseExpression();
                    if (matchKeyword(TokenType::KW_ASC)) item->ascending = true;
                    else if (matchKeyword(TokenType::KW_DESC)) item->ascending = false;
                    if (matchKeyword(TokenType::KW_NULLS)) {
                        item->has_nulls_spec = true;
                        if (matchKeyword(TokenType::KW_FIRST)) item->nulls_first = true;
                        else if (matchKeyword(TokenType::KW_LAST)) item->nulls_last = true;
                    }
                    win->order_by.push_back(item);
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RIGHT_PAREN, "Expected ) after window clause");
            fn->window = win;
        }
    }

    return fn;
}

parser::v3::Expression* Parser::parseCaseExpr() {
    consumeKeyword(TokenType::KW_CASE, "Expected CASE");
    auto* expr = arena()->create<parser::v3::CaseExpr>();
    if (!check(TokenType::KW_WHEN)) {
        expr->operand = parseExpression();
    }
    while (matchKeyword(TokenType::KW_WHEN)) {
        parser::v3::CaseExpr::WhenClause clause;
        clause.when_expr = parseExpression();
        consumeKeyword(TokenType::KW_THEN, "Expected THEN in CASE");
        clause.then_expr = parseExpression();
        expr->when_clauses.push_back(clause);
    }
    if (matchKeyword(TokenType::KW_ELSE)) {
        expr->else_expr = parseExpression();
    }
    consumeKeyword(TokenType::KW_END, "Expected END in CASE");
    return expr;
}

parser::v3::Expression* Parser::parseCastExpr() {
    consumeKeyword(TokenType::KW_CAST, "Expected CAST");
    consume(TokenType::LEFT_PAREN, "Expected (");
    auto* value = parseExpression();
    consumeKeyword(TokenType::KW_AS, "Expected AS");
    PgDataType type = parseDataType();
    consume(TokenType::RIGHT_PAREN, "Expected )");
    auto* expr = arena()->create<parser::v3::CastExpr>();
    expr->expr = value;
    expr->target_type = pgTypeToTypeName(type, string_pool_);
    return expr;
}

parser::v3::Expression* Parser::parseExtractExpr() {
    auto* expr = arena()->create<parser::v3::ExtractExpr>();
    expr->selector = parseElementSelector();
    consumeKeyword(TokenType::KW_FROM, "Expected FROM in EXTRACT");
    expr->source = parseExpression();
    return expr;
}

parser::v3::Expression* Parser::parseAlterElementExpr() {
    auto* expr = arena()->create<parser::v3::AlterElementExpr>();
    expr->selector = parseElementSelector();
    expr->source = parseExpression();
    if (matchKeyword(TokenType::KW_SET)) {
        expr->new_value = parseExpression();
    }
    return expr;
}

parser::v3::ElementSelector Parser::parseElementSelector() {
    parser::v3::ElementSelector sel;
    if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER) || isNonReservedKeyword(current_token_.type)) {
        sel.kind = parser::v3::ElementSelector::Kind::IDENTIFIER;
        sel.identifier = parseIdentifierId();
    } else if (check(TokenType::STRING_LITERAL)) {
        sel.kind = parser::v3::ElementSelector::Kind::STRING_LITERAL;
        sel.string_literal = internFromLexer(current_token_.value.string_id);
        advance();
    } else if (check(TokenType::INTEGER_LITERAL)) {
        sel.kind = parser::v3::ElementSelector::Kind::INTEGER_EXPR;
        sel.expr = makeLiteralInt(current_token_.value.int_value);
        advance();
    } else {
        error("Expected element selector");
    }
    return sel;
}

parser::v3::Expression* Parser::parseArrayConstructor() {
    consumeKeyword(TokenType::KW_ARRAY, "Expected ARRAY");
    auto* expr = arena()->create<parser::v3::ArrayExpr>();
    if (match(TokenType::LEFT_BRACKET)) {
        if (!match(TokenType::RIGHT_BRACKET)) {
            do { expr->elements.push_back(parseExpression()); } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_BRACKET, "Expected ]");
        }
    } else if (match(TokenType::LEFT_PAREN)) {
        auto* sub = parseSubquery();
        expr->subquery = sub;
        expr->has_subquery = true;
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }
    return expr;
}

parser::v3::SelectStmt* Parser::parseSubquery() {
    return parseSelectStmt();
}

parser::v3::Expression* Parser::parseTypeCast(parser::v3::Expression* base) {
    PgDataType type = parseDataType();
    auto* expr = arena()->create<parser::v3::CastExpr>();
    expr->expr = base;
    expr->target_type = pgTypeToTypeName(type, string_pool_);
    return expr;
}

} // namespace scratchbird::parser::postgresql
