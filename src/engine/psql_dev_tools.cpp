#include "scratchbird/engine/psql_dev_tools.h"

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/parser.h"
#include "scratchbird/engine/system_oids.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>
#include <sstream>

namespace scratchbird::engine
{
    // PsqlDependencyAnalyzer implementation

    PsqlDependencyAnalyzer::AnalysisResult
    PsqlDependencyAnalyzer::analyze_dependencies(const std::string& procedure_name,
                                                 const std::string& psql_code)
    {
        AnalysisResult result;
        result.procedure_name = procedure_name;

        // Extract procedure/function calls
        extract_calls(psql_code, result.dependencies);

        // Extract table references
        extract_table_references(psql_code, result.dependencies);

        // Remove duplicates
        std::sort(result.dependencies.begin(), result.dependencies.end(),
                  [](const Dependency& a, const Dependency& b) { return a.name < b.name; });
        result.dependencies.erase(std::unique(result.dependencies.begin(),
                                              result.dependencies.end(),
                                              [](const Dependency& a, const Dependency& b) {
                                                  return a.name == b.name && a.type == b.type;
                                              }),
                                  result.dependencies.end());

        return result;
    }

    void PsqlDependencyAnalyzer::extract_calls(const std::string& code,
                                               std::vector<Dependency>& deps)
    {
        // Find CALL statements
        std::regex call_regex(R"(\bCALL\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\()",
                              std::regex_constants::icase);
        std::smatch match;
        std::string::const_iterator start = code.cbegin();

        while (std::regex_search(start, code.cend(), match, call_regex)) {
            Dependency dep;
            dep.name = match[1].str();
            dep.type = "procedure";
            deps.push_back(dep);
            start = match.suffix().first;
        }

        // Find function calls (basic pattern matching)
        std::regex func_regex(R"([a-zA-Z_][a-zA-Z0-9_]*\s*\([^)]*\))", std::regex_constants::icase);
        start = code.cbegin();
        while (std::regex_search(start, code.cend(), match, func_regex)) {
            std::string func_call = match[0].str();
            auto paren_pos = func_call.find('(');
            if (paren_pos != std::string::npos) {
                std::string func_name = func_call.substr(0, paren_pos);
                // Remove whitespace
                func_name.erase(std::remove_if(func_name.begin(), func_name.end(),
                                               [](char c) { return std::isspace(c); }),
                                func_name.end());

                // Skip built-in functions and keywords
                if (func_name != "UPPER" && func_name != "LOWER" && func_name != "LENGTH" &&
                    func_name != "COUNT" && func_name != "SUM" && func_name != "MAX" &&
                    func_name != "MIN" && func_name != "AVG") {
                    Dependency dep;
                    dep.name = func_name;
                    dep.type = "function";
                    deps.push_back(dep);
                }
            }
            start = match.suffix().first;
        }
    }

    void PsqlDependencyAnalyzer::extract_table_references(const std::string& code,
                                                          std::vector<Dependency>& deps)
    {
        // Find FROM/JOIN clauses
        std::regex from_regex(R"(\b(?:FROM|JOIN)\s+([a-zA-Z_][a-zA-Z0-9_]*)\b)",
                              std::regex_constants::icase);
        std::smatch match;
        std::string::const_iterator start = code.cbegin();

        while (std::regex_search(start, code.cend(), match, from_regex)) {
            Dependency dep;
            dep.name = match[1].str();
            dep.type = "table";
            deps.push_back(dep);
            start = match.suffix().first;
        }

        // Find INSERT INTO/UPDATE/DELETE FROM
        std::regex dml_regex(
            R"(\b(?:INSERT\s+INTO|UPDATE|DELETE\s+FROM)\s+([a-zA-Z_][a-zA-Z0-9_]*)\b)",
            std::regex_constants::icase);
        start = code.cbegin();
        while (std::regex_search(start, code.cend(), match, dml_regex)) {
            Dependency dep;
            dep.name = match[1].str();
            dep.type = "table";
            deps.push_back(dep);
            start = match.suffix().first;
        }
    }

    std::map<std::string, PsqlDependencyAnalyzer::AnalysisResult>
    PsqlDependencyAnalyzer::analyze_dependency_graph(
        const std::map<std::string, std::string>& procedures)
    {
        std::map<std::string, AnalysisResult> results;
        for (const auto& [name, code] : procedures) {
            results[name] = analyze_dependencies(name, code);
        }
        return results;
    }

    std::vector<std::vector<std::string>> PsqlDependencyAnalyzer::find_circular_dependencies(
        const std::map<std::string, AnalysisResult>& analysis)
    {
        std::vector<std::vector<std::string>> cycles;
        // Basic cycle detection (simplified for demonstration)
        // In a full implementation, this would use graph algorithms
        return cycles;
    }

    // PsqlCodeFormatter implementation

    std::string PsqlCodeFormatter::format_code(const std::string& psql_code)
    {
        return format_code(psql_code, FormatOptions{});
    }

    std::string PsqlCodeFormatter::format_statement(const std::string& statement)
    {
        return format_statement(statement, FormatOptions{});
    }

    bool PsqlCodeFormatter::is_well_formatted(const std::string& psql_code)
    {
        return is_well_formatted(psql_code, FormatOptions{});
    }

    std::string PsqlCodeFormatter::format_code(const std::string& psql_code,
                                               const FormatOptions& options)
    {
        std::string formatted = psql_code;

        if (options.normalize_whitespace) {
            formatted = normalize_whitespace(formatted);
        }

        if (options.uppercase_keywords) {
            formatted = format_keywords(formatted, true);
        }

        formatted = apply_indentation(formatted, options.indent_size);

        if (options.break_long_lines) {
            formatted = break_long_lines(formatted, options.max_line_length);
        }

        return formatted;
    }

    std::string PsqlCodeFormatter::format_keywords(const std::string& code, bool uppercase)
    {
        std::string result = code;
        std::vector<std::string> keywords = {
            "BEGIN",  "END",    "DECLARE", "AS",     "IF",        "THEN",     "ELSE",
            "WHILE",  "DO",     "FOR",     "CALL",   "PROCEDURE", "FUNCTION", "RETURNS",
            "CREATE", "ALTER",  "DROP",    "SELECT", "FROM",      "WHERE",    "INSERT",
            "UPDATE", "DELETE", "INTO",    "VALUES", "AND",       "OR",       "NOT",
            "NULL",   "TRUE",   "FALSE",   "WHEN",   "EXCEPTION", "RAISE"};

        for (const auto& keyword : keywords) {
            std::string pattern = "\\b" + keyword + "\\b";
            std::string replacement =
                uppercase ? keyword : std::string(1, std::tolower(keyword[0])) + keyword.substr(1);
            result = std::regex_replace(result, std::regex(pattern, std::regex_constants::icase),
                                        replacement);
        }

        return result;
    }

    std::string PsqlCodeFormatter::normalize_whitespace(const std::string& code)
    {
        std::string result = code;

        // Replace multiple spaces with single spaces
        result = std::regex_replace(result, std::regex(R"(\s+)"), " ");

        // Remove trailing whitespace
        result = std::regex_replace(result, std::regex(R"(\s+$)"), "");

        return result;
    }

    std::string PsqlCodeFormatter::apply_indentation(const std::string& code, int indent_size)
    {
        std::istringstream iss(code);
        std::string line;
        std::ostringstream result;
        int indent_level = 0;
        std::string indent(indent_size, ' ');

        while (std::getline(iss, line)) {
            // Trim leading whitespace
            line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](unsigned char ch) {
                           return !std::isspace(ch);
                       }));

            // Decrease indent for END statements
            if (line.rfind("END", 0) == 0) {
                indent_level = std::max(0, indent_level - 1);
            }

            // Apply current indentation
            for (int i = 0; i < indent_level; ++i) {
                result << indent;
            }
            result << line << "\n";

            // Increase indent for BEGIN statements
            if (line.find("BEGIN") != std::string::npos) {
                indent_level++;
            }
        }

        return result.str();
    }

    std::string PsqlCodeFormatter::break_long_lines(const std::string& code, int max_length)
    {
        std::istringstream iss(code);
        std::string line;
        std::ostringstream result;

        while (std::getline(iss, line)) {
            if (static_cast<int>(line.length()) > max_length) {
                // Simple line breaking at commas or keywords
                size_t break_pos = line.rfind(',', max_length);
                if (break_pos == std::string::npos) {
                    break_pos = line.rfind(' ', max_length);
                }

                if (break_pos != std::string::npos) {
                    result << line.substr(0, break_pos + 1) << "\n";
                    std::string remaining = line.substr(break_pos + 1);
                    // Add indentation to continuation line
                    result << "    " << remaining << "\n";
                    continue;
                }
            }
            result << line << "\n";
        }

        return result.str();
    }

    std::string PsqlCodeFormatter::format_statement(const std::string& statement,
                                                    const FormatOptions& options)
    {
        // Format single statement using same logic as format_code
        return format_code(statement, options);
    }

    bool PsqlCodeFormatter::is_well_formatted(const std::string& psql_code,
                                              const FormatOptions& options)
    {
        std::string formatted = format_code(psql_code, options);
        return formatted == psql_code;
    }

    // PsqlPerformanceProfiler implementation

    void PsqlPerformanceProfiler::start_profiling(const std::string& procedure_name)
    {
        start_times_[procedure_name] = std::chrono::steady_clock::now();
        if (metrics_.find(procedure_name) == metrics_.end()) {
            metrics_[procedure_name].procedure_name = procedure_name;
        }
    }

    void PsqlPerformanceProfiler::stop_profiling(const std::string& procedure_name)
    {
        auto end_time = std::chrono::steady_clock::now();
        auto it = start_times_.find(procedure_name);
        if (it != start_times_.end()) {
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(end_time - it->second);

            auto& metrics = metrics_[procedure_name];
            metrics.execution_count++;
            metrics.total_time += duration;
            metrics.avg_time = metrics.total_time / metrics.execution_count;
            metrics.min_time = std::min(metrics.min_time, duration);
            metrics.max_time = std::max(metrics.max_time, duration);

            start_times_.erase(it);
        }
    }

    void PsqlPerformanceProfiler::record_statement(const std::string& procedure_name,
                                                   const std::string& statement_type)
    {
        metrics_[procedure_name].statement_counts[statement_type]++;
    }

    PsqlPerformanceProfiler::ProfileMetrics
    PsqlPerformanceProfiler::get_metrics(const std::string& procedure_name) const
    {
        auto it = metrics_.find(procedure_name);
        return (it != metrics_.end()) ? it->second : ProfileMetrics{};
    }

    std::map<std::string, PsqlPerformanceProfiler::ProfileMetrics>
    PsqlPerformanceProfiler::get_all_metrics() const
    {
        return metrics_;
    }

    void PsqlPerformanceProfiler::clear_metrics()
    {
        metrics_.clear();
        start_times_.clear();
    }

    std::string PsqlPerformanceProfiler::generate_report() const
    {
        std::ostringstream report;
        report << "=== PSQL Performance Report ===\n\n";

        for (const auto& [name, metrics] : metrics_) {
            report << "Procedure: " << name << "\n";
            report << "  Executions: " << metrics.execution_count << "\n";
            report << "  Total Time: " << metrics.total_time.count() << "μs\n";
            report << "  Average Time: " << metrics.avg_time.count() << "μs\n";
            report << "  Min Time: " << metrics.min_time.count() << "μs\n";
            report << "  Max Time: " << metrics.max_time.count() << "μs\n";

            if (!metrics.statement_counts.empty()) {
                report << "  Statement Counts:\n";
                for (const auto& [stmt_type, count] : metrics.statement_counts) {
                    report << "    " << stmt_type << ": " << count << "\n";
                }
            }
            report << "\n";
        }

        return report.str();
    }

    // PsqlSyntaxValidator implementation

    PsqlSyntaxValidator::ValidationResult
    PsqlSyntaxValidator::validate_syntax(const std::string& psql_code)
    {
        ValidationResult result;

        try {
            // Try parsing the code
            auto ast = parse_sql(psql_code);

            if (ast.kind == NodeKind::Unknown) {
                ValidationIssue issue;
                issue.type = "error";
                issue.message = "Unable to parse PSQL code";
                issue.suggestion = "Check syntax for errors";
                result.issues.push_back(issue);
                result.is_valid = false;
            }
        } catch (const std::exception& e) {
            ValidationIssue issue;
            issue.type = "error";
            issue.message = std::string("Parse error: ") + e.what();
            result.issues.push_back(issue);
            result.is_valid = false;
        }

        // Check for common issues
        auto common_issues = check_common_issues(psql_code);
        result.issues.insert(result.issues.end(), common_issues.begin(), common_issues.end());

        result.formatted_report = generate_report(result);
        return result;
    }

    std::vector<PsqlSyntaxValidator::ValidationIssue>
    PsqlSyntaxValidator::check_common_issues(const std::string& psql_code)
    {
        std::vector<ValidationIssue> issues;

        // Check for missing semicolons
        if (psql_code.find_last_of(';') == std::string::npos) {
            ValidationIssue issue;
            issue.type = "warning";
            issue.message = "No semicolon found - statements should end with semicolons";
            issue.suggestion = "Add semicolons after statements";
            issues.push_back(issue);
        }

        // Check for unmatched BEGIN/END
        size_t begin_count = 0, end_count = 0;
        std::regex begin_regex(R"(\bBEGIN\b)", std::regex_constants::icase);
        std::regex end_regex(R"(\bEND\b)", std::regex_constants::icase);

        begin_count =
            std::distance(std::sregex_iterator(psql_code.begin(), psql_code.end(), begin_regex),
                          std::sregex_iterator());
        end_count =
            std::distance(std::sregex_iterator(psql_code.begin(), psql_code.end(), end_regex),
                          std::sregex_iterator());

        if (begin_count != end_count) {
            ValidationIssue issue;
            issue.type = "error";
            issue.message = "Unmatched BEGIN/END blocks";
            issue.suggestion = "Ensure every BEGIN has a matching END";
            issues.push_back(issue);
        }

        return issues;
    }

    PsqlSyntaxValidator::ValidationResult
    PsqlSyntaxValidator::validate_procedure(const std::string& procedure_code)
    {
        ValidationResult result = validate_syntax(procedure_code);

        // Additional procedure-specific validation
        validate_variable_declarations(procedure_code, result.issues);
        validate_control_flow(procedure_code, result.issues);
        validate_exception_handling(procedure_code, result.issues);

        result.is_valid =
            result.issues.empty() ||
            std::none_of(result.issues.begin(), result.issues.end(),
                         [](const ValidationIssue& issue) { return issue.type == "error"; });
        result.formatted_report = generate_report(result);
        return result;
    }

    PsqlSyntaxValidator::ValidationResult
    PsqlSyntaxValidator::validate_function(const std::string& function_code)
    {
        ValidationResult result = validate_syntax(function_code);

        // Check for RETURNS clause in functions
        if (function_code.find("RETURNS") == std::string::npos &&
            function_code.find("FUNCTION") != std::string::npos) {
            ValidationIssue issue;
            issue.type = "warning";
            issue.message = "Function should have RETURNS clause";
            issue.suggestion = "Add RETURNS clause to specify return type";
            result.issues.push_back(issue);
        }

        result.is_valid =
            result.issues.empty() ||
            std::none_of(result.issues.begin(), result.issues.end(),
                         [](const ValidationIssue& issue) { return issue.type == "error"; });
        result.formatted_report = generate_report(result);
        return result;
    }

    PsqlSyntaxValidator::ValidationResult
    PsqlSyntaxValidator::validate_package(const std::string& package_code)
    {
        ValidationResult result = validate_syntax(package_code);

        // Check for package structure
        if (package_code.find("PACKAGE") == std::string::npos) {
            ValidationIssue issue;
            issue.type = "error";
            issue.message = "Missing PACKAGE declaration";
            issue.suggestion = "Add CREATE PACKAGE declaration";
            result.issues.push_back(issue);
        }

        result.is_valid =
            result.issues.empty() ||
            std::none_of(result.issues.begin(), result.issues.end(),
                         [](const ValidationIssue& issue) { return issue.type == "error"; });
        result.formatted_report = generate_report(result);
        return result;
    }

    void PsqlSyntaxValidator::validate_variable_declarations(const std::string& code,
                                                             std::vector<ValidationIssue>& issues)
    {
        // Check for variable declarations without types
        std::regex var_decl_regex(R"(\bDECLARE\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*;)",
                                  std::regex_constants::icase);
        std::smatch match;
        std::string::const_iterator start = code.cbegin();

        while (std::regex_search(start, code.cend(), match, var_decl_regex)) {
            ValidationIssue issue;
            issue.type = "error";
            issue.message = "Variable declaration missing type: " + match[1].str();
            issue.suggestion = "Specify variable type (e.g., INTEGER, VARCHAR)";
            issues.push_back(issue);
            start = match.suffix().first;
        }
    }

    void PsqlSyntaxValidator::validate_control_flow(const std::string& code,
                                                    std::vector<ValidationIssue>& issues)
    {
        // Check for IF without THEN
        std::regex if_regex(R"(\bIF\s+[^;]+(?!\bTHEN\b)[^;]*;)", std::regex_constants::icase);
        if (std::regex_search(code, if_regex)) {
            ValidationIssue issue;
            issue.type = "error";
            issue.message = "IF statement missing THEN clause";
            issue.suggestion = "Add THEN after IF condition";
            issues.push_back(issue);
        }

        // Check for WHILE without DO
        std::regex while_regex(R"(\bWHILE\s+[^;]+(?!\bDO\b)[^;]*;)", std::regex_constants::icase);
        if (std::regex_search(code, while_regex)) {
            ValidationIssue issue;
            issue.type = "error";
            issue.message = "WHILE statement missing DO clause";
            issue.suggestion = "Add DO after WHILE condition";
            issues.push_back(issue);
        }
    }

    void PsqlSyntaxValidator::validate_exception_handling(const std::string& code,
                                                          std::vector<ValidationIssue>& issues)
    {
        // Check for WHEN without proper exception handler
        std::regex when_regex(R"(\bWHEN\s+([a-zA-Z_][a-zA-Z0-9_]*)\s+DO\s*$)",
                              std::regex_constants::icase | std::regex_constants::multiline);
        if (std::regex_search(code, when_regex)) {
            ValidationIssue issue;
            issue.type = "warning";
            issue.message = "Exception handler found - ensure proper exception handling";
            issue.suggestion = "Verify exception handling logic is complete";
            issues.push_back(issue);
        }
    }

    std::string PsqlSyntaxValidator::generate_report(const ValidationResult& result)
    {
        std::ostringstream report;
        report << "=== PSQL Syntax Validation Report ===\n\n";

        if (result.is_valid) {
            report << "✓ Syntax is valid\n";
        } else {
            report << "✗ Syntax validation failed\n";
        }

        if (!result.issues.empty()) {
            report << "\nIssues found:\n";
            for (const auto& issue : result.issues) {
                report << "  [" << issue.type << "]";
                if (issue.line_number > 0) {
                    report << " Line " << issue.line_number;
                }
                report << ": " << issue.message;
                if (!issue.suggestion.empty()) {
                    report << " (" << issue.suggestion << ")";
                }
                report << "\n";
            }
        }

        return report.str();
    }

    // PsqlDevEnvironment implementation

    PsqlDevEnvironment::PsqlDevEnvironment(const std::string& db_path) : db_path_(db_path) {}

    PsqlDevEnvironment::CodeAnalysis PsqlDevEnvironment::analyze_code(const std::string& name,
                                                                      const std::string& code)
    {
        CodeAnalysis analysis;

        analysis.dependencies = dependency_analyzer_.analyze_dependencies(name, code);
        analysis.validation = syntax_validator_.validate_syntax(code);
        analysis.performance = profiler_.get_metrics(name);
        analysis.formatted_code = code_formatter_.format_code(code);

        return analysis;
    }

    std::vector<std::string>
    PsqlDevEnvironment::get_code_completion(const std::string& partial_code, int cursor_position)
    {
        std::vector<std::string> suggestions;

        // Basic keyword completion
        std::vector<std::string> keywords = {
            "BEGIN",  "END",    "DECLARE",   "AS",        "IF",       "THEN",    "ELSE",   "WHILE",
            "DO",     "FOR",    "CALL",      "PROCEDURE", "FUNCTION", "RETURNS", "CREATE", "ALTER",
            "DROP",   "SELECT", "FROM",      "WHERE",     "INSERT",   "UPDATE",  "DELETE", "INTO",
            "VALUES", "WHEN",   "EXCEPTION", "RAISE",     "CONTINUE", "LEAVE"};

        // Extract word at cursor
        std::string current_word;
        int start = cursor_position;
        while (start > 0 && std::isalnum(partial_code[start - 1])) {
            start--;
        }
        if (start < cursor_position) {
            current_word = partial_code.substr(start, cursor_position - start);
        }

        // Find matching keywords
        for (const auto& keyword : keywords) {
            if (keyword.substr(0, current_word.length()) == current_word) {
                suggestions.push_back(keyword);
            }
        }

        return suggestions;
    }

    std::string PsqlDevEnvironment::find_definition(const std::string& name)
    {
        try {
            // Search in catalog using same pattern as PSQL executor
            if (db_path_.empty()) {
                return "Error: Database path not configured for definition search";
            }

            // Use CatalogManager to look up the routine
            std::string db_path = db_path_;
            CatalogManager cm(db_path);
            auto schema_oid = oid_public_schema(); // Default to public schema

            auto routine_info = cm.get_routine_by_name(schema_oid, name);
            if (!routine_info) {
                return "Definition not found for: " + name;
            }

            // Format the definition information
            std::ostringstream definition;
            definition << "=== Definition for " << name << " ===\n\n";
            definition << "Type: " << routine_info->kind << "\n";
            definition << "Language: " << routine_info->language << "\n";
            definition << "Security: " << routine_info->security << "\n";
            definition << "Volatility: " << routine_info->volatility << "\n";

            if (!routine_info->source_code.empty()) {
                definition << "\nSource Code:\n";
                definition << "----------------------------------------\n";
                definition << routine_info->source_code << "\n";
                definition << "----------------------------------------\n";
            } else {
                definition << "\nNo source code available\n";
            }

            return definition.str();
        } catch (const std::exception& e) {
            return "Error searching definition: " + std::string(e.what());
        }
    }

    std::vector<std::string> PsqlDevEnvironment::find_references(const std::string& name)
    {
        std::vector<std::string> references;

        try {
            if (db_path_.empty()) {
                references.push_back("Error: Database path not configured for reference search");
                return references;
            }

            // Use CatalogManager to get all routines
            CatalogManager cm(db_path_);
            auto schema_oid = oid_public_schema(); // Default to public schema

            auto all_routines = cm.list_routines(schema_oid);

            // Search through all routine source code for references to 'name'
            for (const auto& routine : all_routines) {
                if (routine.name == name) {
                    continue; // Skip self-references
                }

                // Search for CALL statements and function calls
                std::string source = routine.source_code;
                bool has_reference = false;

                // Search for CALL statements
                std::regex call_regex("\\bCALL\\s+" + name + "\\s*\\(",
                                      std::regex_constants::icase);
                if (std::regex_search(source, call_regex)) {
                    has_reference = true;
                }

                // Search for function calls (name followed by parentheses)
                if (!has_reference) {
                    std::regex func_regex("\\b" + name + "\\s*\\(", std::regex_constants::icase);
                    if (std::regex_search(source, func_regex)) {
                        has_reference = true;
                    }
                }

                if (has_reference) {
                    references.push_back("Referenced in " + routine.kind + ": " + routine.name);
                }
            }

            if (references.empty()) {
                references.push_back("No references found for: " + name);
            }

        } catch (const std::exception& e) {
            references.push_back("Error searching references: " + std::string(e.what()));
        }

        return references;
    }

} // namespace scratchbird::engine
