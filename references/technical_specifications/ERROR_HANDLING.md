# Error Handling Specification

## Error Codes

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
    
    // TODO: Add more error codes
} sb_error_t;
```

## Error Handling Patterns

```c
#define RETURN_IF_ERROR(expr) \
    do { \
        sb_error_t err = (expr); \
        if (err != SB_OK) return err; \
    } while(0)
```

## TODO: Complete specification
- Add error context structure
- Define error propagation rules
- Add recovery strategies
