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

// ============================================================================
// Expression Parsing (Operator Precedence)
// ============================================================================

void Parser::parseExpression() {
    parseOrExpr();
}

void Parser::parseOrExpr() {
    parseAndExpr();

    while (matchKeyword(TokenType::KW_OR)) {
        parseAndExpr();
        emit(sblr::Opcode::EXPR_OR);
    }
}

void Parser::parseAndExpr() {
    parseNotExpr();

    while (matchKeyword(TokenType::KW_AND)) {
        parseNotExpr();
        emit(sblr::Opcode::EXPR_AND);
    }
}

void Parser::parseNotExpr() {
    if (matchKeyword(TokenType::KW_NOT)) {
        parseNotExpr();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_NOT));
        return;
    }
    parseComparisonExpr();
}

void Parser::parseComparisonExpr() {
    parseIsExpr();

    // Comparison operators
    if (match(TokenType::EQUAL)) {
        parseIsExpr();
        emit(sblr::Opcode::EXPR_EQ);
    } else if (match(TokenType::NOT_EQUAL) || match(TokenType::LESS_GREATER)) {
        parseIsExpr();
        emit(sblr::Opcode::EXPR_NE);
    } else if (match(TokenType::LESS_THAN)) {
        parseIsExpr();
        emit(sblr::Opcode::EXPR_LT);
    } else if (match(TokenType::GREATER_THAN)) {
        parseIsExpr();
        emit(sblr::Opcode::EXPR_GT);
    } else if (match(TokenType::LESS_EQUAL)) {
        parseIsExpr();
        emit(sblr::Opcode::EXPR_LE);
    } else if (match(TokenType::GREATER_EQUAL)) {
        parseIsExpr();
        emit(sblr::Opcode::EXPR_GE);
    }
}

void Parser::parseIsExpr() {
    parseInExpr();

    // IS [NOT] NULL, IS [NOT] TRUE, IS [NOT] FALSE, IS [NOT] DISTINCT FROM
    if (matchKeyword(TokenType::KW_IS)) {
        bool is_not = matchKeyword(TokenType::KW_NOT);

        if (matchKeyword(TokenType::KW_NULL)) {
            // IS [NOT] NULL
            emit(sblr::Opcode::LITERAL_NULL);
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_NULL_SAFE_EQ));
            if (is_not) {
                emit(sblr::Opcode::LITERAL_INT32);
                emitU32(0);
                emit(sblr::Opcode::EXPR_EQ);
            }
        } else if (matchKeyword(TokenType::KW_TRUE)) {
            // IS [NOT] TRUE
            emit(sblr::Opcode::LITERAL_INT32);
            emitU32(1);
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_NULL_SAFE_EQ));
            if (is_not) {
                emit(sblr::Opcode::LITERAL_INT32);
                emitU32(0);
                emit(sblr::Opcode::EXPR_EQ);
            }
        } else if (matchKeyword(TokenType::KW_FALSE)) {
            // IS [NOT] FALSE
            emit(sblr::Opcode::LITERAL_INT32);
            emitU32(0);
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_NULL_SAFE_EQ));
            if (is_not) {
                emit(sblr::Opcode::LITERAL_INT32);
                emitU32(0);
                emit(sblr::Opcode::EXPR_EQ);
            }
        } else if (matchKeyword(TokenType::KW_DISTINCT)) {
            consumeKeyword(TokenType::KW_FROM, "Expected FROM after DISTINCT");
            parseInExpr();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_NULL_SAFE_EQ));
            if (!is_not) {
                emit(sblr::Opcode::LITERAL_INT32);
                emitU32(0);
                emit(sblr::Opcode::EXPR_EQ);
            }
        } else if (matchKeyword(TokenType::KW_UNKNOWN)) {
            // IS [NOT] UNKNOWN (same as IS [NOT] NULL for boolean)
            emit(sblr::Opcode::LITERAL_NULL);
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_NULL_SAFE_EQ));
            if (is_not) {
                emit(sblr::Opcode::LITERAL_INT32);
                emitU32(0);
                emit(sblr::Opcode::EXPR_EQ);
            }
        } else {
            error("Expected NULL, TRUE, FALSE, DISTINCT, or UNKNOWN after IS");
        }
    }
}

void Parser::parseInExpr() {
    parseBetweenExpr();

    // [NOT] IN (values) or [NOT] IN (subquery)
    bool is_not = false;
    if (matchKeyword(TokenType::KW_NOT)) {
        is_not = true;
    }

    if (matchKeyword(TokenType::KW_IN)) {
        consume(TokenType::LEFT_PAREN, "Expected ( after IN");

        if (check(TokenType::KW_SELECT)) {
            // Subquery
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(is_not ? sblr::ExtendedOpcode::EXT_SUBQUERY_NOT_IN
                                                  : sblr::ExtendedOpcode::EXT_SUBQUERY_IN));
            parseSubquery();
        } else {
            // Value list
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_IN_LIST));
            emitByte(is_not ? 1 : 0);

            emit(sblr::Opcode::BEGIN_LIST);
            size_t count_pos = bytecode_.size();
            emitU32(0);

            uint32_t count = 0;
            do {
                parseExpression();
                count++;
            } while (match(TokenType::COMMA));

            sblr::writeInt32(&bytecode_[count_pos], count);
            emit(sblr::Opcode::END_LIST);
        }

        consume(TokenType::RIGHT_PAREN, "Expected ) after IN list");
    } else if (is_not) {
        // NOT without IN - put back for parseBetweenExpr
        // Actually we already consumed NOT, so this is an error
        error("Expected IN after NOT");
    }
}

void Parser::parseBetweenExpr() {
    parseLikeExpr();

    // [NOT] BETWEEN expr AND expr
    bool is_not = false;
    if (matchKeyword(TokenType::KW_NOT)) {
        is_not = true;
    }

    if (matchKeyword(TokenType::KW_BETWEEN)) {
        // BETWEEN is: expr >= low AND expr <= high
        // NOT BETWEEN is: expr < low OR expr > high

        parseLikeExpr();  // low value
        consumeKeyword(TokenType::KW_AND, "Expected AND in BETWEEN");
        parseLikeExpr();  // high value

        // Emit BETWEEN as compound comparison
        if (is_not) {
            // NOT BETWEEN: x < low OR x > high
            emit(sblr::Opcode::EXPR_LT);
            emit(sblr::Opcode::EXPR_GT);
            emit(sblr::Opcode::EXPR_OR);
        } else {
            // BETWEEN: x >= low AND x <= high
            emit(sblr::Opcode::EXPR_GE);
            emit(sblr::Opcode::EXPR_LE);
            emit(sblr::Opcode::EXPR_AND);
        }
    } else if (is_not) {
        // NOT consumed but no BETWEEN - error
        error("Expected BETWEEN after NOT");
    }
}

void Parser::parseLikeExpr() {
    parseBitwiseOrExpr();

    // [NOT] LIKE pattern [ESCAPE escape]
    // [NOT] ILIKE pattern [ESCAPE escape]
    // [NOT] SIMILAR TO pattern [ESCAPE escape]
    bool is_not = false;
    if (matchKeyword(TokenType::KW_NOT)) {
        is_not = true;
    }

    if (matchKeyword(TokenType::KW_LIKE)) {
        parseBitwiseOrExpr();  // pattern
        bool has_escape = false;
        if (matchKeyword(TokenType::KW_ESCAPE)) {
            parseBitwiseOrExpr();  // escape char
            has_escape = true;
        }

        if (has_escape) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_LIKE_ESCAPE));
        } else {
            emit(sblr::Opcode::EXPR_LIKE);
        }

        if (is_not) {
            emit(sblr::Opcode::LITERAL_INT32);
            emitU32(0);
            emit(sblr::Opcode::EXPR_EQ);
        }
    } else if (matchKeyword(TokenType::KW_ILIKE)) {
        parseBitwiseOrExpr();  // pattern
        bool has_escape = false;
        if (matchKeyword(TokenType::KW_ESCAPE)) {
            parseBitwiseOrExpr();
            has_escape = true;
        }

        if (has_escape) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ILIKE_ESCAPE));
        } else {
            emit(sblr::Opcode::EXPR_ILIKE);
        }

        if (is_not) {
            emit(sblr::Opcode::LITERAL_INT32);
            emitU32(0);
            emit(sblr::Opcode::EXPR_EQ);
        }
    } else if (matchKeyword(TokenType::KW_SIMILAR)) {
        consumeKeyword(TokenType::KW_TO, "Expected TO after SIMILAR");
        parseBitwiseOrExpr();  // pattern
        // SIMILAR TO uses regex matching
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(is_not ? sblr::ExtendedOpcode::EXT_REGEX_NOT_MATCH
                                              : sblr::ExtendedOpcode::EXT_REGEX_MATCH));

        if (matchKeyword(TokenType::KW_ESCAPE)) {
            parseBitwiseOrExpr();
        }
    } else if (is_not) {
        error("Expected LIKE, ILIKE, or SIMILAR after NOT");
    }
}

void Parser::parseBitwiseOrExpr() {
    parseBitwiseXorExpr();

    while (match(TokenType::PIPE)) {
        parseBitwiseXorExpr();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_OR));
    }
}

void Parser::parseBitwiseXorExpr() {
    parseBitwiseAndExpr();

    while (match(TokenType::CARET)) {
        parseBitwiseAndExpr();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_XOR));
    }
}

void Parser::parseBitwiseAndExpr() {
    parseShiftExpr();

    while (match(TokenType::AMPERSAND)) {
        parseShiftExpr();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_AND));
    }
}

void Parser::parseShiftExpr() {
    parseAdditiveExpr();

    while (true) {
        if (match(TokenType::LEFT_SHIFT)) {
            parseAdditiveExpr();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_SHIFT_LEFT));
        } else if (match(TokenType::RIGHT_SHIFT)) {
            parseAdditiveExpr();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_SHIFT_RIGHT));
        } else {
            break;
        }
    }
}

void Parser::parseAdditiveExpr() {
    parseMultiplicativeExpr();

    while (true) {
        if (match(TokenType::PLUS)) {
            parseMultiplicativeExpr();
            emit(sblr::Opcode::EXPR_ADD);
        } else if (match(TokenType::MINUS)) {
            parseMultiplicativeExpr();
            emit(sblr::Opcode::EXPR_SUBTRACT);
        } else if (match(TokenType::CONCAT)) {
            // || is string concatenation in PostgreSQL
            parseMultiplicativeExpr();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ARRAY_CAT));  // Reuse for string concat
        } else {
            break;
        }
    }
}

void Parser::parseMultiplicativeExpr() {
    parseUnaryExpr();

    while (true) {
        if (match(TokenType::STAR)) {
            parseUnaryExpr();
            emit(sblr::Opcode::EXPR_MULTIPLY);
        } else if (match(TokenType::SLASH)) {
            parseUnaryExpr();
            emit(sblr::Opcode::EXPR_DIVIDE);
        } else if (match(TokenType::PERCENT)) {
            parseUnaryExpr();
            emit(sblr::Opcode::EXPR_MODULO);
        } else {
            break;
        }
    }
}

void Parser::parseUnaryExpr() {
    if (match(TokenType::MINUS)) {
        parseUnaryExpr();
        // Negate: multiply by -1
        emit(sblr::Opcode::LITERAL_INT64);
        emitI64(-1);
        emit(sblr::Opcode::EXPR_MULTIPLY);
        return;
    }
    if (match(TokenType::PLUS)) {
        parseUnaryExpr();
        return;
    }
    if (match(TokenType::TILDE)) {
        // Bitwise NOT
        parseUnaryExpr();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_NOT));
        return;
    }

    parsePostfixExpr();
}

void Parser::parsePostfixExpr() {
    parsePrimaryExpr();
    parsePostfixTail();
}

void Parser::parsePostfixTail() {
    // Handle postfix operators
    while (true) {
        if (match(TokenType::DOUBLE_COLON)) {
            // :: type cast operator
            parseTypeCast();
        } else if (match(TokenType::LEFT_BRACKET)) {
            // Array subscript
            parseExpression();
            consume(TokenType::RIGHT_BRACKET, "Expected ]");
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ARRAY_SUBSCRIPT));
        } else if (match(TokenType::ARROW)) {
            // -> JSON object field
            parseExpression();
            emit(sblr::Opcode::JSON_ARROW);
        } else if (match(TokenType::DOUBLE_ARROW)) {
            // ->> JSON object field as text
            parseExpression();
            emit(sblr::Opcode::JSON_DOUBLE_ARROW);
        } else if (match(TokenType::HASH_ARROW)) {
            // #> JSON path
            parseExpression();
            emit(sblr::Opcode::JSON_HASH_ARROW);
        } else if (match(TokenType::HASH_DOUBLE_ARROW)) {
            // #>> JSON path as text
            parseExpression();
            emit(sblr::Opcode::JSON_HASH_DOUBLE_ARROW);
        } else if (match(TokenType::AT_GREATER)) {
            // @> contains
            parsePrimaryExpr();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ARRAY_CONTAINS));
        } else if (match(TokenType::LESS_AT)) {
            // <@ contained by
            parsePrimaryExpr();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ARRAY_CONTAINED_BY));
        } else if (match(TokenType::QUESTION)) {
            // ? key exists
            parsePrimaryExpr();
            // JSON key exists check
            emit(sblr::Opcode::JSON_EXTRACT);
            emit(sblr::Opcode::LITERAL_NULL);
            emit(sblr::Opcode::EXPR_NE);
        } else if (match(TokenType::QUESTION_PIPE)) {
            // ?| any key exists
            parsePrimaryExpr();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ARRAY_OVERLAP));
        } else if (match(TokenType::QUESTION_AMPERSAND)) {
            // ?& all keys exist
            parsePrimaryExpr();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ARRAY_CONTAINS));
        } else if (match(TokenType::AT_AT)) {
            // @@ text search match or JSON path match
            parsePrimaryExpr();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_TSMATCH));
        } else {
            break;
        }
    }
}

void Parser::parsePrimaryExpr() {
    // Literals
    if (check(TokenType::INTEGER_LITERAL)) {
        emit(sblr::Opcode::LITERAL_INT64);
        emitI64(current_token_.value.int_value);
        advance();
        return;
    }

    if (check(TokenType::FLOAT_LITERAL)) {
        emit(sblr::Opcode::LITERAL_DOUBLE);
        emitF64(current_token_.value.float_value);
        advance();
        return;
    }

    if (check(TokenType::STRING_LITERAL) || check(TokenType::DOLLAR_STRING) ||
        check(TokenType::ESCAPE_STRING)) {
        uint32_t id = current_token_.value.string_id;
        std::string_view str = lexer_.stringPool().get(id);
        emit(sblr::Opcode::LITERAL_STRING);
        emitString(str);
        advance();
        return;
    }

    if (matchKeyword(TokenType::KW_NULL)) {
        emit(sblr::Opcode::LITERAL_NULL);
        return;
    }

    if (matchKeyword(TokenType::KW_TRUE)) {
        emit(sblr::Opcode::LITERAL_INT32);
        emitU32(1);
        return;
    }

    if (matchKeyword(TokenType::KW_FALSE)) {
        emit(sblr::Opcode::LITERAL_INT32);
        emitU32(0);
        return;
    }

    // CASE expression
    if (check(TokenType::KW_CASE)) {
        parseCaseExpr();
        return;
    }

    // CAST expression
    if (check(TokenType::KW_CAST)) {
        parseCastExpr();
        return;
    }

    if (matchKeyword(TokenType::KW_EXTRACT)) {
        parseExtractExpr();
        return;
    }

    if (matchKeyword(TokenType::KW_ALTER_ELEMENT)) {
        parseAlterElementExpr();
        return;
    }

    // ARRAY constructor
    if (check(TokenType::KW_ARRAY)) {
        parseArrayConstructor();
        return;
    }

    // Subquery (SELECT)
    if (check(TokenType::LEFT_PAREN)) {
        Token paren = current_token_;
        advance();

        if (check(TokenType::KW_SELECT)) {
            // Scalar subquery
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_SCALAR));
            parseSubquery();
            consume(TokenType::RIGHT_PAREN, "Expected ) after subquery");
            return;
        }

        // Parenthesized expression
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    // EXISTS subquery
    if (matchKeyword(TokenType::KW_EXISTS)) {
        consume(TokenType::LEFT_PAREN, "Expected ( after EXISTS");
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_EXISTS));
        parseSubquery();
        consume(TokenType::RIGHT_PAREN, "Expected ) after EXISTS subquery");
        return;
    }

    // Positional parameter ($1, $2, etc.)
    if (check(TokenType::PARAMETER)) {
        // Parameters are stored as their number
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_PLACEHOLDER));
        int64_t position = current_token_.value.int_value;
        if (position <= 0 ||
            position > static_cast<int64_t>(std::numeric_limits<uint16_t>::max())) {
            error("Parameter index out of range");
            advance();
            return;
        }
        emitU16(static_cast<uint16_t>(position));
        emitU16(0);  // type hint unknown
        advance();
        return;
    }

    // Identifier (column reference or function call)
    if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER) ||
        isNonReservedKeyword(current_token_.type)) {
        std::string name = parseIdentifier();
        int parts = 1;
        std::string column = name;

        // Check for function call
        if (match(TokenType::LEFT_PAREN)) {
            parseFunctionCall(name);
            return;
        }

        // Check for qualified name (table.column or schema.table.column)
        while (match(TokenType::DOT)) {
            if (parts >= 3) {
                error("PostgreSQL column references must be schema.table.column");
            }
            column = parseIdentifier();
            parts++;
        }

        emit(sblr::Opcode::COLUMN_REF);
        emitString(column);
        return;
    }

    // Special aggregate functions without arguments
    if (matchKeyword(TokenType::KW_COUNT)) {
        if (match(TokenType::LEFT_PAREN)) {
            if (match(TokenType::STAR)) {
                emit(sblr::Opcode::AGG_COUNT);
                emitByte(1);
                emit(sblr::Opcode::LITERAL_NULL);  // COUNT(*)
            } else {
                emit(sblr::Opcode::AGG_COUNT);
                emitByte(1);
                parseExpression();
            }
            consume(TokenType::RIGHT_PAREN, "Expected ) after COUNT");
            return;
        }
    }

    error("Expected expression");
}

void Parser::parseFunctionCall(const std::string& name) {
    // Convert function name to lowercase for comparison
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

    // Special handling for aggregate functions
    if (lower_name == "count") {
        if (match(TokenType::STAR)) {
            emit(sblr::Opcode::AGG_COUNT);
            emitByte(1);
            emit(sblr::Opcode::LITERAL_NULL);
        } else {
            emit(sblr::Opcode::AGG_COUNT);
            emitByte(1);
            parseExpression();
        }
        consume(TokenType::RIGHT_PAREN, "Expected ) after COUNT");
        return;
    }
    if (lower_name == "sum") {
        emit(sblr::Opcode::AGG_SUM);
        emitByte(1);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }
    if (lower_name == "avg") {
        emit(sblr::Opcode::AGG_AVG);
        emitByte(1);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }
    if (lower_name == "min") {
        emit(sblr::Opcode::AGG_MIN);
        emitByte(1);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }
    if (lower_name == "max") {
        emit(sblr::Opcode::AGG_MAX);
        emitByte(1);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    // Window functions
    if (lower_name == "row_number") {
        emit(sblr::Opcode::WIN_ROW_NUMBER);
        consume(TokenType::RIGHT_PAREN, "Expected )");
        if (matchKeyword(TokenType::KW_OVER)) {
            parseWindowClause();
        }
        return;
    }
    if (lower_name == "rank") {
        emit(sblr::Opcode::WIN_RANK);
        consume(TokenType::RIGHT_PAREN, "Expected )");
        if (matchKeyword(TokenType::KW_OVER)) {
            parseWindowClause();
        }
        return;
    }
    if (lower_name == "dense_rank") {
        emit(sblr::Opcode::WIN_DENSE_RANK);
        consume(TokenType::RIGHT_PAREN, "Expected )");
        if (matchKeyword(TokenType::KW_OVER)) {
            parseWindowClause();
        }
        return;
    }
    if (lower_name == "lag" || lower_name == "lead") {
        emit(lower_name == "lag" ? sblr::Opcode::WIN_LAG : sblr::Opcode::WIN_LEAD);
        parseExpression();  // value expression
        if (match(TokenType::COMMA)) {
            parseExpression();  // offset
            if (match(TokenType::COMMA)) {
                parseExpression();  // default
            }
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
        if (matchKeyword(TokenType::KW_OVER)) {
            parseWindowClause();
        }
        return;
    }

    // String functions
    if (lower_name == "upper") {
        emit(sblr::Opcode::FUNC_UPPER);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }
    if (lower_name == "lower") {
        emit(sblr::Opcode::FUNC_LOWER);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }
    if (lower_name == "length" || lower_name == "char_length" || lower_name == "character_length") {
        emit(sblr::Opcode::FUNC_CHAR_LENGTH);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }
    if (lower_name == "substring" || lower_name == "substr") {
        emit(sblr::Opcode::FUNC_SUBSTRING);
        parseExpression();  // string
        if (matchKeyword(TokenType::KW_FROM) || match(TokenType::COMMA)) {
            parseExpression();  // start
        }
        if (matchKeyword(TokenType::KW_FOR) || match(TokenType::COMMA)) {
            parseExpression();  // length
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }
    if (lower_name == "trim") {
        emit(sblr::Opcode::FUNC_TRIM);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    // Date/time functions
    if (lower_name == "now" || lower_name == "current_timestamp") {
        emit(sblr::Opcode::FUNC_NOW);
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }
    if (lower_name == "current_date") {
        emit(sblr::Opcode::FUNC_CURRENT_DATE);
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    // Math functions
    if (lower_name == "abs") {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_FUNC_ABS));
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }
    if (lower_name == "sqrt") {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_FUNC_SQRT));
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }
    if (lower_name == "round") {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_FUNC_ROUND));
        parseExpression();
        if (match(TokenType::COMMA)) {
            parseExpression();  // precision
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    // COALESCE and NULLIF
    if (lower_name == "coalesce") {
        emit(sblr::Opcode::COALESCE);
        emit(sblr::Opcode::BEGIN_LIST);
        size_t count_pos = bytecode_.size();
        emitU32(0);
        uint32_t count = 0;
        do {
            parseExpression();
            count++;
        } while (match(TokenType::COMMA));
        sblr::writeInt32(&bytecode_[count_pos], count);
        emit(sblr::Opcode::END_LIST);
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }
    if (lower_name == "nullif") {
        emit(sblr::Opcode::NULLIF);
        parseExpression();
        consume(TokenType::COMMA, "Expected , in NULLIF");
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    // JSON functions
    if (lower_name == "json_object" || lower_name == "jsonb_build_object") {
        emit(lower_name == "json_object" ? sblr::Opcode::JSON_OBJECT : sblr::Opcode::JSONB_BUILD_OBJECT);
        emit(sblr::Opcode::BEGIN_LIST);
        size_t count_pos = bytecode_.size();
        emitU32(0);
        uint32_t count = 0;
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                parseExpression();
                count++;
            } while (match(TokenType::COMMA));
        }
        sblr::writeInt32(&bytecode_[count_pos], count);
        emit(sblr::Opcode::END_LIST);
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    // Generic function call
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CALL));
    emitString(name);

    // Parse arguments
    emit(sblr::Opcode::BEGIN_LIST);
    size_t count_pos = bytecode_.size();
    emitU32(0);

    uint32_t count = 0;
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            parseExpression();
            count++;
        } while (match(TokenType::COMMA));
    }

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);
    consume(TokenType::RIGHT_PAREN, "Expected )");
}

void Parser::parseCaseExpr() {
    consume(TokenType::KW_CASE, "Expected CASE");
    emit(sblr::Opcode::CASE_WHEN);

    // Simple CASE vs searched CASE
    bool is_simple = !check(TokenType::KW_WHEN);
    if (is_simple) {
        // Simple CASE: CASE expr WHEN value1 THEN result1 ...
        parseExpression();
    }
    emitByte(is_simple ? 1 : 0);

    // Parse WHEN clauses
    emit(sblr::Opcode::BEGIN_LIST);
    size_t count_pos = bytecode_.size();
    emitU32(0);

    uint32_t count = 0;
    while (matchKeyword(TokenType::KW_WHEN)) {
        parseExpression();  // condition or value
        consumeKeyword(TokenType::KW_THEN, "Expected THEN");
        parseExpression();  // result
        count++;
    }

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);

    // ELSE clause
    if (matchKeyword(TokenType::KW_ELSE)) {
        parseExpression();
    } else {
        emit(sblr::Opcode::LITERAL_NULL);
    }

    consumeKeyword(TokenType::KW_END, "Expected END");
}

void Parser::parseCastExpr() {
    consume(TokenType::KW_CAST, "Expected CAST");
    consume(TokenType::LEFT_PAREN, "Expected (");

    parseExpression();

    consumeKeyword(TokenType::KW_AS, "Expected AS");

    PgDataType type = parseDataType();
    core::CastFormat cast_format = core::CastFormat::DEFAULT;
    // CAST ... USING <format> (see docs/specifications/DATA_TYPE_PERSISTENCE_AND_CASTS.md)
    if (matchKeyword(TokenType::KW_USING)) {
        std::string fmt = parseIdentifier();
        std::transform(fmt.begin(), fmt.end(), fmt.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (fmt == "hex" || fmt == "hexadecimal") {
            cast_format = core::CastFormat::HEX;
        } else if (fmt == "base64") {
            cast_format = core::CastFormat::BASE64;
        } else if (fmt == "escape") {
            cast_format = core::CastFormat::ESCAPE;
        } else {
            error("Unknown CAST USING format: " + fmt);
        }
    }
    emit(sblr::Opcode::EXPR_CAST);
    emitByte(0);  // try_cast = false
    emitTypeDefinition(type);
    emitByte(static_cast<uint8_t>(cast_format));

    consume(TokenType::RIGHT_PAREN, "Expected )");
}

void Parser::parseExtractExpr() {
    consume(TokenType::LEFT_PAREN, "Expected ( after EXTRACT");
    uint8_t arg_count = 0;
    sblr::ExtractField field = parseElementSelector(arg_count);
    consumeKeyword(TokenType::KW_FROM, "Expected FROM in EXTRACT expression");
    parseExpression();
    consume(TokenType::RIGHT_PAREN, "Expected ) after EXTRACT");

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_EXTRACT));
    emitByte(static_cast<uint8_t>(field));
    emitByte(arg_count);
}

void Parser::parseAlterElementExpr() {
    consume(TokenType::LEFT_PAREN, "Expected ( after ALTER_ELEMENT");
    uint8_t arg_count = 0;
    sblr::ExtractField field = parseElementSelector(arg_count);
    consumeKeyword(TokenType::KW_IN, "Expected IN in ALTER_ELEMENT expression");
    parseExpression();
    consumeKeyword(TokenType::KW_TO, "Expected TO in ALTER_ELEMENT expression");
    parseExpression();
    consume(TokenType::RIGHT_PAREN, "Expected ) after ALTER_ELEMENT");

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_ELEMENT));
    emitByte(static_cast<uint8_t>(field));
    emitByte(arg_count);
}

sblr::ExtractField Parser::parseElementSelector(uint8_t& arg_count) {
    arg_count = 0;

    if (check(TokenType::STRING_LITERAL) || check(TokenType::DOLLAR_STRING) ||
        check(TokenType::ESCAPE_STRING)) {
        std::string_view path = lexer_.stringPool().get(current_token_.value.string_id);
        emit(sblr::Opcode::LITERAL_STRING);
        emitString(path);
        advance();
        arg_count = 1;
        return sblr::ExtractField::PATH;
    }

    if (check(TokenType::INTEGER_LITERAL) || check(TokenType::PLUS) ||
        check(TokenType::MINUS) || check(TokenType::LEFT_PAREN)) {
        parseExpression();
        arg_count = 1;
        return sblr::ExtractField::ELEMENT;
    }

    std::string name = parseIdentifier();
    bool has_args = false;

    if (match(TokenType::LEFT_PAREN)) {
        has_args = true;
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                parseExpression();
                arg_count++;
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RIGHT_PAREN, "Expected ) after element selector arguments");
    }

    auto resolved = sblr::resolveExtractFieldName(name);
    if (!resolved) {
        if (has_args) {
            error("Unknown EXTRACT element: " + name);
            return sblr::ExtractField::VALUE;
        }
        emit(sblr::Opcode::LITERAL_STRING);
        emitString(name);
        arg_count = 1;
        return sblr::ExtractField::FIELD;
    }

    return *resolved;
}

void Parser::parseArrayConstructor() {
    consume(TokenType::KW_ARRAY, "Expected ARRAY");

    if (match(TokenType::LEFT_BRACKET)) {
        // ARRAY[val1, val2, ...]
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ARRAY_CONSTRUCT));

        emit(sblr::Opcode::BEGIN_LIST);
        size_t count_pos = bytecode_.size();
        emitU32(0);

        uint32_t count = 0;
        if (!check(TokenType::RIGHT_BRACKET)) {
            do {
                parseExpression();
                count++;
            } while (match(TokenType::COMMA));
        }

        sblr::writeInt32(&bytecode_[count_pos], count);
        emit(sblr::Opcode::END_LIST);
        consume(TokenType::RIGHT_BRACKET, "Expected ]");
    } else if (match(TokenType::LEFT_PAREN)) {
        // ARRAY(SELECT ...) - array subquery
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_ARRAY));
        parseSubquery();
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }
}

void Parser::parseSubquery() {
    // Parse a full SELECT statement
    parseSelectStmt();
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_END));
}

void Parser::parseTypeCast() {
    // Already consumed ::
    PgDataType type = parseDataType();
    emit(sblr::Opcode::EXPR_CAST);
    emitByte(0);  // try_cast = false
    emitTypeDefinition(type);
    emitByte(static_cast<uint8_t>(core::CastFormat::DEFAULT));
}

} // namespace scratchbird::parser::postgresql
