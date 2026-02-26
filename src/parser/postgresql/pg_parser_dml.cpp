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
 * PostgreSQL Parser - DML Statement Parsing (AST v3)
 */

#include "scratchbird/parser/postgresql/pg_parser.h"
#include <algorithm>

namespace scratchbird::parser::postgresql {

static parser::v3::SchemaPath buildPathFromQualified(parser::v3::StringPool& pool,
                                                     const std::string& name) {
    std::vector<parser::v3::StringPool::StringId> comps;
    std::string cur;
    for (char ch : name) {
        if (ch == '.') {
            if (!cur.empty()) {
                comps.push_back(pool.intern(cur));
                cur.clear();
            }
        } else {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) comps.push_back(pool.intern(cur));
    parser::v3::PathType path_type = comps.size() > 1 ? parser::v3::PathType::ABSOLUTE
                                                       : parser::v3::PathType::UNQUALIFIED;
    return parser::v3::SchemaPath(path_type, std::move(comps));
}

parser::v3::SelectStmt* Parser::parseSelectStmt() {
    consumeKeyword(TokenType::KW_SELECT, "Expected SELECT");
    auto* stmt = arena()->create<parser::v3::SelectStmt>();

    if (matchKeyword(TokenType::KW_DISTINCT)) {
        stmt->distinct = true;
        if (matchKeyword(TokenType::KW_ON)) {
            consume(TokenType::LEFT_PAREN, "Expected ( after DISTINCT ON");
            // DISTINCT ON list is parsed for validation; executor behavior is handled later.
            if (!check(TokenType::RIGHT_PAREN)) {
                do { parseExpression(); } while (match(TokenType::COMMA));
            }
            consume(TokenType::RIGHT_PAREN, "Expected ) after DISTINCT ON list");
        }
    } else if (matchKeyword(TokenType::KW_ALL)) {
        stmt->all = true;
    }

    std::vector<SelectItem> items;
    parseSelectList(items);
    for (auto& item : items) {
        auto* out = arena()->create<parser::v3::SelectItem>();
        if (item.kind == SelectItem::Kind::Star) {
            out->item_type = parser::v3::SelectItem::Type::STAR;
        } else if (item.kind == SelectItem::Kind::Column && item.expr == nullptr) {
            out->item_type = parser::v3::SelectItem::Type::TABLE_STAR;
            out->table_path = buildPathFromQualified(string_pool_, item.column_name);
        } else {
            out->item_type = parser::v3::SelectItem::Type::EXPRESSION;
            out->expr = item.expr;
            if (!item.alias.empty()) {
                out->alias = string_pool_.intern(item.alias);
                out->has_alias = true;
            }
        }
        stmt->items.push_back(out);
    }

    if (matchKeyword(TokenType::KW_FROM)) {
        auto parse_alias = [&]() -> parser::v3::StringPool::StringId {
            auto consume_alias_column_list = [&]() {
                if (!match(TokenType::LEFT_PAREN)) {
                    return;
                }
                if (!check(TokenType::RIGHT_PAREN)) {
                    do {
                        parseIdentifierId();
                    } while (match(TokenType::COMMA));
                }
                consume(TokenType::RIGHT_PAREN, "Expected ) after table alias column list");
            };

            if (matchKeyword(TokenType::KW_AS)) {
                auto alias = parseIdentifierId();
                consume_alias_column_list();
                return alias;
            }
            if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
                auto alias = parseIdentifierId();
                consume_alias_column_list();
                return alias;
            }
            return parser::v3::StringPool::INVALID_ID;
        };

        auto parse_table_ref = [&]() -> parser::v3::TableRefNode* {
            auto* ref = arena()->create<parser::v3::TableRefNode>();
            if (matchKeyword(TokenType::KW_LATERAL)) {
                ref->lateral = true;
            }
            if (match(TokenType::LEFT_PAREN)) {
                if (!check(TokenType::KW_SELECT)) {
                    error("Expected SELECT in subquery table reference");
                }
                ref->ref_type = parser::v3::TableRefNode::Type::SUBQUERY;
                ref->subquery = parseSelectStmt();
                consume(TokenType::RIGHT_PAREN, "Expected ) after subquery table reference");
            } else {
                ref->ref_type = parser::v3::TableRefNode::Type::TABLE;
                ref->table_path = buildPathFromQualified(string_pool_, parseQualifiedName());
            }
            ref->alias = parse_alias();
            ref->has_alias = ref->alias != parser::v3::StringPool::INVALID_ID;
            return ref;
        };

        auto* base = parse_table_ref();
        stmt->from = base;

        // joins
        while (true) {
            parser::JoinType join_type = parser::JoinType::INNER;
            bool has_join = false;
            if (matchKeyword(TokenType::KW_NATURAL)) {
                // NATURAL [INNER|LEFT|RIGHT|FULL] JOIN
                if (matchKeyword(TokenType::KW_LEFT)) {
                    matchKeyword(TokenType::KW_OUTER);
                    join_type = parser::JoinType::LEFT;
                } else if (matchKeyword(TokenType::KW_RIGHT)) {
                    matchKeyword(TokenType::KW_OUTER);
                    join_type = parser::JoinType::RIGHT;
                } else if (matchKeyword(TokenType::KW_FULL)) {
                    matchKeyword(TokenType::KW_OUTER);
                    join_type = parser::JoinType::FULL;
                } else if (matchKeyword(TokenType::KW_INNER)) {
                    join_type = parser::JoinType::INNER;
                }
                consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
                has_join = true;
            } else if (matchKeyword(TokenType::KW_INNER)) {
                consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
                join_type = parser::JoinType::INNER;
                has_join = true;
            } else if (matchKeyword(TokenType::KW_LEFT)) {
                matchKeyword(TokenType::KW_OUTER);
                consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
                join_type = parser::JoinType::LEFT;
                has_join = true;
            } else if (matchKeyword(TokenType::KW_RIGHT)) {
                matchKeyword(TokenType::KW_OUTER);
                consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
                join_type = parser::JoinType::RIGHT;
                has_join = true;
            } else if (matchKeyword(TokenType::KW_FULL)) {
                matchKeyword(TokenType::KW_OUTER);
                consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
                join_type = parser::JoinType::FULL;
                has_join = true;
            } else if (matchKeyword(TokenType::KW_CROSS)) {
                consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
                join_type = parser::JoinType::CROSS;
                has_join = true;
            } else if (matchKeyword(TokenType::KW_JOIN)) {
                join_type = parser::JoinType::INNER;
                has_join = true;
            } else if (match(TokenType::COMMA)) {
                // PostgreSQL comma-join syntax in FROM lists is equivalent to CROSS JOIN.
                join_type = parser::JoinType::CROSS;
                has_join = true;
            }

            if (!has_join) break;

            auto* join = arena()->create<parser::v3::JoinNode>();
            join->join_type = join_type;
            join->right = parse_table_ref();

            if (matchKeyword(TokenType::KW_ON)) {
                join->on_condition = parseExpression();
            } else if (matchKeyword(TokenType::KW_USING)) {
                join->has_using = true;
                consume(TokenType::LEFT_PAREN, "Expected (");
                do {
                    join->using_columns.push_back(parseIdentifierId());
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }
            stmt->joins.push_back(join);
        }
    }

    if (matchKeyword(TokenType::KW_WHERE)) {
        stmt->where = parseExpression();
    }

    if (matchKeyword(TokenType::KW_GROUP)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after GROUP");
        do {
            stmt->group_by.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    if (matchKeyword(TokenType::KW_HAVING)) {
        stmt->having = parseExpression();
    }

    if (matchKeyword(TokenType::KW_ORDER)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after ORDER");
        do {
            auto* item = arena()->create<parser::v3::OrderByItem>();
            item->expr = parseExpression();
            if (matchKeyword(TokenType::KW_USING)) {
                // PostgreSQL supports ORDER BY <expr> USING <operator>. We map
                // simple operator forms to ASC/DESC semantics for compatibility.
                if (match(TokenType::LESS_THAN) || match(TokenType::LESS_EQUAL)) {
                    item->ascending = true;
                } else if (match(TokenType::GREATER_THAN) || match(TokenType::GREATER_EQUAL)) {
                    item->ascending = false;
                } else if (match(TokenType::EQUAL)) {
                    item->ascending = true;
                } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
                    (void)parseIdentifierId();
                } else {
                    error("Expected operator after USING in ORDER BY");
                }
            }
            if (matchKeyword(TokenType::KW_ASC)) item->ascending = true;
            else if (matchKeyword(TokenType::KW_DESC)) item->ascending = false;
            if (matchKeyword(TokenType::KW_NULLS)) {
                item->has_nulls_spec = true;
                if (matchKeyword(TokenType::KW_FIRST)) item->nulls_first = true;
                else if (matchKeyword(TokenType::KW_LAST)) item->nulls_last = true;
            }
            stmt->order_by.push_back(item);
        } while (match(TokenType::COMMA));
    }

    if (matchKeyword(TokenType::KW_LIMIT)) {
        stmt->limit = parseExpression();
    }
    if (matchKeyword(TokenType::KW_OFFSET)) {
        stmt->offset = parseExpression();
    }
    if (matchKeyword(TokenType::KW_FETCH)) {
        if (!(matchKeyword(TokenType::KW_FIRST) || matchKeyword(TokenType::KW_NEXT))) {
            error("Expected FIRST or NEXT after FETCH");
        }
        if (!check(TokenType::KW_ROW) && !check(TokenType::KW_ROWS)) {
            stmt->limit = parseExpression();
        }
        matchKeyword(TokenType::KW_ROW);
        matchKeyword(TokenType::KW_ROWS);
        if (matchKeyword(TokenType::KW_WITH)) {
            consumeKeyword(TokenType::KW_TIES, "Expected TIES after WITH");
        } else {
            consumeKeyword(TokenType::KW_ONLY, "Expected ONLY after FETCH");
        }
    }

    if (matchKeyword(TokenType::KW_FOR)) {
        if (matchKeyword(TokenType::KW_UPDATE)) {
            stmt->for_update = true;
        } else if (matchKeyword(TokenType::KW_SHARE) || matchIdentifierKeyword("SHARE")) {
            stmt->for_share = true;
        } else if (matchKeyword(TokenType::KW_KEY)) {
            if (!(matchKeyword(TokenType::KW_SHARE) || matchIdentifierKeyword("SHARE"))) {
                error("Expected SHARE after FOR KEY");
            }
            stmt->for_share = true;
        } else if (matchKeyword(TokenType::KW_NO)) {
            consumeKeyword(TokenType::KW_KEY, "Expected KEY after FOR NO");
            consumeKeyword(TokenType::KW_UPDATE, "Expected UPDATE after FOR NO KEY");
            stmt->for_update = true;
        }
        if (matchKeyword(TokenType::KW_OF)) {
            // Consume optional table list in FOR ... OF table[, ...]
            do {
                parseQualifiedName();
            } while (match(TokenType::COMMA));
        }
        if (matchKeyword(TokenType::KW_NOWAIT)) stmt->nowait = true;
        if (matchKeyword(TokenType::KW_SKIP)) {
            consumeKeyword(TokenType::KW_LOCKED, "Expected LOCKED after SKIP");
            stmt->skip_locked = true;
        }
    }

    emit(sblr::Opcode::SELECT);
    emitByte(stmt->distinct ? 1u : 0u);
    emit(sblr::Opcode::BEGIN_LIST);
    emitUVarint(static_cast<uint64_t>(items.size()));
    for (const auto& item : items) {
        if (item.kind == SelectItem::Kind::Star) {
            emit(sblr::Opcode::SELECT_STAR);
        } else {
            emit(sblr::Opcode::SELECT_STAR);
        }
    }
    emit(sblr::Opcode::END_LIST);

    if (stmt->from) {
        auto emit_table_ref = [&](const parser::v3::TableRefNode* ref) {
            emit(sblr::Opcode::TABLE_REF);
            emitByte(0);  // name-based ref
            if (ref->ref_type == parser::v3::TableRefNode::Type::SUBQUERY) {
                emitString("(subquery)");
            } else {
                emitString(parser::v3::schemaPathToString(ref->table_path, string_pool_));
            }
            if (ref->has_alias) {
                emitString(std::string(string_pool_.get(ref->alias)));
            } else {
                emitString("");
            }
        };

        emit(sblr::Opcode::BEGIN_LIST);
        emitUVarint(1);
        emit_table_ref(stmt->from);
        emit(sblr::Opcode::END_LIST);

        for (const auto* join : stmt->joins) {
            emit(sblr::Opcode::JOIN_TYPE);
            emitByte(static_cast<uint8_t>(join->join_type));
            if (join->right) {
                emit_table_ref(join->right);
            }
            if (join->on_condition) {
                emit(sblr::Opcode::JOIN_CONDITION);
            }
        }
    }

    return stmt;
}

void Parser::parseSelectList(std::vector<SelectItem>& items) {
    do {
        if (match(TokenType::STAR)) {
            SelectItem item; item.kind = SelectItem::Kind::Star;
            items.push_back(item);
            continue;
        }
        SelectItem item;
        item.kind = SelectItem::Kind::Expression;
        item.expr = parseExpression();

        if (auto* column_ref = dynamic_cast<parser::v3::ColumnRefExpr*>(item.expr);
            column_ref && column_ref->column.has_table_qualifier &&
            column_ref->column.column_name != parser::v3::StringPool::INVALID_ID &&
            string_pool_.get(column_ref->column.column_name) == "*") {
            item.kind = SelectItem::Kind::Column;
            item.column_name = parser::v3::schemaPathToString(column_ref->column.table_path, string_pool_);
            item.expr = nullptr;
        } else if (matchKeyword(TokenType::KW_AS)) {
            item.alias = parseIdentifier();
        } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
            item.alias = parseIdentifier();
        }

        items.push_back(item);
    } while (match(TokenType::COMMA));
}

parser::v3::InsertStmt* Parser::parseInsertStmt() {
    consumeKeyword(TokenType::KW_INSERT, "Expected INSERT");
    consumeKeyword(TokenType::KW_INTO, "Expected INTO");

    auto* stmt = arena()->create<parser::v3::InsertStmt>();
    stmt->table_path = buildPathFromQualified(string_pool_, parseQualifiedName());

    if (match(TokenType::LEFT_PAREN)) {
        do {
            stmt->columns.push_back(parseIdentifierId());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    if (matchKeyword(TokenType::KW_VALUES)) {
        stmt->source = parser::v3::InsertStmt::Source::VALUES;
        do {
            consume(TokenType::LEFT_PAREN, "Expected (");
            std::vector<parser::v3::Expression*> row;
            if (!match(TokenType::RIGHT_PAREN)) {
                do { row.push_back(parseExpression()); } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }
            stmt->values_rows.push_back(std::move(row));
        } while (match(TokenType::COMMA));
    } else if (check(TokenType::KW_SELECT)) {
        stmt->source = parser::v3::InsertStmt::Source::SELECT;
        stmt->select_source = parseSelectStmt();
    } else if (matchKeyword(TokenType::KW_DEFAULT)) {
        consumeKeyword(TokenType::KW_VALUES, "Expected VALUES after DEFAULT");
        stmt->source = parser::v3::InsertStmt::Source::DEFAULT;
    }

    if (matchKeyword(TokenType::KW_ON)) {
        consumeKeyword(TokenType::KW_CONFLICT, "Expected CONFLICT after ON");
        auto* conflict = arena()->create<parser::v3::OnConflictClause>();

        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    conflict->columns.push_back(parseIdentifierId());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RIGHT_PAREN, "Expected ) after ON CONFLICT column list");
        } else if (matchKeyword(TokenType::KW_ON)) {
            consumeKeyword(TokenType::KW_CONSTRAINT, "Expected CONSTRAINT after ON CONFLICT ON");
            conflict->constraint_name = parseIdentifierId();
        }

        if (matchKeyword(TokenType::KW_WHERE)) {
            conflict->where_target = parseExpression();
        }

        consumeKeyword(TokenType::KW_DO, "Expected DO in ON CONFLICT clause");
        if (matchKeyword(TokenType::KW_NOTHING)) {
            conflict->action = parser::v3::ConflictAction::NOTHING;
        } else {
            consumeKeyword(TokenType::KW_UPDATE, "Expected UPDATE or NOTHING after DO");
            conflict->action = parser::v3::ConflictAction::UPDATE;
            consumeKeyword(TokenType::KW_SET, "Expected SET in ON CONFLICT DO UPDATE");
            do {
                auto col = parseIdentifierId();
                consume(TokenType::EQUAL, "Expected = in ON CONFLICT DO UPDATE SET");
                auto* expr = parseExpression();
                conflict->set_items.push_back({col, expr});
            } while (match(TokenType::COMMA));
            if (matchKeyword(TokenType::KW_WHERE)) {
                conflict->where_action = parseExpression();
            }
        }

        stmt->on_conflict = conflict;
    }

    if (matchKeyword(TokenType::KW_RETURNING)) {
        std::vector<SelectItem> items;
        parseSelectList(items);
        for (auto& item : items) {
            auto* out = arena()->create<parser::v3::SelectItem>();
            out->item_type = parser::v3::SelectItem::Type::EXPRESSION;
            out->expr = item.expr;
            if (!item.alias.empty()) {
                out->alias = string_pool_.intern(item.alias);
                out->has_alias = true;
            }
            stmt->returning.push_back(out);
        }
    }

    if (stmt->on_conflict) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT));

        if (!stmt->on_conflict->columns.empty()) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_COLUMN));
            emit(sblr::Opcode::BEGIN_LIST);
            emitUVarint(static_cast<uint64_t>(stmt->on_conflict->columns.size()));
            for (auto col : stmt->on_conflict->columns) {
                emit(sblr::Opcode::COLUMN_REF);
                emitString(std::string(string_pool_.get(col)));
            }
            emit(sblr::Opcode::END_LIST);
        }

        emit(sblr::Opcode::EXTENDED_OPCODE);
        if (stmt->on_conflict->action == parser::v3::ConflictAction::NOTHING) {
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_NOTHING));
        } else {
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_UPDATE));
        }
    }

    return stmt;
}

parser::v3::UpdateStmt* Parser::parseUpdateStmt() {
    consumeKeyword(TokenType::KW_UPDATE, "Expected UPDATE");
    auto* stmt = arena()->create<parser::v3::UpdateStmt>();
    stmt->table_path = buildPathFromQualified(string_pool_, parseQualifiedName());
    if (matchKeyword(TokenType::KW_AS)) {
        stmt->alias = parseIdentifierId();
        stmt->has_alias = true;
    } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
        stmt->alias = parseIdentifierId();
        stmt->has_alias = true;
    }

    consumeKeyword(TokenType::KW_SET, "Expected SET");
    do {
        auto col = parseIdentifierId();
        if (match(TokenType::DOT)) {
            // Qualified targets (alias.column) are legal in PostgreSQL grammar
            // even when later rejected semantically.
            col = parseIdentifierId();
        }
        consume(TokenType::EQUAL, "Expected = in SET clause");
        auto* expr = parseExpression();
        stmt->set_items.push_back({col, expr});
    } while (match(TokenType::COMMA));

    if (matchKeyword(TokenType::KW_FROM)) {
        auto parse_alias = [&]() -> parser::v3::StringPool::StringId {
            auto consume_alias_column_list = [&]() {
                if (!match(TokenType::LEFT_PAREN)) {
                    return;
                }
                if (!check(TokenType::RIGHT_PAREN)) {
                    do {
                        parseIdentifierId();
                    } while (match(TokenType::COMMA));
                }
                consume(TokenType::RIGHT_PAREN, "Expected ) after table alias column list");
            };

            if (matchKeyword(TokenType::KW_AS)) {
                auto alias = parseIdentifierId();
                consume_alias_column_list();
                return alias;
            }
            if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
                auto alias = parseIdentifierId();
                consume_alias_column_list();
                return alias;
            }
            return parser::v3::StringPool::INVALID_ID;
        };

        auto* base = arena()->create<parser::v3::TableRefNode>();
        base->ref_type = parser::v3::TableRefNode::Type::TABLE;
        base->table_path = buildPathFromQualified(string_pool_, parseQualifiedName());
        base->alias = parse_alias();
        base->has_alias = base->alias != parser::v3::StringPool::INVALID_ID;
        stmt->from = base;

        while (true) {
            parser::JoinType join_type = parser::JoinType::INNER;
            bool has_join = false;
            if (matchKeyword(TokenType::KW_INNER)) {
                consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
                join_type = parser::JoinType::INNER;
                has_join = true;
            } else if (matchKeyword(TokenType::KW_LEFT)) {
                matchKeyword(TokenType::KW_OUTER);
                consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
                join_type = parser::JoinType::LEFT;
                has_join = true;
            } else if (matchKeyword(TokenType::KW_RIGHT)) {
                matchKeyword(TokenType::KW_OUTER);
                consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
                join_type = parser::JoinType::RIGHT;
                has_join = true;
            } else if (matchKeyword(TokenType::KW_FULL)) {
                matchKeyword(TokenType::KW_OUTER);
                consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
                join_type = parser::JoinType::FULL;
                has_join = true;
            } else if (matchKeyword(TokenType::KW_CROSS)) {
                consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
                join_type = parser::JoinType::CROSS;
                has_join = true;
            } else if (matchKeyword(TokenType::KW_JOIN)) {
                join_type = parser::JoinType::INNER;
                has_join = true;
            } else if (match(TokenType::COMMA)) {
                join_type = parser::JoinType::CROSS;
                has_join = true;
            }

            if (!has_join) break;

            auto* join = arena()->create<parser::v3::JoinNode>();
            join->join_type = join_type;
            auto* right = arena()->create<parser::v3::TableRefNode>();
            right->ref_type = parser::v3::TableRefNode::Type::TABLE;
            right->table_path = buildPathFromQualified(string_pool_, parseQualifiedName());
            right->alias = parse_alias();
            right->has_alias = right->alias != parser::v3::StringPool::INVALID_ID;
            join->right = right;

            if (matchKeyword(TokenType::KW_ON)) {
                join->on_condition = parseExpression();
            } else if (matchKeyword(TokenType::KW_USING)) {
                join->has_using = true;
                consume(TokenType::LEFT_PAREN, "Expected (");
                do {
                    join->using_columns.push_back(parseIdentifierId());
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }
            stmt->joins.push_back(join);
        }
    }

    if (matchKeyword(TokenType::KW_WHERE)) stmt->where = parseExpression();

    if (matchKeyword(TokenType::KW_RETURNING)) {
        std::vector<SelectItem> items;
        parseSelectList(items);
        for (auto& item : items) {
            auto* out = arena()->create<parser::v3::SelectItem>();
            out->item_type = parser::v3::SelectItem::Type::EXPRESSION;
            out->expr = item.expr;
            if (!item.alias.empty()) {
                out->alias = string_pool_.intern(item.alias);
                out->has_alias = true;
            }
            stmt->returning.push_back(out);
        }
    }

    auto emit_table_ref = [&](const parser::v3::TableRefNode* ref) {
        emit(sblr::Opcode::TABLE_REF);
        emitByte(0);  // name-based ref
        emitString(parser::v3::schemaPathToString(ref->table_path, string_pool_));
        if (ref->has_alias) {
            emitString(std::string(string_pool_.get(ref->alias)));
        } else {
            emitString("");
        }
    };

    emit(sblr::Opcode::TABLE_REF);
    emitByte(0);
    emitString(parser::v3::schemaPathToString(stmt->table_path, string_pool_));
    if (stmt->has_alias) {
        emitString(std::string(string_pool_.get(stmt->alias)));
    } else {
        emitString("");
    }

    if (stmt->from) {
        emit_table_ref(stmt->from);
    }
    for (const auto* join : stmt->joins) {
        if (join->right) {
            emit_table_ref(join->right);
        }
    }

    return stmt;
}

parser::v3::DeleteStmt* Parser::parseDeleteStmt() {
    consumeKeyword(TokenType::KW_DELETE, "Expected DELETE");
    consumeKeyword(TokenType::KW_FROM, "Expected FROM");
    auto* stmt = arena()->create<parser::v3::DeleteStmt>();
    stmt->table_path = buildPathFromQualified(string_pool_, parseQualifiedName());
    if (matchKeyword(TokenType::KW_AS)) {
        stmt->alias = parseIdentifierId();
        stmt->has_alias = true;
        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                do { parseIdentifierId(); } while (match(TokenType::COMMA));
            }
            consume(TokenType::RIGHT_PAREN, "Expected ) after table alias column list");
        }
    } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
        stmt->alias = parseIdentifierId();
        stmt->has_alias = true;
        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                do { parseIdentifierId(); } while (match(TokenType::COMMA));
            }
            consume(TokenType::RIGHT_PAREN, "Expected ) after table alias column list");
        }
    }

    if (matchKeyword(TokenType::KW_USING)) {
        auto parse_alias = [&]() -> parser::v3::StringPool::StringId {
            auto consume_alias_column_list = [&]() {
                if (!match(TokenType::LEFT_PAREN)) {
                    return;
                }
                if (!check(TokenType::RIGHT_PAREN)) {
                    do {
                        parseIdentifierId();
                    } while (match(TokenType::COMMA));
                }
                consume(TokenType::RIGHT_PAREN, "Expected ) after table alias column list");
            };

            if (matchKeyword(TokenType::KW_AS)) {
                auto alias = parseIdentifierId();
                consume_alias_column_list();
                return alias;
            }
            if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
                auto alias = parseIdentifierId();
                consume_alias_column_list();
                return alias;
            }
            return parser::v3::StringPool::INVALID_ID;
        };

        auto* using_base = arena()->create<parser::v3::TableRefNode>();
        using_base->ref_type = parser::v3::TableRefNode::Type::TABLE;
        using_base->table_path = buildPathFromQualified(string_pool_, parseQualifiedName());
        using_base->alias = parse_alias();
        using_base->has_alias = using_base->alias != parser::v3::StringPool::INVALID_ID;
        stmt->using_clause = using_base;
    }

    if (matchKeyword(TokenType::KW_WHERE)) stmt->where = parseExpression();

    if (matchKeyword(TokenType::KW_RETURNING)) {
        std::vector<SelectItem> items;
        parseSelectList(items);
        for (auto& item : items) {
            auto* out = arena()->create<parser::v3::SelectItem>();
            out->item_type = parser::v3::SelectItem::Type::EXPRESSION;
            out->expr = item.expr;
            if (!item.alias.empty()) {
                out->alias = string_pool_.intern(item.alias);
                out->has_alias = true;
            }
            stmt->returning.push_back(out);
        }
    }

    emit(sblr::Opcode::TABLE_REF);
    emitByte(0);
    emitString(parser::v3::schemaPathToString(stmt->table_path, string_pool_));
    if (stmt->has_alias) {
        emitString(std::string(string_pool_.get(stmt->alias)));
    } else {
        emitString("");
    }
    if (stmt->using_clause) {
        emit(sblr::Opcode::TABLE_REF);
        emitByte(0);
        emitString(parser::v3::schemaPathToString(stmt->using_clause->table_path, string_pool_));
        if (stmt->using_clause->has_alias) {
            emitString(std::string(string_pool_.get(stmt->using_clause->alias)));
        } else {
            emitString("");
        }
    }

    return stmt;
}

parser::v3::MergeStmt* Parser::parseMergeStmt() {
    consumeKeyword(TokenType::KW_MERGE, "Expected MERGE");
    consumeKeyword(TokenType::KW_INTO, "Expected INTO");

    auto* stmt = arena()->create<parser::v3::MergeStmt>();
    stmt->target_table = buildPathFromQualified(string_pool_, parseQualifiedName());
    if (matchKeyword(TokenType::KW_AS)) {
        stmt->target_alias = parseIdentifierId();
    } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
        stmt->target_alias = parseIdentifierId();
    }

    consumeKeyword(TokenType::KW_USING, "Expected USING");
    if (match(TokenType::LEFT_PAREN)) {
        stmt->source_query = parseSelectStmt();
        consume(TokenType::RIGHT_PAREN, "Expected ) after MERGE USING subquery");
    } else {
        stmt->source_table = buildPathFromQualified(string_pool_, parseQualifiedName());
    }
    if (matchKeyword(TokenType::KW_AS)) {
        stmt->source_alias = parseIdentifierId();
    } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
        stmt->source_alias = parseIdentifierId();
    }

    consumeKeyword(TokenType::KW_ON, "Expected ON in MERGE");
    consume(TokenType::LEFT_PAREN, "Expected ( after MERGE ON");
    stmt->on_condition = parseExpression();
    consume(TokenType::RIGHT_PAREN, "Expected ) after MERGE ON condition");

    while (matchKeyword(TokenType::KW_WHEN)) {
        bool not_matched = false;
        bool by_source = false;
        if (matchKeyword(TokenType::KW_MATCHED) || matchIdentifierKeyword("MATCHED")) {
            // WHEN MATCHED
        } else if (matchKeyword(TokenType::KW_NOT) || matchIdentifierKeyword("NOT")) {
            if (!(matchKeyword(TokenType::KW_MATCHED) || matchIdentifierKeyword("MATCHED"))) {
                error("Expected MATCHED after WHEN NOT");
            }
            not_matched = true;
            if (matchKeyword(TokenType::KW_BY) || matchIdentifierKeyword("BY")) {
                if (!matchIdentifierKeyword("SOURCE")) {
                    error("Expected SOURCE after WHEN NOT MATCHED BY");
                }
                by_source = true;
            }
        } else {
            error("Expected MATCHED or NOT MATCHED after WHEN");
            break;
        }

        consumeKeyword(TokenType::KW_THEN, "Expected THEN in MERGE WHEN clause");

        if (!not_matched) {
            parser::v3::MergeStmt::WhenMatched branch;
            if (matchKeyword(TokenType::KW_UPDATE)) {
                consumeKeyword(TokenType::KW_SET, "Expected SET in MERGE WHEN MATCHED UPDATE");
                do {
                    auto col = parseIdentifierId();
                    consume(TokenType::EQUAL, "Expected = in MERGE UPDATE assignment");
                    branch.assignments.push_back({col, parseExpression()});
                } while (match(TokenType::COMMA));
            } else if (matchKeyword(TokenType::KW_DELETE)) {
                branch.is_delete = true;
            } else {
                error("Expected UPDATE or DELETE in MERGE WHEN MATCHED");
            }
            stmt->when_matched.push_back(std::move(branch));
            continue;
        }

        if (by_source) {
            parser::v3::MergeStmt::WhenNotMatchedBySource branch;
            if (matchKeyword(TokenType::KW_DELETE)) {
                branch.is_delete = true;
            } else if (matchKeyword(TokenType::KW_UPDATE)) {
                consumeKeyword(TokenType::KW_SET, "Expected SET in MERGE WHEN NOT MATCHED BY SOURCE UPDATE");
                do {
                    auto col = parseIdentifierId();
                    consume(TokenType::EQUAL, "Expected = in MERGE UPDATE assignment");
                    branch.assignments.push_back({col, parseExpression()});
                } while (match(TokenType::COMMA));
            } else {
                error("Expected DELETE or UPDATE in MERGE WHEN NOT MATCHED BY SOURCE");
            }
            stmt->when_not_matched_by_source.push_back(std::move(branch));
            continue;
        }

        parser::v3::MergeStmt::WhenNotMatched branch;
        consumeKeyword(TokenType::KW_INSERT, "Expected INSERT in MERGE WHEN NOT MATCHED");
        if (!match(TokenType::LEFT_PAREN)) {
            error("MERGE INSERT requires explicit column list");
            break;
        }
        if (!check(TokenType::RIGHT_PAREN)) {
            do { branch.columns.push_back(parseIdentifierId()); } while (match(TokenType::COMMA));
        }
        consume(TokenType::RIGHT_PAREN, "Expected ) after MERGE INSERT column list");
        consumeKeyword(TokenType::KW_VALUES, "Expected VALUES in MERGE INSERT");
        consume(TokenType::LEFT_PAREN, "Expected ( after VALUES");
        if (!check(TokenType::RIGHT_PAREN)) {
            do { branch.values.push_back(parseExpression()); } while (match(TokenType::COMMA));
        }
        consume(TokenType::RIGHT_PAREN, "Expected ) after MERGE INSERT values");
        stmt->when_not_matched.push_back(std::move(branch));
    }

    auto emit_expr_blob = [&]() {
        emitU32(1);
        emitByte(1);
    };

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_START));
    emitString(parser::v3::schemaPathToString(stmt->target_table, string_pool_));

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_SOURCE));
    if (stmt->source_query) {
        emitString("(subquery)");
    } else {
        emitString(parser::v3::schemaPathToString(stmt->source_table, string_pool_));
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_ON));
    emit_expr_blob();

    for (const auto& branch : stmt->when_matched) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_WHEN_MATCHED));
        emitByte(0);  // has_condition
        emitByte(branch.is_delete ? 1 : 0);
        emitU32(static_cast<uint32_t>(branch.assignments.size()));
        for (const auto& assignment : branch.assignments) {
            emitString(std::string(string_pool_.get(assignment.first)));
            emit_expr_blob();
        }
    }

    for (const auto& branch : stmt->when_not_matched) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_WHEN_NOT_MATCHED));
        emitByte(0);  // has_condition
        emitU32(static_cast<uint32_t>(branch.columns.size()));
        for (auto col : branch.columns) {
            emitString(std::string(string_pool_.get(col)));
        }
        for (size_t i = 0; i < branch.columns.size(); ++i) {
            emit_expr_blob();
        }
    }

    for (const auto& branch : stmt->when_not_matched_by_source) {
        (void)branch;
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_WHEN_NOT_MATCHED_SOURCE));
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MERGE_END));

    return stmt;
}

parser::v3::WithClause* Parser::parseWithClause() {
    consume(TokenType::KW_WITH, "Expected WITH");
    auto* with = arena()->create<parser::v3::WithClause>();
    with->recursive = matchKeyword(TokenType::KW_RECURSIVE);

    do {
        parser::v3::CTE cte;
        cte.name = parseIdentifierId();
        if (match(TokenType::LEFT_PAREN)) {
            do { cte.column_names.push_back(parseIdentifierId()); } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
        consumeKeyword(TokenType::KW_AS, "Expected AS");
        if (matchKeyword(TokenType::KW_MATERIALIZED)) {
            cte.materialized = true;
        } else if (matchKeyword(TokenType::KW_NOT)) {
            consumeKeyword(TokenType::KW_MATERIALIZED, "Expected MATERIALIZED");
            cte.not_materialized = true;
        }
        consume(TokenType::LEFT_PAREN, "Expected (");
        cte.query = parseSelectStmt();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        with->ctes.push_back(std::move(cte));
    } while (match(TokenType::COMMA));

    return with;
}

} // namespace scratchbird::parser::postgresql
