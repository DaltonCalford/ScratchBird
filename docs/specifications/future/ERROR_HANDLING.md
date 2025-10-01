# Error Handling Specification

---

## IMPLEMENTATION STATUS: NOT IMPLEMENTED - C API DESIGN ONLY

**IMPORTANT:** This specification describes C-style error handling using `sb_error_t` enums. This **DOES NOT MATCH** the current C++ implementation.

**Current Implementation:**
- ScratchBird uses C++ `enum class Status` (see `include/scratchbird/core/status.h`)
- Uses different error codes and semantics
- This C-style API does not exist

**This document is for the future C API only.** For current error handling, see the C++ Status class implementation.

---

## Error Codes (C API - NOT IMPLEMENTED)

```c
typedef enum sb_error {
    SB_OK = 0,
    
    // File errors (1000-1999)
    SB_ERR_FILE_NOT_FOUND = 1001,
    SB_ERR_FILE_EXISTS = 1002,
    SB_ERR_IO_ERROR = 1003,
    
    // Page errors (2000-2999)
    SB_ERR_PAGE_CORRUPT = 2001,
    SB_ERR_CHECKSUM_MISMATCH = 2002,
    
    // Transaction errors (3000-3999)
    SB_ERR_DEADLOCK = 3001,
    SB_ERR_LOCK_TIMEOUT = 3002,
    SB_ERR_OOM = 3003,
    
    // Additional error codes may be added in future phases
} sb_error_t;
```

## Error Handling Patterns

```c
#define RETURN_IF_ERROR(expr) \
    do { \
        sb_error_t err = (expr); \
        if (err != SB_OK) return err; \
    } while(0)

#define TRY_OR_CLEANUP(expr, cleanup) \
    do { \
        sb_error_t _err = (expr); \
        if (_err != SB_OK) { \
            cleanup; \
            return _err; \
        } \
    } while(0)
```

## Error Context Structure

```c
typedef struct sb_error_context {
    sb_error_t   code;           // Error code
    const char*  message;        // Human-readable description
    const char*  file;           // Source file
    int          line;           // Line number
    const char*  function;       // Function name
    struct sb_error_context* cause; // Optional chained cause
} sb_error_context_t;

#define SB_MAKE_ERROR(ctx_ptr, err_code, msg) \
    do { \
        (ctx_ptr)->code = (err_code); \
        (ctx_ptr)->message = (msg); \
        (ctx_ptr)->file = __FILE__; \
        (ctx_ptr)->line = __LINE__; \
        (ctx_ptr)->function = __func__; \
        (ctx_ptr)->cause = NULL; \
    } while(0)
```

## Propagation Rules

- Functions MAY accept an optional `sb_error_context_t* out_ctx`
- On failure, set out_ctx (if non-null) with SB_MAKE_ERROR and return non-SB_OK
- If an error occurs inside a callee, set `ctx.cause = callee_ctx`
- Always preserve the original root cause when rewrapping

## Recovery Classes

- Retryable: SB_ERR_IO_ERROR (transient), SB_ERR_LOCK_TIMEOUT
- Irrecoverable: SB_ERR_PAGE_CORRUPT, SB_ERR_CHECKSUM_MISMATCH, SB_ERR_OOM
- Policy: Callers decide to retry based on class + operation idempotency

## Mapping and Logging (Guidance)

- Map internal codes to protocol-specific errors (later phases)
- Include page_id, file path, LSN where relevant in messages
- In debug builds, include backtrace when available

## Error Code Semantics (Explicit)

Each error is identified by an error_code of type `sb_error_t`. Functions MUST
return `SB_OK` on success or a non-zero error_code on failure. When an
`sb_error_context_t*` is provided, the callee MUST populate it with details
about the failure and optionally chain a cause for deeper diagnosis.

## Specification Status

This specification is complete for Alpha requirements. Enhancements may extend the error_code catalog in later phases without altering the contracts defined here.

## Examples

```c
sb_error_t read_page_checked(int fd, uint32_t page_id, uint8_t* buf, uint32_t ps, sb_error_context_t* ctx) {
    off_t off = (off_t)page_id * ps;
    if (lseek(fd, off, SEEK_SET) != off) {
        SB_MAKE_ERROR(ctx, SB_ERR_IO_ERROR, "seek failed");
        return SB_ERR_IO_ERROR;
    }
    ssize_t n = read(fd, buf, ps);
    if (n != (ssize_t)ps) {
        SB_MAKE_ERROR(ctx, SB_ERR_IO_ERROR, "short read");
        return SB_ERR_IO_ERROR;
    }
    const PageHeader* h = (const PageHeader*)buf;
    if (h->magic != 0x53425244) {
        SB_MAKE_ERROR(ctx, SB_ERR_PAGE_CORRUPT, "bad magic");
        return SB_ERR_PAGE_CORRUPT;
    }
    if (!validate_page_checksum(buf, ps)) {
        SB_MAKE_ERROR(ctx, SB_ERR_CHECKSUM_MISMATCH, "checksum mismatch");
        return SB_ERR_CHECKSUM_MISMATCH;
    }
    return SB_OK;
}
```
