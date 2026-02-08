# **ScratchBird: GIN Low-Level Implementation Specification**

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Version 1.0  
Last Updated: September 15, 2025

## **1\. Overview & Use Case**

This document provides a complete low-level technical specification for the ScratchBird **G**eneralized **I**nverted **I**ndex (GIN). This index type is designed for indexing composite or multi-valued types, where a single table entry can contain many indexable items. Common use cases include:

* Indexing arrays (e.g., finding all users with 'reading' in their hobbies array).  
* Full-text search (e.g., finding all documents containing the word 'database').  
* Indexing key-value types like JSONB.

A GIN index is "inverted" because it maps items (keys) to the tuples that contain them, rather than mapping tuples to their keys.

## **2\. Core Concepts and Architecture**

A GIN index has a two-tiered structure:

1. **Keys B-Tree:** A standard B-Tree (as specified in BTREE\_LOW\_LEVEL\_SPEC.md) that stores all unique items (e.g., every unique word ever seen). The "key" is the item itself.  
2. **Posting Tree / Posting List:** The "value" part of a leaf entry in the Keys B-Tree is not a TupleId. Instead, it's a pointer to a separate data structure that contains a sorted list of all TupleIds where that item appears. This can be a B-Tree of TupleIds (a "posting tree") or a simple sorted list on a page ("posting list"). We will use a B-Tree for scalability.

**Fast Update Strategy:** Directly updating the Keys B-Tree and many Posting Trees for a single row insertion can be slow. GIN mitigates this with a **pending list**—a temporary, unsorted list of new (item, TupleId) pairs. This list is processed and merged into the main index in batches.

## **3\. On-Disk Page Structures (C++)**

A GIN index uses several types of pages.

// The single metadata page for the entire GIN index.  
struct SBGinIndexMetaPage {  
    PageHeader      gin\_header;  
    UUID            gin\_index\_uuid;  
    uint64\_t        gin\_keys\_btree\_root; // Root page of the Keys B-Tree  
    uint64\_t        gin\_pending\_list\_head; // Head of the pending list pages  
    uint64\_t        gin\_pending\_list\_tail; // Tail of the pending list  
    uint64\_t        gin\_pending\_list\_count; // Number of entries in the pending list  
};

// A page for the pending list. This is a simple append-only log.  
struct SBGinPendingListPage {  
    PageHeader      gpp\_header;  
    uint64\_t        gpp\_next\_page; // Next page in the chain  
    uint16\_t        gpp\_entry\_count;  
    // Followed by a list of PendingEntry structs.  
};

struct PendingEntry {  
    TupleId         tid;  
    Datum           key; // The key/item  
};

* **Keys B-Tree Pages:** Uses the exact same SBBTreePage and SBBTreeNode structures defined in the B-Tree specification. The payload of a leaf node, instead of a TupleId, will be a uint64\_t page number pointing to the root of a Posting B-Tree.  
* **Posting B-Tree Pages:** Also uses the SBBTreePage structure, but it is specialized to store TupleIds. The "key" in these nodes is the TupleId itself, and there is no associated value.

## **4\. Core Algorithms (C++)**

### **4.1. Key Extraction**

The GIN index is generic; it requires a data-type-specific function to extract indexable items from a column value.

// Example key extractor for an array of strings.  
// The database provides this function based on the column's data type.  
std::vector\<Datum\> extract\_keys\_from\_string\_array(const Datum& array\_datum) {  
    // ... logic to parse the array format and return each element as a Datum ...  
    std::vector\<Datum\> keys;  
    // ... fill keys ...  
    return keys;  
}

### **4.2. Insertion (into Pending List)**

Insertion is extremely fast as it only involves appending to the pending list.

void gin\_insert(SBGinIndexMetaPage\* meta, const Datum& composite\_value, const TupleId& tid) {  
    std::vector\<Datum\> keys \= extract\_keys\_from\_string\_array(composite\_value);  
      
    // ... logic to find the tail page of the pending list ...  
    SBGinPendingListPage\* tail\_page \= load\_page(meta-\>gin\_pending\_list\_tail);

    for (const auto& key : keys) {  
        // If the current tail page is full, allocate a new one and link it.  
        if (page\_is\_full(tail\_page)) {  
             tail\_page \= allocate\_new\_pending\_page(meta);  
        }  
          
        // Append the new entry to the page.  
        append\_entry\_to\_page(tail\_page, {tid, key});  
        meta-\>gin\_pending\_list\_count++;  
    }  
}

### **4.3. Pending List Merge**

This is the core maintenance operation, typically triggered by a VACUUM process or when the pending list exceeds a size threshold.

void gin\_merge\_pending\_list(SBGinIndexMetaPage\* meta) {  
    if (meta-\>gin\_pending\_list\_count \== 0\) return;

    // 1\. Read all entries from all pending list pages into memory.  
    std::vector\<PendingEntry\> entries \= read\_all\_pending\_entries(meta);

    // 2\. Sort the entries first by key, then by TupleId. This is crucial for  
    //    efficiency as it groups all TupleIds for a given key together.  
    std::sort(entries.begin(), entries.end(), /*lambda*/ (const auto& a, const auto& b) {  
        int key\_cmp \= compare\_datums(a.key, b.key);  
        if (key\_cmp \!= 0\) return key\_cmp \< 0;  
        return compare\_tids(a.tid, b.tid) \< 0;  
    });

    // 3\. Iterate through the sorted list and insert into the main index.  
    for (size\_t i \= 0; i \< entries.size(); ) {  
        const Datum& current\_key \= entries\[i\].key;  
          
        // Find or create an entry for current\_key in the main Keys B-Tree.  
        uint64\_t posting\_tree\_root \= find\_or\_create\_posting\_tree(meta-\>gin\_keys\_btree\_root, current\_key);  
          
        // Collect all TupleIds for this key from the sorted list.  
        std::vector\<TupleId\> tids\_for\_key;  
        size\_t j \= i;  
        while (j \< entries.size() && compare\_datums(entries\[j\].key, current\_key) \== 0\) {  
            tids\_for\_key.push\_back(entries\[j\].tid);  
            j++;  
        }  
          
        // Bulk-insert these sorted TupleIds into the corresponding posting tree.  
        // This can be done very efficiently with a sort-based B-Tree build.  
        bulk\_insert\_into\_posting\_tree(posting\_tree\_root, tids\_for\_key);  
          
        i \= j; // Move to the next distinct key.  
    }

    // 4\. Clear the pending list.  
    free\_pending\_list\_pages(meta);  
    meta-\>gin\_pending\_list\_count \= 0;  
}

### **4.4. Querying**

A query for a single key involves two B-Tree lookups.

// Returns an iterator over all TupleIds containing the query\_key.  
GINIterator gin\_find(const SBGinIndexMetaPage\* meta, const Datum& query\_key) {  
    // 1\. Search the Keys B-Tree for the query\_key.  
    SearchResult key\_result \= btree\_find(meta-\>gin\_keys\_btree\_root, query\_key);  
      
    if (\!key\_result.found) {  
        return GINIterator::end(); // Key does not exist in any tuple.  
    }  
      
    // 2\. The payload of the found leaf node is the root of the posting tree.  
    uint64\_t posting\_tree\_root \= key\_result.payload.page\_number;

    // 3\. Return a standard B-Tree iterator that scans the posting tree.  
    return GINIterator(posting\_tree\_root);  
}

// For multi-key queries (e.g., documents containing 'database' AND 'performance'),  
// the query planner gets an iterator for each key and finds the intersection of the  
// TupleId streams, often by converting them to temporary bitmaps and using  
// bitwise AND.  
