# Database Internals: Index Implementation

## Table of Contents
1. [FirebirdSQL Index Implementation](#firebirdsql-index-implementation)
2. [PostgreSQL Index Implementation](#postgresql-index-implementation)
3. [MySQL/MariaDB Index Implementation](#mysqlmariadb-index-implementation)
4. [Microsoft SQL Server Index Implementation](#microsoft-sql-server-index-implementation)

---

# FirebirdSQL Index Implementation

## B-Tree Implementation

### B-Tree Structure
```c
// Firebird B-Tree page structure
typedef struct btree_page {
    PageHeader  btr_header;         // Standard page header
    USHORT      btr_relation_id;   // Relation ID
    USHORT      btr_level;          // Level (0 = leaf)
    USHORT      btr_id;             // Index ID
    USHORT      btr_count;          // Number of nodes
    ULONG       btr_sibling;        // Right sibling page
    ULONG       btr_left_sibling;   // Left sibling page
    USHORT      btr_prefix_total;   // Total prefix compression
    USHORT      btr_compress_header; // Compression header size
    BTNode      btr_nodes[1];       // Array of nodes
} BTreePage;

// B-Tree node structure
typedef struct bt_node {
    USHORT      btn_prefix;         // Prefix compression length
    USHORT      btn_length;         // Key length
    USHORT      btn_number;         // Record number (leaf) or page (non-leaf)
    UCHAR       btn_flags;          // Node flags
    UCHAR       btn_data[1];        // Key data
} BTNode;

// Node flags
#define BTN_DUPLICATE    1          // Duplicate key
#define BTN_END_LEVEL    2          // End of level marker
#define BTN_END_BUCKET   4          // End of duplicate bucket
#define BTN_MARKER       8          // Marker node
#define BTN_DELETED      16         // Deleted entry

// B-Tree page split algorithm
BTreePage* BTR_split_page(
    Database*   dbb,
    BTreePage*  page,
    BTNode*     split_node,
    USHORT      split_point)
{
    // Allocate new page
    WIN new_window;
    BTreePage* new_page = (BTreePage*) DPM_allocate(dbb, &new_window);
    
    // Initialize new page header
    new_page->btr_header.pag_type = pag_index;
    new_page->btr_relation_id = page->btr_relation_id;
    new_page->btr_level = page->btr_level;
    new_page->btr_id = page->btr_id;
    
    // Set sibling pointers
    new_page->btr_sibling = page->btr_sibling;
    new_page->btr_left_sibling = page->btr_header.pag_pageno;
    page->btr_sibling = new_page->btr_header.pag_pageno;
    
    // Calculate split position
    USHORT total_size = 0;
    USHORT split_size = (PAGE_SIZE - sizeof(BTreePage)) / 2;
    BTNode* node = page->btr_nodes;
    USHORT split_count = 0;
    
    while (total_size < split_size && split_count < page->btr_count) {
        USHORT node_size = BTN_SIZE(node);
        total_size += node_size;
        node = BTN_NEXT(node);
        split_count++;
    }
    
    // Move nodes to new page
    UCHAR* dest = (UCHAR*) new_page->btr_nodes;
    UCHAR* source = (UCHAR*) node;
    USHORT move_count = page->btr_count - split_count;
    USHORT move_size = (UCHAR*)page + page->btr_header.pag_length - source;
    
    memcpy(dest, source, move_size);
    new_page->btr_count = move_count;
    page->btr_count = split_count;
    
    // Adjust prefix compression for new page
    BTR_recompress_page(new_page);
    
    // Update parent if not root
    if (page->btr_level < MAX_LEVEL) {
        BTR_update_parent(dbb, page, new_page, split_node);
    }
    
    // Mark pages as modified
    CCH_mark(dbb, &window, page->btr_header.pag_generation + 1);
    CCH_mark(dbb, &new_window, new_page->btr_header.pag_generation + 1);
    
    return new_page;
}

// B-Tree page merge algorithm
void BTR_merge_pages(
    Database*   dbb,
    BTreePage*  left_page,
    BTreePage*  right_page)
{
    // Check if pages can be merged
    USHORT left_size = BTR_page_size(left_page);
    USHORT right_size = BTR_page_size(right_page);
    
    if (left_size + right_size > PAGE_SIZE - sizeof(BTreePage)) {
        return;  // Cannot merge - too large
    }
    
    // Copy nodes from right page to left page
    UCHAR* dest = BTR_find_end(left_page);
    UCHAR* source = (UCHAR*) right_page->btr_nodes;
    
    memcpy(dest, source, right_size);
    left_page->btr_count += right_page->btr_count;
    
    // Update sibling pointers
    left_page->btr_sibling = right_page->btr_sibling;
    
    if (right_page->btr_sibling) {
        // Update left sibling of next page
        WIN window;
        BTreePage* next_page = (BTreePage*) CCH_fetch(dbb, &window, 
                                                      right_page->btr_sibling);
        next_page->btr_left_sibling = left_page->btr_header.pag_pageno;
        CCH_mark(dbb, &window, next_page->btr_header.pag_generation + 1);
        CCH_release(dbb, &window);
    }
    
    // Remove right page from parent
    BTR_remove_from_parent(dbb, right_page);
    
    // Release right page
    DPM_release(dbb, right_page->btr_header.pag_pageno);
    
    // Recompress merged page
    BTR_recompress_page(left_page);
}

// B-Tree key insertion
IndexResult BTR_insert_key(
    Database*       dbb,
    IndexDesc*      index,
    IndexKey*       key,
    RecordNumber    record_number)
{
    // Start from root
    ULONG page_number = index->idx_root_page;
    
    while (true) {
        WIN window;
        BTreePage* page = (BTreePage*) CCH_fetch(dbb, &window, page_number);
        
        if (page->btr_level == 0) {
            // Leaf page - insert here
            IndexResult result = BTR_insert_leaf(dbb, page, key, record_number);
            
            if (result == INDEX_SPLIT) {
                // Page split required
                BTNode split_node;
                BTreePage* new_page = BTR_split_page(dbb, page, &split_node, 
                                                     page->btr_count / 2);
                
                // Retry insertion
                if (BTR_key_compare(key, &split_node) < 0) {
                    result = BTR_insert_leaf(dbb, page, key, record_number);
                } else {
                    result = BTR_insert_leaf(dbb, new_page, key, record_number);
                }
            }
            
            CCH_release(dbb, &window);
            return result;
        } else {
            // Non-leaf page - find child
            page_number = BTR_find_child(page, key);
            CCH_release(dbb, &window);
        }
    }
}

// B-Tree key deletion
IndexResult BTR_delete_key(
    Database*       dbb,
    IndexDesc*      index,
    IndexKey*       key,
    RecordNumber    record_number)
{
    // Find leaf page containing key
    BTreePage* page = BTR_find_leaf(dbb, index, key);
    
    if (!page) {
        return INDEX_KEY_NOT_FOUND;
    }
    
    // Find and mark node as deleted
    BTNode* node = BTR_find_node(page, key, record_number);
    
    if (!node) {
        return INDEX_KEY_NOT_FOUND;
    }
    
    node->btn_flags |= BTN_DELETED;
    
    // Check if page is underfull
    if (BTR_page_size(page) < MIN_PAGE_SIZE) {
        // Try to merge with sibling
        if (page->btr_left_sibling) {
            BTreePage* left = BTR_fetch_page(dbb, page->btr_left_sibling);
            BTR_merge_pages(dbb, left, page);
        } else if (page->btr_sibling) {
            BTreePage* right = BTR_fetch_page(dbb, page->btr_sibling);
            BTR_merge_pages(dbb, page, right);
        }
    }
    
    return INDEX_SUCCESS;
}
```

### B-Tree Prefix Compression
```c
// Firebird prefix compression for B-Tree keys
typedef struct prefix_context {
    UCHAR*      last_key;           // Last key for comparison
    USHORT      last_length;        // Last key length
    USHORT      prefix_length;      // Common prefix length
} PrefixContext;

// Compress key using prefix compression
USHORT BTR_compress_key(
    PrefixContext*  context,
    IndexKey*       key,
    UCHAR*          output)
{
    USHORT prefix = 0;
    
    // Calculate common prefix with previous key
    if (context->last_key) {
        prefix = BTR_common_prefix(context->last_key, context->last_length,
                                   key->key_data, key->key_length);
    }
    
    // Store prefix length
    *output++ = prefix;
    
    // Store suffix length
    USHORT suffix_length = key->key_length - prefix;
    *output++ = suffix_length;
    
    // Store suffix data
    memcpy(output, key->key_data + prefix, suffix_length);
    
    // Update context
    context->last_key = key->key_data;
    context->last_length = key->key_length;
    context->prefix_length = prefix;
    
    return 2 + suffix_length;  // Total compressed size
}

// Decompress key
void BTR_decompress_key(
    UCHAR*      compressed,
    UCHAR*      previous_key,
    USHORT      previous_length,
    IndexKey*   output_key)
{
    USHORT prefix_length = *compressed++;
    USHORT suffix_length = *compressed++;
    
    // Copy prefix from previous key
    if (prefix_length > 0) {
        memcpy(output_key->key_data, previous_key, prefix_length);
    }
    
    // Copy suffix from compressed data
    memcpy(output_key->key_data + prefix_length, compressed, suffix_length);
    
    output_key->key_length = prefix_length + suffix_length;
}
```

## Specialized Index Types

### Expression Index
```c
// Firebird expression index
typedef struct expression_index {
    IndexDesc       base_index;      // Base index descriptor
    JRD_NOD*        idx_expression;  // Expression tree
    DSC             idx_result_desc; // Result descriptor
    USHORT          idx_dependencies[MAX_DEPENDENCIES]; // Field dependencies
    USHORT          idx_dep_count;   // Number of dependencies
} ExpressionIndex;

// Evaluate expression for indexing
void EXPR_evaluate_index(
    ExpressionIndex*    index,
    Record*             record,
    IndexKey*           output_key)
{
    // Set up evaluation context
    REQUEST request;
    memset(&request, 0, sizeof(request));
    request.req_record = record;
    
    // Evaluate expression
    DSC* result = EVL_expr(&request, index->idx_expression);
    
    // Convert result to index key
    BTR_make_key(result, output_key, index->idx_result_desc);
    
    // Handle NULL values
    if (request.req_flags & req_null) {
        output_key->key_flags |= KEY_NULL;
    }
}

// Update expression index
void EXPR_update_index(
    Database*           dbb,
    ExpressionIndex*    index,
    Record*             old_record,
    Record*             new_record)
{
    IndexKey old_key, new_key;
    
    if (old_record) {
        // Evaluate old expression
        EXPR_evaluate_index(index, old_record, &old_key);
        
        // Remove old key
        BTR_delete_key(dbb, &index->base_index, &old_key, 
                      old_record->rec_number);
    }
    
    if (new_record) {
        // Evaluate new expression
        EXPR_evaluate_index(index, new_record, &new_key);
        
        // Insert new key
        BTR_insert_key(dbb, &index->base_index, &new_key,
                      new_record->rec_number);
    }
}
```

### Partial Index
```c
// Firebird partial index (filtered index)
typedef struct partial_index {
    IndexDesc       base_index;      // Base index descriptor
    JRD_NOD*        idx_condition;   // Filter condition
    USHORT          idx_selectivity; // Estimated selectivity
} PartialIndex;

// Check if record matches partial index condition
BOOLEAN PARTIAL_matches_condition(
    PartialIndex*   index,
    Record*         record)
{
    // Set up evaluation context
    REQUEST request;
    memset(&request, 0, sizeof(request));
    request.req_record = record;
    
    // Evaluate condition
    DSC* result = EVL_expr(&request, index->idx_condition);
    
    // Check result
    if (request.req_flags & req_null) {
        return FALSE;  // NULL doesn't match
    }
    
    return MOV_get_boolean(result);
}

// Update partial index
void PARTIAL_update_index(
    Database*       dbb,
    PartialIndex*   index,
    Record*         old_record,
    Record*         new_record)
{
    BOOLEAN old_matches = FALSE;
    BOOLEAN new_matches = FALSE;
    
    if (old_record) {
        old_matches = PARTIAL_matches_condition(index, old_record);
    }
    
    if (new_record) {
        new_matches = PARTIAL_matches_condition(index, new_record);
    }
    
    // Update index only if condition match changes
    if (old_matches && !new_matches) {
        // Remove from index
        IndexKey key;
        BTR_make_key_from_record(old_record, index, &key);
        BTR_delete_key(dbb, &index->base_index, &key, old_record->rec_number);
    } else if (!old_matches && new_matches) {
        // Add to index
        IndexKey key;
        BTR_make_key_from_record(new_record, index, &key);
        BTR_insert_key(dbb, &index->base_index, &key, new_record->rec_number);
    } else if (old_matches && new_matches) {
        // Update existing entry
        IndexKey old_key, new_key;
        BTR_make_key_from_record(old_record, index, &old_key);
        BTR_make_key_from_record(new_record, index, &new_key);
        
        if (BTR_key_compare(&old_key, &new_key) != 0) {
            BTR_delete_key(dbb, &index->base_index, &old_key, 
                          old_record->rec_number);
            BTR_insert_key(dbb, &index->base_index, &new_key,
                          new_record->rec_number);
        }
    }
}
```

---

# PostgreSQL Index Implementation

## B-Tree Implementation

### PostgreSQL B-Tree Structure
```c
// B-Tree page structure
typedef struct BTPageOpaqueData {
    BlockNumber btpo_prev;      // Left sibling
    BlockNumber btpo_next;      // Right sibling
    union {
        uint32  level;          // Tree level
        TransactionId xact;     // Deletion XID
    } btpo;
    uint16      btpo_flags;     // Page flags
    BTCycleId   btpo_cycleid;   // Vacuum cycle ID
} BTPageOpaqueData;

// Page flags
#define BTP_LEAF        (1 << 0)    // Leaf page
#define BTP_ROOT        (1 << 1)    // Root page
#define BTP_DELETED     (1 << 2)    // Deleted page
#define BTP_META        (1 << 3)    // Meta page
#define BTP_HALF_DEAD   (1 << 4)    // Half-dead page
#define BTP_SPLIT_END   (1 << 5)    // Rightmost page after split
#define BTP_HAS_GARBAGE (1 << 6)    // Has deletable tuples

// B-Tree index tuple
typedef struct IndexTupleData {
    ItemPointerData t_tid;      // Reference to heap tuple
    unsigned short  t_info;     // Various info
} IndexTupleData;

// B-Tree page split
Buffer
_bt_split(Relation rel, BTScanInsert itup_key, Buffer buf,
          Buffer cbuf, OffsetNumber newitemoff, Size newitemsz,
          IndexTuple newitem)
{
    Page        origpage = BufferGetPage(buf);
    BTPageOpaque origopaque = (BTPageOpaque) PageGetSpecialPointer(origpage);
    Size        itemsz;
    ItemId      itemid;
    IndexTuple  firstright;
    OffsetNumber firstright_off;
    OffsetNumber maxoff;
    OffsetNumber i;
    bool        newitemonleft;
    
    // Find split point using suffix truncation
    firstright_off = _bt_findsplitloc(rel, origpage, newitemoff,
                                      newitemsz, newitem,
                                      &newitemonleft);
    
    // Allocate new right page
    Buffer      rbuf = _bt_getbuf(rel, P_NEW, BT_WRITE);
    Page        rightpage = BufferGetPage(rbuf);
    BTPageOpaque ropaque = (BTPageOpaque) PageGetSpecialPointer(rightpage);
    
    // Initialize right page
    _bt_pageinit(rightpage, BufferGetPageSize(rbuf));
    ropaque->btpo_prev = origopaque->btpo_prev;
    ropaque->btpo_next = origopaque->btpo_next;
    ropaque->btpo.level = origopaque->btpo.level;
    ropaque->btpo_flags = origopaque->btpo_flags;
    ropaque->btpo_flags &= ~BTP_ROOT;
    ropaque->btpo_cycleid = 0;
    
    // Copy items to right page
    maxoff = PageGetMaxOffsetNumber(origpage);
    
    for (i = firstright_off; i <= maxoff; i++) {
        itemid = PageGetItemId(origpage, i);
        itemsz = ItemIdGetLength(itemid);
        IndexTuple item = (IndexTuple) PageGetItem(origpage, itemid);
        
        if (i == newitemoff && !newitemonleft) {
            // Insert new item on right page
            if (PageAddItem(rightpage, (Item) newitem, newitemsz,
                           InvalidOffsetNumber, false, false) == InvalidOffsetNumber) {
                elog(ERROR, "failed to add item to right page after split");
            }
        }
        
        if (PageAddItem(rightpage, (Item) item, itemsz,
                       InvalidOffsetNumber, false, false) == InvalidOffsetNumber) {
            elog(ERROR, "failed to add item to right page after split");
        }
    }
    
    // Truncate left page
    PageIndexTupleDelete(origpage, firstright_off, maxoff - firstright_off + 1);
    
    // Insert new item on left page if needed
    if (newitemonleft) {
        if (PageAddItem(origpage, (Item) newitem, newitemsz,
                       newitemoff, false, false) == InvalidOffsetNumber) {
            elog(ERROR, "failed to add item to left page after split");
        }
    }
    
    // Update sibling links
    origopaque->btpo_next = BufferGetBlockNumber(rbuf);
    ropaque->btpo_prev = BufferGetBlockNumber(buf);
    
    // Create high key for left page (suffix truncation)
    firstright = (IndexTuple) PageGetItem(rightpage,
                                         PageGetItemId(rightpage, P_FIRSTDATAKEY(ropaque)));
    
    IndexTuple lefthikey = _bt_truncate(rel, firstright, origpage, origopaque);
    
    // Insert high key on left page
    if (PageAddItem(origpage, (Item) lefthikey, IndexTupleSize(lefthikey),
                   P_HIKEY, false, false) == InvalidOffsetNumber) {
        elog(ERROR, "failed to add high key to left page");
    }
    
    // Mark pages dirty
    MarkBufferDirty(buf);
    MarkBufferDirty(rbuf);
    
    // Update parent
    _bt_insert_parent(rel, buf, rbuf, stack, is_root, is_only);
    
    return rbuf;
}

// B-Tree page deletion (merge)
void
_bt_pagedel(Relation rel, Buffer leafbuf, TransactionId *oldestBtpoXact)
{
    BlockNumber leafblkno = BufferGetBlockNumber(leafbuf);
    Page        leafpage = BufferGetPage(leafbuf);
    BTPageOpaque leafopaque = (BTPageOpaque) PageGetSpecialPointer(leafpage);
    
    // Can't delete root
    if (P_ISROOT(leafopaque)) {
        return;
    }
    
    // Page must be empty
    if (P_FIRSTDATAKEY(leafopaque) <= PageGetMaxOffsetNumber(leafpage)) {
        return;
    }
    
    // Mark page as half-dead
    leafopaque->btpo_flags |= BTP_HALF_DEAD;
    
    // Find parent and remove downlink
    BTStack     stack = _bt_search(rel, NULL, &leafbuf, BT_WRITE, NULL);
    Buffer      parentbuf = stack->bts_parent;
    Page        parentpage = BufferGetPage(parentbuf);
    BTPageOpaque parentopaque = (BTPageOpaque) PageGetSpecialPointer(parentpage);
    
    // Find and delete downlink in parent
    OffsetNumber parentoff = _bt_binsrch(rel, parentpage, leafblkno);
    PageIndexTupleDelete(parentpage, parentoff, 1);
    
    // Update sibling links
    if (leafopaque->btpo_prev != P_NONE) {
        Buffer prevbuf = _bt_getbuf(rel, leafopaque->btpo_prev, BT_WRITE);
        Page prevpage = BufferGetPage(prevbuf);
        BTPageOpaque prevopaque = (BTPageOpaque) PageGetSpecialPointer(prevpage);
        
        prevopaque->btpo_next = leafopaque->btpo_next;
        MarkBufferDirty(prevbuf);
        _bt_relbuf(rel, prevbuf);
    }
    
    if (leafopaque->btpo_next != P_NONE) {
        Buffer nextbuf = _bt_getbuf(rel, leafopaque->btpo_next, BT_WRITE);
        Page nextpage = BufferGetPage(nextbuf);
        BTPageOpaque nextopaque = (BTPageOpaque) PageGetSpecialPointer(nextpage);
        
        nextopaque->btpo_prev = leafopaque->btpo_prev;
        MarkBufferDirty(nextbuf);
        _bt_relbuf(rel, nextbuf);
    }
    
    // Mark page as deleted
    leafopaque->btpo_flags |= BTP_DELETED;
    leafopaque->btpo.xact = GetCurrentTransactionId();
    
    // Page will be recycled by VACUUM
    MarkBufferDirty(leafbuf);
    MarkBufferDirty(parentbuf);
}
```

### PostgreSQL B-Tree Deduplication
```c
// B-Tree deduplication (PostgreSQL 13+)
typedef struct BTDedupState {
    Size        maxpostingsize;     // Limit on size of posting list
    bool        checkingunique;     // Checking unique constraint?
    int         nmaxitems;          // Max items in posting list
    Size        phystupsize;        // Physical tuple size
    
    // Current pending posting list
    IndexTuple  base;               // Base tuple of posting list
    ItemPointer htids;              // Heap TIDs in posting list
    int         nhtids;             // Number of heap TIDs
    int         nitems;             // Number of items merged
    Size        totalpagesaving;    // Space saved so far
} BTDedupState;

// Create posting list tuple
IndexTuple
_bt_form_posting(IndexTuple base, ItemPointer htids, int nhtids)
{
    Size        keysize;
    Size        newsize;
    IndexTuple  itup;
    
    // Calculate sizes
    if (BTreeTupleIsPosting(base)) {
        keysize = BTreeTupleGetPostingOffset(base);
    } else {
        keysize = IndexTupleSize(base);
    }
    
    newsize = MAXALIGN(keysize + nhtids * sizeof(ItemPointerData));
    
    // Allocate new tuple
    itup = palloc0(newsize);
    memcpy(itup, base, keysize);
    itup->t_info &= ~INDEX_SIZE_MASK;
    itup->t_info |= newsize;
    
    // Mark as posting list
    BTreeTupleSetPosting(itup, nhtids, keysize);
    
    // Copy TIDs
    ItemPointer dest = BTreeTupleGetPosting(itup);
    memcpy(dest, htids, sizeof(ItemPointerData) * nhtids);
    
    // Sort TIDs
    qsort(dest, nhtids, sizeof(ItemPointerData), _bt_compare_tid);
    
    return itup;
}

// Deduplicate items on page
void
_bt_dedup_one_page(Relation rel, Buffer buf, Relation heapRel,
                  IndexTuple newitem, Size newitemsz,
                  bool checkingunique)
{
    Page        page = BufferGetPage(buf);
    BTPageOpaque opaque = (BTPageOpaque) PageGetSpecialPointer(page);
    Page        newpage;
    OffsetNumber offnum,
                minoff,
                maxoff;
    Size        pagesaving = 0;
    bool        singlevalstrat = false;
    
    // Initialize deduplication state
    BTDedupState state;
    state.maxpostingsize = Min(BTMaxItemSize(page) / 2, INDEX_SIZE_MASK);
    state.checkingunique = checkingunique;
    state.nmaxitems = state.maxpostingsize / sizeof(ItemPointerData);
    state.base = NULL;
    state.htids = palloc(state.nmaxitems * sizeof(ItemPointerData));
    state.nhtids = 0;
    state.nitems = 0;
    
    // Create temporary page for deduplicated items
    newpage = PageGetTempPageCopy(page);
    
    // Process each item on page
    minoff = P_FIRSTDATAKEY(opaque);
    maxoff = PageGetMaxOffsetNumber(page);
    
    for (offnum = minoff; offnum <= maxoff; offnum++) {
        ItemId      itemid = PageGetItemId(page, offnum);
        IndexTuple  itup = (IndexTuple) PageGetItem(page, itemid);
        
        if (state.base == NULL) {
            // First item - start new posting list
            state.base = itup;
            state.nhtids = 0;
            
            if (BTreeTupleIsPosting(itup)) {
                // Already a posting list
                ItemPointer htids = BTreeTupleGetPosting(itup);
                int nhtids = BTreeTupleGetNPosting(itup);
                memcpy(state.htids, htids, sizeof(ItemPointerData) * nhtids);
                state.nhtids = nhtids;
            } else {
                // Single tuple
                state.htids[0] = itup->t_tid;
                state.nhtids = 1;
            }
            state.nitems = 1;
        } else {
            // Check if we can merge with current posting list
            if (_bt_keep_natts_fast(rel, state.base, itup) > 0) {
                // Different key - flush current posting list
                IndexTuple  final;
                
                if (state.nitems > 1) {
                    final = _bt_form_posting(state.base, state.htids, state.nhtids);
                    pagesaving += state.phystupsize * state.nitems - IndexTupleSize(final);
                } else {
                    final = state.base;
                }
                
                // Add to new page
                if (PageAddItem(newpage, (Item) final, IndexTupleSize(final),
                               InvalidOffsetNumber, false, false) == InvalidOffsetNumber) {
                    elog(ERROR, "failed to add item during deduplication");
                }
                
                // Start new posting list
                state.base = itup;
                // ... reset state
            } else {
                // Same key - add to posting list
                if (BTreeTupleIsPosting(itup)) {
                    ItemPointer htids = BTreeTupleGetPosting(itup);
                    int nhtids = BTreeTupleGetNPosting(itup);
                    
                    if (state.nhtids + nhtids <= state.nmaxitems) {
                        memcpy(&state.htids[state.nhtids], htids,
                               sizeof(ItemPointerData) * nhtids);
                        state.nhtids += nhtids;
                        state.nitems++;
                    }
                } else {
                    if (state.nhtids < state.nmaxitems) {
                        state.htids[state.nhtids++] = itup->t_tid;
                        state.nitems++;
                    }
                }
            }
        }
    }
    
    // Flush final posting list
    if (state.base != NULL) {
        IndexTuple final;
        
        if (state.nitems > 1) {
            final = _bt_form_posting(state.base, state.htids, state.nhtids);
        } else {
            final = state.base;
        }
        
        PageAddItem(newpage, (Item) final, IndexTupleSize(final),
                   InvalidOffsetNumber, false, false);
    }
    
    // Copy deduplicated page back
    PageRestoreTempPage(newpage, page);
    MarkBufferDirty(buf);
}
```

## Specialized Index Types

### GIN (Generalized Inverted Index)
```c
// GIN index structure for full-text search
typedef struct GinState {
    Relation    index;
    bool        oneCol;             // Single column index?
    
    // Opclass support functions
    FmgrInfo    compareFn[GINMAXATTRS];
    FmgrInfo    extractValueFn[GINMAXATTRS];
    FmgrInfo    extractQueryFn[GINMAXATTRS];
    FmgrInfo    consistentFn[GINMAXATTRS];
    FmgrInfo    triConsistentFn[GINMAXATTRS];
    FmgrInfo    comparePartialFn[GINMAXATTRS];
    
    // Opclass options
    bool        canPartialMatch[GINMAXATTRS];
    Pointer     opclassOptions[GINMAXATTRS];
} GinState;

// GIN posting tree page
typedef struct GinPostingTreeScan {
    Buffer      buffer;
    BlockNumber blkno;
    OffsetNumber offset;
} GinPostingTreeScan;

// Insert entry into GIN index
void
ginInsertValue(GinBtree btree, GinBtreeStack *stack,
              void *insertdata, GinStatsData *buildStats)
{
    GinBtreeData *page;
    BlockNumber rightlink;
    Buffer      childbuf = InvalidBuffer;
    
    // Find leaf page
    stack = ginFindLeafPage(btree, false, false, stack);
    page = BufferGetPage(stack->buffer);
    
    // Try to insert on leaf page
    if (btree->isEnoughSpace(btree, stack->buffer, stack->off)) {
        // Enough space - insert directly
        btree->placeToPage(btree, stack->buffer, stack,
                          insertdata, InvalidBuffer, buildStats);
        return;
    }
    
    // Need to split page
    rightlink = btree->getLeftMostChild(btree, page);
    
    // Split leaf page
    childbuf = ginPageSplit(btree, stack->buffer, stack, insertdata, buildStats);
    
    // Insert downlink for new page in parent
    ginInsertParents(btree, stack, buildStats);
}

// GIN fast insert buffer
typedef struct GinMetaPageData {
    BlockNumber head;               // Head of pending list
    BlockNumber tail;               // Tail of pending list
    uint32      tailFreeSize;       // Free space on tail page
    BlockNumber nPendingPages;      // Number of pending pages
    int64       nPendingHeapTuples; // Number of heap tuples in pending list
} GinMetaPageData;

// Add item to fast insert buffer
void
ginHeapTupleFastInsert(GinState *ginstate, GinTupleCollector *collector)
{
    Relation    index = ginstate->index;
    Buffer      metabuffer;
    Page        metapage;
    GinMetaPageData *metadata;
    
    // Lock meta page
    metabuffer = ReadBuffer(index, GIN_METAPAGE_BLKNO);
    LockBuffer(metabuffer, GIN_EXCLUSIVE);
    metapage = BufferGetPage(metabuffer);
    metadata = GinPageGetMeta(metapage);
    
    // Check if we need to allocate new page
    if (metadata->head == InvalidBlockNumber ||
        metadata->tailFreeSize < collector->sumsize) {
        
        // Allocate new page
        Buffer newbuf = GinNewBuffer(index);
        
        // Link to pending list
        if (metadata->head == InvalidBlockNumber) {
            metadata->head = metadata->tail = BufferGetBlockNumber(newbuf);
        } else {
            // Link to tail
            Buffer tailbuf = ReadBuffer(index, metadata->tail);
            Page tailpage = BufferGetPage(tailbuf);
            
            GinPageGetOpaque(tailpage)->rightlink = BufferGetBlockNumber(newbuf);
            MarkBufferDirty(tailbuf);
            UnlockReleaseBuffer(tailbuf);
            
            metadata->tail = BufferGetBlockNumber(newbuf);
        }
        
        metadata->tailFreeSize = GinDataPageMaxDataSize;
        metadata->nPendingPages++;
    }
    
    // Insert tuples to tail page
    Buffer tailbuf = ReadBuffer(index, metadata->tail);
    Page tailpage = BufferGetPage(tailbuf);
    
    for (int i = 0; i < collector->ntuples; i++) {
        GinPageAddPostingItem(tailpage, &collector->tuples[i]);
        metadata->tailFreeSize -= collector->tuples[i].size;
    }
    
    metadata->nPendingHeapTuples += collector->ntuples;
    
    MarkBufferDirty(tailbuf);
    UnlockReleaseBuffer(tailbuf);
    
    MarkBufferDirty(metabuffer);
    UnlockReleaseBuffer(metabuffer);
}
```

### GiST (Generalized Search Tree)
```c
// GiST index for spatial and other complex data
typedef struct GISTSTATE {
    MemoryContext tempCxt;
    
    FmgrInfo    consistentFn[INDEX_MAX_KEYS];
    FmgrInfo    unionFn[INDEX_MAX_KEYS];
    FmgrInfo    compressFn[INDEX_MAX_KEYS];
    FmgrInfo    decompressFn[INDEX_MAX_KEYS];
    FmgrInfo    penaltyFn[INDEX_MAX_KEYS];
    FmgrInfo    picksplitFn[INDEX_MAX_KEYS];
    FmgrInfo    equalFn[INDEX_MAX_KEYS];
    FmgrInfo    distanceFn[INDEX_MAX_KEYS];
    FmgrInfo    fetchFn[INDEX_MAX_KEYS];
    
    TupleDesc   leafTupdesc;
    TupleDesc   nonLeafTupdesc;
    TupleDesc   fetchTupdesc;
} GISTSTATE;

// GiST page split
SplitedPageLayout *
gistSplit(Relation r, Page page, IndexTuple *itup, int len,
         GISTSTATE *giststate)
{
    IndexTuple *lvectup,
               *rvectup;
    GistSplitVector v;
    int         i;
    SplitedPageLayout *res = NULL;
    
    // Initialize split vector
    v.spl_left = NULL;
    v.spl_nleft = 0;
    v.spl_right = NULL;
    v.spl_nright = 0;
    v.spl_ldatum = PointerGetDatum(NULL);
    v.spl_rdatum = PointerGetDatum(NULL);
    
    // Call user-defined picksplit function
    FunctionCall2Coll(&giststate->picksplitFn[0],
                     giststate->supportCollation[0],
                     PointerGetDatum(&v),
                     PointerGetDatum(itup));
    
    // Create left page entries
    lvectup = (IndexTuple *) palloc(sizeof(IndexTuple) * v.spl_nleft);
    for (i = 0; i < v.spl_nleft; i++) {
        lvectup[i] = itup[v.spl_left[i] - 1];
    }
    
    // Create right page entries
    rvectup = (IndexTuple *) palloc(sizeof(IndexTuple) * v.spl_nright);
    for (i = 0; i < v.spl_nright; i++) {
        rvectup[i] = itup[v.spl_right[i] - 1];
    }
    
    // Create split result
    res = (SplitedPageLayout *) palloc0(sizeof(SplitedPageLayout));
    
    res->block.blkno = InvalidBlockNumber;
    res->block.num = v.spl_nleft;
    res->list = lvectup;
    res->itup = gistFormTuple(giststate, r, v.spl_ldatum, false, false);
    
    res->next = (SplitedPageLayout *) palloc0(sizeof(SplitedPageLayout));
    res->next->block.blkno = InvalidBlockNumber;
    res->next->block.num = v.spl_nright;
    res->next->list = rvectup;
    res->next->itup = gistFormTuple(giststate, r, v.spl_rdatum, false, false);
    
    return res;
}
```

### Hash Index
```c
// PostgreSQL hash index
typedef struct HashMetaPageData {
    uint32      hashm_magic;        // Magic number
    uint32      hashm_version;      // Version number
    double      hashm_ntuples;      // Number of tuples
    uint16      hashm_ffactor;      // Fill factor
    uint16      hashm_bsize;        // Bucket size
    uint16      hashm_bmsize;       // Bitmap size
    uint16      hashm_bmshift;      // Bitmap shift
    uint32      hashm_maxbucket;    // ID of maximum bucket
    uint32      hashm_highmask;     // Mask for high bucket
    uint32      hashm_lowmask;      // Mask for low bucket
    uint32      hashm_ovflpoint;    // Overflow split point
    uint32      hashm_firstfree;    // First free overflow page
    uint32      hashm_nmaps;        // Number of bitmap pages
    RegProcedure hashm_procid;      // Hash function OID
    uint32      hashm_spares[HASH_MAX_SPLITPOINTS]; // Spare pages
    uint32      hashm_mapp[HASH_MAX_BITMAPS];       // Bitmap pages
} HashMetaPageData;

// Hash bucket split
void
_hash_expandtable(Relation rel, Buffer metabuf)
{
    HashMetaPage metap;
    Bucket      old_bucket;
    Bucket      new_bucket;
    uint32      spare_ndx;
    BlockNumber start_oblkno;
    BlockNumber start_nblkno;
    
    metap = HashPageGetMeta(BufferGetPage(metabuf));
    
    // Determine buckets involved in split
    new_bucket = metap->hashm_maxbucket + 1;
    old_bucket = (new_bucket & metap->hashm_lowmask);
    
    // Get starting block numbers
    start_oblkno = BUCKET_TO_BLKNO(metap, old_bucket);
    start_nblkno = BUCKET_TO_BLKNO(metap, new_bucket);
    
    // Allocate new bucket page if needed
    if (start_nblkno >= metap->hashm_nblocks) {
        // Allocate new page
        _hash_alloc_buckets(rel, start_nblkno);
    }
    
    // Split tuples between old and new bucket
    _hash_splitbucket(rel, metabuf, old_bucket, new_bucket,
                     start_oblkno, start_nblkno,
                     metap->hashm_maxbucket,
                     metap->hashm_highmask,
                     metap->hashm_lowmask);
    
    // Update meta page
    metap->hashm_maxbucket = new_bucket;
    
    if (new_bucket > metap->hashm_highmask) {
        // Time to increase masks
        metap->hashm_lowmask = metap->hashm_highmask;
        metap->hashm_highmask = new_bucket | metap->hashm_lowmask;
    }
    
    // Update spare pages if needed
    spare_ndx = _hash_spareindex(new_bucket + 1);
    if (spare_ndx > metap->hashm_ovflpoint) {
        metap->hashm_spares[spare_ndx] = metap->hashm_spares[metap->hashm_ovflpoint];
        metap->hashm_ovflpoint = spare_ndx;
    }
    
    MarkBufferDirty(metabuf);
}
```

---

# MySQL/MariaDB Index Implementation

## InnoDB B-Tree Implementation

### InnoDB B-Tree Structure
```c
// InnoDB B-Tree page structure
typedef struct page_t {
    byte        fil_page_space_id[4];      // Space ID
    byte        fil_page_offset[4];        // Page number
    byte        fil_page_prev[4];          // Previous page
    byte        fil_page_next[4];          // Next page
    byte        fil_page_lsn[8];           // LSN of last modification
    byte        fil_page_type[2];          // Page type
    byte        fil_page_file_flush_lsn[8]; // File flush LSN
    byte        fil_page_arch_log_no[4];   // Archive log number
} page_t;

// B-Tree page header
typedef struct page_header_t {
    byte        page_n_dir_slots[2];       // Number of directory slots
    byte        page_heap_top[2];          // Heap top position
    byte        page_n_heap[2];            // Number of records in heap
    byte        page_free[2];              // Pointer to free record list
    byte        page_garbage[2];           // Garbage space in page
    byte        page_last_insert[2];       // Last insert position
    byte        page_direction[2];         // Last insert direction
    byte        page_n_direction[2];       // Number of inserts in direction
    byte        page_n_recs[2];            // Number of user records
    byte        page_max_trx_id[8];        // Maximum transaction ID
    byte        page_level[2];             // Level in B-Tree
    byte        page_index_id[8];          // Index ID
} page_header_t;

// B-Tree record
typedef struct rec_t {
    byte        info_bits;      // Info bits
    byte        n_owned;        // Number of records owned
    byte        heap_no[2];     // Heap number
    byte        n_fields[2];    // Number of fields
    byte        next_rec[2];    // Pointer to next record
    // Variable length data follows
} rec_t;

// B-Tree page split
rec_t*
btr_page_split_and_insert(
    btr_cur_t*      cursor,
    const dtuple_t* tuple,
    ulint           n_ext,
    mtr_t*          mtr)
{
    page_t*     page = btr_cur_get_page(cursor);
    ulint       page_no = page_get_page_no(page);
    byte        direction = page_get_direction(page);
    page_t*     new_page;
    rec_t*      split_rec;
    rec_t*      first_rec;
    rec_t*      move_limit;
    
    // Decide split direction
    if (direction == PAGE_LEFT) {
        // Split at left
        split_rec = page_rec_get_next(page_get_infimum_rec(page));
        move_limit = page_rec_get_nth(page, page_get_n_recs(page) / 2);
    } else if (direction == PAGE_RIGHT) {
        // Split at right
        split_rec = page_rec_get_nth(page, (page_get_n_recs(page) + 1) / 2);
        move_limit = page_get_supremum_rec(page);
    } else {
        // Split at middle
        split_rec = page_rec_get_nth(page, page_get_n_recs(page) / 2);
        move_limit = page_get_supremum_rec(page);
    }
    
    // Allocate new page
    new_page = btr_page_alloc(cursor->index, mtr);
    
    // Initialize new page
    btr_page_create(new_page, cursor->index, page_get_level(page), mtr);
    
    // Copy records to new page
    first_rec = split_rec;
    while (split_rec != move_limit) {
        rec_t* next_rec = page_rec_get_next(split_rec);
        
        // Copy record to new page
        rec_t* insert_rec = page_cur_insert_rec_low(
            new_page,
            page_cur_get_rec(&new_page_cursor),
            cursor->index,
            split_rec,
            mtr
        );
        
        split_rec = next_rec;
    }
    
    // Delete moved records from old page
    page_delete_rec_list_start(first_rec, page, cursor->index, mtr);
    
    // Update node pointers in parent
    btr_insert_on_non_leaf_level(cursor->index, page_get_level(page) + 1,
                                 tuple, mtr);
    
    // Link pages
    btr_page_set_next(page, page_get_page_no(new_page), mtr);
    btr_page_set_prev(new_page, page_no, mtr);
    
    // Insert tuple
    if (cmp_dtuple_rec(tuple, first_rec, cursor->index) < 0) {
        // Insert on old page
        return page_cur_insert_rec_low(page, cursor->rec, cursor->index,
                                       tuple, mtr);
    } else {
        // Insert on new page
        return page_cur_insert_rec_low(new_page, 
                                       page_cur_get_rec(&new_page_cursor),
                                       cursor->index, tuple, mtr);
    }
}

// B-Tree page merge
ibool
btr_compress(
    btr_cur_t*  cursor,
    ibool       adjust,
    mtr_t*      mtr)
{
    page_t*     page = btr_cur_get_page(cursor);
    dict_index_t* index = cursor->index;
    page_t*     merge_page;
    page_t*     father_page;
    ulint       space = page_get_space_id(page);
    ulint       left_page_no;
    ulint       right_page_no;
    
    // Can't merge root
    if (btr_page_get_level(page) == ULINT_MAX) {
        return FALSE;
    }
    
    // Get neighbor pages
    left_page_no = btr_page_get_prev(page);
    right_page_no = btr_page_get_next(page);
    
    // Try merge with left neighbor
    if (left_page_no != FIL_NULL) {
        merge_page = btr_page_get(space, left_page_no, RW_X_LATCH, mtr);
        
        if (page_get_n_recs(page) + page_get_n_recs(merge_page) <
            BTR_COMPRESS_LIMIT) {
            
            // Move all records from page to merge_page
            rec_t* rec = page_rec_get_next(page_get_infimum_rec(page));
            
            while (!page_rec_is_supremum(rec)) {
                rec_t* next_rec = page_rec_get_next(rec);
                
                page_cur_insert_rec_low(merge_page,
                                       page_get_supremum_rec(merge_page),
                                       index, rec, mtr);
                
                rec = next_rec;
            }
            
            // Update links
            btr_page_set_next(merge_page, right_page_no, mtr);
            if (right_page_no != FIL_NULL) {
                page_t* right_page = btr_page_get(space, right_page_no,
                                                  RW_X_LATCH, mtr);
                btr_page_set_prev(right_page, left_page_no, mtr);
            }
            
            // Remove page from parent
            btr_node_ptr_delete(index, page, mtr);
            
            // Free page
            btr_page_free(index, page, mtr);
            
            return TRUE;
        }
    }
    
    // Try merge with right neighbor
    // ... similar logic
    
    return FALSE;
}
```

### InnoDB Adaptive Hash Index
```c
// Adaptive hash index for frequently accessed pages
typedef struct btr_search_sys_t {
    hash_table_t**  hash_tables;    // Hash tables
    ulint           n_hash_tables;  // Number of hash tables
    
    // Statistics
    ulint           n_hash_succ;    // Successful hash searches
    ulint           n_hash_fail;    // Failed hash searches
    ulint           n_patt_succ;    // Successful pattern searches
    ulint           n_searches;     // Total searches
} btr_search_sys_t;

// Build adaptive hash index for page
void
btr_search_build_page_hash_index(
    dict_index_t*   index,
    page_t*         page,
    ulint           n_fields,
    ulint           n_bytes,
    ibool           left_side)
{
    rec_t*          rec;
    rec_t*          next_rec;
    ulint           fold;
    ulint           next_fold;
    ulint           n_cached = 0;
    ulint           n_recs = page_get_n_recs(page);
    hash_table_t*   table;
    
    // Check if worth building
    if (n_recs < BTR_SEARCH_BUILD_LIMIT) {
        return;
    }
    
    // Get hash table
    table = btr_get_search_table(index);
    
    // Build hash index
    rec = page_rec_get_next(page_get_infimum_rec(page));
    
    while (!page_rec_is_supremum(rec)) {
        next_rec = page_rec_get_next(rec);
        
        // Calculate fold (hash value)
        fold = rec_fold(rec, n_fields, n_bytes, index);
        
        if (!page_rec_is_supremum(next_rec)) {
            next_fold = rec_fold(next_rec, n_fields, n_bytes, index);
        }
        
        // Insert to hash table
        ha_insert_for_fold(table, fold, page, rec);
        n_cached++;
        
        rec = next_rec;
    }
    
    // Update statistics
    index->search_info->n_hash_potential += n_cached;
}

// Search using adaptive hash
rec_t*
btr_search_guess_on_hash(
    dict_index_t*   index,
    btr_search_t*   info,
    const dtuple_t* tuple,
    ulint           mode,
    ulint           latch_mode,
    btr_cur_t*      cursor,
    mtr_t*          mtr)
{
    hash_table_t*   table;
    rec_t*          rec;
    ulint           fold;
    
    // Calculate fold
    fold = dtuple_fold(tuple, info->n_fields, info->n_bytes, index);
    
    // Get hash table
    table = btr_get_search_table(index);
    
    // Search in hash table
    rw_lock_s_lock(&table->latch);
    
    rec = ha_search_and_get_data(table, fold);
    
    if (rec == NULL) {
        rw_lock_s_unlock(&table->latch);
        info->n_hash_fail++;
        return NULL;
    }
    
    // Verify record matches
    if (cmp_dtuple_rec(tuple, rec, index) != 0) {
        rw_lock_s_unlock(&table->latch);
        info->n_hash_fail++;
        return NULL;
    }
    
    // Success
    info->n_hash_succ++;
    
    // Position cursor
    btr_cur_position(index, rec, cursor);
    
    rw_lock_s_unlock(&table->latch);
    
    return rec;
}
```

---

# Microsoft SQL Server Index Implementation

## B-Tree Implementation

### SQL Server B-Tree Structure
```c
// SQL Server B-Tree page structure
typedef struct PAGE_HEADER {
    BYTE        m_headerVersion;    // Header version
    BYTE        m_type;            // Page type
    BYTE        m_typeFlagBits;    // Type flag bits
    BYTE        m_level;           // Level in B-Tree
    USHORT      m_flagBits;        // Page flags
    USHORT      m_indexId;         // Index ID
    USHORT      m_prevPage[3];     // Previous page (6 bytes for page ID)
    USHORT      m_nextPage[3];     // Next page
    USHORT      m_slotCnt;         // Number of slots
    USHORT      m_freeCnt;         // Free space count
    USHORT      m_freeData;        // Free space offset
    USHORT      m_reservedCnt;     // Reserved count
    LSN         m_lsn;             // Log sequence number
    USHORT      m_xactReserved;    // Transaction reserved space
    USHORT      m_xdesId[3];       // XDesID
    USHORT      m_ghostRecCnt;     // Ghost record count
} PAGE_HEADER;

// Page types
#define PG_DATA         1   // Data page
#define PG_INDEX        2   // Index page
#define PG_TEXT         3   // Text/Image page
#define PG_IAM          10  // Index Allocation Map
#define PG_GAM          8   // Global Allocation Map
#define PG_SGAM         9   // Shared Global Allocation Map
#define PG_PFS          11  // Page Free Space
#define PG_BCM          16  // Bulk Changed Map
#define PG_DCM          17  // Differential Changed Map

// B-Tree key structure
typedef struct KEY_RECORD {
    USHORT      m_statusBits;      // Status bits
    USHORT      m_nullBitmap;      // NULL bitmap offset
    USHORT      m_varOffset;       // Variable column offset
    USHORT      m_keySize;         // Key size
    // Key columns follow
    // Child page pointer (non-leaf)
    // RID (leaf)
} KEY_RECORD;

// Page split algorithm
PAGE_ID
btree_split_page(
    INDEX_CONTEXT*  context,
    PAGE_ID         page_id,
    KEY_RECORD*     new_key)
{
    PAGE*           page = get_page(page_id);
    PAGE*           new_page;
    KEY_RECORD*     split_key;
    USHORT          split_point;
    USHORT          total_size = 0;
    USHORT          target_size = (PAGE_SIZE - sizeof(PAGE_HEADER)) / 2;
    
    // Find split point
    for (split_point = 0; split_point < page->m_slotCnt; split_point++) {
        KEY_RECORD* key = get_key_record(page, split_point);
        total_size += key->m_keySize;
        
        if (total_size >= target_size) {
            break;
        }
    }
    
    // Allocate new page
    new_page = allocate_page(context->index_id, page->m_level);
    
    // Move keys to new page
    for (USHORT i = split_point; i < page->m_slotCnt; i++) {
        KEY_RECORD* key = get_key_record(page, i);
        insert_key_to_page(new_page, key);
    }
    
    // Remove moved keys from original page
    page->m_slotCnt = split_point;
    
    // Update page links
    new_page->m_prevPage = page_id;
    new_page->m_nextPage = page->m_nextPage;
    page->m_nextPage = get_page_id(new_page);
    
    // Get middle key for parent
    split_key = get_key_record(new_page, 0);
    
    // Insert pointer in parent
    if (page->m_level < context->index_depth - 1) {
        insert_to_parent(context, page_id, split_key, get_page_id(new_page));
    } else {
        // Create new root
        create_new_root(context, page_id, split_key, get_page_id(new_page));
    }
    
    // Decide which page to insert new key
    if (compare_keys(new_key, split_key) < 0) {
        insert_key_to_page(page, new_key);
        return page_id;
    } else {
        insert_key_to_page(new_page, new_key);
        return get_page_id(new_page);
    }
}

// Page merge algorithm
void
btree_merge_pages(
    INDEX_CONTEXT*  context,
    PAGE_ID         left_page_id,
    PAGE_ID         right_page_id)
{
    PAGE*   left_page = get_page(left_page_id);
    PAGE*   right_page = get_page(right_page_id);
    
    // Check if pages can be merged
    USHORT left_size = calculate_page_size(left_page);
    USHORT right_size = calculate_page_size(right_page);
    
    if (left_size + right_size > PAGE_SIZE - sizeof(PAGE_HEADER)) {
        return;  // Cannot merge
    }
    
    // Move all keys from right to left
    for (USHORT i = 0; i < right_page->m_slotCnt; i++) {
        KEY_RECORD* key = get_key_record(right_page, i);
        insert_key_to_page(left_page, key);
    }
    
    // Update page links
    left_page->m_nextPage = right_page->m_nextPage;
    
    if (right_page->m_nextPage != NULL_PAGE) {
        PAGE* next_page = get_page(right_page->m_nextPage);
        next_page->m_prevPage = left_page_id;
    }
    
    // Remove right page from parent
    remove_from_parent(context, right_page_id);
    
    // Deallocate right page
    deallocate_page(right_page_id);
}
```

### SQL Server Columnstore Index
```c
// Columnstore index structure
typedef struct COLUMN_SEGMENT {
    ULONG       row_count;          // Number of rows
    ULONG       min_data_id;        // Minimum data ID
    ULONG       max_data_id;        // Maximum data ID
    BYTE        encoding_type;      // Encoding type
    BYTE        compression_type;   // Compression type
    ULONG       dictionary_id;      // Dictionary ID
    ULONG       data_size;          // Compressed data size
    BYTE*       data;               // Compressed data
} COLUMN_SEGMENT;

typedef struct ROWGROUP {
    ULONG           rowgroup_id;    // Rowgroup ID
    ULONG           row_count;      // Number of rows
    BYTE            state;          // State (OPEN, CLOSED, COMPRESSED)
    COLUMN_SEGMENT* segments;       // Array of column segments
    ULONG           segment_count;  // Number of segments
} ROWGROUP;

// Rowgroup states
#define RG_OPEN         1   // Accepting new rows
#define RG_CLOSED       2   // Full, awaiting compression
#define RG_COMPRESSED   3   // Compressed
#define RG_TOMBSTONE    4   // Marked for deletion

// Compress rowgroup
void
compress_rowgroup(ROWGROUP* rowgroup)
{
    if (rowgroup->state != RG_CLOSED) {
        return;
    }
    
    // Compress each column segment
    for (ULONG i = 0; i < rowgroup->segment_count; i++) {
        COLUMN_SEGMENT* segment = &rowgroup->segments[i];
        
        // Build dictionary if beneficial
        DICTIONARY* dict = NULL;
        if (should_use_dictionary(segment)) {
            dict = build_dictionary(segment);
            segment->dictionary_id = store_dictionary(dict);
        }
        
        // Choose encoding
        segment->encoding_type = choose_encoding(segment);
        
        // Encode data
        BYTE* encoded_data = encode_segment(segment, dict);
        
        // Compress encoded data
        segment->compression_type = COMPRESSION_XPRESS;
        segment->data = compress_data(encoded_data, 
                                     segment->data_size,
                                     &segment->data_size);
        
        free(encoded_data);
    }
    
    rowgroup->state = RG_COMPRESSED;
}

// Tuple mover process
void
tuple_mover_process(COLUMNSTORE_INDEX* index)
{
    while (true) {
        // Find closed rowgroups
        for (ULONG i = 0; i < index->rowgroup_count; i++) {
            ROWGROUP* rg = &index->rowgroups[i];
            
            if (rg->state == RG_CLOSED) {
                // Compress rowgroup
                compress_rowgroup(rg);
                
                // Update metadata
                update_rowgroup_metadata(index, rg);
            }
        }
        
        // Find open rowgroups that should be closed
        for (ULONG i = 0; i < index->rowgroup_count; i++) {
            ROWGROUP* rg = &index->rowgroups[i];
            
            if (rg->state == RG_OPEN) {
                if (rg->row_count >= MAX_ROWGROUP_SIZE ||
                    time_since_last_insert(rg) > ROWGROUP_TIMEOUT) {
                    
                    rg->state = RG_CLOSED;
                }
            }
        }
        
        // Sleep
        sleep(TUPLE_MOVER_INTERVAL);
    }
}

// Batch mode processing
void
batch_mode_scan(
    COLUMNSTORE_INDEX*  index,
    PREDICATE*          predicate,
    BATCH*              output_batch)
{
    // Process each compressed rowgroup
    for (ULONG rg_id = 0; rg_id < index->rowgroup_count; rg_id++) {
        ROWGROUP* rg = &index->rowgroups[rg_id];
        
        if (rg->state != RG_COMPRESSED) {
            continue;
        }
        
        // Check rowgroup elimination
        if (can_eliminate_rowgroup(rg, predicate)) {
            continue;
        }
        
        // Process each column segment
        BATCH* batch = allocate_batch(BATCH_SIZE);
        
        for (ULONG col = 0; col < rg->segment_count; col++) {
            COLUMN_SEGMENT* segment = &rg->segments[col];
            
            // Decompress segment
            BYTE* decompressed = decompress_segment(segment);
            
            // Decode values
            decode_segment_to_batch(decompressed, segment, batch, col);
            
            free(decompressed);
        }
        
        // Apply predicate
        apply_predicate_to_batch(batch, predicate);
        
        // Add qualifying rows to output
        append_batch(output_batch, batch);
        
        free_batch(batch);
    }
}
```

### SQL Server Memory-Optimized Index
```c
// Memory-optimized (Hekaton) index structures

// Bw-Tree for memory-optimized tables
typedef struct BW_NODE {
    ULONG       node_size;          // Node size
    ULONG       split_size;         // Split threshold
    BYTE        node_type;          // LEAF or INTERNAL
    ULONG       epoch;              // Epoch for SMO
    void*       delta_chain;        // Delta record chain
    union {
        struct {  // Leaf node
            ULONG   record_count;
            void**  records;        // Array of record pointers
        } leaf;
        struct {  // Internal node
            ULONG   separator_count;
            void**  separators;     // Key separators
            void**  children;       // Child pointers
        } internal;
    };
} BW_NODE;

// Delta record types
typedef enum {
    DELTA_INSERT,
    DELTA_DELETE,
    DELTA_UPDATE,
    DELTA_SPLIT,
    DELTA_MERGE,
    DELTA_CONSOLIDATE
} DELTA_TYPE;

// Delta record
typedef struct DELTA_RECORD {
    DELTA_TYPE          type;
    struct DELTA_RECORD* next;
    ULONG               epoch;
    union {
        struct {
            void*   key;
            void*   record;
        } insert;
        struct {
            void*   key;
        } delete;
        struct {
            BW_NODE* new_sibling;
            void*    separator;
        } split;
    };
} DELTA_RECORD;

// Insert into Bw-Tree
void
bwtree_insert(BW_TREE* tree, void* key, void* record)
{
    BW_NODE* node = find_leaf_node(tree, key);
    
    // Create insert delta
    DELTA_RECORD* delta = allocate_delta();
    delta->type = DELTA_INSERT;
    delta->insert.key = key;
    delta->insert.record = record;
    delta->epoch = get_current_epoch();
    
    // CAS to install delta
    while (true) {
        delta->next = node->delta_chain;
        
        if (CAS(&node->delta_chain, delta->next, delta)) {
            break;
        }
    }
    
    // Check if consolidation needed
    if (count_deltas(node) > DELTA_CHAIN_LIMIT) {
        consolidate_node(tree, node);
    }
    
    // Check if split needed
    if (calculate_node_size(node) > node->split_size) {
        split_node(tree, node);
    }
}

// Hash index for memory-optimized tables
typedef struct HASH_BUCKET {
    void*   head;               // Head of chain
    ULONG   lock;              // Bucket lock (optimistic)
} HASH_BUCKET;

typedef struct MEM_OPT_HASH_INDEX {
    ULONG           bucket_count;   // Number of buckets
    HASH_BUCKET*    buckets;       // Bucket array
    ULONG           record_count;   // Number of records
    double          load_factor;    // Current load factor
} MEM_OPT_HASH_INDEX;

// Insert into hash index
void
hash_index_insert(MEM_OPT_HASH_INDEX* index, void* key, void* record)
{
    ULONG hash = compute_hash(key);
    ULONG bucket_id = hash % index->bucket_count;
    HASH_BUCKET* bucket = &index->buckets[bucket_id];
    
    // Optimistic insertion
    void* old_head;
    do {
        old_head = bucket->head;
        record->next = old_head;
    } while (!CAS(&bucket->head, old_head, record));
    
    // Update statistics
    atomic_increment(&index->record_count);
    
    // Check if rehash needed
    if (index->record_count > index->bucket_count * MAX_LOAD_FACTOR) {
        initiate_rehash(index);
    }
}
```