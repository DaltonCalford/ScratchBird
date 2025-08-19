#include "scratchbird/engine/parser_dml.h"

#include "scratchbird/engine/lexer.h"
#include "scratchbird/engine/parser_expr.h"

#include <string>
#include <vector>

namespace scratchbird::engine
{
    static std::vector<Token> lex(std::string_view s)
    {
        Lexer lx(s);
        return lx.lex();
    }

    static void push_warn(std::vector<std::string>& msgs, std::vector<SourceSpan>* spans,
                          const std::string& m, const Token* t = nullptr)
    {
        msgs.push_back(m);
        if (spans)
            spans->push_back(t ? t->span : SourceSpan{0, 0});
    }

    static bool eat_kw(const std::vector<Token>& t, size_t& i, const char* kw)
    {
        if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == kw) {
            ++i;
            return true;
        }
        return false;
    }

    static void parse_returning(const std::vector<Token>& t, size_t& i,
                                std::vector<std::string>& out)
    {
        if (eat_kw(t, i, "RETURNING")) {
            std::string item;
            for (; i < t.size() && t[i].kind != TokenKind::End; ++i) {
                if (t[i].kind == TokenKind::Symbol && t[i].text == ",") {
                    if (!item.empty()) {
                        out.push_back(item);
                        item.clear();
                    }
                    continue;
                }
                if (!item.empty())
                    item.push_back(' ');
                item += t[i].text;
            }
            if (!out.empty() || !item.empty()) {
                if (!item.empty())
                    out.push_back(item);
            }
        }
    }

    InsertStmt parse_insert_minimal(const std::string& sql)
    {
        auto t = lex(sql);
        InsertStmt st{};
        size_t i = 0;

        auto eat = [&](TokenKind k, const char* txt = nullptr) {
            if (i >= t.size() || t[i].kind != k)
                return false;
            if (txt && t[i].text != txt)
                return false;
            ++i;
            return true;
        };
        if (!eat(TokenKind::Keyword, "INSERT"))
            return st;
        if (!eat(TokenKind::Keyword, "INTO"))
            return st;
        if (i < t.size() &&
            (t[i].kind == TokenKind::Identifier || t[i].kind == TokenKind::QuotedIdentifier)) {
            st.target = t[i++].text;
            // Handle schema.table format
            if (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == ".") {
                ++i; // consume '.'
                if (i < t.size() && (t[i].kind == TokenKind::Identifier ||
                                     t[i].kind == TokenKind::QuotedIdentifier)) {
                    st.target += "." + t[i++].text;
                }
            }
        }
        // columns optional: (a, b)
        if (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == "(") {
            ++i;
            while (i < t.size() && !(t[i].kind == TokenKind::Symbol && t[i].text == ")")) {
                if (t[i].kind == TokenKind::Identifier || t[i].kind == TokenKind::QuotedIdentifier)
                    st.columns.push_back(t[i].text);
                ++i;
            }
            if (i < t.size())
                ++i;
        }
        // DEFAULT VALUES
        if (eat(TokenKind::Keyword, "DEFAULT") && eat(TokenKind::Keyword, "VALUES")) {
            st.default_values = true;
        } else if (eat(TokenKind::Keyword, "VALUES")) {
            // VALUES (..)[, (..)]*
            while (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == "(") {
                ++i;
                std::vector<std::string> tuple;
                while (i < t.size() && !(t[i].kind == TokenKind::Symbol && t[i].text == ")")) {
                    if (t[i].kind == TokenKind::Symbol && t[i].text == ",") {
                        ++i; // skip comma
                        continue;
                    }
                    tuple.push_back(t[i].text);
                    st.values.push_back(t[i].text); // keep flat for compat
                    ++i;
                }
                if (i < t.size())
                    ++i; // consume ')'
                st.values_tuples.push_back(std::move(tuple));
                if (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == ",") {
                    ++i; // next tuple
                    continue;
                }
                break;
            }
            if (!st.columns.empty() && !st.values_tuples.empty() &&
                st.columns.size() != st.values_tuples[0].size()) {
                push_warn(st.warnings, &st.warning_spans,
                          "INSERT column count does not match VALUES tuple length");
            }
        } else if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "SELECT") {
            // INSERT ... SELECT ... → capture raw SELECT tail
            std::string sel;
            while (i < t.size() && t[i].kind != TokenKind::End) {
                if (!sel.empty())
                    sel.push_back(' ');
                sel += t[i].text;
                ++i;
            }
            st.select_raw = sel;
        }
        // RETURNING list
        parse_returning(t, i, st.returning);
        st.has_returning = !st.returning.empty();
        return st;
    }

    ExecProcStmt parse_execproc_minimal(const std::string& sql)
    {
        auto t = lex(sql);
        ExecProcStmt st{};
        size_t i = 0;
        if (!(i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "EXECUTE"))
            return st;
        ++i;
        if (!(i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "PROCEDURE"))
            return st;
        ++i;
        if (i < t.size() &&
            (t[i].kind == TokenKind::Identifier || t[i].kind == TokenKind::QuotedIdentifier))
            st.name = t[i++].text;
        if (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == "(") {
            ++i;
            while (i < t.size() && !(t[i].kind == TokenKind::Symbol && t[i].text == ")")) {
                if (t[i].kind != TokenKind::Symbol || t[i].text != ",")
                    st.args.push_back(t[i].text);
                ++i;
            }
        }
        return st;
    }

    UpdateStmt parse_update_minimal(const std::string& sql)
    {
        auto t = lex(sql);
        UpdateStmt st{};
        size_t i = 0;
        auto eat = [&](TokenKind k, const char* txt = nullptr) {
            if (i >= t.size() || t[i].kind != k)
                return false;
            if (txt && t[i].text != txt)
                return false;
            ++i;
            return true;
        };
        if (!eat(TokenKind::Keyword, "UPDATE"))
            return st;
        if (i < t.size() &&
            (t[i].kind == TokenKind::Identifier || t[i].kind == TokenKind::QuotedIdentifier)) {
            st.target = t[i++].text;
            // Handle schema.table format
            if (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == ".") {
                ++i; // consume '.'
                if (i < t.size() && (t[i].kind == TokenKind::Identifier ||
                                     t[i].kind == TokenKind::QuotedIdentifier)) {
                    st.target += "." + t[i++].text;
                }
            }
        }
        if (!eat(TokenKind::Keyword, "SET"))
            return st;
        // parse name = value pairs until keyword
        while (i < t.size()) {
            if (t[i].kind == TokenKind::Keyword)
                break;
            std::string name, value;
            if (t[i].kind == TokenKind::Identifier || t[i].kind == TokenKind::QuotedIdentifier) {
                name = t[i].text;
                ++i;
            }
            if (eat(TokenKind::Symbol, "=")) {
                if (i < t.size()) {
                    value = t[i].text;
                    ++i;
                }
            }
            if (!name.empty())
                st.assignments.emplace_back(name, value);
            if (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == ",") {
                ++i;
                continue;
            } else if (i >= t.size() || t[i].kind == TokenKind::Keyword) {
                break;
            } else {
                // Skip unexpected tokens to avoid infinite loop
                ++i;
                continue;
            }
        }
        if (st.assignments.empty()) {
            push_warn(st.warnings, &st.warning_spans, "UPDATE has empty SET list");
        }
        // FROM clause (UPDATE ... FROM ...)
        if (eat_kw(t, i, "FROM")) {
            std::string fr;
            for (; i < t.size() && !(t[i].kind == TokenKind::Keyword &&
                                     (t[i].text == "WHERE" || t[i].text == "RETURNING"));
                 ++i) {
                if (!fr.empty())
                    fr.push_back(' ');
                fr += t[i].text;
            }
            st.from_raw = fr;
        }
        // WHERE normalization / WHERE CURRENT OF
        if (eat_kw(t, i, "WHERE")) {
            if (eat_kw(t, i, "CURRENT") && eat_kw(t, i, "OF")) {
                if (i < t.size() && (t[i].kind == TokenKind::Identifier ||
                                     t[i].kind == TokenKind::QuotedIdentifier))
                    st.where_current_cursor = t[i++].text;
            } else {
                std::string rest;
                for (; i < t.size() && t[i].kind != TokenKind::End; ++i) {
                    if (t[i].kind == TokenKind::Keyword && t[i].text == "RETURNING")
                        break;
                    if (!rest.empty())
                        rest.push_back(' ');
                    rest += t[i].text;
                }
                st.where_expr = parse_expression_to_string(rest);
            }
        }
        // RETURNING list
        parse_returning(t, i, st.returning);
        st.has_returning = !st.returning.empty();
        // FOR UPDATE presence
        for (; i < t.size(); ++i) {
            if (t[i].kind == TokenKind::Keyword && t[i].text == "FOR") {
                if (i + 1 < t.size() && t[i + 1].kind == TokenKind::Keyword &&
                    t[i + 1].text == "UPDATE")
                    st.has_for_update = true;
            }
        }
        return st;
    }

    DeleteStmt parse_delete_minimal(const std::string& sql)
    {
        auto t = lex(sql);
        DeleteStmt st{};
        size_t i = 0;
        auto eat = [&](TokenKind k, const char* txt = nullptr) {
            if (i >= t.size() || t[i].kind != k)
                return false;
            if (txt && t[i].text != txt)
                return false;
            ++i;
            return true;
        };
        if (!eat(TokenKind::Keyword, "DELETE"))
            return st;
        if (!eat(TokenKind::Keyword, "FROM"))
            return st;
        if (i < t.size() &&
            (t[i].kind == TokenKind::Identifier || t[i].kind == TokenKind::QuotedIdentifier)) {
            st.target = t[i++].text;
            // Handle schema.table format
            if (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == ".") {
                ++i; // consume '.'
                if (i < t.size() && (t[i].kind == TokenKind::Identifier ||
                                     t[i].kind == TokenKind::QuotedIdentifier)) {
                    st.target += "." + t[i++].text;
                }
            }
        }
        // USING clause (DELETE ... USING ...) with minimal validation stub
        if (eat_kw(t, i, "USING")) {
            std::string ur;
            bool saw_ident = false;
            for (; i < t.size() && !(t[i].kind == TokenKind::Keyword &&
                                     (t[i].text == "WHERE" || t[i].text == "RETURNING"));
                 ++i) {
                if (!ur.empty())
                    ur.push_back(' ');
                ur += t[i].text;
                if (t[i].kind == TokenKind::Identifier || t[i].kind == TokenKind::QuotedIdentifier)
                    saw_ident = true;
            }
            st.using_raw = ur;
            if (!saw_ident) {
                push_warn(st.warnings, &st.warning_spans, "DELETE USING has no identifiers");
            }
        }
        // WHERE normalization / WHERE CURRENT OF
        if (eat_kw(t, i, "WHERE")) {
            if (eat_kw(t, i, "CURRENT") && eat_kw(t, i, "OF")) {
                if (i < t.size() && (t[i].kind == TokenKind::Identifier ||
                                     t[i].kind == TokenKind::QuotedIdentifier))
                    st.where_current_cursor = t[i++].text;
            } else {
                std::string rest;
                for (; i < t.size() && t[i].kind != TokenKind::End; ++i) {
                    if (t[i].kind == TokenKind::Keyword && t[i].text == "RETURNING")
                        break;
                    if (!rest.empty())
                        rest.push_back(' ');
                    rest += t[i].text;
                }
                st.where_expr = parse_expression_to_string(rest);
            }
        }
        parse_returning(t, i, st.returning);
        st.has_returning = !st.returning.empty();
        return st;
    }

    MergeStmt parse_merge_minimal(const std::string& sql)
    {
        auto t = lex(sql);
        MergeStmt st{};
        size_t i = 0;
        auto is_ident = [&](size_t k) {
            return k < t.size() &&
                   (t[k].kind == TokenKind::Identifier || t[k].kind == TokenKind::QuotedIdentifier);
        };
        if (!(i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "MERGE"))
            return st;
        ++i;
        if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "INTO")
            ++i;
        if (is_ident(i)) {
            st.target = t[i].text;
            ++i;
        }
        if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "USING") {
            ++i;
            std::string src;
            while (i < t.size() && !(t[i].kind == TokenKind::Keyword && t[i].text == "ON")) {
                if (!src.empty())
                    src.push_back(' ');
                src += t[i].text;
                ++i;
            }
            st.using_source = src;
        }
        if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "ON") {
            ++i;
            std::string on;
            while (i < t.size() && !(t[i].kind == TokenKind::Keyword && (t[i].text == "WHEN"))) {
                if (!on.empty())
                    on.push_back(' ');
                on += t[i].text;
                ++i;
            }
            st.on_match = on;
        }
        // Structured actions
        auto read_until_kw = [&](std::string& out, const char* stop_kw1, const char* stop_kw2) {
            while (i < t.size()) {
                if (t[i].kind == TokenKind::Keyword &&
                    (t[i].text == stop_kw1 || t[i].text == stop_kw2))
                    break;
                if (!out.empty())
                    out.push_back(' ');
                out += t[i].text;
                ++i;
            }
        };
        while (i < t.size()) {
            if (!(t[i].kind == TokenKind::Keyword && t[i].text == "WHEN")) {
                ++i;
                continue;
            }
            ++i; // WHEN
            bool is_matched = false, is_not = false;
            if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "NOT") {
                is_not = true;
                ++i;
            }
            if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "MATCHED") {
                is_matched = true;
                ++i;
            }
            // Optional AND guard
            std::string guard;
            if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "AND") {
                ++i;
                read_until_kw(guard, "THEN", "THEN");
            }
            if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "THEN")
                ++i;
            // Action kind
            MergeAction action{};
            // DO NOTHING style
            if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "DO") {
                ++i;
                if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "NOTHING") {
                    ++i;
                    action.do_nothing = true;
                    action.kind = MergeAction::Kind::Update; // neutral
                    st.actions.push_back(std::move(action));
                    continue;
                }
            }
            if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "UPDATE") {
                action.kind = MergeAction::Kind::Update;
                ++i;
                if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "SET") {
                    ++i;
                    // parse name = value [, ...]
                    while (i < t.size() &&
                           !(t[i].kind == TokenKind::Keyword && t[i].text == "WHEN")) {
                        std::string name, value;
                        if (t[i].kind == TokenKind::Identifier ||
                            t[i].kind == TokenKind::QuotedIdentifier) {
                            name = t[i].text;
                            ++i;
                        }
                        if (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == "=") {
                            ++i;
                            if (i < t.size()) {
                                value = t[i].text;
                                ++i;
                            }
                        }
                        if (!name.empty())
                            action.set.emplace_back(name, value);
                        if (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == ",") {
                            ++i;
                            continue;
                        }
                        if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "WHEN")
                            break;
                    }
                }
            } else if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "DELETE") {
                action.kind = MergeAction::Kind::Delete;
                ++i;
            } else if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "INSERT") {
                action.kind = MergeAction::Kind::Insert;
                ++i;
                if (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == "(") {
                    ++i;
                    while (i < t.size() && !(t[i].kind == TokenKind::Symbol && t[i].text == ")")) {
                        if (t[i].kind != TokenKind::Symbol || t[i].text != ",")
                            action.insert_cols.push_back(t[i].text);
                        ++i;
                    }
                    if (i < t.size())
                        ++i;
                }
                if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "VALUES") {
                    ++i;
                    if (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == "(") {
                        ++i;
                        while (i < t.size() &&
                               !(t[i].kind == TokenKind::Symbol && t[i].text == ")")) {
                            if (t[i].kind != TokenKind::Symbol || t[i].text != ",")
                                action.insert_values.push_back(t[i].text);
                            ++i;
                        }
                        if (i < t.size())
                            ++i;
                    }
                }
            }
            action.guard = guard;
            st.actions.push_back(std::move(action));
        }
        return st;
    }

    UpsertStmt parse_upsert_minimal(const std::string& sql)
    {
        auto t = lex(sql);
        UpsertStmt st{};
        size_t i = 0;
        if (!(i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "UPDATE"))
            return st;
        ++i;
        if (!(i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "OR"))
            return st;
        ++i;
        if (!(i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "INSERT"))
            return st;
        ++i;
        if (!(i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "INTO"))
            return st;
        ++i;
        if (i < t.size() &&
            (t[i].kind == TokenKind::Identifier || t[i].kind == TokenKind::QuotedIdentifier))
            st.target = t[i++].text;
        // optional column list
        if (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == "(") {
            ++i;
            while (i < t.size() && !(t[i].kind == TokenKind::Symbol && t[i].text == ")")) {
                if (t[i].kind != TokenKind::Symbol || t[i].text != ",")
                    st.columns.push_back(t[i].text);
                ++i;
            }
            if (i < t.size())
                ++i;
        }
        // values
        if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "VALUES") {
            ++i;
            if (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == "(") {
                ++i;
                while (i < t.size() && !(t[i].kind == TokenKind::Symbol && t[i].text == ")")) {
                    if (t[i].kind != TokenKind::Symbol || t[i].text != ",")
                        st.values.push_back(t[i].text);
                    ++i;
                }
                if (i < t.size())
                    ++i;
            }
        }
        // MATCHING (...)
        if (i < t.size() && t[i].kind == TokenKind::Keyword && t[i].text == "MATCHING") {
            ++i;
            if (i < t.size() && t[i].kind == TokenKind::Symbol && t[i].text == "(") {
                ++i;
                while (i < t.size() && !(t[i].kind == TokenKind::Symbol && t[i].text == ")")) {
                    if (t[i].kind != TokenKind::Symbol || t[i].text != ",")
                        st.matching_cols.push_back(t[i].text);
                    ++i;
                }
            }
        }
        // Basic diagnostic: if columns are provided and matching cols are not subset (textual),
        // warn
        if (!st.columns.empty() && !st.matching_cols.empty()) {
            for (const auto& m : st.matching_cols) {
                bool found = false;
                for (const auto& c : st.columns)
                    if (c == m) {
                        found = true;
                        break;
                    }
                if (!found) {
                    push_warn(st.warnings, &st.warning_spans,
                              "UPSERT MATCHING column not in INSERT column list");
                    break;
                }
            }
        }
        return st;
    }
} // namespace scratchbird::engine
