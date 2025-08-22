#ifndef SCRATCHBIRD_ENGINE_PSQL_DEV_TOOLS_H
#define SCRATCHBIRD_ENGINE_PSQL_DEV_TOOLS_H

#include "scratchbird/engine/ast.h"
#include "scratchbird/engine/executor.h"

#include <chrono>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace scratchbird::engine
{
    // PSQL Development Tools Collection

    // 1. Procedure Dependency Analyzer
    class PsqlDependencyAnalyzer
    {
      public:
        struct Dependency {
            std::string name;
            std::string type; // "procedure", "function", "table", "view"
            bool exists{false};
        };

        struct AnalysisResult {
            std::string procedure_name;
            std::vector<Dependency> dependencies;
            std::vector<std::string> warnings;
            std::vector<std::string> errors;
        };

        // Analyze dependencies in PSQL code
        AnalysisResult analyze_dependencies(const std::string& procedure_name,
                                            const std::string& psql_code);

        // Get dependency graph for multiple procedures
        std::map<std::string, AnalysisResult>
        analyze_dependency_graph(const std::map<std::string, std::string>& procedures);

        // Find circular dependencies
        std::vector<std::vector<std::string>>
        find_circular_dependencies(const std::map<std::string, AnalysisResult>& analysis);

      private:
        void extract_calls(const std::string& code, std::vector<Dependency>& deps);
        void extract_table_references(const std::string& code, std::vector<Dependency>& deps);
    };

    // 2. PSQL Code Formatter
    class PsqlCodeFormatter
    {
      public:
        struct FormatOptions {
            int indent_size{4};
            bool uppercase_keywords{true};
            bool align_assignments{true};
            bool break_long_lines{true};
            int max_line_length{120};
            bool normalize_whitespace{true};
        };

        // Format PSQL code with specified options
        std::string format_code(const std::string& psql_code);
        std::string format_code(const std::string& psql_code, const FormatOptions& options);

        // Format individual statement
        std::string format_statement(const std::string& statement);
        std::string format_statement(const std::string& statement, const FormatOptions& options);

        // Validate formatting (check if code is well-formatted)
        bool is_well_formatted(const std::string& psql_code);
        bool is_well_formatted(const std::string& psql_code, const FormatOptions& options);

      private:
        std::string format_keywords(const std::string& code, bool uppercase);
        std::string normalize_whitespace(const std::string& code);
        std::string apply_indentation(const std::string& code, int indent_size);
        std::string break_long_lines(const std::string& code, int max_length);
    };

    // 3. Performance Profiler for Procedures
    class PsqlPerformanceProfiler
    {
      public:
        struct ProfileMetrics {
            std::string procedure_name;
            size_t execution_count{0};
            std::chrono::microseconds total_time{0};
            std::chrono::microseconds avg_time{0};
            std::chrono::microseconds min_time{std::chrono::microseconds::max()};
            std::chrono::microseconds max_time{0};
            size_t memory_usage{0}; // bytes
            std::map<std::string, size_t> statement_counts;
        };

        // Start profiling a procedure execution
        void start_profiling(const std::string& procedure_name);

        // Stop profiling and record metrics
        void stop_profiling(const std::string& procedure_name);

        // Record statement execution
        void record_statement(const std::string& procedure_name, const std::string& statement_type);

        // Get profiling results
        ProfileMetrics get_metrics(const std::string& procedure_name) const;

        // Get all profiling results
        std::map<std::string, ProfileMetrics> get_all_metrics() const;

        // Clear profiling data
        void clear_metrics();

        // Generate performance report
        std::string generate_report() const;

      private:
        std::map<std::string, ProfileMetrics> metrics_;
        std::map<std::string, std::chrono::steady_clock::time_point> start_times_;
    };

    // 4. PSQL Syntax Validator
    class PsqlSyntaxValidator
    {
      public:
        struct ValidationIssue {
            std::string type; // "error", "warning", "info"
            int line_number{0};
            int column{0};
            std::string message;
            std::string suggestion;
        };

        struct ValidationResult {
            bool is_valid{true};
            std::vector<ValidationIssue> issues;
            std::string formatted_report;
        };

        // Validate PSQL syntax
        ValidationResult validate_syntax(const std::string& psql_code);

        // Validate specific PSQL constructs
        ValidationResult validate_procedure(const std::string& procedure_code);
        ValidationResult validate_function(const std::string& function_code);
        ValidationResult validate_package(const std::string& package_code);

        // Check for common issues
        std::vector<ValidationIssue> check_common_issues(const std::string& psql_code);

        // Generate validation report
        std::string generate_report(const ValidationResult& result);

      private:
        void validate_variable_declarations(const std::string& code,
                                            std::vector<ValidationIssue>& issues);
        void validate_control_flow(const std::string& code, std::vector<ValidationIssue>& issues);
        void validate_exception_handling(const std::string& code,
                                         std::vector<ValidationIssue>& issues);
    };

    // 5. Integrated Development Environment Helper
    class PsqlDevEnvironment
    {
      public:
        PsqlDevEnvironment(const std::string& db_path = "");

        // Comprehensive code analysis
        struct CodeAnalysis {
            PsqlDependencyAnalyzer::AnalysisResult dependencies;
            PsqlSyntaxValidator::ValidationResult validation;
            PsqlPerformanceProfiler::ProfileMetrics performance;
            std::string formatted_code;
        };

        // Analyze PSQL procedure/function
        CodeAnalysis analyze_code(const std::string& name, const std::string& code);

        // Get code completion suggestions
        std::vector<std::string> get_code_completion(const std::string& partial_code,
                                                     int cursor_position);

        // Find procedure/function definition
        std::string find_definition(const std::string& name);

        // Get usage references
        std::vector<std::string> find_references(const std::string& name);

      private:
        std::string db_path_;
        PsqlDependencyAnalyzer dependency_analyzer_;
        PsqlCodeFormatter code_formatter_;
        PsqlPerformanceProfiler profiler_;
        PsqlSyntaxValidator syntax_validator_;
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_PSQL_DEV_TOOLS_H
