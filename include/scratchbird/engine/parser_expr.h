#ifndef SCRATCHBIRD_ENGINE_PARSER_EXPR_H
#define SCRATCHBIRD_ENGINE_PARSER_EXPR_H

#include "scratchbird/engine/source_span.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace scratchbird::engine
{
    struct Expr;

    struct TypeDescriptor {
        std::string name;  // base type name
        int length{-1};    // for CHAR/VARCHAR/BINARY
        int precision{-1}; // for DECIMAL/NUMERIC
        int scale{-1};
        std::string charset; // optional
        std::string collate; // optional
        int array_rank{0};   // 0=scalar; >0 arrays with [] count
        // TYPE OF acceptance
        std::string type_of_target; // e.g., domain name or table.column
        bool type_of_is_column{false};
    };

    enum class ExprKind {
        Identifier,
        Literal,
        Unary,
        Binary,
        Call,
        Paren,
        Case,
        Exists,
        Subquery,
        Cast,
        Collate
    };

    struct Expr {
        ExprKind kind{};
        SourceSpan span{};
        std::string text;                        // normalized textual form for now
        std::vector<Expr> children;              // operands/args
        std::optional<TypeDescriptor> cast_type; // when kind == Cast
        std::vector<std::string> warnings;       // parse warnings for this expression
    };

    // Returns a normalized string for the parsed expression, or empty on failure.
    std::string parse_expression_to_string(const std::string& sql);

    // Returns a minimal AST for the parsed expression (with spans and normalized text).
    Expr parse_expression_to_ast(const std::string& sql);

    // Parse a SQL type descriptor (used by CAST/::) into a structured form.
    // Accepts inputs like:
    //   VARCHAR(20)
    //   DECIMAL(10,2)
    //   CHAR(10) CHARACTER SET UTF8
    //   INTEGER[][]
    TypeDescriptor parse_type_descriptor(const std::string& type_sql);
} // namespace scratchbird::engine

#endif
