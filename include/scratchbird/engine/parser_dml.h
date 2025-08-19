#ifndef SCRATCHBIRD_ENGINE_PARSER_DML_H
#define SCRATCHBIRD_ENGINE_PARSER_DML_H

#include "scratchbird/engine/source_span.h"

#include <string>
#include <vector>

namespace scratchbird::engine
{
    struct ExecProcStmt {
        std::string name;
        std::vector<std::string> args;
    };

    struct UpsertStmt {
        std::string target;
        std::vector<std::string> columns;
        std::vector<std::string> values;
        std::vector<std::string> matching_cols;
        std::vector<std::string> warnings;
        std::vector<SourceSpan> warning_spans;
    };
    struct InsertStmt {
        std::string target;
        std::vector<std::string> columns;
        std::vector<std::string> values;                     // flat, backward-compat
        std::vector<std::vector<std::string>> values_tuples; // structured tuples
        std::string select_raw;                              // INSERT ... SELECT ...
        bool default_values{false};
        bool has_returning{false};
        std::vector<std::string> returning;
        std::vector<std::string> warnings;
        std::vector<SourceSpan> warning_spans;
    };
    struct UpdateStmt {
        std::string target;
        std::vector<std::pair<std::string, std::string>> assignments;
        std::string where_expr;
        std::string where_current_cursor; // WHERE CURRENT OF cursor
        std::string from_raw;             // UPDATE ... FROM ...
        bool has_returning{false};
        std::vector<std::string> returning;
        bool has_for_update{false};
        std::vector<std::string> warnings;
        std::vector<SourceSpan> warning_spans;
    };
    struct DeleteStmt {
        std::string target;
        std::string where_expr;
        std::string where_current_cursor; // WHERE CURRENT OF cursor
        std::string using_raw;            // DELETE ... USING ... (if supported)
        bool has_returning{false};
        std::vector<std::string> returning;
        std::vector<std::string> warnings;
        std::vector<SourceSpan> warning_spans;
    };

    struct MergeAction {
        enum class Kind { Update, Insert, Delete };
        Kind kind{Kind::Update};
        std::string guard;                                    // after AND ...
        std::vector<std::pair<std::string, std::string>> set; // UPDATE SET list
        std::vector<std::string> insert_cols;                 // INSERT columns
        std::vector<std::string> insert_values;               // INSERT values
        bool do_nothing{false};                               // THEN DO NOTHING
    };

    struct MergeStmt {
        std::string target;
        std::string using_source;         // raw
        std::string on_match;             // raw condition
        std::vector<MergeAction> actions; // structured actions (WHEN ... THEN ...)
        std::vector<std::string> warnings;
        std::vector<SourceSpan> warning_spans;
        // Optional: structure actions later
    };

    InsertStmt parse_insert_minimal(const std::string& sql);
    UpdateStmt parse_update_minimal(const std::string& sql);
    DeleteStmt parse_delete_minimal(const std::string& sql);
    MergeStmt parse_merge_minimal(const std::string& sql);
    UpsertStmt parse_upsert_minimal(const std::string& sql);
    ExecProcStmt parse_execproc_minimal(const std::string& sql);
} // namespace scratchbird::engine

#endif
