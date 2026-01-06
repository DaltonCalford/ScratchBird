/**
 * PostgreSQL Parser - DML Statement Parsing
 *
 * Handles SELECT, INSERT, UPDATE, DELETE, MERGE statements.
 */

#include "scratchbird/parser/postgresql/pg_parser.h"
#include "scratchbird/core/catalog_manager.h"
#include <algorithm>

namespace scratchbird::parser::postgresql {

// ============================================================================
// SELECT Statement
// ============================================================================

void Parser::parseSelectStmt() {
    consume(TokenType::KW_SELECT, "Expected SELECT");
    emit(sblr::Opcode::SELECT);

    // Handle SELECT modifiers
    if (matchKeyword(TokenType::KW_DISTINCT)) {
        if (matchKeyword(TokenType::KW_ON)) {
            consume(TokenType::LEFT_PAREN, "Expected (");
            bool prev_emit = emit_enabled_;
            emit_enabled_ = false;
            do {
                parseExpression();
            } while (match(TokenType::COMMA));
            emit_enabled_ = prev_emit;
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
    } else {
        matchKeyword(TokenType::KW_ALL);  // Optional, default
    }

    std::vector<SelectItem> items;
    parseSelectList(items);

    bool has_from = false;
    std::string table_path;

    if (matchKeyword(TokenType::KW_FROM)) {
        if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
            has_from = true;
            std::string schema;
            std::string table = parseIdentifier();
            if (match(TokenType::DOT)) {
                schema = table;
                table = parseIdentifier();
            }
            resolveTableName(schema, table);
            table_path = schema.empty() ? table : schema + "/" + table;

            if (matchKeyword(TokenType::KW_AS)) {
                parseIdentifier();
            } else if ((check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) &&
                       !check(TokenType::KW_WHERE) && !check(TokenType::KW_GROUP) &&
                       !check(TokenType::KW_HAVING) && !check(TokenType::KW_ORDER) &&
                       !check(TokenType::KW_LIMIT) && !check(TokenType::KW_JOIN) &&
                       !check(TokenType::KW_LEFT) && !check(TokenType::KW_RIGHT) &&
                       !check(TokenType::KW_INNER) && !check(TokenType::KW_CROSS) &&
                       !check(TokenType::KW_FULL) && !check(TokenType::KW_NATURAL) &&
                       !check(TokenType::KW_OFFSET)) {
                parseIdentifier();
            }

            auto is_join_token = [](TokenType type) {
                return type == TokenType::KW_JOIN || type == TokenType::KW_LEFT ||
                       type == TokenType::KW_RIGHT || type == TokenType::KW_INNER ||
                       type == TokenType::KW_CROSS || type == TokenType::KW_NATURAL ||
                       type == TokenType::KW_FULL;
            };
            auto is_from_terminator = [](TokenType type) {
                return type == TokenType::KW_WHERE || type == TokenType::KW_GROUP ||
                       type == TokenType::KW_HAVING || type == TokenType::KW_ORDER ||
                       type == TokenType::KW_LIMIT || type == TokenType::KW_OFFSET ||
                       type == TokenType::KW_WINDOW || type == TokenType::KW_FETCH ||
                       type == TokenType::KW_FOR || type == TokenType::KW_UNION ||
                       type == TokenType::KW_EXCEPT || type == TokenType::KW_INTERSECT;
            };

            if (check(TokenType::COMMA) || is_join_token(current_token_.type)) {
                int depth = 0;
                while (!check(TokenType::END_OF_FILE) && !check(TokenType::SEMICOLON)) {
                    if (depth == 0 && is_from_terminator(current_token_.type)) {
                        break;
                    }
                    if (match(TokenType::LEFT_PAREN)) {
                        depth++;
                        continue;
                    }
                    if (match(TokenType::RIGHT_PAREN)) {
                        if (depth > 0) {
                            depth--;
                        }
                        continue;
                    }
                    advance();
                }
            }
        } else {
            bool prev_emit = emit_enabled_;
            emit_enabled_ = false;
            parseFromClause();
            emit_enabled_ = prev_emit;
        }
    }

    if (emit_enabled_) {
        emit(sblr::Opcode::BEGIN_LIST);
        size_t count_pos = bytecode_.size();
        emitU32(0);

        uint32_t emit_count = 0;

        if (has_from) {
            bool has_star = false;
            std::vector<const SelectItem*> columns;
            for (const auto& item : items) {
                if (item.kind == SelectItem::Kind::Star) {
                    has_star = true;
                } else if (item.kind == SelectItem::Kind::Column) {
                    columns.push_back(&item);
                }
            }

            if (has_star || columns.empty()) {
                emit(sblr::Opcode::SELECT_STAR);
                emit_count = 1;
            } else {
                for (const auto* item : columns) {
                    emit(sblr::Opcode::COLUMN_REF);
                    emitString(item->column_name);
                    if (!item->alias.empty() && item->alias != item->column_name) {
                        emit(sblr::Opcode::COLUMN_REF);
                        emitString("");
                        emitString(item->alias);
                    }
                    emit_count++;
                }
            }
        } else {
            for (const auto& item : items) {
                if (item.kind == SelectItem::Kind::Star) {
                    continue;
                }
                bytecode_.insert(bytecode_.end(),
                                 item.expr_bytecode.begin(),
                                 item.expr_bytecode.end());
                if (!item.alias.empty()) {
                    emit(sblr::Opcode::COLUMN_REF);
                    emitString(item.alias);
                }
                emit_count++;
            }

            if (emit_count == 0) {
                emit(sblr::Opcode::LITERAL_NULL);
                emit_count = 1;
            }
        }

        sblr::writeInt32(&bytecode_[count_pos], emit_count);
        emit(sblr::Opcode::END_LIST);

        emit(sblr::Opcode::TABLE_REF);
        emitString(has_from ? table_path : "");
    }

    if (matchKeyword(TokenType::KW_WHERE)) {
        parseWhereClause();
    }

    if (matchKeyword(TokenType::KW_GROUP)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after GROUP");
        parseGroupByClause();
    }

    if (matchKeyword(TokenType::KW_HAVING)) {
        parseHavingClause();
    }

    if (matchKeyword(TokenType::KW_WINDOW)) {
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        parseWindowClause();
        emit_enabled_ = prev_emit;
    }

    if (matchKeyword(TokenType::KW_ORDER)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after ORDER");
        parseOrderByClause();
    }

    bool limit_parsed = false;
    bool offset_parsed = false;
    bool progress = true;
    while (progress) {
        progress = false;
        if (!limit_parsed && matchKeyword(TokenType::KW_LIMIT)) {
            parseLimitClause();
            limit_parsed = true;
            progress = true;
        }
        if (!offset_parsed && matchKeyword(TokenType::KW_OFFSET)) {
            parseOffsetClause();
            offset_parsed = true;
            progress = true;
        }
    }

    if (matchKeyword(TokenType::KW_FETCH)) {
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        parseFetchClause();
        emit_enabled_ = prev_emit;
    }

    if (matchKeyword(TokenType::KW_FOR)) {
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        parseForClause();
        emit_enabled_ = prev_emit;
    }
}

void Parser::parseSelectList(std::vector<SelectItem>& items) {
    auto decode_simple_column = [](const std::vector<uint8_t>& expr_bytes, std::string& column_out) {
        if (expr_bytes.empty()) {
            return false;
        }
        size_t pc = 0;
        if (expr_bytes[pc++] != static_cast<uint8_t>(sblr::Opcode::COLUMN_REF)) {
            return false;
        }
        if (pc + 4 > expr_bytes.size()) {
            return false;
        }
        uint32_t len = sblr::readInt32(&expr_bytes[pc]);
        pc += 4;
        if (pc + len != expr_bytes.size()) {
            return false;
        }
        column_out.assign(reinterpret_cast<const char*>(expr_bytes.data() + pc), len);
        return true;
    };

    if (match(TokenType::STAR)) {
        SelectItem item;
        item.kind = SelectItem::Kind::Star;
        items.push_back(std::move(item));
        return;
    }

    do {
        SelectItem item;
        if (match(TokenType::STAR)) {
            item.kind = SelectItem::Kind::Star;
        } else {
            item.expr_bytecode = captureExpressionBytecode();
            std::string column_name;
            if (decode_simple_column(item.expr_bytecode, column_name)) {
                item.kind = SelectItem::Kind::Column;
                item.column_name = column_name;
            } else {
                item.kind = SelectItem::Kind::Expression;
            }
        }

        if (matchKeyword(TokenType::KW_AS)) {
            item.alias = parseIdentifier();
        } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
            item.alias = parseIdentifier();
        }

        items.push_back(std::move(item));
    } while (match(TokenType::COMMA));
}

void Parser::parseFromClause() {
    bool emit_now = emit_enabled_;
    size_t count_pos = 0;

    if (emit_now) {
        emit(sblr::Opcode::BEGIN_LIST);
        count_pos = bytecode_.size();
        emitU32(0);  // Placeholder
    }

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

    if (emit_now) {
        sblr::writeInt32(&bytecode_[count_pos], count);
        emit(sblr::Opcode::END_LIST);
    }
}

void Parser::parseJoinClause() {
    bool emit_now = emit_enabled_;

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
                size_t using_count_pos = 0;
                if (emit_now) {
                    emit(sblr::Opcode::BEGIN_LIST);
                    using_count_pos = bytecode_.size();
                    emitU32(0);
                }
                uint32_t using_count = 0;

                do {
                    std::string col = parseIdentifier();
                    if (emit_now) {
                        emit(sblr::Opcode::COLUMN_REF);
                        emitString(col);
                    }
                    using_count++;
                } while (match(TokenType::COMMA));

                if (emit_now) {
                    sblr::writeInt32(&bytecode_[using_count_pos], using_count);
                    emit(sblr::Opcode::END_LIST);
                }
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
    if (!emit_enabled_) {
        do {
            parseExpression();
        } while (match(TokenType::COMMA));
        return;
    }

    if (check(TokenType::KW_ROLLUP) || check(TokenType::KW_CUBE) ||
        check(TokenType::KW_GROUPING)) {
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        if (matchKeyword(TokenType::KW_ROLLUP) || matchKeyword(TokenType::KW_CUBE)) {
            consume(TokenType::LEFT_PAREN, "Expected (");
            do {
                parseExpression();
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        } else if (matchKeyword(TokenType::KW_GROUPING)) {
            matchKeyword(TokenType::KW_SETS);
            consume(TokenType::LEFT_PAREN, "Expected (");
            do {
                if (match(TokenType::LEFT_PAREN)) {
                    do {
                        parseExpression();
                    } while (match(TokenType::COMMA));
                    consume(TokenType::RIGHT_PAREN, "Expected )");
                } else {
                    parseExpression();
                }
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
        emit_enabled_ = prev_emit;
        return;
    }

    emit(sblr::Opcode::GROUP_BY);
    size_t count_pos = bytecode_.size();
    emitU32(0);

    uint32_t count = 0;
    do {
        parseExpression();
        count++;
    } while (match(TokenType::COMMA));

    sblr::writeInt32(&bytecode_[count_pos], count);
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
    struct SortKey {
        std::string column;
        bool descending = false;
        bool nulls_first = false;
        bool nulls_specified = false;
    };

    std::vector<SortKey> keys;

    do {
        SortKey key;
        std::vector<uint8_t> expr = captureExpressionBytecode();
        std::string column_name;

        if (!expr.empty() &&
            expr[0] == static_cast<uint8_t>(sblr::Opcode::COLUMN_REF) &&
            expr.size() >= 5) {
            uint32_t len = sblr::readInt32(&expr[1]);
            if (1 + 4 + len == expr.size()) {
                column_name.assign(reinterpret_cast<const char*>(expr.data() + 5), len);
                key.column = column_name;
            }
        }

        if (matchKeyword(TokenType::KW_DESC)) {
            key.descending = true;
        } else {
            matchKeyword(TokenType::KW_ASC);
        }

        if (matchKeyword(TokenType::KW_NULLS)) {
            key.nulls_specified = true;
            if (matchKeyword(TokenType::KW_FIRST)) {
                key.nulls_first = true;
            } else if (matchKeyword(TokenType::KW_LAST)) {
                key.nulls_first = false;
            }
        }

        if (!key.column.empty()) {
            keys.push_back(key);
        }
    } while (match(TokenType::COMMA));

    if (keys.empty()) {
        return;
    }

    emit(sblr::Opcode::ORDER_BY);
    emitU32(static_cast<uint32_t>(keys.size()));

    for (const auto& key : keys) {
        emit(sblr::Opcode::SORT_KEY);
        emit(sblr::Opcode::COLUMN_REF);
        emitString(key.column);
        emit(key.descending ? sblr::Opcode::SORT_DESC : sblr::Opcode::SORT_ASC);
        if (key.nulls_specified) {
            emit(key.nulls_first ? sblr::Opcode::NULLS_FIRST : sblr::Opcode::NULLS_LAST);
        }
    }
}

void Parser::parseLimitClause() {
    if (matchKeyword(TokenType::KW_ALL)) {
        return;
    }

    if (check(TokenType::INTEGER_LITERAL)) {
        emit(sblr::Opcode::LIMIT);
        emitI64(current_token_.value.int_value);
        advance();
        return;
    }

    bool prev_emit = emit_enabled_;
    emit_enabled_ = false;
    parseExpression();
    emit_enabled_ = prev_emit;
}

void Parser::parseOffsetClause() {
    if (check(TokenType::INTEGER_LITERAL)) {
        emit(sblr::Opcode::OFFSET);
        emitI64(current_token_.value.int_value);
        advance();
    } else {
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        parseExpression();
        emit_enabled_ = prev_emit;
    }

    // Optional ROW/ROWS
    matchKeyword(TokenType::KW_ROW) || matchKeyword(TokenType::KW_ROWS);
}

void Parser::parseFetchClause() {
    // FETCH FIRST/NEXT n ROW(S) ONLY
    matchKeyword(TokenType::KW_FIRST) || matchKeyword(TokenType::KW_NEXT);

    if (check(TokenType::INTEGER_LITERAL)) {
        emit(sblr::Opcode::LIMIT);
        emitI64(current_token_.value.int_value);
        advance();
    } else {
        emit(sblr::Opcode::LIMIT);
        emitI64(1);
    }

    matchKeyword(TokenType::KW_ROW) || matchKeyword(TokenType::KW_ROWS);
    matchKeyword(TokenType::KW_ONLY);
}

std::vector<uint8_t> Parser::captureExpressionBytecode() {
    std::vector<uint8_t> saved;
    saved.swap(bytecode_);

    bool saved_emit = emit_enabled_;
    emit_enabled_ = true;

    bytecode_.clear();
    parseExpression();

    std::vector<uint8_t> expr;
    expr.swap(bytecode_);

    bytecode_.swap(saved);
    emit_enabled_ = saved_emit;
    return expr;
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

    std::string table_path = schema.empty() ? table : schema + "/" + table;
    emit(sblr::Opcode::TABLE_REF);
    emitString(table_path);

    if (matchKeyword(TokenType::KW_AS)) {
        parseIdentifier();
    }

    std::vector<std::string> columns;
    bool has_column_list = false;
    if (match(TokenType::LEFT_PAREN)) {
        has_column_list = true;
        do {
            columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    if (!has_column_list) {
        auto split_components = [](const std::string& path) {
            std::vector<std::string> parts;
            std::string current;
            for (char ch : path) {
                if (ch == '/' || ch == '.') {
                    if (!current.empty()) {
                        parts.push_back(current);
                        current.clear();
                    }
                } else {
                    current.push_back(ch);
                }
            }
            if (!current.empty()) {
                parts.push_back(current);
            }
            return parts;
        };

        if (db_) {
            core::ObjectPath path;
            path.components = split_components(schema);
            path.components.push_back(table);
            path.type = schema.empty() ? core::PathType::UNQUALIFIED : core::PathType::ABSOLUTE;

            core::CatalogManager::ResolveOptions opts;
            core::CatalogManager::ObjectType resolved_type;
            core::ID table_id;
            core::ErrorContext ctx;
            if (db_->catalog_manager()->resolveObjectPath(path,
                                                         core::CatalogManager::ObjectType::TABLE,
                                                         opts, table_id, resolved_type, &ctx) == core::Status::OK) {
                std::vector<core::CatalogManager::ColumnInfo> cols;
                if (db_->catalog_manager()->getColumns(table_id, cols, &ctx) == core::Status::OK) {
                    for (const auto& col : cols) {
                        columns.push_back(col.column_name);
                    }
                }
            }
        }
    }

    struct InsertValue {
        bool is_default = false;
        std::vector<uint8_t> expr;
    };
    std::vector<InsertValue> values;

    if (matchKeyword(TokenType::KW_DEFAULT)) {
        matchKeyword(TokenType::KW_VALUES);
    } else if (matchKeyword(TokenType::KW_VALUES)) {
        consume(TokenType::LEFT_PAREN, "Expected (");
        do {
            InsertValue val;
            if (matchKeyword(TokenType::KW_DEFAULT)) {
                val.is_default = true;
            } else {
                val.expr = captureExpressionBytecode();
            }
            values.push_back(std::move(val));
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");

        while (match(TokenType::COMMA)) {
            consume(TokenType::LEFT_PAREN, "Expected (");
            int depth = 1;
            while (depth > 0 && !check(TokenType::END_OF_FILE)) {
                if (match(TokenType::LEFT_PAREN)) {
                    depth++;
                } else if (match(TokenType::RIGHT_PAREN)) {
                    depth--;
                } else {
                    advance();
                }
            }
        }
    } else if (check(TokenType::KW_SELECT)) {
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        parseSelectStmt();
        emit_enabled_ = prev_emit;
    }

    std::vector<std::string> emit_columns;
    std::vector<std::vector<uint8_t>> emit_values;

    if (!columns.empty() && !values.empty()) {
        size_t pair_count = std::min(columns.size(), values.size());
        for (size_t i = 0; i < pair_count; ++i) {
            if (values[i].is_default) {
                continue;
            }
            emit_columns.push_back(columns[i]);
            emit_values.push_back(values[i].expr);
        }
    }

    emit(sblr::Opcode::BEGIN_LIST);
    emitU32(static_cast<uint32_t>(emit_columns.size()));
    for (const auto& col : emit_columns) {
        emit(sblr::Opcode::COLUMN_REF);
        emitString(col);
    }
    emit(sblr::Opcode::END_LIST);

    emit(sblr::Opcode::BEGIN_LIST);
    emitU32(static_cast<uint32_t>(emit_values.size()));
    for (const auto& expr : emit_values) {
        if (emit_enabled_) {
            bytecode_.insert(bytecode_.end(), expr.begin(), expr.end());
        }
    }
    emit(sblr::Opcode::END_LIST);

    if (matchKeyword(TokenType::KW_ON)) {
        consumeKeyword(TokenType::KW_CONFLICT, "Expected CONFLICT");
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        parseOnConflictClause();
        emit_enabled_ = prev_emit;
    }

    if (matchKeyword(TokenType::KW_RETURNING)) {
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        parseReturningClause();
        emit_enabled_ = prev_emit;
    }
}

void Parser::parseOnConflictClause() {
    bool emit_now = emit_enabled_;

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT));

    // Conflict target (optional)
    if (match(TokenType::LEFT_PAREN)) {
        // Column list
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_COLUMN));
        size_t count_pos = 0;
        if (emit_now) {
            emit(sblr::Opcode::BEGIN_LIST);
            count_pos = bytecode_.size();
            emitU32(0);
        }
        uint32_t count = 0;

        do {
            std::string col = parseIdentifier();
            if (emit_now) {
                emit(sblr::Opcode::COLUMN_REF);
                emitString(col);
            }
            count++;
        } while (match(TokenType::COMMA));

        if (emit_now) {
            sblr::writeInt32(&bytecode_[count_pos], count);
            emit(sblr::Opcode::END_LIST);
        }
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
        size_t count_pos = 0;
        if (emit_now) {
            emit(sblr::Opcode::BEGIN_LIST);
            count_pos = bytecode_.size();
            emitU32(0);
        }
        uint32_t count = 0;

        do {
            std::string col = parseIdentifier();
            consume(TokenType::EQUAL, "Expected =");
            if (emit_now) {
                emit(sblr::Opcode::ASSIGNMENT);
                emitString(col);
            }
            parseExpression();
            count++;
        } while (match(TokenType::COMMA));

        if (emit_now) {
            sblr::writeInt32(&bytecode_[count_pos], count);
            emit(sblr::Opcode::END_LIST);
        }

        // WHERE clause for UPDATE
        if (matchKeyword(TokenType::KW_WHERE)) {
            emit(sblr::Opcode::WHERE_CLAUSE);
            parseExpression();
        }
    }
}

void Parser::parseReturningClause() {
    std::vector<std::string> columns;
    bool can_emit = true;

    auto decode_simple_column = [](const std::vector<uint8_t>& expr_bytes, std::string& column_out) {
        if (expr_bytes.empty()) {
            return false;
        }
        size_t pc = 0;
        if (expr_bytes[pc++] != static_cast<uint8_t>(sblr::Opcode::COLUMN_REF)) {
            return false;
        }
        if (pc + 4 > expr_bytes.size()) {
            return false;
        }
        uint32_t len = sblr::readInt32(&expr_bytes[pc]);
        pc += 4;
        if (pc + len != expr_bytes.size()) {
            return false;
        }
        column_out.assign(reinterpret_cast<const char*>(expr_bytes.data() + pc), len);
        return true;
    };

    if (match(TokenType::STAR)) {
        can_emit = false;
    } else {
        do {
            std::vector<uint8_t> expr = captureExpressionBytecode();
            std::string col;
            if (decode_simple_column(expr, col)) {
                columns.push_back(col);
            } else {
                can_emit = false;
            }
            if (matchKeyword(TokenType::KW_AS)) {
                parseIdentifier();
            } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
                parseIdentifier();
            }
        } while (match(TokenType::COMMA));
    }

    if (!emit_enabled_ || !can_emit || columns.empty()) {
        return;
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_RETURNING));
    emitU32(static_cast<uint32_t>(columns.size()));
    for (const auto& col : columns) {
        emitString(col);
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
    emitString(schema.empty() ? table : schema + "/" + table);

    if (matchKeyword(TokenType::KW_AS)) {
        parseIdentifier();
    } else if (check(TokenType::IDENTIFIER) && !check(TokenType::KW_SET)) {
        parseIdentifier();
    }

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
                emit(sblr::Opcode::COLUMN_REF);
                emitString(cols[i++]);
                parseExpression();
                count++;
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        } else {
            std::string col = parseIdentifier();
            consume(TokenType::EQUAL, "Expected =");
            emit(sblr::Opcode::ASSIGNMENT);
            emit(sblr::Opcode::COLUMN_REF);
            emitString(col);
            parseExpression();
            count++;
        }
    } while (match(TokenType::COMMA));

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);

    // FROM clause (PostgreSQL extension)
    if (matchKeyword(TokenType::KW_FROM)) {
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        parseFromClause();
        emit_enabled_ = prev_emit;
    }

    // WHERE clause
    if (matchKeyword(TokenType::KW_WHERE)) {
        parseWhereClause();
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
    emitString(schema.empty() ? table : schema + "/" + table);

    if (matchKeyword(TokenType::KW_AS)) {
        parseIdentifier();
    } else if (check(TokenType::IDENTIFIER) && !check(TokenType::KW_WHERE) &&
               !check(TokenType::KW_USING) && !check(TokenType::KW_RETURNING)) {
        parseIdentifier();
    }

    // USING clause (PostgreSQL extension)
    if (matchKeyword(TokenType::KW_USING)) {
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        parseFromClause();
        emit_enabled_ = prev_emit;
    }

    // WHERE clause
    if (matchKeyword(TokenType::KW_WHERE)) {
        parseWhereClause();
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
