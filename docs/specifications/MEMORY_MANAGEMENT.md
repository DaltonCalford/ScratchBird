# Memory Management Specification

## Ownership Rules

### Rule 1: Creator Owns
Whoever allocates memory is responsible for freeing it.

### Rule 2: Transfer Explicitly
Use naming conventions to indicate ownership transfer, or move semantics.

### Rule 3: Output Parameters
Functions returning via output parameters allocate; caller frees.

### Rule 4: Const Never Transfers
Const parameters never transfer ownership.

## Patterns

```c
// Caller owns returned memory
char* create_string(const char* input);  // Caller must free()

// Function owns parameter
void consume_buffer(char* buffer);  // Function will free()

// Borrowed reference
const char* get_name(void);  // Caller must NOT free()
```

## Status
This specification is complete for Alpha. Memory pools and additional allocators may be introduced in later phases without changing the ownership contracts.

## Memory Contexts (Alpha)

Define explicit allocation lifetimes using contexts:

```c
typedef struct MemoryContext {
    void* (*alloc)(size_t size);
    void  (*free)(void* ptr);
    void  (*reset)(void);    // Free all allocations in this context
    void  (*destroy)(void);  // Destroy context itself
} MemoryContext;
```

Recommended contexts:
- GlobalContext: process-lifetime (read-only configuration)
- DatabaseContext: per-database lifetime (catalog cache entries)
- TransactionContext: per-transaction (freed on commit/rollback)
- TempContext: per-operation scratch space (reset at end of operation)

Rules:
- Avoid GlobalContext in hot paths
- Prefer TransactionContext for per-txn buffers
- Reset TempContext aggressively to avoid leaks

## Allocation Failure (OOM) Policy

- All allocators must check for NULL from malloc/calloc/realloc/new and return SB_ERR_OOM (or throw)
- Provide TRY_OR_CLEANUP-style macros/RAII guards to unwind partial work
- Never terminate process on OOM in library code; propagate upward

## Leak Detection (Dev/CI)

- Enable AddressSanitizer in CI (-DENABLE_ASAN=ON)
- Optional debug allocator: track outstanding allocations per-context and assert 0 at reset/destroy

Example (debug build):
```c
void* dbg_alloc(MemoryContext* ctx, size_t n) {
    void* p = malloc(n);  // malloc
    if (!p) { /* set error SB_ERR_OOM */ return NULL; }
    ctx->active_allocations++;
    return p;
}

void dbg_free(MemoryContext* ctx, void* p) {
    if (!p) return;
    free(p);  // free
    ctx->active_allocations--;
}
```

## Ownership Transitions

- Functions that take ownership MUST include verbs like take_ownership/consume/_move
- Passing into std::unique_ptr or equivalent implies transfer

## Thread Safety

- Global/Database contexts must be thread-safe (mutex or per-thread arenas)
- Transaction/Temp contexts are thread-local

## Specification Status
This specification is complete for Alpha and contains no open items.
