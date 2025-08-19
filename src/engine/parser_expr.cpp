#include "scratchbird/engine/parser_expr.h"

#include "scratchbird/engine/lexer.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace scratchbird::engine
{
    namespace
    {
        // Per-parse warnings sink (thread-local for safety in parallel tests)
        static thread_local std::vector<std::string> g_expr_warnings;
        static inline void warn(const std::string& msg)
        {
            g_expr_warnings.push_back(msg);
        }
        struct Tok {
            TokenKind kind{};
            std::string text;
            SourceSpan span{};
        };

        struct Cursor {
            const std::vector<Tok>& t;
            size_t i{0};
            bool at_end() const
            {
                return i >= t.size() || t[i].kind == TokenKind::End;
            }
            const Tok* peek() const
            {
                return at_end() ? nullptr : &t[i];
            }
            const Tok* take()
            {
                return at_end() ? nullptr : &t[i++];
            }
        };

    } // namespace

    static std::vector<Tok> tokenize(std::string_view s)
    {
        Lexer lx(s);
        auto v = lx.lex();
        std::vector<Tok> out;
        out.reserve(v.size());
        for (auto& x : v)
            out.push_back({x.kind, x.text, x.span});
        return out;
    }

    // Forward declare Pratt core
    static std::string expr_bp(struct Cursor& c, int min_bp);

    // parse_primary is unused currently; keep only declaration to avoid -Wunused-function

    // Pratt parser helpers
    static int lbp(const Tok* t)
    {
        if (!t)
            return 0;
        if (t->kind == TokenKind::Symbol) {
            if (t->text == "*" || t->text == "/")
                return 70;
            if (t->text == "+" || t->text == "-")
                return 60;
            if (t->text == "||")
                return 55;
            if (t->text == "=" || t->text == "<>" || t->text == "!=" || t->text == "<" ||
                t->text == ">" || t->text == "<=" || t->text == ">=")
                return 50;
            if (t->text == "::")
                return 80;
        }
        if (t->kind == TokenKind::Keyword) {
            if (t->text == "IN" || t->text == "BETWEEN" || t->text == "LIKE" ||
                t->text == "SIMILAR" || t->text == "IS" || t->text == "COLLATE")
                return 45;
            if (t->text == "AND")
                return 30;
            if (t->text == "OR")
                return 20;
            if (t->text == "CAST")
                return 90; // prefix CAST handling
        }
        return 0;
    }

    static std::string nud(Cursor& c)
    {
        if (auto p = c.take()) {
            if (p->kind == TokenKind::Integer || p->kind == TokenKind::Decimal ||
                p->kind == TokenKind::Identifier || p->kind == TokenKind::QuotedIdentifier ||
                p->kind == TokenKind::String) {
                return p->text;
            }
            if (p->kind == TokenKind::Symbol && p->text == "(") {
                // (expr)
                // parse until )
                std::string inside;
                int depth = 1;
                while (!c.at_end() && depth > 0) {
                    auto q = c.take();
                    if (!q)
                        break;
                    if (q->kind == TokenKind::Symbol && q->text == "(")
                        depth++;
                    else if (q->kind == TokenKind::Symbol && q->text == ")") {
                        depth--;
                        if (depth == 0)
                            break;
                    }
                    if (!inside.empty())
                        inside.push_back(' ');
                    inside += q->text;
                }
                return "(" + inside + ")";
            }
            if (p->kind == TokenKind::Keyword && p->text == "NOT") {
                // unary NOT bp 90
                std::string rhs = nud(c);
                return "NOT " + rhs;
            }
            if (p->kind == TokenKind::Symbol && p->text == "+") {
                std::string rhs = nud(c);
                return "+" + rhs;
            }
            if (p->kind == TokenKind::Keyword && (p->text == "CAST" || p->text == "TYPE")) {
                // CAST(expr AS type)
                std::string inside;
                // expect (
                if (auto lp = c.take()) {
                    if (lp->kind == TokenKind::Symbol && lp->text == "(") {
                        // collect expr until AS
                        int depth = 1;
                        while (!c.at_end() && depth > 0) {
                            auto q = c.peek();
                            if (!q)
                                break;
                            if (q->kind == TokenKind::Symbol && q->text == "(") {
                                depth++;
                                c.take();
                                inside += " (";
                                continue;
                            }
                            if (q->kind == TokenKind::Symbol && q->text == ")") {
                                depth--;
                                c.take();
                                if (depth == 0)
                                    break;
                                inside += " )";
                                continue;
                            }
                            if (q->kind == TokenKind::Keyword && q->text == "AS" && depth == 1) {
                                c.take(); // consume AS
                                break;
                            }
                            if (!inside.empty())
                                inside.push_back(' ');
                            inside += q->text;
                            c.take();
                        }
                        // parse type tail until )
                        std::string type;
                        int depth2 = 1;
                        while (!c.at_end() && depth2 > 0) {
                            auto q = c.take();
                            if (!q)
                                break;
                            if (q->kind == TokenKind::Symbol && q->text == "(") {
                                depth2++;
                            } else if (q->kind == TokenKind::Symbol && q->text == ")") {
                                depth2--;
                                if (depth2 == 0)
                                    break;
                            }
                            if (depth2 > 0) {
                                if (!type.empty())
                                    type.push_back(' ');
                                type += q->text;
                            }
                        }
                        if (p->text == "CAST")
                            return std::string("CAST(") + inside + " AS " + type + ")";
                        // Accept TYPE OF and TYPE OF COLUMN as first-class: normalize to
                        // TYPEOF(...)
                        return std::string("TYPEOF(") + inside + ")";
                    }
                }
                return p->text;
            }
        }
        return {};
    }

    static std::string expr_bp(Cursor& c, int min_bp)
    {
        std::string lhs = nud(c);
        for (;;) {
            const Tok* t = c.peek();
            if (!t || t->kind == TokenKind::End)
                break;
            int bp = lbp(t);
            if (bp < min_bp)
                break;
            std::string op = t->text;
            c.take();
            // Handle NOT + (IN|BETWEEN|LIKE|SIMILAR)
            if (op == "NOT") {
                if (c.peek() && c.peek()->kind == TokenKind::Keyword &&
                    (c.peek()->text == "IN" || c.peek()->text == "BETWEEN" ||
                     c.peek()->text == "LIKE" || c.peek()->text == "SIMILAR")) {
                    op = std::string("NOT ") + c.peek()->text;
                    c.take();
                }
            }
            if (op == "::") {
                std::string rhs = expr_bp(c, bp + 1);
                lhs = std::string("CAST(") + lhs + " AS " + rhs + ")";
            } else if (op == "IN" || op == "NOT IN") {
                if (c.peek() && c.peek()->kind == TokenKind::Symbol && c.peek()->text == "(") {
                    c.take();
                    int depth = 1;
                    std::string inside;
                    while (!c.at_end() && depth > 0) {
                        auto q = c.take();
                        if (!q)
                            break;
                        if (q->kind == TokenKind::Symbol && q->text == "(")
                            depth++;
                        else if (q->kind == TokenKind::Symbol && q->text == ")") {
                            depth--;
                            if (depth == 0)
                                break;
                        }
                        if (!inside.empty())
                            inside.push_back(' ');
                        inside += q->text;
                    }
                    lhs = lhs + " " + op + " (" + inside + ")";
                } else {
                    std::string rhs = expr_bp(c, bp + 1);
                    lhs = lhs + " " + op + " " + rhs;
                }
            } else if (op == "BETWEEN" || op == "NOT BETWEEN") {
                std::string a = expr_bp(c, 0);
                if (c.peek() && c.peek()->kind == TokenKind::Keyword && c.peek()->text == "AND")
                    c.take();
                std::string b = expr_bp(c, 0);
                lhs = lhs + " " + op + " " + a + " AND " + b;
            } else if (op == "LIKE" || op == "NOT LIKE" || op == "SIMILAR" || op == "NOT SIMILAR") {
                std::string likeop = op;
                if (op.find("SIMILAR") != std::string::npos) {
                    if (c.peek() && c.peek()->kind == TokenKind::Keyword &&
                        c.peek()->text == "TO") {
                        c.take();
                        likeop = op + " TO";
                    }
                }
                std::string pat = expr_bp(c, 0);
                std::string esc;
                if (c.peek() && c.peek()->kind == TokenKind::Keyword &&
                    c.peek()->text == "ESCAPE") {
                    c.take();
                    esc = expr_bp(c, 0);
                }
                lhs = lhs + " " + likeop + " " + pat +
                      (esc.empty() ? "" : std::string(" ESCAPE ") + esc);
            } else if (op == "IS") {
                std::string tail;
                if (c.peek() && c.peek()->kind == TokenKind::Keyword && c.peek()->text == "NOT") {
                    c.take();
                    tail = " NOT";
                }
                if (c.peek()) {
                    tail += " ";
                    tail += c.take()->text;
                }
                lhs = lhs + " IS" + tail;
            } else if (op == "=" || op == "<" || op == ">" || op == "<=" || op == ">=" ||
                       op == "<>" || op == "!=") {
                if (c.peek() && c.peek()->kind == TokenKind::Keyword &&
                    (c.peek()->text == "ANY" || c.peek()->text == "ALL")) {
                    std::string q = c.take()->text;
                    if (c.peek() && c.peek()->kind == TokenKind::Symbol && c.peek()->text == "(") {
                        c.take();
                        int depth = 1;
                        std::string sub;
                        while (!c.at_end() && depth > 0) {
                            auto q2 = c.take();
                            if (!q2)
                                break;
                            if (q2->kind == TokenKind::Symbol && q2->text == "(")
                                depth++;
                            else if (q2->kind == TokenKind::Symbol && q2->text == ")") {
                                depth--;
                                if (depth == 0)
                                    break;
                            }
                            if (!sub.empty())
                                sub.push_back(' ');
                            sub += q2->text;
                        }
                        lhs = lhs + " " + op + " " + q + " (" + sub + ")";
                    } else {
                        std::string rhs = expr_bp(c, bp + 1);
                        lhs = lhs + " " + op + " " + q + " " + rhs;
                    }
                } else {
                    std::string rhs = expr_bp(c, bp + 1);
                    lhs = lhs + " " + op + " " + rhs;
                }
            } else {
                std::string rhs = expr_bp(c, bp + 1);
                lhs = lhs + " " + op + " " + rhs;
            }
        }
        return lhs;
    }

    std::string parse_expression_to_string(const std::string& sql)
    {
        auto toks = tokenize(sql);
        Cursor c{toks};
        return expr_bp(c, 0);
    }

    Expr parse_expression_to_ast(const std::string& sql)
    {
        auto toks = tokenize(sql);
        Cursor c{toks};
        // Minimal AST: wrap normalized string and span of entire input
        Expr e{};
        e.kind = ExprKind::Paren; // placeholder kind for any expression
        std::string norm = expr_bp(c, 0);
        e.text = norm;
        // Attempt to detect CAST(expr AS type) to attach TypeDescriptor
        auto pos = norm.find("CAST(");
        if (pos != std::string::npos) {
            // naive parse: find AS and final ')'
            auto aspos = norm.find(" AS ", pos);
            auto rpar = norm.rfind(')');
            if (aspos != std::string::npos && rpar != std::string::npos && rpar > aspos) {
                std::string type_sql = norm.substr(aspos + 4, rpar - (aspos + 4));
                e.kind = ExprKind::Cast;
                e.cast_type = parse_type_descriptor(type_sql);
            }
        }
        if (!toks.empty()) {
            e.span.start = toks.front().span.start;
            e.span.end = toks.back().span.end;
        }
        return e;
    }

    static int parse_int(const std::string& s, size_t& i)
    {
        int v = 0;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
            v = v * 10 + (s[i] - '0');
            ++i;
        }
        return v;
    }

    TypeDescriptor parse_type_descriptor(const std::string& type_sql)
    {
        TypeDescriptor td{};
        std::string s = type_sql;
        // normalize spaces
        for (auto& c : s)
            if (c == '\t' || c == '\n')
                c = ' ';
        // Trim
        auto trim_ws = [](std::string& x) {
            x.erase(x.begin(),
                    std::find_if(x.begin(), x.end(), [](int ch) { return !std::isspace(ch); }));
            x.erase(
                std::find_if(x.rbegin(), x.rend(), [](int ch) { return !std::isspace(ch); }).base(),
                x.end());
        };
        trim_ws(s);
        // TYPEOF(...) special form
        {
            std::string up = s;
            std::transform(up.begin(), up.end(), up.begin(),
                           [](unsigned char c) { return char(std::toupper(c)); });
            if (up.rfind("TYPEOF(", 0) == 0 && up.size() >= 8 && up.back() == ')') {
                auto inside = s.substr(7, s.size() - 8);
                trim_ws(inside);
                std::string upi = inside;
                std::transform(upi.begin(), upi.end(), upi.begin(),
                               [](unsigned char c) { return char(std::toupper(c)); });
                if (upi.rfind("COLUMN ", 0) == 0) {
                    td.type_of_is_column = true;
                    td.type_of_target = inside.substr(7);
                } else {
                    td.type_of_is_column = false;
                    td.type_of_target = inside;
                }
                td.name = "TYPEOF";
                return td;
            }
            // TYPE OF [COLUMN] name form
            if (up.rfind("TYPE OF ", 0) == 0) {
                std::string inside = s.substr(8);
                trim_ws(inside);
                std::string upi = inside;
                std::transform(upi.begin(), upi.end(), upi.begin(),
                               [](unsigned char c) { return char(std::toupper(c)); });
                if (upi.rfind("COLUMN ", 0) == 0) {
                    td.type_of_is_column = true;
                    td.type_of_target = inside.substr(7);
                } else {
                    td.type_of_is_column = false;
                    td.type_of_target = inside;
                }
                td.name = "TYPEOF";
                return td;
            }
        }
        size_t i = 0;
        auto skip_ws = [&] {
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
                ++i;
        };
        skip_ws();
        // name
        size_t start = i;
        while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_'))
            ++i;
        td.name = s.substr(start, i - start);
        // Uppercase for checks
        std::string uname = td.name;
        std::transform(uname.begin(), uname.end(), uname.begin(),
                       [](unsigned char c) { return char(std::toupper(c)); });
        skip_ws();
        // (len[,precision_or_scale])
        if (i < s.size() && s[i] == '(') {
            ++i;
            skip_ws();
            // detect DECIMAL/NUMERIC to map to precision/scale
            bool is_dec =
                (uname == "DECIMAL" || uname == "NUMERIC" || uname == "DEC" || uname == "DECFLOAT");
            int first = parse_int(s, i);
            skip_ws();
            if (i < s.size() && s[i] == ',') {
                ++i;
                skip_ws();
                int second = parse_int(s, i);
                if (is_dec) {
                    td.precision = first;
                    td.scale = second;
                } else {
                    td.length = first;
                    td.scale = second;
                }
            } else {
                if (is_dec) {
                    td.precision = first;
                } else {
                    td.length = first;
                }
            }
            // consume )
            while (i < s.size() && s[i] != ')')
                ++i;
            if (i < s.size())
                ++i;
        }
        skip_ws();
        // CHARACTER SET xyz
        if (i + 14 <= s.size() && s.substr(i, 14) == "CHARACTER SET") {
            i += 14;
            skip_ws();
            size_t cs = i;
            while (i < s.size() &&
                   (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == '-'))
                ++i;
            td.charset = s.substr(cs, i - cs);
        }
        // COLLATE name
        skip_ws();
        if (i + 7 <= s.size() && s.substr(i, 7) == "COLLATE") {
            i += 7;
            skip_ws();
            size_t cs = i;
            while (i < s.size() &&
                   (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == '-'))
                ++i;
            td.collate = s.substr(cs, i - cs);
        }
        skip_ws();
        // array suffixes []
        while (i < s.size() && s[i] == '[') {
            ++td.array_rank;
            // skip until ]
            while (i < s.size() && s[i] != ']')
                ++i;
            if (i < s.size())
                ++i;
            skip_ws();
        }
        return td;
    }
} // namespace scratchbird::engine
