// SPDX-License-Identifier: IDPL
#include "meta.h"

#include <algorithm>
#include <cctype>

namespace scratchbird::tools::isql
{
    namespace
    {
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

    MetaCommand parse_meta_command(const std::string& line)
    {
        MetaCommand mc{};
        std::string s = line;
        trim(s);
        std::string l = lowercase(s);
        auto starts = [&](const char* kw) { return l.rfind(kw, 0) == 0; };

        if (starts("set autoddl")) {
            mc.kind = MetaCmdKind::SetAutoDdl;
            return mc;
        }
        if (starts("set parser smart terminator") || starts("set smart terminator")) {
            mc.kind = MetaCmdKind::SetSmartTerminator;
            auto pos = s.find_last_of(' ');
            if (pos != std::string::npos) {
                mc.arg = lowercase(s.substr(pos + 1));
                trim(mc.arg);
            }
            return mc;
        }
        if (starts("set doc comments")) {
            mc.kind = MetaCmdKind::SetDocComments;
            auto pos = s.find_last_of(' ');
            if (pos != std::string::npos) {
                mc.arg = lowercase(s.substr(pos + 1));
                trim(mc.arg);
                if (mc.arg == "marker-only")
                    mc.arg = "marker_only";
            }
            return mc;
        }
        if (starts("input")) {
            mc.kind = MetaCmdKind::Input;
            mc.arg = s.substr(5);
            trim(mc.arg);
            return mc;
        }
        if (starts("output")) {
            mc.kind = MetaCmdKind::Output;
            mc.arg = s.substr(6);
            trim(mc.arg);
            return mc;
        }
        if (starts("show header")) {
            mc.kind = MetaCmdKind::ShowHeader;
            return mc;
        }
        if (starts("show")) {
            mc.kind = MetaCmdKind::Show;
            mc.arg = s.substr(4);
            trim(mc.arg);
            return mc;
        }
        if (starts("shell")) {
            mc.kind = MetaCmdKind::Shell;
            mc.arg = s.substr(5);
            trim(mc.arg);
            return mc;
        }
        if (starts("edit")) {
            mc.kind = MetaCmdKind::Edit;
            return mc;
        }
        if (starts("echo")) {
            mc.kind = MetaCmdKind::Echo;
            mc.arg = s.substr(4);
            trim(mc.arg);
            return mc;
        }
        if (starts("term")) {
            mc.kind = MetaCmdKind::Term;
            mc.arg = s.substr(4);
            trim(mc.arg);
            return mc;
        }
        if (starts("plan")) {
            mc.kind = MetaCmdKind::Plan;
            return mc;
        }
        if (starts("timing")) {
            mc.kind = MetaCmdKind::Timing;
            return mc;
        }
        if (starts("connect")) {
            mc.kind = MetaCmdKind::Connect;
            mc.arg = s.substr(7);
            trim(mc.arg);
            return mc;
        }
        return mc;
    }

    std::vector<std::string> split_statements_smart(const std::string& input,
                                                    const ParserSettings& s)
    {
        std::vector<std::string> out;
        std::string cur;
        int paren = 0;
        bool in_squote = false;
        bool in_line_comment = false;
        bool in_block_comment = false;
        auto flush = [&]() {
            std::string tmp = cur;
            trim(tmp);
            if (!tmp.empty())
                out.push_back(tmp);
            cur.clear();
        };
        auto is_top_level_starter = [&](const std::string& token) {
            static const char* starters[] = {
                "create",  "alter",    "drop",      "recreate", "grant",   "revoke",     "set",
                "commit",  "rollback", "savepoint", "release",  "connect", "disconnect", "execute",
                "comment", "merge",    "insert",    "update",   "delete",  "select",     "with"};
            std::string t = lowercase(token);
            for (auto* k : starters)
                if (t == k)
                    return true;
            return false;
        };
        std::string token_acc;
        for (size_t i = 0; i < input.size(); ++i) {
            char c = input[i];
            char n = (i + 1 < input.size() ? input[i + 1] : '\0');
            cur.push_back(c);
            if (in_line_comment) {
                if (c == '\n')
                    in_line_comment = false;
                continue;
            }
            if (in_block_comment) {
                if (c == '*' && n == '/') {
                    in_block_comment = false;
                    cur.push_back(n);
                    ++i;
                }
                continue;
            }
            if (!in_squote) {
                if (c == '-' && n == '-') {
                    in_line_comment = true;
                    ++i;
                    cur.push_back(n);
                    continue;
                }
                if (c == '/' && n == '*') {
                    in_block_comment = true;
                    ++i;
                    cur.push_back(n);
                    continue;
                }
            }
            if (c == '\'') {
                in_squote = !in_squote;
                token_acc.clear();
                continue;
            }
            if (in_squote)
                continue;
            if (c == '(') {
                paren++;
                token_acc.clear();
                continue;
            }
            if (c == ')') {
                if (paren > 0)
                    paren--;
                token_acc.clear();
                continue;
            }
            if (c == ';') {
                flush();
                token_acc.clear();
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!token_acc.empty())
                    token_acc.push_back(' ');
                continue;
            }
            if (std::isalpha(static_cast<unsigned char>(c))) {
                token_acc.push_back(static_cast<char>(std::tolower(c)));
                if (s.smart_terminator && paren == 0 && !cur.empty()) {
                    std::string t = token_acc;
                    if (!t.empty() && t.back() == ' ')
                        t.pop_back();
                    if (is_top_level_starter(t)) {
                        size_t pos = cur.find_last_of('\n');
                        if (pos == std::string::npos)
                            pos = 0;
                        else
                            pos += 1;
                        std::string head = cur.substr(0, pos);
                        trim(head);
                        if (!head.empty())
                            out.push_back(head);
                        cur.erase(0, pos);
                        token_acc.clear();
                    }
                }
                continue;
            } else {
                token_acc.clear();
            }
        }
        trim(cur);
        if (!cur.empty())
            out.push_back(cur);
        return out;
    }

    std::string capture_leading_comments(const std::string& input, size_t stmt_start_pos,
                                         DocCommentsMode mode)
    {
        (void)stmt_start_pos;
        std::string out;
        size_t i = 0;
        while (i < input.size()) {
            size_t line_start = i;
            while (i < input.size() && input[i] != '\n')
                i++;
            std::string line = input.substr(line_start, i - line_start);
            if (i < input.size() && input[i] == '\n')
                i++;
            std::string l = lowercase(line);
            if (l.rfind("--", 0) == 0 || l.rfind("/*", 0) == 0) {
                if (mode == DocCommentsMode::MarkerOnly) {
                    if (!(l.rfind("--!", 0) == 0 || l.rfind("/**!", 0) == 0 ||
                          l.rfind("/*<!>", 0) == 0)) {
                        continue;
                    }
                }
                out += line;
                out += "\n";
                continue;
            }
            bool blank = true;
            for (char c : line)
                if (!std::isspace(static_cast<unsigned char>(c))) {
                    blank = false;
                    break;
                }
            if (blank) {
                out += line;
                out += "\n";
                continue;
            }
            break;
        }
        return out;
    }

    std::string emit_with_doc_comments(const std::string& comments, const std::string& ddl)
    {
        if (!comments.empty())
            return comments + ddl;
        return ddl;
    }
} // namespace scratchbird::tools::isql
