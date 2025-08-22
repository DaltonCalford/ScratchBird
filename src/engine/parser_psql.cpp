// SPDX-License-Identifier: IDPL
#include "scratchbird/engine/parser_psql.h"

#include <algorithm>
#include <cctype>

namespace scratchbird::engine
{
    // Lightweight type parser to avoid including parser_expr.h
    struct TypeDescriptorLite {
        std::string name;
        int length{-1};
        int precision{-1};
        int scale{-1};
        std::string charset;
        std::string collate;
        int array_rank{0};
    };
    static TypeDescriptorLite parse_type_descriptor_lite(const std::string& type_sql)
    {
        TypeDescriptorLite td{};
        std::string s = type_sql;
        auto lower = [&](std::string x) {
            std::transform(x.begin(), x.end(), x.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
            return x;
        };
        auto ls = lower(s);
        // name before space or '('
        size_t cut = s.find_first_of(" (\t\n");
        td.name = (cut == std::string::npos) ? s : s.substr(0, cut);
        // length/precision
        auto lp = s.find('(');
        auto rp = s.find(')', lp);
        if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
            auto inside = s.substr(lp + 1, rp - lp - 1);
            size_t comma = inside.find(',');
            try {
                if (comma == std::string::npos)
                    td.length = std::stoi(inside);
                else {
                    td.precision = std::stoi(inside.substr(0, comma));
                    td.scale = std::stoi(inside.substr(comma + 1));
                }
            } catch (...) {
            }
        }
        // charset
        auto csp = lower(s).find("character set ");
        if (csp != std::string::npos) {
            td.charset = s.substr(csp + 14);
            td.charset = td.charset.substr(0, td.charset.find_first_of(" ,\t\n"));
        }
        // collate
        auto colp = lower(s).find("collate ");
        if (colp != std::string::npos) {
            td.collate = s.substr(colp + 8);
            td.collate = td.collate.substr(0, td.collate.find_first_of(" ,\t\n"));
        }
        // arrays
        size_t arr = 0;
        for (size_t i = 0; i < s.size(); ++i)
            if (s[i] == '[')
                arr++;
        td.array_rank = int(arr);
        return td;
    }
} // namespace scratchbird::engine

namespace scratchbird::engine
{
    namespace
    {
        static inline void push_warn(Ast& ast, int start, int end, const std::string& msg)
        {
            ast.warnings.push_back(msg);
            ast.warning_spans.push_back({start, end});
        }
        // reserved for future clause-level recovery in PSQL body if needed
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
    } // namespace

    Ast parse_psql_block(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::PsqlBlock;
        ast.psqlBlock.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // EXECUTE BLOCK [(...)] [RETURNS (...)] AS BEGIN ... END
        auto aspos = ls.find(" as ");
        auto beginpos = ls.find(" begin", aspos == std::string::npos ? 0 : aspos);
        auto endpos = ls.rfind(" end");
        if (beginpos != std::string::npos && endpos != std::string::npos && endpos > beginpos) {
            // params / returns capture
            auto head = s.substr(0, aspos == std::string::npos ? beginpos : aspos);
            auto lpar = head.find('(');
            auto rpar = head.find(')');
            if (lpar != std::string::npos && rpar != std::string::npos && rpar > lpar)
                ast.psqlBlock.params_raw = head.substr(lpar + 1, rpar - lpar - 1);
            auto rpos = lowercase(head).find("returns");
            if (rpos != std::string::npos) {
                auto l2 = head.find('(', rpos);
                auto r2 = head.find(')', l2);
                if (l2 != std::string::npos && r2 != std::string::npos && r2 > l2)
                    ast.psqlBlock.returns_raw = head.substr(l2 + 1, r2 - l2 - 1);
            }
            auto body = s.substr(beginpos + 7, endpos - (beginpos + 7));
            // split statements respecting nested BEGIN...END blocks
            size_t pos = 0;
            while (pos < body.size()) {
                size_t i = pos;
                int depth = 0;
                bool in_sq = false, in_dq = false;
                size_t stmt_end = std::string::npos;
                for (; i < body.size(); ++i) {
                    char c = body[i];
                    if (c == '\'' && !in_dq)
                        in_sq = !in_sq;
                    else if (c == '"' && !in_sq)
                        in_dq = !in_dq;
                    if (in_sq || in_dq)
                        continue;
                    // BEGIN/END detection (case-insensitive)
                    if (i + 5 <= body.size()) {
                        auto seg = lowercase(body.substr(i, 5));
                        if (seg == "begin") {
                            depth++;
                            i += 4;
                            continue;
                        }
                        if (seg == " end" || seg == "\nend" || seg == "\tend" || seg == " end;") {
                            if (depth > 0)
                                depth--;
                        }
                    }
                    if (body[i] == ';' && depth == 0) {
                        stmt_end = i;
                        break;
                    }
                }
                auto piece = body.substr(pos, stmt_end == std::string::npos ? std::string::npos
                                                                            : stmt_end - pos);
                std::string t = piece;
                trim(t);
                if (!t.empty()) {
                    Ast::PsqlStmt st{};
                    st.raw = t;
                    auto lt = lowercase(t);
                    // handle nested block immediately and classify inner statements
                    if (lt.rfind("begin", 0) == 0 && lt.size() > 5) {
                        // Strip outer BEGIN ... END
                        auto inner = t.substr(5);
                        auto ep = lowercase(inner).rfind("end");
                        if (ep != std::string::npos)
                            inner = inner.substr(0, ep);
                        size_t j = 0;
                        while (j < inner.size()) {
                            // split nested body; recover if commas or missing semicolons by
                            // scanning to sentinel 'END'
                            auto sc2 = inner.find(';', j);
                            std::string sub = inner.substr(
                                j, sc2 == std::string::npos ? std::string::npos : sc2 - j);
                            trim(sub);
                            if (!sub.empty()) {
                                Ast::PsqlStmt subSt{};
                                subSt.raw = sub;
                                auto ls2 = lowercase(sub);
                                if (ls2.rfind("if ", 0) == 0)
                                    subSt.kind = Ast::PsqlStmtKind::If;
                                else if (ls2.rfind("for select ", 0) == 0)
                                    subSt.kind = Ast::PsqlStmtKind::ForSelect;
                                else if (ls2.rfind("declare ", 0) == 0)
                                    subSt.kind = Ast::PsqlStmtKind::Declare;
                                else if (ls2.rfind("exception", 0) == 0 ||
                                         ls2.rfind("when ", 0) == 0)
                                    subSt.kind = Ast::PsqlStmtKind::Exception;
                                else if (ls2 == "suspend")
                                    subSt.kind = Ast::PsqlStmtKind::Suspend;
                                else if (ls2.rfind("return", 0) == 0)
                                    subSt.kind = Ast::PsqlStmtKind::Return;
                                else if (ls2.rfind("execute statement", 0) == 0)
                                    subSt.kind = Ast::PsqlStmtKind::ExecStmt;
                                else if (ls2.rfind("while ", 0) == 0)
                                    subSt.kind = Ast::PsqlStmtKind::While;
                                else if (ls2.rfind("leave", 0) == 0 || ls2.rfind("exit", 0) == 0)
                                    subSt.kind = Ast::PsqlStmtKind::Leave;
                                else if (ls2 == "continue")
                                    subSt.kind = Ast::PsqlStmtKind::Continue;
                                else if (ls2.rfind("open ", 0) == 0)
                                    subSt.kind = Ast::PsqlStmtKind::OpenCursor;
                                else if (ls2.rfind("fetch ", 0) == 0)
                                    subSt.kind = Ast::PsqlStmtKind::FetchCursor;
                                else if (ls2.rfind("close ", 0) == 0)
                                    subSt.kind = Ast::PsqlStmtKind::CloseCursor;
                                else
                                    subSt.kind = Ast::PsqlStmtKind::Unknown;
                                st.nested.push_back(std::move(subSt));
                            }
                            if (sc2 == std::string::npos) {
                                break;
                            }
                            j = sc2 + 1;
                        }
                    }
                    if (lt.rfind("if ", 0) == 0)
                        st.kind = Ast::PsqlStmtKind::If;
                    else if (lt.rfind("for select ", 0) == 0) {
                        st.kind = Ast::PsqlStmtKind::ForSelect;
                        // capture query until INTO and INTO var list
                        auto into_pos = lt.find(" into ");
                        if (into_pos != std::string::npos) {
                            st.for_query_raw = t.substr(11, into_pos - 11);
                            auto vars = t.substr(into_pos + 6);
                            // split by commas
                            size_t vp = 0;
                            while (vp < vars.size()) {
                                auto comma = vars.find(',', vp);
                                auto tok =
                                    vars.substr(vp, comma == std::string::npos ? std::string::npos
                                                                               : comma - vp);
                                trim(tok);
                                if (!tok.empty())
                                    st.into_vars.push_back(tok);
                                if (comma == std::string::npos) {
                                    break;
                                }
                                vp = comma + 1;
                            }
                            // Heuristic: compare INTO count vs SELECT projection count
                            auto proj_text = st.for_query_raw; // expected to start with 'SELECT'
                            // cut leading SELECT
                            auto lq = lowercase(proj_text);
                            if (lq.rfind("select ", 0) == 0) {
                                // take substring after 'select '
                                std::string list = proj_text.substr(7);
                                // stop at top-level FROM
                                int depth = 0;
                                bool in_sq2 = false, in_dq2 = false;
                                size_t stop = list.size();
                                for (size_t k = 0; k < list.size(); ++k) {
                                    char c2 = list[k];
                                    if (c2 == '\'' && !in_dq2)
                                        in_sq2 = !in_sq2;
                                    else if (c2 == '"' && !in_sq2)
                                        in_dq2 = !in_dq2;
                                    if (in_sq2 || in_dq2)
                                        continue;
                                    if (c2 == '(')
                                        depth++;
                                    else if (c2 == ')' && depth > 0)
                                        depth--;
                                    if (depth == 0 && k + 5 <= list.size()) {
                                        auto seg = lowercase(list.substr(k, 5));
                                        if (seg == " from") {
                                            stop = k;
                                            break;
                                        }
                                    }
                                }
                                list = list.substr(0, stop);
                                // count top-level CSV
                                int proj_count = 0;
                                depth = 0;
                                in_sq2 = false;
                                in_dq2 = false;
                                bool token_started = false;
                                for (size_t k = 0; k < list.size(); ++k) {
                                    char c2 = list[k];
                                    if (c2 == '\'' && !in_dq2)
                                        in_sq2 = !in_sq2;
                                    else if (c2 == '"' && !in_sq2)
                                        in_dq2 = !in_dq2;
                                    if (in_sq2 || in_dq2) {
                                        token_started = true;
                                        continue;
                                    }
                                    if (c2 == '(')
                                        depth++;
                                    else if (c2 == ')' && depth > 0)
                                        depth--;
                                    if (depth == 0 && c2 == ',') {
                                        proj_count++;
                                        token_started = false;
                                    } else if (!std::isspace((unsigned char)c2))
                                        token_started = true;
                                }
                                if (token_started)
                                    proj_count++;
                                if (proj_count > 0 && !st.into_vars.empty() &&
                                    (int)st.into_vars.size() != proj_count) {
                                    push_warn(ast, int(ast.psqlBlock.span.start),
                                              int(ast.psqlBlock.span.end),
                                              "FOR SELECT INTO variable count mismatch with "
                                              "projection (heuristic)");
                                }
                            }
                        } else {
                            st.for_query_raw = t.substr(11);
                        }
                    } else if (lt.rfind("declare ", 0) == 0) {
                        st.kind = Ast::PsqlStmtKind::Declare;
                        auto after = t.substr(8);
                        trim(after);
                        // cursor: DECLARE c CURSOR FOR SELECT ...
                        if (lowercase(after).find(" cursor ") != std::string::npos) {
                            st.declare_is_cursor = true;
                            auto sp = after.find_first_of(" \t\n");
                            st.name = (sp == std::string::npos) ? after : after.substr(0, sp);
                        } else {
                            // var: DECLARE x TYPE
                            auto sp = after.find_first_of(" \t\n");
                            st.name = (sp == std::string::npos) ? after : after.substr(0, sp);
                            if (sp != std::string::npos) {
                                st.type_raw = after.substr(sp + 1);
                                trim(st.type_raw);
                                if (st.type_raw.empty())
                                    push_warn(ast, int(ast.psqlBlock.span.start),
                                              int(ast.psqlBlock.span.end),
                                              "DECLARE missing type for variable '" + st.name +
                                                  "'");
                                else {
                                    // Basic type parsing using lite parser
                                    auto td = parse_type_descriptor_lite(st.type_raw);
                                    st.decl_type.name = td.name;
                                    st.decl_type.length = td.length;
                                    st.decl_type.precision = td.precision;
                                    st.decl_type.scale = td.scale;
                                    st.decl_type.charset = td.charset;
                                    st.decl_type.array_rank = td.array_rank;
                                }
                            }
                        }
                    } else if (lt.rfind("exception", 0) == 0 || lt.rfind("when ", 0) == 0) {
                        st.kind = Ast::PsqlStmtKind::Exception;
                        if (lt.rfind("when ", 0) == 0) {
                            // WHEN cond DO body
                            auto dop = lt.find(" do ");
                            if (dop != std::string::npos) {
                                st.when_has_do = true;
                                st.when_condition_raw = t.substr(5, dop - 5);
                                if (dop + 4 >= lt.size())
                                    push_warn(ast, int(ast.psqlBlock.span.start),
                                              int(ast.psqlBlock.span.end), "WHEN without DO body");
                            } else {
                                st.when_condition_raw = t.substr(5);
                                push_warn(ast, int(ast.psqlBlock.span.start),
                                          int(ast.psqlBlock.span.end), "WHEN without DO body");
                            }
                        }
                    } else if (lt == "suspend")
                        st.kind = Ast::PsqlStmtKind::Suspend;
                    else if (lt.rfind("return", 0) == 0)
                        st.kind = Ast::PsqlStmtKind::Return;
                    else if (lt.rfind("execute statement", 0) == 0) {
                        st.kind = Ast::PsqlStmtKind::ExecStmt;
                        // Options: WITH CALLER PRIVILEGES | AS USER 'u' PASSWORD 'p' [ROLE 'r']
                        // ON EXTERNAL 'conn' | WITH BIND | TIMEOUT n | INTO varlist
                        auto lower = [](std::string s) {
                            std::transform(s.begin(), s.end(), s.begin(),
                                           [](unsigned char c) { return char(std::tolower(c)); });
                            return s;
                        };
                        auto find_quoted = [](const std::string& s, size_t pos) -> std::string {
                            auto q1 = s.find('\'', pos);
                            if (q1 == std::string::npos)
                                return {};
                            auto q2 = s.find('\'', q1 + 1);
                            if (q2 == std::string::npos)
                                return {};
                            return s.substr(q1 + 1, q2 - q1 - 1);
                        };
                        std::string lraw = lower(t);
                        if (lraw.find(" with caller privileges") != std::string::npos)
                            st.exec_opts.caller_privileges = true;
                        // AS USER
                        {
                            auto p = lraw.find(" as user ");
                            if (p != std::string::npos) {
                                st.exec_opts.as_user = find_quoted(t, p);
                                if (st.exec_opts.as_user.empty()) {
                                    push_warn(ast, int(ast.psqlBlock.span.start),
                                              int(ast.psqlBlock.span.end),
                                              "EXECUTE STATEMENT AS USER missing quoted username");
                                }
                            }
                        }
                        // PASSWORD
                        {
                            auto p = lraw.find(" password ");
                            if (p != std::string::npos) {
                                st.exec_opts.password = find_quoted(t, p);
                                if (st.exec_opts.password.empty()) {
                                    push_warn(ast, int(ast.psqlBlock.span.start),
                                              int(ast.psqlBlock.span.end),
                                              "EXECUTE STATEMENT PASSWORD missing quoted value");
                                }
                            }
                        }
                        // ROLE
                        {
                            auto p = lraw.find(" role ");
                            if (p != std::string::npos) {
                                st.exec_opts.role = find_quoted(t, p);
                                if (st.exec_opts.role.empty()) {
                                    push_warn(ast, int(ast.psqlBlock.span.start),
                                              int(ast.psqlBlock.span.end),
                                              "EXECUTE STATEMENT ROLE missing quoted value");
                                }
                            }
                        }
                        // ON EXTERNAL
                        {
                            auto p = lraw.find(" on external ");
                            if (p != std::string::npos) {
                                st.exec_opts.on_external = find_quoted(t, p);
                                if (st.exec_opts.on_external.empty()) {
                                    push_warn(ast, int(ast.psqlBlock.span.start),
                                              int(ast.psqlBlock.span.end),
                                              "EXECUTE STATEMENT ON EXTERNAL missing quoted "
                                              "connection string");
                                }
                            }
                        }
                        // WITH BIND / BIND option
                        {
                            auto p = lraw.find(" with bind ");
                            if (p != std::string::npos)
                                st.exec_opts.bind_option = t.substr(p + 11);
                        }
                        // TIMEOUT n (take token after TIMEOUT)
                        {
                            auto p = lraw.find(" timeout ");
                            if (p != std::string::npos) {
                                auto tail = t.substr(p + 8);
                                std::string v;
                                size_t i = 0;
                                while (i < tail.size() && std::isspace((unsigned char)tail[i]))
                                    ++i;
                                while (i < tail.size() && std::isdigit((unsigned char)tail[i])) {
                                    v.push_back(tail[i]);
                                    ++i;
                                }
                                st.exec_opts.timeout = v;
                                if (st.exec_opts.timeout.empty()) {
                                    push_warn(ast, int(ast.psqlBlock.span.start),
                                              int(ast.psqlBlock.span.end),
                                              "EXECUTE STATEMENT TIMEOUT missing numeric value");
                                }
                            }
                        }
                        // INTO vars
                        {
                            auto into_pos3 = lraw.find(" into ");
                            if (into_pos3 != std::string::npos) {
                                auto vars3 = t.substr(into_pos3 + 6);
                                size_t vp3 = 0;
                                while (vp3 < vars3.size()) {
                                    auto comma3 = vars3.find(',', vp3);
                                    auto tok3 = vars3.substr(vp3, comma3 == std::string::npos
                                                                      ? std::string::npos
                                                                      : comma3 - vp3);
                                    trim(tok3);
                                    if (!tok3.empty())
                                        st.into_vars.push_back(tok3);
                                    if (comma3 == std::string::npos) {
                                        break;
                                    }
                                    vp3 = comma3 + 1;
                                }
                                if (st.into_vars.empty()) {
                                    push_warn(ast, int(ast.psqlBlock.span.start),
                                              int(ast.psqlBlock.span.end),
                                              "EXECUTE STATEMENT INTO has no variables");
                                }
                            }
                        }
                    } else if (lt.rfind("execute procedure", 0) == 0) {
                        st.kind = Ast::PsqlStmtKind::ExecProc;
                        // EXECUTE PROCEDURE name(args)
                        auto after = t.substr(18);
                        trim(after);
                        // name until '('
                        auto lp = after.find('(');
                        if (lp != std::string::npos) {
                            auto rp = after.rfind(')');
                            auto inside = (rp != std::string::npos && rp > lp)
                                              ? after.substr(lp + 1, rp - lp - 1)
                                              : std::string();
                            // split args by commas at top level
                            size_t p = 0;
                            while (p < inside.size()) {
                                auto c = inside.find(',', p);
                                auto tok = inside.substr(
                                    p, c == std::string::npos ? std::string::npos : c - p);
                                trim(tok);
                                if (!tok.empty())
                                    st.args.push_back(tok);
                                if (c == std::string::npos)
                                    break;
                                p = c + 1;
                            }
                        }
                        // INTO vars
                        auto ltp = lowercase(after);
                        auto ip = ltp.find(" into ");
                        if (ip != std::string::npos) {
                            auto varseg = after.substr(ip + 6);
                            size_t vp = 0;
                            while (vp < varseg.size()) {
                                auto c = varseg.find(',', vp);
                                auto tok = varseg.substr(
                                    vp, c == std::string::npos ? std::string::npos : c - vp);
                                trim(tok);
                                if (!tok.empty())
                                    st.into_vars.push_back(tok);
                                if (c == std::string::npos)
                                    break;
                                vp = c + 1;
                            }
                            if (st.into_vars.empty()) {
                                push_warn(ast, int(ast.psqlBlock.span.start),
                                          int(ast.psqlBlock.span.end),
                                          "EXECUTE PROCEDURE INTO has no variables");
                            }
                        }
                    } else if (lt.rfind("call ", 0) == 0) {
                        st.kind = Ast::PsqlStmtKind::Call;
                        auto after = t.substr(5);
                        // split args if present
                        auto lp = after.find('(');
                        if (lp != std::string::npos) {
                            auto rp = after.rfind(')');
                            auto inside = (rp != std::string::npos && rp > lp)
                                              ? after.substr(lp + 1, rp - lp - 1)
                                              : std::string();
                            size_t p = 0;
                            while (p < inside.size()) {
                                auto c = inside.find(',', p);
                                auto tok = inside.substr(
                                    p, c == std::string::npos ? std::string::npos : c - p);
                                trim(tok);
                                if (!tok.empty())
                                    st.args.push_back(tok);
                                if (c == std::string::npos)
                                    break;
                                p = c + 1;
                            }
                        }
                        // INTO vars
                        auto lta = lowercase(after);
                        auto ip = lta.find(" into ");
                        if (ip != std::string::npos) {
                            auto varseg = after.substr(ip + 6);
                            size_t vp = 0;
                            while (vp < varseg.size()) {
                                auto c = varseg.find(',', vp);
                                auto tok = varseg.substr(
                                    vp, c == std::string::npos ? std::string::npos : c - vp);
                                trim(tok);
                                if (!tok.empty())
                                    st.into_vars.push_back(tok);
                                if (c == std::string::npos)
                                    break;
                                vp = c + 1;
                            }
                            if (st.into_vars.empty()) {
                                push_warn(ast, int(ast.psqlBlock.span.start),
                                          int(ast.psqlBlock.span.end),
                                          "CALL INTO has no variables");
                            }
                        }
                    }
                    // DML with RETURNING ... INTO ...
                    else if (lt.rfind("insert ", 0) == 0 || lt.rfind("update ", 0) == 0 ||
                             lt.rfind("delete ", 0) == 0) {
                        // Detect RETURNING
                        auto rpos = lt.find(" returning ");
                        if (rpos != std::string::npos) {
                            // isolate returning list and var list after INTO
                            auto after_ret = t.substr(rpos + 11);
                            auto l_after = lowercase(after_ret);
                            auto ip = l_after.find(" into ");
                            std::string ret_list =
                                (ip == std::string::npos) ? after_ret : after_ret.substr(0, ip);
                            std::string var_list = (ip == std::string::npos)
                                                       ? std::string()
                                                       : after_ret.substr(ip + 6);
                            // split ret_list at top-level commas respecting quotes/parentheses
                            auto split_top_csv = [](const std::string& s) {
                                std::vector<std::string> out;
                                std::string cur;
                                int depth = 0;
                                bool in_sq = false, in_dq = false;
                                for (char c : s) {
                                    if (c == '\'' && !in_dq)
                                        in_sq = !in_sq;
                                    else if (c == '"' && !in_sq)
                                        in_dq = !in_dq;
                                    if (!in_sq && !in_dq) {
                                        if (c == '(')
                                            depth++;
                                        else if (c == ')' && depth > 0)
                                            depth--;
                                        if (c == ',' && depth == 0) {
                                            std::string t = cur;
                                            trim(t);
                                            if (!t.empty())
                                                out.push_back(t);
                                            cur.clear();
                                            continue;
                                        }
                                    }
                                    cur.push_back(c);
                                }
                                std::string t = cur;
                                trim(t);
                                if (!t.empty())
                                    out.push_back(t);
                                return out;
                            };
                            auto rets = split_top_csv(ret_list);
                            if (!var_list.empty()) {
                                size_t vp = 0;
                                while (vp < var_list.size()) {
                                    auto c = var_list.find(',', vp);
                                    auto tok = var_list.substr(
                                        vp, c == std::string::npos ? std::string::npos : c - vp);
                                    trim(tok);
                                    if (!tok.empty())
                                        st.into_vars.push_back(tok);
                                    if (c == std::string::npos)
                                        break;
                                    vp = c + 1;
                                }
                            }
                            if (!rets.empty() && !st.into_vars.empty() &&
                                (int)st.into_vars.size() != (int)rets.size()) {
                                push_warn(ast, int(ast.psqlBlock.span.start),
                                          int(ast.psqlBlock.span.end),
                                          "DML RETURNING INTO variable count mismatch (heuristic)");
                            }
                        }
                    } else if (lt.rfind("raise ", 0) == 0)
                        st.kind = Ast::PsqlStmtKind::Raise;
                    else if (lt.rfind("post event ", 0) == 0)
                        st.kind = Ast::PsqlStmtKind::PostEvent;
                    else if (lt.rfind("while ", 0) == 0)
                        st.kind = Ast::PsqlStmtKind::While;
                    else if (lt.rfind("case ", 0) == 0 || lt == "case")
                        st.kind = Ast::PsqlStmtKind::Case;
                    else if (lt.rfind("leave", 0) == 0 || lt.rfind("exit", 0) == 0) {
                        st.kind = Ast::PsqlStmtKind::Leave;
                        // optional label after LEAVE/EXIT
                        auto sp4 = t.find_first_of(" \t\n");
                        if (sp4 != std::string::npos) {
                            auto lab = t.substr(sp4 + 1);
                            trim(lab);
                            st.label = lab;
                        }
                    } else if (lt == "continue")
                        st.kind = Ast::PsqlStmtKind::Continue;
                    else if (lt.rfind("open ", 0) == 0) {
                        st.kind = Ast::PsqlStmtKind::OpenCursor;
                        st.cursor_name = t.substr(5);
                        trim(st.cursor_name);
                    } else if (lt.rfind("fetch ", 0) == 0) {
                        st.kind = Ast::PsqlStmtKind::FetchCursor;
                        auto into_pos2 = lt.find(" into ");
                        if (into_pos2 != std::string::npos) {
                            st.cursor_name = t.substr(6, into_pos2 - 6);
                            trim(st.cursor_name);
                            auto vars2 = t.substr(into_pos2 + 6);
                            size_t vp2 = 0;
                            while (vp2 < vars2.size()) {
                                auto comma2 = vars2.find(',', vp2);
                                auto tok2 = vars2.substr(vp2, comma2 == std::string::npos
                                                                  ? std::string::npos
                                                                  : comma2 - vp2);
                                trim(tok2);
                                if (!tok2.empty())
                                    st.into_vars.push_back(tok2);
                                if (comma2 == std::string::npos) {
                                    break;
                                }
                                vp2 = comma2 + 1;
                            }
                        } else {
                            st.cursor_name = t.substr(6);
                            trim(st.cursor_name);
                        }
                    } else if (lt.rfind("close ", 0) == 0) {
                        st.kind = Ast::PsqlStmtKind::CloseCursor;
                        st.cursor_name = t.substr(6);
                        trim(st.cursor_name);
                    } else if (lt.find("=") != std::string::npos)
                        st.kind = Ast::PsqlStmtKind::Assign;
                    ast.psqlBlock.body.push_back(st);
                }
                if (stmt_end == std::string::npos)
                    break;
                pos = stmt_end + 1;
            }
        }
        return ast;
    }

    Ast parse_psql_execstmt(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::PsqlExecStmt;
        ast.psqlExec.span = {0, int(sql.size())};
        ast.psqlExec.raw = sql;
        return ast;
    }

    Ast parse_psql_routine(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::PsqlRoutine;
        ast.psqlRoutine.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // CREATE [OR ALTER] {PROCEDURE|FUNCTION} name [(in params)] [RETURNS (...|TABLE (...))]
        // AS BEGIN ... END
        auto aspos = ls.find(" as ");
        auto head = (aspos == std::string::npos) ? s : s.substr(0, aspos);
        auto body = (aspos == std::string::npos) ? std::string() : s.substr(aspos + 4);
        ast.psqlRoutine.body_raw = body;
        auto ppos = ls.find(" procedure ");
        auto fpos = ls.find(" function ");
        size_t kpos = std::min(ppos == std::string::npos ? ls.size() : ppos,
                               fpos == std::string::npos ? ls.size() : fpos);
        if (kpos != std::string::npos && kpos < ls.size()) {
            bool is_proc = (kpos == ppos);
            ast.psqlRoutine.kind = is_proc ? "PROCEDURE" : "FUNCTION";
            auto after = s.substr(kpos + (is_proc ? 11 : 9));
            // name and params
            auto sp = after.find_first_not_of(" \t\n");
            if (sp != std::string::npos) {
                auto rest = after.substr(sp);
                auto lpar = rest.find('(');
                if (lpar == std::string::npos) {
                    auto end = rest.find_first_of(" \t\n");
                    ast.psqlRoutine.name = end == std::string::npos ? rest : rest.substr(0, end);
                } else {
                    ast.psqlRoutine.name = rest.substr(0, lpar);
                    auto rpar = rest.find(')', lpar);
                    if (rpar != std::string::npos)
                        ast.psqlRoutine.params_in = rest.substr(lpar + 1, rpar - lpar - 1);
                    else {
                        // Recover until RETURNS or AS
                        auto lrest = lowercase(rest);
                        size_t sent = std::string::npos;
                        for (const auto& kw : {std::string(" returns "), std::string(" as ")}) {
                            size_t p = lrest.find(kw, lpar + 1);
                            if (p != std::string::npos) {
                                sent = p;
                                break;
                            }
                        }
                        size_t endcut = (sent == std::string::npos) ? rest.size() : sent;
                        ast.psqlRoutine.params_in =
                            rest.substr(lpar + 1, endcut > lpar + 1 ? endcut - (lpar + 1) : 0);
                        push_warn(ast, int(ast.psqlRoutine.span.start),
                                  int(ast.psqlRoutine.span.end),
                                  "ROUTINE header malformed; recovered");
                    }
                }
            }
            // returns (TABLE capture allowed)
            auto rpos = lowercase(head).find("returns");
            if (rpos != std::string::npos) {
                // detect TABLE
                bool is_table = (lowercase(head).find("returns table", rpos) != std::string::npos);
                auto l2 = head.find('(', rpos);
                auto r2 = head.find(')', l2);
                if (l2 != std::string::npos && r2 != std::string::npos && r2 > l2) {
                    ast.psqlRoutine.returns = head.substr(l2 + 1, r2 - l2 - 1);
                    if (is_table &&
                        ast.psqlRoutine.attributes_raw.find("RETURNS TABLE") == std::string::npos) {
                        ast.psqlRoutine.attributes_raw += "RETURNS TABLE ";
                    }
                } else if (l2 != std::string::npos && (r2 == std::string::npos || r2 < l2)) {
                    // Recover until AS
                    auto lhead = lowercase(head);
                    size_t sent = lhead.find(" as ", l2 + 1);
                    size_t endcut = (sent == std::string::npos) ? head.size() : sent;
                    ast.psqlRoutine.returns =
                        head.substr(l2 + 1, endcut > l2 + 1 ? endcut - (l2 + 1) : 0);
                    push_warn(ast, int(ast.psqlRoutine.span.start), int(ast.psqlRoutine.span.end),
                              "ROUTINE header malformed; recovered");
                }
            }
            // parse IN/OUT/INOUT param modes and simple types
            if (!ast.psqlRoutine.params_in.empty()) {
                std::string p = ast.psqlRoutine.params_in;
                size_t i = 0;
                auto next_tok = [&](std::string& tok) {
                    while (i < p.size() && std::isspace((unsigned char)p[i]))
                        ++i;
                    if (i >= p.size())
                        return false;
                    size_t start = i;
                    while (i < p.size() && !std::isspace((unsigned char)p[i]) && p[i] != ',')
                        ++i;
                    tok = p.substr(start, i - start);
                    return true;
                };
                while (i < p.size()) {
                    std::string mode, name, type;
                    std::string tok;
                    if (!next_tok(tok))
                        break;
                    std::string ltok = lowercase(tok);
                    if (ltok == "in" || ltok == "out" || ltok == "inout") {
                        mode = tok;
                        if (!next_tok(name))
                            break;
                    } else {
                        name = tok;
                    }
                    // collect rest of type until comma
                    while (i < p.size() && std::isspace((unsigned char)p[i]))
                        ++i;
                    size_t start = i;
                    int depth = 0;
                    while (i < p.size()) {
                        char c = p[i];
                        if (c == '(')
                            depth++;
                        else if (c == ')') {
                            if (depth > 0)
                                depth--;
                        }
                        if (c == ',' && depth == 0)
                            break;
                        ++i;
                    }
                    type = p.substr(start, i - start);
                    // trim trailing spaces
                    auto not_space2 = [](int ch) { return !std::isspace(ch); };
                    type.erase(type.begin(), std::find_if(type.begin(), type.end(), not_space2));
                    type.erase(std::find_if(type.rbegin(), type.rend(), not_space2).base(),
                               type.end());
                    ast.psqlRoutine.params.push_back(
                        {mode + (mode.empty() ? "" : " ") + name, type});
                    // parse lite type details
                    auto td = parse_type_descriptor_lite(type);
                    decltype(ast.psqlRoutine.param_types)::value_type tdl{};
                    tdl.name = td.name;
                    tdl.length = td.length;
                    tdl.precision = td.precision;
                    tdl.scale = td.scale;
                    tdl.charset = td.charset;
                    tdl.collate = td.collate;
                    tdl.array_rank = td.array_rank;
                    ast.psqlRoutine.param_types.push_back(tdl);
                    if (!mode.empty()) {
                        if (!ast.psqlRoutine.param_modes_raw.empty())
                            ast.psqlRoutine.param_modes_raw += ", ";
                        ast.psqlRoutine.param_modes_raw += mode + (mode.empty() ? "" : " ") + name;
                    }
                    if (i < p.size() && p[i] == ',')
                        ++i;
                }
            }
            // routine attributes: DETERMINISTIC, SQL SECURITY ... (raw capture)
            auto attrs_pos = lowercase(head).find("deterministic");
            if (attrs_pos != std::string::npos)
                ast.psqlRoutine.attributes_raw += "DETERMINISTIC ";
            attrs_pos = lowercase(head).find("sql security");
            if (attrs_pos != std::string::npos)
                ast.psqlRoutine.attributes_raw += "SQL SECURITY ";
        }
        return ast;
    }

    Ast parse_psql_trigger(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::PsqlTrigger;
        ast.psqlTrigger.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // CREATE TRIGGER name [ACTIVE|INACTIVE] {BEFORE|AFTER} events ON table [FOR EACH
        // {ROW|STATEMENT}] AS BEGIN ... END
        auto aspos = ls.find(" as ");
        ast.psqlTrigger.body_raw =
            (aspos == std::string::npos) ? std::string() : s.substr(aspos + 4);
        // name
        auto tpos = ls.find(" trigger ");
        if (tpos != std::string::npos) {
            auto after = s.substr(tpos + 9);
            auto sp = after.find_first_not_of(" \t\n");
            if (sp != std::string::npos) {
                auto rest = after.substr(sp);
                auto end = rest.find_first_of(" \t\n");
                ast.psqlTrigger.name = end == std::string::npos ? rest : rest.substr(0, end);
            }
        }
        // active/inactive
        ast.psqlTrigger.active = (ls.find(" inactive ") == std::string::npos);
        // timing and events
        if (ls.find(" before ") != std::string::npos)
            ast.psqlTrigger.timing = "BEFORE";
        else if (ls.find(" after ") != std::string::npos)
            ast.psqlTrigger.timing = "AFTER";
        // events list split by OR (basic)
        {
            std::vector<std::string> ev;
            for (auto e : {"insert", "update", "delete"}) {
                if (ls.find(e) != std::string::npos)
                    ev.emplace_back(e);
            }
            ast.psqlTrigger.events_list = std::move(ev);
            if (ast.psqlTrigger.events_list.empty()) {
                ast.warnings.push_back("Trigger has no events specified");
            }
        }
        // UPDATE OF col-list
        {
            auto upos = ls.find(" update ");
            auto ofpos = ls.find(" of ", upos == std::string::npos ? 0 : upos + 1);
            if (upos != std::string::npos && ofpos != std::string::npos && ofpos > upos) {
                // capture until ON
                auto onpos2 = ls.find(" on ", ofpos + 1);
                std::string cols = (onpos2 == std::string::npos)
                                       ? s.substr(ofpos + 4)
                                       : s.substr(ofpos + 4, (onpos2) - (ofpos + 4));
                // split by commas
                std::vector<std::string> list;
                size_t p2 = 0;
                while (p2 < cols.size()) {
                    auto comma = cols.find(',', p2);
                    auto tok = cols.substr(p2, comma == std::string::npos ? std::string::npos
                                                                          : comma - p2);
                    trim(tok);
                    if (!tok.empty())
                        list.emplace_back(tok);
                    if (comma == std::string::npos)
                        break;
                    p2 = comma + 1;
                }
                ast.psqlTrigger.update_of_columns = std::move(list);
                if (ast.psqlTrigger.update_of_columns.empty())
                    ast.warnings.push_back("Trigger UPDATE OF has empty column list");
            }
        }
        // FOR EACH
        if (ls.find(" for each row") != std::string::npos)
            ast.psqlTrigger.for_each = "ROW";
        else if (ls.find(" for each statement") != std::string::npos)
            ast.psqlTrigger.for_each = "STATEMENT";
        // ON table
        auto onpos = ls.find(" on ");
        if (onpos != std::string::npos) {
            auto tail = s.substr(onpos + 4);
            auto end = tail.find_first_of(" \t\n");
            ast.psqlTrigger.table = end == std::string::npos ? tail : tail.substr(0, end);
        }
        // POSITION
        {
            auto ppos2 = ls.find(" position ");
            if (ppos2 != std::string::npos) {
                auto after2 = s.substr(ppos2 + 10);
                ast.psqlTrigger.position = std::atoi(after2.c_str());
            }
        }
        return ast;
    }

    Ast parse_psql_package(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::PsqlPackage;
        ast.psqlPackage.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);
        // CREATE PACKAGE name AS ... [END] | CREATE PACKAGE BODY name AS ... END
        auto aspos = ls.find(" as ");
        ast.psqlPackage.header_raw = (aspos == std::string::npos) ? s : s.substr(0, aspos);
        ast.psqlPackage.body_raw =
            (aspos == std::string::npos) ? std::string() : s.substr(aspos + 4);
        ast.psqlPackage.is_body = (ls.find(" package body ") != std::string::npos);
        // name after PACKAGE or PACKAGE BODY
        auto ppos2 = ls.find(" package ");
        if (ppos2 != std::string::npos) {
            size_t start = ppos2 + 9;
            if (ls.find(" package body ") == ppos2)
                start = ppos2 + 13;
            auto after3 = s.substr(start);
            auto sp3 = after3.find_first_not_of(" \t\n");
            if (sp3 != std::string::npos) {
                auto rest3 = after3.substr(sp3);
                auto end3 = rest3.find_first_of(" \t\n");
                ast.psqlPackage.name = end3 == std::string::npos ? rest3 : rest3.substr(0, end3);
            }
        }
        return ast;
    }

    Ast parse_psql_call(const std::string& sql)
    {
        Ast ast{};
        ast.kind = NodeKind::PsqlCall;
        ast.psqlCall.span = {0, int(sql.size())};
        std::string s = sql;
        std::string ls = lowercase(s);

        // CALL procedure_name[(arguments)]
        auto call_pos = ls.find("call ");
        if (call_pos == 0) {
            auto after_call = s.substr(5); // Skip "call "
            trim(after_call);

            // Find procedure name and arguments
            auto paren_pos = after_call.find('(');
            if (paren_pos == std::string::npos) {
                // No arguments: CALL proc_name
                ast.psqlCall.routine_name = after_call;
            } else {
                // With arguments: CALL proc_name(args)
                ast.psqlCall.routine_name = after_call.substr(0, paren_pos);
                trim(ast.psqlCall.routine_name);

                auto close_paren = after_call.find(')', paren_pos);
                if (close_paren != std::string::npos) {
                    std::string args =
                        after_call.substr(paren_pos + 1, close_paren - paren_pos - 1);
                    trim(args);
                    ast.psqlCall.args_raw = args;

                    // Simple comma-separated argument parsing
                    if (!args.empty()) {
                        size_t pos = 0;
                        while (pos < args.length()) {
                            auto comma = args.find(',', pos);
                            auto arg = (comma == std::string::npos) ? args.substr(pos)
                                                                    : args.substr(pos, comma - pos);
                            trim(arg);
                            if (!arg.empty()) {
                                ast.psqlCall.arguments.push_back(arg);
                            }
                            if (comma == std::string::npos)
                                break;
                            pos = comma + 1;
                        }
                    }
                } else {
                    // Malformed - missing closing parenthesis
                    ast.warnings.push_back("CALL statement missing closing parenthesis");
                    ast.psqlCall.args_raw = after_call.substr(paren_pos + 1);
                }
            }
        }
        return ast;
    }
} // namespace scratchbird::engine
