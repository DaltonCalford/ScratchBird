# ScratchBird Complete Bytecode Specification (SBLR)

## Version 1.1 - Unified BLR/SBLR Specification

### Table of Contents
1. [Overview](#overview)
2. [Historical Context: BLR Heritage](#historical-context-blr-heritage)
3. [Design Principles](#design-principles)
4. [Bytecode Structure](#bytecode-structure)
5. [Complete Instruction Set](#complete-instruction-set)
6. [Type System](#type-system)
7. [Memory Model](#memory-model)
8. [Code Generation](#code-generation)
9. [Optimization Strategies](#optimization-strategies)
10. [Adaptive Execution](#adaptive-execution)
11. [JIT Compilation](#jit-compilation)
12. [Runtime Execution](#runtime-execution)
13. [Debugging and Tools](#debugging-and-tools)
14. [Migration from BLR](#migration-from-blr)
15. [Implementation Guidelines](#implementation-guidelines)

---

## Overview

ScratchBird Bytecode Language Representation (SBLR) is a comprehensive bytecode format that evolves from FirebirdSQL's Binary Language Representation (BLR) while incorporating modern optimization techniques. This specification combines the simplicity and proven design of BLR with adaptive optimization from Python 3.11+ and JIT compilation capabilities from PostgreSQL and SQL Server.

### Key Enhancements over Traditional BLR

1. **Adaptive Specialization**: Runtime type profiling and instruction specialization
2. **JIT Compilation Support**: Framework for compiling hot paths to native code
3. **Vectorization**: SIMD operations for batch processing
4. **Extended Type System**: Richer type information for optimization
5. **Debug Information**: Source mapping and profiling data
6. **Backward Compatibility**: Can execute legacy BLR with translation layer

---

## Historical Context: BLR Heritage

### Original BLR Design (FirebirdSQL)

Binary Language Representation (BLR) was designed as a compact, platform-independent bytecode for database operations. Key characteristics:

- **Stack-based execution model**: Simple and compact
- **Database-centric opcodes**: Optimized for SQL operations
- **Type-aware instructions**: Built-in SQL type handling
- **Compact encoding**: Minimal memory footprint

### BLR Opcode Categories from FirebirdSQL

The original BLR instruction set that SBLR extends:

```c
// Classic BLR version markers
#define blr_version4    4    // Older version
#define blr_version5    5    // Current version

// Core BLR instruction ranges preserved in SBLR
enum blr_legacy_opcodes {
    // Control (0x00-0x0F) - Preserved
    blr_begin = 2,
    blr_end = 255,
    blr_message = 4,
    blr_eoc = 76,
    blr_assignment = 1,
    
    // Data types (0x07-0x27) - Enhanced in SBLR
    blr_text = 14,
    blr_text2 = 15,
    blr_short = 7,
    blr_long = 8,
    blr_int64 = 16,
    blr_float = 10,
    blr_double = 27,
    blr_timestamp = 35,
    blr_varying = 37,
    blr_varying2 = 38,
    blr_blob = 261,
    blr_blob2 = 262,
    blr_quad = 9,
    blr_d_float = 11,
    blr_sql_date = 12,
    blr_sql_time = 13,
    
    // Expressions (0x22-0x28) - Extended in SBLR
    blr_add = 34,
    blr_subtract = 35,
    blr_multiply = 36,
    blr_divide = 37,
    blr_negate = 38,
    blr_concatenate = 39,
    blr_substring = 40,
    blr_trim = 41,
    blr_cast = 124,
    blr_upcase = 42,
    blr_lowcase = 43,
    
    // Comparisons (0x2F-0x36) - Preserved
    blr_eql = 47,
    blr_neq = 48,
    blr_gtr = 49,
    blr_geq = 50,
    blr_lss = 51,
    blr_leq = 52,
    blr_between = 53,
    blr_like = 54,
    blr_contains = 55,
    blr_matching = 56,
    blr_starting = 57,
    blr_similar = 58,
    
    // Boolean operations (0x3A-0x3C)
    blr_and = 58,
    blr_or = 59,
    blr_not = 60,
    blr_any = 61,
    blr_unique = 62,
    blr_all = 63,
    
    // Control flow (0x3C-0x42) - Enhanced
    blr_if = 60,
    blr_loop = 61,
    blr_for = 62,
    blr_while = 63,
    blr_leave = 64,
    blr_continue = 65,
    blr_do_while = 66,
    blr_break = 67,
    
    // Database operations (0x46-0x4D) - Core of BLR
    blr_store = 70,
    blr_store2 = 71,
    blr_modify = 72,
    blr_modify2 = 73,
    blr_erase = 74,
    blr_erase2 = 75,
    blr_fetch = 76,
    blr_for_select = 77,
    blr_send = 78,
    blr_receive = 79,
    blr_select = 80,
    blr_rse = 81,           // Record Selection Expression
    blr_first = 82,
    blr_project = 83,
    blr_sort = 84,
    blr_boolean = 85,
    blr_ascending = 86,
    blr_descending = 87,
    blr_relation = 88,
    blr_relation2 = 89,
    blr_rid = 90,
    blr_union = 91,
    blr_map = 92,
    blr_group_by = 93,
    blr_aggregate = 94,
    blr_join_type = 95,
    
    // Functions (0x64-0x6C) - Extended
    blr_function = 100,
    blr_gen_id = 101,
    blr_gen_id2 = 102,
    blr_count = 103,
    blr_count2 = 104,
    blr_max = 105,
    blr_min = 106,
    blr_average = 107,
    blr_total = 108,
    blr_from = 109,
    blr_via = 110,
    blr_user_name = 111,
    blr_current_date = 112,
    blr_current_time = 113,
    blr_current_timestamp = 114,
    blr_current_role = 115,
    
    // Field/Variable access (0x78-0x7C) - Preserved
    blr_field = 120,
    blr_field2 = 121,
    blr_parameter = 122,
    blr_parameter2 = 123,
    blr_variable = 124,
    blr_literal = 125,
    blr_dbkey = 126,
    blr_index = 127,
    blr_bookmark = 128,
    
    // Procedure/trigger (0x82-0x86) - Enhanced
    blr_procedure = 130,
    blr_procedure2 = 131,
    blr_trigger = 132,
    blr_exec_proc = 133,
    blr_exec_proc2 = 134,
    blr_exec_stmt = 135,
    blr_exec_into = 136,
    blr_exec_sql = 137,
    blr_internal_info = 138,
    blr_block = 139,
    
    // Handlers and exceptions (0x8A-0x8F)
    blr_error_handler = 140,
    blr_start_savepoint = 141,
    blr_end_savepoint = 142,
    blr_handler = 143,
    blr_post = 144,
    blr_post_arg = 145,
    blr_put_slice = 146,
    blr_get_slice = 147,
    blr_dcl_variable = 148,
    
    // Plan and optimization hints (0x96-0x9A)
    blr_plan = 150,
    blr_merge = 151,
    blr_join = 152,
    blr_sequential = 153,
    blr_navigational = 154,
    blr_indices = 155,
    blr_retrieve = 156,
    
    // Cursor operations (0xA0-0xA5)
    blr_declare_cursor = 160,
    blr_cursor_stmt = 161,
    blr_open_cursor = 162,
    blr_close_cursor = 163,
    blr_fetch_cursor = 164,
    blr_cursor_name = 165,
    
    // SQL standard functions (0xAA-0xAF)
    blr_extract = 170,
    blr_extract_year = 171,
    blr_extract_month = 172,
    blr_extract_day = 173,
    blr_extract_hour = 174,
    blr_extract_minute = 175,
    blr_extract_second = 176,
    blr_extract_weekday = 177,
    blr_extract_yearday = 178,
    
    // Null handling (0xB0-0xB2)
    blr_null = 180,
    blr_equiv = 181,
    blr_not_equiv = 182,
    blr_missing = 183,
    blr_not_missing = 184,
    
    // Set operations (0xB8-0xBA)
    blr_list = 188,
    blr_in_list = 189,
    blr_any_in_list = 190,
    blr_all_in_list = 191,
    blr_not_in_list = 192
};
```

---

## Design Principles

### Core Philosophy

1. **Backward Compatibility**: Support existing BLR code through translation
2. **Progressive Enhancement**: Start simple, optimize based on runtime behavior
3. **Database-First Design**: Optimized for SQL and database operations
4. **Memory Efficiency**: Compact representation with efficient caching
5. **Type Safety**: Strong type system for correctness and optimization
6. **Debuggability**: Rich debug information and tooling support

### Design Goals

- **Performance**: Match or exceed native SQL execution speed
- **Scalability**: Efficient from single queries to complex procedures
- **Maintainability**: Clear specification and implementation
- **Extensibility**: Room for future enhancements
- **Interoperability**: Work with existing database infrastructure

---

## Bytecode Structure

### Enhanced File Format

```c
// SBLR File Header (Extended from BLR)
typedef struct SBLR_Header {
    // Magic and version
    uint32_t    magic;              // 0x53424C52 ('SBLR')
    uint16_t    version_major;      // 1
    uint16_t    version_minor;      // 1
    uint8_t     blr_compat_version; // BLR compatibility (5 for v5)
    uint8_t     reserved_byte;      // Alignment
    
    // Flags and metadata
    uint32_t    flags;              // Compilation flags
    uint32_t    checksum;           // CRC32 of bytecode
    uint64_t    timestamp;          // Compilation timestamp
    uint32_t    source_hash;        // Hash of source SQL
    
    // Section sizes
    uint32_t    code_size;          // Size of code section
    uint32_t    const_size;         // Size of constants pool
    uint32_t    type_size;          // Size of type information
    uint32_t    debug_size;         // Size of debug info
    uint32_t    profile_size;       // Size of profile data
    
    // Execution requirements
    uint32_t    stack_size;         // Required stack size
    uint32_t    heap_size;          // Required heap size
    uint32_t    temp_size;          // Required temp space
    
    // Entry points
    uint32_t    main_entry;         // Main entry point
    uint32_t    init_entry;         // Initialization entry
    uint32_t    cleanup_entry;      // Cleanup entry
    
    // Reserved for future use
    uint32_t    reserved[8];
} SBLR_Header;

// Module sections
typedef struct SBLR_Module {
    SBLR_Header     header;
    
    // Code section
    uint8_t*        code;               // Bytecode instructions
    uint32_t        code_length;        // Code length
    
    // Constants pool (BLR-compatible)
    SBLR_Constant*  constants;          // Constant values
    uint32_t        const_count;        // Number of constants
    
    // Type information (Enhanced)
    SBLR_TypeInfo*  type_info;          // Type descriptors
    uint32_t        type_count;         // Number of types
    
    // Symbol table
    SBLR_Symbol*    symbols;            // Symbol information
    uint32_t        symbol_count;       // Number of symbols
    
    // Debug information (Optional)
    SBLR_DebugInfo* debug_info;         // Debug data
    
    // Profile data (Optional)
    SBLR_ProfileData* profile_data;     // Runtime profile
    
    // Message descriptors (BLR compatibility)
    SBLR_Message*   messages;           // Message formats
    uint32_t        message_count;      // Number of messages
    
    // Relation descriptors
    SBLR_Relation*  relations;          // Relation information
    uint32_t        relation_count;     // Number of relations
    
    // Exception table
    SBLR_Exception* exceptions;         // Exception handlers
    uint32_t        exception_count;    // Number of handlers
} SBLR_Module;
```

### Instruction Encoding (BLR-Compatible)

```c
// Instruction format preserves BLR encoding
typedef struct SBLR_Instruction {
    uint8_t     opcode;             // Operation code (BLR compatible)
    uint8_t     flags;              // SBLR extension flags
    union {
        // BLR-style arguments
        struct {
            uint8_t     count;      // Argument count
            uint8_t     args[1];    // Variable arguments
        } blr;
        
        // SBLR extended arguments
        struct {
            uint16_t    arg1;
            uint16_t    arg2;
        } extended;
        
        // Immediate values
        struct {
            union {
                int32_t     int_val;
                float       float_val;
                uint32_t    offset;
            } immediate;
        } imm;
    } args;
} SBLR_Instruction;

// Encoding helpers for BLR compatibility
#define BLR_ENCODE_BYTE(p, v)    *(p)++ = (uint8_t)(v)
#define BLR_ENCODE_WORD(p, v)    do { \
    *(p)++ = (uint8_t)((v) & 0xFF); \
    *(p)++ = (uint8_t)((v) >> 8); \
} while(0)
#define BLR_ENCODE_LONG(p, v)    do { \
    *(p)++ = (uint8_t)((v) & 0xFF); \
    *(p)++ = (uint8_t)(((v) >> 8) & 0xFF); \
    *(p)++ = (uint8_t)(((v) >> 16) & 0xFF); \
    *(p)++ = (uint8_t)((v) >> 24); \
} while(0)
```

---

## Complete Instruction Set

### Instruction Categories with BLR Compatibility

```c
// SBLR Opcode ranges (Preserving BLR compatibility)
enum SBLR_Opcodes {
    // ============================================
    // BLR-Compatible Range (0x00-0xFF)
    // ============================================
    
    // Core control (0x00-0x0F) - BLR compatible
    SBLR_NOP            = 0x00,     // No operation
    SBLR_VERSION        = 0x01,     // Version marker (blr_version)
    SBLR_BEGIN          = 0x02,     // Begin block (blr_begin)
    SBLR_END            = 0xFF,     // End block (blr_end)
    SBLR_MESSAGE        = 0x04,     // Message descriptor (blr_message)
    SBLR_EOC            = 0x4C,     // End of command (blr_eoc)
    SBLR_ASSIGNMENT     = 0x01,     // Assignment (blr_assignment)
    
    // Data types (0x07-0x27) - BLR compatible
    SBLR_SHORT          = 0x07,     // 16-bit integer (blr_short)
    SBLR_LONG           = 0x08,     // 32-bit integer (blr_long)
    SBLR_QUAD           = 0x09,     // 64-bit integer (blr_quad)
    SBLR_FLOAT          = 0x0A,     // 32-bit float (blr_float)
    SBLR_D_FLOAT        = 0x0B,     // 64-bit float (blr_d_float)
    SBLR_SQL_DATE       = 0x0C,     // SQL DATE (blr_sql_date)
    SBLR_SQL_TIME       = 0x0D,     // SQL TIME (blr_sql_time)
    SBLR_TEXT           = 0x0E,     // Fixed text (blr_text)
    SBLR_TEXT2          = 0x0F,     // Text with length (blr_text2)
    SBLR_INT64          = 0x10,     // 64-bit integer (blr_int64)
    SBLR_DOUBLE         = 0x1B,     // Double precision (blr_double)
    SBLR_TIMESTAMP      = 0x23,     // Timestamp (blr_timestamp)
    SBLR_VARYING        = 0x25,     // Varying string (blr_varying)
    SBLR_VARYING2       = 0x26,     // Varying with length (blr_varying2)
    
    // Arithmetic (0x22-0x28) - BLR compatible
    SBLR_ADD            = 0x22,     // Addition (blr_add)
    SBLR_SUBTRACT       = 0x23,     // Subtraction (blr_subtract)
    SBLR_MULTIPLY       = 0x24,     // Multiplication (blr_multiply)
    SBLR_DIVIDE         = 0x25,     // Division (blr_divide)
    SBLR_NEGATE         = 0x26,     // Negation (blr_negate)
    SBLR_CONCATENATE    = 0x27,     // String concat (blr_concatenate)
    SBLR_SUBSTRING      = 0x28,     // Substring (blr_substring)
    
    // Comparison (0x2F-0x36) - BLR compatible
    SBLR_EQL            = 0x2F,     // Equal (blr_eql)
    SBLR_NEQ            = 0x30,     // Not equal (blr_neq)
    SBLR_GTR            = 0x31,     // Greater than (blr_gtr)
    SBLR_GEQ            = 0x32,     // Greater or equal (blr_geq)
    SBLR_LSS            = 0x33,     // Less than (blr_lss)
    SBLR_LEQ            = 0x34,     // Less or equal (blr_leq)
    SBLR_BETWEEN        = 0x35,     // Between (blr_between)
    SBLR_LIKE           = 0x36,     // Like pattern (blr_like)
    
    // Boolean (0x3A-0x3F) - BLR compatible
    SBLR_AND            = 0x3A,     // Logical AND (blr_and)
    SBLR_OR             = 0x3B,     // Logical OR (blr_or)
    SBLR_NOT            = 0x3C,     // Logical NOT (blr_not)
    SBLR_ANY            = 0x3D,     // ANY predicate (blr_any)
    SBLR_UNIQUE         = 0x3E,     // UNIQUE predicate (blr_unique)
    SBLR_ALL            = 0x3F,     // ALL predicate (blr_all)
    
    // Control flow (0x3C-0x42) - BLR compatible
    SBLR_IF             = 0x3C,     // If statement (blr_if)
    SBLR_LOOP           = 0x3D,     // Loop (blr_loop)
    SBLR_FOR            = 0x3E,     // For loop (blr_for)
    SBLR_WHILE          = 0x3F,     // While loop (blr_while)
    SBLR_LEAVE          = 0x40,     // Leave/break (blr_leave)
    SBLR_CONTINUE       = 0x41,     // Continue (blr_continue)
    
    // Database operations (0x46-0x5F) - BLR compatible
    SBLR_STORE          = 0x46,     // Store record (blr_store)
    SBLR_STORE2         = 0x47,     // Store with return (blr_store2)
    SBLR_MODIFY         = 0x48,     // Modify record (blr_modify)
    SBLR_MODIFY2        = 0x49,     // Modify with return (blr_modify2)
    SBLR_ERASE          = 0x4A,     // Erase record (blr_erase)
    SBLR_ERASE2         = 0x4B,     // Erase with return (blr_erase2)
    SBLR_FETCH          = 0x4C,     // Fetch record (blr_fetch)
    SBLR_FOR_SELECT     = 0x4D,     // For select loop (blr_for_select)
    SBLR_SEND           = 0x4E,     // Send message (blr_send)
    SBLR_RECEIVE        = 0x4F,     // Receive message (blr_receive)
    SBLR_SELECT         = 0x50,     // Select (blr_select)
    SBLR_RSE            = 0x51,     // Record selection (blr_rse)
    SBLR_FIRST          = 0x52,     // First n records (blr_first)
    SBLR_PROJECT        = 0x53,     // Project fields (blr_project)
    SBLR_SORT           = 0x54,     // Sort (blr_sort)
    SBLR_BOOLEAN        = 0x55,     // Boolean expression (blr_boolean)
    SBLR_ASCENDING      = 0x56,     // Ascending sort (blr_ascending)
    SBLR_DESCENDING     = 0x57,     // Descending sort (blr_descending)
    SBLR_RELATION       = 0x58,     // Relation reference (blr_relation)
    SBLR_RELATION2      = 0x59,     // Relation with alias (blr_relation2)
    SBLR_RID            = 0x5A,     // Record ID (blr_rid)
    SBLR_UNION          = 0x5B,     // Union (blr_union)
    SBLR_MAP            = 0x5C,     // Map operation (blr_map)
    SBLR_GROUP_BY       = 0x5D,     // Group by (blr_group_by)
    SBLR_AGGREGATE      = 0x5E,     // Aggregate (blr_aggregate)
    SBLR_JOIN_TYPE      = 0x5F,     // Join type (blr_join_type)
    
    // Functions (0x64-0x73) - BLR compatible
    SBLR_FUNCTION       = 0x64,     // Function call (blr_function)
    SBLR_GEN_ID         = 0x65,     // Generate ID (blr_gen_id)
    SBLR_GEN_ID2        = 0x66,     // Generate ID v2 (blr_gen_id2)
    SBLR_COUNT          = 0x67,     // Count (blr_count)
    SBLR_COUNT2         = 0x68,     // Count distinct (blr_count2)
    SBLR_MAX            = 0x69,     // Maximum (blr_max)
    SBLR_MIN            = 0x6A,     // Minimum (blr_min)
    SBLR_AVERAGE        = 0x6B,     // Average (blr_average)
    SBLR_TOTAL          = 0x6C,     // Sum/Total (blr_total)
    SBLR_FROM           = 0x6D,     // From clause (blr_from)
    SBLR_VIA            = 0x6E,     // Via clause (blr_via)
    SBLR_USER_NAME      = 0x6F,     // Current user (blr_user_name)
    SBLR_CURRENT_DATE   = 0x70,     // Current date (blr_current_date)
    SBLR_CURRENT_TIME   = 0x71,     // Current time (blr_current_time)
    SBLR_CURRENT_TIMESTAMP = 0x72,  // Current timestamp (blr_current_timestamp)
    SBLR_CURRENT_ROLE   = 0x73,     // Current role (blr_current_role)
    
    // Field/Variable (0x78-0x80) - BLR compatible
    SBLR_FIELD          = 0x78,     // Field reference (blr_field)
    SBLR_FIELD2         = 0x79,     // Field with context (blr_field2)
    SBLR_PARAMETER      = 0x7A,     // Parameter (blr_parameter)
    SBLR_PARAMETER2     = 0x7B,     // Parameter v2 (blr_parameter2)
    SBLR_VARIABLE       = 0x7C,     // Variable (blr_variable)
    SBLR_LITERAL        = 0x7D,     // Literal value (blr_literal)
    SBLR_DBKEY          = 0x7E,     // Database key (blr_dbkey)
    SBLR_INDEX          = 0x7F,     // Index reference (blr_index)
    SBLR_BOOKMARK       = 0x80,     // Bookmark (blr_bookmark)
    
    // Procedures (0x82-0x8B) - BLR compatible
    SBLR_PROCEDURE      = 0x82,     // Procedure call (blr_procedure)
    SBLR_PROCEDURE2     = 0x83,     // Procedure v2 (blr_procedure2)
    SBLR_TRIGGER        = 0x84,     // Trigger (blr_trigger)
    SBLR_EXEC_PROC      = 0x85,     // Execute procedure (blr_exec_proc)
    SBLR_EXEC_PROC2     = 0x86,     // Execute procedure v2 (blr_exec_proc2)
    SBLR_EXEC_STMT      = 0x87,     // Execute statement (blr_exec_stmt)
    SBLR_EXEC_INTO      = 0x88,     // Execute into (blr_exec_into)
    SBLR_EXEC_SQL       = 0x89,     // Execute SQL (blr_exec_sql)
    SBLR_INTERNAL_INFO  = 0x8A,     // Internal info (blr_internal_info)
    SBLR_BLOCK          = 0x8B,     // Code block (blr_block)
    
    // Exception handling (0x8C-0x93) - BLR compatible
    SBLR_ERROR_HANDLER  = 0x8C,     // Error handler (blr_error_handler)
    SBLR_START_SAVEPOINT = 0x8D,    // Start savepoint (blr_start_savepoint)
    SBLR_END_SAVEPOINT  = 0x8E,     // End savepoint (blr_end_savepoint)
    SBLR_HANDLER        = 0x8F,     // Exception handler (blr_handler)
    SBLR_POST           = 0x90,     // Post exception (blr_post)
    SBLR_POST_ARG       = 0x91,     // Post with arg (blr_post_arg)
    SBLR_PUT_SLICE      = 0x92,     // Put array slice (blr_put_slice)
    SBLR_GET_SLICE      = 0x93,     // Get array slice (blr_get_slice)
    
    // Declarations (0x94-0x95) - BLR compatible
    SBLR_DCL_VARIABLE   = 0x94,     // Declare variable (blr_dcl_variable)
    SBLR_DCL_CURSOR     = 0x95,     // Declare cursor
    
    // Plan hints (0x96-0x9B) - BLR compatible
    SBLR_PLAN           = 0x96,     // Query plan (blr_plan)
    SBLR_MERGE          = 0x97,     // Merge join (blr_merge)
    SBLR_JOIN           = 0x98,     // Join (blr_join)
    SBLR_SEQUENTIAL     = 0x99,     // Sequential scan (blr_sequential)
    SBLR_NAVIGATIONAL   = 0x9A,     // Navigational (blr_navigational)
    SBLR_INDICES        = 0x9B,     // Index hints (blr_indices)
    
    // Cursor operations (0xA0-0xA5) - BLR compatible
    SBLR_DECLARE_CURSOR = 0xA0,     // Declare cursor (blr_declare_cursor)
    SBLR_CURSOR_STMT    = 0xA1,     // Cursor statement (blr_cursor_stmt)
    SBLR_OPEN_CURSOR    = 0xA2,     // Open cursor (blr_open_cursor)
    SBLR_CLOSE_CURSOR   = 0xA3,     // Close cursor (blr_close_cursor)
    SBLR_FETCH_CURSOR   = 0xA4,     // Fetch from cursor (blr_fetch_cursor)
    SBLR_CURSOR_NAME    = 0xA5,     // Cursor name (blr_cursor_name)
    
    // SQL functions (0xAA-0xB2) - BLR compatible
    SBLR_EXTRACT        = 0xAA,     // Extract datetime (blr_extract)
    SBLR_EXTRACT_YEAR   = 0xAB,     // Extract year (blr_extract_year)
    SBLR_EXTRACT_MONTH  = 0xAC,     // Extract month (blr_extract_month)
    SBLR_EXTRACT_DAY    = 0xAD,     // Extract day (blr_extract_day)
    SBLR_EXTRACT_HOUR   = 0xAE,     // Extract hour (blr_extract_hour)
    SBLR_EXTRACT_MINUTE = 0xAF,     // Extract minute (blr_extract_minute)
    SBLR_EXTRACT_SECOND = 0xB0,     // Extract second (blr_extract_second)
    SBLR_EXTRACT_WEEKDAY = 0xB1,    // Extract weekday (blr_extract_weekday)
    SBLR_EXTRACT_YEARDAY = 0xB2,    // Extract yearday (blr_extract_yearday)
    
    // Null handling (0xB4-0xB8) - BLR compatible
    SBLR_NULL           = 0xB4,     // Null value (blr_null)
    SBLR_EQUIV          = 0xB5,     // Equivalent (blr_equiv)
    SBLR_NOT_EQUIV      = 0xB6,     // Not equivalent (blr_not_equiv)
    SBLR_MISSING        = 0xB7,     // Is missing/null (blr_missing)
    SBLR_NOT_MISSING    = 0xB8,     // Not missing (blr_not_missing)
    
    // Set operations (0xBC-0xC0) - BLR compatible
    SBLR_LIST           = 0xBC,     // Value list (blr_list)
    SBLR_IN_LIST        = 0xBD,     // In list (blr_in_list)
    SBLR_ANY_IN_LIST    = 0xBE,     // Any in list (blr_any_in_list)
    SBLR_ALL_IN_LIST    = 0xBF,     // All in list (blr_all_in_list)
    SBLR_NOT_IN_LIST    = 0xC0,     // Not in list (blr_not_in_list)
    
    // Cast and coercion (0xC8) - BLR compatible
    SBLR_CAST           = 0xC8,     // Type cast (blr_cast)
    
    // Transaction control (0xD0-0xD3) - BLR extensions
    SBLR_BEGIN_TRANS    = 0xD0,     // Begin transaction
    SBLR_COMMIT         = 0xD1,     // Commit
    SBLR_ROLLBACK       = 0xD2,     // Rollback
    SBLR_SAVEPOINT      = 0xD3,     // Savepoint
    
    // Stack operations (0xE0-0xEF) - SBLR extensions
    SBLR_DUP            = 0xE0,     // Duplicate top
    SBLR_DUP2           = 0xE1,     // Duplicate top 2
    SBLR_SWAP           = 0xE2,     // Swap top 2
    SBLR_ROT3           = 0xE3,     // Rotate top 3
    SBLR_POP            = 0xE4,     // Pop value
    SBLR_POP2           = 0xE5,     // Pop 2 values
    SBLR_PUSH_NULL      = 0xE6,     // Push null
    SBLR_PUSH_TRUE      = 0xE7,     // Push true
    SBLR_PUSH_FALSE     = 0xE8,     // Push false
    
    // Jump operations (0xF0-0xF7) - SBLR extensions
    SBLR_JUMP           = 0xF0,     // Unconditional jump
    SBLR_JUMP_IF_TRUE   = 0xF1,     // Jump if true
    SBLR_JUMP_IF_FALSE  = 0xF2,     // Jump if false
    SBLR_JUMP_IF_NULL   = 0xF3,     // Jump if null
    SBLR_JUMP_IF_NOT_NULL = 0xF4,   // Jump if not null
    SBLR_CALL           = 0xF5,     // Call subroutine
    SBLR_RETURN         = 0xF6,     // Return
    SBLR_THROW          = 0xF7,     // Throw exception
    
    // ============================================
    // SBLR Extended Range (0x100+)
    // ============================================
    
    // Extended marker
    SBLR_EXTENDED       = 0x100,    // Extended instruction marker
    
    // Adaptive specializations (0x200-0x2FF)
    SBLR_ADAPTIVE_BASE  = 0x200,
    SBLR_LOAD_FIELD_FAST = 0x200,   // Specialized field load
    SBLR_LOAD_FIELD_DICT = 0x201,   // Dictionary field load
    SBLR_LOAD_FIELD_SLOT = 0x202,   // Slot field load
    SBLR_LOAD_FIELD_CACHED = 0x203, // Cached field load
    
    SBLR_ADD_INT_FAST   = 0x210,    // Fast integer add
    SBLR_ADD_FLOAT_FAST = 0x211,    // Fast float add
    SBLR_ADD_STRING_FAST = 0x212,   // Fast string concat
    SBLR_ADD_DECIMAL_FAST = 0x213,  // Fast decimal add
    
    SBLR_CMP_INT_FAST   = 0x220,    // Fast integer compare
    SBLR_CMP_FLOAT_FAST = 0x221,    // Fast float compare
    SBLR_CMP_STRING_FAST = 0x222,   // Fast string compare
    SBLR_CMP_DATE_FAST  = 0x223,    // Fast date compare
    
    SBLR_CALL_BUILTIN_FAST = 0x230, // Fast builtin call
    SBLR_CALL_CACHED    = 0x231,    // Cached procedure call
    SBLR_CALL_INLINE    = 0x232,    // Inlined call
    
    // Vector operations (0x300-0x3FF)
    SBLR_VECTOR_BASE    = 0x300,
    SBLR_VEC_LOAD       = 0x300,    // Vector load
    SBLR_VEC_STORE      = 0x301,    // Vector store
    SBLR_VEC_ADD        = 0x302,    // Vector add
    SBLR_VEC_SUB        = 0x303,    // Vector subtract
    SBLR_VEC_MUL        = 0x304,    // Vector multiply
    SBLR_VEC_DIV        = 0x305,    // Vector divide
    SBLR_VEC_CMP        = 0x306,    // Vector compare
    SBLR_VEC_FILTER     = 0x307,    // Vector filter
    SBLR_VEC_REDUCE     = 0x308,    // Vector reduce
    SBLR_VEC_GATHER     = 0x309,    // Vector gather
    SBLR_VEC_SCATTER    = 0x30A,    // Vector scatter
    
    // Parallel operations (0x400-0x4FF)
    SBLR_PARALLEL_BASE  = 0x400,
    SBLR_PAR_BEGIN      = 0x400,    // Begin parallel region
    SBLR_PAR_END        = 0x401,    // End parallel region
    SBLR_PAR_FORK       = 0x402,    // Fork execution
    SBLR_PAR_JOIN       = 0x403,    // Join execution
    SBLR_PAR_BARRIER    = 0x404,    // Synchronization barrier
    SBLR_PAR_REDUCE     = 0x405,    // Parallel reduction
    
    // Debug operations (0x500-0x5FF)
    SBLR_DEBUG_BASE     = 0x500,
    SBLR_BREAKPOINT     = 0x500,    // Debugger breakpoint
    SBLR_TRACE          = 0x501,    // Trace execution
    SBLR_ASSERT         = 0x502,    // Assert condition
    SBLR_PROFILE_START  = 0x503,    // Start profiling
    SBLR_PROFILE_END    = 0x504,    // End profiling
    SBLR_LOG            = 0x505,    // Log message
};
```

---

## Type System

### Enhanced Type System (BLR-Compatible)

```c
// SBLR Type System - Superset of BLR types
typedef enum SBLR_Type {
    // ============================================
    // BLR-Compatible Types (0x00-0x7F)
    // ============================================
    
    // Numeric types - BLR compatible
    SBLR_TYPE_SHORT     = 0x07,     // 16-bit integer (blr_short)
    SBLR_TYPE_LONG      = 0x08,     // 32-bit integer (blr_long)
    SBLR_TYPE_QUAD      = 0x09,     // 64-bit integer (blr_quad)
    SBLR_TYPE_FLOAT     = 0x0A,     // 32-bit float (blr_float)
    SBLR_TYPE_D_FLOAT   = 0x0B,     // 64-bit float (blr_d_float)
    SBLR_TYPE_INT64     = 0x10,     // 64-bit integer (blr_int64)
    SBLR_TYPE_DOUBLE    = 0x1B,     // Double precision (blr_double)
    
    // String types - BLR compatible
    SBLR_TYPE_TEXT      = 0x0E,     // Fixed text (blr_text)
    SBLR_TYPE_TEXT2     = 0x0F,     // Text with length (blr_text2)
    SBLR_TYPE_VARYING   = 0x25,     // Varying string (blr_varying)
    SBLR_TYPE_VARYING2  = 0x26,     // Varying with length (blr_varying2)
    SBLR_TYPE_CSTRING   = 0x28,     // C-style string (blr_cstring)
    
    // Date/Time types - BLR compatible
    SBLR_TYPE_SQL_DATE  = 0x0C,     // SQL DATE (blr_sql_date)
    SBLR_TYPE_SQL_TIME  = 0x0D,     // SQL TIME (blr_sql_time)
    SBLR_TYPE_TIMESTAMP = 0x23,     // Timestamp (blr_timestamp)
    
    // BLOB types - BLR compatible
    SBLR_TYPE_BLOB      = 0x105,    // BLOB (blr_blob extended)
    SBLR_TYPE_BLOB2     = 0x106,    // BLOB v2 (blr_blob2 extended)
    
    // Special types - BLR compatible
    SBLR_TYPE_NULL      = 0xB4,     // NULL type (blr_null)
    SBLR_TYPE_BOOLEAN   = 0x17,     // Boolean (SQL standard)
    
    // ============================================
    // SBLR Extended Types (0x80+)
    // ============================================
    
    // Extended numeric types
    SBLR_TYPE_INT8      = 0x80,     // 8-bit integer
    SBLR_TYPE_UINT8     = 0x81,     // Unsigned 8-bit
    SBLR_TYPE_UINT16    = 0x82,     // Unsigned 16-bit
    SBLR_TYPE_UINT32    = 0x83,     // Unsigned 32-bit
    SBLR_TYPE_UINT64    = 0x84,     // Unsigned 64-bit
    SBLR_TYPE_DECIMAL   = 0x85,     // Decimal/Numeric
    SBLR_TYPE_FLOAT16   = 0x86,     // Half precision
    SBLR_TYPE_FLOAT128  = 0x87,     // Quad precision
    
    // Extended string types
    SBLR_TYPE_UTF8      = 0x90,     // UTF-8 string
    SBLR_TYPE_UTF16     = 0x91,     // UTF-16 string
    SBLR_TYPE_UTF32     = 0x92,     // UTF-32 string
    SBLR_TYPE_BINARY    = 0x93,     // Binary data
    SBLR_TYPE_VARBINARY = 0x94,     // Variable binary
    
    // Extended date/time types
    SBLR_TYPE_TIME_TZ   = 0xA0,     // Time with timezone
    SBLR_TYPE_TIMESTAMP_TZ = 0xA1,  // Timestamp with timezone
    SBLR_TYPE_INTERVAL  = 0xA2,     // Interval type
    SBLR_TYPE_DURATION  = 0xA3,     // Duration type
    
    // Complex types
    SBLR_TYPE_ARRAY     = 0xB0,     // Array type
    SBLR_TYPE_RECORD    = 0xB1,     // Record/Row type
    SBLR_TYPE_REF       = 0xB2,     // Reference type
    SBLR_TYPE_CURSOR    = 0xB3,     // Cursor type
    SBLR_TYPE_TABLE     = 0xB4,     // Table type
    SBLR_TYPE_JSON      = 0xB5,     // JSON type
    SBLR_TYPE_XML       = 0xB6,     // XML type
    
    // Special extended types
    SBLR_TYPE_ANY       = 0xFF,     // Any type (polymorphic)
} SBLR_Type;

// Type descriptor (Extended from BLR)
typedef struct SBLR_TypeInfo {
    SBLR_Type       base_type;          // Base type
    uint16_t        size;               // Size in bytes
    uint16_t        precision;          // Precision (decimals)
    uint16_t        scale;              // Scale (decimals)
    uint16_t        charset_id;         // Character set (BLR compat)
    uint16_t        collation_id;       // Collation (BLR compat)
    uint16_t        subtype;            // Subtype (BLR compat)
    bool            nullable;           // Can be NULL
    bool            is_array;           // Array type
    uint32_t        array_dimensions;   // Number of dimensions
    uint32_t*       array_bounds;       // Array bounds
    
    // Extended type information
    union {
        // For numeric types
        struct {
            bool    is_signed;
            bool    is_exact;
            double  min_value;
            double  max_value;
        } numeric;
        
        // For string types
        struct {
            uint32_t    max_length;
            uint32_t    avg_length;
            bool        is_fixed;
            bool        is_unicode;
        } string;
        
        // For complex types
        struct {
            uint32_t        field_count;
            SBLR_FieldInfo* fields;
            uint32_t        size_estimate;
        } record;
        
        // For array types
        struct {
            SBLR_Type   element_type;
            uint32_t    element_size;
            bool        is_fixed_size;
        } array;
    } details;
} SBLR_TypeInfo;
```

---

## Memory Model

### Memory Layout (BLR-Compatible)

```c
// Memory regions
typedef struct SBLR_MemoryLayout {
    // BLR-compatible regions
    struct {
        uint8_t*    message_buffer;     // Message buffer area
        size_t      message_size;       // Message buffer size
        uint8_t*    record_buffer;       // Record buffer area
        size_t      record_size;        // Record buffer size
    } blr_compat;
    
    // Stack segment
    struct {
        SBLR_Value* base;               // Stack base
        SBLR_Value* top;                // Stack top
        SBLR_Value* limit;              // Stack limit
        size_t      size;               // Total size
    } stack;
    
    // Heap segment
    struct {
        uint8_t*    base;               // Heap base
        uint8_t*    current;            // Current allocation point
        uint8_t*    limit;              // Heap limit
        size_t      size;               // Total size
    } heap;
    
    // Constant pool
    struct {
        void*       base;               // Constants base
        size_t      size;               // Total size
        uint32_t    count;              // Number of constants
    } constants;
    
    // Code segment
    struct {
        uint8_t*    base;               // Code base
        size_t      size;               // Code size
        uint32_t    entry;              // Entry point
    } code;
    
    // Global data
    struct {
        SBLR_Value* variables;          // Global variables
        uint32_t    count;              // Variable count
        size_t      size;               // Total size
    } globals;
    
    // Temporary space
    struct {
        uint8_t*    base;               // Temp base
        size_t      size;               // Temp size
        uint32_t    allocated;          // Currently allocated
    } temp;
} SBLR_MemoryLayout;

// BLR Message descriptor
typedef struct SBLR_Message {
    uint16_t        msg_number;         // Message number (BLR compat)
    uint16_t        msg_length;         // Message length
    uint16_t        msg_parameter_count; // Parameter count
    SBLR_Parameter* msg_parameters;     // Parameters
    uint8_t*        msg_buffer;         // Message buffer
} SBLR_Message;

// BLR-compatible parameter
typedef struct SBLR_Parameter {
    SBLR_Type       par_type;           // Parameter type
    uint16_t        par_length;         // Length
    uint16_t        par_scale;          // Scale (for numerics)
    uint16_t        par_sub_type;       // Subtype
    uint16_t        par_charset;        // Character set
    uint16_t        par_offset;         // Offset in message
    bool            par_nullable;       // Can be NULL
    uint16_t        par_null_offset;    // Null indicator offset
} SBLR_Parameter;
```

---

## Code Generation

### Generation from SQL/PSQL (BLR-Compatible)

```c
// Code generator context
typedef struct SBLR_CodeGen {
    // Output buffer
    uint8_t*        code_buffer;        // Generated code
    uint32_t        code_size;          // Buffer size
    uint32_t        code_offset;        // Current offset
    
    // BLR compatibility mode
    bool            blr_compatible;     // Generate BLR-compatible code
    uint8_t         blr_version;        // BLR version (4 or 5)
    
    // Symbol management
    SBLR_SymbolTable* symbols;          // Symbol table
    uint32_t        next_label;         // Next label ID
    uint32_t        next_temp;          // Next temp variable
    
    // Type tracking
    SBLR_TypeStack* type_stack;         // Type inference stack
    
    // Optimization hints
    bool            enable_adaptive;     // Enable adaptive opcodes
    bool            enable_vectorize;   // Enable vectorization
    bool            enable_parallel;    // Enable parallelization
} SBLR_CodeGen;

// Generate code from AST
void SBLR_generate_statement(
    SBLR_CodeGen*   gen,
    AST_Node*       node)
{
    switch (node->type) {
        case AST_SELECT:
            // Generate BLR-compatible RSE
            if (gen->blr_compatible) {
                SBLR_emit_byte(gen, SBLR_RSE);
                SBLR_generate_rse(gen, node);
            } else {
                SBLR_generate_select(gen, node);
            }
            break;
            
        case AST_INSERT:
            SBLR_emit_byte(gen, SBLR_STORE2);
            SBLR_generate_store(gen, node);
            break;
            
        case AST_UPDATE:
            SBLR_emit_byte(gen, SBLR_MODIFY2);
            SBLR_generate_modify(gen, node);
            break;
            
        case AST_DELETE:
            SBLR_emit_byte(gen, SBLR_ERASE2);
            SBLR_generate_erase(gen, node);
            break;
            
        case AST_EXECUTE_BLOCK:
            SBLR_emit_byte(gen, SBLR_BLOCK);
            SBLR_generate_block(gen, node);
            break;
            
        case AST_IF:
            SBLR_generate_if(gen, node);
            break;
            
        case AST_WHILE:
            SBLR_generate_while(gen, node);
            break;
            
        case AST_FOR_SELECT:
            SBLR_emit_byte(gen, SBLR_FOR_SELECT);
            SBLR_generate_for_select(gen, node);
            break;
    }
}

// Generate BLR-compatible RSE (Record Selection Expression)
void SBLR_generate_rse(
    SBLR_CodeGen*   gen,
    AST_Node*       select_node)
{
    // Number of streams
    SBLR_emit_byte(gen, select_node->from_count);
    
    // Generate relation references
    for (int i = 0; i < select_node->from_count; i++) {
        AST_Node* from = select_node->from_list[i];
        
        if (from->type == AST_TABLE) {
            SBLR_emit_byte(gen, SBLR_RELATION2);
            SBLR_emit_string(gen, from->table_name);
            SBLR_emit_string(gen, from->alias);
            SBLR_emit_byte(gen, i);  // Context number
        } else if (from->type == AST_SUBQUERY) {
            // Recursive RSE for subquery
            SBLR_emit_byte(gen, SBLR_RSE);
            SBLR_generate_rse(gen, from);
        }
    }
    
    // Generate boolean (WHERE clause)
    if (select_node->where_clause) {
        SBLR_emit_byte(gen, SBLR_BOOLEAN);
        SBLR_generate_expression(gen, select_node->where_clause);
    }
    
    // Generate sort (ORDER BY)
    if (select_node->order_by) {
        SBLR_emit_byte(gen, SBLR_SORT);
        SBLR_emit_byte(gen, select_node->order_count);
        
        for (int i = 0; i < select_node->order_count; i++) {
            SBLR_generate_expression(gen, select_node->order_list[i]);
            SBLR_emit_byte(gen, select_node->order_desc[i] ? 
                          SBLR_DESCENDING : SBLR_ASCENDING);
        }
    }
    
    // Generate projection
    SBLR_emit_byte(gen, SBLR_PROJECT);
    SBLR_emit_byte(gen, select_node->select_count);
    
    for (int i = 0; i < select_node->select_count; i++) {
        SBLR_generate_expression(gen, select_node->select_list[i]);
    }
    
    // End of RSE
    SBLR_emit_byte(gen, SBLR_END);
}

// Generate expression with type tracking
void SBLR_generate_expression(
    SBLR_CodeGen*   gen,
    AST_Node*       expr)
{
    switch (expr->type) {
        case AST_FIELD:
            // Check for adaptive optimization opportunity
            if (gen->enable_adaptive && is_hot_field(expr)) {
                SBLR_emit_byte(gen, SBLR_LOAD_FIELD_FAST);
                SBLR_emit_word(gen, expr->field_id);
            } else {
                SBLR_emit_byte(gen, SBLR_FIELD2);
                SBLR_emit_byte(gen, expr->context);
                SBLR_emit_word(gen, expr->field_id);
            }
            break;
            
        case AST_LITERAL:
            SBLR_emit_byte(gen, SBLR_LITERAL);
            SBLR_emit_type(gen, expr->literal_type);
            SBLR_emit_value(gen, expr->literal_value);
            break;
            
        case AST_PARAMETER:
            SBLR_emit_byte(gen, SBLR_PARAMETER2);
            SBLR_emit_word(gen, expr->param_index);
            break;
            
        case AST_BINARY_OP:
            // Check for vectorization opportunity
            if (gen->enable_vectorize && can_vectorize(expr)) {
                SBLR_generate_vector_op(gen, expr);
            } else {
                SBLR_generate_expression(gen, expr->left);
                SBLR_generate_expression(gen, expr->right);
                SBLR_emit_byte(gen, get_opcode_for_operator(expr->op));
            }
            break;
            
        case AST_FUNCTION_CALL:
            // Check for builtin fast path
            if (is_builtin_function(expr->function_name)) {
                SBLR_emit_byte(gen, SBLR_CALL_BUILTIN_FAST);
                SBLR_emit_word(gen, get_builtin_id(expr->function_name));
            } else {
                SBLR_emit_byte(gen, SBLR_FUNCTION);
                SBLR_emit_string(gen, expr->function_name);
            }
            
            // Generate arguments
            SBLR_emit_byte(gen, expr->arg_count);
            for (int i = 0; i < expr->arg_count; i++) {
                SBLR_generate_expression(gen, expr->args[i]);
            }
            break;
            
        case AST_AGGREGATE:
            SBLR_emit_byte(gen, SBLR_AGGREGATE);
            SBLR_emit_byte(gen, expr->agg_type);
            SBLR_generate_expression(gen, expr->agg_expr);
            break;
    }
}
```

---

## Optimization Strategies

### Multi-Level Optimization

```c
// Optimization levels
typedef enum SBLR_OptLevel {
    SBLR_OPT_NONE = 0,          // No optimization (BLR compatible)
    SBLR_OPT_BASIC = 1,         // Basic optimizations
    SBLR_OPT_STANDARD = 2,      // Standard optimizations
    SBLR_OPT_AGGRESSIVE = 3,    // Aggressive optimizations
    SBLR_OPT_ADAPTIVE = 4,      // Runtime adaptive optimization
} SBLR_OptLevel;

// Optimization pipeline
void SBLR_optimize(
    SBLR_Module*    module,
    SBLR_OptLevel   level)
{
    if (level == SBLR_OPT_NONE) {
        return;  // Keep BLR-compatible code as-is
    }
    
    // Basic optimizations (Level 1+)
    if (level >= SBLR_OPT_BASIC) {
        SBLR_constant_folding(module);
        SBLR_dead_code_elimination(module);
        SBLR_peephole_optimization(module);
    }
    
    // Standard optimizations (Level 2+)
    if (level >= SBLR_OPT_STANDARD) {
        SBLR_common_subexpression_elimination(module);
        SBLR_loop_invariant_motion(module);
        SBLR_strength_reduction(module);
        SBLR_inline_small_functions(module);
    }
    
    // Aggressive optimizations (Level 3+)
    if (level >= SBLR_OPT_AGGRESSIVE) {
        SBLR_loop_unrolling(module);
        SBLR_vectorization(module);
        SBLR_parallel_detection(module);
        SBLR_speculative_optimization(module);
    }
    
    // Adaptive optimization preparation (Level 4)
    if (level >= SBLR_OPT_ADAPTIVE) {
        SBLR_insert_profile_points(module);
        SBLR_mark_adaptive_candidates(module);
        SBLR_prepare_specialization_stubs(module);
    }
}

// BLR-specific optimization
void SBLR_optimize_blr_patterns(SBLR_Module* module)
{
    // Optimize common BLR patterns
    
    // Pattern: RSE with single relation and simple boolean
    // Can be optimized to direct index access
    for (uint32_t i = 0; i < module->code_length; i++) {
        if (module->code[i] == SBLR_RSE) {
            if (is_simple_rse_pattern(&module->code[i])) {
                optimize_to_index_scan(&module->code[i]);
            }
        }
    }
    
    // Pattern: FOR SELECT with STORE inside
    // Can be optimized to bulk insert
    for (uint32_t i = 0; i < module->code_length; i++) {
        if (module->code[i] == SBLR_FOR_SELECT) {
            if (has_simple_store_pattern(&module->code[i])) {
                optimize_to_bulk_operation(&module->code[i]);
            }
        }
    }
    
    // Pattern: Multiple field loads from same record
    // Can be optimized to record load + field extraction
    optimize_field_access_patterns(module);
}
```

---

## Adaptive Execution

### Runtime Specialization System

```c
// Adaptive execution context
typedef struct SBLR_AdaptiveContext {
    // Profile data
    struct {
        uint64_t*   instruction_counts;     // Execution counts
        uint64_t*   type_signatures;        // Type patterns
        double*     branch_probabilities;   // Branch prediction
        uint64_t*   cache_misses;          // Cache miss counts
    } profile;
    
    // Specialization cache
    struct {
        SBLR_SpecializedCode* entries;      // Specialized versions
        uint32_t    count;                  // Number of specializations
        uint32_t    capacity;               // Cache capacity
        uint32_t    hits;                   // Cache hits
        uint32_t    misses;                 // Cache misses
    } cache;
    
    // JIT compilation queue
    struct {
        SBLR_HotPath* paths;                // Hot paths to compile
        uint32_t    count;                  // Queue size
        bool        compiling;              // Compilation in progress
    } jit_queue;
    
    // Thresholds
    struct {
        uint32_t    specialization_threshold;   // When to specialize
        uint32_t    jit_threshold;             // When to JIT compile
        uint32_t    deoptimization_threshold;  // When to deoptimize
    } thresholds;
} SBLR_AdaptiveContext;

// Specialization for hot code
void SBLR_specialize_hot_code(
    SBLR_ExecutionContext* ctx,
    uint32_t               ip)
{
    SBLR_AdaptiveContext* adaptive = ctx->adaptive;
    uint8_t opcode = ctx->module->code[ip];
    
    // Check execution count
    if (adaptive->profile.instruction_counts[ip] < 
        adaptive->thresholds.specialization_threshold) {
        return;
    }
    
    // Get type signature
    uint64_t type_sig = adaptive->profile.type_signatures[ip];
    
    // Check for existing specialization
    SBLR_SpecializedCode* spec = find_specialization(
        &adaptive->cache, ip, type_sig);
    
    if (spec) {
        // Use existing specialization
        adaptive->cache.hits++;
        ctx->module->code[ip] = spec->specialized_opcode;
        return;
    }
    
    // Create new specialization
    adaptive->cache.misses++;
    
    switch (opcode) {
        case SBLR_FIELD:
        case SBLR_FIELD2:
            specialize_field_access(ctx, ip, type_sig);
            break;
            
        case SBLR_ADD:
        case SBLR_SUBTRACT:
        case SBLR_MULTIPLY:
        case SBLR_DIVIDE:
            specialize_arithmetic(ctx, ip, type_sig);
            break;
            
        case SBLR_EQL:
        case SBLR_NEQ:
        case SBLR_GTR:
        case SBLR_LSS:
            specialize_comparison(ctx, ip, type_sig);
            break;
            
        case SBLR_FOR_SELECT:
            specialize_loop(ctx, ip, type_sig);
            break;
            
        case SBLR_FUNCTION:
            specialize_function_call(ctx, ip, type_sig);
            break;
    }
}

// Deoptimization when specialization fails
void SBLR_deoptimize(
    SBLR_ExecutionContext* ctx,
    uint32_t               ip)
{
    // Restore original opcode
    uint8_t original = get_original_opcode(ctx->module, ip);
    ctx->module->code[ip] = original;
    
    // Clear specialization cache entry
    clear_specialization(&ctx->adaptive->cache, ip);
    
    // Update profile to prevent re-specialization
    ctx->adaptive->profile.type_signatures[ip] = TYPE_SIG_POLYMORPHIC;
}
```

---

## JIT Compilation

### JIT Framework

```c
// JIT compilation context
typedef struct SBLR_JITContext {
    // LLVM integration
    void*           llvm_context;       // LLVM context
    void*           llvm_module;        // LLVM module
    void*           llvm_engine;        // Execution engine
    
    // Compiled code cache
    struct {
        SBLR_NativeCode* entries;       // Native code entries
        uint32_t        count;          // Number of entries
        size_t          total_size;     // Total code size
    } code_cache;
    
    // Compilation statistics
    struct {
        uint64_t        compilations;   // Total compilations
        uint64_t        compile_time;   // Total compile time
        uint64_t        code_size;      // Generated code size
        double          speedup;        // Average speedup
    } stats;
} SBLR_JITContext;

// JIT compile hot path
SBLR_NativeCode* SBLR_jit_compile(
    SBLR_JITContext*    jit,
    SBLR_Module*        module,
    uint32_t            start_ip,
    uint32_t            end_ip)
{
    // Check if already compiled
    SBLR_NativeCode* existing = find_compiled_code(
        &jit->code_cache, module, start_ip, end_ip);
    
    if (existing) {
        return existing;
    }
    
    // Generate LLVM IR
    void* llvm_function = generate_llvm_ir(
        jit, module, start_ip, end_ip);
    
    // Optimize LLVM IR
    optimize_llvm_ir(jit, llvm_function);
    
    // Compile to native code
    void* native_code = compile_llvm_to_native(
        jit->llvm_engine, llvm_function);
    
    // Create native code entry
    SBLR_NativeCode* entry = allocate_native_code();
    entry->start_ip = start_ip;
    entry->end_ip = end_ip;
    entry->native_ptr = native_code;
    entry->size = get_function_size(native_code);
    
    // Add to cache
    add_to_code_cache(&jit->code_cache, entry);
    
    // Update statistics
    jit->stats.compilations++;
    jit->stats.code_size += entry->size;
    
    return entry;
}
```

---

## Runtime Execution

### Virtual Machine Implementation

```c
// SBLR Virtual Machine
typedef struct SBLR_VM {
    // Execution contexts (per thread)
    SBLR_ExecutionContext** contexts;
    uint32_t                context_count;
    
    // Module management
    SBLR_ModuleCache*       module_cache;
    
    // Memory management
    SBLR_MemoryManager*     memory;
    
    // Adaptive optimization
    SBLR_AdaptiveContext*   adaptive;
    
    // JIT compilation
    SBLR_JITContext*        jit;
    
    // BLR compatibility layer
    BLR_Compatibility*      blr_compat;
    
    // Statistics
    SBLR_Statistics*        stats;
} SBLR_VM;

// Main execution loop
SBLR_Value SBLR_execute(
    SBLR_VM*        vm,
    SBLR_Module*    module,
    SBLR_Value*     args,
    uint32_t        arg_count)
{
    // Get or create execution context
    SBLR_ExecutionContext* ctx = get_thread_context(vm);
    
    // Initialize context
    init_execution_context(ctx, module, args, arg_count);
    
    // Check for BLR module
    if (module->header.blr_compat_version > 0) {
        return execute_blr_compatible(vm->blr_compat, module, ctx);
    }
    
    // Main execution loop
    while (ctx->ip < module->code_length) {
        // Check for native code
        SBLR_NativeCode* native = find_native_code(
            vm->jit, module, ctx->ip);
        
        if (native) {
            // Execute native code
            SBLR_Value result = execute_native(native, ctx);
            ctx->ip = native->end_ip;
            continue;
        }
        
        // Check for adaptive specialization
        if (should_specialize(vm->adaptive, ctx->ip)) {
            SBLR_specialize_hot_code(ctx, ctx->ip);
        }
        
        // Fetch instruction
        uint8_t opcode = module->code[ctx->ip];
        
        // Dispatch instruction
        switch (opcode) {
            // BLR-compatible opcodes
            case SBLR_BEGIN:
                execute_begin(ctx);
                break;
                
            case SBLR_FIELD:
            case SBLR_FIELD2:
                execute_field(ctx);
                break;
                
            case SBLR_LITERAL:
                execute_literal(ctx);
                break;
                
            case SBLR_ADD:
                execute_add(ctx);
                break;
                
            case SBLR_RSE:
                execute_rse(ctx);
                break;
                
            case SBLR_FOR_SELECT:
                execute_for_select(ctx);
                break;
                
            // SBLR extended opcodes
            case SBLR_LOAD_FIELD_FAST:
                execute_field_fast(ctx);
                break;
                
            case SBLR_VEC_ADD:
                execute_vector_add(ctx);
                break;
                
            case SBLR_PAR_BEGIN:
                execute_parallel_begin(ctx);
                break;
                
            default:
                if (opcode >= SBLR_EXTENDED) {
                    execute_extended(ctx, opcode);
                } else {
                    error("Unknown opcode: %02X", opcode);
                }
                break;
        }
        
        ctx->ip++;
    }
    
    // Return top of stack
    return pop(ctx);
}

// BLR compatibility layer
SBLR_Value execute_blr_compatible(
    BLR_Compatibility*      blr,
    SBLR_Module*           module,
    SBLR_ExecutionContext* ctx)
{
    // Translate BLR to SBLR if needed
    if (!module->blr_translated) {
        translate_blr_to_sblr(blr, module);
        module->blr_translated = true;
    }
    
    // Execute with BLR semantics
    ctx->blr_mode = true;
    return SBLR_execute(ctx->vm, module, ctx->args, ctx->arg_count);
}
```

---

## Debugging and Tools

### Debugging Support

```c
// Debug information
typedef struct SBLR_DebugInfo {
    // Source mapping
    struct {
        uint32_t*   instruction_offsets;    // Instruction to source
        uint32_t*   line_numbers;          // Line numbers
        char**      source_files;          // Source file names
    } source_map;
    
    // Symbol information
    struct {
        SBLR_Symbol* symbols;              // Symbol table
        uint32_t    count;                 // Symbol count
    } symbols;
    
    // Breakpoints
    struct {
        uint32_t*   locations;             // Breakpoint locations
        uint32_t    count;                 // Breakpoint count
    } breakpoints;
} SBLR_DebugInfo;

// Disassembler
void SBLR_disassemble(
    SBLR_Module*    module,
    FILE*           output)
{
    fprintf(output, "; SBLR Module Disassembly\n");
    fprintf(output, "; Version: %d.%d\n", 
           module->header.version_major,
           module->header.version_minor);
    
    if (module->header.blr_compat_version) {
        fprintf(output, "; BLR Compatible: v%d\n",
               module->header.blr_compat_version);
    }
    
    uint32_t ip = 0;
    while (ip < module->code_length) {
        uint8_t opcode = module->code[ip];
        
        // Print offset and opcode
        fprintf(output, "%08X: %02X ", ip, opcode);
        
        // Decode instruction
        const char* mnemonic = get_mnemonic(opcode);
        fprintf(output, "%-16s", mnemonic);
        
        // Decode operands
        ip++;
        ip += decode_operands(module, ip, output);
        
        // Add debug info if available
        if (module->debug_info) {
            uint32_t line = get_source_line(module->debug_info, ip);
            if (line > 0) {
                fprintf(output, " ; line %d", line);
            }
        }
        
        fprintf(output, "\n");
    }
}

// Bytecode verifier
bool SBLR_verify(SBLR_Module* module)
{
    // Verify header
    if (module->header.magic != 0x53424C52) {
        return false;
    }
    
    // Verify checksum
    uint32_t checksum = calculate_checksum(module);
    if (checksum != module->header.checksum) {
        return false;
    }
    
    // Verify code
    if (!verify_code_integrity(module)) {
        return false;
    }
    
    // Verify type safety
    if (!verify_type_safety(module)) {
        return false;
    }
    
    // Verify stack balance
    if (!verify_stack_balance(module)) {
        return false;
    }
    
    // Verify BLR compatibility if claimed
    if (module->header.blr_compat_version) {
        if (!verify_blr_compatibility(module)) {
            return false;
        }
    }
    
    return true;
}
```

---

## Migration from BLR

### BLR to SBLR Translation

```c
// BLR to SBLR translator
typedef struct BLR_Translator {
    const uint8_t*  blr_code;           // BLR bytecode
    uint32_t        blr_length;         // BLR length
    uint8_t         blr_version;        // BLR version
    
    uint8_t*        sblr_code;          // Output SBLR code
    uint32_t        sblr_length;        // Output length
    uint32_t        sblr_capacity;      // Buffer capacity
    
    // Translation state
    uint32_t        context_count;      // Number of contexts
    uint32_t        message_count;      // Number of messages
    bool            in_rse;             // Inside RSE
} BLR_Translator;

// Translate BLR to SBLR
SBLR_Module* translate_blr_to_sblr(
    const uint8_t*  blr_code,
    uint32_t        blr_length)
{
    BLR_Translator trans;
    memset(&trans, 0, sizeof(trans));
    
    trans.blr_code = blr_code;
    trans.blr_length = blr_length;
    
    // Check BLR version
    trans.blr_version = blr_code[0];
    if (trans.blr_version != blr_version4 && 
        trans.blr_version != blr_version5) {
        error("Unsupported BLR version: %d", trans.blr_version);
        return NULL;
    }
    
    // Allocate output buffer
    trans.sblr_capacity = blr_length * 2;  // Conservative estimate
    trans.sblr_code = malloc(trans.sblr_capacity);
    
    // Write SBLR header
    SBLR_Module* module = create_module();
    module->header.blr_compat_version = trans.blr_version;
    
    // Translate instructions
    uint32_t blr_ip = 1;  // Skip version byte
    
    while (blr_ip < blr_length) {
        uint8_t opcode = blr_code[blr_ip++];
        
        // Direct mapping for compatible opcodes
        if (is_compatible_opcode(opcode)) {
            emit_sblr_byte(&trans, opcode);
            blr_ip += copy_operands(&trans, opcode, blr_ip);
        } else {
            // Translate incompatible opcodes
            blr_ip += translate_opcode(&trans, opcode, blr_ip);
        }
    }
    
    // Copy translated code to module
    module->code = trans.sblr_code;
    module->code_length = trans.sblr_length;
    
    // Mark as BLR-translated
    module->header.flags |= SBLR_FLAG_BLR_TRANSLATED;
    
    return module;
}

// Compatibility checker
bool is_blr_compatible_module(SBLR_Module* module)
{
    // Check if module uses only BLR-compatible opcodes
    for (uint32_t i = 0; i < module->code_length; i++) {
        uint8_t opcode = module->code[i];
        
        if (opcode >= SBLR_EXTENDED) {
            return false;  // Uses extended opcodes
        }
        
        if (!is_blr_opcode(opcode)) {
            return false;  // Uses SBLR-specific opcode
        }
    }
    
    return true;
}

// Generate BLR from SBLR (for compatibility)
uint8_t* generate_blr_from_sblr(
    SBLR_Module*    module,
    uint32_t*       blr_length)
{
    if (!is_blr_compatible_module(module)) {
        error("Module uses SBLR-specific features");
        return NULL;
    }
    
    // Allocate BLR buffer
    uint8_t* blr = malloc(module->code_length + 1);
    
    // Write BLR version
    blr[0] = blr_version5;
    
    // Copy compatible bytecode
    memcpy(blr + 1, module->code, module->code_length);
    
    *blr_length = module->code_length + 1;
    
    return blr;
}
```

---

## Implementation Guidelines

### Implementation Phases

```c
// Phase 1: Core BLR-compatible implementation
typedef struct Phase1_Implementation {
    // Basic VM with BLR opcodes
    SBLR_VM*        basic_vm;
    
    // BLR code generator
    BLR_CodeGen*    blr_codegen;
    
    // Simple executor
    BLR_Executor*   blr_executor;
} Phase1_Implementation;

// Phase 2: SBLR extensions
typedef struct Phase2_Implementation {
    // Extended opcodes
    SBLR_ExtendedOps* extended_ops;
    
    // Stack operations
    SBLR_StackOps*  stack_ops;
    
    // Enhanced types
    SBLR_TypeSystem* type_system;
} Phase2_Implementation;

// Phase 3: Optimization
typedef struct Phase3_Implementation {
    // Optimization passes
    SBLR_Optimizer* optimizer;
    
    // Adaptive system
    SBLR_AdaptiveSystem* adaptive;
    
    // Profiling
    SBLR_Profiler*  profiler;
} Phase3_Implementation;

// Phase 4: JIT compilation
typedef struct Phase4_Implementation {
    // JIT compiler
    SBLR_JIT*       jit_compiler;
    
    // Native code cache
    SBLR_CodeCache* code_cache;
    
    // Runtime linker
    SBLR_Linker*    linker;
} Phase4_Implementation;

// Phase 5: Advanced features
typedef struct Phase5_Implementation {
    // Vectorization
    SBLR_Vectorizer* vectorizer;
    
    // Parallelization
    SBLR_Parallelizer* parallelizer;
    
    // Distributed execution
    SBLR_Distributed* distributed;
} Phase5_Implementation;
```

### Testing Strategy

```c
// Test framework
typedef struct SBLR_TestFramework {
    // BLR compatibility tests
    struct {
        TestCase*   firebird_tests;     // Firebird BLR tests
        TestCase*   regression_tests;   // Regression suite
        uint32_t    test_count;
    } compatibility;
    
    // Performance tests
    struct {
        Benchmark*  micro_benchmarks;   // Micro benchmarks
        Benchmark*  macro_benchmarks;   // Macro benchmarks
        Benchmark*  real_world;        // Real-world queries
    } performance;
    
    // Correctness tests
    struct {
        TestCase*   type_tests;        // Type system tests
        TestCase*   semantic_tests;    // Semantic tests
        TestCase*   edge_cases;        // Edge cases
    } correctness;
} SBLR_TestFramework;
```

---

## Appendix: Example Bytecode

### Example 1: Simple SELECT (BLR-Compatible)

SQL:
```sql
SELECT id, name FROM users WHERE age > 18
```

BLR-Compatible SBLR:
```
00: 05              ; SBLR_VERSION (BLR v5 compatible)
01: 51              ; SBLR_RSE (Record Selection Expression)
02: 01              ; 1 stream
03: 59              ; SBLR_RELATION2
04: 05 75 73 65 72 73  ; "users"
09: 00              ; No alias
0A: 00              ; Context 0
0B: 55              ; SBLR_BOOLEAN
0C: 79              ; SBLR_FIELD2
0D: 00              ; Context 0
0E: 02 00           ; Field ID 2 (age)
10: 7D              ; SBLR_LITERAL
11: 08              ; Type: LONG
12: 12 00 00 00     ; Value: 18
16: 31              ; SBLR_GTR
17: 53              ; SBLR_PROJECT
18: 02              ; 2 fields
19: 79              ; SBLR_FIELD2
1A: 00              ; Context 0
1B: 00 00           ; Field ID 0 (id)
1D: 79              ; SBLR_FIELD2
1E: 00              ; Context 0
1F: 01 00           ; Field ID 1 (name)
21: FF              ; SBLR_END
```

Optimized SBLR (with extensions):
```
00: 05              ; SBLR_VERSION
01: A2              ; SBLR_OPEN_CURSOR
02: 00 00           ; Cursor 0
04: 00              ; Table: users

05: F3              ; SBLR_JUMP_IF_NULL
06: 2A 00           ; Jump to end (offset 0x2A)

08: 00 02           ; SBLR_LOAD_FIELD_FAST
0A: 02 00           ; Field 2 (age)
0C: E6              ; SBLR_PUSH_INT8
0D: 12              ; Value: 18
0E: 20              ; SBLR_CMP_INT_FAST
0F: 31              ; GT comparison
10: F2              ; SBLR_JUMP_IF_FALSE
11: 05 00           ; Jump to next (relative +5)

13: 00 02           ; SBLR_LOAD_FIELD_FAST
15: 00 00           ; Field 0 (id)
17: 00 02           ; SBLR_LOAD_FIELD_FAST
19: 01 00           ; Field 1 (name)
1B: F6              ; SBLR_RETURN
1C: 02              ; Return 2 values

1D: 4C              ; SBLR_FETCH
1E: 00              ; Cursor 0
1F: F0              ; SBLR_JUMP
20: E7 FF           ; Jump back to loop start

22: A3              ; SBLR_CLOSE_CURSOR
23: 00              ; Cursor 0
24: E6              ; SBLR_PUSH_NULL
25: F6              ; SBLR_RETURN
26: 01              ; Return 1 value
```

### Example 2: Stored Procedure with Adaptive Optimization

PSQL:
```sql
CREATE PROCEDURE sum_orders(customer_id INTEGER)
RETURNS (total DECIMAL(15,2))
AS
BEGIN
    SELECT SUM(amount) FROM orders WHERE customer_id = :customer_id
    INTO :total;
END
```

SBLR with Adaptive Hints:
```
00: 05              ; SBLR_VERSION
01: 8B              ; SBLR_BLOCK
02: 94              ; SBLR_DCL_VARIABLE
03: 00              ; Variable 0 (total)
04: 85              ; Type: DECIMAL
05: 0F 02           ; Precision: 15, Scale: 2

07: 51              ; SBLR_RSE
08: 01              ; 1 stream
09: 59              ; SBLR_RELATION2
0A: 06 6F 72 64 65 72 73  ; "orders"
10: 00              ; No alias
11: 00              ; Context 0

12: 55              ; SBLR_BOOLEAN
13: 79              ; SBLR_FIELD2
14: 00              ; Context 0
15: 01 00           ; Field 1 (customer_id)
17: 7A              ; SBLR_PARAMETER
18: 00 00           ; Parameter 0
1A: 2F              ; SBLR_EQL

1B: 53              ; SBLR_PROJECT
1C: 01              ; 1 field
1D: 6C              ; SBLR_TOTAL (SUM)
1E: 79              ; SBLR_FIELD2
1F: 00              ; Context 0
20: 02 00           ; Field 2 (amount)

22: 7C              ; SBLR_VARIABLE
23: 00              ; Variable 0 (total)
24: 01              ; SBLR_ASSIGNMENT

25: FF              ; SBLR_END
```

With Adaptive Specialization After Profiling:
```
00: 05              ; SBLR_VERSION
01: 8B              ; SBLR_BLOCK
02: 94              ; SBLR_DCL_VARIABLE
03: 00              ; Variable 0 (total)
04: 85              ; Type: DECIMAL

; Specialized for indexed customer_id lookup
06: 31 02           ; SBLR_CALL_CACHED
08: 00 00           ; Cached plan ID 0
0A: 7A              ; SBLR_PARAMETER
0B: 00 00           ; Parameter 0

; Fast decimal accumulation (vectorized)
0D: 08 03           ; SBLR_VEC_REDUCE
0F: 02              ; Operation: SUM
10: 85              ; Type: DECIMAL
11: 00              ; Target: Variable 0

12: FF              ; SBLR_END
```

---

## Version History

- **1.0** - Initial SBLR specification
- **1.1** - Unified BLR/SBLR specification with full compatibility

## Future Enhancements

- GPU acceleration support
- Distributed execution across nodes
- Machine learning-based optimization
- Quantum computing integration (future)

---

## References

1. FirebirdSQL BLR Documentation
2. FirebirdSQL Source Code (src/jrd/blr.h)
3. PostgreSQL Executor Documentation
4. Python 3.11 Adaptive Bytecode (PEP 659)
5. LLVM JIT Compilation Framework
6. Java HotSpot VM Specification
7. V8 JavaScript Engine Optimization Techniques