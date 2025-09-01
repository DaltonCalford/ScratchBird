#pragma once

#include <cstdint>
#include <string>
#include "scratchbird/core/status.h"

namespace scratchbird {
namespace core {

// Error context structure per ERROR_HANDLING.md
struct ErrorContext {
    Status code;                    // Error code
    std::string message;            // Human-readable description
    const char* file;              // Source file
    int line;                      // Line number
    const char* function;          // Function name
    ErrorContext* cause;           // Optional chained cause
    
    ErrorContext() : code(Status::Ok), file(nullptr), line(0), function(nullptr), cause(nullptr) {}
    
    ~ErrorContext() {
        if (cause) {
            delete cause;
            cause = nullptr;
        }
    }
    
    void set(Status err_code, const char* msg, const char* f, int l, const char* func) {
        code = err_code;
        message = msg ? msg : "";
        file = f;
        line = l;
        function = func;
    }
};

// Macro for setting error context
#define SET_ERROR_CONTEXT(ctx, err_code, msg) \
    do { \
        if (ctx) { \
            (ctx)->set((err_code), (msg), __FILE__, __LINE__, __func__); \
        } \
    } while(0)

} // namespace core
} // namespace scratchbird