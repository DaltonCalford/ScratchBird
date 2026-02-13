# **ScratchBird: Hash Index Low-Level Implementation Specification**

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Version 1.0  
Last Updated: September 15, 2025

## **1\. Overview & Use Case**

This document provides a complete low-level technical specification for the ScratchBird Hash Index. This index type is designed for one purpose: extremely fast equality lookups (WHERE column \= 'value'). It is not suitable for range queries or sorted data retrieval. The implementation is based on extendible hashing, allowing the index to grow dynamically and gracefully handle large volumes of data while minimizing hash collisions.

## **2\. Core Concepts**

A hash index works by applying a hash function to the indexed key, which yields a hash value. This value is used to determine which "bucket" (a dedicated page or set of pages) the data entry resides in.

* **Hash Function:** Maps a variable-length key to a fixed-size integer. We will use the 64-bit MurmurHash3 algorithm.  
* **Buckets:** A bucket is a page that stores key-value pairs. All keys in a bucket will have a hash value that maps to that bucket.  
* **Directory:** A mapping from a hash value to a bucket page. In extendible hashing, this is a power-of-two-sized array of pointers.  
* **Collisions:** When two different keys produce the same hash or map to the same bucket. Collisions are handled by creating overflow pages.

## **3\. On-Disk Page Structures (C++)**

A hash index consists of one meta page, one or more directory pages, and many bucket pages.

### **3.1. Meta Page**

There is only one Meta Page per hash index, located at a fixed position (e.g., page 0 of the index file). It stores the overall state of the index.

// The single metadata page for the entire hash index.  
struct SBHashIndexMetaPage {  
    PageHeader      hip\_header;  
    UUID            hip\_index\_uuid;  
    uint32\_t        hip\_hash\_func\_id;   // ID for the hash function (e.g., Murmur3\_64)  
    uint32\_t        hip\_global\_depth;   // Determines size of the directory (2^depth)  
    uint64\_t        hip\_directory\_page; // Page number of the first directory page  
    uint64\_t        hip\_num\_tuples;     // Total number of indexed tuples  
};

### **3.2. Directory Page(s)**

The directory can span multiple pages if it becomes too large. It's an array of page numbers pointing to the bucket pages.

// A page that holds a segment of the directory.  
struct SBHashDirectoryPage {  
    PageHeader      hdp\_header;  
    uint64\_t        hdp\_next\_page; // Page number of the next directory page, if any.  
      
    // The directory is an array of page numbers.  
    uint64\_t        hdp\_bucket\_pointers\[/\* ... \*/\]; // Fills the rest of the page.  
};

### **3.3. Bucket Page and Entry Structure**

This is where the actual indexed data is stored.

// A hash bucket page, which stores HashEntry items.  
struct SBHashBucketPage {  
    PageHeader      hbp\_header;  
    uint16\_t        hbp\_entry\_count;    // Number of entries currently in this page  
    uint16\_t        hbp\_local\_depth;    // Depth of this specific bucket  
    uint64\_t        hbp\_overflow\_page;  // Page number of the next overflow page (0 if none)  
      
    // A small header is followed by a contiguous block of HashEntry structs.  
    // HashEntry hbp\_entries\[...\];  
};

// A single entry within a bucket page.  
struct HashEntry {  
    uint64\_t        he\_key\_hash;    // The full 64-bit hash of the key  
    TupleId         he\_tuple\_id;    // Pointer to the tuple in the main table  
    // The actual key data is NOT stored in the hash index to save space.  
    // A re-check against the heap is required to confirm a match.  
};

## **4\. Core Algorithms (C++)**

### **4.1. Key Hashing and Bucket Lookup**

This is the primary lookup mechanism.

// 64-bit MurmurHash3 implementation (public domain)  
uint64\_t MurmurHash64(const void\* key, int len, uint64\_t seed); // Assumed to exist

// Calculates which directory slot a key belongs to.  
uint32\_t get\_directory\_index(const Datum& key, uint32\_t global\_depth) {  
    uint64\_t hash \= MurmurHash64(key.data, key.length, 0);  
    // Use the first 'global\_depth' bits of the hash as the index.  
    return hash & ((1 \<\< global\_depth) \- 1);  
}

// Finds the bucket page number for a given key.  
uint64\_t find\_bucket\_page\_for\_key(const SBHashIndexMetaPage\* meta, const Datum& key) {  
    uint32\_t dir\_index \= get\_directory\_index(key, meta-\>hip\_global\_depth);  
      
    // ... logic to load the correct directory page ...  
    SBHashDirectoryPage\* dir\_page \= load\_directory\_page(meta-\>hip\_directory\_page, dir\_index);  
      
    return dir\_page-\>hdp\_bucket\_pointers\[dir\_index\];  
}

### **4.2. Insertion**

Insertion involves hashing the key, finding the right bucket, and adding the entry. If the bucket is full, it may need to be split, which can trigger a directory expansion.

// Inserts a key/TID pair into the hash index.  
void hash\_insert(SBHashIndexMetaPage\* meta, const Datum& key, const TupleId& tid) {  
    uint64\_t bucket\_page\_num \= find\_bucket\_page\_for\_key(meta, key);  
    SBHashBucketPage\* bucket\_page \= load\_page(bucket\_page\_num);

    // Check if there is space in the bucket page (or its overflow chain).  
    if (bucket\_page\_has\_space(bucket\_page)) {  
        HashEntry new\_entry;  
        new\_entry.he\_key\_hash \= MurmurHash64(key.data, key.length, 0);  
        new\_entry.he\_tuple\_id \= tid;  
          
        // ... logic to add new\_entry to the page ...  
        return;  
    }  
      
    // \--- Bucket is full, handle split \---  
      
    if (bucket\_page-\>hbp\_local\_depth \< meta-\>hip\_global\_depth) {  
        // Case 1: Just split the bucket, no directory expansion needed.  
        // 1\. Allocate a new bucket page.  
        // 2\. Increment local\_depth for both pages.  
        // 3\. Re-hash all entries from the old bucket and distribute them  
        //    between the old and new pages based on the new local\_depth bit.  
        // 4\. Update the second half of the directory pointers that were  
        //    pointing to the old bucket to now point to the new bucket.  
        // 5\. Retry the insertion.  
          
    } else { // local\_depth \== global\_depth  
        // Case 2: Split the bucket AND expand the directory.  
        // 1\. Double the size of the directory. This involves allocating new  
        //    directory pages and copying the old pointers.  
        // 2\. Increment global\_depth in the meta page.  
        // 3\. Now local\_depth \< global\_depth, so proceed as in Case 1\.  
    }  
}

### **4.3. Lookup**

Lookup is very fast. It involves one hash calculation and typically one page read.

// Finds all TupleIds for a given key.  
std::vector\<TupleId\> hash\_find(const SBHashIndexMetaPage\* meta, const Datum& key) {  
    std::vector\<TupleId\> results;  
    uint64\_t full\_key\_hash \= MurmurHash64(key.data, key.length, 0);  
      
    uint64\_t bucket\_page\_num \= find\_bucket\_page\_for\_key(meta, key);  
    SBHashBucketPage\* bucket\_page \= load\_page(bucket\_page\_num);

    // Iterate through the bucket and all its overflow pages.  
    do {  
        // ... logic to iterate over all hbp\_entries in the current bucket\_page ...  
        for (const auto& entry : entries\_on\_page) {  
            if (entry.he\_key\_hash \== full\_key\_hash) {  
                // Potential match\! The full key hash matches.  
                // A re-check is required: load the tuple from the heap using  
                // entry.he\_tuple\_id and compare the actual key data.  
                if (heap\_key\_matches(entry.he\_tuple\_id, key)) {  
                    results.push\_back(entry.he\_tuple\_id);  
                }  
            }  
        }  
        // Move to the next overflow page, if one exists.  
        bucket\_page \= load\_page(bucket\_page-\>hbp\_overflow\_page);  
    } while (bucket\_page \!= nullptr);  
      
    return results;  
}  
