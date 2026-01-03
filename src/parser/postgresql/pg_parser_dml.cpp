/**
 * PostgreSQL Parser - DML Statement Parsing
 *
 * Handles SELECT, INSERT, UPDATE, DELETE, MERGE statements.
 */

#include "scratchbird/parser/postgresql/pg_parser.h"
#include <algorithm>

namespace scratchbird::parser::postgresql {

// ============================================================================
// SELECT Statement
// ============================================================================

void Parser::parseSelectStmt() {
    consume(TokenType::KW_SELECT, "Expected SELECT");
    emit(sblr::Opcode::SELECT);

    // Handle SELECT modifiers
    bool is_distinct = false;
    if (matchKeyword(TokenType::KW_DISTINCT)) {
        is_distinct = true;
        // Handle DISTINCT ON (expr, ...)
        if (matchKeyword(TokenType::KW_ON)) {
            consume(TokenType::LEFT_PAREN, "Expected (");
            // Parse DISTINCT ON expressions
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
        }
    } else {
        matchKeyword(TokenType::KW_ALL);  // Optional, default
    }

    // Emit DISTINCT flag
    emitByte(is_distinct ? 1 : 0);

    // Parse select list
    parseSelectList();

    // FROM clause (optional in PostgreSQL for constant expressions)
    if (matchKeyword(TokenType::KW_FROM)) {
        parseFromClause();
    } else {
        // No FROM - emit null table ref
        emitU32(0);  // 0 tables
    }

    // WHERE clause
    if (matchKeyword(TokenType::KW_WHERE)) {
        parseWhereClause();
    } else {
        emit(sblr::Opcode::LITERAL_NULL);  // No WHERE
    }

    // GROUP BY clause
    if (matchKeyword(TokenType::KW_GROUP)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after GROUP");
        parseGroupByClause();
    } else {
        emitU32(0);  // 0 group by columns
    }

    // HAVING clause
    if (matchKeyword(TokenType::KW_HAVING)) {
        parseHavingClause();
    } else {
        emit(sblr::Opcode::LITERAL_NULL);  // No HAVING
    }

    // WINDOW clause (named window definitions)
    if (matchKeyword(TokenType::KW_WINDOW)) {
        parseWindowClause();
    }

    // ORDER BY clause
    if (matchKeyword(TokenType::KW_ORDER)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after ORDER");
        parseOrderByClause();
    } else {
        emitU32(0);  // 0 order by columns
    }

    // LIMIT clause
    if (matchKeyword(TokenType::KW_LIMIT)) {
        parseLimitClause();
    } else {
        emit(sblr::Opcode::LITERAL_NULL);  // No LIMIT
    }

    // OFFSET clause (can appear before or after LIMIT)
    if (matchKeyword(TokenType::KW_OFFSET)) {
        parseOffsetClause();
    }

    // FETCH clause (SQL:2008 syntax)
    if (matchKeyword(TokenType::KW_FETCH)) {
        parseFetchClause();
    }

    // FOR clause (FOR UPDATE, FOR SHARE, etc.)
    if (matchKeyword(TokenType::KW_FOR)) {
        parseForClause();
    }
}

void Parser::parseSelectList() {
    emit(sblr::Opcode::BEGIN_LIST);

    // Count position for patching
    size_t count_pos = bytecode_.size();
    emitU32(0);  // Placeholder for count

    uint32_t count = 0;

    if (match(TokenType::STAR)) {
        // SELECT *
        emit(sblr::Opcode::SELECT_STAR);
        count = 1;
    } else {
        do {
            // Check for table.* syntax
            if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
                std::string first = parseIdentifier();
                if (match(TokenType::DOT)) {
                    if (match(TokenType::STAR)) {
                        // table.*
                        emit(sblr::Opcode::EXTENDED_OPCODE);
                        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SELECT_TABLE_STAR));
                        emitString(first);
                        count++;
                        continue;
                    }
                    // table.column - put back and parse as expression
                    // Actually we need to reconstruct the column reference
                    emit(sblr::Opcode::COLUMN_REF);
                    std::string col = parseIdentifier();
                    emitString(first + "." + col);
                    parsePostfixTail();
                } else if (match(TokenType::LEFT_PAREN)) {
                    // Function call
                    parseFunctionCall(first);
                    parsePostfixTail();
                } else {
                    // Simple column reference
                    emit(sblr::Opcode::COLUMN_REF);
                    emitString(first);
                    parsePostfixTail();
                }
            } else {
                parseExpression();
            }

            // Optional alias
            std::string alias;
            if (matchKeyword(TokenType::KW_AS)) {
                alias = parseIdentifier();
            } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
                alias = parseIdentifier();
            }

            // Emit alias (empty string if none)
            emitString(alias);

            count++;
        } while (match(TokenType::COMMA));
    }

    // Patch the count
    sblr::writeInt32(&bytecode_[count_pos], count);

    emit(sblr::Opcode::END_LIST);
}

void Parser::parseFromClause() {
    emit(sblr::Opcode::BEGIN_LIST);

    size_t count_pos = bytecode_.size();
    emitU32(0);  // Placeholder

    uint32_t count = 0;

    do {
        // Check for LATERAL
        bool is_lateral = matchKeyword(TokenType::KW_LATERAL);

        // Parse table reference
        if (match(TokenType::LEFT_PAREN)) {
            // Subquery or joined table
            if (check(TokenType::KW_SELECT)) {
                emit(sblr::Opcode::EXTENDED_OPCODE);
                emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_SCALAR));
                parseSubquery();
            } else {
                // Parenthesized join - recurse
                parseFromClause();
            }
            consume(TokenType::RIGHT_PAREN, "Expected )");
        } else {
            // Table name
            std::string schema;
            std::string table = parseIdentifier();

            // Check for schema.table
            if (match(TokenType::DOT)) {
                schema = table;
                table = parseIdentifier();
            }

            resolveTableName(schema, table);

            emit(sblr::Opcode::TABLE_REF);
            emitString(schema + "/" + table);
        }

        // Optional alias
        std::string alias;
        if (matchKeyword(TokenType::KW_AS)) {
            alias = parseIdentifier();
        } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
            // Check if this looks like an alias (not a keyword)
            if (!check(TokenType::KW_WHERE) && !check(TokenType::KW_GROUP) &&
                !check(TokenType::KW_ORDER) && !check(TokenType::KW_LIMIT) &&
                !check(TokenType::KW_JOIN) && !check(TokenType::KW_LEFT) &&
                !check(TokenType::KW_RIGHT) && !check(TokenType::KW_INNER) &&
                !check(TokenType::KW_CROSS) && !check(TokenType::KW_ON) &&
                !check(TokenType::KW_FULL) && !check(TokenType::KW_NATURAL) &&
                !check(TokenType::KW_LATERAL)) {
                alias = parseIdentifier();
            }
        }
        emitString(alias);

        count++;

        // Handle JOINs
        parseJoinClause();

    } while (match(TokenType::COMMA));

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);
}

void Parser::parseJoinClause() {
    while (check(TokenType::KW_JOIN) || check(TokenType::KW_LEFT) ||
           check(TokenType::KW_RIGHT) || check(TokenType::KW_INNER) ||
           check(TokenType::KW_CROSS) || check(TokenType::KW_NATURAL) ||
           check(TokenType::KW_FULL)) {

        // Join type
        uint8_t join_type = 0;  // 0=INNER, 1=LEFT, 2=RIGHT, 3=FULL, 4=CROSS

        bool is_natural = matchKeyword(TokenType::KW_NATURAL);

        if (matchKeyword(TokenType::KW_LEFT)) {
            join_type = 1;
            matchKeyword(TokenType::KW_OUTER);
        } else if (matchKeyword(TokenType::KW_RIGHT)) {
            join_type = 2;
            matchKeyword(TokenType::KW_OUTER);
        } else if (matchKeyword(TokenType::KW_FULL)) {
            join_type = 3;
            matchKeyword(TokenType::KW_OUTER);
        } else if (matchKeyword(TokenType::KW_CROSS)) {
            join_type = 4;
        } else if (matchKeyword(TokenType::KW_INNER)) {
            join_type = 0;
        }

        consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");

        emit(sblr::Opcode::JOIN_TYPE);
        emitByte(join_type);

        // Check for LATERAL
        bool is_lateral = matchKeyword(TokenType::KW_LATERAL);

        // Parse joined table
        std::string j_schema;
        std::string j_table = parseIdentifier();
        if (match(TokenType::DOT)) {
            j_schema = j_table;
            j_table = parseIdentifier();
        }
        resolveTableName(j_schema, j_table);

        emit(sblr::Opcode::TABLE_REF);
        emitString(j_schema + "/" + j_table);

        // Optional alias for joined table
        std::string j_alias;
        if (matchKeyword(TokenType::KW_AS)) {
            j_alias = parseIdentifier();
        } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
            if (!check(TokenType::KW_ON) && !check(TokenType::KW_USING) &&
                !check(TokenType::KW_WHERE)) {
                j_alias = parseIdentifier();
            }
        }
        emitString(j_alias);

        // ON or USING clause (not for CROSS JOIN)
        if (join_type != 4 && !is_natural) {
            if (matchKeyword(TokenType::KW_ON)) {
                emit(sblr::Opcode::JOIN_CONDITION);
                parseExpression();
            } else if (matchKeyword(TokenType::KW_USING)) {
                consume(TokenType::LEFT_PAREN, "Expected ( after USING");
                emit(sblr::Opcode::BEGIN_LIST);
                size_t using_count_pos = bytecode_.size();
                emitU32(0);
                uint32_t using_count = 0;

                do {
                    std::string col = parseIdentifier();
                    emit(sblr::Opcode::COLUMN_REF);
                    emitString(col);
                    using_count++;
                } while (match(TokenType::COMMA));

                sblr::writeInt32(&bytecode_[using_count_pos], using_count);
                emit(sblr::Opcode::END_LIST);
                consume(TokenType::RIGHT_PAREN, "Expected ) after USING columns");
            }
        }
    }
}

void Parser::parseWhereClause() {
    emit(sblr::Opcode::WHERE_CLAUSE);
    parseExpression();
}

void Parser::parseGroupByClause() {
    emit(sblr::Opcode::GROUP_BY);
    emit(sblr::Opcode::BEGIN_LIST);

    size_t count_pos = bytecode_.size();
    emitU32(0);

    uint32_t count = 0;

    // Check for ROLLUP, CUBE, GROUPING SETS
    if (matchKeyword(TokenType::KW_ROLLUP)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_GROUP_ROLLUP));
        consume(TokenType::LEFT_PAREN, "Expected (");
        do {
            parseExpression();
            count++;
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    } else if (matchKeyword(TokenType::KW_CUBE)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_GROUP_CUBE));
        consume(TokenType::LEFT_PAREN, "Expected (");
        do {
            parseExpression();
            count++;
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    } else if (matchKeyword(TokenType::KW_GROUPING)) {
        if (matchKeyword(TokenType::KW_SETS)) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_GROUP_GROUPING_SETS));
            consume(TokenType::LEFT_PAREN, "Expected (");
            // Parse grouping sets
            do {
                if (match(TokenType::LEFT_PAREN)) {
                    // Nested set
                    do {
                        parseExpression();
                        count++;
                    } while (match(TokenType::COMMA));
                    consume(TokenType::RIGHT_PAREN, "Expected )");
                } else {
                    parseExpression();
                    count++;
                }
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
    } else {
        // Regular GROUP BY
        do {
            parseExpression();
            count++;
        } while (match(TokenType::COMMA));
    }

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);
}

void Parser::parseHavingClause() {
    emit(sblr::Opcode::HAVING);
    parseExpression();
}

void Parser::parseWindowClause() {
    // Named window definitions or OVER clause
    emit(sblr::Opcode::WINDOW);

    if (match(TokenType::LEFT_PAREN)) {
        emit(sblr::Opcode::WINDOW_SPEC);

        // PARTITION BY
        if (matchKeyword(TokenType::KW_PARTITION)) {
            consumeKeyword(TokenType::KW_BY, "Expected BY after PARTITION");
            emit(sblr::Opcode::PARTITION_BY);
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

        // ORDER BY within window
        if (matchKeyword(TokenType::KW_ORDER)) {
            consumeKeyword(TokenType::KW_BY, "Expected BY after ORDER");
            emit(sblr::Opcode::WINDOW_ORDER_BY);
            emit(sblr::Opcode::BEGIN_LIST);
            size_t count_pos = bytecode_.size();
            emitU32(0);
            uint32_t count = 0;
            do {
                emit(sblr::Opcode::SORT_KEY);
                parseExpression();
                if (matchKeyword(TokenType::KW_DESC)) {
                    emit(sblr::Opcode::SORT_DESC);
                } else {
                    matchKeyword(TokenType::KW_ASC);
                    emit(sblr::Opcode::SORT_ASC);
                }
                if (matchKeyword(TokenType::KW_NULLS)) {
                    if (matchKeyword(TokenType::KW_FIRST)) {
                        emit(sblr::Opcode::NULLS_FIRST);
                    } else if (matchKeyword(TokenType::KW_LAST)) {
                        emit(sblr::Opcode::NULLS_LAST);
                    }
                }
                count++;
            } while (match(TokenType::COMMA));
            sblr::writeInt32(&bytecode_[count_pos], count);
            emit(sblr::Opcode::END_LIST);
        }

        // Frame clause
        if (matchKeyword(TokenType::KW_ROWS) || matchKeyword(TokenType::KW_RANGE) ||
            matchKeyword(TokenType::KW_GROUPS)) {
            emit(sblr::Opcode::FRAME_CLAUSE);
            // Frame mode already consumed, emit it
            // Parse frame bounds
            if (matchKeyword(TokenType::KW_BETWEEN)) {
                // BETWEEN start AND end
                parseFrameBound();
                consumeKeyword(TokenType::KW_AND, "Expected AND in frame clause");
                parseFrameBound();
            } else {
                // Single bound (start only)
                parseFrameBound();
            }
        }

        consume(TokenType::RIGHT_PAREN, "Expected )");
    } else {
        // Named window reference
        std::string window_name = parseIdentifier();
        emitString(window_name);
    }
}

void Parser::parseFrameBound() {
    if (matchKeyword(TokenType::KW_UNBOUNDED)) {
        if (matchKeyword(TokenType::KW_PRECEDING)) {
            emit(sblr::Opcode::FRAME_UNBOUNDED_PRECEDING);
        } else if (matchKeyword(TokenType::KW_FOLLOWING)) {
            emit(sblr::Opcode::FRAME_UNBOUNDED_FOLLOWING);
        }
    } else if (matchKeyword(TokenType::KW_CURRENT)) {
        consumeKeyword(TokenType::KW_ROW, "Expected ROW after CURRENT");
        emit(sblr::Opcode::FRAME_CURRENT_ROW);
    } else {
        // N PRECEDING or N FOLLOWING
        parseExpression();
        if (matchKeyword(TokenType::KW_PRECEDING)) {
            emit(sblr::Opcode::FRAME_PRECEDING);
        } else if (matchKeyword(TokenType::KW_FOLLOWING)) {
            emit(sblr::Opcode::FRAME_FOLLOWING);
        }
    }
}

void Parser::parseOrderByClause() {
    emit(sblr::Opcode::ORDER_BY);
    emit(sblr::Opcode::BEGIN_LIST);

    size_t count_pos = bytecode_.size();
    emitU32(0);

    uint32_t count = 0;
    do {
        emit(sblr::Opcode::SORT_KEY);
        parseExpression();

        // Direction
        if (matchKeyword(TokenType::KW_DESC)) {
            emit(sblr::Opcode::SORT_DESC);
        } else {
            matchKeyword(TokenType::KW_ASC);  // Optional
            emit(sblr::Opcode::SORT_ASC);
        }

        // NULLS FIRST/LAST
        if (matchKeyword(TokenType::KW_NULLS)) {
            if (matchKeyword(TokenType::KW_FIRST)) {
                emit(sblr::Opcode::NULLS_FIRST);
            } else if (matchKeyword(TokenType::KW_LAST)) {
                emit(sblr::Opcode::NULLS_LAST);
            }
        }

        count++;
    } while (match(TokenType::COMMA));

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);
}

void Parser::parseLimitClause() {
    emit(sblr::Opcode::LIMIT);

    if (matchKeyword(TokenType::KW_ALL)) {
        // LIMIT ALL means no limit
        emit(sblr::Opcode::LITERAL_NULL);
    } else {
        // Parse limit value
        if (check(TokenType::INTEGER_LITERAL)) {
            emit(sblr::Opcode::LITERAL_INT64);
            emitI64(current_token_.value.int_value);
            advance();
        } else {
            parseExpression();
        }
    }
}

void Parser::parseOffsetClause() {
    emit(sblr::Opcode::OFFSET);

    if (check(TokenType::INTEGER_LITERAL)) {
        emit(sblr::Opcode::LITERAL_INT64);
        emitI64(current_token_.value.int_value);
        advance();
    } else {
        parseExpression();
    }

    // Optional ROW/ROWS
    matchKeyword(TokenType::KW_ROW) || matchKeyword(TokenType::KW_ROWS);
}

void Parser::parseFetchClause() {
    // FETCH FIRST/NEXT n ROW(S) ONLY
    matchKeyword(TokenType::KW_FIRST) || matchKeyword(TokenType::KW_NEXT);

    emit(sblr::Opcode::LIMIT);

    if (check(TokenType::INTEGER_LITERAL)) {
        emit(sblr::Opcode::LITERAL_INT64);
        emitI64(current_token_.value.int_value);
        advance();
    } else {
        // Default to 1
        emit(sblr::Opcode::LITERAL_INT64);
        emitI64(1);
    }

    matchKeyword(TokenType::KW_ROW) || matchKeyword(TokenType::KW_ROWS);
    matchKeyword(TokenType::KW_ONLY);
}

void Parser::parseForClause() {
    // FOR UPDATE/SHARE/KEY SHARE/NO KEY UPDATE
    if (matchKeyword(TokenType::KW_UPDATE)) {
        // FOR UPDATE
        emitByte(1);  // Lock type
    } else if (matchKeyword(TokenType::KW_SHARE)) {
        emitByte(2);  // Lock type
    } else if (matchKeyword(TokenType::KW_KEY)) {
        if (matchKeyword(TokenType::KW_SHARE)) {
            emitByte(3);
        } else if (matchKeyword(TokenType::KW_UPDATE)) {
            emitByte(4);
        }
    } else if (matchKeyword(TokenType::KW_NO)) {
        consumeKeyword(TokenType::KW_KEY, "Expected KEY");
        consumeKeyword(TokenType::KW_UPDATE, "Expected UPDATE");
        emitByte(5);
    }

    // OF table_name, ...
    if (matchKeyword(TokenType::KW_OF)) {
        do {
            parseIdentifier();
        } while (match(TokenType::COMMA));
    }

    // NOWAIT or SKIP LOCKED
    if (matchKeyword(TokenType::KW_NOWAIT)) {
        emitByte(1);
    } else if (matchKeyword(TokenType::KW_SKIP)) {
        consumeKeyword(TokenType::KW_LOCKED, "Expected LOCKED");
        emitByte(2);
    }
}

// ============================================================================
// INSERT Statement
// ============================================================================

void Parser::parseInsertStmt() {
    consume(TokenType::KW_INSERT, "Expected INSERT");
    consumeKeyword(TokenType::KW_INTO, "Expected INTO");

    emit(sblr::Opcode::INSERT);

    // Table name
    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);

    emit(sblr::Opcode::TABLE_REF);
    emitString(schema + "/" + table);

    // Optional alias
    std::string alias;
    if (matchKeyword(TokenType::KW_AS)) {
        alias = parseIdentifier();
    }
    emitString(alias);

    // Column list (optional)
    if (match(TokenType::LEFT_PAREN)) {
        emit(sblr::Opcode::BEGIN_LIST);
        size_t count_pos = bytecode_.size();
        emitU32(0);
        uint32_t count = 0;

        do {
            std::string col = parseIdentifier();
            emit(sblr::Opcode::COLUMN_REF);
            emitString(col);
            count++;
        } while (match(TokenType::COMMA));

        sblr::writeInt32(&bytecode_[count_pos], count);
        emit(sblr::Opcode::END_LIST);
        consume(TokenType::RIGHT_PAREN, "Expected )");
    } else {
        emitU32(0);  // No column list
    }

    // VALUES, SELECT, or DEFAULT VALUES
    if (matchKeyword(TokenType::KW_DEFAULT)) {
        consumeKeyword(TokenType::KW_VALUES, "Expected VALUES");
        emitByte(1);  // DEFAULT VALUES flag
    } else if (matchKeyword(TokenType::KW_VALUES)) {
        emitByte(0);  // VALUES clause
        emit(sblr::Opcode::BEGIN_LIST);
        size_t rows_count_pos = bytecode_.size();
        emitU32(0);
        uint32_t rows_count = 0;

        do {
            consume(TokenType::LEFT_PAREN, "Expected (");
            emit(sblr::Opcode::BEGIN_LIST);
            size_t values_count_pos = bytecode_.size();
            emitU32(0);
            uint32_t values_count = 0;

            do {
                if (matchKeyword(TokenType::KW_DEFAULT)) {
                    emit(sblr::Opcode::DEFAULT_VALUE);
                } else {
                    parseExpression();
                }
                values_count++;
            } while (match(TokenType::COMMA));

            sblr::writeInt32(&bytecode_[values_count_pos], values_count);
            emit(sblr::Opcode::END_LIST);
            consume(TokenType::RIGHT_PAREN, "Expected )");
            rows_count++;
        } while (match(TokenType::COMMA));

        sblr::writeInt32(&bytecode_[rows_count_pos], rows_count);
        emit(sblr::Opcode::END_LIST);
    } else if (check(TokenType::KW_SELECT)) {
        emitByte(2);  // SELECT subquery
        parseSelectStmt();
    }

    // ON CONFLICT clause (PostgreSQL upsert)
    if (matchKeyword(TokenType::KW_ON)) {
        consumeKeyword(TokenType::KW_CONFLICT, "Expected CONFLICT");
        parseOnConflictClause();
    }

    // RETURNING clause
    if (matchKeyword(TokenType::KW_RETURNING)) {
        parseReturningClause();
    }
}

void Parser::parseOnConflictClause() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT));

    // Conflict target (optional)
    if (match(TokenType::LEFT_PAREN)) {
        // Column list
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_COLUMN));
        emit(sblr::Opcode::BEGIN_LIST);
        size_t count_pos = bytecode_.size();
        emitU32(0);
        uint32_t count = 0;

        do {
            std::string col = parseIdentifier();
            emit(sblr::Opcode::COLUMN_REF);
            emitString(col);
            count++;
        } while (match(TokenType::COMMA));

        sblr::writeInt32(&bytecode_[count_pos], count);
        emit(sblr::Opcode::END_LIST);
        consume(TokenType::RIGHT_PAREN, "Expected )");

        // Optional WHERE for partial index
        if (matchKeyword(TokenType::KW_WHERE)) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_WHERE));
            parseExpression();
        }
    } else if (matchKeyword(TokenType::KW_ON)) {
        consumeKeyword(TokenType::KW_CONSTRAINT, "Expected CONSTRAINT");
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_CONSTRAINT));
        std::string constraint_name = parseIdentifier();
        emitString(constraint_name);
    }

    // DO action
    consumeKeyword(TokenType::KW_DO, "Expected DO");

    if (matchKeyword(TokenType::KW_NOTHING)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_NOTHING));
    } else if (matchKeyword(TokenType::KW_UPDATE)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_UPDATE));
        consumeKeyword(TokenType::KW_SET, "Expected SET");

        // Parse SET assignments
        emit(sblr::Opcode::BEGIN_LIST);
        size_t count_pos = bytecode_.size();
        emitU32(0);
        uint32_t count = 0;

        do {
            std::string col = parseIdentifier();
            consume(TokenType::EQUAL, "Expected =");
            emit(sblr::Opcode::ASSIGNMENT);
            emitString(col);
            parseExpression();
            count++;
        } while (match(TokenType::COMMA));

        sblr::writeInt32(&bytecode_[count_pos], count);
        emit(sblr::Opcode::END_LIST);

        // WHERE clause for UPDATE
        if (matchKeyword(TokenType::KW_WHERE)) {
            emit(sblr::Opcode::WHERE_CLAUSE);
            parseExpression();
        }
    }
}

void Parser::parseReturningClause() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_RETURNING));

    if (match(TokenType::STAR)) {
        emit(sblr::Opcode::SELECT_STAR);
    } else {
        emit(sblr::Opcode::BEGIN_LIST);
        size_t count_pos = bytecode_.size();
        emitU32(0);
        uint32_t count = 0;

        do {
            parseExpression();
            std::string alias;
            if (matchKeyword(TokenType::KW_AS)) {
                alias = parseIdentifier();
            }
            emitString(alias);
            count++;
        } while (match(TokenType::COMMA));

        sblr::writeInt32(&bytecode_[count_pos], count);
        emit(sblr::Opcode::END_LIST);
    }
}

// ============================================================================
// UPDATE Statement
// ============================================================================

void Parser::parseUpdateStmt() {
    consume(TokenType::KW_UPDATE, "Expected UPDATE");

    emit(sblr::Opcode::UPDATE);

    // Table name
    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);

    emit(sblr::Opcode::TABLE_REF);
    emitString(schema + "/" + table);

    // Optional alias
    std::string alias;
    if (matchKeyword(TokenType::KW_AS)) {
        alias = parseIdentifier();
    } else if (check(TokenType::IDENTIFIER) && !check(TokenType::KW_SET)) {
        alias = parseIdentifier();
    }
    emitString(alias);

    // SET clause
    consumeKeyword(TokenType::KW_SET, "Expected SET");

    emit(sblr::Opcode::BEGIN_LIST);
    size_t count_pos = bytecode_.size();
    emitU32(0);
    uint32_t count = 0;

    do {
        // Column = value or (column_list) = (values)
        if (match(TokenType::LEFT_PAREN)) {
            // Multi-column assignment
            std::vector<std::string> cols;
            do {
                cols.push_back(parseIdentifier());
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
            consume(TokenType::EQUAL, "Expected =");
            consume(TokenType::LEFT_PAREN, "Expected (");

            size_t i = 0;
            do {
                emit(sblr::Opcode::ASSIGNMENT);
                emitString(cols[i++]);
                parseExpression();
                count++;
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        } else {
            std::string col = parseIdentifier();
            consume(TokenType::EQUAL, "Expected =");
            emit(sblr::Opcode::ASSIGNMENT);
            emitString(col);
            parseExpression();
            count++;
        }
    } while (match(TokenType::COMMA));

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);

    // FROM clause (PostgreSQL extension)
    if (matchKeyword(TokenType::KW_FROM)) {
        parseFromClause();
    }

    // WHERE clause
    if (matchKeyword(TokenType::KW_WHERE)) {
        parseWhereClause();
    } else {
        emit(sblr::Opcode::LITERAL_NULL);
    }

    // RETURNING clause
    if (matchKeyword(TokenType::KW_RETURNING)) {
        parseReturningClause();
    }
}

// ============================================================================
// DELETE Statement
// ============================================================================

void Parser::parseDeleteStmt() {
    consume(TokenType::KW_DELETE, "Expected DELETE");
    consumeKeyword(TokenType::KW_FROM, "Expected FROM");

    emit(sblr::Opcode::DELETE);

    // Table name
    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);

    emit(sblr::Opcode::TABLE_REF);
    emitString(schema + "/" + table);

    // Optional alias
    std::string alias;
    if (matchKeyword(TokenType::KW_AS)) {
        alias = parseIdentifier();
    } else if (check(TokenType::IDENTIFIER) && !check(TokenType::KW_WHERE) &&
               !check(TokenType::KW_USING) && !check(TokenType::KW_RETURNING)) {
        alias = parseIdentifier();
    }
    emitString(alias);

    // USING clause (PostgreSQL extension)
    if (matchKeyword(TokenType::KW_USING)) {
        parseFromClause();
    }

    // WHERE clause
    if (matchKeyword(TokenType::KW_WHERE)) {
        parseWhereClause();
    } else {
        emit(sblr::Opcode::LITERAL_NULL);
    }

    // RETURNING clause
    if (matchKeyword(TokenType::KW_RETURNING)) {
        parseReturningClause();
    }
}

// ============================================================================
// MERGE Statement (PostgreSQL 15+)
// ============================================================================

void Parser::parseMergeStmt() {
    consume(TokenType::KW_MERGE, "Expected MERGE");
    consumeKeyword(TokenType::KW_INTO, "Expected INTO");

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_START));

    // Target table
    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);

    emit(sblr::Opcode::TABLE_REF);
    emitString(schema + "/" + table);

    // Optional alias
    std::string alias;
    if (matchKeyword(TokenType::KW_AS)) {
        alias = parseIdentifier();
    }
    emitString(alias);

    // USING clause
    consumeKeyword(TokenType::KW_USING, "Expected USING");
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_SOURCE));

    if (match(TokenType::LEFT_PAREN)) {
        // Subquery
        parseSelectStmt();
        consume(TokenType::RIGHT_PAREN, "Expected )");
    } else {
        // Table reference
        std::string src_schema;
        std::string src_table = parseIdentifier();
        if (match(TokenType::DOT)) {
            src_schema = src_table;
            src_table = parseIdentifier();
        }
        resolveTableName(src_schema, src_table);
        emit(sblr::Opcode::TABLE_REF);
        emitString(src_schema + "/" + src_table);
    }

    // Source alias
    std::string src_alias;
    if (matchKeyword(TokenType::KW_AS)) {
        src_alias = parseIdentifier();
    }
    emitString(src_alias);

    // ON clause
    consumeKeyword(TokenType::KW_ON, "Expected ON");
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_ON));
    parseExpression();

    // WHEN clauses
    while (matchKeyword(TokenType::KW_WHEN)) {
        bool is_matched = false;
        bool is_not = matchKeyword(TokenType::KW_NOT);

        if (matchKeyword(TokenType::KW_MATCHED)) {
            is_matched = !is_not;
        }

        // Optional AND condition
        if (matchKeyword(TokenType::KW_AND)) {
            parseExpression();
        }

        consumeKeyword(TokenType::KW_THEN, "Expected THEN");

        if (is_matched) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_WHEN_MATCHED));

            if (matchKeyword(TokenType::KW_UPDATE)) {
                emitByte(1);  // UPDATE action
                consumeKeyword(TokenType::KW_SET, "Expected SET");

                emit(sblr::Opcode::BEGIN_LIST);
                size_t count_pos = bytecode_.size();
                emitU32(0);
                uint32_t count = 0;

                do {
                    std::string col = parseIdentifier();
                    consume(TokenType::EQUAL, "Expected =");
                    emit(sblr::Opcode::ASSIGNMENT);
                    emitString(col);
                    parseExpression();
                    count++;
                } while (match(TokenType::COMMA));

                sblr::writeInt32(&bytecode_[count_pos], count);
                emit(sblr::Opcode::END_LIST);
            } else if (matchKeyword(TokenType::KW_DELETE)) {
                emitByte(2);  // DELETE action
            }
        } else {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_WHEN_NOT_MATCHED));

            if (matchKeyword(TokenType::KW_INSERT)) {
                emitByte(1);  // INSERT action

                // Column list (optional)
                if (match(TokenType::LEFT_PAREN)) {
                    emit(sblr::Opcode::BEGIN_LIST);
                    size_t count_pos = bytecode_.size();
                    emitU32(0);
                    uint32_t count = 0;

                    do {
                        std::string col = parseIdentifier();
                        emit(sblr::Opcode::COLUMN_REF);
                        emitString(col);
                        count++;
                    } while (match(TokenType::COMMA));

                    sblr::writeInt32(&bytecode_[count_pos], count);
                    emit(sblr::Opcode::END_LIST);
                    consume(TokenType::RIGHT_PAREN, "Expected )");
                }

                // VALUES
                consumeKeyword(TokenType::KW_VALUES, "Expected VALUES");
                consume(TokenType::LEFT_PAREN, "Expected (");

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
            }
        }
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_END));
}

// ============================================================================
// WITH Clause (Common Table Expressions)
// ============================================================================

void Parser::parseWithClause() {
    consume(TokenType::KW_WITH, "Expected WITH");

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_WITH_CLAUSE));

    // RECURSIVE keyword
    bool is_recursive = matchKeyword(TokenType::KW_RECURSIVE);
    emitByte(is_recursive ? 1 : 0);

    // Parse CTE definitions
    emit(sblr::Opcode::BEGIN_LIST);
    size_t count_pos = bytecode_.size();
    emitU32(0);
    uint32_t count = 0;

    do {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CTE_DEF));

        std::string cte_name = parseIdentifier();
        emitString(cte_name);

        // Optional column list
        if (match(TokenType::LEFT_PAREN)) {
            emit(sblr::Opcode::BEGIN_LIST);
            size_t col_count_pos = bytecode_.size();
            emitU32(0);
            uint32_t col_count = 0;

            do {
                std::string col = parseIdentifier();
                emitString(col);
                col_count++;
            } while (match(TokenType::COMMA));

            sblr::writeInt32(&bytecode_[col_count_pos], col_count);
            emit(sblr::Opcode::END_LIST);
            consume(TokenType::RIGHT_PAREN, "Expected )");
        } else {
            emitU32(0);  // No column list
        }

        consumeKeyword(TokenType::KW_AS, "Expected AS");

        // MATERIALIZED / NOT MATERIALIZED (PostgreSQL 12+)
        if (matchKeyword(TokenType::KW_MATERIALIZED)) {
            emitByte(1);
        } else if (matchKeyword(TokenType::KW_NOT)) {
            consumeKeyword(TokenType::KW_MATERIALIZED, "Expected MATERIALIZED");
            emitByte(2);
        } else {
            emitByte(0);  // Default
        }

        consume(TokenType::LEFT_PAREN, "Expected (");
        parseSelectStmt();  // CTE query
        consume(TokenType::RIGHT_PAREN, "Expected )");

        count++;
    } while (match(TokenType::COMMA));

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);
}

// Helper for frame bounds - moved earlier in the file

} // namespace scratchbird::parser::postgresql
