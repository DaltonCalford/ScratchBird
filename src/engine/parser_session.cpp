// SPDX-License-Identifier: IDPL
#include "scratchbird/engine/parser_session.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace scratchbird::engine
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

    Ast parse_session_stmt(const std::string& sql)
    {
        Ast ast{};
        std::string s = sql;
        trim(s);
        std::string l = lowercase(s);

        auto starts = [&](const char* kw) { return l.rfind(kw, 0) == 0; };
        auto capture_after = [&](const char* kw) {
            std::string out;
            if (starts(kw)) {
                out = s.substr(std::string(kw).size());
                trim(out);
            }
            return out;
        };

        ast.kind = NodeKind::SessionStmt;
        ast.session.span = {0, int(sql.size())};

        if (starts("create database")) {
            ast.session.kind = SessionKind::CreateDb;
            ast.session.a = capture_after("create database");
            // rudimentary option extraction
            std::string tail = lowercase(ast.session.a);
            auto find_kw_val = [&](const char* kw) -> std::string {
                auto pos = tail.find(kw);
                if (pos == std::string::npos)
                    return {};
                std::string rest = ast.session.a.substr(pos + std::strlen(kw));
                trim(rest);
                // take first token or parenthesized string
                if (!rest.empty() && rest[0] == '\'') {
                    auto end = rest.find('\'', 1);
                    if (end != std::string::npos)
                        return rest.substr(1, end - 1);
                }
                // until next space
                size_t sp = rest.find(' ');
                return sp == std::string::npos ? rest : rest.substr(0, sp);
            };
            ast.session.dbopts.page_size = find_kw_val("page_size");
            // validate allowed sizes if numeric
            if (!ast.session.dbopts.page_size.empty()) {
                std::string ps = ast.session.dbopts.page_size;
                // strip quotes if any
                if (!ps.empty() && ps.front() == '\'' && ps.back() == '\'')
                    ps = ps.substr(1, ps.size() - 2);
                try {
                    long v = std::stol(ps);
                    if (!(v == 4096 || v == 8192 || v == 16384 || v == 32768 || v == 65536 ||
                          v == 131072)) {
                        ast.warnings.push_back("PAGE_SIZE not in allowed set (4096..131072)");
                        ast.warning_spans.push_back({0, 0});
                    }
                } catch (...) {
                }
            }
            ast.session.dbopts.default_charset = find_kw_val("default character set");
            ast.session.dbopts.dialect = find_kw_val("dialect");
            ast.session.dbopts.page_cache = find_kw_val("page cache");
            ast.session.dbopts.sweep_interval = find_kw_val("sweep interval");
            ast.session.dbopts.reserve_space = find_kw_val("reserve space");
            // crude file/shadow capture: look for FILE or SHADOW tokens and grab following quoted
            // path
            auto capture_many = [&](const char* kw, std::vector<std::string>& out) {
                std::string low = lowercase(ast.session.a);
                size_t pos = 0;
                while ((pos = low.find(kw, pos)) != std::string::npos) {
                    size_t q1 = ast.session.a.find('\'', pos);
                    if (q1 == std::string::npos)
                        break;
                    size_t q2 = ast.session.a.find('\'', q1 + 1);
                    if (q2 == std::string::npos)
                        break;
                    out.push_back(ast.session.a.substr(q1 + 1, q2 - q1 - 1));
                    pos = q2 + 1;
                }
            };
            capture_many(" file ", ast.session.dbopts.files);
            capture_many(" shadow ", ast.session.dbopts.shadows);
            return ast;
        }
        if (starts("alter database")) {
            ast.session.kind = SessionKind::AlterDb;
            ast.session.a = capture_after("alter database");
            std::string tail = lowercase(ast.session.a);
            auto get_val = [&](const char* kw) -> std::string {
                auto pos = tail.find(kw);
                if (pos == std::string::npos)
                    return {};
                std::string rest = ast.session.a.substr(pos + std::strlen(kw));
                trim(rest);
                size_t sp = rest.find(' ');
                return sp == std::string::npos ? rest : rest.substr(0, sp);
            };
            ast.session.dbopts.page_size = get_val("page_size");
            ast.session.dbopts.default_charset = get_val("default character set");
            ast.session.dbopts.dialect = get_val("dialect");
            ast.session.dbopts.page_cache = get_val("page cache");
            ast.session.dbopts.sweep_interval = get_val("sweep interval");
            ast.session.dbopts.reserve_space = get_val("reserve space");
            return ast;
        }
        if (starts("drop database")) {
            ast.session.kind = SessionKind::DropDb;
            ast.session.a = capture_after("drop database");
            return ast;
        }
        if (starts("connect")) {
            ast.session.kind = SessionKind::Connect;
            ast.session.a = capture_after("connect");
            return ast;
        }
        if (starts("disconnect")) {
            ast.session.kind = SessionKind::Disconnect;
            return ast;
        }
        if (starts("set names")) {
            ast.session.kind = SessionKind::SetNames;
            ast.session.a = capture_after("set names");
            return ast;
        }
        if (starts("set role")) {
            ast.session.kind = SessionKind::SetRole;
            ast.session.a = capture_after("set role");
            return ast;
        }
        if (starts("set sql dialect")) {
            ast.session.kind = SessionKind::SetDialect;
            ast.session.a = capture_after("set sql dialect");
            return ast;
        }
        if (starts("set transaction")) {
            ast.session.kind = SessionKind::SetTxn;
            ast.session.a = capture_after("set transaction");
            std::string t = lowercase(ast.session.a);
            // isolation
            if (t.find("read committed") != std::string::npos)
                ast.session.setopts.isolation = "READ COMMITTED";
            else if (t.find("snapshot") != std::string::npos)
                ast.session.setopts.isolation = "SNAPSHOT";
            if (t.find("snapshot table stability") != std::string::npos)
                ast.session.setopts.snapshot_table_stability = true;
            // access
            if (t.find("read only") != std::string::npos)
                ast.session.setopts.access = "READ ONLY";
            else if (t.find("read write") != std::string::npos)
                ast.session.setopts.access = "READ WRITE";
            // wait/no wait
            if (t.find("no wait") != std::string::npos)
                ast.session.setopts.wait = "NO WAIT";
            else if (t.find("wait") != std::string::npos)
                ast.session.setopts.wait = "WAIT";
            // read consistency hints (raw capture if present)
            auto rcpos = t.find("record");
            if (rcpos != std::string::npos) {
                ast.session.setopts.read_consistency = ast.session.a.substr(rcpos);
            }
            // RESERVING ... FOR ... (naive split)
            auto rpos = t.find("reserving ");
            if (rpos != std::string::npos) {
                // capture from RESERVING to end
                std::string rest = ast.session.a.substr(rpos + 10);
                // split tables by comma at top-level until FOR tokens
                size_t fpos = lowercase(rest).find(" for ");
                std::string left = (fpos == std::string::npos) ? rest : rest.substr(0, fpos);
                std::string mode =
                    (fpos == std::string::npos) ? std::string() : rest.substr(fpos + 5);
                // normalize mode to first token
                if (!mode.empty()) {
                    size_t sp = mode.find_first_of(",;\t\n");
                    if (sp != std::string::npos)
                        mode = mode.substr(0, sp);
                }
                // split left by commas
                size_t p = 0;
                while (p < left.size()) {
                    auto comma = left.find(',', p);
                    auto tok =
                        left.substr(p, comma == std::string::npos ? std::string::npos : comma - p);
                    trim(tok);
                    if (!tok.empty())
                        ast.session.setopts.table_reservations.push_back({tok, mode});
                    if (comma == std::string::npos)
                        break;
                    p = comma + 1;
                }
            }
            return ast;
        }
        if (starts("set time zone")) {
            ast.session.kind = SessionKind::SetOption;
            ast.session.a = capture_after("set time zone");
            ast.session.setopts.time_zone = ast.session.a; // raw value like 'UTC' or +02:00
            return ast;
        }
        if (starts("set bind")) {
            ast.session.kind = SessionKind::SetOption;
            ast.session.a = capture_after("set bind");
            ast.session.setopts.bind = ast.session.a;
            return ast;
        }
        if (starts("set constraints")) {
            ast.session.kind = SessionKind::SetOption;
            ast.session.a = capture_after("set constraints");
            // Store the raw tail for executor to interpret (ALL or list, DEFERRED/IMMEDIATE)
            ast.session.setopts.debug_option = std::string("SET CONSTRAINTS ") + ast.session.a;
            return ast;
        }
        if (starts("set optimize")) {
            ast.session.kind = SessionKind::SetOption;
            ast.session.a = capture_after("set optimize");
            ast.session.setopts.optimize = ast.session.a;
            return ast;
        }
        if (starts("set search path")) {
            ast.session.kind = SessionKind::SetOption;
            ast.session.a = capture_after("set search path");
            ast.session.setopts.search_path = ast.session.a;
            return ast;
        }
        if (starts("set debug option")) {
            ast.session.kind = SessionKind::SetOption;
            ast.session.a = capture_after("set debug option");
            ast.session.setopts.debug_option = ast.session.a;
            return ast;
        }
        if (starts("set decfloat round") || starts("set decfloat traps")) {
            ast.session.kind = SessionKind::SetOption;
            ast.session.a = s; // keep full text
            if (starts("set decfloat round"))
                ast.session.setopts.decfloat_round = capture_after("set decfloat round");
            if (starts("set decfloat traps"))
                ast.session.setopts.decfloat_traps = capture_after("set decfloat traps");
            return ast;
        }
        if (starts("session reset")) {
            ast.session.kind = SessionKind::SetOption;
            ast.session.a = "SESSION RESET";
            ast.session.setopts.session_reset = true;
            return ast;
        }
        if (starts("set lock timeout") || starts("set wait") || starts("set no wait") ||
            starts("set statistics") || starts("set plan") || starts("set timing")) {
            ast.session.kind = SessionKind::SetOption;
            ast.session.a = s;
            std::string ltail = lowercase(s);
            if (ltail.rfind("set lock timeout", 0) == 0) {
                std::string rest = s.substr(std::string("set lock timeout").size());
                trim(rest);
                ast.session.setopts.lock_timeout = rest;
            } else if (ltail.rfind("set wait", 0) == 0) {
                ast.session.setopts.lock_timeout = "WAIT";
            } else if (ltail.rfind("set no wait", 0) == 0) {
                ast.session.setopts.lock_timeout = "NO WAIT";
            }
            // stats toggles
            auto capture_onoff = [&](const char* kw) {
                auto pos = ltail.find(kw);
                if (pos != std::string::npos) {
                    std::string rest = s.substr(pos);
                    trim(rest);
                    ast.session.setopts.stats = rest; // e.g., "PLAN ON" or "STATISTICS OFF"
                }
            };
            capture_onoff("set statistics");
            capture_onoff("set plan");
            capture_onoff("set timing");
            return ast;
        }
        if (starts("explain ")) {
            ast.kind = NodeKind::DdlExplain;
            ast.ddlExplain.span = {0, int(sql.size())};
            ast.ddlExplain.statement_raw = s.substr(8);
            ast.ddlExplain.analyze = (lowercase(s).rfind("explain analyze ", 0) == 0);
            return ast;
        }
        if (starts("analyze ") || starts("vacuum ")) {
            ast.kind = NodeKind::DdlAnalyzeVacuum;
            ast.ddlAnalyzeVacuum.span = {0, int(sql.size())};
            std::string l2 = lowercase(s);
            ast.ddlAnalyzeVacuum.kind = (l2.rfind("analyze ", 0) == 0) ? "ANALYZE" : "VACUUM";
            std::string tail = s.substr(ast.ddlAnalyzeVacuum.kind.size() + 1);
            trim(tail);
            if (lowercase(tail).rfind("full ", 0) == 0) {
                ast.ddlAnalyzeVacuum.full = true;
                tail = tail.substr(5);
                trim(tail);
            }
            if (lowercase(tail).rfind("verbose ", 0) == 0) {
                ast.ddlAnalyzeVacuum.verbose = true;
                tail = tail.substr(8);
                trim(tail);
            }
            size_t lp = tail.find('(');
            if (lp == std::string::npos) {
                ast.ddlAnalyzeVacuum.table_name = tail;
            } else {
                ast.ddlAnalyzeVacuum.table_name = tail.substr(0, lp);
                size_t rp = tail.find(')', lp);
                if (rp != std::string::npos) {
                    ast.ddlAnalyzeVacuum.column_list = tail.substr(lp + 1, rp - lp - 1);
                }
            }
            trim(ast.ddlAnalyzeVacuum.table_name);
            return ast;
        }
        if (starts("commit")) {
            ast.session.kind = SessionKind::Commit;
            return ast;
        }
        if (starts("rollback")) {
            ast.session.kind = SessionKind::Rollback;
            return ast;
        }
        if (starts("savepoint")) {
            ast.session.kind = SessionKind::Savepoint;
            ast.session.a = capture_after("savepoint");
            return ast;
        }
        if (starts("release savepoint")) {
            ast.session.kind = SessionKind::Release;
            ast.session.a = capture_after("release savepoint");
            return ast;
        }

        // Admin surfaces (parser-only acceptance stubs)
        // START/STOP/PAUSE/RESUME TRACE | SUBSCRIPTION | BACKGROUND TASK
        if (starts("start trace") || starts("stop trace") || starts("pause trace") ||
            starts("resume trace") || starts("pause subscription") ||
            starts("resume subscription") || starts("start background task") ||
            starts("stop background task")) {
            ast.session.kind = SessionKind::SetOption;
            ast.session.a = s; // keep full command for admin routing later
            return ast;
        }
        // JOB/SCHEDULE lifecycle; RUN JOB NOW
        if (starts("create schedule") || starts("alter schedule") || starts("drop schedule") ||
            starts("create job") || starts("alter job") || starts("drop job") ||
            starts("run job")) {
            ast.session.kind = SessionKind::SetOption;
            ast.session.a = s;
            return ast;
        }
        // Maintenance knobs (parser-only): SWEEP, PAGE CACHE, RESERVE SPACE, READ CONSISTENCY
        if (starts("alter database sweep") || starts("set sweep interval") ||
            starts("set page cache") || starts("set reserve space") ||
            starts("set read consistency")) {
            ast.session.kind = SessionKind::SetOption;
            ast.session.a = s;
            return ast;
        }
        // BACKUP/RESTORE, SHOW BACKUP HISTORY
        if (starts("backup database") || starts("restore database") ||
            starts("backup tablespace") || starts("show backup history")) {
            ast.session.kind = SessionKind::SetOption;
            ast.session.a = s;
            return ast;
        }

        // Unknown: keep kind SessionStmt but with default fields
        return ast;
    }
} // namespace scratchbird::engine
