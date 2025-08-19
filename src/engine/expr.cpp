#include "scratchbird/engine/expr.h"

#include <cctype>
#include <sstream>

namespace scratchbird::engine
{

    static std::string trim(const std::string& s)
    {
        size_t a = 0, b = s.size();
        while (a < b && std::isspace((unsigned char)s[a]))
            ++a;
        while (b > a && std::isspace((unsigned char)s[b - 1]))
            --b;
        return s.substr(a, b - a);
    }

    static bool parse_literal_int(const std::string& t, std::int64_t& out)
    {
        if (t.empty())
            return false;
        size_t i = 0;
        if (t[i] == '+' || t[i] == '-')
            ++i;
        for (; i < t.size(); ++i) {
            if (!std::isdigit((unsigned char)t[i]))
                return false;
        }
        try {
            out = std::stoll(t);
            return true;
        } catch (...) {
            return false;
        }
    }

    static bool parse_literal_str(const std::string& t, std::string& out)
    {
        if (t.size() >= 2 && t.front() == '\'' && t.back() == '\'') {
            out = t.substr(1, t.size() - 2);
            return true;
        }
        return false;
    }

    static std::vector<std::string> tokenize(const std::string& expr)
    {
        // naive whitespace tokenizer keeping operators and parentheses as separate tokens
        std::vector<std::string> out;
        std::string cur;
        auto flush = [&]() {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        };
        for (size_t i = 0; i < expr.size(); ++i) {
            char c = expr[i];
            if (std::isspace((unsigned char)c)) {
                flush();
                continue;
            }
            if (c == '(' || c == ')' || c == ',') {
                flush();
                out.emplace_back(1, c);
                continue;
            }
            if ((c == '<' || c == '>' || c == '=' || c == '!') && i + 1 < expr.size()) {
                flush();
                if (expr[i + 1] == '=') {
                    out.push_back(std::string() + c + '=');
                    ++i;
                    continue;
                }
            }
            if (c == '\'') {
                // collect until next single quote
                std::string s(1, c);
                ++i;
                while (i < expr.size()) {
                    s.push_back(expr[i]);
                    if (expr[i] == '\'')
                        break;
                    ++i;
                }
                out.push_back(s);
                continue;
            }
            cur.push_back(c);
        }
        flush();
        return out;
    }

    static int precedence(const std::string& op)
    {
        if (op == "OR")
            return 1;
        if (op == "AND")
            return 2;
        if (op == "NOT")
            return 3;
        // comparisons
        return 4;
    }

    static std::vector<std::string> to_postfix(const std::vector<std::string>& toks)
    {
        std::vector<std::string> out;
        std::vector<std::string> st;
        for (size_t i = 0; i < toks.size(); ++i) {
            std::string t = toks[i];
            // uppercase keywords
            std::string up;
            up.reserve(t.size());
            for (char c : t)
                up.push_back((char)std::toupper((unsigned char)c));
            if (up == "AND" || up == "OR" || up == "NOT" || up == "IS") {
                // IS NULL / IS NOT NULL handled by merging tokens
                while (!st.empty() && st.back() != "(" && precedence(st.back()) >= precedence(up)) {
                    out.push_back(st.back());
                    st.pop_back();
                }
                st.push_back(up);
            } else if (t == "=" || t == "!=" || t == "<" || t == "<=" || t == ">" || t == ">=") {
                while (!st.empty() && st.back() != "(" &&
                       precedence(st.back()) >= precedence("=")) {
                    out.push_back(st.back());
                    st.pop_back();
                }
                st.push_back(t);
            } else if (t == "(") {
                st.push_back(t);
            } else if (t == ")") {
                while (!st.empty() && st.back() != "(") {
                    out.push_back(st.back());
                    st.pop_back();
                }
                if (!st.empty() && st.back() == "(")
                    st.pop_back();
            } else {
                out.push_back(t);
            }
        }
        while (!st.empty()) {
            out.push_back(st.back());
            st.pop_back();
        }
        return out;
    }

    static bool value_from_token(const std::string& tok,
                                 const std::unordered_map<std::string, std::size_t>& col_index,
                                 const std::vector<Value>& row, Value& out)
    {
        // column name
        auto it = col_index.find(tok);
        if (it != col_index.end()) {
            if (it->second < row.size()) {
                out = row[it->second];
                return true;
            }
        }
        // integer literal
        std::int64_t iv = 0;
        if (parse_literal_int(tok, iv)) {
            out.is_null = false;
            out.u64 = (std::uint64_t)iv;
            // Also preserve textual form so comparisons against stored VarBytes succeed
            out.bytes = tok;
            return true;
        }
        // string literal
        std::string sv;
        if (parse_literal_str(tok, sv)) {
            out.is_null = false;
            out.bytes = sv;
            return true;
        }
        // NULL literal
        if (tok == "NULL" || tok == "null") {
            out.is_null = true;
            out.u64 = 0;
            out.bytes.clear();
            return true;
        }
        return false;
    }

    bool evaluate_predicate(const std::string& expr,
                            const std::unordered_map<std::string, std::size_t>& col_index,
                            const std::vector<Value>& row)
    {
        std::string e = trim(expr);
        if (e.empty())
            return true;
        auto toks = tokenize(e);
        // merge IS [NOT] NULL
        std::vector<std::string> norm;
        for (size_t i = 0; i < toks.size(); ++i) {
            std::string up;
            up.reserve(toks[i].size());
            for (char c : toks[i])
                up.push_back((char)std::toupper((unsigned char)c));
            if (up == "IS" && i + 1 < toks.size()) {
                std::string up2;
                for (char c : toks[i + 1])
                    up2.push_back((char)std::toupper((unsigned char)c));
                if (up2 == "NULL") {
                    norm.push_back("ISNULL");
                    ++i;
                    continue;
                } else if (up2 == "NOT" && i + 2 < toks.size()) {
                    std::string up3;
                    for (char c : toks[i + 2])
                        up3.push_back((char)std::toupper((unsigned char)c));
                    if (up3 == "NULL") {
                        norm.push_back("ISNOTNULL");
                        i += 2;
                        continue;
                    }
                }
            }
            norm.push_back(toks[i]);
        }
        auto pf = to_postfix(norm);
        std::vector<Value> st;
        for (size_t i = 0; i < pf.size(); ++i) {
            const std::string& t = pf[i];
            if (t == "AND" || t == "OR") {
                if (st.size() < 2)
                    return false;
                Value b = st.back();
                st.pop_back();
                Value a = st.back();
                st.pop_back();
                bool av = !a.is_null && (a.u64 != 0 || !a.bytes.empty());
                bool bv = !b.is_null && (b.u64 != 0 || !b.bytes.empty());
                Value r{};
                r.is_null = false;
                r.u64 = (t == "AND" ? (av && bv) : (av || bv));
                st.push_back(r);
            } else if (t == "NOT") {
                if (st.empty())
                    return false;
                Value a = st.back();
                st.pop_back();
                bool av = !a.is_null && (a.u64 != 0 || !a.bytes.empty());
                Value r{};
                r.is_null = false;
                r.u64 = (!av);
                st.push_back(r);
            } else if (t == "=" || t == "!=" || t == "<" || t == "<=" || t == ">" || t == ">=") {
                if (st.size() < 2)
                    return false;
                Value b = st.back();
                st.pop_back();
                Value a = st.back();
                st.pop_back();
                // NULL comparison yields NULL -> treat as false
                if (a.is_null || b.is_null) {
                    st.push_back(Value{true});
                    continue;
                }
                int cmp = 0;
                if (!a.bytes.empty() || !b.bytes.empty())
                    cmp = a.bytes.compare(b.bytes);
                else if (a.u64 == b.u64)
                    cmp = 0;
                else
                    cmp = (a.u64 < b.u64 ? -1 : 1);
                bool ok = false;
                if (t == "=")
                    ok = (cmp == 0);
                else if (t == "!=")
                    ok = (cmp != 0);
                else if (t == "<")
                    ok = (cmp < 0);
                else if (t == "<=")
                    ok = (cmp <= 0);
                else if (t == ">")
                    ok = (cmp > 0);
                else if (t == ">=")
                    ok = (cmp >= 0);
                Value r{};
                r.is_null = false;
                r.u64 = ok ? 1 : 0;
                st.push_back(r);
            } else if (t == "ISNULL" || t == "ISNOTNULL") {
                if (st.empty())
                    return false;
                Value a = st.back();
                st.pop_back();
                bool ok = (t == "ISNULL") ? a.is_null : !a.is_null;
                Value r{};
                r.is_null = false;
                r.u64 = ok ? 1 : 0;
                st.push_back(r);
            } else {
                Value v{};
                if (!value_from_token(t, col_index, row, v))
                    return false;
                st.push_back(v);
            }
        }
        if (st.empty())
            return true;
        Value top = st.back();
        if (top.is_null)
            return false;
        return (top.u64 != 0 || !top.bytes.empty());
    }

    std::vector<std::string> compile_predicate(const std::string& expr)
    {
        auto toks = tokenize(expr);
        // merge IS [NOT] NULL
        std::vector<std::string> norm;
        for (size_t i = 0; i < toks.size(); ++i) {
            std::string up;
            up.reserve(toks[i].size());
            for (char c : toks[i])
                up.push_back((char)std::toupper((unsigned char)c));
            if (up == "IS" && i + 1 < toks.size()) {
                std::string up2;
                for (char c : toks[i + 1])
                    up2.push_back((char)std::toupper((unsigned char)c));
                if (up2 == "NULL") {
                    norm.push_back("ISNULL");
                    ++i;
                    continue;
                } else if (up2 == "NOT" && i + 2 < toks.size()) {
                    std::string up3;
                    for (char c : toks[i + 2])
                        up3.push_back((char)std::toupper((unsigned char)c));
                    if (up3 == "NULL") {
                        norm.push_back("ISNOTNULL");
                        i += 2;
                        continue;
                    }
                }
            }
            norm.push_back(toks[i]);
        }
        return to_postfix(norm);
    }

    bool evaluate_predicate_compiled(const std::vector<std::string>& pf,
                                     const std::unordered_map<std::string, std::size_t>& col_index,
                                     const std::vector<Value>& row)
    {
        if (pf.empty())
            return true;
        std::vector<Value> st;
        for (size_t i = 0; i < pf.size(); ++i) {
            const std::string& t = pf[i];
            if (t == "AND" || t == "OR") {
                if (st.size() < 2)
                    return false;
                Value b = st.back();
                st.pop_back();
                Value a = st.back();
                st.pop_back();
                bool av = !a.is_null && (a.u64 != 0 || !a.bytes.empty());
                bool bv = !b.is_null && (b.u64 != 0 || !b.bytes.empty());
                Value r{};
                r.is_null = false;
                r.u64 = (t == "AND" ? (av && bv) : (av || bv));
                st.push_back(r);
            } else if (t == "NOT") {
                if (st.empty())
                    return false;
                Value a = st.back();
                st.pop_back();
                bool av = !a.is_null && (a.u64 != 0 || !a.bytes.empty());
                Value r{};
                r.is_null = false;
                r.u64 = (!av);
                st.push_back(r);
            } else if (t == "=" || t == "!=" || t == "<" || t == "<=" || t == ">" || t == ">=") {
                if (st.size() < 2)
                    return false;
                Value b = st.back();
                st.pop_back();
                Value a = st.back();
                st.pop_back();
                if (a.is_null || b.is_null) {
                    st.push_back(Value{true});
                    continue;
                }
                int cmp = 0;
                if (!a.bytes.empty() || !b.bytes.empty())
                    cmp = a.bytes.compare(b.bytes);
                else if (a.u64 == b.u64)
                    cmp = 0;
                else
                    cmp = (a.u64 < b.u64 ? -1 : 1);
                bool ok = false;
                if (t == "=")
                    ok = (cmp == 0);
                else if (t == "!=")
                    ok = (cmp != 0);
                else if (t == "<")
                    ok = (cmp < 0);
                else if (t == "<=")
                    ok = (cmp <= 0);
                else if (t == ">")
                    ok = (cmp > 0);
                else if (t == ">=")
                    ok = (cmp >= 0);
                Value r{};
                r.is_null = false;
                r.u64 = ok ? 1 : 0;
                st.push_back(r);
            } else if (t == "ISNULL" || t == "ISNOTNULL") {
                if (st.empty())
                    return false;
                Value a = st.back();
                st.pop_back();
                bool ok = (t == "ISNULL") ? a.is_null : !a.is_null;
                Value r{};
                r.is_null = false;
                r.u64 = ok ? 1 : 0;
                st.push_back(r);
            } else {
                Value v{};
                if (!value_from_token(t, col_index, row, v))
                    return false;
                st.push_back(v);
            }
        }
        if (st.empty())
            return true;
        Value top = st.back();
        if (top.is_null)
            return false;
        return (top.u64 != 0 || !top.bytes.empty());
    }

    std::vector<std::string>
    project_row(const std::vector<std::string>& projections,
                const std::vector<std::string>& colnames,
                const std::unordered_map<std::string, std::size_t>& col_index,
                const std::vector<Value>& row)
    {
        // If no projections, default to *
        if (projections.empty()) {
            std::vector<std::string> out;
            out.reserve(row.size());
            for (const auto& v : row) {
                if (v.is_null)
                    out.emplace_back("NULL");
                else if (!v.bytes.empty())
                    out.emplace_back(v.bytes);
                else
                    out.emplace_back(std::to_string(v.u64));
            }
            return out;
        }
        // Expand * once
        std::vector<std::string> out;
        for (const auto& p_raw : projections) {
            std::string p = trim(p_raw);
            if (p == "*") {
                for (const auto& v : row) {
                    if (v.is_null)
                        out.emplace_back("NULL");
                    else if (!v.bytes.empty())
                        out.emplace_back(v.bytes);
                    else
                        out.emplace_back(std::to_string(v.u64));
                }
                continue;
            }
            // Handle AS alias by splitting
            std::string expr = p;
            auto pos = p.find(" AS ");
            if (pos != std::string::npos)
                expr = trim(p.substr(0, pos));
            // Column by name
            auto it = col_index.find(expr);
            if (it != col_index.end() && it->second < row.size()) {
                const auto& v = row[it->second];
                if (v.is_null)
                    out.emplace_back("NULL");
                else if (!v.bytes.empty())
                    out.emplace_back(v.bytes);
                else
                    out.emplace_back(std::to_string(v.u64));
                continue;
            }
            // Ordinal
            std::int64_t idx = 0;
            if (parse_literal_int(expr, idx) && idx >= 1 && (std::size_t)idx <= row.size()) {
                const auto& v = row[(std::size_t)idx - 1];
                if (v.is_null)
                    out.emplace_back("NULL");
                else if (!v.bytes.empty())
                    out.emplace_back(v.bytes);
                else
                    out.emplace_back(std::to_string(v.u64));
                continue;
            }
            // Otherwise, return empty placeholder
            out.emplace_back(std::string());
        }
        return out;
    }

    std::vector<std::string> projection_headers(const std::vector<std::string>& projections,
                                                const std::vector<std::string>& colnames)
    {
        if (projections.empty())
            return colnames;
        std::vector<std::string> out;
        out.reserve(projections.size());
        for (const auto& p_raw : projections) {
            std::string p = trim(p_raw);
            if (p == "*") {
                for (const auto& c : colnames)
                    out.push_back(c);
                continue;
            }
            auto pos = p.find(" AS ");
            if (pos != std::string::npos) {
                out.push_back(trim(p.substr(pos + 4)));
                continue;
            }
            out.push_back(p);
        }
        return out;
    }

} // namespace scratchbird::engine
