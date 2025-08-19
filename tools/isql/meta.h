// SPDX-License-Identifier: IDPL
#ifndef SCRATCHBIRD_TOOLS_ISQL_META_H
#define SCRATCHBIRD_TOOLS_ISQL_META_H

#include <string>
#include <vector>

namespace scratchbird::tools::isql
{
    enum class MetaCmdKind {
        Unknown,
        SetAutoDdl,
        Input,
        Output,
        Show,
        Shell,
        Edit,
        Echo,
        Term,
        Plan,
        Timing,
        Connect,
        SetSmartTerminator,
        SetDocComments,
        ShowHeader
    };

    struct MetaCommand {
        MetaCmdKind kind{MetaCmdKind::Unknown};
        std::string arg;
    };

    enum class DocCommentsMode { Off, On, MarkerOnly };

    struct ParserSettings {
        bool smart_terminator{false};
        DocCommentsMode doc_mode{DocCommentsMode::Off};
    };

    // Parse a single isql meta-command line (starting with SET, INPUT, SHOW, etc.)
    MetaCommand parse_meta_command(const std::string& line);

    // Split input into statements using semicolons and optional smart-terminator heuristics.
    std::vector<std::string> split_statements_smart(const std::string& input,
                                                    const ParserSettings& s);

    // Capture contiguous leading comments before the first non-comment token of a statement.
    std::string capture_leading_comments(const std::string& input, size_t stmt_start_pos,
                                         DocCommentsMode mode);

    // Emit doc comments (if any) followed by DDL text.
    std::string emit_with_doc_comments(const std::string& comments, const std::string& ddl);
} // namespace scratchbird::tools::isql

#endif // SCRATCHBIRD_TOOLS_ISQL_META_H
