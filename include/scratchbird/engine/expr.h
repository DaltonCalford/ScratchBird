#ifndef SCRATCHBIRD_ENGINE_EXPR_H
#define SCRATCHBIRD_ENGINE_EXPR_H

#include "scratchbird/engine/heap.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    // Evaluate a boolean expression over a row using simple SQL operators.
    // Supported: identifiers (column names), integer and single-quoted string literals,
    // operators: =, !=, <, <=, >, >=, IS NULL, IS NOT NULL, AND, OR, NOT, parentheses.
    bool evaluate_predicate(const std::string& expr,
                            const std::unordered_map<std::string, std::size_t>& col_index,
                            const std::vector<Value>& row);

    // Compile/evaluate variant using cached postfix tokens
    std::vector<std::string> compile_predicate(const std::string& expr);
    bool evaluate_predicate_compiled(const std::vector<std::string>& postfix,
                                     const std::unordered_map<std::string, std::size_t>& col_index,
                                     const std::vector<Value>& row);

    // Project a row according to a list of projection items.
    // Supported items: "*", column name, ordinal ("1"-based), and optional alias via "<expr> AS
    // name" For now, <expr> must be a column name or ordinal.
    std::vector<std::string>
    project_row(const std::vector<std::string>& projections,
                const std::vector<std::string>& colnames,
                const std::unordered_map<std::string, std::size_t>& col_index,
                const std::vector<Value>& row);

    // Compute header labels for projections
    std::vector<std::string> projection_headers(const std::vector<std::string>& projections,
                                                const std::vector<std::string>& colnames);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_EXPR_H
