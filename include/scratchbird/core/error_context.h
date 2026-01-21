#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include "scratchbird/core/status.h"
#include "scratchbird/core/sqlstate.h"

namespace scratchbird::core
{

    // Error context structure per ERROR_HANDLING.md
    // P2-17: Enhanced with constraint violation context
    struct ErrorContext
    {
        Status code{Status::OK};       // Error code
        const char* sqlstate{SQLSTATE_SUCCESS}; // SQLSTATE (5-char SQL standard error code)
        std::string sqlstate_text;     // Owned SQLSTATE when custom values are provided
        std::string message;           // Human-readable description
        std::string& error_message = message;  // Alias for legacy code
        const char *file{nullptr};     // Source file
        int line{0};                   // Line number
        const char *function{nullptr}; // Function name
        ErrorContext *cause{nullptr};  // Optional chained cause

        // P2-17: Constraint violation context for enhanced error messages
        std::string constraint_name;           // Name of violated constraint
        std::string table_name;                // Table where violation occurred
        std::string column_name;               // Column involved (if applicable)
        std::string violating_value;           // The value that caused the violation
        std::string referenced_table;          // FK: referenced parent table
        std::string referenced_column;         // FK: referenced parent column
        std::string check_expression;          // CHECK: the expression that failed
        std::string hint;                      // Suggested remediation

        ErrorContext() {}

        ~ErrorContext()
        {
            if (cause != nullptr)
            {
                delete cause;
                cause = nullptr;
            }
        }

        // Delete copy operations (prevent double-free bugs)
        ErrorContext(const ErrorContext &) = delete;
        ErrorContext &operator=(const ErrorContext &) = delete;

        // Delete move operations (prevent use-after-move bugs)
        ErrorContext(ErrorContext &&) = delete;
        ErrorContext &operator=(ErrorContext &&) = delete;

        void set(Status err_code, const char *msg, const char *f, int l, const char *func)
        {
            code = err_code;
            sqlstate = statusToSQLState(err_code); // Automatically map Status to SQLSTATE
            sqlstate_text.clear();
            message = (msg != nullptr) ? msg : "";
            file = f;
            line = l;
            function = func;
        }

        // Optional: Override SQLSTATE manually (for specific cases)
        void setSQLState(const char* custom_sqlstate)
        {
            if (custom_sqlstate == nullptr)
            {
                sqlstate = statusToSQLState(code);
                sqlstate_text.clear();
                return;
            }
            sqlstate_text = custom_sqlstate;
            sqlstate = sqlstate_text.c_str();
        }
    };

// Macro for setting error context (always safe, checks for nullptr)
#define SET_ERROR_CONTEXT(ctx, err_code, msg)                            \
    do                                                                   \
    {                                                                    \
        if (ctx)                                                         \
        {                                                                \
            (ctx)->set((err_code), (msg), __FILE__, __LINE__, __func__); \
        }                                                                \
    } while (0)

// Optional helper macros (Phase 0 - October 7, 2025)

// Assert ctx is non-null in debug builds (for functions requiring error context)
#define REQUIRE_ERROR_CONTEXT(ctx, err_code, msg)                       \
    do                                                                  \
    {                                                                   \
        assert((ctx) != nullptr && "ErrorContext must not be nullptr"); \
        (ctx)->set((err_code), (msg), __FILE__, __LINE__, __func__);    \
    } while (0)

// P2-17: Enhanced constraint violation macros with context
// Foreign key violation with full context
#define SET_FK_VIOLATION(ctx, constraint, table, col, value, ref_table, ref_col) \
    do                                                                           \
    {                                                                            \
        if (ctx)                                                                 \
        {                                                                        \
            (ctx)->code = Status::FOREIGN_KEY_VIOLATION;                         \
            (ctx)->sqlstate = SQLSTATE_FOREIGN_KEY_VIOLATION;                    \
            (ctx)->constraint_name = (constraint);                               \
            (ctx)->table_name = (table);                                         \
            (ctx)->column_name = (col);                                          \
            (ctx)->violating_value = (value);                                    \
            (ctx)->referenced_table = (ref_table);                               \
            (ctx)->referenced_column = (ref_col);                                \
            (ctx)->message = "Foreign key constraint '" + std::string(constraint) + \
                "' violated: no parent row found for " + std::string(col) +      \
                " = " + std::string(value) + " in table '" + std::string(ref_table) + "'"; \
            (ctx)->hint = "Insert a matching row in '" + std::string(ref_table) + \
                "' first, or use an existing " + std::string(ref_col) + " value"; \
            (ctx)->file = __FILE__;                                              \
            (ctx)->line = __LINE__;                                              \
            (ctx)->function = __func__;                                          \
        }                                                                        \
    } while (0)

// Unique constraint violation with context
#define SET_UNIQUE_VIOLATION(ctx, constraint, table, col, value)                 \
    do                                                                           \
    {                                                                            \
        if (ctx)                                                                 \
        {                                                                        \
            (ctx)->code = Status::UNIQUE_VIOLATION;                              \
            (ctx)->sqlstate = SQLSTATE_UNIQUE_VIOLATION;                         \
            (ctx)->constraint_name = (constraint);                               \
            (ctx)->table_name = (table);                                         \
            (ctx)->column_name = (col);                                          \
            (ctx)->violating_value = (value);                                    \
            (ctx)->message = "Unique constraint '" + std::string(constraint) +   \
                "' violated: duplicate value '" + std::string(value) +           \
                "' in column '" + std::string(col) + "'";                        \
            (ctx)->hint = "Use a different value or UPDATE the existing row";    \
            (ctx)->file = __FILE__;                                              \
            (ctx)->line = __LINE__;                                              \
            (ctx)->function = __func__;                                          \
        }                                                                        \
    } while (0)

// NOT NULL violation with context
#define SET_NOT_NULL_VIOLATION(ctx, constraint, table, col)                      \
    do                                                                           \
    {                                                                            \
        if (ctx)                                                                 \
        {                                                                        \
            (ctx)->code = Status::NOT_NULL_VIOLATION;                            \
            (ctx)->sqlstate = SQLSTATE_NOT_NULL_VIOLATION;                       \
            (ctx)->constraint_name = (constraint);                               \
            (ctx)->table_name = (table);                                         \
            (ctx)->column_name = (col);                                          \
            (ctx)->message = "NOT NULL constraint violated: column '" +          \
                std::string(col) + "' cannot be NULL";                           \
            (ctx)->hint = "Provide a non-NULL value for column '" +              \
                std::string(col) + "'";                                          \
            (ctx)->file = __FILE__;                                              \
            (ctx)->line = __LINE__;                                              \
            (ctx)->function = __func__;                                          \
        }                                                                        \
    } while (0)

// CHECK constraint violation with context
#define SET_CHECK_VIOLATION(ctx, constraint, table, expr, value)                 \
    do                                                                           \
    {                                                                            \
        if (ctx)                                                                 \
        {                                                                        \
            (ctx)->code = Status::CHECK_VIOLATION;                               \
            (ctx)->sqlstate = SQLSTATE_CHECK_VIOLATION;                          \
            (ctx)->constraint_name = (constraint);                               \
            (ctx)->table_name = (table);                                         \
            (ctx)->check_expression = (expr);                                    \
            (ctx)->violating_value = (value);                                    \
            (ctx)->message = "Check constraint '" + std::string(constraint) +    \
                "' violated: value does not satisfy: " + std::string(expr);      \
            (ctx)->hint = "Ensure value satisfies: " + std::string(expr);        \
            (ctx)->file = __FILE__;                                              \
            (ctx)->line = __LINE__;                                              \
            (ctx)->function = __func__;                                          \
        }                                                                        \
    } while (0)

} // namespace scratchbird::core
