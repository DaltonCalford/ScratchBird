# **Appendix A: SBLR Bytecode Specification**

## **1\. Introduction and Overview**

This document provides the complete, low-level technical specification for the **ScratchBird Bytecode Language Representation (SBLR)**. SBLR is the intermediate language that the ScratchBird engine compiles all PSQL (procedural SQL) code into. This bytecode is then executed by a high-performance, stack-based virtual machine.

This specification is intended for developers working on the database engine's core, those creating development tools like debuggers, or anyone seeking a deep understanding of the PSQL execution model. It is a technical appendix and not part of the high-level SQL dialect definition.

SBLR is an evolution of Firebird's Binary Language Representation (BLR), extending its proven, database-centric design with modern features like adaptive optimization, vectorization, and a framework for Just-in-Time (JIT) compilation.

### **Key Enhancements over Traditional BLR**

1. **Adaptive Specialization**: Runtime type profiling and instruction specialization for hot code paths.  
2. **JIT Compilation Support**: A framework for compiling frequently executed bytecode into native machine code.  
3. **Vectorization**: SIMD (Single Instruction, Multiple Data) operations for high-performance batch processing.  
4. **Extended Type System**: Richer type information embedded in the bytecode to aid optimization.  
5. **Rich Debug Information**: Source mapping and profiling data for advanced debugging.  
6. **Backward Compatibility**: The ability to execute legacy Firebird BLR through a translation layer.

## **2\. Historical Context: BLR Heritage**

SBLR is built upon the foundation of FirebirdSQL's Binary Language Representation (BLR), a compact, platform-independent bytecode designed for database operations. SBLR preserves the core stack-based execution model and database-centric opcodes of BLR while extending its capabilities.

### **Original BLR Opcode Categories (Preserved in SBLR)**

The classic BLR instruction set is preserved for backward compatibility. The following enum represents the core opcodes from Firebird's blr.h that SBLR extends.

// Classic BLR version markers  
\#define blr\_version4    4  
\#define blr\_version5    5

// Core BLR instruction ranges preserved in SBLR  
enum blr\_legacy\_opcodes {  
    // Control  
    blr\_begin \= 2, blr\_end \= 255, blr\_message \= 4, blr\_eoc \= 76, blr\_assignment \= 1,  
      
    // Data types (Enhanced in SBLR)  
    blr\_text \= 14, blr\_text2 \= 15, blr\_short \= 7, blr\_long \= 8, blr\_int64 \= 16,  
    blr\_float \= 10, blr\_double \= 27, blr\_timestamp \= 35, blr\_varying \= 37,  
    blr\_varying2 \= 38, blr\_blob \= 261, blr\_blob2 \= 262, blr\_quad \= 9,  
    blr\_d\_float \= 11, blr\_sql\_date \= 12, blr\_sql\_time \= 13,  
      
    // Expressions (Extended in SBLR)  
    blr\_add \= 34, blr\_subtract \= 35, blr\_multiply \= 36, blr\_divide \= 37,  
    blr\_negate \= 38, blr\_concatenate \= 39, blr\_substring \= 40, blr\_trim \= 41,  
    blr\_cast \= 124, blr\_upcase \= 42, blr\_lowcase \= 43,  
      
    // Comparisons  
    blr\_eql \= 47, blr\_neq \= 48, blr\_gtr \= 49, blr\_geq \= 50, blr\_lss \= 51, blr\_leq \= 52,  
    blr\_between \= 53, blr\_like \= 54, blr\_contains \= 55, blr\_matching \= 56,  
    blr\_starting \= 57, blr\_similar \= 58,  
      
    // Boolean operations  
    blr\_and \= 58, blr\_or \= 59, blr\_not \= 60, blr\_any \= 61, blr\_unique \= 62, blr\_all \= 63,  
      
    // Control flow (Enhanced in SBLR)  
    blr\_if \= 60, blr\_loop \= 61, blr\_for \= 62, blr\_while \= 63, blr\_leave \= 64,  
    blr\_continue \= 65, blr\_do\_while \= 66, blr\_break \= 67,  
      
    // Database operations  
    blr\_store \= 70, blr\_store2 \= 71, blr\_modify \= 72, blr\_modify2 \= 73,  
    blr\_erase \= 74, blr\_erase2 \= 75, blr\_fetch \= 76, blr\_for\_select \= 77,  
    blr\_send \= 78, blr\_receive \= 79, blr\_select \= 80, blr\_rse \= 81,  
    blr\_first \= 82, blr\_project \= 83, blr\_sort \= 84, blr\_boolean \= 85,  
    blr\_ascending \= 86, blr\_descending \= 87, blr\_relation \= 88, blr\_relation2 \= 89,  
    blr\_rid \= 90, blr\_union \= 91, blr\_map \= 92, blr\_group\_by \= 93,  
    blr\_aggregate \= 94, blr\_join\_type \= 95,  
      
    // Functions (Extended in SBLR)  
    blr\_function \= 100, blr\_gen\_id \= 101, blr\_gen\_id2 \= 102, blr\_count \= 103,  
    blr\_count2 \= 104, blr\_max \= 105, blr\_min \= 106, blr\_average \= 107,  
    blr\_total \= 108, blr\_from \= 109, blr\_via \= 110, blr\_user\_name \= 111,  
    blr\_current\_date \= 112, blr\_current\_time \= 113, blr\_current\_timestamp \= 114,  
    blr\_current\_role \= 115,  
      
    // Field/Variable access  
    blr\_field \= 120, blr\_field2 \= 121, blr\_parameter \= 122, blr\_parameter2 \= 123,  
    blr\_variable \= 124, blr\_literal \= 125, blr\_dbkey \= 126, blr\_index \= 127,  
      
    // Procedures/Triggers (Enhanced in SBLR)  
    blr\_procedure \= 130, blr\_procedure2 \= 131, blr\_trigger \= 132,  
    blr\_exec\_proc \= 133, blr\_exec\_proc2 \= 134, blr\_exec\_stmt \= 135,  
    blr\_exec\_into \= 136, blr\_exec\_sql \= 137, blr\_internal\_info \= 138,  
    blr\_block \= 139,  
      
    // Handlers and exceptions  
    blr\_error\_handler \= 140, blr\_start\_savepoint \= 141, blr\_end\_savepoint \= 142,  
    blr\_handler \= 143, blr\_post \= 144, blr\_post\_arg \= 145, blr\_put\_slice \= 146,  
    blr\_get\_slice \= 147, blr\_dcl\_variable \= 148,  
      
    // Plan and optimization hints  
    blr\_plan \= 150, blr\_merge \= 151, blr\_join \= 152, blr\_sequential \= 153,  
    blr\_navigational \= 154, blr\_indices \= 155, blr\_retrieve \= 156,  
      
    // Cursor operations  
    blr\_declare\_cursor \= 160, blr\_cursor\_stmt \= 161, blr\_open\_cursor \= 162,  
    blr\_close\_cursor \= 163, blr\_fetch\_cursor \= 164, blr\_cursor\_name \= 165,  
      
    // Null handling  
    blr\_null \= 180, blr\_missing \= 183  
};

## **3\. SBLR Bytecode Structure**

An SBLR module is a binary structure composed of a header followed by several data sections.

### **3.0. Compact Stream Format (Engine Default)**

ScratchBird currently uses a compact stream format for most statements:

```
[VERSION opcode][sblr_version byte]...[bytecode]...[END opcode]
```

The `sblr_version` byte is authoritative for opcode decoding in the executor.
As of this update, the active version is **2** and earlier versions are rejected.

### **3.1. SBLR File Header**

The header contains metadata about the bytecode, including version, section sizes, and execution requirements.

typedef struct SBLR\_Header {  
    // Magic number and versioning  
    uint32\_t    magic;              // 0x53424C52 ('SBLR')  
    uint16\_t    version\_major;      // 2  
    uint16\_t    version\_minor;      // 0  
    uint8\_t     blr\_compat\_version; // BLR compatibility level (e.g., 5 for Firebird v5)  
    uint8\_t     reserved\_flags;     // Alignment and future flags  
      
    // Metadata  
    uint32\_t    flags;              // Compilation flags (e.g., adaptive enabled)  
    uint32\_t    checksum;           // CRC32 of the entire SBLR module  
    uint64\_t    timestamp;          // Compilation timestamp (UTC)  
    uint32\_t    source\_hash;        // Hash of the original PSQL source text  
      
    // Section Offsets (from start of header)  
    uint32\_t    code\_offset;        // Offset to the code section  
    uint32\_t    const\_offset;       // Offset to the constants pool  
    uint32\_t    type\_offset;        // Offset to the type information section  
    uint32\_t    debug\_offset;       // Offset to the debug info section  
    uint32\_t    profile\_offset;     // Offset to the profile data section  
      
    // Section Sizes  
    uint32\_t    code\_size;          // Size of the code section in bytes  
    uint32\_t    const\_size;         // Size of the constants pool  
    uint32\_t    type\_size;          // Size of the type information  
    uint32\_t    debug\_size;         // Size of debug info  
    uint32\_t    profile\_size;       // Size of profile data  
      
    // Execution requirements  
    uint32\_t    stack\_size\_req;     // Required stack size for execution  
    uint32\_t    variable\_slots;     // Number of local variable slots needed  
      
    // Entry points (offsets within the code section)  
    uint32\_t    main\_entry\_point;   // Main execution entry  
    uint32\_t    init\_entry\_point;   // Initialization entry (optional)  
    uint32\_t    cleanup\_entry\_point;// Cleanup entry (optional)  
      
    // Reserved for future use  
    uint32\_t    reserved\[8\];  
} SBLR\_Header;

### **3.2. SBLR Module Sections**

The full SBLR module is composed of the header and the following data sections:

typedef struct SBLR\_Module {  
    SBLR\_Header     header;  
      
    // Code section: The executable bytecode instructions  
    uint8\_t\* code;  
      
    // Constants pool: Literal values (strings, numbers) referenced by the code  
    SBLR\_Constant\* constants;  
      
    // Type information: Detailed descriptors for all data types used  
    SBLR\_TypeInfo\* type\_info;  
      
    // Symbol table: Names of variables, parameters, procedures, etc.  
    SBLR\_Symbol\* symbols;  
      
    // Debug information (Optional): Source line mapping, breakpoints  
    SBLR\_DebugInfo\* debug\_info;  
      
    // Profile data (Optional): Runtime profiling data for adaptive optimization  
    SBLR\_ProfileData\* profile\_data;  
      
    // Exception table: Defines exception handlers and their code ranges  
    SBLR\_Exception\* exceptions;  
} SBLR\_Module;

## **4\. Complete Instruction Set (Opcodes)**

Base SBLR opcodes are byte-sized. The range 0x00-0xFF is reserved for BLR-compatible instructions.
Extended SBLR-specific instructions use a prefixing system:
- **Version 2+**: `0xFF` followed by a **16-bit little-endian** extended opcode.
- **Version 1**: `0xFF` followed by an 8-bit extended opcode (deprecated, not accepted by the engine).

### **4.1. Extended Opcode Encoding (v2)**

**Encoding rule (v2)**:

```
0xFF <ext_opcode_lo> <ext_opcode_hi> [payload...]
```

All existing extended opcodes are re-encoded as 16-bit values using their original 8-bit numeric IDs (e.g., `0x0028` instead of `0x28`).
New extended opcodes may use values above `0x00FF`.

**New extended opcodes (v2)**:

- `EXT_RENAME_OBJECT = 0x0100`
- `EXT_MOVE_OBJECT   = 0x0101`
- `EXT_SET_AUTOCOMMIT = 0x0102`
- `EXT_COMMIT_RETAINING = 0x0103` (deprecated alias; prefer COMMIT with RETAINING flag)
- `EXT_ROLLBACK_RETAINING = 0x0104` (deprecated alias; prefer ROLLBACK with RETAINING flag)
- `EXT_PREPARE_TRANSACTION = 0x0105`
- `EXT_COMMIT_PREPARED = 0x0106`
- `EXT_ROLLBACK_PREPARED = 0x0107`

### **4.2. Transaction Opcodes (SBLR v2)**

ScratchBird is **always in a transaction**. Transaction opcodes therefore **always** end by starting a new transaction unless an explicit conflict action says otherwise.

#### **START_TRANSACTION (0x13)**
```
[START_TRANSACTION]
[flags:uint16]
[conflict_action:uint8]               // 0=DEFAULT,1=COMMIT,2=ROLLBACK,3=ERROR,4=KEEP
[conflict_error_code:int32]           // only if flags has CONFLICT_ERROR_CODE
[autocommit_mode:uint8]               // 0=UNCHANGED,1=ON,2=OFF (only if flags has AUTOCOMMIT)
[isolation_level:uint8]               // only if flags has ISOLATION
[access_mode:uint8]                   // only if flags has ACCESS_MODE (0=RW,1=RO)
[deferrable:uint8]                    // only if flags has DEFERRABLE (0=NOT,1=YES)
[wait_mode:uint8]                     // only if flags has WAIT_MODE (0=NO WAIT,1=WAIT)
[lock_timeout:uint32]                 // only if flags has LOCK_TIMEOUT
[reservations:list]                   // only if flags has RESERVATIONS
```

#### **SET_TRANSACTION (0x17)**
Identical payload to `START_TRANSACTION`. There is **no** in-place modification of the current transaction.

#### **COMMIT (0x14)**
```
[COMMIT]
[flags:uint8] // bit0=AND_CHAIN, bit1=AND_NO_CHAIN, bit2=RETAINING
```

#### **ROLLBACK (0x15)**
```
[ROLLBACK]
[flags:uint8] // bit0=AND_CHAIN, bit1=AND_NO_CHAIN, bit2=RETAINING
```

#### **Extended Transaction Opcodes (16-bit)**
- `EXT_SET_AUTOCOMMIT (0x0102)`
  ```
  [EXT_SET_AUTOCOMMIT]
  [mode:uint8]            // 0=OFF,1=ON
  [conflict_action:uint8]
  [conflict_error_code:int32]  // only if conflict_action == ERROR
  ```
- `EXT_PREPARE_TRANSACTION (0x0105)` -> `[gid:string]`
- `EXT_COMMIT_PREPARED (0x0106)` -> `[gid:string]`
- `EXT_ROLLBACK_PREPARED (0x0107)` -> `[gid:string]`

#### **Transaction Flags**
- `0x0001` HAS_ISOLATION
- `0x0002` HAS_ACCESS_MODE
- `0x0004` HAS_DEFERRABLE
- `0x0008` HAS_WAIT_MODE
- `0x0010` HAS_LOCK_TIMEOUT
- `0x0020` HAS_RESERVATIONS
- `0x0040` HAS_AUTOCOMMIT
- `0x0080` HAS_CONFLICT_ERROR_CODE

enum SBLR\_Opcodes {  
    // \============================================  
    // BLR-Compatible Range (0x00-0xFF)  
    // Most BLR opcodes map directly. See blr\_legacy\_opcodes.  
    // \============================================  
    SBLR\_NOP            \= 0x00,  
    SBLR\_ASSIGNMENT     \= 0x01, // blr\_assignment  
    SBLR\_BEGIN          \= 0x02, // blr\_begin  
    SBLR\_MESSAGE        \= 0x04, // blr\_message  
    SBLR\_SHORT          \= 0x07, // blr\_short  
    SBLR\_LONG           \= 0x08, // blr\_long  
    SBLR\_QUAD           \= 0x09, // blr\_quad  
    SBLR\_FLOAT          \= 0x0A, // blr\_float  
    SBLR\_D\_FLOAT        \= 0x0B, // blr\_d\_float  
    SBLR\_SQL\_DATE       \= 0x0C, // blr\_sql\_date  
    SBLR\_SQL\_TIME       \= 0x0D, // blr\_sql\_time  
    SBLR\_TEXT           \= 0x0E, // blr\_text  
    SBLR\_TEXT2          \= 0x0F, // blr\_text2  
    SBLR\_INT64          \= 0x10, // blr\_int64  
    SBLR\_DOUBLE         \= 0x1B, // blr\_double  
    SBLR\_ADD            \= 0x22, // blr\_add  
    SBLR\_SUBTRACT       \= 0x23, // blr\_subtract  
    SBLR\_MULTIPLY       \= 0x24, // blr\_multiply  
    SBLR\_DIVIDE         \= 0x25, // blr\_divide  
    SBLR\_NEGATE         \= 0x26, // blr\_negate  
    SBLR\_CONCATENATE    \= 0x27, // blr\_concatenate  
    SBLR\_SUBSTRING      \= 0x28, // blr\_substring  
    SBLR\_EQL            \= 0x2F, // blr\_eql  
    SBLR\_NEQ            \= 0x30, // blr\_neq  
    SBLR\_GTR            \= 0x31, // blr\_gtr  
    SBLR\_GEQ            \= 0x32, // blr\_geq  
    SBLR\_LSS            \= 0x33, // blr\_lss  
    SBLR\_LEQ            \= 0x34, // blr\_leq  
    SBLR\_BETWEEN        \= 0x35, // blr\_between  
    SBLR\_LIKE           \= 0x36, // blr\_like  
    SBLR\_AND            \= 0x3A, // blr\_and  
    SBLR\_OR             \= 0x3B, // blr\_or  
    SBLR\_NOT            \= 0x3C, // blr\_not  
    SBLR\_ANY            \= 0x3D, // blr\_any  
    SBLR\_UNIQUE         \= 0x3E, // blr\_unique  
    SBLR\_ALL            \= 0x3F, // blr\_all  
    SBLR\_IF             \= 0x3C, // blr\_if  
    SBLR\_LOOP           \= 0x3D, // blr\_loop  
    SBLR\_FOR            \= 0x3E, // blr\_for  
    SBLR\_WHILE          \= 0x3F, // blr\_while  
    SBLR\_LEAVE          \= 0x40, // blr\_leave  
    SBLR\_CONTINUE       \= 0x41, // blr\_continue  
    SBLR\_STORE          \= 0x46, // blr\_store  
    SBLR\_STORE2         \= 0x47, // blr\_store2  
    SBLR\_MODIFY         \= 0x48, // blr\_modify  
    SBLR\_MODIFY2        \= 0x49, // blr\_modify2  
    SBLR\_ERASE          \= 0x4A, // blr\_erase  
    SBLR\_ERASE2         \= 0x4B, // blr\_erase2  
    SBLR\_FETCH          \= 0x4C, // blr\_fetch  
    SBLR\_FOR\_SELECT     \= 0x4D, // blr\_for\_select  
    SBLR\_SEND           \= 0x4E, // blr\_send  
    SBLR\_RECEIVE        \= 0x4F, // blr\_receive  
    SBLR\_SELECT         \= 0x50, // blr\_select  
    SBLR\_RSE            \= 0x51, // blr\_rse (Record Selection Expression)  
    SBLR\_FIRST          \= 0x52, // blr\_first  
    SBLR\_PROJECT        \= 0x53, // blr\_project  
    SBLR\_SORT           \= 0x54, // blr\_sort  
    SBLR\_BOOLEAN        \= 0x55, // blr\_boolean  
    SBLR\_ASCENDING      \= 0x56, // blr\_ascending  
    SBLR\_DESCENDING     \= 0x57, // blr\_descending  
    SBLR\_RELATION       \= 0x58, // blr\_relation  
    SBLR\_RELATION2      \= 0x59, // blr\_relation2  
    SBLR\_RID            \= 0x5A, // blr\_rid  
    SBLR\_UNION          \= 0x5B, // blr\_union  
    SBLR\_MAP            \= 0x5C, // blr\_map  
    SBLR\_GROUP\_BY       \= 0x5D, // blr\_group\_by  
    SBLR\_AGGREGATE      \= 0x5E, // blr\_aggregate  
    SBLR\_JOIN\_TYPE      \= 0x5F, // blr\_join\_type  
    SBLR\_FUNCTION       \= 0x64, // blr\_function  
    SBLR\_GEN\_ID         \= 0x65, // blr\_gen\_id  
    SBLR\_GEN\_ID2        \= 0x66, // blr\_gen\_id2  
    SBLR\_COUNT          \= 0x67, // blr\_count  
    SBLR\_COUNT2         \= 0x68, // blr\_count2  
    SBLR\_MAX            \= 0x69, // blr\_max  
    SBLR\_MIN            \= 0x6A, // blr\_min  
    SBLR\_AVERAGE        \= 0x6B, // blr\_average  
    SBLR\_TOTAL          \= 0x6C, // blr\_total  
    SBLR\_FROM           \= 0x6D, // blr\_from  
    SBLR\_VIA            \= 0x6E, // blr\_via  
    SBLR\_USER\_NAME      \= 0x6F, // blr\_user\_name  
    SBLR\_CURRENT\_DATE   \= 0x70, // blr\_current\_date  
    SBLR\_CURRENT\_TIME   \= 0x71, // blr\_current\_time  
    SBLR\_CURRENT\_TIMESTAMP \= 0x72, // blr\_current\_timestamp  
    SBLR\_CURRENT\_ROLE   \= 0x73, // blr\_current\_role  
    SBLR\_FIELD          \= 0x78, // blr\_field  
    SBLR\_FIELD2         \= 0x79, // blr\_field2  
    SBLR\_PARAMETER      \= 0x7A, // blr\_parameter  
    SBLR\_PARAMETER2     \= 0x7B, // blr\_parameter2  
    SBLR\_VARIABLE       \= 0x7C, // blr\_variable  
    SBLR\_LITERAL        \= 0x7D, // blr\_literal  
    SBLR\_DBKEY          \= 0x7E, // blr\_dbkey  
    SBLR\_INDEX          \= 0x7F, // blr\_index  
    SBLR\_PROCEDURE      \= 0x82, // blr\_procedure  
    SBLR\_PROCEDURE2     \= 0x83, // blr\_procedure2  
    SBLR\_TRIGGER        \= 0x84, // blr\_trigger  
    SBLR\_EXEC\_PROC      \= 0x85, // blr\_exec\_proc  
    SBLR\_EXEC\_PROC2     \= 0x86, // blr\_exec\_proc2  
    SBLR\_EXEC\_STMT      \= 0x87, // blr\_exec\_stmt  
    SBLR\_EXEC\_INTO      \= 0x88, // blr\_exec\_into  
    SBLR\_EXEC\_SQL       \= 0x89, // blr\_exec\_sql  
    SBLR\_INTERNAL\_INFO  \= 0x8A, // blr\_internal\_info  
    SBLR\_BLOCK          \= 0x8B, // blr\_block  
    SBLR\_ERROR\_HANDLER  \= 0x8C, // blr\_error\_handler  
    SBLR\_START\_SAVEPOINT \= 0x8D, // blr\_start\_savepoint  
    SBLR\_END\_SAVEPOINT  \= 0x8E, // blr\_end\_savepoint  
    SBLR\_HANDLER        \= 0x8F, // blr\_handler  
    SBLR\_POST           \= 0x90, // blr\_post  
    SBLR\_POST\_ARG       \= 0x91, // blr\_post\_arg  
    SBLR\_PUT\_SLICE      \= 0x92, // blr\_put\_slice  
    SBLR\_GET\_SLICE      \= 0x93, // blr\_get\_slice  
    SBLR\_DCL\_VARIABLE   \= 0x94, // blr\_dcl\_variable  
    SBLR\_PLAN           \= 0x96, // blr\_plan  
    SBLR\_MERGE          \= 0x97, // blr\_merge  
    SBLR\_JOIN           \= 0x98, // blr\_join  
    SBLR\_SEQUENTIAL     \= 0x99, // blr\_sequential  
    SBLR\_NAVIGATIONAL   \= 0x9A, // blr\_navigational  
    SBLR\_INDICES        \= 0x9B, // blr\_indices  
    SBLR\_DECLARE\_CURSOR \= 0xA0, // blr\_declare\_cursor  
    SBLR\_CURSOR\_STMT    \= 0xA1, // blr\_cursor\_stmt  
    SBLR\_OPEN\_CURSOR    \= 0xA2, // blr\_open\_cursor  
    SBLR\_CLOSE\_CURSOR   \= 0xA3, // blr\_close\_cursor  
    SBLR\_FETCH\_CURSOR   \= 0xA4, // blr\_fetch\_cursor  
    SBLR\_CURSOR\_NAME    \= 0xA5, // blr\_cursor\_name  
    SBLR\_NULL           \= 0xB4, // blr\_null  
    SBLR\_MISSING        \= 0xB7, // blr\_missing  
    SBLR\_CAST           \= 0xC8, // blr\_cast  
    SBLR\_END            \= 0xFF, // blr\_end

    // \============================================  
    // SBLR Extended Range (using a prefix or 0x100+)  
    // \============================================  
    SBLR\_EXTENDED\_PREFIX \= 0x10,

    // Stack operations  
    SBLR\_DUP            \= 0xE0, // Duplicate top of stack  
    SBLR\_POP            \= 0xE4, // Pop value from stack  
    SBLR\_SWAP           \= 0xE2, // Swap top two stack values

    // Control Flow  
    SBLR\_JUMP           \= 0xF0, // Unconditional jump (takes 2-byte offset)  
    SBLR\_JUMP\_IF\_TRUE   \= 0xF1, // Jump if top of stack is true  
    SBLR\_JUMP\_IF\_FALSE  \= 0xF2, // Jump if top of stack is false  
    SBLR\_CALL           \= 0xF5, // Call subroutine (pushes return address)  
    SBLR\_RETURN         \= 0xF6, // Return from subroutine  
    SBLR\_THROW          \= 0xF7, // Raise an exception

    // Adaptive Specializations (dynamically generated opcodes)  
    SBLR\_ADAPTIVE\_BASE  \= 0x200,  
    SBLR\_ADD\_INT\_FAST   \= 0x210,    // Specialized fast integer addition  
    SBLR\_CMP\_STRING\_FAST= 0x222,    // Specialized fast string comparison  
    SBLR\_LOAD\_FIELD\_FAST= 0x200,    // Specialized field load from a known object shape  
    SBLR\_CALL\_BUILTIN\_FAST \= 0x230, // Fast path for a known built-in function

    // Vector Operations  
    SBLR\_VECTOR\_BASE    \= 0x300,  
    SBLR\_VEC\_LOAD       \= 0x300,    // Vector load from memory  
    SBLR\_VEC\_STORE      \= 0x301,    // Vector store to memory  
    SBLR\_VEC\_ADD        \= 0x302,    // Vector addition (e.g., INT\[4\] \+ INT\[4\])  
    SBLR\_VEC\_MUL        \= 0x304,    // Vector multiplication  
    SBLR\_VEC\_REDUCE     \= 0x308,    // Vector reduction (e.g., sum all elements)

    // Debug Operations  
    SBLR\_DEBUG\_BASE     \= 0x500,  
    SBLR\_BREAKPOINT     \= 0x500,    // Debugger breakpoint  
    SBLR\_TRACE          \= 0x501,    // Trace execution point  
    SBLR\_ASSERT         \= 0x502,    // Assert a condition is true  
    SBLR\_LOG            \= 0x505,    // Log a message  
};

## **5\. SBLR Type System**

SBLR includes a rich, self-describing type system that is a superset of the BLR types.

typedef enum SBLR\_Type {  
    // BLR-Compatible Types  
    SBLR\_TYPE\_SHORT     \= 0x07, SBLR\_TYPE\_LONG      \= 0x08, SBLR\_TYPE\_QUAD      \= 0x09,  
    SBLR\_TYPE\_FLOAT     \= 0x0A, SBLR\_TYPE\_DOUBLE    \= 0x1B, SBLR\_TYPE\_TEXT      \= 0x0E,  
    SBLR\_TYPE\_VARYING   \= 0x25, SBLR\_TYPE\_TIMESTAMP \= 0x23, SBLR\_TYPE\_BLOB      \= 0x105,  
    SBLR\_TYPE\_SQL\_DATE  \= 0x0C, SBLR\_TYPE\_SQL\_TIME  \= 0x0D, SBLR\_TYPE\_BOOLEAN   \= 0x17,  
      
    // SBLR Extended Types  
    SBLR\_TYPE\_INT8      \= 0x80, SBLR\_TYPE\_UINT8     \= 0x81, SBLR\_TYPE\_UINT16    \= 0x82,  
    SBLR\_TYPE\_UINT32    \= 0x83, SBLR\_TYPE\_UINT64    \= 0x84, SBLR\_TYPE\_INT128    \= 0x85,  
    SBLR\_TYPE\_DECIMAL   \= 0x86, SBLR\_TYPE\_UTF8      \= 0x90, SBLR\_TYPE\_UUID      \= 0x95,  
    SBLR\_TYPE\_JSON      \= 0xB5, SBLR\_TYPE\_JSONB     \= 0xB6, SBLR\_TYPE\_XML       \= 0xB7,  
    SBLR\_TYPE\_ARRAY     \= 0xB0, // Array of another type  
    SBLR\_TYPE\_RECORD    \= 0xB1, // Structured record type  
    SBLR\_TYPE\_CURSOR    \= 0xB3, // A reference to a cursor  
    SBLR\_TYPE\_DOMAIN    \= 0xB8, // A reference to a ScratchBird Domain  
    SBLR\_TYPE\_VARIANT   \= 0xB9, // Polymorphic type  
    SBLR\_TYPE\_ANY       \= 0xFF, // Generic/polymorphic type placeholder  
} SBLR\_Type;

// Type descriptor structure found in the type info section  
typedef struct SBLR\_TypeInfo {  
    SBLR\_Type       base\_type;      // The base type from the enum  
    uint32\_t        size;           // Size in bytes  
    uint16\_t        precision;      // For decimal types  
    uint16\_t        scale;          // For decimal types  
    uint16\_t        charset\_id;     // For character types  
    uint16\_t        collation\_id;   // For character types  
    bool            nullable;  
      
    // Extended type information for complex types  
    union {  
        // For array types  
        struct {  
            uint32\_t    element\_type\_idx; // Index into the type table for the element type  
            uint32\_t    dimensions;  
            uint32\_t\* bounds;  
        } array\_details;  
          
        // For record types  
        struct {  
            uint32\_t        field\_count;  
            SBLR\_FieldInfo\* fields; // Array of field descriptors  
        } record\_details;  
    } details;  
} SBLR\_TypeInfo;

## **6\. Execution Model**

### **6.1. Stack-Based VM**

The SBLR Virtual Machine is a stack-based machine. Instructions pop operands from the stack, perform an operation, and push the result back onto the stack.

**Example: a \= b \+ 5**

SBLR\_VARIABLE, \<index\_of\_b\>   ; Push value of variable b onto the stack  
SBLR\_LITERAL, \<index\_of\_5\>    ; Push the literal value 5 onto the stack  
SBLR\_ADD                      ; Pop b and 5, add them, push the result  
SBLR\_VARIABLE, \<index\_of\_a\>   ; Push the address of variable a  
SBLR\_ASSIGNMENT               ; Pop result and address of a, assign value to address

### **6.2. Adaptive Execution and JIT Compilation**

The VM profiles the execution of the bytecode. When a code path (like a loop or a frequently called function) is identified as "hot," the adaptive optimization engine can rewrite the generic bytecode with specialized, faster versions of opcodes (e.g., replacing SBLR\_ADD with SBLR\_ADD\_INT\_FAST if the types are always integers).

If a code path becomes extremely hot, it can be queued for the Just-In-Time (JIT) compiler, which translates the SBLR bytecode for that path into native machine code for maximum performance. The VM then transparently calls this native code instead of interpreting the bytecode.

## **7\. Debugging and Tooling**

The SBLR format is designed to be fully debuggable.

* **Debug Information Section**: When compiled with debug flags, the SBLR module includes a section that maps bytecode instruction offsets back to the original line and column numbers in the PSQL source code.  
* **Symbol Table**: This section contains the names of all variables, parameters, and routines, allowing a debugger to display them by name instead of by index.  
* **Debug Opcodes**: SBLR\_BREAKPOINT and SBLR\_ASSERT can be embedded in the bytecode to facilitate debugging.

A disassembler tool can use this information to produce a human-readable version of the bytecode, annotated with source code and variable names.
