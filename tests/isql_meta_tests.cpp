#include "scratchbird/engine/parser.h"
#include "tools/isql/meta.h"

#include <cassert>

using scratchbird::engine::Ast;
using scratchbird::engine::parse_sql;
using namespace scratchbird::tools::isql;

int main()
{
    // Meta commands should be parsed by isql layer, not sent to engine parser.
    {
        auto mc = parse_meta_command("SET AUTODDL ON");
        assert(mc.kind == MetaCmdKind::SetAutoDdl);
        auto ast = parse_sql("SET AUTODDL ON");
        assert(ast.kind == scratchbird::engine::NodeKind::Unknown);
    }
    {
        auto mc = parse_meta_command("CONNECT '/tmp/db.fdb'");
        assert(mc.kind == MetaCmdKind::Connect);
        assert(!mc.arg.empty());
    }
    {
        auto mc = parse_meta_command("SHOW TABLES");
        assert(mc.kind == MetaCmdKind::Show);
        assert(mc.arg.find("TABLES") != std::string::npos);
    }
    {
        auto mc = parse_meta_command("SHELL ls -l");
        assert(mc.kind == MetaCmdKind::Shell);
        assert(mc.arg.find("ls") != std::string::npos);
    }
    // New toggles: smart terminator and doc comments
    {
        auto mc = parse_meta_command("SET PARSER SMART TERMINATOR ON");
        assert(mc.kind == MetaCmdKind::SetSmartTerminator);
        assert(mc.arg == "on");
    }
    {
        auto mc = parse_meta_command("SET DOC COMMENTS MARKER_ONLY");
        assert(mc.kind == MetaCmdKind::SetDocComments);
        assert(mc.arg == "marker_only");
    }
    // Smart splitting: two top-level CREATE statements without semicolons
    {
        ParserSettings ps{};
        ps.smart_terminator = true;
        std::string script = "CREATE TABLE t(id INT)\nCREATE INDEX ix ON t(id)\n";
        auto stmts = split_statements_smart(script, ps);
        assert(stmts.size() == 2);
    }
    // Leading comments capture + emit order
    {
        ParserSettings ps{};
        ps.doc_mode = DocCommentsMode::MarkerOnly;
        std::string src = "--! Customer master table\nCREATE TABLE customer(id INT)\n";
        auto comments = capture_leading_comments(src, 0, ps.doc_mode);
        assert(comments.find("Customer master") != std::string::npos);
        auto out = emit_with_doc_comments(comments, "CREATE TABLE customer(id INT)\n");
        assert(out.rfind("CREATE TABLE", 0) != 0);
    }
    return 0;
}
