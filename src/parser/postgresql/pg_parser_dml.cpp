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
    bool distinct = false;
    std::vector<std::vector<uint8_t>> distinct_on_exprs;
    auto capture_expr = [&]() {
        std::vector<uint8_t> saved;
        saved.swap(bytecode_);
        bool prev_emit = emit_enabled_;
        emit_enabled_ = true;
        bytecode_.clear();
        parseExpression();
        std::vector<uint8_t> expr;
        expr.swap(bytecode_);
        bytecode_.swap(saved);
        emit_enabled_ = prev_emit;
        return expr;
    };
    if (matchKeyword(TokenType::KW_DISTINCT)) {
        distinct = true;
        if (matchKeyword(TokenType::KW_ON)) {
            consume(TokenType::LEFT_PAREN, "Expected (");
            do {
                distinct_on_exprs.push_back(capture_expr());
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
    } else {
        matchKeyword(TokenType::KW_ALL);  // Optional, default
    }
    emitByte(distinct ? 0x01 : 0x00);

    std::vector<SelectItem> items;
    parseSelectList(items);

    bool has_from = false;
    std::vector<uint8_t> from_bytecode;

    if (matchKeyword(TokenType::KW_FROM)) {
        has_from = true;
        std::vector<uint8_t> saved;
        saved.swap(bytecode_);
        bool prev_emit = emit_enabled_;
        emit_enabled_ = true;
        bytecode_.clear();
        parseFromClause();
        from_bytecode.swap(bytecode_);
        bytecode_.swap(saved);
        emit_enabled_ = prev_emit;
    }

    if (emit_enabled_) {
        uint64_t emit_count = 0;
        for (const auto& item : items) {
            if (item.kind == SelectItem::Kind::Star ||
                item.kind == SelectItem::Kind::Column ||
                item.kind == SelectItem::Kind::Expression) {
                emit_count++;
            }
        }
        bool emit_fallback_null = false;
        if (emit_count == 0) {
            emit_count = 1;
            emit_fallback_null = true;
        }

        emit(sblr::Opcode::BEGIN_LIST);
        emitUVarint(emit_count);

        if (emit_fallback_null) {
            emit(sblr::Opcode::LITERAL_NULL);
            emitString("");
        } else {
            for (const auto& item : items) {
                if (item.kind == SelectItem::Kind::Star) {
                    emit(sblr::Opcode::SELECT_STAR);
                    continue;
                }
                bytecode_.insert(bytecode_.end(),
                                 item.expr_bytecode.begin(),
                                 item.expr_bytecode.end());
                emitString(item.alias);
            }
        }

        emit(sblr::Opcode::END_LIST);

        if (has_from) {
            if (emit_enabled_) {
                bytecode_.insert(bytecode_.end(),
                                 from_bytecode.begin(),
                                 from_bytecode.end());
            }
        } else {
            emit(sblr::Opcode::BEGIN_LIST);
            emitUVarint(0);
            emit(sblr::Opcode::END_LIST);
        }

        if (!distinct_on_exprs.empty()) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DISTINCT_ON));
            emit(sblr::Opcode::BEGIN_LIST);
            emitUVarint(distinct_on_exprs.size());
            for (const auto& expr : distinct_on_exprs) {
                emitU32(static_cast<uint32_t>(expr.size()));
                bytecode_.insert(bytecode_.end(), expr.begin(), expr.end());
            }
            emit(sblr::Opcode::END_LIST);
        }
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
        uint64_t len = 0;
        size_t bytes_read = 0;
        if (!sblr::readUVarint(expr_bytes.data() + pc, expr_bytes.size() - pc, len, bytes_read)) {
            return false;
        }
        pc += bytes_read;
        if (pc + len != expr_bytes.size()) {
            return false;
        }
        column_out.assign(reinterpret_cast<const char*>(expr_bytes.data() + pc),
                          static_cast<size_t>(len));
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
    std::vector<uint8_t> join_bytecode;

    if (emit_now) {
        emit(sblr::Opcode::BEGIN_LIST);
        count_pos = bytecode_.size();
        emitUVarint(0);  // Placeholder
    }

    uint32_t count = 0;
    auto patch_varint = [&](size_t pos, uint64_t value) {
        uint8_t buffer[10];
        size_t len = sblr::writeUVarint(buffer, value);
        if (len == 1) {
            bytecode_[pos] = buffer[0];
            return;
        }
        bytecode_.insert(bytecode_.begin() + pos + 1, len - 1, 0);
        std::copy(buffer, buffer + len, bytecode_.begin() + pos);
    };

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
            emitByte(0);  // name-based reference
            emitString(schema.empty() ? table : (schema + "/" + table));
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
        if (emit_now) {
            std::vector<uint8_t> saved;
            saved.swap(bytecode_);
            bool prev_emit = emit_enabled_;
            emit_enabled_ = true;
            bytecode_.clear();
            parseJoinClause();
            join_bytecode.insert(join_bytecode.end(),
                                 bytecode_.begin(),
                                 bytecode_.end());
            bytecode_.swap(saved);
            emit_enabled_ = prev_emit;
        } else {
            parseJoinClause();
        }

    } while (match(TokenType::COMMA));

    if (emit_now) {
        patch_varint(count_pos, count);
        emit(sblr::Opcode::END_LIST);
        if (!join_bytecode.empty()) {
            bytecode_.insert(bytecode_.end(),
                             join_bytecode.begin(),
                             join_bytecode.end());
        }
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
        emitByte(0);  // name-based reference
        emitString(j_schema.empty() ? j_table : (j_schema + "/" + j_table));

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
                error("JOIN USING is not supported in PostgreSQL emulation");
                consume(TokenType::LEFT_PAREN, "Expected ( after USING");
                bool prev_emit = emit_enabled_;
                emit_enabled_ = false;
                do {
                    parseIdentifier();
                } while (match(TokenType::COMMA));
                emit_enabled_ = prev_emit;
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

    std::vector<std::vector<uint8_t>> expressions;
    do {
        expressions.push_back(captureExpressionBytecode());
    } while (match(TokenType::COMMA));
    emit(sblr::Opcode::GROUP_BY);
    emitUVarint(expressions.size());
    for (const auto& expr : expressions) {
        if (emit_enabled_) {
            bytecode_.insert(bytecode_.end(), expr.begin(), expr.end());
        }
    }
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
        std::vector<uint8_t> expr;
        bool descending = false;
        bool nulls_first = false;
        bool nulls_specified = false;
    };

    std::vector<SortKey> keys;

    do {
        SortKey key;
        key.expr = captureExpressionBytecode();

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

        keys.push_back(std::move(key));
    } while (match(TokenType::COMMA));

    if (keys.empty()) {
        return;
    }

    emit(sblr::Opcode::ORDER_BY);
    emitUVarint(keys.size());

    for (const auto& key : keys) {
        emit(sblr::Opcode::SORT_KEY);
        if (emit_enabled_) {
            bytecode_.insert(bytecode_.end(), key.expr.begin(), key.expr.end());
        }
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
    } else if (matchIdentifierKeyword("SHARE")) {
        emitByte(2);  // Lock type
    } else if (matchKeyword(TokenType::KW_KEY)) {
        if (matchIdentifierKeyword("SHARE")) {
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
    std::string table_alias;
    if (matchKeyword(TokenType::KW_AS)) {
        table_alias = parseIdentifier();
    }

    emit(sblr::Opcode::TABLE_REF);
    emitByte(0);  // name-based reference
    emitString(table_path);
    emitString(table_alias);

    std::vector<std::string> columns;
    bool has_column_list = false;
    if (match(TokenType::LEFT_PAREN)) {
        has_column_list = true;
        do {
            columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    struct InsertValue {
        bool is_default = false;
        std::vector<uint8_t> expr;
    };
    std::vector<std::vector<InsertValue>> rows;
    bool default_values_only = false;
    bool has_select = false;
    std::vector<uint8_t> select_bytecode;

    if (matchKeyword(TokenType::KW_DEFAULT)) {
        matchKeyword(TokenType::KW_VALUES);
        default_values_only = true;
    } else if (matchKeyword(TokenType::KW_VALUES)) {
        do {
            consume(TokenType::LEFT_PAREN, "Expected (");
            std::vector<InsertValue> row;
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    InsertValue val;
                    if (matchKeyword(TokenType::KW_DEFAULT)) {
                        val.is_default = true;
                    } else {
                        val.expr = captureExpressionBytecode();
                    }
                    row.push_back(std::move(val));
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RIGHT_PAREN, "Expected )");
            rows.push_back(std::move(row));
        } while (match(TokenType::COMMA));
    } else if (check(TokenType::KW_SELECT)) {
        auto capture_select = [&]() {
            std::vector<uint8_t> saved;
            saved.swap(bytecode_);
            bool saved_emit = emit_enabled_;
            emit_enabled_ = true;
            bytecode_.clear();
            parseSelectStmt();
            std::vector<uint8_t> stmt;
            stmt.swap(bytecode_);
            bytecode_.swap(saved);
            emit_enabled_ = saved_emit;
            return stmt;
        };
        has_select = true;
        select_bytecode = capture_select();
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

        if (db_ && !rows.empty()) {
            core::ObjectPath path;
            path.components = split_components(schema);
            path.components.push_back(table);
            path.type = schema.empty() ? core::PathType::UNQUALIFIED : core::PathType::ABSOLUTE;

            core::CatalogManager::ResolveOptions opts;
            opts.required_privilege =
                static_cast<uint32_t>(core::CatalogManager::Privilege::INSERT);
            core::CatalogManager::ObjectType resolved_type;
            core::ID table_id;
            core::ErrorContext ctx;
            if (db_->catalog_manager()->resolveObjectPath(path,
                                                         core::CatalogManager::ObjectType::TABLE,
                                                         opts, table_id, resolved_type, &ctx) == core::Status::OK) {
                std::vector<core::CatalogManager::ColumnInfo> cols;
                if (db_->catalog_manager()->getColumns(table_id, cols, &ctx,
                                                       opts.required_privilege) == core::Status::OK) {
                    for (const auto& col : cols) {
                        columns.push_back(col.column_name);
                    }
                }
            }
        }
    }

    std::vector<std::string> emit_columns;
    std::vector<std::vector<std::vector<uint8_t>>> emit_rows;

    bool has_default = false;
    for (const auto& row : rows) {
        for (const auto& val : row) {
            if (val.is_default) {
                has_default = true;
                break;
            }
        }
    }

    if (rows.size() > 1 && has_default) {
        error("DEFAULT values in multi-row INSERT are not supported yet");
    }

    if (!rows.empty()) {
        if (has_default && columns.empty()) {
            error("DEFAULT values require a resolved column list");
        }

        if (has_default) {
            const auto& row = rows.front();
            std::vector<std::vector<uint8_t>> row_exprs;
            for (size_t i = 0; i < row.size() && i < columns.size(); ++i) {
                if (row[i].is_default) {
                    continue;
                }
                emit_columns.push_back(columns[i]);
                row_exprs.push_back(row[i].expr);
            }
            emit_rows.push_back(std::move(row_exprs));
        } else {
            emit_columns = columns;
            for (const auto& row : rows) {
                if (!columns.empty() && row.size() != columns.size()) {
                    error("Column count doesn't match value count");
                }
                std::vector<std::vector<uint8_t>> row_exprs;
                row_exprs.reserve(row.size());
                for (const auto& val : row) {
                    row_exprs.push_back(val.expr);
                }
                emit_rows.push_back(std::move(row_exprs));
            }
        }
    }

    emit(sblr::Opcode::BEGIN_LIST);
    emitUVarint(emit_columns.size());
    for (const auto& col : emit_columns) {
        emit(sblr::Opcode::COLUMN_REF);
        emitString(col);
    }
    emit(sblr::Opcode::END_LIST);

    if (has_select) {
        if (emit_enabled_) {
            bytecode_.insert(bytecode_.end(), select_bytecode.begin(), select_bytecode.end());
        }
    } else {
        emit(sblr::Opcode::BEGIN_LIST);
        if (default_values_only) {
            emitUVarint(0);
        } else {
            emitUVarint(emit_rows.size());
            for (const auto& row : emit_rows) {
                emit(sblr::Opcode::BEGIN_LIST);
                emitUVarint(row.size());
                for (const auto& expr : row) {
                    if (emit_enabled_) {
                        bytecode_.insert(bytecode_.end(), expr.begin(), expr.end());
                    }
                }
                emit(sblr::Opcode::END_LIST);
            }
        }
        emit(sblr::Opcode::END_LIST);
    }

    if (matchKeyword(TokenType::KW_ON)) {
        consumeKeyword(TokenType::KW_CONFLICT, "Expected CONFLICT");
        parseOnConflictClause();
    }

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
        std::vector<std::string> cols;
        do {
            cols.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));

        emit(sblr::Opcode::BEGIN_LIST);
        emitUVarint(cols.size());
        for (const auto& col : cols) {
            emit(sblr::Opcode::COLUMN_REF);
            emitString(col);
        }
        emit(sblr::Opcode::END_LIST);
        consume(TokenType::RIGHT_PAREN, "Expected )");

        // Optional WHERE for partial index
        if (matchKeyword(TokenType::KW_WHERE)) {
            bool prev_emit = emit_enabled_;
            emit_enabled_ = false;
            parseExpression();
            emit_enabled_ = prev_emit;
        }
    } else if (matchKeyword(TokenType::KW_ON)) {
        consumeKeyword(TokenType::KW_CONSTRAINT, "Expected CONSTRAINT");
        parseIdentifier();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_CONSTRAINT));
        emitByte(0);  // Constraint ID not resolved yet
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
        struct Assignment {
            std::string column;
            std::vector<uint8_t> expr;
        };
        std::vector<Assignment> assignments;
        do {
            std::string col = parseIdentifier();
            consume(TokenType::EQUAL, "Expected =");
            assignments.push_back({col, captureExpressionBytecode()});
        } while (match(TokenType::COMMA));

        emit(sblr::Opcode::BEGIN_LIST);
        emitUVarint(assignments.size());
        for (const auto& assign : assignments) {
            emit(sblr::Opcode::ASSIGNMENT);
            emit(sblr::Opcode::COLUMN_REF);
            emitString(assign.column);
            if (emit_enabled_) {
                bytecode_.insert(bytecode_.end(), assign.expr.begin(), assign.expr.end());
            }
        }
        emit(sblr::Opcode::END_LIST);

        // WHERE clause for UPDATE
        if (matchKeyword(TokenType::KW_WHERE)) {
            emit(sblr::Opcode::WHERE_CLAUSE);
            parseExpression();
        }
    }
}

void Parser::parseReturningClause() {
    std::vector<SelectItem> items;
    parseSelectList(items);

    if (!emit_enabled_) {
        return;
    }

    uint64_t emit_count = 0;
    for (const auto& item : items) {
        if (item.kind == SelectItem::Kind::Star ||
            item.kind == SelectItem::Kind::Column ||
            item.kind == SelectItem::Kind::Expression) {
            emit_count++;
        }
    }
    bool emit_fallback_null = false;
    if (emit_count == 0) {
        emit_count = 1;
        emit_fallback_null = true;
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_RETURNING));
    emit(sblr::Opcode::BEGIN_LIST);
    emitUVarint(emit_count);

    if (emit_fallback_null) {
        emit(sblr::Opcode::LITERAL_NULL);
        emitString("");
    } else {
        for (const auto& item : items) {
            if (item.kind == SelectItem::Kind::Star) {
                emit(sblr::Opcode::SELECT_STAR);
                continue;
            }
            if (emit_enabled_) {
                bytecode_.insert(bytecode_.end(),
                                 item.expr_bytecode.begin(),
                                 item.expr_bytecode.end());
            }
            emitString(item.alias);
        }
    }

    emit(sblr::Opcode::END_LIST);
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

    std::string table_alias;

    if (matchKeyword(TokenType::KW_AS)) {
        table_alias = parseIdentifier();
    } else if (check(TokenType::IDENTIFIER) && !check(TokenType::KW_SET)) {
        table_alias = parseIdentifier();
    }

    emit(sblr::Opcode::TABLE_REF);
    emitByte(0);
    emitString(schema.empty() ? table : schema + "/" + table);
    emitString(table_alias);

    // SET clause
    consumeKeyword(TokenType::KW_SET, "Expected SET");

    struct Assignment {
        std::string column;
        std::vector<uint8_t> expr;
    };
    std::vector<Assignment> assignments;
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
                if (i >= cols.size()) {
                    error("UPDATE assignment count doesn't match column list");
                }
                assignments.push_back({cols[i++], captureExpressionBytecode()});
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        } else {
            std::string col = parseIdentifier();
            consume(TokenType::EQUAL, "Expected =");
            assignments.push_back({col, captureExpressionBytecode()});
        }
    } while (match(TokenType::COMMA));

    emit(sblr::Opcode::BEGIN_LIST);
    emitUVarint(assignments.size());
    for (const auto& assign : assignments) {
        emit(sblr::Opcode::ASSIGNMENT);
        emit(sblr::Opcode::COLUMN_REF);
        emitString(assign.column);
        if (emit_enabled_) {
            bytecode_.insert(bytecode_.end(), assign.expr.begin(), assign.expr.end());
        }
    }
    emit(sblr::Opcode::END_LIST);

    // FROM clause (PostgreSQL extension)
    if (matchKeyword(TokenType::KW_FROM)) {
        parseFromClause();
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

    std::string table_alias;

    if (matchKeyword(TokenType::KW_AS)) {
        table_alias = parseIdentifier();
    } else if (check(TokenType::IDENTIFIER) && !check(TokenType::KW_WHERE) &&
               !check(TokenType::KW_USING) && !check(TokenType::KW_RETURNING)) {
        table_alias = parseIdentifier();
    }

    emit(sblr::Opcode::TABLE_REF);
    emitByte(0);
    emitString(schema.empty() ? table : schema + "/" + table);
    emitString(table_alias);

    // USING clause (PostgreSQL extension)
    if (matchKeyword(TokenType::KW_USING)) {
        parseFromClause();
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
    emitString(schema.empty() ? table : (schema + "/" + table));

    // Optional alias
    std::string alias;
    if (matchKeyword(TokenType::KW_AS)) {
        alias = parseIdentifier();
    } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
        alias = parseIdentifier();
    }

    // USING clause
    consumeKeyword(TokenType::KW_USING, "Expected USING");
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_SOURCE));

    if (match(TokenType::LEFT_PAREN)) {
        error("MERGE USING subqueries are not supported in PostgreSQL emulation");
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        parseSelectStmt();
        emit_enabled_ = prev_emit;
        consume(TokenType::RIGHT_PAREN, "Expected )");
        emitString("");
    } else {
        // Table reference
        std::string src_schema;
        std::string src_table = parseIdentifier();
        if (match(TokenType::DOT)) {
            src_schema = src_table;
            src_table = parseIdentifier();
        }
        resolveTableName(src_schema, src_table);
        emitString(src_schema.empty() ? src_table : (src_schema + "/" + src_table));
    }

    // Source alias
    std::string src_alias;
    if (matchKeyword(TokenType::KW_AS)) {
        src_alias = parseIdentifier();
    } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
        src_alias = parseIdentifier();
    }

    // ON clause
    consumeKeyword(TokenType::KW_ON, "Expected ON");
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_ON));
    std::vector<uint8_t> on_expr = captureExpressionBytecode();
    emitU32(static_cast<uint32_t>(on_expr.size()));
    if (emit_enabled_) {
        bytecode_.insert(bytecode_.end(), on_expr.begin(), on_expr.end());
    }

    // WHEN clauses
    while (matchKeyword(TokenType::KW_WHEN)) {
        bool is_not = matchKeyword(TokenType::KW_NOT);
        if (!matchIdentifierKeyword("MATCHED")) {
            error("Expected MATCHED");
        }

        enum class MergeClauseType {
            MATCHED,
            NOT_MATCHED,
            NOT_MATCHED_BY_SOURCE
        };
        MergeClauseType clause_type = MergeClauseType::MATCHED;

        if (is_not) {
            bool by_source = false;
            if (matchKeyword(TokenType::KW_BY)) {
                if (matchIdentifierKeyword("SOURCE")) {
                    by_source = true;
                } else if (matchIdentifierKeyword("TARGET")) {
                    by_source = false;
                } else {
                    error("Expected SOURCE or TARGET after BY in MERGE");
                }
            }
            clause_type = by_source ? MergeClauseType::NOT_MATCHED_BY_SOURCE
                                    : MergeClauseType::NOT_MATCHED;
        }

        bool has_condition = false;
        std::vector<uint8_t> condition_expr;
        if (matchKeyword(TokenType::KW_AND)) {
            has_condition = true;
            condition_expr = captureExpressionBytecode();
        }

        consumeKeyword(TokenType::KW_THEN, "Expected THEN");

        if (clause_type == MergeClauseType::MATCHED ||
            clause_type == MergeClauseType::NOT_MATCHED_BY_SOURCE) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(
                clause_type == MergeClauseType::MATCHED
                    ? sblr::ExtendedOpcode::EXT_MERGE_WHEN_MATCHED
                    : sblr::ExtendedOpcode::EXT_MERGE_WHEN_NOT_MATCHED_SOURCE));
            emitByte(has_condition ? 1 : 0);
            if (has_condition) {
                emitU32(static_cast<uint32_t>(condition_expr.size()));
                if (emit_enabled_) {
                    bytecode_.insert(bytecode_.end(),
                                     condition_expr.begin(),
                                     condition_expr.end());
                }
            }

            if (matchKeyword(TokenType::KW_UPDATE)) {
                emitByte(0);  // not a delete
                consumeKeyword(TokenType::KW_SET, "Expected SET");

                struct Assignment {
                    std::string column;
                    std::vector<uint8_t> expr;
                };
                std::vector<Assignment> assignments;
                do {
                    std::string col = parseIdentifier();
                    consume(TokenType::EQUAL, "Expected =");
                    assignments.push_back({col, captureExpressionBytecode()});
                } while (match(TokenType::COMMA));

                emitU32(static_cast<uint32_t>(assignments.size()));
                for (const auto& assign : assignments) {
                    emitString(assign.column);
                    emitU32(static_cast<uint32_t>(assign.expr.size()));
                    if (emit_enabled_) {
                        bytecode_.insert(bytecode_.end(),
                                         assign.expr.begin(),
                                         assign.expr.end());
                    }
                }
            } else if (matchKeyword(TokenType::KW_DELETE)) {
                emitByte(1);  // delete
            } else {
                error("Expected UPDATE or DELETE after MERGE WHEN MATCHED");
            }
        } else {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_WHEN_NOT_MATCHED));
            emitByte(has_condition ? 1 : 0);
            if (has_condition) {
                emitU32(static_cast<uint32_t>(condition_expr.size()));
                if (emit_enabled_) {
                    bytecode_.insert(bytecode_.end(),
                                     condition_expr.begin(),
                                     condition_expr.end());
                }
            }

            consumeKeyword(TokenType::KW_INSERT, "Expected INSERT after WHEN NOT MATCHED");

            std::vector<std::string> insert_columns;
            if (match(TokenType::LEFT_PAREN)) {
                do {
                    insert_columns.push_back(parseIdentifier());
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }

            if (insert_columns.empty()) {
                error("MERGE INSERT requires an explicit column list in PostgreSQL emulation");
            }

            consumeKeyword(TokenType::KW_VALUES, "Expected VALUES");
            consume(TokenType::LEFT_PAREN, "Expected (");

            std::vector<std::vector<uint8_t>> insert_exprs;
            do {
                insert_exprs.push_back(captureExpressionBytecode());
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");

            if (!insert_columns.empty() && insert_columns.size() != insert_exprs.size()) {
                error("MERGE INSERT column count does not match VALUES count");
            }

            uint32_t emit_count = insert_columns.empty()
                ? static_cast<uint32_t>(insert_exprs.size())
                : static_cast<uint32_t>(insert_columns.size());
            if (insert_columns.empty()) {
                insert_columns.resize(emit_count);
            }

            emitU32(emit_count);
            for (const auto& col : insert_columns) {
                emitString(col);
            }
            for (const auto& expr : insert_exprs) {
                emitU32(static_cast<uint32_t>(expr.size()));
                if (emit_enabled_) {
                    bytecode_.insert(bytecode_.end(), expr.begin(), expr.end());
                }
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
    size_t count_pos = bytecode_.size();
    emitU16(0);
    emitByte(is_recursive ? 1 : 0);
    uint16_t count = 0;

    do {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CTE_DEF));

        std::string cte_name = parseIdentifier();
        emitString(cte_name);

        // Optional column list
        if (match(TokenType::LEFT_PAREN)) {
            do {
                parseIdentifier();
            } while (match(TokenType::COMMA));

            consume(TokenType::RIGHT_PAREN, "Expected )");
        } else {
            // No column list
        }

        consumeKeyword(TokenType::KW_AS, "Expected AS");

        // MATERIALIZED / NOT MATERIALIZED (PostgreSQL 12+)
        if (matchKeyword(TokenType::KW_MATERIALIZED)) {
            // Ignored for now
        } else if (matchKeyword(TokenType::KW_NOT)) {
            consumeKeyword(TokenType::KW_MATERIALIZED, "Expected MATERIALIZED");
            // Ignored for now
        }

        consume(TokenType::LEFT_PAREN, "Expected (");
        parseSelectStmt();  // CTE query
        consume(TokenType::RIGHT_PAREN, "Expected )");

        count++;
    } while (match(TokenType::COMMA));

    sblr::writeInt16(&bytecode_[count_pos], count);
}

// Helper for frame bounds - moved earlier in the file

} // namespace scratchbird::parser::postgresql
