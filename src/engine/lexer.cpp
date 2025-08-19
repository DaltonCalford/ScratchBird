#include "scratchbird/engine/lexer.h"

#include "scratchbird/engine/keywords_generated.h"

#include <cctype>
#include <unordered_set>
#include <utility>

namespace scratchbird::engine
{

    bool Lexer::is_ident_start(char c)
    {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }
    bool Lexer::is_ident_cont(char c)
    {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
    }

    Lexer::Lexer(std::string_view input) : s(input), i(0) {}

    static thread_local std::vector<std::string> g_lexer_warnings;

    void lexer_clear_warnings()
    {
        g_lexer_warnings.clear();
    }
    const std::vector<std::string>& lexer_warnings()
    {
        return g_lexer_warnings;
    }

    std::vector<Token> Lexer::lex()
    {
        std::vector<Token> out;
        for (;;) {
            skip_ws_and_comments();
            if (i >= s.size()) {
                out.push_back({TokenKind::End, "", {i, i}});
                break;
            }
            char c = s[i];
            // National/charset-prefixed strings: N'...' or _UTF8 '...'
            if ((c == 'N' || c == 'n') || c == '_') {
                size_t j = i;
                auto is_likely_known_charset = [](const std::string& tag) {
                    if (tag == "")
                        return false;
                    std::string up;
                    up.reserve(tag.size());
                    for (char ch : tag)
                        up.push_back((char)std::toupper((unsigned char)ch));
                    if (up == "N" || up == "UTF8" || up == "UTF-8" || up == "ASCII" ||
                        up == "UNICODE_FSS" || up == "OCTETS")
                        return true;
                    if (up.rfind("WIN", 0) == 0)
                        return true;
                    if (up.rfind("ISO", 0) == 0)
                        return true;
                    if (up.rfind("KOI8", 0) == 0)
                        return true;
                    if (up == "SJIS" || up == "EUCJIS" || up == "BIG_5" || up == "GBK" ||
                        up == "GB18030")
                        return true;
                    return false;
                };
                if (c == 'N' || c == 'n') {
                    j++;
                    // optional spaces
                    while (j < s.size() && std::isspace(static_cast<unsigned char>(s[j])))
                        j++;
                    if (j < s.size() && s[j] == '\'') {
                        size_t start = i; // include prefix in span
                        i = j;            // position at quote
                        Token strTok = lex_string();
                        strTok.span.start = start;
                        strTok.charset = "N"; // Firebird treats N as national (alias to default
                                              // national charset)
                        out.push_back(std::move(strTok));
                        continue;
                    }
                } else if (c == '_') {
                    size_t k = j + 1;
                    // charset tag: [A-Za-z0-9_]+
                    while (k < s.size() &&
                           (std::isalnum(static_cast<unsigned char>(s[k])) || s[k] == '_'))
                        k++;
                    size_t ws = k;
                    while (ws < s.size() && std::isspace(static_cast<unsigned char>(s[ws])))
                        ws++;
                    if (ws < s.size() && s[ws] == '\'') {
                        size_t start = i; // include prefix in span
                        i = ws;           // position at quote
                        Token strTok = lex_string();
                        strTok.span.start = start;
                        std::string tag(s.substr(j + 1, k - (j + 1)));
                        strTok.charset = tag;
                        if (!is_likely_known_charset(tag)) {
                            g_lexer_warnings.push_back(std::string("Unknown charset tag: ") + tag);
                        }
                        out.push_back(std::move(strTok));
                        continue;
                    }
                }
            }
            if (is_ident_start(c)) {
                out.push_back(lex_ident_or_kw());
            } else if (c == '"') {
                out.push_back(lex_quoted_ident());
            } else if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
                out.push_back(lex_number());
            } else if (c == '\'') {
                // Check for DATE/TIME/TIMESTAMP prefix before the string
                Token strTok = lex_string();
                // simple backward scan of 'out' to find last non-symbol token text
                std::string prev;
                for (auto it = out.rbegin(); it != out.rend(); ++it) {
                    if (it->kind != TokenKind::Symbol) {
                        prev = it->text;
                        break;
                    }
                }
                auto is_all_digits = [](const std::string& x) {
                    for (char ch : x) {
                        if (!std::isdigit((unsigned char)ch))
                            return false;
                    }
                    return !x.empty();
                };
                auto is_valid_date = [&](const std::string& v) {
                    // Accept YYYY-MM-DD or YYYY/MM/DD
                    if (v.size() == 10) {
                        char s1 = v[4];
                        char s2 = v[7];
                        if ((s1 == '-' || s1 == '/') && s2 == s1) {
                            if (is_all_digits(v.substr(0, 4)) && is_all_digits(v.substr(5, 2)) &&
                                is_all_digits(v.substr(8, 2)))
                                return true;
                        }
                        // Accept DD-MM-YYYY or DD/MM/YYYY
                        s1 = v[2];
                        s2 = v[5];
                        if ((s1 == '-' || s1 == '/') && s2 == s1) {
                            if (is_all_digits(v.substr(0, 2)) && is_all_digits(v.substr(3, 2)) &&
                                is_all_digits(v.substr(6, 4)))
                                return true;
                        }
                        // Accept MM-DD-YYYY or MM/DD/YYYY (same char positions as DD-MM-YYYY)
                        if ((v[2] == '-' || v[2] == '/') && v[5] == v[2]) {
                            if (is_all_digits(v.substr(0, 2)) && is_all_digits(v.substr(3, 2)) &&
                                is_all_digits(v.substr(6, 4)))
                                return true;
                        }
                    }
                    return false;
                };
                auto is_valid_time =
                    [&](const std::string& v) { // HH:MM[:SS[.fff]][Z|(+|-)HH[:MM|MM]]
                        if (v.size() < 5)
                            return false;
                        if (v[2] != ':')
                            return false;
                        if (!is_all_digits(v.substr(0, 2)))
                            return false;
                        if (!is_all_digits(v.substr(3, 2)))
                            return false;
                        size_t pos = 5;
                        // Optional seconds
                        if (pos + 3 <= v.size() && v[pos] == ':') {
                            if (!is_all_digits(v.substr(pos + 1, 2)))
                                return false;
                            pos += 3;
                        }
                        // Optional fractional seconds
                        if (pos < v.size() && v[pos] == '.') {
                            pos++;
                            if (pos >= v.size())
                                return false;
                            bool any = false;
                            while (pos < v.size() && std::isdigit((unsigned char)v[pos])) {
                                any = true;
                                pos++;
                            }
                            if (!any)
                                return false;
                        }
                        // Optional timezone: 'Z' or (+|-)HH[:MM] or (+|-)HHMM
                        if (pos == v.size())
                            return true;
                        if (v[pos] == 'Z' || v[pos] == 'z') {
                            return pos + 1 == v.size();
                        }
                        if (v[pos] == '+' || v[pos] == '-') {
                            pos++;
                            if (pos + 2 > v.size() || !is_all_digits(v.substr(pos, 2)))
                                return false;
                            pos += 2;
                            if (pos == v.size())
                                return true; // just hours offset
                            if (v[pos] == ':') {
                                if (pos + 3 != v.size())
                                    return false;
                                if (!is_all_digits(v.substr(pos + 1, 2)))
                                    return false;
                                pos += 3;
                                return pos == v.size();
                            }
                            // compact HHMM
                            if (pos + 2 == v.size() && is_all_digits(v.substr(pos, 2))) {
                                pos += 2;
                                return true;
                            }
                            return false;
                        }
                        return false;
                    };
                auto is_valid_timestamp = [&](const std::string& v) {
                    // Accept space or 'T' as separator
                    size_t sp = v.find(' ');
                    size_t tp = v.find('T');
                    size_t sep = (sp != std::string::npos) ? sp : tp;
                    if (sep == std::string::npos)
                        return false;
                    // If 'T' used, ensure ISO-like usage (no other 'T')
                    return is_valid_date(v.substr(0, sep)) && is_valid_time(v.substr(sep + 1));
                };
                auto is_uuid_text = [&](const std::string& v) {
                    if (v.size() != 36)
                        return false;
                    auto hex = [](char ch) { return std::isxdigit((unsigned char)ch) != 0; };
                    const int parts[5] = {8, 4, 4, 4, 12};
                    size_t pos = 0;
                    for (int pi = 0; pi < 5; ++pi) {
                        for (int k = 0; k < parts[pi]; ++k) {
                            if (pos >= v.size() || !hex(v[pos++]))
                                return false;
                        }
                        if (pi < 4) {
                            if (pos >= v.size() || v[pos++] != '-')
                                return false;
                        }
                    }
                    return pos == v.size();
                };
                if (prev == "DATE") {
                    if (is_valid_date(strTok.text))
                        strTok.kind = TokenKind::Date;
                    else
                        strTok.kind = TokenKind::String;
                } else if (prev == "TIME") {
                    if (is_valid_time(strTok.text))
                        strTok.kind = TokenKind::Time;
                    else
                        strTok.kind = TokenKind::String;
                } else if (prev == "TIMESTAMP") {
                    if (is_valid_timestamp(strTok.text))
                        strTok.kind = TokenKind::Timestamp;
                    else
                        strTok.kind = TokenKind::String;
                } else if (prev == "UUID") {
                    if (is_uuid_text(strTok.text))
                        strTok.kind = TokenKind::Uuid;
                }
                out.push_back(std::move(strTok));
            } else if (c == '$') {
                out.push_back(lex_dollar_string());
            } else {
                out.push_back(lex_symbol());
            }
        }
        return out;
    }

    void Lexer::skip_ws_and_comments()
    {
        for (;;) {
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
                i++;
            if (i + 1 < s.size() && s[i] == '-' && s[i + 1] == '-') {
                i += 2;
                while (i < s.size() && s[i] != '\n')
                    i++;
                continue;
            }
            if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*') {
                i += 2;
                while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/'))
                    i++;
                if (i + 1 < s.size())
                    i += 2;
                continue;
            }
            break;
        }
    }

    Token Lexer::lex_ident_or_kw()
    {
        size_t start = i++;
        while (i < s.size() && is_ident_cont(s[i]))
            i++;
        std::string t(s.substr(start, i - start));
        std::string upper;
        upper.reserve(t.size());
        for (char c : t)
            upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        bool matched_generated = false;
        for (int idx = 0; idx < SCRATCHBIRD_KEYWORDS_COUNT; ++idx) {
            if (SCRATCHBIRD_KEYWORDS[idx] && SCRATCHBIRD_KEYWORDS[idx][0] != '\0' &&
                upper == SCRATCHBIRD_KEYWORDS[idx]) {
                matched_generated = true;
                break;
            }
        }
        if (matched_generated) {
            return {TokenKind::Keyword, upper, {start, i}};
        }
        // Fallback keyword set when generated table is empty or unavailable
        static const std::unordered_set<std::string> kFallbackKeywords = {
            "SELECT", "FROM", "WHERE", "GROUP", "HAVING", "ORDER", "BY", "JOIN", "LEFT", "RIGHT",
            "FULL", "CROSS", "NATURAL", "ON", "USING", "LATERAL", "AS", "WITH", "RECURSIVE",
            "UNION", "ALL", "INTERSECT", "EXCEPT", "DISTINCT", "OVER", "PARTITION", "ROWS", "RANGE",
            "FIRST", "SKIP", "FETCH", "OFFSET", "PLAN",
            // DML Keywords
            "INSERT", "INTO", "VALUES", "UPDATE", "SET", "DELETE", "DEFAULT",
            // DDL Keywords
            "CREATE", "ALTER", "DROP", "TABLE", "INDEX", "CONSTRAINT", "TRIGGER", "PRIMARY", "KEY",
            "FOREIGN", "REFERENCES", "UNIQUE", "CHECK", "NOT", "NULL", "DEFERRABLE", "INITIALLY",
            "IMMEDIATE", "DEFERRED",
            // Trigger Keywords
            "BEFORE", "AFTER", "FOR", "EACH", "ROW", "STATEMENT", "WHEN", "ACTIVE", "INACTIVE",
            // Other common keywords
            "AND", "OR", "IN", "EXISTS", "BETWEEN", "LIKE", "IS", "TRUE", "FALSE"};
        if (kFallbackKeywords.find(upper) != kFallbackKeywords.end()) {
            return {TokenKind::Keyword, upper, {start, i}};
        }
        // keep identifier as typed; resolver will normalize
        return {TokenKind::Identifier, t, {start, i}};
    }

    Token Lexer::lex_quoted_ident()
    {
        size_t start = i; // opening quote at start
        i++;
        std::string out;
        for (; i < s.size();) {
            char c = s[i++];
            if (c == '"') {
                if (i < s.size() && s[i] == '"') {
                    out.push_back('"');
                    i++;
                    continue;
                }
                break;
            }
            out.push_back(c);
        }
        return {TokenKind::QuotedIdentifier, out, {start, i}};
    }

    Token Lexer::lex_number()
    {
        size_t start = i;
        bool has_digits = false;
        // Hex literal 0x...
        if (i + 1 < s.size() && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
            i += 2;
            while (i < s.size() && std::isxdigit(static_cast<unsigned char>(s[i])))
                i++;
            std::string t(s.substr(start, i - start));
            return {TokenKind::Integer, t, {start, i}};
        }
        // UUID/Binary literal X'....'
        if (s[i] == 'X' && i + 1 < s.size() && s[i + 1] == '\'') {
            // binary literal X'ABCD...'
            size_t j = i + 2;
            bool ok = true;
            while (j < s.size() && s[j] != '\'') {
                if (!std::isxdigit(static_cast<unsigned char>(s[j]))) {
                    ok = false;
                    break;
                }
                j++;
            }
            if (ok && j < s.size() && s[j] == '\'') {
                j++;
                std::string t(s.substr(i, j - i));
                i = j;
                return {TokenKind::Uuid, t, {start, i}};
            }
        }
        // Leading digits
        while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '_')) {
            if (std::isdigit(static_cast<unsigned char>(s[i])))
                has_digits = true;
            i++;
        }
        bool is_decimal = false;
        // Dot part, including leading-dot case like .5
        if (i < s.size() && s[i] == '.') {
            // If no digits before, accept only if there are digits after dot
            size_t j = i + 1;
            bool any_after = false;
            while (j < s.size() &&
                   (std::isdigit(static_cast<unsigned char>(s[j])) || s[j] == '_')) {
                if (std::isdigit(static_cast<unsigned char>(s[j])))
                    any_after = true;
                j++;
            }
            if (has_digits || any_after) {
                is_decimal = true;
                i = j;
            } else {
                // Not a number: just a '.' symbol
                return {TokenKind::Symbol, std::string(1, s[i++]), {start, i}};
            }
        }
        // Exponent part
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            size_t j = i + 1;
            if (j < s.size() && (s[j] == '+' || s[j] == '-'))
                j++;
            bool any = false;
            while (j < s.size() &&
                   (std::isdigit(static_cast<unsigned char>(s[j])) || s[j] == '_')) {
                if (std::isdigit(static_cast<unsigned char>(s[j])))
                    any = true;
                j++;
            }
            if (any) {
                is_decimal = true;
                i = j; // accept exponent only if digits present
            }
        }
        std::string t(s.substr(start, i - start));
        // Strip underscores from numeric literal text for normalization
        std::string norm;
        norm.reserve(t.size());
        for (char ch : t)
            if (ch != '_')
                norm.push_back(ch);
        return {is_decimal ? TokenKind::Decimal : TokenKind::Integer, norm, {start, i}};
    }

    Token Lexer::lex_string()
    {
        size_t start = i; // opening '
        i++;              // skip opening '
        std::string out;
        for (; i < s.size();) {
            char c = s[i++];
            if (c == '\'') {
                if (i < s.size() && s[i] == '\'') { // escaped by doubling
                    out.push_back('\'');
                    i++;
                    continue;
                }
                break;
            }
            out.push_back(c);
        }
        return {TokenKind::String, out, {start, i}};
    }

    // Helper: look back to previous non-space token text
    static std::string prev_nonspace_text(const std::vector<Token>& toks)
    {
        for (auto it = toks.rbegin(); it != toks.rend(); ++it) {
            if (it->kind != TokenKind::Symbol ||
                (it->text != "," && it->text != ")" && it->text != "("))
                return it->text;
        }
        return {};
    }

    Token Lexer::lex_dollar_string()
    {
        size_t start = i;
        size_t tag_start = i;
        if (s[i] != '$')
            return {TokenKind::Symbol, std::string(1, s[i++]), {start, i}};
        i++; // skip first $
        std::string tag;
        while (i < s.size() && s[i] != '$' &&
               (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_')) {
            tag.push_back(s[i++]);
        }
        if (i >= s.size() || s[i] != '$') {
            // not a dollar-quote, treat as symbol
            i = tag_start;
            return lex_symbol();
        }
        // consume closing of opener
        i++;
        std::string end_tag = "$" + tag + "$";
        std::string content;
        while (i < s.size()) {
            // Forbid nested open tags
            if (s[i] == '$') {
                size_t k = i + 1;
                std::string inner;
                while (k < s.size() && s[k] != '$' &&
                       (std::isalnum(static_cast<unsigned char>(s[k])) || s[k] == '_'))
                    inner.push_back(s[k++]);
                if (k < s.size() && s[k] == '$') {
                    // nested opener detected -> treat as termination (error-like)
                    break;
                }
            }
            if (i + end_tag.size() <= s.size() && s.substr(i, end_tag.size()) == end_tag) {
                i += end_tag.size();
                break;
            }
            content.push_back(s[i++]);
        }
        return {TokenKind::String, content, {start, i}};
    }

    Token Lexer::lex_symbol()
    {
        size_t start = i;
        static const char* two[] = {"!=", "<>", "<=", ">=", "::", "||"};
        for (auto t : two) {
            size_t n = std::char_traits<char>::length(t);
            if (i + n <= s.size() && s.substr(i, n) == t) {
                std::string v(t);
                i += n;
                return {TokenKind::Symbol, v, {start, i}};
            }
        }
        char c = s[i++];
        return {TokenKind::Symbol, std::string(1, c), {start, i}};
    }

} // namespace scratchbird::engine
