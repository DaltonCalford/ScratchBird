#pragma once

#include <cstdint>
#include <string>
#include "scratchbird/core/status.h"


    namespace scratchbird::core
    {

        // Error context structure per ERROR_HANDLING.md
        struct ErrorContext
        {
            Status code{Status::OK};          // Error code
            std::string message;  // Human-readable description
            const char *file{nullptr};     // Source file
            int line{0};             // Line number
            const char *function{nullptr}; // Function name
            ErrorContext *cause{nullptr};  // Optional chained cause

            ErrorContext()

            {
            }

            ~ErrorContext()
            {
                if (cause != nullptr)
                {
                    delete cause;
                    cause = nullptr;
                }
            }

            // Delete copy operations (prevent double-free bugs)
            ErrorContext(const ErrorContext&) = delete;
            ErrorContext& operator=(const ErrorContext&) = delete;

            // Delete move operations (prevent use-after-move bugs)
            ErrorContext(ErrorContext&&) = delete;
            ErrorContext& operator=(ErrorContext&&) = delete;

            void set(Status err_code, const char *msg, const char *f, int l, const char *func)
            {
                code = err_code;
                message = (msg != nullptr) ? msg : "";
                file = f;
                line = l;
                function = func;
            }
        };

// Macro for setting error context
#define SET_ERROR_CONTEXT(ctx, err_code, msg)                            \
    do                                                                   \
    {                                                                    \
        if (ctx)                                                         \
        {                                                                \
            (ctx)->set((err_code), (msg), __FILE__, __LINE__, __func__); \
        }                                                                \
    } while (0)

    } // namespace scratchbird::core
