# Memory Management Specification

## Ownership Rules

### Rule 1: Creator Owns
Whoever allocates memory is responsible for freeing it.

### Rule 2: Transfer Explicitly
Use naming conventions to indicate ownership transfer.

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

## TODO: Complete specification
- Add memory pool design
- Define allocation failure handling
- Add leak detection strategy
