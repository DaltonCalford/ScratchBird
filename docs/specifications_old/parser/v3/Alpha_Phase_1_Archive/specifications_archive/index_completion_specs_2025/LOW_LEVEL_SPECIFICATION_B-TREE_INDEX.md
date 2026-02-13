# **ScratchBird: B-Tree Low-Level Implementation Specification**

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Version 1.0  
Last Updated: September 15, 2025

## **1\. Overview & Use Case**

This document provides a complete low-level technical specification for the ScratchBird B-Tree index subsystem. This index is the default and most versatile type, optimized for a wide range of workloads including point lookups (=), range scans (\<, \>, BETWEEN), and sorted retrieval (ORDER BY). The implementation emphasizes high concurrency, efficient space utilization through compression, and specialized optimizations for time-series data using UUIDv7.

This document leaves no ambiguity for implementation. All data structures, on-disk layouts, and core algorithms are defined herein.

## **2\. Foundational Data Structures (C++)**

To provide a concrete implementation context, the following C++ types are assumed. These are simplified but sufficient for defining the index structure.

\#include \<cstdint\>  
\#include \<cstring\>  
\#include \<vector\>

// Represents a physical location of a tuple on a heap page.  
struct TupleId {  
    uint32\_t page\_number;  
    uint16\_t item\_offset;  
};

// A generic representation of a data value.  
// The actual data is stored externally and pointed to by \`data\`.  
struct Datum {  
    void\* data;  
    uint16\_t length;  
};

// Represents a unique 128-bit identifier, compatible with UUIDs.  
struct UUID {  
    uint8\_t bytes\[16\];  
};

// Represents a transaction identifier.  
using TransactionId \= uint64\_t;

// The standard 48-byte header for every page on disk.  
struct PageHeader {  
    uint64\_t page\_lsn;          // Log Sequence Number for WAL recovery  
    uint32\_t page\_checksum;     // Checksum of the page contents  
    uint16\_t page\_flags;        // Flags like PAGE\_IS\_GARBAGED  
    // ... other header fields  
};

## **3\. On-Disk Page and Node Structures**

### **3.1. B-Tree Page Layout**

The B-Tree is composed of fixed-size pages (e.g., 8KB, 16KB). Each page contains a fixed-size header, followed by node data that grows from the end of the page towards the beginning. The contiguous space between the header and the first node is the free space.

**Visual Layout:**

\+-------------------------------------------------------------------------+  
| SBBTreePage Header (Fixed Size)                                         |  
|-------------------------------------------------------------------------|  
|                                                                         |  
|                        Free Space (Contiguous)                          |  
|                                                                         |  
|-------------------------------------------------------------------------|  
| Node N (Variable Size) | ... | Node 2 (Var Size) | Node 1 (Var Size)     |  
\+-------------------------------------------------------------------------+  
^ Page Start                                                        ^ Page End

### **3.2. B-Tree Page Header (C++)**

This structure is the fixed-size header at the start of every B-Tree page.

// ScratchBird B-Tree page structure (fixed-size header)  
struct SBBTreePage {  
    PageHeader      btr\_header;         // Standard ScratchBird page header (48 bytes)  
    UUID            btr\_index\_uuid;     // Index UUID v7 (16 bytes)  
    UUID            btr\_table\_uuid;     // Table this index belongs to (16 bytes)  
    uint16\_t        btr\_level;          // Level (0 \= leaf, increases upward)  
    uint16\_t        btr\_flags;          // Page flags (see enum below)  
    uint16\_t        btr\_count;          // Number of nodes on this page  
    uint16\_t        btr\_free\_space;     // Free space in bytes  
    uint64\_t        btr\_left\_sibling;   // Left sibling page number (0 if none)  
    uint64\_t        btr\_right\_sibling;  // Right sibling page number (0 if none)  
    TransactionId   btr\_xmin;           // Page creation transaction  
    TransactionId   btr\_xmax;           // Page deletion transaction (0 if active)  
    uint16\_t        btr\_lower;          // Offset to start of free space  
    uint16\_t        btr\_upper;          // Offset to end of free space (start of first node)  
};

// Page flags  
enum SBBTreePageFlags : uint16\_t {  
    BTR\_FLAG\_LEAF           \= 0x0001,  // Leaf page  
    BTR\_FLAG\_ROOT           \= 0x0002,  // Root page  
    BTR\_FLAG\_RIGHTMOST      \= 0x0004,  // Rightmost page at this level  
    BTR\_FLAG\_HAS\_GARBAGE    \= 0x0040,  // Page has deleted entries needing cleanup  
    BTR\_FLAG\_INCOMPLETE\_SPLIT \= 0x0080, // A split was interrupted  
};

### **3.3. B-Tree Node Structure (C++)**

Nodes are variable-length and are packed at the end of the page. The structure below defines the fixed-size header of each node. The key and data follow immediately after.

// B-Tree node header (fixed part)  
struct SBBTreeNode {  
    uint16\_t        btn\_flags;          // Node flags (see enum below)  
    uint16\_t        btn\_prefix\_len;     // Prefix compression length  
    uint16\_t        btn\_key\_len;        // Actual key length stored (after compression)  
      
    // Union for data payload. The type of data depends on the page level.  
    union {  
        // For LEAF pages (btr\_level \== 0\)  
        TupleId     leaf\_tuple\_id;

        // For INTERNAL pages (btr\_level \> 0\)  
        uint64\_t    internal\_child\_page;  
    } payload;  
      
    // Variable length key data starts here: uint8\_t key\_data\[btn\_key\_len\];  
};

// Node flags  
enum SBBTreeNodeFlags : uint16\_t {  
    BTN\_FLAG\_DELETED        \= 0x0001,  // Logically deleted  
    BTN\_FLAG\_NULL\_KEY       \= 0x0010,  // Key is NULL  
};

// Helper to calculate total node size  
inline uint16\_t calculate\_node\_size(const SBBTreeNode\* node) {  
    return sizeof(SBBTreeNode) \+ node-\>btn\_key\_len;  
}

## **4\. Core Algorithms (C++)**

This section provides C++ implementations for the most critical B-Tree operations.

### **4.1. Page Search**

A binary search is used to efficiently find a key within a sorted page.

// Performs a binary search for a key on a single page.  
// Returns the index of the node. If not found, returns the insertion point.  
int find\_node\_in\_page(const SBBTreePage\* page, const Datum& key,  
                      int (\*compare\_func)(const Datum&, const Datum&)) {  
    int low \= 0;  
    int high \= page-\>btr\_count \- 1;  
    int result\_idx \= 0;

    while (low \<= high) {  
        int mid\_idx \= low \+ (high \- low) / 2;  
          
        char\* node\_ptr \= (char\*)page \+ page-\>btr\_upper \+ (mid\_idx \* sizeof(SBBTreeNode)); // Simplified; needs real offsets  
        SBBTreeNode\* mid\_node \= (SBBTreeNode\*)node\_ptr;

        Datum mid\_key;  
        mid\_key.data \= (void\*)(mid\_node \+ 1); // Key data follows the node header  
        mid\_key.length \= mid\_node-\>btn\_key\_len;  
        // NOTE: A real implementation must decompress the key before comparing.  
          
        int cmp \= compare\_func(key, mid\_key);

        if (cmp \== 0\) {  
            return mid\_idx; // Exact match  
        } else if (cmp \< 0\) {  
            high \= mid\_idx \- 1;  
        } else {  
            low \= mid\_idx \+ 1;  
        }  
    }  
    return low; // Insertion point  
}

### **4.2. Page Insertion and Split Logic**

This is the most complex operation. The function btree\_insert will recursively descend the tree and, upon returning, may trigger a page split.

// High-level logic for insertion that leads to a page split.  
// This is a simplified representation of the full recursive process.

// Step 1: Attempt to insert into a leaf page.  
bool insert\_into\_leaf(SBBTreePage\* page, const Datum& key, const TupleId& tid) {  
    uint16\_t required\_space \= sizeof(SBBTreeNode) \+ key.length;  
    if (page-\>btr\_free\_space \< required\_space) {  
        return false; // Not enough space, caller must handle split.  
    }

    int insertion\_point \= find\_node\_in\_page(page, key, /\* ... \*/);  
      
    // Shift existing nodes to make space for the new one.  
    // ... memcpy logic to move nodes from insertion\_point onward ...

    // Create and place the new node.  
    SBBTreeNode new\_node;  
    new\_node.btn\_flags \= 0;  
    new\_node.btn\_prefix\_len \= 0; // Simplified; compression calculated here.  
    new\_node.btn\_key\_len \= key.length;  
    new\_node.payload.leaf\_tuple\_id \= tid;  
      
    // ... memcpy new\_node and key data into the correct position ...

    page-\>btr\_count++;  
    page-\>btr\_free\_space \-= required\_space;  
    return true;  
}

// Step 2: Handle the page split when insert\_into\_leaf returns false.  
void split\_leaf\_page(SBBTreePage\* left\_page, const Datum& new\_key, const TupleId& new\_tid) {  
    // 1\. Allocate a new page (the right\_page).  
    SBBTreePage\* right\_page \= allocate\_new\_page();  
      
    // 2\. Determine the split point. Find the median key among all existing  
    //    nodes plus the new node to be inserted.  
    int total\_nodes \= left\_page-\>btr\_count \+ 1;  
    int split\_idx \= total\_nodes / 2;

    // 3\. Move the second half of the nodes from left\_page to right\_page.  
    // ... complex memcpy and pointer management logic ...  
      
    // 4\. Determine which page the new key/TID pair belongs to and insert it.

    // 5\. Update sibling pointers.  
    right\_page-\>btr\_right\_sibling \= left\_page-\>btr\_right\_sibling;  
    right\_page-\>btr\_left\_sibling \= get\_page\_number(left\_page);  
    left\_page-\>btr\_right\_sibling \= get\_page\_number(right\_page);

    // 6\. Get the separator key (the first key on the right\_page).  
    Datum separator\_key \= get\_first\_key(right\_page);  
      
    // 7\. Insert the separator key into the parent page. This is a recursive  
    //    call that may cause the parent page to split as well.  
    insert\_into\_parent(left\_page, separator\_key, right\_page);  
}

### **4.3. Prefix/Suffix Key Compression**

Compression is critical for performance and storage efficiency. It is applied when a node is added to a page.

// Calculates the length of the common prefix between two keys.  
uint16\_t calculate\_prefix\_length(const Datum& key1, const Datum& key2) {  
    uint16\_t len \= std::min(key1.length, key2.length);  
    uint16\_t prefix \= 0;  
    const uint8\_t\* k1 \= static\_cast\<const uint8\_t\*\>(key1.data);  
    const uint8\_t\* k2 \= static\_cast\<const uint8\_t\*\>(key2.data);  
      
    while (prefix \< len && k1\[prefix\] \== k2\[prefix\]) {  
        prefix++;  
    }  
    return prefix;  
}

// Creates a compressed key to be stored in a node.  
void create\_compressed\_node(SBBTreeNode\* target\_node, const Datum& key\_to\_add, const Datum& previous\_key) {  
    uint16\_t prefix \= calculate\_prefix\_length(key\_to\_add, previous\_key);  
      
    target\_node-\>btn\_prefix\_len \= prefix;  
    target\_node-\>btn\_key\_len \= key\_to\_add.length \- prefix;  
      
    // Copy only the suffix (the part of the key after the common prefix)  
    // into the storage area immediately following the target\_node header.  
    memcpy(target\_node \+ 1,   
           static\_cast\<const uint8\_t\*\>(key\_to\_add.data) \+ prefix,  
           target\_node-\>btn\_key\_len);  
}

## **5\. UUIDv7 Time-Series Optimizations**

For indexes on UUIDv7 keys, which are time-ordered, we can significantly optimize range scans by storing time bounds in the page header.

// Add these fields to the SBBTreePage struct:  
// uint64\_t btr\_min\_timestamp\_ms; // Minimum timestamp on the page  
// uint64\_t btr\_max\_timestamp\_ms; // Maximum timestamp on the page

// Extracts the 48-bit timestamp from a UUIDv7.  
uint64\_t extract\_uuid\_v7\_timestamp(const UUID& uuid) {  
    uint64\_t ts \= 0;  
    ts |= static\_cast\<uint64\_t\>(uuid.bytes\[0\]) \<\< 40;  
    ts |= static\_cast\<uint64\_t\>(uuid.bytes\[1\]) \<\< 32;  
    ts |= static\_cast\<uint64\_t\>(uuid.bytes\[2\]) \<\< 24;  
    ts |= static\_cast\<uint64\_t\>(uuid.bytes\[3\]) \<\< 16;  
    ts |= static\_cast\<uint64\_t\>(uuid.bytes\[4\]) \<\< 8;  
    ts |= static\_cast\<uint64\_t\>(uuid.bytes\[5\]);  
    return ts;  
}

// During a search for a time range (e.g., WHERE ts \> T1 AND ts \< T2),  
// the query planner can use these bounds to prune entire sub-trees from the search.  
bool page\_is\_relevant(const SBBTreePage\* page, uint64\_t start\_time, uint64\_t end\_time) {  
    // If the page's max time is before our search start, skip it.  
    if (page-\>btr\_max\_timestamp\_ms \< start\_time) return false;  
      
    // If the page's min time is after our search end, skip it.  
    if (page-\>btr\_min\_timestamp\_ms \> end\_time) return false;  
      
    return true; // The page's time range overlaps with the query range.  
}  
