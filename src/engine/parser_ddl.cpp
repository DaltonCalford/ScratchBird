// SPDX-License-Identifier: IDPL
#include "scratchbird/engine/parser_ddl.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace scratchbird::engine
{
    // Helper to normalize index entry tokens: collapse spaces, handle quotes, remove
    // spaces around dots, after '(', before ')', and after commas. Preserve content inside quotes.
    static std::string normalize_index_entry(const std::string& input)
    {
        std::string out;
        out.reserve(input.size());
        bool in_single = false;
        bool in_double = false;
        auto is_space = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
        auto skip_spaces = [&](const std::string& s, size_t& i) {
            while (i < s.size() && is_space(s[i]))
                ++i;
        };
        // First pass: remove redundant spaces outside quotes
        for (size_t i = 0; i < input.size(); ++i) {
            char c = input[i];
            if (c == '\'' && !in_double) {
                in_single = !in_single;
                out.push_back(c);
                continue;
            }
            if (c == '"' && !in_single) {
                // handle doubled quotes inside quoted identifier
                if (in_double && i + 1 < input.size() && input[i + 1] == '"') {
                    out.push_back('"');
                    out.push_back('"');
                    ++i;
                    continue;
                }
                in_double = !in_double;
                out.push_back(c);
                continue;
            }
            if (in_single || in_double) {
                out.push_back(c);
                continue;
            }
            if (is_space(c)) {
                // compress spaces to single pending space, but drop around punctuation handled
                // below Lookahead to decide if we should keep a space
                size_t j = i;
                skip_spaces(input, j);
                char next = (j < input.size() ? input[j] : '\0');
                char prev = (out.empty() ? '\0' : out.back());
                bool drop = (next == ')' || next == ',' || next == '.' || prev == '(' ||
                             prev == ',' || prev == '.');
                if (!drop && prev != ' ')
                    out.push_back(' ');
                i = j - 1;
                continue;
            }
            if (c == '.') {
                if (!out.empty() && out.back() == ' ')
                    out.pop_back();
                out.push_back('.');
                // skip spaces after dot
                size_t j = i + 1;
                while (j < input.size() && is_space(input[j]))
                    ++j;
                i = j - 1;
                continue;
            }
            if (c == '(') {
                out.push_back('(');
                // skip spaces after '('
                size_t j = i + 1;
                while (j < input.size() && is_space(input[j]))
                    ++j;
                i = j - 1;
                continue;
            }
            if (c == ',') {
                out.push_back(',');
                // ensure single space after comma if next is not ')' or end
                size_t j = i + 1;
                while (j < input.size() && is_space(input[j]))
                    ++j;
                if (j < input.size() && input[j] != ')' && input[j] != ',')
                    out.push_back(' ');
                i = j - 1;
                continue;
            }
            if (c == ')') {
                // remove space before ')'
                if (!out.empty() && out.back() == ' ')
                    out.pop_back();
                out.push_back(')');
                continue;
            }
            out.push_back(c);
        }
        // Trim result
        size_t a = 0, b = out.size();
        while (a < b && is_space(out[a]))
            ++a;
        while (b > a && is_space(out[b - 1]))
            --b;
        return out.substr(a, b - a);
    }
    namespace
    {
        static inline void push_warn(Ast& ast, int start, int end, const std::string& msg)
        {
            ast.warnings.push_back(msg);
            ast.warning_spans.push_back({start, end});
        }
        static std::string lowercase(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
            return s;
        }
        static void trim(std::string& s)
        {
            auto not_space = [](int ch) { return !std::isspace(ch); };
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
            s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
        }
        static bool starts_with_ci(const std::string& s, const char* kw)
        {
            std::string l = s;
            std::transform(l.begin(), l.end(), l.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
            std::string k(kw);
            std::transform(k.begin(), k.end(), k.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
            return l.rfind(k, 0) == 0;
        }
    } // namespace

    Ast parse_ddl_table(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlTable;
        ast.ddlTable.span = {0, int(sql.size())};
        std::string s = sql;
        trim(s);
        std::string ls = lowercase(s);
        // Handle ALTER TABLE minimally
        if (ls.rfind("alter table", 0) == 0) {
            // ALTER TABLE name <ops>
            auto after = s.substr(11);
            trim(after);
            auto sp = after.find_first_of(" \t\n");
            ast.ddlTable.name = (sp == std::string::npos) ? after : after.substr(0, sp);
            ast.ddlTable.table_attrs_raw =
                (sp == std::string::npos) ? std::string() : after.substr(sp + 1);
            // Populate alter_ops: split by commas/semicolons at top level and classify
            {
                std::string ops = ast.ddlTable.table_attrs_raw;
                std::string cur;
                int depth = 0;
                auto flush = [&](const std::string& piece) {
                    std::string o = piece;
                    trim(o);
                    if (o.empty())
                        return;
                    // build op entry
                    // Kind detection
                    std::string lo = lowercase(o);
                    std::string kind;
                    if (lo.rfind("add ", 0) == 0)
                        kind = "ADD";
                    else if (lo.rfind("drop ", 0) == 0)
                        kind = "DROP";
                    else if (lo.rfind("alter ", 0) == 0)
                        kind = "ALTER";
                    // Target: after COLUMN/CONSTRAINT or first token after kind
                    std::string target;
                    size_t pos = std::string::npos;
                    for (const char* kw : {" column ", " constraint "}) {
                        auto p = lo.find(kw);
                        if (p != std::string::npos) {
                            pos = p + std::strlen(kw);
                            break;
                        }
                    }
                    if (pos == std::string::npos) {
                        // after kind word
                        size_t sp = o.find(' ');
                        if (sp != std::string::npos)
                            target = o.substr(sp + 1);
                    } else {
                        target = o.substr(pos);
                    }
                    // take up to next space as name
                    {
                        std::string t = target;
                        trim(t);
                        size_t sp = t.find_first_of(" \t\n");
                        if (sp != std::string::npos)
                            t = t.substr(0, sp);
                        target = t;
                    }
                    // store
                    decltype(ast.ddlTable.alter_ops)::value_type op;
                    op.kind = kind;
                    op.target = target;
                    op.raw = o;
                    ast.ddlTable.alter_ops.push_back(std::move(op));
                };
                for (char c : ops) {
                    if (c == '(')
                        depth++;
                    else if (c == ')')
                        depth = depth > 0 ? depth - 1 : 0;
                    if ((c == ',' || c == ';') && depth == 0) {
                        flush(cur);
                        cur.clear();
                    } else {
                        cur.push_back(c);
                    }
                }
                flush(cur);
                if (depth != 0) {
                    push_warn(ast, int(ast.ddlTable.span.start), int(ast.ddlTable.span.end),
                              "ALTER TABLE operations parentheses unbalanced; recovered");
                }
            }
            // Diagnostics: TYPE change missing type
            auto la = lowercase(ast.ddlTable.table_attrs_raw);
            auto ac = la.find(" alter column ");
            if (ac != std::string::npos) {
                auto tp = la.find(" type ", ac);
                if (tp != std::string::npos) {
                    auto tail = ast.ddlTable.table_attrs_raw.substr(tp + 6);
                    std::string ttail = lowercase(tail);
                    std::string tt = tail;
                    trim(tt);
                    if (tt.empty()) {
                        push_warn(ast, int(ast.ddlTable.span.start), int(ast.ddlTable.span.end),
                                  "ALTER COLUMN TYPE missing type");
                    }
                }
            }
            return ast;
        }
        // Handle CREATE/RECREATE TABLE name ( ... )
        if (ls.rfind("create table", 0) == 0 || ls.rfind("recreate table", 0) == 0) {
            auto rest = s.substr(12);
            trim(rest);
            auto p3 = rest.find_first_of(" (\t\n");
            ast.ddlTable.name = (p3 == std::string::npos) ? rest : rest.substr(0, p3);
            auto lp = rest.find('(');
            auto rp = rest.rfind(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                std::string inside = rest.substr(lp + 1, rp - lp - 1);
                ast.ddlTable.columns_raw = inside;
                // Split entries by commas at top-level parentheses depth
                std::vector<std::string> entries;
                std::string cur;
                int depth = 0;
                for (char c : inside) {
                    if (c == '(')
                        depth++;
                    else if (c == ')')
                        depth = depth > 0 ? depth - 1 : 0;
                    if (c == ',' && depth == 0) {
                        size_t a = cur.find_first_not_of(" \t\n");
                        size_t b = cur.find_last_not_of(" \t\n");
                        if (a != std::string::npos)
                            entries.push_back(cur.substr(a, b - a + 1));
                        cur.clear();
                    } else {
                        cur.push_back(c);
                    }
                }
                if (!cur.empty()) {
                    size_t a = cur.find_first_not_of(" \t\n");
                    size_t b = cur.find_last_not_of(" \t\n");
                    if (a != std::string::npos)
                        entries.push_back(cur.substr(a, b - a + 1));
                }
                // Classify entries
                std::vector<std::string> cols, cons;
                for (auto& e : entries) {
                    std::string le = lowercase(e);
                    if (le.rfind("constraint ", 0) == 0 || le.rfind("primary ", 0) == 0 ||
                        le.rfind("unique ", 0) == 0 || le.rfind("check ", 0) == 0 ||
                        le.rfind("foreign ", 0) == 0) {
                        cons.push_back(e);
                    } else {
                        cols.push_back(e);
                    }
                }
                auto join = [](const std::vector<std::string>& v) {
                    std::string r;
                    for (size_t k = 0; k < v.size(); ++k) {
                        if (k)
                            r += ", ";
                        r += v[k];
                    }
                    return r;
                };
                ast.ddlTable.column_defs_raw = join(cols);
                ast.ddlTable.constraints_raw = join(cons);
                // Parse structured table constraints from cons list
                auto split_csv_top = [](const std::string& s) {
                    std::vector<std::string> out;
                    std::string cur;
                    int d = 0;
                    for (char c : s) {
                        if (c == '(')
                            d++;
                        else if (c == ')' && d > 0)
                            d--;
                        if (c == ',' && d == 0) {
                            std::string t = cur;
                            trim(t);
                            if (!t.empty())
                                out.push_back(t);
                            cur.clear();
                        } else
                            cur.push_back(c);
                    }
                    std::string t = cur;
                    trim(t);
                    if (!t.empty())
                        out.push_back(t);
                    return out;
                };
                auto trim_copy = [](std::string s) {
                    trim(s);
                    return s;
                };
                for (auto& cstr : cons) {
                    std::string lc = lowercase(cstr);
                    decltype(ast.ddlTable.table_constraints)::value_type tc{};
                    // Optional CONSTRAINT name prefix
                    if (lc.rfind("constraint ", 0) == 0) {
                        auto after = cstr.substr(11);
                        std::string la = lowercase(after);
                        auto sp = after.find_first_of(" \t\n");
                        if (sp != std::string::npos) {
                            tc.name = after.substr(0, sp);
                            cstr = after.substr(sp + 1);
                            lc = lowercase(cstr);
                        }
                    }
                    // Helper to parse DEFERRABLE/INITIALLY options
                    auto parse_deferrable =
                        [&](const std::string& raw,
                            decltype(ast.ddlTable.table_constraints)::value_type& out_tc) {
                            std::string ll = lowercase(raw);
                            if (ll.find(" deferrable") != std::string::npos)
                                out_tc.deferrable = true;
                            auto ip = ll.find(" initially ");
                            if (ip != std::string::npos) {
                                auto seg = ll.substr(ip + 11);
                                if (seg.rfind("deferred", 0) == 0)
                                    out_tc.initially_deferred = true;
                            }
                        };
                    // PRIMARY KEY / UNIQUE
                    if (lc.rfind("primary key", 0) == 0 || lc.rfind("unique", 0) == 0) {
                        tc.kind = (lc.rfind("primary key", 0) == 0) ? "PRIMARY KEY" : "UNIQUE";
                        auto lp1 = cstr.find('(');
                        auto rp1 = cstr.rfind(')');
                        if (lp1 != std::string::npos && rp1 != std::string::npos && rp1 > lp1) {
                            auto list = cstr.substr(lp1 + 1, rp1 - lp1 - 1);
                            for (auto& it : split_csv_top(list))
                                tc.columns.push_back(trim_copy(it));
                        }
                        parse_deferrable(cstr, tc);
                        ast.ddlTable.table_constraints.push_back(std::move(tc));
                        continue;
                    }
                    // CHECK (expr)
                    if (lc.rfind("check ", 0) == 0) {
                        tc.kind = "CHECK";
                        auto lp1 = cstr.find('(');
                        auto rp1 = cstr.rfind(')');
                        if (lp1 != std::string::npos && rp1 != std::string::npos && rp1 > lp1)
                            tc.check_expr = trim_copy(cstr.substr(lp1 + 1, rp1 - lp1 - 1));
                        parse_deferrable(cstr, tc);
                        ast.ddlTable.table_constraints.push_back(std::move(tc));
                        continue;
                    }
                    // FOREIGN KEY (cols) REFERENCES ref_table (ref_cols) [ON UPDATE ...] [ON DELETE
                    // ...]
                    if (lc.rfind("foreign key", 0) == 0) {
                        tc.kind = "FOREIGN KEY";
                        auto lp1 = cstr.find('(');
                        auto rp1 = cstr.find(')');
                        if (lp1 != std::string::npos && rp1 != std::string::npos && rp1 > lp1) {
                            auto list = cstr.substr(lp1 + 1, rp1 - lp1 - 1);
                            for (auto& it : split_csv_top(list))
                                tc.columns.push_back(trim_copy(it));
                        }
                        auto refp = lowercase(cstr).find(" references ");
                        if (refp != std::string::npos) {
                            auto after = cstr.substr(refp + 12);
                            trim(after);
                            // ref table name until '(' or space
                            size_t stop = after.find_first_of(" (\t\n");
                            tc.ref_table =
                                (stop == std::string::npos) ? after : after.substr(0, stop);
                            auto lp2 = after.find('(');
                            auto rp2 = after.find(')', lp2);
                            if (lp2 != std::string::npos && rp2 != std::string::npos && rp2 > lp2) {
                                auto rlist = after.substr(lp2 + 1, rp2 - lp2 - 1);
                                for (auto& it : split_csv_top(rlist))
                                    tc.ref_columns.push_back(trim_copy(it));
                            }
                        }
                        auto ll = lowercase(cstr);
                        auto upd = ll.find(" on update ");
                        if (upd != std::string::npos) {
                            auto seg = cstr.substr(upd + 11);
                            auto end = seg.find_first_of(",)");
                            auto act =
                                trim_copy((end == std::string::npos) ? seg : seg.substr(0, end));
                            auto la = lowercase(act);
                            if (la.find("cascade") != std::string::npos)
                                tc.on_update = "CASCADE";
                            else if (la.find("set null") != std::string::npos)
                                tc.on_update = "SET NULL";
                            else if (la.find("set default") != std::string::npos)
                                tc.on_update = "SET DEFAULT";
                            else if (la.find("no action") != std::string::npos)
                                tc.on_update = "NO ACTION";
                            else if (la.find("restrict") != std::string::npos)
                                tc.on_update = "RESTRICT";
                            else
                                tc.on_update = act;
                        }
                        auto del = ll.find(" on delete ");
                        if (del != std::string::npos) {
                            auto seg = cstr.substr(del + 11);
                            auto end = seg.find_first_of(",)");
                            auto act =
                                trim_copy((end == std::string::npos) ? seg : seg.substr(0, end));
                            auto la = lowercase(act);
                            if (la.find("cascade") != std::string::npos)
                                tc.on_delete = "CASCADE";
                            else if (la.find("set null") != std::string::npos)
                                tc.on_delete = "SET NULL";
                            else if (la.find("set default") != std::string::npos)
                                tc.on_delete = "SET DEFAULT";
                            else if (la.find("no action") != std::string::npos)
                                tc.on_delete = "NO ACTION";
                            else if (la.find("restrict") != std::string::npos)
                                tc.on_delete = "RESTRICT";
                            else
                                tc.on_delete = act;
                        }
                        parse_deferrable(cstr, tc);
                        ast.ddlTable.table_constraints.push_back(std::move(tc));
                        continue;
                    }
                }
                // COMPUTED BY / GENERATED ALWAYS AS (expr) / per-column charset/collate / NOT NULL
                // / IDENTITY capture (naive)
                for (auto& c : cols) {
                    auto lc = lowercase(c);
                    auto p = lc.find(" computed by ");
                    if (p != std::string::npos) {
                        // column name up to first space
                        std::string colname;
                        size_t sp = c.find_first_of(" \t\n");
                        colname = (sp == std::string::npos) ? c : c.substr(0, sp);
                        // extract expression within parentheses after COMPUTED BY
                        size_t lp2 = c.find('(', p);
                        size_t rp2 = c.rfind(')');
                        if (lp2 != std::string::npos && rp2 != std::string::npos && rp2 > lp2) {
                            std::string expr = c.substr(lp2 + 1, rp2 - lp2 - 1);
                            // keep raw for now; normalization via expr parser possible in future
                            ast.ddlTable.computed_by.push_back({colname, expr});
                        }
                    }
                    // GENERATED ALWAYS AS (expr) [VIRTUAL]
                    if (lc.find(" generated ") != std::string::npos &&
                        lc.find(" as identity") == std::string::npos &&
                        lc.find(" as ") != std::string::npos) {
                        // Heuristic: treat as computed-by if followed by parenthesized expression
                        size_t sp = c.find_first_of(" \t\n");
                        std::string colname = (sp == std::string::npos) ? c : c.substr(0, sp);
                        auto apos = lc.find(" as ");
                        size_t lp2 = c.find('(', apos);
                        size_t rp2 = c.rfind(')');
                        if (apos != std::string::npos && lp2 != std::string::npos &&
                            rp2 != std::string::npos && rp2 > lp2) {
                            std::string expr = c.substr(lp2 + 1, rp2 - lp2 - 1);
                            ast.ddlTable.computed_by.push_back({colname, expr});
                            if (lc.find(" virtual") != std::string::npos) {
                                push_warn(ast, int(ast.ddlTable.span.start),
                                          int(ast.ddlTable.span.end),
                                          "Column '" + colname +
                                              "': VIRTUAL modifier noted for GENERATED AS; treated "
                                              "as computed");
                            }
                        }
                    }
                    // CHARACTER SET
                    auto cs = lc.find(" character set ");
                    if (cs != std::string::npos) {
                        std::string colname;
                        size_t sp = c.find_first_of(" \t\n");
                        colname = (sp == std::string::npos) ? c : c.substr(0, sp);
                        auto after = c.substr(cs + 15);
                        size_t end = after.find_first_of(" \t\n,");
                        std::string val = (end == std::string::npos) ? after : after.substr(0, end);
                        ast.ddlTable.column_charsets.push_back({colname, val});
                    }
                    // COLLATE
                    auto co = lc.find(" collate ");
                    if (co != std::string::npos) {
                        std::string colname;
                        size_t sp = c.find_first_of(" \t\n");
                        colname = (sp == std::string::npos) ? c : c.substr(0, sp);
                        auto after = c.substr(co + 9);
                        size_t end = after.find_first_of(" \t\n,");
                        std::string val = (end == std::string::npos) ? after : after.substr(0, end);
                        ast.ddlTable.column_collates.push_back({colname, val});
                    }
                    // NOT NULL
                    if (lc.find(" not null") != std::string::npos) {
                        size_t sp = c.find_first_of(" \t\n");
                        std::string colname = (sp == std::string::npos) ? c : c.substr(0, sp);
                        ast.ddlTable.not_null_columns.push_back(colname);
                    }
                    // IDENTITY: GENERATED {ALWAYS|BY DEFAULT} AS IDENTITY
                    if (lc.find(" generated ") != std::string::npos &&
                        lc.find(" as identity") != std::string::npos) {
                        size_t sp = c.find_first_of(" \t\n");
                        std::string colname = (sp == std::string::npos) ? c : c.substr(0, sp);
                        std::string variant =
                            (lc.find(" always ") != std::string::npos)
                                ? "ALWAYS"
                                : ((lc.find(" by default ") != std::string::npos) ? "BY DEFAULT"
                                                                                  : "");
                        ast.ddlTable.identity_columns.push_back({colname, variant});
                    }
                }
                // Diagnostics: identity options malformed
                if (lowercase(ast.ddlTable.column_defs_raw).find("generated") !=
                        std::string::npos &&
                    lowercase(ast.ddlTable.column_defs_raw).find("as identity") !=
                        std::string::npos) {
                    // If no '(' appears after 'as identity'
                    auto posid = lowercase(ast.ddlTable.column_defs_raw).find("as identity");
                    auto tail = ast.ddlTable.column_defs_raw.substr(posid + 11);
                    if (tail.find('(') == std::string::npos) {
                        push_warn(ast, int(ast.ddlTable.span.start), int(ast.ddlTable.span.end),
                                  "IDENTITY options missing parentheses");
                    }
                    // If present, capture options inside as naive raw key->value pairs per column
                    // e.g., GENERATED ... AS IDENTITY (START WITH 1 INCREMENT BY 1)
                    size_t lp3 = ast.ddlTable.column_defs_raw.find('(', posid + 11);
                    size_t rp3 = ast.ddlTable.column_defs_raw.find(')', lp3);
                    if (lp3 != std::string::npos && rp3 != std::string::npos && rp3 > lp3) {
                        std::string opts =
                            ast.ddlTable.column_defs_raw.substr(lp3 + 1, rp3 - lp3 - 1);
                        // Try to associate with last seen identity column name
                        if (!ast.ddlTable.identity_columns.empty()) {
                            auto colname = ast.ddlTable.identity_columns.back().first;
                            ast.ddlTable.identity_options.push_back({colname, opts});
                            // Parse simple options
                            decltype(ast.ddlTable.identity_options_parsed)::value_type::second_type
                                parsed{};
                            parsed.raw = opts;
                            auto lo = lowercase(opts);
                            auto sw = lo.find("start with");
                            if (sw != std::string::npos) {
                                auto seg = opts.substr(sw + 10);
                                long long v = 0;
                                try {
                                    v = std::stoll(seg);
                                } catch (...) {
                                }
                                parsed.start_with = v;
                            }
                            auto ib = lo.find("increment by");
                            if (ib != std::string::npos) {
                                auto seg = opts.substr(ib + 12);
                                long long v = 0;
                                try {
                                    v = std::stoll(seg);
                                } catch (...) {
                                }
                                parsed.increment_by = v;
                            }
                            if (lo.find("cycle") != std::string::npos)
                                parsed.cycle = true;
                            ast.ddlTable.identity_options_parsed.push_back(
                                std::make_pair(colname, parsed));
                        }
                    }
                }
                // Diagnostics: FK actions presence sanity
                std::string lccon = lowercase(ast.ddlTable.constraints_raw);
                if (lccon.find("foreign key") != std::string::npos) {
                    auto check_kw = [&](const char* kw) {
                        auto p = lccon.find(kw);
                        if (p != std::string::npos) {
                            auto rest = lccon.substr(p + strlen(kw));
                            if (rest.find("cascade") == std::string::npos &&
                                rest.find("set null") == std::string::npos &&
                                rest.find("set default") == std::string::npos &&
                                rest.find("no action") == std::string::npos &&
                                rest.find("restrict") == std::string::npos) {
                                push_warn(ast, int(ast.ddlTable.span.start),
                                          int(ast.ddlTable.span.end),
                                          std::string("FK ") + kw + " without action");
                            }
                        }
                    };
                    check_kw("on update");
                    check_kw("on delete");
                }
            } else if (lp != std::string::npos && (rp == std::string::npos || rp < lp)) {
                // Malformed: missing closing ')'. Fast-forward until sentinel or end
                auto lower_rest = lowercase(rest);
                size_t sent = std::string::npos;
                for (const auto& kw :
                     {std::string(" external file "), std::string(" with "), std::string(" on "),
                      std::string(" using "), std::string(" as ")}) {
                    size_t p = lower_rest.find(kw, lp + 1);
                    if (p != std::string::npos) {
                        sent = p;
                        break;
                    }
                }
                size_t end = (sent == std::string::npos) ? rest.size() : sent;
                std::string inside = rest.substr(lp + 1, (end > lp + 1 ? end - (lp + 1) : 0));
                ast.ddlTable.columns_raw = inside;
                push_warn(ast, int(ast.ddlTable.span.start), int(ast.ddlTable.span.end),
                          "CREATE TABLE columns/constraints malformed; recovered");
            }
            if (rp != std::string::npos && rp + 1 < rest.size()) {
                ast.ddlTable.table_attrs_raw = rest.substr(rp + 1);
                trim(ast.ddlTable.table_attrs_raw);
                // Capture EXTERNAL FILE 'path'
                auto lattrs = lowercase(ast.ddlTable.table_attrs_raw);
                auto epos = lattrs.find(" external file ");
                if (epos != std::string::npos) {
                    auto after = ast.ddlTable.table_attrs_raw.substr(epos + 15);
                    // Find first quoted string
                    auto q1 = after.find('\'');
                    if (q1 != std::string::npos) {
                        auto q2 = after.find('\'', q1 + 1);
                        if (q2 != std::string::npos && q2 > q1) {
                            ast.ddlTable.external_file = after.substr(q1 + 1, q2 - q1 - 1);
                        }
                    }
                }
            }
        }
        return ast;
    }

    Ast parse_ddl_index(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlIndex;
        ast.ddlIndex.span = {0, int(sql.size())};
        std::string s = sql;
        trim(s);
        std::string l = lowercase(s);
        // action
        if (l.rfind("create index", 0) == 0 || l.rfind("recreate index", 0) == 0)
            ast.ddlIndex.action = "CREATE";
        else if (l.rfind("alter index", 0) == 0)
            ast.ddlIndex.action = "ALTER";
        else if (l.rfind("drop index", 0) == 0)
            ast.ddlIndex.action = "DROP";
        else if (l.rfind("reindex ", 0) == 0 || l.find(" reindex ") != std::string::npos)
            ast.ddlIndex.action = "REINDEX";
        else if (l.rfind("validate index", 0) == 0 ||
                 l.find(" validate index ") != std::string::npos)
            ast.ddlIndex.action = "VALIDATE";
        ast.ddlIndex.unique = l.find("unique") != std::string::npos;
        // CREATE/RECREATE [UNIQUE] INDEX name ON table (... | COMPUTED BY (...))
        auto onpos = l.find(" on ");
        if (onpos != std::string::npos) {
            auto head = s.substr(0, onpos);
            auto tail = s.substr(onpos + 4);
            // extract index name from head
            auto words_end = head.find_last_not_of(" \t\n");
            auto words = head.substr(0, words_end + 1);
            auto sp = words.find_last_of(" \t\n");
            if (sp != std::string::npos)
                ast.ddlIndex.name = words.substr(sp + 1);
            trim(tail);
            // table
            auto sp2 = tail.find_first_of(" (\t\n");
            ast.ddlIndex.on_table = (sp2 == std::string::npos) ? tail : tail.substr(0, sp2);
            // method USING ... or PARTIAL HASH marker
            auto usingpos = l.find(" using ");
            if (usingpos != std::string::npos) {
                auto afteru = s.substr(usingpos + 7);
                auto spm = afteru.find_first_of(" \t\n(");
                ast.ddlIndex.method = (spm == std::string::npos) ? afteru : afteru.substr(0, spm);
            }
            if (l.find(" partial hash index ") != std::string::npos ||
                l.find(" partial hash ") != std::string::npos) {
                ast.ddlIndex.method = "PARTIAL_HASH";
            }
            // detect COMPUTED BY form
            auto cbpos_l = lowercase(tail).find(" computed by ");
            if (cbpos_l != std::string::npos) {
                auto after = tail.substr(cbpos_l + 12);
                auto lp = after.find('(');
                auto rp = after.rfind(')');
                if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                    ast.ddlIndex.expr_raw = after.substr(lp + 1, rp - lp - 1);
                }
            } else {
                auto lp = tail.find('(');
                auto rp = tail.rfind(')');
                if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                    ast.ddlIndex.expr_raw = tail.substr(lp + 1, rp - lp - 1);
                    // split columns by commas (naive top-level), and detect ASC/DESC and COLLATE
                    std::string cols = ast.ddlIndex.expr_raw;
                    std::string cur;
                    int depth = 0;
                    auto flush = [&](const std::string& piece) {
                        std::string p = piece;
                        size_t a = p.find_first_not_of(" \t\n");
                        size_t b = p.find_last_not_of(" \t\n");
                        if (a == std::string::npos)
                            return;
                        std::string token = normalize_index_entry(p.substr(a, b - a + 1));
                        // detect COLLATE and ASC/DESC at end
                        std::string lt = lowercase(token);
                        std::string coll;
                        auto colp = lt.rfind(" collate ");
                        if (colp != std::string::npos) {
                            std::string name = token.substr(colp + 9);
                            size_t end = name.find_first_of(" \t\n");
                            coll = (end == std::string::npos) ? name : name.substr(0, end);
                            token = token.substr(0, colp);
                            lt = lowercase(token);
                        }
                        std::string dir;
                        if (lt.size() >= 3 && lt.rfind(" desc") == lt.size() - 5)
                            dir = "DESC";
                        else if (lt.size() >= 3 && lt.rfind(" asc") == lt.size() - 4)
                            dir = "ASC";
                        if (!dir.empty()) {
                            size_t pos = token.find_last_of(' ');
                            std::string base =
                                (pos == std::string::npos) ? token : token.substr(0, pos);
                            ast.ddlIndex.columns.push_back(base);
                            ast.ddlIndex.column_directions.push_back({base, dir});
                            if (!coll.empty())
                                ast.ddlIndex.column_collates.push_back({base, coll});
                        } else {
                            ast.ddlIndex.columns.push_back(token);
                            ast.ddlIndex.column_directions.push_back({token, ""});
                            if (!coll.empty())
                                ast.ddlIndex.column_collates.push_back({token, coll});
                        }
                    };
                    for (char c : cols) {
                        if (c == '(')
                            depth++;
                        else if (c == ')')
                            depth = depth > 0 ? depth - 1 : 0;
                        if (c == ',' && depth == 0) {
                            flush(cur);
                            cur.clear();
                        } else
                            cur.push_back(c);
                    }
                    flush(cur);
                }
            }
            // WHERE condition (partial index)
            auto wherep = lowercase(s).find(" where ");
            if (wherep != std::string::npos) {
                ast.ddlIndex.where_raw = s.substr(wherep + 7);
            }
        }
        // options
        if (l.find(" active") != std::string::npos)
            ast.ddlIndex.options += "ACTIVE ";
        if (l.find(" inactive") != std::string::npos)
            ast.ddlIndex.options += "INACTIVE ";
        if (l.find(" desc") != std::string::npos)
            ast.ddlIndex.options += "DESC ";
        if (l.find(" asc") != std::string::npos)
            ast.ddlIndex.options += "ASC ";
        if (l.find(" include ") != std::string::npos)
            ast.ddlIndex.options += "INCLUDE ";
        trim(ast.ddlIndex.options);
        // ALTER INDEX ... REBUILD
        if (l.rfind("alter index", 0) == 0 && l.find(" rebuild") != std::string::npos)
            ast.ddlIndex.rebuild = true;
        // SET STATISTICS n
        {
            auto sp = l.find(" set statistics ");
            if (sp != std::string::npos) {
                ast.ddlIndex.statistics = s.substr(sp + 16);
                trim(ast.ddlIndex.statistics);
            }
        }
        return ast;
    }

    Ast parse_ddl_sequence(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlSequence;
        ast.ddlSequence.span = {0, int(sql.size())};
        std::string s = sql;
        trim(s);
        // CREATE SEQUENCE name | ALTER SEQUENCE name RESTART WITH n | SET GENERATOR name TO n
        auto words_end = s.find_first_of(" \t\n");
        std::string w1 = (words_end == std::string::npos) ? s : s.substr(0, words_end);
        std::string rest =
            (words_end == std::string::npos) ? std::string() : s.substr(words_end + 1);
        std::string l1 = lowercase(w1);
        if (l1 == "create" || l1 == "alter" || l1 == "set") {
            // naive name capture: last word before keyword WITH/TO
            auto topos = lowercase(s).find(" to ");
            auto withpos = lowercase(s).find(" with ");
            if (topos != std::string::npos || withpos != std::string::npos) {
                auto cut = (topos != std::string::npos) ? topos : withpos;
                auto before = s.substr(0, cut);
                auto sp = before.find_last_of(" \t\n");
                if (sp != std::string::npos)
                    ast.ddlSequence.name = before.substr(sp + 1);
                ast.ddlSequence.action = s.substr(cut);
                trim(ast.ddlSequence.action);
            } else {
                // CREATE SEQUENCE name
                auto sp = s.find_last_of(" \t\n");
                if (sp != std::string::npos)
                    ast.ddlSequence.name = s.substr(sp + 1);
            }
        }
        // Extract numerics
        {
            std::string la = lowercase(s);
            auto rpos = la.find("restart with ");
            if (rpos != std::string::npos) {
                auto after = s.substr(rpos + 13);
                long long v = 0;
                try {
                    v = std::stoll(after);
                } catch (...) {
                }
                ast.ddlSequence.start_with = v;
            }
            auto ipos = la.find("increment by ");
            if (ipos != std::string::npos) {
                auto after = s.substr(ipos + 13);
                long long v = 0;
                try {
                    v = std::stoll(after);
                } catch (...) {
                }
                ast.ddlSequence.increment_by = v;
            }
            if (la.find(" cycle") != std::string::npos)
                ast.ddlSequence.cycle = true;
        }
        // Validation: RESTART/SET GENERATOR should carry a numeric value
        if (!ast.ddlSequence.action.empty()) {
            bool has_digit = false;
            for (char c : ast.ddlSequence.action)
                if (std::isdigit(static_cast<unsigned char>(c))) {
                    has_digit = true;
                    break;
                }
            if (!has_digit) {
                ast.warnings.push_back("Sequence action is missing a numeric value");
            }
        }
        return ast;
    }

    Ast parse_ddl_domain(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlDomain;
        ast.ddlDomain.span = {0, int(sql.size())};
        std::string s = sql;
        trim(s);
        // CREATE DOMAIN name AS type [DEFAULT ...] [CHECK (...)] [COLLATE x]
        auto sp = s.find_first_of(" \t\n");
        if (sp != std::string::npos) {
            auto rest = s.substr(sp + 1);
            auto p2 = rest.find_first_not_of(" \t\n");
            if (p2 != std::string::npos) {
                auto after_kw = rest.substr(p2);
                // skip DOMAIN
                if (starts_with_ci(after_kw, "domain")) {
                    auto after_dom = after_kw.substr(6);
                    trim(after_dom);
                    auto name_end = after_dom.find_first_of(" \t\n");
                    ast.ddlDomain.name =
                        (name_end == std::string::npos) ? after_dom : after_dom.substr(0, name_end);
                    std::string tail = (name_end == std::string::npos) ? std::string()
                                                                       : after_dom.substr(name_end);
                    trim(tail);
                    // AS type ...
                    if (starts_with_ci(tail, "as")) {
                        auto t2 = tail.substr(2);
                        trim(t2);
                        // type until DEFAULT/CHECK/COLLATE
                        auto cut = lowercase(t2);
                        size_t stop = std::string::npos;
                        for (auto kw : {" default", " check", " collate"}) {
                            auto p = cut.find(kw);
                            if (p != std::string::npos)
                                stop = (stop == std::string::npos) ? p : std::min(stop, p);
                        }
                        ast.ddlDomain.type_raw =
                            (stop == std::string::npos) ? t2 : t2.substr(0, stop);
                        if (stop != std::string::npos) {
                            auto rest2 = t2.substr(stop);
                            auto l2 = lowercase(rest2);
                            auto dp = l2.find("default");
                            if (dp != std::string::npos) {
                                auto seg = rest2.substr(dp + 7);
                                trim(seg);
                                ast.ddlDomain.default_raw = seg;
                            }
                            auto cp = l2.find("check");
                            if (cp != std::string::npos) {
                                auto seg = rest2.substr(cp);
                                ast.ddlDomain.check_raw = seg;
                                // Diagnostic: missing parentheses
                                if (seg.find('(') == std::string::npos ||
                                    seg.find(')') == std::string::npos)
                                    ast.warnings.push_back("DOMAIN CHECK missing parentheses");
                            }
                            auto colp = l2.find("collate");
                            if (colp != std::string::npos) {
                                auto seg = rest2.substr(colp + 7);
                                trim(seg);
                                ast.ddlDomain.collate = seg;
                            }
                        }
                    }
                }
            }
        }
        return ast;
    }
    Ast parse_ddl_schema(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlSchema;
        ast.ddlSchema.span = {0, int(sql.size())};
        ast.ddlSchema.attrs = sql;
        std::string s = sql;
        std::string l = lowercase(s);
        auto kp = l.find(" schema ");
        if (kp != std::string::npos) {
            auto after = s.substr(kp + 8);
            auto sp = after.find_first_of(" \t\n");
            ast.ddlSchema.name = (sp == std::string::npos) ? after : after.substr(0, sp);
        }
        return ast;
    }

    Ast parse_ddl_dblink(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlDbLink;
        ast.ddlDbLink.span = {0, int(sql.size())};
        std::string s = sql;
        std::string l = lowercase(s);
        if (l.rfind("create database link", 0) == 0)
            ast.ddlDbLink.action = "CREATE";
        else if (l.rfind("alter database link", 0) == 0)
            ast.ddlDbLink.action = "ALTER";
        else if (l.rfind("drop database link", 0) == 0)
            ast.ddlDbLink.action = "DROP";
        auto lp = l.find(" link ");
        if (lp != std::string::npos) {
            auto after = s.substr(lp + 6);
            auto sp = after.find_first_of(" \t\n");
            ast.ddlDbLink.name = (sp == std::string::npos) ? after : after.substr(0, sp);
            ast.ddlDbLink.attrs = after.substr(sp == std::string::npos ? after.size() : sp);
        }
        return ast;
    }
} // namespace scratchbird::engine

namespace scratchbird::engine
{
    static void trim_inplace(std::string& s)
    {
        auto not_space = [](int ch) { return !std::isspace(ch); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
        s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    }

    Ast parse_grant_stmt(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlGrant;
        ast.grantStmt.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        auto onpos = ls.find(" on ");
        auto topos = ls.find(" to ");
        if (onpos != std::string::npos && topos != std::string::npos && topos > onpos) {
            ast.grantStmt.privileges = s.substr(6, onpos - 6);
            // split privileges by comma
            {
                std::string cur;
                for (char c : ast.grantStmt.privileges) {
                    if (c == ',') {
                        size_t a = cur.find_first_not_of(" \t\n");
                        size_t b = cur.find_last_not_of(" \t\n");
                        if (a != std::string::npos)
                            ast.grantStmt.privilege_list.push_back(cur.substr(a, b - a + 1));
                        cur.clear();
                    } else {
                        cur.push_back(c);
                    }
                }
                if (!cur.empty()) {
                    size_t a = cur.find_first_not_of(" \t\n");
                    size_t b = cur.find_last_not_of(" \t\n");
                    if (a != std::string::npos)
                        ast.grantStmt.privilege_list.push_back(cur.substr(a, b - a + 1));
                }
            }
            auto between = s.substr(onpos + 4, topos - (onpos + 4));
            auto sp = between.find(' ');
            if (sp != std::string::npos) {
                auto ot = lowercase(between.substr(0, sp));
                auto rest = between.substr(sp + 1);
                if (ot == "database" && lowercase(rest).rfind("link ", 0) == 0) {
                    ast.grantStmt.object_type = "database link";
                    ast.grantStmt.object_name = rest.substr(5);
                } else {
                    ast.grantStmt.object_type = ot;
                    ast.grantStmt.object_name = rest;
                }
            }
            auto tail = s.substr(topos + 4);
            ast.grantStmt.grantees = tail;
        }
        if (ls.find("with grant option") != std::string::npos)
            ast.grantStmt.with_grant_option = true;
        if (ls.find("with admin option") != std::string::npos)
            ast.grantStmt.admin_option = true;
        // Normalize privileges per object type (expanded allowlists)
        if (!ast.grantStmt.object_type.empty()) {
            std::vector<std::string> allowed;
            auto ot = ast.grantStmt.object_type;
            if (ot == "table" || ot == "view")
                allowed = {"SELECT",     "INSERT",  "UPDATE", "DELETE",
                           "REFERENCES", "TRIGGER", "ALTER"};
            else if (ot == "procedure" || ot == "function")
                allowed = {"EXECUTE", "ALTER"};
            else if (ot == "sequence")
                allowed = {"USAGE", "SELECT", "UPDATE", "ALTER"};
            else if (ot == "package")
                allowed = {"EXECUTE", "ALTER"};
            else if (ot == "exception" || ot == "domain" || ot == "generator")
                allowed = {"ALTER"};
            else if (ot == "database link")
                allowed = {"USAGE", "MANAGE"};
            else if (ot == "tablespace")
                allowed = {"USAGE", "CREATE IN", "ALTER"};
            else if (ot == "foreign server")
                allowed = {"USAGE", "ALTER"};
            else if (ot == "foreign table")
                allowed = {"SELECT", "INSERT", "UPDATE", "DELETE", "REFERENCES", "ALTER"};
            else if (ot == "publication")
                allowed = {"USAGE", "MANAGE"};
            else if (ot == "subscription")
                allowed = {"USAGE", "MANAGE"};
            else if (ot == "trace profile" || ot == "audit policy")
                allowed = {"MANAGE"};
            else if (ot == "job")
                allowed = {"USAGE", "EXECUTE", "MANAGE"};
            else if (ot == "schedule")
                allowed = {"USAGE", "MANAGE"};
            else if (ot == "cluster" || ot == "service")
                allowed = {"USAGE", "MANAGE"};
            else if (ot == "auth provider")
                allowed = {"MANAGE"};
            else if (ot == "backup" || ot == "backup_restore" || ot == "restore")
                allowed = {"BACKUP DATABASE", "RESTORE DATABASE", "BACKUP TABLESPACE"};
            if (!allowed.empty()) {
                std::vector<std::string> filtered;
                for (auto p : ast.grantStmt.privilege_list) {
                    auto up = p;
                    std::transform(up.begin(), up.end(), up.begin(), ::toupper);
                    if (std::find(allowed.begin(), allowed.end(), up) != allowed.end())
                        filtered.push_back(up);
                    else {
                        std::string ctx = ast.grantStmt.object_name.empty()
                                              ? std::string("")
                                              : (" '" + ast.grantStmt.object_name + "'");
                        ast.warnings.push_back("Unknown or invalid privilege '" + p +
                                               "' for object type '" + ot + "'" + ctx);
                    }
                }
                if (!filtered.empty())
                    ast.grantStmt.privilege_list = std::move(filtered);
            }
        }
        // PUBLIC grantee normalization (keep as-is in grantees)
        if (lowercase(ast.grantStmt.grantees).find("public") != std::string::npos) {
            // no-op: presence indicates global scope; leave raw
        }
        // If no comma found and privileges are multi-word system privilege, keep as one item
        if (ast.grantStmt.privilege_list.empty() && ls.find("grant ") == 0) {
            std::string p = ast.grantStmt.privileges;
            trim_inplace(p);
            if (!p.empty())
                ast.grantStmt.privilege_list.push_back(p);
        }
        return ast;
    }

    Ast parse_revoke_stmt(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlRevoke;
        ast.grantStmt.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        auto onpos = ls.find(" on ");
        auto frompos = ls.find(" from ");
        if (onpos != std::string::npos && frompos != std::string::npos && frompos > onpos) {
            ast.grantStmt.privileges = s.substr(7, onpos - 7);
            // split privileges
            {
                std::string cur;
                for (char c : ast.grantStmt.privileges) {
                    if (c == ',') {
                        size_t a = cur.find_first_not_of(" \t\n");
                        size_t b = cur.find_last_not_of(" \t\n");
                        if (a != std::string::npos)
                            ast.grantStmt.privilege_list.push_back(cur.substr(a, b - a + 1));
                        cur.clear();
                    } else {
                        cur.push_back(c);
                    }
                }
                if (!cur.empty()) {
                    size_t a = cur.find_first_not_of(" \t\n");
                    size_t b = cur.find_last_not_of(" \t\n");
                    if (a != std::string::npos)
                        ast.grantStmt.privilege_list.push_back(cur.substr(a, b - a + 1));
                }
            }
            auto between = s.substr(onpos + 4, frompos - (onpos + 4));
            auto sp = between.find(' ');
            if (sp != std::string::npos) {
                auto ot = lowercase(between.substr(0, sp));
                auto rest = between.substr(sp + 1);
                if (ot == "database" && lowercase(rest).rfind("link ", 0) == 0) {
                    ast.grantStmt.object_type = "database link";
                    ast.grantStmt.object_name = rest.substr(5);
                } else {
                    ast.grantStmt.object_type = ot;
                    ast.grantStmt.object_name = rest;
                }
            }
            auto tail = s.substr(frompos + 6);
            ast.grantStmt.grantees = tail;
        }
        // REVOKE GRANT OPTION FOR ...
        if (ls.find("revoke grant option for") == 0)
            ast.grantStmt.revoke_grant_option = true;
        // Warn on invalid privilege-object combos similar to GRANT; allow system privileges (no ON
        // clause)
        if (!ast.grantStmt.object_type.empty() && !ast.grantStmt.privilege_list.empty()) {
            std::vector<std::string> allowed;
            auto ot = ast.grantStmt.object_type;
            if (ot == "table" || ot == "view")
                allowed = {"SELECT",     "INSERT",  "UPDATE", "DELETE",
                           "REFERENCES", "TRIGGER", "ALTER"};
            else if (ot == "procedure" || ot == "function")
                allowed = {"EXECUTE", "ALTER"};
            else if (ot == "sequence")
                allowed = {"USAGE", "SELECT", "UPDATE", "ALTER"};
            else if (ot == "package")
                allowed = {"EXECUTE", "ALTER"};
            else if (ot == "database link")
                allowed = {"CONNECT", "QUERY", "USAGE"};
            if (!allowed.empty()) {
                for (auto p : ast.grantStmt.privilege_list) {
                    auto up = p;
                    std::transform(up.begin(), up.end(), up.begin(), ::toupper);
                    if (std::find(allowed.begin(), allowed.end(), up) == allowed.end()) {
                        std::string ctx = ast.grantStmt.object_name.empty()
                                              ? std::string("")
                                              : (" '" + ast.grantStmt.object_name + "'");
                        ast.warnings.push_back("Unknown or invalid privilege '" + p +
                                               "' for object type '" + ot + "'" + ctx);
                    }
                }
            }
        }
        return ast;
    }
} // namespace scratchbird::engine

// Secondary DDL stubs (minimal capture)
namespace scratchbird::engine
{
    Ast parse_ddl_view(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlView;
        ast.ddlView.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        auto aspos = ls.find(" as ");
        if (aspos != std::string::npos) {
            auto head = s.substr(0, aspos);
            auto sp = head.find_last_of(" \t\n");
            if (sp != std::string::npos)
                ast.ddlView.name = head.substr(sp + 1);
            ast.ddlView.body_raw = s.substr(aspos + 4);
        }
        if (ls.find("with check option") != std::string::npos) {
            ast.ddlView.with_check_option = true;
            if (ls.find(" with local check option") != std::string::npos)
                ast.ddlView.check_option_variant = "LOCAL";
            else if (ls.find(" with cascaded check option") != std::string::npos)
                ast.ddlView.check_option_variant = "CASCADED";
        }
        // Optional column list after view name: CREATE VIEW v(col1, col2) AS ...
        auto name_end = s.find(ast.ddlView.name);
        if (name_end != std::string::npos) {
            auto after = s.substr(name_end + ast.ddlView.name.size());
            auto lp = after.find('(');
            auto rp = after.find(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp)
                ast.ddlView.columns_raw = after.substr(lp + 1, rp - lp - 1);
            // Sanity: if we can detect a simple SELECT list, compare counts
            if (!ast.ddlView.columns_raw.empty()) {
                // Attempt to extract SELECT list inside body_raw before FROM/WHERE
                auto body_l = lowercase(ast.ddlView.body_raw);
                auto sel_pos = body_l.find("select ");
                if (sel_pos != std::string::npos) {
                    auto list = ast.ddlView.body_raw.substr(sel_pos + 7);
                    // cut at first FROM if present
                    auto fromp = lowercase(list).find(" from ");
                    if (fromp != std::string::npos)
                        list = list.substr(0, fromp);
                    // split top-level CSV for both
                    auto split_csv = [](const std::string& text) {
                        std::vector<std::string> out;
                        std::string cur;
                        int d = 0;
                        for (char c : text) {
                            if (c == '(')
                                d++;
                            else if (c == ')' && d > 0)
                                d--;
                            if (c == ',' && d == 0) {
                                std::string t = cur;
                                trim(t);
                                if (!t.empty())
                                    out.push_back(t);
                                cur.clear();
                            } else
                                cur.push_back(c);
                        }
                        std::string t = cur;
                        trim(t);
                        if (!t.empty())
                            out.push_back(t);
                        return out;
                    };
                    auto name_cols = split_csv(ast.ddlView.columns_raw);
                    auto proj_cols = split_csv(list);
                    if (!name_cols.empty() && !proj_cols.empty() &&
                        name_cols.size() != proj_cols.size()) {
                        ast.warnings.push_back(
                            "VIEW column list count does not match SELECT list count (heuristic)");
                    }
                }
            }
        }
        return ast;
    }

    Ast parse_ddl_collation(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlCollation;
        ast.ddlCollation.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        trim(s);
        // name is last token before options typically; get first name after keyword
        auto kp = ls.find("create collation ");
        if (kp != std::string::npos) {
            auto rest = s.substr(kp + 17);
            trim(rest);
            auto sp = rest.find_first_of(" \t\n");
            ast.ddlCollation.name = (sp == std::string::npos) ? rest : rest.substr(0, sp);
        } else {
            auto sp = s.find_last_of(" \t\n");
            if (sp != std::string::npos)
                ast.ddlCollation.name = s.substr(sp + 1);
        }
        // BASED ON name
        auto bop = ls.find(" based on ");
        if (bop != std::string::npos) {
            auto after = s.substr(bop + 10);
            auto sp = after.find_first_of(" \t\n");
            ast.ddlCollation.based_on = (sp == std::string::npos) ? after : after.substr(0, sp);
        }
        // FROM external / PAD SPACE tail
        auto fp = ls.find(" from ");
        if (fp != std::string::npos) {
            ast.ddlCollation.from_external = s.substr(fp + 6);
            trim(ast.ddlCollation.from_external);
        }
        return ast;
    }

    Ast parse_ddl_charset(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlCharset;
        ast.ddlCharset.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        trim(s);
        auto kp = ls.find("create character set ");
        if (kp != std::string::npos) {
            auto rest = s.substr(kp + 22);
            trim(rest);
            auto sp = rest.find_first_of(" \t\n");
            ast.ddlCharset.name = (sp == std::string::npos) ? rest : rest.substr(0, sp);
            ast.ddlCharset.attributes = rest;
        } else {
            auto sp = s.find_last_of(" \t\n");
            if (sp != std::string::npos)
                ast.ddlCharset.name = s.substr(sp + 1);
        }
        return ast;
    }

    Ast parse_ddl_exception(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlException;
        ast.ddlException.span = {0, int(sql.size())};
        std::string s = sql;
        trim(s);
        // CREATE EXCEPTION name 'message'
        auto sp = s.find_first_of(" \t\n");
        if (sp != std::string::npos) {
            auto tail = s.substr(sp + 1);
            auto p2 = tail.find_first_not_of(" \t\n");
            if (p2 != std::string::npos) {
                auto rest = tail.substr(p2);
                auto sp2 = rest.find_first_of(" \t\n");
                ast.ddlException.name = (sp2 == std::string::npos) ? rest : rest.substr(0, sp2);
                auto q = rest.find('\'');
                if (q != std::string::npos)
                    ast.ddlException.message = rest.substr(q);
            }
        }
        return ast;
    }

    static std::pair<std::string, std::string>
    parse_type_and_name_multi(const std::string& between_lc, const std::string& between_raw)
    {
        // Known multi-word object types
        static const char* multi[] = {"foreign server", "database link", "trace profile",
                                      "audit policy",   "auth provider", "materialized view",
                                      "foreign table"};
        for (auto* mw : multi) {
            std::string mws(mw);
            if (between_lc.rfind(mws + " ", 0) == 0) {
                std::string name = between_raw.substr(mws.size() + 1);
                return {mws, name};
            }
        }
        // Fallback: split first space
        auto sp = between_raw.find(' ');
        if (sp != std::string::npos) {
            return {between_raw.substr(0, sp), between_raw.substr(sp + 1)};
        }
        return {between_raw, std::string{}};
    }

    Ast parse_ddl_comment(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlComment;
        ast.ddlComment.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // COMMENT ON TABLE t IS '...'
        auto onpos = ls.find("comment on ");
        auto ispos = ls.find(" is ");
        if (onpos != std::string::npos && ispos != std::string::npos && ispos > onpos) {
            auto between = s.substr(onpos + 11, ispos - (onpos + 11));
            std::string between_trim = between;
            trim(between_trim);
            auto [ot, name] = parse_type_and_name_multi(lowercase(between_trim), between_trim);
            ast.ddlComment.object_type = ot;
            ast.ddlComment.object_name = name;
            ast.ddlComment.text = s.substr(ispos + 4);
        }
        return ast;
    }

    Ast parse_ddl_rename(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlRename;
        ast.ddlRename.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // ALTER TABLE t RENAME TO x | ALTER INDEX i RENAME TO x
        auto rpos = ls.find(" rename to ");
        if (rpos != std::string::npos) {
            auto before = s.substr(0, rpos);
            auto after = s.substr(rpos + 11);
            trim(after);
            ast.ddlRename.new_name = after;
            // Extract segment after "alter " and before old_name
            auto alter_sp = ls.find("alter ");
            std::string tail = (alter_sp != std::string::npos)
                                   ? s.substr(alter_sp + 6, rpos - (alter_sp + 6))
                                   : before;
            std::string tail_l = lowercase(tail);
            trim(tail);
            trim(tail_l);
            // tail is "<object-type> <old_name>"
            auto [ot, restname] = parse_type_and_name_multi(tail_l, tail);
            ast.ddlRename.object_type = ot;
            ast.ddlRename.old_name = restname;
        }
        return ast;
    }

    Ast parse_ddl_move(const std::string& sql)
    {
        // ALTER PROCEDURE|FUNCTION|PACKAGE name SET SCHEMA new_schema
        Ast ast{};
        ast.kind = NodeKind::DdlMove;
        ast.ddlMove.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        if (ls.rfind("alter ", 0) == 0 && ls.find(" set schema ") != std::string::npos) {
            // extract object type
            auto sp1 = ls.find(' ');
            auto sp2 = ls.find(' ', sp1 + 1);
            if (sp2 != std::string::npos) {
                ast.ddlMove.object_type =
                    ls.substr(sp1 + 1, sp2 - sp1 - 1); // procedure|function|package
                // name is token after type until " set schema "
                auto setpos = ls.find(" set schema ");
                std::string name = s.substr(sp2 + 1, setpos - (sp2 + 1));
                trim(name);
                ast.ddlMove.name = name;
                std::string schema = s.substr(setpos + std::string(" set schema ").size());
                trim(schema);
                ast.ddlMove.new_schema = schema;
            }
        }
        return ast;
    }

    Ast parse_ddl_tablespace(const std::string& sql)
    {
        // CREATE/ALTER/DROP/DETACH/ATTACH TABLESPACE ... | SET/MOVE TABLESPACE handled by
        // parse_ddl_table
        Ast ast{};
        ast.kind = NodeKind::DdlTablespace;
        ast.ddlTablespace.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        auto first_sp = ls.find(' ');
        if (first_sp == std::string::npos)
            return ast;
        auto second_sp = ls.find(' ', first_sp + 1);
        if (second_sp == std::string::npos)
            return ast;
        ast.ddlTablespace.action = ls.substr(0, first_sp); // create|alter|drop|detach|attach
        // after TABLESPACE keyword
        auto kw = ls.find("tablespace", second_sp);
        if (kw != std::string::npos) {
            auto name_start = kw + std::string("tablespace").size();
            // name is next token
            while (name_start < s.size() && std::isspace((unsigned char)s[name_start]))
                name_start++;
            size_t i = name_start;
            while (i < s.size() && !std::isspace((unsigned char)s[i]))
                i++;
            ast.ddlTablespace.name = s.substr(name_start, i - name_start);
            if (i < s.size())
                ast.ddlTablespace.attrs = s.substr(i);
            trim(ast.ddlTablespace.attrs);
        }
        return ast;
    }

    Ast parse_ddl_foreign_server(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlForeignServer;
        ast.ddlForeignServer.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // CREATE/ALTER/DROP FOREIGN SERVER name OPTIONS (...)
        ast.ddlForeignServer.action = ls.substr(0, ls.find(' '));
        auto p = ls.find(" foreign server ");
        if (p != std::string::npos) {
            auto after = s.substr(p + 16);
            trim(after);
            auto sp = after.find_first_of(" (\t\n");
            ast.ddlForeignServer.name = (sp == std::string::npos) ? after : after.substr(0, sp);
            if (sp != std::string::npos) {
                ast.ddlForeignServer.options = after.substr(sp + 1);
                trim(ast.ddlForeignServer.options);
            }
        }
        return ast;
    }

    Ast parse_ddl_user_mapping(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlUserMapping;
        ast.ddlUserMapping.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // CREATE/ALTER/DROP USER MAPPING FOR user SERVER server OPTIONS (...)
        ast.ddlUserMapping.action = ls.substr(0, ls.find(' '));
        auto forp = ls.find(" for ");
        auto servp = ls.find(" server ");
        if (forp != std::string::npos && servp != std::string::npos && servp > forp) {
            ast.ddlUserMapping.user_name = s.substr(forp + 5, servp - (forp + 5));
            auto after = s.substr(servp + 8);
            trim(after);
            auto sp = after.find_first_of(" (\t\n");
            ast.ddlUserMapping.server_name =
                (sp == std::string::npos) ? after : after.substr(0, sp);
            if (sp != std::string::npos) {
                ast.ddlUserMapping.options = after.substr(sp + 1);
                trim(ast.ddlUserMapping.options);
            }
        }
        return ast;
    }

    Ast parse_ddl_foreign_table(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlForeignTable;
        ast.ddlForeignTable.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // CREATE FOREIGN TABLE name (...) SERVER srv OPTIONS (...)
        ast.ddlForeignTable.action = ls.substr(0, ls.find(' '));
        auto ftp = ls.find(" foreign table ");
        if (ftp != std::string::npos) {
            auto after = s.substr(ftp + 15);
            trim(after);
            auto sp = after.find_first_of(" (\t\n");
            ast.ddlForeignTable.name = (sp == std::string::npos) ? after : after.substr(0, sp);
            // columns raw
            auto lp = after.find('(');
            auto rp = after.rfind(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp)
                ast.ddlForeignTable.columns_raw = after.substr(lp + 1, rp - lp - 1);
            auto servp = ls.find(" server ");
            if (servp != std::string::npos) {
                auto tail = s.substr(servp + 8);
                trim(tail);
                auto sp2 = tail.find_first_of(" (\t\n");
                ast.ddlForeignTable.server_name =
                    (sp2 == std::string::npos) ? tail : tail.substr(0, sp2);
                if (sp2 != std::string::npos) {
                    ast.ddlForeignTable.options = tail.substr(sp2 + 1);
                    trim(ast.ddlForeignTable.options);
                }
            }
        }
        return ast;
    }

    Ast parse_ddl_import_foreign_schema(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlImportForeignSchema;
        ast.ddlImportForeignSchema.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // IMPORT FOREIGN SCHEMA remote FROM SERVER srv INTO local OPTIONS (...)
        auto imp = ls.find("import foreign schema ");
        if (imp != std::string::npos) {
            auto after = s.substr(imp + 24);
            trim(after);
            auto fromp = lowercase(after).find(" from server ");
            if (fromp != std::string::npos) {
                ast.ddlImportForeignSchema.remote_schema = after.substr(0, fromp);
                auto tail = after.substr(fromp + 13);
                auto into = lowercase(tail).find(" into ");
                if (into != std::string::npos) {
                    ast.ddlImportForeignSchema.server_name = tail.substr(0, into);
                    auto rest = tail.substr(into + 6);
                    auto sp = rest.find_first_of(" (\t\n");
                    ast.ddlImportForeignSchema.into_schema =
                        (sp == std::string::npos) ? rest : rest.substr(0, sp);
                    if (sp != std::string::npos) {
                        ast.ddlImportForeignSchema.options = rest.substr(sp + 1);
                        trim(ast.ddlImportForeignSchema.options);
                    }
                }
            }
        }
        return ast;
    }

    Ast parse_ddl_publication(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlPublication;
        ast.ddlPublication.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        ast.ddlPublication.action = ls.substr(0, ls.find(' '));
        auto p = ls.find(" publication ");
        if (p != std::string::npos) {
            auto after = s.substr(p + 13);
            trim(after);
            auto sp = after.find_first_of(" (\t\n");
            ast.ddlPublication.name = (sp == std::string::npos) ? after : after.substr(0, sp);
            if (sp != std::string::npos) {
                ast.ddlPublication.options = after.substr(sp + 1);
                trim(ast.ddlPublication.options);
            }
        }
        return ast;
    }

    Ast parse_ddl_subscription(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlSubscription;
        ast.ddlSubscription.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        ast.ddlSubscription.action = ls.substr(0, ls.find(' '));
        auto p = ls.find(" subscription ");
        if (p != std::string::npos) {
            auto after = s.substr(p + 14);
            trim(after);
            auto sp = after.find_first_of(" (\t\n");
            ast.ddlSubscription.name = (sp == std::string::npos) ? after : after.substr(0, sp);
            if (sp != std::string::npos) {
                ast.ddlSubscription.options = after.substr(sp + 1);
                trim(ast.ddlSubscription.options);
            }
        }
        return ast;
    }

    Ast parse_ddl_trace_profile(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlTraceProfile;
        ast.ddlTraceProfile.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        ast.ddlTraceProfile.action = ls.substr(0, ls.find(' '));
        auto p = ls.find(" trace profile ");
        if (p != std::string::npos) {
            auto after = s.substr(p + 15);
            trim(after);
            auto sp = after.find_first_of(" (\t\n");
            ast.ddlTraceProfile.name = (sp == std::string::npos) ? after : after.substr(0, sp);
            if (sp != std::string::npos) {
                ast.ddlTraceProfile.options = after.substr(sp + 1);
                trim(ast.ddlTraceProfile.options);
            }
        }
        return ast;
    }

    Ast parse_ddl_audit_policy(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlAuditPolicy;
        ast.ddlAuditPolicy.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        ast.ddlAuditPolicy.action = ls.substr(0, ls.find(' '));
        auto p = ls.find(" audit policy ");
        if (p != std::string::npos) {
            auto after = s.substr(p + 14);
            trim(after);
            auto sp = after.find_first_of(" (\t\n");
            ast.ddlAuditPolicy.name = (sp == std::string::npos) ? after : after.substr(0, sp);
            if (sp != std::string::npos) {
                ast.ddlAuditPolicy.options = after.substr(sp + 1);
                trim(ast.ddlAuditPolicy.options);
            }
        }
        return ast;
    }

    static void capture_simple_object(const std::string& sql, const std::string& key,
                                      std::string& out_name, std::string& out_opts)
    {
        std::string s = sql;
        std::string ls = lowercase(s);
        auto p = ls.find(key);
        if (p != std::string::npos) {
            auto after = s.substr(p + key.size());
            trim(after);
            auto sp = after.find_first_of(" (\t\n");
            out_name = (sp == std::string::npos) ? after : after.substr(0, sp);
            if (sp != std::string::npos) {
                out_opts = after.substr(sp + 1);
                trim(out_opts);
            }
        }
    }

    Ast parse_ddl_cluster(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlCluster;
        ast.ddlCluster.span = {0, int(sql.size())};
        std::string ls = lowercase(sql);
        ast.ddlCluster.action = ls.substr(0, ls.find(' '));
        capture_simple_object(sql, " cluster ", ast.ddlCluster.name, ast.ddlCluster.options);
        return ast;
    }

    Ast parse_ddl_cluster_node(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlClusterNode;
        ast.ddlClusterNode.span = {0, int(sql.size())};
        std::string ls = lowercase(sql);
        ast.ddlClusterNode.action = ls.substr(0, ls.find(' '));
        capture_simple_object(sql, " cluster node ", ast.ddlClusterNode.name,
                              ast.ddlClusterNode.options);
        return ast;
    }

    Ast parse_ddl_cluster_service(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlClusterService;
        ast.ddlClusterService.span = {0, int(sql.size())};
        std::string ls = lowercase(sql);
        ast.ddlClusterService.action = ls.substr(0, ls.find(' '));
        capture_simple_object(sql, " cluster service ", ast.ddlClusterService.name,
                              ast.ddlClusterService.options);
        return ast;
    }

    Ast parse_ddl_auth_provider(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlAuthProvider;
        ast.ddlAuthProvider.span = {0, int(sql.size())};
        std::string ls = lowercase(sql);
        ast.ddlAuthProvider.action = ls.substr(0, ls.find(' '));
        capture_simple_object(sql, " auth provider ", ast.ddlAuthProvider.name,
                              ast.ddlAuthProvider.options);
        return ast;
    }

    Ast parse_ddl_rls_policy(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlRlsPolicy;
        ast.ddlRlsPolicy.span = {0, int(sql.size())};
        std::string ls = lowercase(sql);
        ast.ddlRlsPolicy.action = ls.substr(0, ls.find(' '));
        // CREATE/ALTER/DROP POLICY name ...
        capture_simple_object(sql, " policy ", ast.ddlRlsPolicy.name, ast.ddlRlsPolicy.options);
        return ast;
    }

    Ast parse_ddl_materialized_view(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlMaterializedView;
        ast.ddlMaterializedView.span = {0, int(sql.size())};
        std::string ls = lowercase(sql);
        // REFRESH MATERIALIZED VIEW name [WITH|WITHOUT DATA]
        if (ls.rfind("refresh materialized view", 0) == 0) {
            ast.ddlMaterializedView.action = "refresh";
            std::string after = sql.substr(26);
            trim(after);
            auto sp = after.find_first_of(" (\t\n");
            ast.ddlMaterializedView.name = (sp == std::string::npos) ? after : after.substr(0, sp);
            if (sp != std::string::npos) {
                ast.ddlMaterializedView.options = after.substr(sp + 1);
                trim(ast.ddlMaterializedView.options);
            }
            return ast;
        }
        // CREATE/ALTER/DROP MATERIALIZED VIEW name AS SELECT ...
        ast.ddlMaterializedView.action = ls.substr(0, ls.find(' '));
        capture_simple_object(sql, " materialized view ", ast.ddlMaterializedView.name,
                              ast.ddlMaterializedView.options);
        return ast;
    }

    Ast parse_ddl_role(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlRole;
        ast.ddlRole.span = {0, int(sql.size())};
        std::string s = sql;
        trim(s);
        // name is last token; attrs is full tail; ACTIVE/INACTIVE
        auto sp = s.find_last_of(" \t\n");
        if (sp != std::string::npos) {
            ast.ddlRole.name = s.substr(sp + 1);
            ast.ddlRole.attrs = s;
            auto ls = lowercase(s);
            if (ls.find(" inactive") != std::string::npos)
                ast.ddlRole.active = false;
        }
        return ast;
    }

    Ast parse_ddl_user(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlUser;
        ast.ddlUser.span = {0, int(sql.size())};
        std::string s = sql;
        trim(s);
        auto sp = s.find_last_of(" \t\n");
        if (sp != std::string::npos) {
            ast.ddlUser.name = s.substr(sp + 1);
            ast.ddlUser.attrs = s;
            auto ls = lowercase(s);
            // Extract quoted values after keywords
            auto findq = [&](const char* kw) -> std::string {
                auto p = ls.find(kw);
                if (p == std::string::npos)
                    return {};
                auto q1 = s.find('\'', p);
                if (q1 == std::string::npos)
                    return {};
                auto q2 = s.find('\'', q1 + 1);
                if (q2 == std::string::npos)
                    return {};
                return s.substr(q1 + 1, q2 - q1 - 1);
            };
            ast.ddlUser.password = findq(" password ");
            ast.ddlUser.first_name = findq(" firstname ");
            ast.ddlUser.last_name = findq(" lastname ");
            ast.ddlUser.middle_name = findq(" middlename ");
            if (ls.find(" inactive") != std::string::npos)
                ast.ddlUser.active = false;
        }
        return ast;
    }
} // namespace scratchbird::engine

namespace scratchbird::engine
{
    Ast parse_ddl_udf(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlUdf;
        ast.ddlUdf.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // DECLARE EXTERNAL FUNCTION name ... ENTRY_POINT / MODULE_NAME ...
        auto kw = ls.find("declare external function ");
        if (kw != std::string::npos) {
            auto rest = s.substr(kw + 27);
            trim_inplace(rest);
            auto end = rest.find_first_of(" \t\n");
            ast.ddlUdf.name = (end == std::string::npos) ? rest : rest.substr(0, end);
            ast.ddlUdf.attrs = rest.substr(end == std::string::npos ? rest.size() : end);
        }
        return ast;
    }

    Ast parse_ddl_udr(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlUdr;
        ast.ddlUdr.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // CREATE FUNCTION name ... EXTERNAL NAME '...' ENGINE UDR ...
        auto fpos = ls.find("create function ");
        if (fpos != std::string::npos) {
            auto rest = s.substr(fpos + 16);
            trim_inplace(rest);
            auto end = rest.find_first_of(" \t\n(");
            ast.ddlUdr.name = (end == std::string::npos) ? rest : rest.substr(0, end);
            auto en = ls.find("external name ");
            if (en != std::string::npos) {
                auto q = s.find('\'', en);
                auto q2 = s.find('\'', q + 1);
                if (q != std::string::npos && q2 != std::string::npos)
                    ast.ddlUdr.external_name = s.substr(q + 1, q2 - q - 1);
            }
            auto eng = ls.find(" engine ");
            if (eng != std::string::npos) {
                auto after = s.substr(eng + 8);
                auto e2 = after.find_first_of(" \t\n");
                ast.ddlUdr.engine = (e2 == std::string::npos) ? after : after.substr(0, e2);
            }
            // Tail attrs raw
            ast.ddlUdr.attrs = s.substr(fpos);
        }
        return ast;
    }

    Ast parse_ddl_mapping(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlMapping;
        ast.ddlMapping.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // CREATE MAPPING name USING ... FROM ... TO ... (raw capture)
        auto mpos = ls.find("create mapping ");
        if (mpos != std::string::npos) {
            auto rest = s.substr(mpos + 16);
            trim_inplace(rest);
            auto end = rest.find_first_of(" \t\n");
            ast.ddlMapping.name = (end == std::string::npos) ? rest : rest.substr(0, end);
            ast.ddlMapping.attrs = rest;
        }
        return ast;
    }

    Ast parse_ddl_gtt(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlGtt;
        ast.ddlGtt.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // CREATE GLOBAL TEMPORARY TABLE name (...) ON COMMIT {PRESERVE|DELETE} ROWS
        auto gpos = ls.find("create global temporary table ");
        if (gpos != std::string::npos) {
            auto rest = s.substr(gpos + 30);
            trim_inplace(rest);
            auto end = rest.find_first_of(" \t\n(");
            ast.ddlGtt.name = (end == std::string::npos) ? rest : rest.substr(0, end);
            auto lp = rest.find('(');
            auto rp = rest.rfind(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp)
                ast.ddlGtt.columns_raw = rest.substr(lp + 1, rp - lp - 1);
            auto oc = ls.find("on commit ");
            if (oc != std::string::npos) {
                auto after = s.substr(oc + 10);
                if (lowercase(after).find("preserve") != std::string::npos)
                    ast.ddlGtt.on_commit = "PRESERVE ROWS";
                else if (lowercase(after).find("delete") != std::string::npos)
                    ast.ddlGtt.on_commit = "DELETE ROWS";
            }
        }
        return ast;
    }

    Ast parse_ddl_blob_filter(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::DdlBlobFilter;
        ast.ddlBlobFilter.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // DECLARE FILTER name INPUT_TYPE x OUTPUT_TYPE y ENTRY_POINT 'ep' MODULE_NAME 'lib'
        // CREATE FILTER ...  | DROP FILTER name
        auto fpos = ls.find(" filter ");
        if (fpos != std::string::npos) {
            // name after FILTER
            auto after = s.substr(fpos + 8);
            auto sp = after.find_first_not_of(" \t\n");
            if (sp != std::string::npos) {
                auto rest = after.substr(sp);
                auto end = rest.find_first_of(" \t\n");
                ast.ddlBlobFilter.name = end == std::string::npos ? rest : rest.substr(0, end);
            }
        }
        // input/output types
        auto ip = ls.find(" input_type ");
        if (ip != std::string::npos) {
            auto after = s.substr(ip + 12);
            auto end = after.find_first_of(" \t\n");
            ast.ddlBlobFilter.input_type = end == std::string::npos ? after : after.substr(0, end);
        }
        auto op = ls.find(" output_type ");
        if (op != std::string::npos) {
            auto after = s.substr(op + 13);
            auto end = after.find_first_of(" \t\n");
            ast.ddlBlobFilter.output_type = end == std::string::npos ? after : after.substr(0, end);
        }
        // entry/module quotes
        auto ep = ls.find(" entry_point ");
        if (ep != std::string::npos) {
            auto q = s.find('\'', ep);
            auto q2 = s.find('\'', q + 1);
            if (q != std::string::npos && q2 != std::string::npos)
                ast.ddlBlobFilter.entry_point = s.substr(q + 1, q2 - q - 1);
        }
        auto mp = ls.find(" module_name ");
        if (mp != std::string::npos) {
            auto q = s.find('\'', mp);
            auto q2 = s.find('\'', q + 1);
            if (q != std::string::npos && q2 != std::string::npos)
                ast.ddlBlobFilter.module_name = s.substr(q + 1, q2 - q - 1);
        }
        ast.ddlBlobFilter.attrs = s;
        return ast;
    }
} // namespace scratchbird::engine
