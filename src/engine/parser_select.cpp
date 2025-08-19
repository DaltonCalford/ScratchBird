#include "scratchbird/engine/parser_select.h"

#include "scratchbird/engine/lexer.h"
#include "scratchbird/engine/parser_expr.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

namespace scratchbird::engine
{

    static void push_warning(struct SelectQuery& q, const std::string& msg)
    {
        q.warnings.push_back(msg);
        q.warning_spans.push_back(SourceSpan{0, 0});
    }

    static std::string trim(const std::string& s)
    {
        size_t a = 0, b = s.size();
        while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
            a++;
        while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
            b--;
        return s.substr(a, b - a);
    }

    static std::string lowercase(std::string s)
    {
        for (auto& c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    static bool debug_enabled()
    {
        static bool enabled = [] {
            const char* v = std::getenv("SB_DEBUG_SELECT");
            return v && *v;
        }();
        return enabled;
    }

    static bool is_integer_text(const std::string& t)
    {
        if (t.empty())
            return false;
        for (char c : t) {
            if (!std::isdigit(static_cast<unsigned char>(c)))
                return false;
        }
        return true;
    }

    static bool is_unsigned_integer_text(const std::string& t)
    {
        if (t.empty())
            return false;
        for (char c : t) {
            if (!std::isdigit(static_cast<unsigned char>(c)))
                return false;
        }
        return true;
    }

    // Helpers for set operations
    static bool is_setop_kw(const Token& t)
    {
        return t.kind == TokenKind::Keyword &&
               (t.text == "UNION" || t.text == "INTERSECT" || t.text == "EXCEPT");
    }

    static int setop_prec(const std::string& op)
    {
        if (op == "INTERSECT")
            return 2; // higher precedence
        // UNION and EXCEPT
        return 1;
    }

    static std::string join_tokens(const std::vector<Token>& toks, size_t a, size_t b)
    {
        std::string out;
        if (a > b || a >= toks.size())
            return out;
        for (size_t k = a; k <= b && k < toks.size(); ++k) {
            if (!out.empty())
                out.push_back(' ');
            out += toks[k].text;
        }
        return out;
    }

    static bool has_top_level_setop(const std::vector<Token>& toks)
    {
        int depth = 0;
        for (size_t k = 0; k < toks.size(); ++k) {
            const auto& t = toks[k];
            if (t.kind == TokenKind::Symbol && t.text == "(")
                depth++;
            else if (t.kind == TokenKind::Symbol && t.text == ")")
                depth = depth > 0 ? depth - 1 : 0;
            else if (depth == 0 && is_setop_kw(t))
                return true;
        }
        return false;
    }

    // Parse a parenthesized join group text into a right-deep JoinTree (best-effort)
    static std::unique_ptr<JoinTree> parse_join_group_text(const std::string& text)
    {
        Lexer lx(text);
        auto tokens = lx.lex();
        size_t i = 0;
        auto peek_kw = [&](const char* k) {
            return i < tokens.size() && tokens[i].kind == TokenKind::Keyword && tokens[i].text == k;
        };
        auto take_ident_like = [&]() -> std::string {
            if (i < tokens.size() && (tokens[i].kind == TokenKind::Identifier ||
                                      tokens[i].kind == TokenKind::QuotedIdentifier))
                return tokens[i++].text;
            return std::string();
        };
        std::function<std::unique_ptr<JoinTree>()> parsePrimary =
            [&]() -> std::unique_ptr<JoinTree> {
            auto node = std::make_unique<JoinTree>();
            node->is_leaf = true;
            FromItem fi{};
            bool lateral = false;
            if (peek_kw("LATERAL")) {
                ++i;
                lateral = true;
            }
            if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol && tokens[i].text == "(") {
                ++i;
                int d = 1;
                size_t start = i;
                size_t end = i;
                while (i < tokens.size() && d > 0) {
                    if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == "(")
                        ++d;
                    else if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")") {
                        --d;
                        if (d == 0) {
                            end = i;
                            ++i;
                            break;
                        }
                    }
                    ++i;
                }
                std::string body = join_tokens(tokens, start, end - 1);
                std::string lb = lowercase(body);
                if (lb.find("select ") != std::string::npos) {
                    fi.is_subquery = true;
                    fi.subquery = trim(body);
                } else {
                    // nested join group
                    auto sub = parse_join_group_text(body);
                    if (sub)
                        return sub;
                }
                // optional alias
                if (i < tokens.size() && (tokens[i].kind == TokenKind::Identifier ||
                                          tokens[i].kind == TokenKind::QuotedIdentifier)) {
                    fi.alias = tokens[i].text;
                    ++i;
                }
            } else {
                std::string ident = take_ident_like();
                if (!ident.empty()) {
                    // table or function
                    if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                        tokens[i].text == "(") {
                        int d = 0;
                        std::string args;
                        while (i < tokens.size()) {
                            if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == "(")
                                ++d;
                            else if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")") {
                                --d;
                                if (d == 0) {
                                    ++i;
                                    break;
                                }
                            }
                            if (!args.empty())
                                args.push_back(' ');
                            args += tokens[i].text;
                            ++i;
                        }
                        fi.is_subquery = true;
                        fi.subquery = ident + "(" + trim(args) + ")";
                    } else {
                        // support table@link notation
                        fi.table = ident;
                        if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                            tokens[i].text == "@") {
                            ++i;
                            std::string link = take_ident_like();
                            if (!link.empty())
                                fi.table += "@" + link;
                        }
                    }
                    // alias
                    if (i < tokens.size() && (tokens[i].kind == TokenKind::Identifier ||
                                              tokens[i].kind == TokenKind::QuotedIdentifier)) {
                        fi.alias = tokens[i].text;
                        ++i;
                    }
                }
            }
            fi.lateral = lateral;
            node->leaf = std::move(fi);
            return node;
        };
        auto root = parsePrimary();
        auto parse_join_type_local = [&]() -> JoinType {
            if (peek_kw("LEFT")) {
                ++i;
                return JoinType::Left;
            }
            if (peek_kw("RIGHT")) {
                ++i;
                return JoinType::Right;
            }
            if (peek_kw("FULL")) {
                ++i;
                return JoinType::Full;
            }
            if (peek_kw("CROSS")) {
                ++i;
                return JoinType::Cross;
            }
            if (peek_kw("NATURAL")) {
                ++i;
                return JoinType::Natural;
            }
            return JoinType::Inner;
        };
        while (i < tokens.size()) {
            size_t saved = i;
            JoinType jt = parse_join_type_local();
            if (!peek_kw("JOIN")) {
                i = saved;
                break;
            }
            ++i; // JOIN
            auto rhs = parsePrimary();
            if (!rhs)
                break;
            auto parent = std::make_unique<JoinTree>();
            parent->is_leaf = false;
            parent->type = jt;
            parent->left = std::move(root);
            parent->right = std::move(rhs);
            // optional ON/USING
            if (peek_kw("ON")) {
                ++i;
                size_t start = i;
                int d = 0;
                while (i < tokens.size()) {
                    if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == "(")
                        ++d;
                    else if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")") {
                        if (d > 0)
                            --d;
                    } else if (d == 0 &&
                               (tokens[i].kind == TokenKind::Keyword &&
                                (tokens[i].text == "JOIN" || tokens[i].text == "LEFT" ||
                                 tokens[i].text == "RIGHT" || tokens[i].text == "FULL" ||
                                 tokens[i].text == "CROSS" || tokens[i].text == "NATURAL"))) {
                        break;
                    }
                    ++i;
                }
                parent->has_on = true;
                parent->on_raw = join_tokens(tokens, start, i - 1);
            } else if (peek_kw("USING")) {
                ++i; // USING
                parent->has_using = true;
                if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                    tokens[i].text == "(") {
                    ++i;
                    while (i < tokens.size() &&
                           !(tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")")) {
                        if (tokens[i].kind == TokenKind::Identifier ||
                            tokens[i].kind == TokenKind::QuotedIdentifier)
                            parent->using_cols.push_back(tokens[i].text);
                        ++i;
                    }
                    if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                        tokens[i].text == ")")
                        ++i;
                }
            }
            root = std::move(parent);
        }
        return root;
    }

    static void parse_compound_tail(const std::vector<Token>& tokens, size_t& i, SetTree& root)
    {
        auto peek_kw = [&](const char* k) {
            return i < tokens.size() && tokens[i].kind == TokenKind::Keyword && tokens[i].text == k;
        };
        auto eat_int = [&]() -> int {
            if (i < tokens.size() && tokens[i].kind == TokenKind::Integer)
                return std::stoi(tokens[i++].text);
            return 0;
        };
        if (peek_kw("ORDER")) {
            ++i;
            if (peek_kw("BY"))
                ++i;
            while (i < tokens.size()) {
                OrderItem item{};
                std::string expr;
                while (i < tokens.size()) {
                    if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ",")
                        break;
                    if (tokens[i].kind == TokenKind::Keyword && (tokens[i].text == "FETCH"))
                        break;
                    if (tokens[i].kind == TokenKind::Keyword &&
                        (tokens[i].text == "ASC" || tokens[i].text == "DESC" ||
                         tokens[i].text == "NULLS"))
                        break;
                    if (!expr.empty())
                        expr.push_back(' ');
                    expr += tokens[i].text;
                    ++i;
                }
                if (!expr.empty() && is_integer_text(expr)) {
                    item.ordinal = std::stoi(expr);
                    expr.clear();
                }
                item.expression = trim(expr);
                if (peek_kw("ASC")) {
                    ++i;
                    item.ascending = true;
                } else if (peek_kw("DESC")) {
                    ++i;
                    item.ascending = false;
                }
                if (peek_kw("NULLS")) {
                    ++i;
                    if (peek_kw("FIRST")) {
                        ++i;
                        item.nulls = NullsOrder::First;
                    } else if (peek_kw("LAST")) {
                        ++i;
                        item.nulls = NullsOrder::Last;
                    }
                }
                root.order_by.push_back(item);
                if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                    tokens[i].text == ",") {
                    ++i;
                    continue;
                }
                break;
            }
            // (removed erroneous window inheritance in compound tail)
        }
        if (peek_kw("FETCH")) {
            ++i;
            if (peek_kw("FIRST") || peek_kw("NEXT"))
                ++i;
            root.fetch_n = eat_int();
            if (peek_kw("ROWS"))
                ++i;
            if (peek_kw("ONLY"))
                ++i;
        }
    }

    static void parse_plan_tokens(std::vector<Token> const& tokens, size_t& i, PlanNode& plan)
    {
        auto peek_kw = [&](const char* k) {
            return i < tokens.size() && tokens[i].kind == TokenKind::Keyword && tokens[i].text == k;
        };
        if (peek_kw("JOIN")) {
            plan.kind = "JOIN";
            ++i;
            if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol && tokens[i].text == "(") {
                ++i;
                while (i < tokens.size()) {
                    if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")") {
                        ++i;
                        break;
                    }
                    PlanOp op{};
                    // Expect relation alias/name
                    if (i < tokens.size() && (tokens[i].kind == TokenKind::Identifier ||
                                              tokens[i].kind == TokenKind::QuotedIdentifier)) {
                        op.relation = tokens[i].text;
                        ++i;
                    }
                    // NATURAL or INDEX(…)
                    if (peek_kw("NATURAL")) {
                        ++i;
                        op.method = "NATURAL";
                    } else if (peek_kw("INDEX")) {
                        ++i;
                        op.method = "INDEX";
                        if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                            tokens[i].text == "(") {
                            ++i;
                            while (i < tokens.size() && !(tokens[i].kind == TokenKind::Symbol &&
                                                          tokens[i].text == ")")) {
                                if (tokens[i].kind == TokenKind::Identifier ||
                                    tokens[i].kind == TokenKind::QuotedIdentifier)
                                    op.args.push_back(tokens[i].text);
                                ++i;
                            }
                            if (i < tokens.size())
                                ++i;
                        }
                    }
                    plan.ops.push_back(std::move(op));
                    if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                        tokens[i].text == ",") {
                        ++i;
                        continue;
                    }
                }
            }
        } else {
            // Single relation plan: PLAN R t NATURAL|INDEX(...)
            plan.kind = "SINGLE";
            if (peek_kw("R"))
                ++i; // Firebird syntax uses R for relation marker
            PlanOp op{};
            if (i < tokens.size() && (tokens[i].kind == TokenKind::Identifier ||
                                      tokens[i].kind == TokenKind::QuotedIdentifier)) {
                op.relation = tokens[i].text;
                ++i;
            }
            if (peek_kw("NATURAL")) {
                ++i;
                op.method = "NATURAL";
            } else if (peek_kw("INDEX")) {
                ++i;
                op.method = "INDEX";
                if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                    tokens[i].text == "(") {
                    ++i;
                    while (i < tokens.size() &&
                           !(tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")")) {
                        if (tokens[i].kind == TokenKind::Identifier ||
                            tokens[i].kind == TokenKind::QuotedIdentifier)
                            op.args.push_back(tokens[i].text);
                        ++i;
                    }
                    if (i < tokens.size())
                        ++i;
                }
            }
            plan.ops.push_back(std::move(op));
        }
    }

    SelectQuery parse_select_minimal(const std::string& sql)
    {
        Lexer lx(sql);
        // Debug: dump tokens for SELECT to diagnose FROM parsing
        auto tokens = lx.lex();
#ifdef DEBUG
        std::fprintf(stderr, "[PARSE SELECT] sql='%s' tokens=%zu\n", sql.c_str(), tokens.size());
        for (size_t ti = 0; ti < tokens.size(); ++ti) {
            std::fprintf(stderr, "  t[%zu]: kind=%d text='%s'\n", ti, (int)tokens[ti].kind,
                         tokens[ti].text.c_str());
        }
#endif

        if (has_top_level_setop(tokens)) {
            size_t i = 0;
            auto parse_set_term = [&](auto& self, size_t& idx) -> std::unique_ptr<SetTree> {
                if (idx < tokens.size() && tokens[idx].kind == TokenKind::Symbol &&
                    tokens[idx].text == "(") {
                    ++idx;
                    int depth = 1;
                    size_t start = idx;
                    while (idx < tokens.size() && depth > 0) {
                        if (tokens[idx].kind == TokenKind::Symbol && tokens[idx].text == "(")
                            depth++;
                        else if (tokens[idx].kind == TokenKind::Symbol && tokens[idx].text == ")")
                            depth--;
                        if (depth == 0)
                            break;
                        ++idx;
                    }
                    size_t end = (idx > start) ? idx - 1 : idx;
                    std::string inner_sql = join_tokens(tokens, start, end);
                    if (idx < tokens.size() && tokens[idx].kind == TokenKind::Symbol &&
                        tokens[idx].text == ")")
                        ++idx;
                    auto inner_q = std::make_unique<SelectQuery>(parse_select_minimal(inner_sql));
                    auto node = std::make_unique<SetTree>();
                    node->leaf = std::move(inner_q);
                    return node;
                }
                size_t start = idx;
                int depth = 0;
                while (idx < tokens.size()) {
                    const auto& t = tokens[idx];
                    if (t.kind == TokenKind::Symbol && t.text == "(")
                        depth++;
                    else if (t.kind == TokenKind::Symbol && t.text == ")")
                        depth = depth > 0 ? depth - 1 : 0;
                    else if (depth == 0 && is_setop_kw(t))
                        break;
                    ++idx;
                }
                size_t end = (idx > start) ? idx - 1 : start;
                std::string sub_sql = join_tokens(tokens, start, end);
                auto leaf_q = std::make_unique<SelectQuery>(parse_select_minimal(sub_sql));
                auto node = std::make_unique<SetTree>();
                node->leaf = std::move(leaf_q);
                return node;
            };
            auto parse_set_expr = [&](auto& self, size_t& idx,
                                      int min_bp) -> std::unique_ptr<SetTree> {
                auto left = parse_set_term(parse_set_term, idx);
                while (idx < tokens.size() && is_setop_kw(tokens[idx])) {
                    std::string op = tokens[idx].text;
                    int prec = setop_prec(op);
                    if (prec < min_bp)
                        break;
                    ++idx;
                    bool all = false;
                    if (idx < tokens.size() && tokens[idx].kind == TokenKind::Keyword &&
                        tokens[idx].text == "ALL") {
                        all = true;
                        ++idx;
                    }
                    int next_bp = prec + 1;
                    auto right = self(self, idx, next_bp);
                    auto parent = std::make_unique<SetTree>();
                    parent->op = op;
                    parent->all = all;
                    parent->left = std::move(left);
                    parent->right = std::move(right);
                    left = std::move(parent);
                }
                return left;
            };
            auto root_tree = parse_set_expr(parse_set_expr, i, 1);
            parse_compound_tail(tokens, i, *root_tree);
            SelectQuery root{};
            root.compound = std::move(root_tree);
            return root;
        }

        SelectQuery q{};
        size_t i = 0;
        auto eat = [&](TokenKind k, const char* text_upper = nullptr) -> bool {
            if (i >= tokens.size())
                return false;
            if (tokens[i].kind != k)
                return false;
            if (text_upper && tokens[i].text != text_upper)
                return false;
            ++i;
            return true;
        };
        if (!eat(TokenKind::Keyword, "SELECT"))
            return q;
        auto peek_kw = [&](const char* k) {
            return i < tokens.size() && tokens[i].kind == TokenKind::Keyword && tokens[i].text == k;
        };
        auto eat_int = [&]() -> int {
            if (i < tokens.size() && (tokens[i].kind == TokenKind::Integer))
                return std::stoi(tokens[i++].text);
            return 0;
        };
        if (peek_kw("DISTINCT")) {
            ++i;
            q.distinct = true;
            if (peek_kw("ON")) {
                ++i;
                if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                    tokens[i].text == "(") {
                    ++i;
                    std::string item;
                    while (i < tokens.size() &&
                           !(tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")")) {
                        if (!item.empty())
                            item.push_back(' ');
                        item += tokens[i].text;
                        if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ",") {
                            q.distinct_on.push_back(trim(item.substr(0, item.size() - 1)));
                            item.clear();
                        }
                        ++i;
                    }
                    if (!item.empty())
                        q.distinct_on.push_back(trim(item));
                    if (i < tokens.size())
                        ++i;
                }
            }
        }
        if (peek_kw("WITH")) {
            ++i;
            if (peek_kw("RECURSIVE")) {
                ++i;
                q.with_recursive = true;
            }
            while (i < tokens.size()) {
                CteDef def;
                if (i < tokens.size() && (tokens[i].kind == TokenKind::Identifier ||
                                          tokens[i].kind == TokenKind::QuotedIdentifier)) {
                    def.name = tokens[i].text;
                    ++i;
                }
                if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                    tokens[i].text == "(") {
                    ++i;
                    while (i < tokens.size() &&
                           !(tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")")) {
                        if (tokens[i].kind == TokenKind::Identifier ||
                            tokens[i].kind == TokenKind::QuotedIdentifier)
                            def.columns.push_back(tokens[i].text);
                        ++i;
                    }
                    if (i < tokens.size())
                        ++i;
                }
                if (peek_kw("AS"))
                    ++i;
                if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                    tokens[i].text == "(") {
                    ++i;
                    size_t body_start = i;
                    int depth = 1;
                    while (i < tokens.size() && depth > 0) {
                        if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == "(")
                            depth++;
                        else if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")") {
                            depth--;
                            if (depth == 0) {
                                ++i;
                                break;
                            }
                        } else {
                            ++i;
                        }
                    }
                    std::string body = join_tokens(tokens, body_start, i - 2);
                    SelectQuery body_q = parse_select_minimal(body);
                    if (!def.columns.empty() && !body_q.projections.empty() &&
                        static_cast<int>(def.columns.size()) !=
                            static_cast<int>(body_q.projections.size())) {
                        q.warnings.push_back(
                            "CTE column count does not match SELECT projection count");
                    }
                    def.body_raw = trim(body);
                }
                if (!def.name.empty())
                    q.ctes.push_back(def);
                if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                    tokens[i].text == ",") {
                    ++i;
                    continue;
                }
                if (i < tokens.size() && tokens[i].kind == TokenKind::Keyword &&
                    tokens[i].text == "SELECT")
                    break;
            }
        }
        if (peek_kw("SKIP")) {
            ++i;
            q.skip = eat_int();
        }
        if (peek_kw("FIRST")) {
            ++i;
            q.first = eat_int();
        }
        std::string proj;
        for (; i < tokens.size(); ++i) {
            if (tokens[i].kind == TokenKind::Keyword &&
                (tokens[i].text == "FROM" || tokens[i].text == "WHERE" ||
                 tokens[i].text == "GROUP" || tokens[i].text == "HAVING" ||
                 tokens[i].text == "ORDER" || tokens[i].text == "ROWS" || tokens[i].text == "PLAN"))
                break;
            if (tokens[i].kind == TokenKind::End)
                break;
            if (!proj.empty())
                proj.push_back(' ');
            proj += tokens[i].text;
            if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ",") {
                q.projections.push_back(trim(proj.substr(0, proj.size() - 1)));
                proj.clear();
            }
        }
        if (!proj.empty())
            q.projections.push_back(trim(proj));
        // Capture window function OVER(...) occurrences in projections and synthesize minimal
        // WindowSpec entries (best-effort, guarded)
        for (auto const& p : q.projections) {
            auto over_pos = p.find(" OVER ");
            if (over_pos != std::string::npos) {
                q.window_functions.push_back(p);
                // try to extract spec inside parentheses
                auto lp = p.find('(', over_pos);
                if (lp != std::string::npos) {
                    int depth = 0;
                    size_t rp = lp;
                    for (; rp < p.size(); ++rp) {
                        if (p[rp] == '(')
                            depth++;
                        else if (p[rp] == ')') {
                            depth--;
                            if (depth == 0) {
                                ++rp;
                                break;
                            }
                        }
                    }
                    if (depth == 0 && rp <= p.size()) {
                        std::string spec =
                            trim(p.substr(lp + 1, (rp - lp - 2) < p.size() ? rp - lp - 2 : 0));
                        if (!spec.empty()) {
                            WindowSpec wd{};
                            // partition/order/frame split similar to WINDOW clause
                            auto pos_part = spec.find("PARTITION BY");
                            auto pos_ob = spec.find("ORDER BY");
                            auto pos_rows = spec.find("ROWS");
                            auto pos_range = spec.find("RANGE");
                            size_t pos_frame = std::string::npos;
                            if (pos_rows != std::string::npos && pos_range != std::string::npos)
                                pos_frame = std::min(pos_rows, pos_range);
                            else if (pos_rows != std::string::npos)
                                pos_frame = pos_rows;
                            else if (pos_range != std::string::npos)
                                pos_frame = pos_range;
                            if (pos_frame != std::string::npos) {
                                std::string before = trim(spec.substr(0, pos_frame));
                                if (before.find("ORDER BY") != std::string::npos) {
                                    wd.order_by = trim(before.substr(before.find("ORDER BY") + 9));
                                    if (before.find("PARTITION BY") != std::string::npos) {
                                        wd.partition_by = trim(
                                            before.substr(before.find("PARTITION BY") + 12,
                                                          before.find("ORDER BY") -
                                                              (before.find("PARTITION BY") + 12)));
                                    }
                                } else if (before.find("PARTITION BY") != std::string::npos) {
                                    wd.partition_by =
                                        trim(before.substr(before.find("PARTITION BY") + 12));
                                }
                                if (spec.substr(pos_frame, 4) == "ROWS")
                                    wd.frame.unit = "ROWS";
                                else
                                    wd.frame.unit = "RANGE";
                                wd.frame.between = trim(spec.substr(pos_frame));
                                // parse simple PRECEDING/FOLLOWING directions for OVER inline
                                auto up = [](std::string s) {
                                    for (auto& c : s)
                                        c = (char)std::toupper((unsigned char)c);
                                    return s;
                                };
                                std::string sdir = up(wd.frame.between);
                                if (sdir.rfind("BETWEEN", 0) == 0) {
                                    sdir = trim(sdir.substr(7));
                                    size_t andp = sdir.find("AND");
                                    auto lhs = andp == std::string::npos
                                                   ? sdir
                                                   : trim(sdir.substr(0, andp));
                                    auto rhs = andp == std::string::npos
                                                   ? std::string()
                                                   : trim(sdir.substr(andp + 3));
                                    if (lhs.find("PRECEDING") != std::string::npos)
                                        wd.frame.start_dir = "preceding";
                                    else if (lhs.find("FOLLOWING") != std::string::npos)
                                        wd.frame.start_dir = "following";
                                    if (rhs.find("PRECEDING") != std::string::npos)
                                        wd.frame.end_dir = "preceding";
                                    else if (rhs.find("FOLLOWING") != std::string::npos)
                                        wd.frame.end_dir = "following";
                                }
                                if (wd.frame.unit == "RANGE" && wd.order_by.empty()) {
                                    q.warnings.push_back("RANGE frame used without ORDER BY; "
                                                         "semantics may be undefined");
                                }
                            } else {
                                if (pos_part != std::string::npos) {
                                    if (pos_ob != std::string::npos && pos_ob > pos_part)
                                        wd.partition_by = trim(
                                            spec.substr(pos_part + 12, pos_ob - (pos_part + 12)));
                                    else
                                        wd.partition_by = trim(spec.substr(pos_part + 12));
                                }
                                if (pos_ob != std::string::npos) {
                                    wd.order_by = trim(spec.substr(pos_ob + 9));
                                }
                            }
                            auto split_csv = [&](const std::string& txt) {
                                std::vector<std::string> out;
                                std::string cur;
                                int d = 0;
                                for (char c : txt) {
                                    if (c == '(')
                                        d++;
                                    else if (c == ')')
                                        d = d > 0 ? d - 1 : 0;
                                    if (c == ',' && d == 0) {
                                        auto a = cur.find_first_not_of(' ');
                                        auto b = cur.find_last_not_of(' ');
                                        if (a != std::string::npos)
                                            out.push_back(cur.substr(a, b - a + 1));
                                        cur.clear();
                                    } else
                                        cur.push_back(c);
                                }
                                if (!cur.empty()) {
                                    auto a = cur.find_first_not_of(' ');
                                    auto b = cur.find_last_not_of(' ');
                                    if (a != std::string::npos)
                                        out.push_back(cur.substr(a, b - a + 1));
                                }
                                return out;
                            };
                            if (!wd.partition_by.empty())
                                wd.partition_by_list = split_csv(wd.partition_by);
                            if (!wd.order_by.empty())
                                wd.order_by_list = split_csv(wd.order_by);
                            q.windows.emplace_back(std::move(wd));
                        }
                    }
                }
            }
        }
        if (i < tokens.size() && tokens[i].kind == TokenKind::Keyword && tokens[i].text == "FROM") {
            ++i;
            auto parseFromItem = [&]() {
                FromItem fi{};
                if (peek_kw("LATERAL")) {
                    ++i;
                    fi.lateral = true;
                }
                if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                    tokens[i].text == "(") {
                    ++i;
                    int depth = 1;
                    std::string body;
                    while (i < tokens.size() && depth > 0) {
                        if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == "(")
                            depth++;
                        else if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")") {
                            depth--;
                            if (depth == 0) {
                                ++i;
                                break;
                            }
                        } else {
                            if (!body.empty())
                                body.push_back(' ');
                            body += tokens[i].text;
                            ++i;
                        }
                    }
                    fi.is_subquery = true;
                    fi.subquery = trim(body);
                    if (i < tokens.size() && (tokens[i].kind == TokenKind::Identifier ||
                                              tokens[i].kind == TokenKind::QuotedIdentifier)) {
                        fi.alias = tokens[i].text;
                        ++i;
                    }
                    // If the parenthesized body looks like a SELECT, require alias as derived table
                    // Otherwise, treat as a parenthesized join nest and don't require alias
                    std::string lbody = lowercase(fi.subquery);
                    bool looks_select = lbody.find("select ") != std::string::npos;
                    bool looks_join = (lbody.find(" join ") != std::string::npos);
                    if (looks_select && fi.alias.empty()) {
                        q.ok = false;
                        q.error = "Derived table must have an alias";
                    } else if (!looks_select && looks_join && fi.alias.empty()) {
                        // Parenthesized join group without alias is acceptable; keep as-is
                    }
                } else {
                    if (i < tokens.size() && (tokens[i].kind == TokenKind::Identifier ||
                                              tokens[i].kind == TokenKind::QuotedIdentifier)) {
                        // table name (optionally schema-qualified) or table function
                        std::string ident = tokens[i].text;
                        ++i;
                        // Handle schema-qualified: schema . table
                        if (i + 1 < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                            tokens[i].text == "." &&
                            (tokens[i + 1].kind == TokenKind::Identifier ||
                             tokens[i + 1].kind == TokenKind::QuotedIdentifier)) {
                            ident += ".";
                            ident += tokens[i + 1].text;
                            i += 2;
                        }
                        if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                            tokens[i].text == "(") {
                            // table function: capture arguments (without parentheses)
                            ++i; // consume '('
                            int d2 = 1;
                            std::string args;
                            while (i < tokens.size() && d2 > 0) {
                                if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == "(") {
                                    ++d2;
                                } else if (tokens[i].kind == TokenKind::Symbol &&
                                           tokens[i].text == ")") {
                                    --d2;
                                    if (d2 == 0) {
                                        ++i;
                                        break;
                                    }
                                }
                                if (d2 > 0) {
                                    if (!(tokens[i].kind == TokenKind::Symbol &&
                                          (tokens[i].text == "(" || tokens[i].text == ")"))) {
                                        if (!args.empty())
                                            args.push_back(' ');
                                        args += tokens[i].text;
                                    }
                                    ++i;
                                }
                            }
                            fi.is_subquery = true; // treat like derived source for now
                            fi.subquery = ident + "(" + trim(args) + ")";
                        } else {
                            fi.table = ident;
                        }
                    }
                    if (i < tokens.size() && tokens[i].kind == TokenKind::Identifier) {
                        fi.alias = tokens[i].text;
                        ++i;
                    }
                    if (fi.lateral && !fi.is_subquery)
                        q.warnings.push_back("LATERAL used without a subquery or table function");
                }
                return fi;
            };
            if (i < tokens.size()) {
                auto fi = parseFromItem();
                q.from_table = fi.table;
                q.from_alias = fi.alias;
                q.from_items.push_back(std::move(fi));
                // Initialize join tree root with the first FROM item
                q.join_tree_root = std::make_unique<JoinTree>();
                q.join_tree_root->leaf = q.from_items.back();
                q.join_tree_root->is_leaf = true;
            }
            while (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                   tokens[i].text == ",") {
                ++i;
                q.from_items.push_back(parseFromItem());
            }
            auto parse_join_type = [&]() -> JoinType {
                if (peek_kw("LEFT")) {
                    ++i;
                    return JoinType::Left;
                } else if (peek_kw("RIGHT")) {
                    ++i;
                    return JoinType::Right;
                } else if (peek_kw("FULL")) {
                    ++i;
                    return JoinType::Full;
                } else if (peek_kw("CROSS")) {
                    ++i;
                    return JoinType::Cross;
                } else if (peek_kw("NATURAL")) {
                    ++i;
                    return JoinType::Natural;
                } else {
                    return JoinType::Inner;
                }
            };
            while (i < tokens.size()) {
                size_t saved = i;
                JoinType jt = parse_join_type();
                bool is_natural = (jt == JoinType::Natural);
                if (!peek_kw("JOIN")) {
                    i = saved;
                    break;
                }
                ++i;
                JoinClause jc{};
                jc.type = jt;
                // Parse joined RHS: (subquery|join-group) or table name or table function
                if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                    tokens[i].text == "(") {
                    ++i;
                    int d = 1;
                    std::string body;
                    while (i < tokens.size() && d > 0) {
                        if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == "(")
                            ++d;
                        else if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")") {
                            --d;
                            if (d == 0) {
                                ++i;
                                break;
                            }
                        }
                        if (d > 0) {
                            if (!body.empty())
                                body.push_back(' ');
                            body += tokens[i].text;
                            ++i;
                        }
                    }
                    jc.rhs_is_subquery = true;
                    jc.rhs_raw = trim(body);
                    // optional alias
                    if (i < tokens.size() && (tokens[i].kind == TokenKind::Identifier ||
                                              tokens[i].kind == TokenKind::QuotedIdentifier)) {
                        jc.alias = tokens[i].text;
                        ++i;
                    }
                } else if (i < tokens.size() && (tokens[i].kind == TokenKind::Identifier ||
                                                 tokens[i].kind == TokenKind::QuotedIdentifier)) {
                    std::string ident = tokens[i].text;
                    ++i;
                    if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                        tokens[i].text == "(") {
                        // table function
                        ++i;
                        int d = 1;
                        std::string args;
                        while (i < tokens.size() && d > 0) {
                            if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == "(")
                                ++d;
                            else if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")") {
                                --d;
                                if (d == 0) {
                                    ++i;
                                    break;
                                }
                            }
                            if (d > 0) {
                                if (!args.empty())
                                    args.push_back(' ');
                                args += tokens[i].text;
                                ++i;
                            }
                        }
                        jc.rhs_is_subquery = true;
                        jc.rhs_raw = ident + "(" + trim(args) + ")";
                        if (i < tokens.size() && (tokens[i].kind == TokenKind::Identifier ||
                                                  tokens[i].kind == TokenKind::QuotedIdentifier)) {
                            jc.alias = tokens[i].text;
                            ++i;
                        }
                    } else {
                        jc.table = ident;
                        // optional alias
                        if (i < tokens.size() && (tokens[i].kind == TokenKind::Identifier ||
                                                  tokens[i].kind == TokenKind::QuotedIdentifier)) {
                            jc.alias = tokens[i].text;
                            ++i;
                        }
                    }
                }
                if (peek_kw("USING")) {
                    ++i;
                    if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                        tokens[i].text == "(") {
                        ++i;
                        int d = 1;
                        std::string cols_raw;
                        while (i < tokens.size() && d > 0) {
                            if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == "(")
                                ++d;
                            else if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")") {
                                --d;
                                if (d == 0) {
                                    ++i;
                                    break;
                                }
                            }
                            if (d > 0) {
                                if (!cols_raw.empty())
                                    cols_raw.push_back(' ');
                                cols_raw += tokens[i].text;
                                ++i;
                            }
                        }
                        // split by commas at top level
                        int lvl = 0;
                        std::string cur;
                        for (char c : cols_raw) {
                            if (c == '(')
                                ++lvl;
                            else if (c == ')')
                                lvl = lvl > 0 ? lvl - 1 : 0;
                            if (c == ',' && lvl == 0) {
                                if (!trim(cur).empty())
                                    jc.using_cols.push_back(trim(cur));
                                cur.clear();
                            } else
                                cur.push_back(c);
                        }
                        if (!trim(cur).empty())
                            jc.using_cols.push_back(trim(cur));
                    }
                    if (jc.using_cols.empty())
                        q.warnings.push_back("USING has no columns");
                    else {
                        // Heuristic: if both FROM sides are simple names, warn if a column
                        // name appears only once across FROM items
                        if (q.from_items.size() >= 2) {
                            for (const auto& col : jc.using_cols) {
                                int count = 0;
                                for (const auto& fi : q.from_items) {
                                    if (!fi.table.empty() && fi.alias.empty()) {
                                        // simple identifier; we cannot resolve columns; emit hint
                                        // that validation is deferred but surface heuristic
                                        // This is only a hint; real name-resolution happens later
                                        count++;
                                    }
                                }
                                if (count < 2) {
                                    q.warnings.push_back(
                                        std::string("USING column '") + col +
                                        "' may not exist on both sides (heuristic)");
                                }
                            }
                        }
                    }
                } else if (peek_kw("ON")) {
                    ++i;
                    std::string onr;
                    int on_depth = 0;
                    while (i < tokens.size()) {
                        if (tokens[i].kind == TokenKind::Keyword &&
                            (tokens[i].text == "JOIN" || tokens[i].text == "WHERE" ||
                             tokens[i].text == "GROUP" || tokens[i].text == "HAVING" ||
                             tokens[i].text == "ORDER" || tokens[i].text == "ROWS" ||
                             tokens[i].text == "PLAN") &&
                            on_depth == 0)
                            break;
                        if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == "(")
                            ++on_depth;
                        else if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")")
                            on_depth = on_depth > 0 ? on_depth - 1 : 0;
                        if (!onr.empty())
                            onr.push_back(' ');
                        if (tokens[i].kind == TokenKind::End)
                            break;
                        onr += tokens[i].text;
                        ++i;
                    }
                    jc.on_raw = trim(onr);
                }
                if (is_natural)
                    q.warnings.push_back(
                        "NATURAL JOIN present; column alignment validation is deferred");
                q.joins.push_back(jc);
                // Extend structured join tree as right-deep
                if (q.join_tree_root) {
                    auto new_node = std::make_unique<JoinTree>();
                    new_node->is_leaf = false;
                    new_node->type = jt;
                    new_node->on_raw = jc.on_raw;
                    new_node->using_cols = jc.using_cols;
                    new_node->has_on = !jc.on_raw.empty();
                    new_node->has_using = !jc.using_cols.empty();
                    new_node->natural = (jt == JoinType::Natural);
                    new_node->left = std::move(q.join_tree_root);
                    // If rhs_raw looks like a join-group, parse it into a subtree; otherwise a leaf
                    if (jc.rhs_is_subquery) {
                        auto subtree = parse_join_group_text(jc.rhs_raw);
                        if (subtree) {
                            new_node->right = std::move(subtree);
                        } else {
                            auto right_leaf = std::make_unique<JoinTree>();
                            right_leaf->is_leaf = true;
                            FromItem rfi{};
                            rfi.is_subquery = true;
                            rfi.subquery = jc.rhs_raw;
                            rfi.alias = jc.alias;
                            right_leaf->leaf = std::move(rfi);
                            new_node->right = std::move(right_leaf);
                        }
                    } else {
                        auto right_leaf = std::make_unique<JoinTree>();
                        right_leaf->is_leaf = true;
                        FromItem rfi{};
                        rfi.table = jc.table;
                        rfi.alias = jc.alias;
                        right_leaf->leaf = std::move(rfi);
                        new_node->right = std::move(right_leaf);
                    }
                    q.join_tree_root = std::move(new_node);
                }
            }
        }
        if (peek_kw("WHERE")) {
            ++i;
            std::string rest;
            for (; i < tokens.size(); ++i) {
                if (tokens[i].kind == TokenKind::Keyword &&
                    (tokens[i].text == "GROUP" || tokens[i].text == "HAVING" ||
                     tokens[i].text == "ORDER" || tokens[i].text == "ROWS" ||
                     tokens[i].text == "PLAN"))
                    break;
                if (tokens[i].kind == TokenKind::End)
                    break;
                if (!rest.empty())
                    rest.push_back(' ');
                rest += tokens[i].text;
            }
            q.where_expr = parse_expression_to_string(trim(rest));
        }
        if (peek_kw("GROUP")) {
            ++i;
            if (peek_kw("BY"))
                ++i;
            std::string g;
            for (; i < tokens.size(); ++i) {
                if (tokens[i].kind == TokenKind::Keyword &&
                    (tokens[i].text == "HAVING" || tokens[i].text == "ORDER" ||
                     tokens[i].text == "ROWS" || tokens[i].text == "PLAN"))
                    break;
                if (tokens[i].kind == TokenKind::End)
                    break;
                if (!g.empty())
                    g.push_back(' ');
                g += tokens[i].text;
                if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ",") {
                    q.group_by.push_back(trim(g.substr(0, g.size() - 1)));
                    g.clear();
                }
            }
            if (!g.empty())
                q.group_by.push_back(trim(g));
        }
        if (peek_kw("HAVING")) {
            ++i;
            std::string rest;
            for (; i < tokens.size(); ++i) {
                if (tokens[i].kind == TokenKind::Keyword &&
                    (tokens[i].text == "ORDER" || tokens[i].text == "ROWS" ||
                     tokens[i].text == "PLAN"))
                    break;
                if (tokens[i].kind == TokenKind::End)
                    break;
                if (!rest.empty())
                    rest.push_back(' ');
                rest += tokens[i].text;
            }
            q.having_raw = parse_expression_to_string(trim(rest));
            if (q.group_by.empty())
                q.warnings.push_back("HAVING used without GROUP BY (informational)");
        }
        if (peek_kw("ORDER")) {
            ++i;
            if (peek_kw("BY"))
                ++i;
            while (i < tokens.size()) {
                OrderItem item{};
                std::string expr;
                if (i < tokens.size() && tokens[i].kind == TokenKind::Integer) {
                    expr = tokens[i].text;
                    ++i;
                    if (is_integer_text(expr)) {
                        item.ordinal = std::stoi(expr);
                        item.expression.clear();
                    } else {
                        item.expression = expr;
                    }
                } else {
                    while (i < tokens.size()) {
                        if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ",")
                            break;
                        if (tokens[i].kind == TokenKind::Keyword &&
                            (tokens[i].text == "ROWS" || tokens[i].text == "FETCH" ||
                             tokens[i].text == "FOR" || tokens[i].text == "PLAN"))
                            break;
                        if (tokens[i].kind == TokenKind::Keyword &&
                            (tokens[i].text == "ASC" || tokens[i].text == "DESC" ||
                             tokens[i].text == "NULLS"))
                            break;
                        if (!expr.empty())
                            expr.push_back(' ');
                        expr += tokens[i].text;
                        ++i;
                    }
                    item.expression = trim(expr);
                    if (item.expression.size() > 0 && is_integer_text(item.expression)) {
                        item.ordinal = std::stoi(item.expression);
                        item.expression.clear();
                    }
                }
                if (peek_kw("ASC")) {
                    ++i;
                    item.ascending = true;
                } else if (peek_kw("DESC")) {
                    ++i;
                    item.ascending = false;
                }
                if (peek_kw("NULLS")) {
                    ++i;
                    if (peek_kw("FIRST")) {
                        ++i;
                        item.nulls = NullsOrder::First;
                    } else if (peek_kw("LAST")) {
                        ++i;
                        item.nulls = NullsOrder::Last;
                    }
                }
                if (item.ordinal > 0) {
                    if (item.ordinal < 1 || item.ordinal > static_cast<int>(q.projections.size())) {
                        q.ok = false;
                        q.error = "ORDER BY ordinal out of range";
                    }
                }
                q.order_by.push_back(item);
                if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                    tokens[i].text == ",") {
                    ++i;
                    continue;
                }
                if (i >= tokens.size() || tokens[i].kind == TokenKind::End ||
                    (tokens[i].kind == TokenKind::Keyword &&
                     (tokens[i].text == "ROWS" || tokens[i].text == "FETCH" ||
                      tokens[i].text == "FOR" || tokens[i].text == "PLAN")))
                    break;
            }
        }
        if (q.distinct && !q.distinct_on.empty() && !q.order_by.empty()) {
            // Real prefix check: DISTINCT ON list must match the first N ORDER BY expressions
            // (ignoring ordinals)
            bool prefix = true;
            if (q.distinct_on.size() > q.order_by.size()) {
                prefix = false;
            } else {
                for (size_t k = 0; k < q.distinct_on.size(); ++k) {
                    const auto& dobj = trim(q.distinct_on[k]);
                    const auto& ob = q.order_by[k];
                    std::string ob_expr = ob.expression;
                    if (ob.ordinal > 0 && ob.ordinal <= (int)q.projections.size()) {
                        ob_expr = trim(q.projections[ob.ordinal - 1]);
                    }
                    // alias-awareness: if ORDER BY references an alias, try to match against
                    // projection text prefix before AS
                    if (!ob_expr.empty() && ob_expr.find(' ') == std::string::npos) {
                        for (const auto& proj : q.projections) {
                            auto aspos = proj.find(" AS ");
                            if (aspos != std::string::npos) {
                                auto alias = trim(proj.substr(aspos + 4));
                                if (alias == ob_expr) {
                                    ob_expr = trim(proj.substr(0, aspos));
                                    break;
                                }
                            }
                        }
                    }
                    if (trim(ob_expr) != dobj) {
                        prefix = false;
                        break;
                    }
                }
            }
            if (!prefix)
                q.warnings.push_back("DISTINCT ON list is not a prefix of ORDER BY");
        }
        // Validate ORDER BY expressions referencing unknown aliases
        for (const auto& ob : q.order_by) {
            if (!ob.expression.empty() && ob.expression.find(' ') == std::string::npos &&
                ob.ordinal == 0) {
                bool matched_alias = false;
                for (const auto& proj : q.projections) {
                    auto aspos = proj.find(" AS ");
                    if (aspos != std::string::npos) {
                        auto alias = trim(proj.substr(aspos + 4));
                        if (alias == ob.expression) {
                            matched_alias = true;
                            break;
                        }
                    }
                }
                if (!matched_alias) {
                    // Could still be a plain expression; leave as informational hint
                    // We avoid flipping ok=false here.
                }
            }
        }
        if (peek_kw("ROWS")) {
            ++i;
            q.rows_from = eat_int();
            if (peek_kw("TO")) {
                ++i;
                q.rows_to = eat_int();
            }
        }
        if (peek_kw("FETCH")) {
            ++i;
            if (peek_kw("FIRST") || peek_kw("NEXT"))
                ++i;
            q.fetch_n = eat_int();
            if (peek_kw("ROWS"))
                ++i;
            if (peek_kw("ONLY"))
                ++i;
        }
        if (peek_kw("FOR")) {
            size_t save = i;
            ++i;
            if (peek_kw("UPDATE")) {
                q.for_update.enabled = true;
                ++i;
                if (peek_kw("OF")) {
                    ++i;
                    while (i < tokens.size() &&
                           !(tokens[i].kind == TokenKind::Keyword &&
                             (tokens[i].text == "NOWAIT" ||
                              (tokens[i].text == "SKIP" && i + 1 < tokens.size() &&
                               tokens[i + 1].kind == TokenKind::Keyword &&
                               tokens[i + 1].text == "LOCKED") ||
                              tokens[i].text == "PLAN"))) {
                        if (tokens[i].kind == TokenKind::Identifier ||
                            tokens[i].kind == TokenKind::QuotedIdentifier)
                            q.for_update.of_columns.push_back(tokens[i].text);
                        ++i;
                    }
                }
                if (peek_kw("NOWAIT")) {
                    ++i;
                    q.for_update.nowait = true;
                }
                if (peek_kw("SKIP")) {
                    ++i;
                    if (peek_kw("LOCKED")) {
                        ++i;
                        q.for_update.skip_locked = true;
                    }
                }
            } else {
                i = save;
            }
        }
        if (peek_kw("PLAN")) {
            ++i;
            // collect raw PLAN
            std::string pr;
            size_t start_i = i;
            while (i < tokens.size() && tokens[i].kind != TokenKind::End) {
                if (!pr.empty())
                    pr.push_back(' ');
                pr += tokens[i].text;
                ++i;
            }
            q.plan_raw = trim(pr);
            // parse structured plan tokens from start_i..end
            PlanNode pn{};
            size_t j = start_i;
            // Recursive descent: allow JOIN(...), NESTED( ... ), MERGE( ... ), SORT( ... )
            auto parse_plan_rd = [&](auto& self, size_t& idx) -> PlanNode {
                PlanNode node{};
                auto is_kw = [&](const char* k) {
                    return idx < tokens.size() && tokens[idx].kind == TokenKind::Keyword &&
                           tokens[idx].text == k;
                };
                if (is_kw("JOIN") || is_kw("NESTED") || is_kw("MERGE") || is_kw("SORT") ||
                    is_kw("HASH") || is_kw("UNION")) {
                    node.kind = tokens[idx].text;
                    ++idx;
                    if (idx < tokens.size() && tokens[idx].kind == TokenKind::Symbol &&
                        tokens[idx].text == "(") {
                        ++idx;
                        while (idx < tokens.size()) {
                            if (tokens[idx].kind == TokenKind::Symbol && tokens[idx].text == ")") {
                                ++idx;
                                break;
                            }
                            if (tokens[idx].kind == TokenKind::Keyword &&
                                (tokens[idx].text == "JOIN" || tokens[idx].text == "NESTED" ||
                                 tokens[idx].text == "MERGE" || tokens[idx].text == "SORT" ||
                                 tokens[idx].text == "HASH" || tokens[idx].text == "UNION")) {
                                node.subplans.push_back(self(self, idx));
                                if (idx < tokens.size() && tokens[idx].kind == TokenKind::Symbol &&
                                    tokens[idx].text == ",")
                                    ++idx;
                                continue;
                            }
                            // otherwise parse a PlanOp (relation access)
                            PlanOp op{};
                            // Optional relation marker 'R'
                            if (idx < tokens.size() && (tokens[idx].text == "R")) {
                                ++idx;
                            }
                            if (idx < tokens.size() &&
                                (tokens[idx].kind == TokenKind::Identifier ||
                                 tokens[idx].kind == TokenKind::QuotedIdentifier)) {
                                op.relation = tokens[idx].text;
                                ++idx;
                            }
                            if (idx < tokens.size() && tokens[idx].kind == TokenKind::Keyword &&
                                tokens[idx].text == "NATURAL") {
                                ++idx;
                                op.method = "NATURAL";
                            } else if (idx < tokens.size() &&
                                       tokens[idx].kind == TokenKind::Keyword &&
                                       tokens[idx].text == "INDEX") {
                                ++idx;
                                op.method = "INDEX";
                                if (idx < tokens.size() && tokens[idx].kind == TokenKind::Symbol &&
                                    tokens[idx].text == "(") {
                                    ++idx;
                                    while (idx < tokens.size() &&
                                           !(tokens[idx].kind == TokenKind::Symbol &&
                                             tokens[idx].text == ")")) {
                                        if (tokens[idx].kind == TokenKind::Identifier ||
                                            tokens[idx].kind == TokenKind::QuotedIdentifier)
                                            op.args.push_back(tokens[idx].text);
                                        ++idx;
                                    }
                                    if (idx < tokens.size())
                                        ++idx;
                                }
                                // Optional index type hint after INDEX(...)
                                auto up = [&](std::string s) {
                                    for (auto& c : s)
                                        c = (char)std::toupper((unsigned char)c);
                                    return s;
                                };
                                if (idx < tokens.size() &&
                                    tokens[idx].kind == TokenKind::Identifier) {
                                    std::string t = up(tokens[idx].text);
                                    if (t == "HASH" || t == "BITMAP" || t == "CLUSTERED" ||
                                        t == "NON-CLUSTERED" || t == "NONCLUSTERED" ||
                                        t == "FULLTEXT" || t == "FULL-TEXT" || t == "MULTIKEY" ||
                                        t == "MULTI-KEY" || t == "GEOSPATIAL" || t == "INVERTED" ||
                                        t == "BTREE" || t == "B-TREE") {
                                        op.index_type = t;
                                        ++idx;
                                    }
                                }
                            } else if (idx < tokens.size() &&
                                       tokens[idx].kind == TokenKind::Keyword &&
                                       tokens[idx].text == "ORDER") {
                                ++idx;
                                op.method = "ORDER";
                                if (idx < tokens.size() &&
                                    (tokens[idx].kind == TokenKind::Identifier ||
                                     tokens[idx].kind == TokenKind::QuotedIdentifier)) {
                                    op.order_index = tokens[idx].text;
                                    ++idx;
                                }
                                if (idx < tokens.size() && tokens[idx].kind == TokenKind::Keyword &&
                                    tokens[idx].text == "INDEX") {
                                    ++idx;
                                    if (idx < tokens.size() &&
                                        tokens[idx].kind == TokenKind::Symbol &&
                                        tokens[idx].text == "(") {
                                        ++idx;
                                        while (idx < tokens.size() &&
                                               !(tokens[idx].kind == TokenKind::Symbol &&
                                                 tokens[idx].text == ")")) {
                                            if (tokens[idx].kind == TokenKind::Identifier ||
                                                tokens[idx].kind == TokenKind::QuotedIdentifier)
                                                op.args.push_back(tokens[idx].text);
                                            ++idx;
                                        }
                                        if (idx < tokens.size())
                                            ++idx;
                                    }
                                    // Optional index type after ORDER ... INDEX(...)
                                    auto up = [&](std::string s) {
                                        for (auto& c : s)
                                            c = (char)std::toupper((unsigned char)c);
                                        return s;
                                    };
                                    if (idx < tokens.size() &&
                                        tokens[idx].kind == TokenKind::Identifier) {
                                        std::string t = up(tokens[idx].text);
                                        if (t == "HASH" || t == "BITMAP" || t == "CLUSTERED" ||
                                            t == "NON-CLUSTERED" || t == "NONCLUSTERED" ||
                                            t == "FULLTEXT" || t == "FULL-TEXT" ||
                                            t == "MULTIKEY" || t == "MULTI-KEY" ||
                                            t == "GEOSPATIAL" || t == "INVERTED" || t == "BTREE" ||
                                            t == "B-TREE") {
                                            op.index_type = t;
                                            ++idx;
                                        }
                                    }
                                }
                                // Also allow ORDER with plain type hint
                                if (idx < tokens.size() &&
                                    tokens[idx].kind == TokenKind::Identifier) {
                                    auto up2 = [&](std::string s) {
                                        for (auto& c : s)
                                            c = (char)std::toupper((unsigned char)c);
                                        return s;
                                    };
                                    std::string t = up2(tokens[idx].text);
                                    if (t == "HASH" || t == "BITMAP" || t == "CLUSTERED" ||
                                        t == "NON-CLUSTERED" || t == "NONCLUSTERED" ||
                                        t == "FULLTEXT" || t == "FULL-TEXT" || t == "MULTIKEY" ||
                                        t == "MULTI-KEY" || t == "GEOSPATIAL" || t == "INVERTED" ||
                                        t == "BTREE" || t == "B-TREE") {
                                        op.index_type = t;
                                        ++idx;
                                    }
                                }
                            } else if (idx < tokens.size() &&
                                       (tokens[idx].kind == TokenKind::Identifier)) {
                                // Generic trailing hints after relation
                                auto up3 = [&](std::string s) {
                                    for (auto& c : s)
                                        c = (char)std::toupper((unsigned char)c);
                                    return s;
                                };
                                std::string t = up3(tokens[idx].text);
                                if (t == "HASH" || t == "BITMAP" || t == "CLUSTERED" ||
                                    t == "NON-CLUSTERED" || t == "NONCLUSTERED" ||
                                    t == "FULLTEXT" || t == "FULL-TEXT" || t == "MULTIKEY" ||
                                    t == "MULTI-KEY" || t == "GEOSPATIAL" || t == "INVERTED" ||
                                    t == "BTREE" || t == "B-TREE") {
                                    op.index_type = t;
                                    ++idx;
                                } else {
                                    // arbitrary hint token, collect and advance
                                    op.hints.push_back(tokens[idx].text);
                                    ++idx;
                                }
                            }
                            node.ops.push_back(std::move(op));
                            if (idx < tokens.size() && tokens[idx].kind == TokenKind::Symbol &&
                                tokens[idx].text == ",") {
                                ++idx;
                                continue;
                            }
                        }
                    }
                    return node;
                }
                // Fallback to existing simple parser for SINGLE/JOIN
                parse_plan_tokens(tokens, idx, node);
                return node;
            };
            PlanNode built = parse_plan_rd(parse_plan_rd, j);
            q.plan = std::make_unique<PlanNode>(std::move(built));
        }
        if (peek_kw("WINDOW")) {
            ++i;
            auto is_major_clause = [&](const Token& t) {
                return t.kind == TokenKind::Keyword &&
                       (t.text == "WHERE" || t.text == "GROUP" || t.text == "HAVING" ||
                        t.text == "ORDER" || t.text == "ROWS" || t.text == "PLAN" ||
                        t.text == "FETCH" || t.text == "FOR");
            };
            int safety_defs = 0;
            while (i < tokens.size() && tokens[i].kind != TokenKind::End &&
                   !is_major_clause(tokens[i]) && safety_defs++ < 64) {
                if (debug_enabled())
                    std::fprintf(stderr, "[WINDOW] def begin at token %zu/%zu\n", i, tokens.size());
                WindowSpec wd{};
                if (debug_enabled())
                    std::fprintf(stderr, "[WINDOW] def constructed\n");
                if (i < tokens.size() && (tokens[i].kind == TokenKind::Identifier ||
                                          tokens[i].kind == TokenKind::QuotedIdentifier)) {
                    wd.name = tokens[i].text;
                    ++i;
                }
                if (peek_kw("AS"))
                    ++i;
                if (!(i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                      tokens[i].text == "(")) {
                    // invalid def: fast-forward to next clause
                    while (i < tokens.size() && tokens[i].kind != TokenKind::End &&
                           !is_major_clause(tokens[i]))
                        ++i;
                    break;
                }
                ++i;
                int depth = 1;
                int safety_inner = 0;
                std::string spec;
                while (i < tokens.size() && depth > 0 && safety_inner++ < 4096) {
                    if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == "(")
                        ++depth;
                    else if (tokens[i].kind == TokenKind::Symbol && tokens[i].text == ")") {
                        --depth;
                        if (depth == 0) {
                            ++i;
                            break;
                        }
                    } else if (tokens[i].kind == TokenKind::End) {
                        break;
                    } else {
                        if (!spec.empty())
                            spec.push_back(' ');
                        spec += tokens[i].text;
                        ++i;
                    }
                }
                if (depth != 0) {
                    // unbalanced; abandon WINDOW entirely
                    while (i < tokens.size() && tokens[i].kind != TokenKind::End &&
                           !is_major_clause(tokens[i]))
                        ++i;
                    break;
                }
                std::string spec_trim = trim(spec);
                if (spec_trim.empty()) {
                    // keep empty; just skip
                } else {
                    // Named reference: a single identifier inside parens
                    bool single_ident = true;
                    for (char c : spec_trim) {
                        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$' ||
                              c == '.')) {
                            single_ident = false;
                            break;
                        }
                    }
                    if (single_ident) {
                        wd.ref_name = spec_trim;
                    } else {
                        auto pos_part = spec_trim.find("PARTITION BY");
                        auto pos_ob = spec_trim.find("ORDER BY");
                        auto pos_rows = spec_trim.find("ROWS");
                        auto pos_range = spec_trim.find("RANGE");
                        size_t pos_frame = std::string::npos;
                        if (pos_rows != std::string::npos && pos_range != std::string::npos)
                            pos_frame = std::min(pos_rows, pos_range);
                        else if (pos_rows != std::string::npos)
                            pos_frame = pos_rows;
                        else if (pos_range != std::string::npos)
                            pos_frame = pos_range;
                        if (pos_frame != std::string::npos) {
                            std::string before = trim(spec_trim.substr(0, pos_frame));
                            if (before.find("ORDER BY") != std::string::npos) {
                                wd.order_by = trim(before.substr(before.find("ORDER BY") + 9));
                                if (before.find("PARTITION BY") != std::string::npos) {
                                    wd.partition_by =
                                        trim(before.substr(before.find("PARTITION BY") + 12,
                                                           before.find("ORDER BY") -
                                                               (before.find("PARTITION BY") + 12)));
                                }
                            } else if (before.find("PARTITION BY") != std::string::npos) {
                                wd.partition_by =
                                    trim(before.substr(before.find("PARTITION BY") + 12));
                            }
                            if (spec_trim.substr(pos_frame, 4) == "ROWS")
                                wd.frame.unit = "ROWS";
                            else
                                wd.frame.unit = "RANGE";
                            wd.frame.between = trim(spec_trim.substr(pos_frame));
                            // Parse BETWEEN/UNBOUNDED/CURRENT ROW into start/end kinds
                            auto parse_bounds = [&](const std::string& ft) {
                                auto up = [](std::string s) {
                                    for (auto& c : s)
                                        c = static_cast<char>(
                                            std::toupper(static_cast<unsigned char>(c)));
                                    return s;
                                };
                                std::string s = up(ft);
                                // Trim leading unit
                                if (s.rfind("ROWS", 0) == 0)
                                    s = trim(s.substr(4));
                                else if (s.rfind("RANGE", 0) == 0)
                                    s = trim(s.substr(5));
                                if (s.rfind("BETWEEN", 0) == 0) {
                                    s = trim(s.substr(7));
                                    size_t and_pos = s.find("AND");
                                    std::string lhs = and_pos == std::string::npos
                                                          ? s
                                                          : trim(s.substr(0, and_pos));
                                    std::string rhs = and_pos == std::string::npos
                                                          ? std::string()
                                                          : trim(s.substr(and_pos + 3));
                                    auto classify = [](const std::string& side, std::string& kind,
                                                       std::string& expr, std::string& dir) {
                                        if (side.empty()) {
                                            kind = "none";
                                            expr.clear();
                                            dir.clear();
                                            return;
                                        }
                                        if (side.rfind("UNBOUNDED", 0) == 0) {
                                            kind = "unbounded";
                                            expr.clear();
                                            dir.clear();
                                            return;
                                        }
                                        if (side.rfind("CURRENT ROW", 0) == 0) {
                                            kind = "current";
                                            expr.clear();
                                            dir.clear();
                                            return;
                                        }
                                        kind = "expr";
                                        // capture optional direction keyword
                                        if (side.find("PRECEDING") != std::string::npos) {
                                            dir = "preceding";
                                            expr = trim(side.substr(0, side.find("PRECEDING")));
                                        } else if (side.find("FOLLOWING") != std::string::npos) {
                                            dir = "following";
                                            expr = trim(side.substr(0, side.find("FOLLOWING")));
                                        } else {
                                            dir.clear();
                                            expr = side;
                                        }
                                    };
                                    classify(lhs, wd.frame.start_kind, wd.frame.start_expr,
                                             wd.frame.start_dir);
                                    classify(rhs, wd.frame.end_kind, wd.frame.end_expr,
                                             wd.frame.end_dir);
                                } else if (!s.empty()) {
                                    if (s.rfind("UNBOUNDED", 0) == 0) {
                                        wd.frame.start_kind = "unbounded";
                                        wd.frame.end_kind = "none";
                                        wd.frame.start_dir.clear();
                                        wd.frame.end_dir.clear();
                                    } else if (s.rfind("CURRENT ROW", 0) == 0) {
                                        wd.frame.start_kind = "current";
                                        wd.frame.end_kind = "none";
                                        wd.frame.start_dir.clear();
                                        wd.frame.end_dir.clear();
                                    } else {
                                        wd.frame.start_kind = "expr";
                                        wd.frame.start_expr = s;
                                        wd.frame.end_kind = "none";
                                        wd.frame.end_expr.clear();
                                        wd.frame.start_dir.clear();
                                        wd.frame.end_dir.clear();
                                    }
                                }
                            };
                            parse_bounds(wd.frame.between);
                            if (wd.frame.unit == "RANGE" && wd.order_by.empty()) {
                                push_warning(q, "WINDOW RANGE frame without ORDER BY");
                            }
                            // Lightweight validation: direction ordering and numeric hints
                            if (wd.frame.start_kind == "expr" && wd.frame.end_kind == "expr") {
                                if (wd.frame.start_dir == "following" &&
                                    wd.frame.end_dir == "preceding") {
                                    push_warning(q, "WINDOW frame FOLLOWING..PRECEDING unusual");
                                }
                                // Heuristic: if start/end exprs look like integers, note if
                                // negative
                                auto looks_int = [&](const std::string& s) {
                                    std::string t = trim(s);
                                    if (!t.empty() && (t[0] == '+' || t[0] == '-'))
                                        t = t.substr(1);
                                    return is_unsigned_integer_text(t);
                                };
                                if (!wd.frame.start_expr.empty() &&
                                    looks_int(wd.frame.start_expr)) {
                                    if (!wd.frame.start_expr.empty() &&
                                        wd.frame.start_expr[0] == '-')
                                        push_warning(q, "WINDOW frame start negative bound");
                                }
                                if (!wd.frame.end_expr.empty() && looks_int(wd.frame.end_expr)) {
                                    if (!wd.frame.end_expr.empty() && wd.frame.end_expr[0] == '-')
                                        push_warning(q, "WINDOW frame end negative bound");
                                }
                            }
                        } else {
                            if (pos_part != std::string::npos) {
                                if (pos_ob != std::string::npos && pos_ob > pos_part)
                                    wd.partition_by = trim(
                                        spec_trim.substr(pos_part + 12, pos_ob - (pos_part + 12)));
                                else
                                    wd.partition_by = trim(spec_trim.substr(pos_part + 12));
                            }
                            if (pos_ob != std::string::npos) {
                                wd.order_by = trim(spec_trim.substr(pos_ob + 9));
                            }
                        }
                        auto split_csv = [&](const std::string& txt) {
                            std::vector<std::string> out;
                            std::string cur;
                            int d = 0;
                            for (char c : txt) {
                                if (c == '(')
                                    d++;
                                else if (c == ')')
                                    d = d > 0 ? d - 1 : 0;
                                if (c == ',' && d == 0) {
                                    auto a = cur.find_first_not_of(' ');
                                    auto b = cur.find_last_not_of(' ');
                                    if (a != std::string::npos)
                                        out.push_back(cur.substr(a, b - a + 1));
                                    cur.clear();
                                } else
                                    cur.push_back(c);
                            }
                            if (!cur.empty()) {
                                auto a = cur.find_first_not_of(' ');
                                auto b = cur.find_last_not_of(' ');
                                if (a != std::string::npos)
                                    out.push_back(cur.substr(a, b - a + 1));
                            }
                            return out;
                        };
                        if (!wd.partition_by.empty())
                            wd.partition_by_list = split_csv(wd.partition_by);
                        if (!wd.order_by.empty())
                            wd.order_by_list = split_csv(wd.order_by);
                    }
                }
                if (debug_enabled())
                    std::fprintf(stderr,
                                 "[WINDOW] emplace: name='%s' ref='%s' unit='%s' between='%s'\n",
                                 wd.name.c_str(), wd.ref_name.c_str(), wd.frame.unit.c_str(),
                                 wd.frame.between.c_str());
                q.windows.emplace_back(std::move(wd));
                if (i < tokens.size() && tokens[i].kind == TokenKind::Symbol &&
                    tokens[i].text == ",") {
                    ++i; // next window def
                    continue;
                }
                break;
            }
            if (debug_enabled())
                std::fprintf(stderr, "[WINDOW] clause end, windows=%zu\n", q.windows.size());
        }
        return q;
    }

    std::string format_select(const SelectQuery& q)
    {
        std::string sig = "SELECT";
        if (!q.projections.empty()) {
            sig += " ";
            sig += q.projections[0];
            if (q.projections.size() > 1)
                sig += ", ...";
        }
        return sig;
    }

    static std::string format_set_tree_impl(const SetTree& node, int depth, int max_depth)
    {
        if (depth > max_depth)
            return "…";
        // Guard against UB if node references freed memory in odd teardown orders
        // by avoiding deep recursion when children are missing
        if (!node.op.empty()) {
            std::string lhs =
                (node.left ? format_set_tree_impl(*node.left, depth + 1, max_depth) : "");
            std::string rhs =
                (node.right ? format_set_tree_impl(*node.right, depth + 1, max_depth) : "");
            std::string op = node.op;
            if (node.all)
                op += " ALL";
            return "(" + lhs + ") " + op + " (" + rhs + ")";
        }
        if (node.leaf) {
            return format_select(*node.leaf);
        }
        return "";
    }

    std::string format_set_tree(const SetTree& node)
    {
        return format_set_tree_impl(node, 0, 4);
    }

    static std::string format_join_tree_impl(const JoinTree& node)
    {
        if (node.is_leaf) {
            if (!node.leaf.table.empty())
                return node.leaf.table +
                       (node.leaf.alias.empty() ? "" : (" AS " + node.leaf.alias));
            if (node.leaf.is_subquery)
                return "(sub)" + (node.leaf.alias.empty() ? "" : (" AS " + node.leaf.alias));
            return "?";
        }
        std::string left = node.left ? format_join_tree_impl(*node.left) : "";
        std::string right = node.right ? format_join_tree_impl(*node.right) : "";
        std::string jt;
        switch (node.type) {
        case JoinType::Inner:
            jt = "JOIN";
            break;
        case JoinType::Left:
            jt = "LEFT JOIN";
            break;
        case JoinType::Right:
            jt = "RIGHT JOIN";
            break;
        case JoinType::Full:
            jt = "FULL JOIN";
            break;
        case JoinType::Cross:
            jt = "CROSS JOIN";
            break;
        case JoinType::Natural:
            jt = "NATURAL JOIN";
            break;
        }
        std::string quals;
        if (node.natural)
            quals = " NATURAL";
        else if (node.has_using)
            quals = " USING(..)";
        else if (node.has_on)
            quals = " ON(..)";
        return "(" + left + ") " + jt + quals + " (" + right + ")";
    }

    std::string format_join_tree(const JoinTree& node)
    {
        return format_join_tree_impl(node);
    }

} // namespace scratchbird::engine
