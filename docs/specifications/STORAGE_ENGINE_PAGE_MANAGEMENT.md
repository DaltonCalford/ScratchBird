# ScratchBird Storage Engine - Page Management and Storage Layer
## Part 2 of Storage Engine Specification

## Overview

This document specifies ScratchBird's page management system, including page layouts, free space management, compression, encryption, and integration with the multi-page-size architecture (8K-128K).

## 1. Page Structure and Layout

### 1.1 Universal Page Header

```c
// Universal page header for all page types
typedef struct sb_page_header {
    // Page identification
    uint32_t        pd_checksum;           // Page checksum (first for validation)
    uint16_t        pd_flags;              // Page flags
    uint16_t        pd_page_size_idx;      // Page size index (0=8K, 1=16K, etc.)
    
    // Page location
    uint64_t        pd_page_number;        // Physical page number
    UUID            pd_table_uuid;         // Table UUID this page belongs to
    
    // Page type and version
    uint16_t        pd_type;               // Page type
    uint16_t        pd_version;            // Page layout version
    
    // LSN and transaction info
    LSN             pd_lsn;                // Last LSN that modified page
    TransactionId   pd_xmin;               // Oldest XID on page
    TransactionId   pd_xmax;               // Newest XID on page
    
    // Space management
    uint16_t        pd_lower;              // Start of free space
    uint16_t        pd_upper;              // End of free space
    uint16_t        pd_special;            // Start of special area
    
    // Compression and encryption
    uint8_t         pd_compression_method; // Compression type
    uint8_t         pd_encryption_method;  // Encryption type
    uint16_t        pd_compressed_size;    // Size if compressed
    
    // Page chain
    uint64_t        pd_prev_page;          // Previous page in chain
    uint64_t        pd_next_page;          // Next page in chain
    
    // Reserved for future use
    uint8_t         pd_reserved[16];       // Reserved space
} SBPageHeader;

// Page flags
#define PD_FLAG_HAS_FREE_LINES  0x0001     // Has free line pointers
#define PD_FLAG_PAGE_FULL       0x0002     // No free space
#define PD_FLAG_ALL_VISIBLE     0x0004     // All tuples visible to all
#define PD_FLAG_COMPRESSED      0x0008     // Page is compressed
#define PD_FLAG_ENCRYPTED       0x0010     // Page is encrypted
#define PD_FLAG_HAS_GARBAGE     0x0020     // Has dead tuples
#define PD_FLAG_CHECKSUM_VALID  0x0040     // Checksum is valid
#define PD_FLAG_TEMP            0x0080     // Temporary page

// Page types
typedef enum page_type {
    PAGE_TYPE_HEAP = 1,         // Heap data page
    PAGE_TYPE_INDEX_BTREE,      // B-tree index page
    PAGE_TYPE_INDEX_HASH,       // Hash index page
    PAGE_TYPE_INDEX_GIN,        // GIN index page
    PAGE_TYPE_INDEX_BITMAP,     // Bitmap index page
    PAGE_TYPE_FSM,              // Free space map
    PAGE_TYPE_VM,               // Visibility map
    PAGE_TYPE_TIP,              // Transaction inventory page
    PAGE_TYPE_TOAST,            // TOAST data page
    PAGE_TYPE_SEQUENCE,         // Sequence page
    PAGE_TYPE_SLRU,             // SLRU page
    PAGE_TYPE_SPECIAL           // Special purpose
} PageType;
```

### 1.2 Heap Page Layout

```c
// Heap page structure for table data
typedef struct heap_page {
    SBPageHeader    hp_header;             // Standard page header
    
    // Line pointer array (grows from start)
    ItemId          hp_linp[FLEXIBLE_ARRAY_MEMBER];
    
    // Free space
    // ...
    
    // Tuple data (grows from end)
    // ...
    
    // Special space (at end)
    HeapPageSpecial hp_special;
} HeapPage;

// Line pointer (item identifier)
typedef struct item_id {
    unsigned        lp_off:15,             // Offset to tuple (from page start)
                   lp_flags:2,             // State flags
                   lp_len:15;              // Tuple length
} ItemId;

// Line pointer flags
#define LP_UNUSED       0                  // Unused line pointer
#define LP_NORMAL       1                  // Normal tuple
#define LP_REDIRECT     2                  // Redirect to another line pointer
#define LP_DEAD         3                  // Dead tuple

// Heap page special data
typedef struct heap_page_special {
    uint16_t        hps_page_size;         // Actual page size
    uint16_t        hps_free_space;        // Free space on page
    TransactionId   hps_oldest_xid;        // Oldest XID on page
    uint16_t        hps_ntuples;           // Number of tuples
    uint16_t        hps_flags;             // Special flags
} HeapPageSpecial;

// Heap tuple header (for MGA/MVCC)
typedef struct heap_tuple_header {
    // Transaction visibility
    TransactionId   t_xmin;                // Insert XID
    TransactionId   t_xmax;                // Delete/update XID
    CommandId       t_cmin;                // Insert CID
    CommandId       t_cmax;                // Delete/update CID
    
    // Tuple identification
    ItemPointer     t_ctid;                // Current tuple ID
    UUID            t_uuid;                // Tuple UUID (optional)
    
    // Tuple metadata
    uint16_t        t_infomask;            // Flags
    uint16_t        t_infomask2;           // More flags
    uint8_t         t_hoff;                // Header offset to user data
    
    // Null bitmap (variable length)
    bits8           t_bits[FLEXIBLE_ARRAY_MEMBER];
} HeapTupleHeader;

// Tuple infomask flags
#define HEAP_HASNULL            0x0001     // Has null attributes
#define HEAP_HASVARWIDTH        0x0002     // Has variable-width attributes
#define HEAP_HASEXTERNAL        0x0004     // Has external stored attributes
#define HEAP_HASOID             0x0008     // Has OID
#define HEAP_XMAX_KEYSHR_LOCK   0x0010     // Key-share lock
#define HEAP_COMBOCID           0x0020     // Combo CID
#define HEAP_XMAX_EXCL_LOCK     0x0040     // Exclusive lock
#define HEAP_XMAX_SHR_LOCK      0x0080     // Share lock
#define HEAP_XMIN_COMMITTED     0x0100     // xmin committed
#define HEAP_XMIN_INVALID       0x0200     // xmin invalid
#define HEAP_XMAX_COMMITTED     0x0400     // xmax committed
#define HEAP_XMAX_INVALID       0x0800     // xmax invalid
#define HEAP_XMAX_IS_MULTI      0x1000     // xmax is multixact
#define HEAP_UPDATED            0x2000     // Tuple was updated
#define HEAP_MOVED_OFF          0x4000     // Moved to another page (vacuum)
#define HEAP_MOVED_IN           0x8000     // Moved from another page (vacuum)
```

### 1.3 Page Operations

```c
// Page initialization
void page_init(
    Page page,
    uint32_t page_size,
    uint16_t special_size)
{
    SBPageHeader* header = (SBPageHeader*)page;
    
    // Clear page
    memset(page, 0, page_size);
    
    // Initialize header
    header->pd_lower = sizeof(SBPageHeader);
    header->pd_upper = page_size - special_size;
    header->pd_special = page_size - special_size;
    header->pd_page_size_idx = get_page_size_index(page_size);
    header->pd_version = CURRENT_PAGE_VERSION;
    
    // Calculate and set checksum
    header->pd_checksum = calculate_page_checksum(page, page_size);
}

// Add tuple to heap page
OffsetNumber heap_page_add_tuple(
    HeapPage* page,
    HeapTuple tuple,
    uint32_t page_size,
    bool allow_compression)
{
    SBPageHeader* header = &page->hp_header;
    uint32_t tuple_size = tuple->t_len;
    
    // Check if compression would help
    if (allow_compression && should_compress_tuple(tuple)) {
        tuple = compress_tuple(tuple);
        tuple_size = tuple->t_len;
    }
    
    // Find free space
    uint16_t free_space = header->pd_upper - header->pd_lower;
    
    if (free_space < tuple_size + sizeof(ItemId)) {
        // Try to compact page
        heap_page_compact(page);
        free_space = header->pd_upper - header->pd_lower;
        
        if (free_space < tuple_size + sizeof(ItemId)) {
            return InvalidOffsetNumber;  // No space
        }
    }
    
    // Find free line pointer
    OffsetNumber offset = find_free_line_pointer(page);
    
    if (offset == InvalidOffsetNumber) {
        // Allocate new line pointer
        offset = header->pd_lower / sizeof(ItemId);
        header->pd_lower += sizeof(ItemId);
    }
    
    // Place tuple at end of free space
    header->pd_upper -= tuple_size;
    memcpy((char*)page + header->pd_upper, tuple->t_data, tuple_size);
    
    // Set line pointer
    ItemId* linp = &page->hp_linp[offset - 1];
    linp->lp_off = header->pd_upper;
    linp->lp_len = tuple_size;
    linp->lp_flags = LP_NORMAL;
    
    // Update page metadata
    HeapPageSpecial* special = (HeapPageSpecial*)
        ((char*)page + header->pd_special);
    special->hps_ntuples++;
    special->hps_free_space = header->pd_upper - header->pd_lower;
    
    // Update checksum
    header->pd_checksum = calculate_page_checksum(page, page_size);
    
    return offset;
}

// Compact heap page (defragment)
void heap_page_compact(HeapPage* page) {
    SBPageHeader* header = &page->hp_header;
    uint32_t page_size = get_page_size(header->pd_page_size_idx);
    
    // Create temporary buffer
    char* temp_buffer = allocate(page_size);
    uint16_t temp_upper = page_size - 
        (page_size - header->pd_special);  // Special size
    
    // Copy live tuples to temp buffer
    uint16_t nline = header->pd_lower / sizeof(ItemId);
    
    for (OffsetNumber i = 1; i <= nline; i++) {
        ItemId* linp = &page->hp_linp[i - 1];
        
        if (linp->lp_flags == LP_NORMAL) {
            // Copy tuple to temp buffer
            temp_upper -= linp->lp_len;
            memcpy(temp_buffer + temp_upper,
                  (char*)page + linp->lp_off,
                  linp->lp_len);
            
            // Update line pointer
            linp->lp_off = temp_upper;
        }
    }
    
    // Copy compacted data back
    uint16_t data_size = page_size - temp_upper;
    memcpy((char*)page + header->pd_special - data_size,
          temp_buffer + temp_upper,
          data_size);
    
    // Update header
    header->pd_upper = header->pd_special - data_size;
    header->pd_flags &= ~PD_FLAG_HAS_GARBAGE;
    
    free(temp_buffer);
}
```

## 2. Free Space Management

### 2.1 Free Space Map (FSM)

```c
// Free space map page
typedef struct fsm_page {
    SBPageHeader    fsm_header;            // Standard page header
    uint8_t         fsm_nodes[FLEXIBLE_ARRAY_MEMBER]; // FSM tree nodes
} FSMPage;

// FSM tree structure (max-heap of free space categories)
#define FSM_CATEGORIES      256             // Number of free space categories
#define FSM_CAT_STEP        (BLCKSZ / FSM_CATEGORIES)

// Get free space category for amount
uint8_t fsm_get_category(uint16_t free_space) {
    return Min(free_space / FSM_CAT_STEP, FSM_CATEGORIES - 1);
}

// FSM operations
typedef struct fsm_operations {
    // Find page with enough free space
    BlockNumber (*search)(FSMPage* fsm,
                         uint16_t needed_space,
                         BlockNumber start_block);
    
    // Update free space for page
    void (*update)(FSMPage* fsm,
                  BlockNumber block,
                  uint16_t free_space);
    
    // Vacuum FSM
    void (*vacuum)(FSMPage* fsm);
} FSMOperations;

// Search FSM for page with enough space
BlockNumber fsm_search_free_space(
    Relation rel,
    uint16_t needed_space)
{
    uint8_t needed_cat = fsm_get_category(needed_space);
    BlockNumber fsm_block = 0;
    
    // Start from root FSM page
    FSMPage* fsm = read_fsm_page(rel, fsm_block);
    
    // Search down the tree
    while (!is_fsm_leaf_page(fsm)) {
        int slot = fsm_find_best_child(fsm, needed_cat);
        
        if (slot < 0) {
            return InvalidBlockNumber;  // No space
        }
        
        fsm_block = fsm_get_child_block(fsm_block, slot);
        fsm = read_fsm_page(rel, fsm_block);
    }
    
    // Search leaf page
    return fsm_search_leaf(fsm, needed_cat);
}
```

### 2.2 Visibility Map (VM)

```c
// Visibility map page
typedef struct vm_page {
    SBPageHeader    vm_header;             // Standard page header
    uint8_t         vm_bits[FLEXIBLE_ARRAY_MEMBER]; // Visibility bits
} VMPage;

// VM bits per heap page
#define VM_ALL_VISIBLE      0x01           // All tuples visible to all
#define VM_ALL_FROZEN       0x02           // All tuples frozen

// Visibility map operations
typedef struct vm_operations {
    // Check if page is all-visible
    bool (*is_all_visible)(VMPage* vm, BlockNumber block);
    
    // Set page visibility
    void (*set_visibility)(VMPage* vm,
                          BlockNumber block,
                          uint8_t flags);
    
    // Clear visibility
    void (*clear_visibility)(VMPage* vm,
                            BlockNumber block);
} VMOperations;

// Check page visibility
bool vm_page_is_all_visible(
    Relation rel,
    BlockNumber heap_block)
{
    BlockNumber vm_block = HEAP_TO_VM_BLOCK(heap_block);
    uint32_t vm_offset = HEAP_TO_VM_OFFSET(heap_block);
    
    VMPage* vm = read_vm_page(rel, vm_block);
    uint8_t vm_byte = vm->vm_bits[vm_offset / 8];
    uint8_t vm_bit = 1 << (vm_offset % 8);
    
    return (vm_byte & vm_bit) != 0;
}
```

## 3. Page Compression

### 3.1 Page-Level Compression

```c
// Compression methods
typedef enum compression_method {
    COMPRESS_NONE = 0,
    COMPRESS_PGLZ,          // PostgreSQL LZ
    COMPRESS_LZ4,           // LZ4 fast compression
    COMPRESS_ZSTD,          // Zstandard compression
    COMPRESS_SNAPPY,        // Snappy compression
    COMPRESS_DICTIONARY     // Dictionary compression
} CompressionMethod;

// Compression context
typedef struct compression_context {
    CompressionMethod method;              // Compression method
    int             level;                 // Compression level
    void*           dictionary;            // Dictionary (if used)
    uint32_t        dict_size;             // Dictionary size
    
    // Statistics
    uint64_t        bytes_uncompressed;
    uint64_t        bytes_compressed;
    uint64_t        pages_compressed;
    double          compression_ratio;
} CompressionContext;

// Compress page
Page compress_page(
    Page uncompressed,
    uint32_t page_size,
    CompressionContext* ctx)
{
    SBPageHeader* header = (SBPageHeader*)uncompressed;
    
    // Don't compress if already compressed
    if (header->pd_flags & PD_FLAG_COMPRESSED) {
        return uncompressed;
    }
    
    // Allocate compressed buffer
    uint32_t max_compressed_size = page_size;
    char* compressed = allocate(max_compressed_size);
    
    // Copy header (never compressed)
    memcpy(compressed, header, sizeof(SBPageHeader));
    
    // Compress page body
    uint32_t body_size = page_size - sizeof(SBPageHeader);
    char* body_start = (char*)uncompressed + sizeof(SBPageHeader);
    char* compressed_body = compressed + sizeof(SBPageHeader);
    
    uint32_t compressed_size;
    
    switch (ctx->method) {
        case COMPRESS_LZ4:
            compressed_size = LZ4_compress_default(
                body_start, compressed_body,
                body_size, max_compressed_size - sizeof(SBPageHeader));
            break;
            
        case COMPRESS_ZSTD:
            compressed_size = ZSTD_compress(
                compressed_body, max_compressed_size - sizeof(SBPageHeader),
                body_start, body_size,
                ctx->level);
            break;
            
        case COMPRESS_DICTIONARY:
            compressed_size = compress_with_dictionary(
                compressed_body, body_start, body_size,
                ctx->dictionary, ctx->dict_size);
            break;
            
        default:
            free(compressed);
            return uncompressed;
    }
    
    // Check if compression is beneficial
    if (compressed_size >= body_size * 0.9) {
        // Not enough benefit
        free(compressed);
        return uncompressed;
    }
    
    // Update header
    SBPageHeader* comp_header = (SBPageHeader*)compressed;
    comp_header->pd_flags |= PD_FLAG_COMPRESSED;
    comp_header->pd_compression_method = ctx->method;
    comp_header->pd_compressed_size = compressed_size + sizeof(SBPageHeader);
    
    // Clear unused space
    memset(compressed + sizeof(SBPageHeader) + compressed_size,
          0,
          page_size - sizeof(SBPageHeader) - compressed_size);
    
    // Update statistics
    ctx->bytes_uncompressed += page_size;
    ctx->bytes_compressed += comp_header->pd_compressed_size;
    ctx->pages_compressed++;
    ctx->compression_ratio = (double)ctx->bytes_compressed / 
                           ctx->bytes_uncompressed;
    
    // Recalculate checksum
    comp_header->pd_checksum = calculate_page_checksum(compressed, page_size);
    
    return (Page)compressed;
}

// Decompress page
Page decompress_page(
    Page compressed,
    uint32_t page_size,
    CompressionContext* ctx)
{
    SBPageHeader* header = (SBPageHeader*)compressed;
    
    // Check if compressed
    if (!(header->pd_flags & PD_FLAG_COMPRESSED)) {
        return compressed;
    }
    
    // Allocate decompressed buffer
    char* decompressed = allocate(page_size);
    
    // Copy header
    memcpy(decompressed, header, sizeof(SBPageHeader));
    
    // Decompress body
    char* compressed_body = (char*)compressed + sizeof(SBPageHeader);
    char* decompressed_body = decompressed + sizeof(SBPageHeader);
    uint32_t compressed_size = header->pd_compressed_size - sizeof(SBPageHeader);
    uint32_t decompressed_size = page_size - sizeof(SBPageHeader);
    
    switch (header->pd_compression_method) {
        case COMPRESS_LZ4:
            LZ4_decompress_safe(
                compressed_body, decompressed_body,
                compressed_size, decompressed_size);
            break;
            
        case COMPRESS_ZSTD:
            ZSTD_decompress(
                decompressed_body, decompressed_size,
                compressed_body, compressed_size);
            break;
            
        case COMPRESS_DICTIONARY:
            decompress_with_dictionary(
                decompressed_body, compressed_body, compressed_size,
                ctx->dictionary, ctx->dict_size);
            break;
    }
    
    // Update header
    SBPageHeader* decomp_header = (SBPageHeader*)decompressed;
    decomp_header->pd_flags &= ~PD_FLAG_COMPRESSED;
    decomp_header->pd_compression_method = COMPRESS_NONE;
    decomp_header->pd_compressed_size = 0;
    
    // Recalculate checksum
    decomp_header->pd_checksum = calculate_page_checksum(decompressed, page_size);
    
    return (Page)decompressed;
}
```

## 4. Page Encryption

### 4.1 Transparent Data Encryption (TDE)

```c
// Encryption methods
typedef enum encryption_method {
    ENCRYPT_NONE = 0,
    ENCRYPT_AES128_CTR,     // AES-128 in CTR mode
    ENCRYPT_AES256_CTR,     // AES-256 in CTR mode
    ENCRYPT_AES128_GCM,     // AES-128 with authentication
    ENCRYPT_AES256_GCM,     // AES-256 with authentication
    ENCRYPT_CHACHA20        // ChaCha20
} EncryptionMethod;

// Encryption context
typedef struct encryption_context {
    EncryptionMethod method;               // Encryption method
    
    // Key management
    uint8_t         master_key[32];        // Master key
    uint8_t         data_key[32];          // Data encryption key
    uint8_t         key_id;                // Key identifier
    
    // Initialization vectors
    uint8_t         iv[16];                // Current IV
    uint64_t        iv_counter;            // IV counter
    
    // Authentication
    uint8_t         auth_tag[16];          // Authentication tag (GCM)
    
    // Statistics
    uint64_t        pages_encrypted;
    uint64_t        pages_decrypted;
} EncryptionContext;

// Encrypt page
Page encrypt_page(
    Page plaintext,
    uint32_t page_size,
    EncryptionContext* ctx)
{
    SBPageHeader* header = (SBPageHeader*)plaintext;
    
    // Don't encrypt if already encrypted
    if (header->pd_flags & PD_FLAG_ENCRYPTED) {
        return plaintext;
    }
    
    // Allocate encrypted buffer
    char* encrypted = allocate(page_size);
    
    // Copy header (encrypted separately or not at all)
    memcpy(encrypted, header, sizeof(SBPageHeader));
    
    // Generate IV for this page
    uint8_t page_iv[16];
    generate_page_iv(ctx, header->pd_page_number, page_iv);
    
    // Encrypt page body
    uint32_t body_size = page_size - sizeof(SBPageHeader);
    char* plaintext_body = (char*)plaintext + sizeof(SBPageHeader);
    char* encrypted_body = encrypted + sizeof(SBPageHeader);
    
    switch (ctx->method) {
        case ENCRYPT_AES256_CTR:
            aes256_ctr_encrypt(
                plaintext_body, encrypted_body, body_size,
                ctx->data_key, page_iv);
            break;
            
        case ENCRYPT_AES256_GCM:
            aes256_gcm_encrypt(
                plaintext_body, encrypted_body, body_size,
                ctx->data_key, page_iv,
                header, sizeof(SBPageHeader),  // Additional auth data
                ctx->auth_tag);
            break;
            
        case ENCRYPT_CHACHA20:
            chacha20_encrypt(
                plaintext_body, encrypted_body, body_size,
                ctx->data_key, page_iv);
            break;
    }
    
    // Update header
    SBPageHeader* enc_header = (SBPageHeader*)encrypted;
    enc_header->pd_flags |= PD_FLAG_ENCRYPTED;
    enc_header->pd_encryption_method = ctx->method;
    
    // Store key ID (for key rotation)
    enc_header->pd_reserved[0] = ctx->key_id;
    
    // Store auth tag if using GCM
    if (ctx->method == ENCRYPT_AES128_GCM || 
        ctx->method == ENCRYPT_AES256_GCM) {
        memcpy(&enc_header->pd_reserved[1], ctx->auth_tag, 16);
    }
    
    // Update statistics
    ctx->pages_encrypted++;
    
    // Recalculate checksum
    enc_header->pd_checksum = calculate_page_checksum(encrypted, page_size);
    
    return (Page)encrypted;
}

// Decrypt page
Page decrypt_page(
    Page encrypted,
    uint32_t page_size,
    EncryptionContext* ctx)
{
    SBPageHeader* header = (SBPageHeader*)encrypted;
    
    // Check if encrypted
    if (!(header->pd_flags & PD_FLAG_ENCRYPTED)) {
        return encrypted;
    }
    
    // Verify checksum first
    uint32_t stored_checksum = header->pd_checksum;
    header->pd_checksum = 0;
    uint32_t calc_checksum = calculate_page_checksum(encrypted, page_size);
    
    if (stored_checksum != calc_checksum) {
        elog(ERROR, "Page checksum mismatch before decryption");
    }
    header->pd_checksum = stored_checksum;
    
    // Allocate decrypted buffer
    char* decrypted = allocate(page_size);
    
    // Copy header
    memcpy(decrypted, header, sizeof(SBPageHeader));
    
    // Generate IV for this page
    uint8_t page_iv[16];
    generate_page_iv(ctx, header->pd_page_number, page_iv);
    
    // Get key ID and auth tag from header
    uint8_t key_id = header->pd_reserved[0];
    uint8_t auth_tag[16];
    
    if (header->pd_encryption_method == ENCRYPT_AES128_GCM ||
        header->pd_encryption_method == ENCRYPT_AES256_GCM) {
        memcpy(auth_tag, &header->pd_reserved[1], 16);
    }
    
    // Decrypt page body
    uint32_t body_size = page_size - sizeof(SBPageHeader);
    char* encrypted_body = (char*)encrypted + sizeof(SBPageHeader);
    char* decrypted_body = decrypted + sizeof(SBPageHeader);
    
    bool success = false;
    
    switch (header->pd_encryption_method) {
        case ENCRYPT_AES256_CTR:
            aes256_ctr_decrypt(
                encrypted_body, decrypted_body, body_size,
                ctx->data_key, page_iv);
            success = true;
            break;
            
        case ENCRYPT_AES256_GCM:
            success = aes256_gcm_decrypt(
                encrypted_body, decrypted_body, body_size,
                ctx->data_key, page_iv,
                header, sizeof(SBPageHeader),  // Additional auth data
                auth_tag);
            break;
            
        case ENCRYPT_CHACHA20:
            chacha20_decrypt(
                encrypted_body, decrypted_body, body_size,
                ctx->data_key, page_iv);
            success = true;
            break;
    }
    
    if (!success) {
        free(decrypted);
        elog(ERROR, "Page decryption failed");
    }
    
    // Update header
    SBPageHeader* dec_header = (SBPageHeader*)decrypted;
    dec_header->pd_flags &= ~PD_FLAG_ENCRYPTED;
    dec_header->pd_encryption_method = ENCRYPT_NONE;
    
    // Clear reserved data
    memset(dec_header->pd_reserved, 0, sizeof(dec_header->pd_reserved));
    
    // Update statistics
    ctx->pages_decrypted++;
    
    // Recalculate checksum
    dec_header->pd_checksum = calculate_page_checksum(decrypted, page_size);
    
    return (Page)decrypted;
}
```

## 5. TOAST (The Oversized-Attribute Storage Technique)

### 5.1 TOAST Strategy

```c
// TOAST strategies
typedef enum toast_strategy {
    TOAST_PLAIN,            // No compression, inline storage
    TOAST_EXTENDED,         // Allow compression and out-of-line
    TOAST_EXTERNAL,         // Allow out-of-line, no compression
    TOAST_MAIN              // Prefer compression, allow out-of-line
} ToastStrategy;

// TOAST pointer for out-of-line storage
typedef struct toast_pointer {
    uint32_t        va_header;             // Varlena header
    UUID            va_toastrelid;         // TOAST table UUID
    UUID            va_valueid;            // Value identifier
    uint32_t        va_rawsize;            // Original size
    uint32_t        va_extsize;            // External size
    CompressionMethod va_compress_method;  // Compression method
} ToastPointer;

// TOAST tuple for external storage
typedef struct toast_tuple {
    UUID            chunk_id;              // Chunk identifier
    int32_t         chunk_seq;             // Sequence number
    bytea           chunk_data;            // Chunk data
} ToastTuple;

// TOAST value
bool toast_value(
    Datum value,
    ToastStrategy strategy,
    Relation toast_rel,
    ToastPointer* result)
{
    uint32_t value_size = VARSIZE_ANY(value);
    
    // Check if needs toasting
    if (value_size <= TOAST_THRESHOLD) {
        return false;  // No toasting needed
    }
    
    // Try compression first if allowed
    if (strategy != TOAST_EXTERNAL) {
        Datum compressed = compress_datum(value);
        uint32_t compressed_size = VARSIZE_ANY(compressed);
        
        if (compressed_size < value_size * COMPRESSION_THRESHOLD) {
            // Compression beneficial
            value = compressed;
            value_size = compressed_size;
            result->va_compress_method = COMPRESS_LZ4;
        }
    }
    
    // Check if still needs external storage
    if (value_size <= TOAST_THRESHOLD) {
        return false;  // Fits inline after compression
    }
    
    // Store externally
    result->va_valueid = generate_uuid_v7();
    result->va_toastrelid = toast_rel->rd_id;
    result->va_rawsize = VARSIZE_ANY(DatumGetPointer(value));
    result->va_extsize = value_size;
    
    // Split into chunks
    uint32_t chunk_size = TOAST_CHUNK_SIZE;
    uint32_t num_chunks = (value_size + chunk_size - 1) / chunk_size;
    char* data = VARDATA_ANY(value);
    
    for (uint32_t i = 0; i < num_chunks; i++) {
        uint32_t this_chunk_size = Min(chunk_size, 
                                      value_size - i * chunk_size);
        
        // Create TOAST tuple
        ToastTuple toast_tuple;
        toast_tuple.chunk_id = result->va_valueid;
        toast_tuple.chunk_seq = i;
        toast_tuple.chunk_data = palloc(this_chunk_size + VARHDRSZ);
        SET_VARSIZE(toast_tuple.chunk_data, this_chunk_size + VARHDRSZ);
        memcpy(VARDATA(toast_tuple.chunk_data),
              data + i * chunk_size,
              this_chunk_size);
        
        // Insert into TOAST table
        simple_heap_insert(toast_rel, &toast_tuple);
    }
    
    return true;
}
```

## 6. Multi-Page-Size Support

### 6.1 Page Size Management

```c
// Supported page sizes
static const uint32_t supported_page_sizes[] = {
    8192,    // 8KB
    16384,   // 16KB
    32768,   // 32KB
    65536,   // 64KB
    131072   // 128KB
};

#define NUM_PAGE_SIZES 5

// Page size context
typedef struct page_size_context {
    uint32_t        default_page_size;     // Default for new tables
    uint32_t        min_page_size;         // Minimum allowed
    uint32_t        max_page_size;         // Maximum allowed
    
    // Per-table page sizes
    HashTable*      table_page_sizes;      // UUID -> page size
    
    // Statistics per page size
    struct {
        uint64_t    tables_count;          // Tables using this size
        uint64_t    pages_count;           // Total pages
        uint64_t    bytes_used;            // Bytes used
        double      avg_fill_factor;       // Average fill factor
    } stats[NUM_PAGE_SIZES];
} PageSizeContext;

// Get page size for table
uint32_t get_table_page_size(UUID table_uuid) {
    PageSizeContext* ctx = get_page_size_context();
    
    uint32_t* page_size = hash_table_lookup(
        ctx->table_page_sizes, &table_uuid);
    
    if (page_size != NULL) {
        return *page_size;
    }
    
    return ctx->default_page_size;
}

// Choose optimal page size for table
uint32_t choose_optimal_page_size(
    TableCreationParams* params)
{
    // Estimate average tuple size
    uint32_t avg_tuple_size = estimate_tuple_size(params->columns);
    
    // Consider expected table size
    uint64_t expected_rows = params->expected_rows;
    uint64_t expected_size = expected_rows * avg_tuple_size;
    
    // Choose page size based on heuristics
    if (avg_tuple_size > 4096) {
        // Large tuples - use larger pages
        if (avg_tuple_size > 16384) {
            return 131072;  // 128KB
        } else if (avg_tuple_size > 8192) {
            return 65536;   // 64KB
        } else {
            return 32768;   // 32KB
        }
    } else if (expected_size < 1024 * 1024) {
        // Small table - use small pages
        return 8192;        // 8KB
    } else {
        // Default case
        return 16384;       // 16KB
    }
}
```

## 7. Checksum Calculation

### 7.1 Page Checksum Implementation

```c
// Calculate page checksum (CRC32C)
uint32_t calculate_page_checksum(
    Page page,
    uint32_t page_size)
{
    SBPageHeader* header = (SBPageHeader*)page;
    uint32_t save_checksum = header->pd_checksum;
    
    // Clear checksum field
    header->pd_checksum = 0;
    
    // Calculate CRC32C
    uint32_t checksum = crc32c(0, page, page_size);
    
    // Restore checksum field
    header->pd_checksum = save_checksum;
    
    return checksum;
}

// Verify page checksum
bool verify_page_checksum(
    Page page,
    uint32_t page_size)
{
    SBPageHeader* header = (SBPageHeader*)page;
    
    // Skip if checksum not enabled
    if (!(header->pd_flags & PD_FLAG_CHECKSUM_VALID)) {
        return true;
    }
    
    uint32_t stored_checksum = header->pd_checksum;
    uint32_t calc_checksum = calculate_page_checksum(page, page_size);
    
    return stored_checksum == calc_checksum;
}
```

## Implementation Notes

This page management system provides:

1. **Universal page header** supporting all page types
2. **Efficient heap page layout** with line pointers
3. **Free space management** with FSM and visibility map
4. **Multiple compression algorithms** with adaptive selection
5. **Transparent encryption** with key management
6. **TOAST support** for large attributes
7. **Multi-page-size support** (8K-128K) with optimal selection
8. **Checksums** for data integrity

The system integrates with the buffer pool management described in Part 1 and provides the foundation for the MGA transaction system.