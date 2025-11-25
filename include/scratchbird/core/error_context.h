#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include "scratchbird/core/status.h"
#include "scratchbird/core/sqlstate.h"

namespace scratchbird::core
{

    // Error context structure per ERROR_HANDLING.md
    struct ErrorContext
    {
        Status code{Status::OK};       // Error code
        const char* sqlstate{SQLSTATE_SUCCESS}; // SQLSTATE (5-char SQL standard error code)
        std::string message;           // Human-readable description
        std::string& error_message = message;  // Alias for legacy code
        const char *file{nullptr};     // Source file
        int line{0};                   // Line number
        const char *function{nullptr}; // Function name
        ErrorContext *cause{nullptr};  // Optional chained cause

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
            message = (msg != nullptr) ? msg : "";
            file = f;
            line = l;
            function = func;
        }

        // Optional: Override SQLSTATE manually (for specific cases)
        void setSQLState(const char* custom_sqlstate)
        {
            sqlstate = custom_sqlstate;
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

    // Placeholder for SET_ERROR_CONTEXT_LOG (requires logging framework from Phase 1)
    // Will be uncommented after logging framework is implemented
    /*
    #define SET_ERROR_CONTEXT_LOG(ctx, err_code, msg)                        \
        do                                                                   \
        {                                                                    \
            SET_ERROR_CONTEXT(ctx, err_code, msg);                          \
            LOG_ERROR(GENERAL, "%s:%d %s - %s",                             \
                     __FILE__, __LINE__, __func__, (msg));                  \
        } while (0)
    */

} // namespace scratchbird::core
