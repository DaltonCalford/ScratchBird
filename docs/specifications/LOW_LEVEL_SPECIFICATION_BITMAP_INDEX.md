# **ScratchBird: Bitmap Index Low-Level Implementation Specification**

Version 1.0  
Last Updated: September 15, 2025

## **1\. Overview & Use Case**

This document provides a complete low-level technical specification for the ScratchBird Bitmap Index. This specialized index is designed for columns with low cardinality (i.e., a small number of distinct values, such as "Gender", "Country", or "Status"). Its primary strength is the ability to perform complex logical operations (AND, OR, NOT) on multiple conditions with extreme speed by using bitwise operations.

A bitmap index consists of one bitmap for each distinct value in the column. Each bitmap is a stream of bits where the Nth bit is set to 1 if the Nth tuple in the table has that value, and 0 otherwise.

## **2\. Core Concepts & Data Structures**

To handle large datasets efficiently, raw bitmaps are too sparse and large. We will implement **Roaring Bitmaps**, the industry standard for compressed bitmaps.

A Roaring Bitmap divides the 32-bit integer space (representing tuple IDs) into 65,536 chunks of 2^16 (65,536) values each. Each chunk is stored in a "container."

* **Array Container:** Used when a chunk has 4096 or fewer set bits. It's a simple sorted array of uint16\_t values. Very space-efficient for sparse data.  
* **Bitset Container:** Used when a chunk has more than 4096 set bits. It's a dense bitset of 8KB (65536 / 8). Efficient for dense data.  
* **Run Container:** A further optimization using run-length encoding (e.g., storing \[start, length\] pairs for contiguous blocks of set bits).

### **2.1. On-Disk Structures (C++)**

The index is composed of a meta page (the "Key-to-Bitmap" dictionary) and many bitmap data pages.

// The main metadata page for the bitmap index.  
// It maps distinct data values to their bitmap streams.  
struct SBBitmapIndexMetaPage {  
    PageHeader      bmp\_header;  
    UUID            bmp\_index\_uuid;  
    // Followed by a variable-length list of DictionaryEntry structs.  
};

struct DictionaryEntry {  
    Datum           value;      // The distinct value (e.g., 'USA', 'Canada')  
    uint64\_t        bitmap\_root\_page; // Page number for the root of its Roaring Bitmap  
    uint32\_t        cardinality; // Number of tuples with this value  
};

// A page containing the top-level structure of a single Roaring Bitmap.  
// This is an array of ContainerPointers, indexed by the high bits of a TupleId.  
struct RoaringBitmapRootPage {  
    PageHeader      rbr\_header;  
    // Followed by 65,536 ContainerPointer entries.  
    // ContainerPointer rbr\_pointers\[65536\];  
};

struct ContainerPointer {  
    uint64\_t        page\_number; // Page where the container is stored  
    uint16\_t        num\_values;  // Number of set bits in that container  
    uint8\_t         type;        // 0=Array, 1=Bitset, 2=Run  
};

// A page containing the actual container data.  
struct RoaringContainerPage {  
    PageHeader      rcp\_header;  
    uint8\_t         rcp\_type;  
    uint16\_t        rcp\_num\_values;  
    // Followed by container data (either a sorted array of uint16\_t,  
    // an 8KB bitset, or start/length pairs).  
};

## **3\. Core Algorithms (C++)**

### **3.1. TupleID to Bitmap Position Mapping**

A TupleId must be mapped to a unique integer to find its position in the bitmap.

// Assuming a fixed number of tuples per heap page.  
constexpr uint32\_t TUPLES\_PER\_PAGE \= 256;

inline uint32\_t tuple\_id\_to\_int(const TupleId& tid) {  
    return (tid.page\_number \* TUPLES\_PER\_PAGE) \+ tid.item\_offset;  
}

inline void int\_to\_high\_low\_bits(uint32\_t id, uint16\_t& high, uint16\_t& low) {  
    high \= id \>\> 16;  
    low \= id & 0xFFFF;  
}

### **3.2. Adding a Tuple to a Bitmap (Set Bit)**

This is the insertion logic.

// Adds a tuple to the bitmap for a specific value.  
void bitmap\_add(SBBitmapIndexMetaPage\* meta, const Datum& value, const TupleId& tid) {  
    // 1\. Find the DictionaryEntry for 'value' in the meta page.  
    //    If it doesn't exist, create it and allocate a new RoaringBitmapRootPage.  
    DictionaryEntry\* entry \= find\_or\_create\_dictionary\_entry(meta, value);  
    RoaringBitmapRootPage\* root \= load\_page(entry-\>bitmap\_root\_page);

    // 2\. Map the TupleId to its high and low bits.  
    uint32\_t int\_id \= tuple\_id\_to\_int(tid);  
    uint16\_t high\_bits, low\_bits;  
    int\_to\_high\_low\_bits(int\_id, high\_bits, low\_bits);  
      
    // 3\. Find the container for this chunk.  
    ContainerPointer\* ptr \= \&root-\>rbr\_pointers\[high\_bits\];  
    RoaringContainerPage\* container\_page \= nullptr;  
    if (ptr-\>page\_number \== 0\) {  
        // First time adding to this chunk, create a new ArrayContainer.  
        container\_page \= create\_new\_container\_page(ContainerType::ARRAY);  
        ptr-\>page\_number \= get\_page\_number(container\_page);  
        ptr-\>type \= ContainerType::ARRAY;  
    } else {  
        container\_page \= load\_page(ptr-\>page\_number);  
    }  
      
    // 4\. Add the low bits to the container.  
    add\_value\_to\_container(container\_page, low\_bits);

    // 5\. Handle container type conversion if necessary.  
    //    If an ArrayContainer grows beyond 4096 values, it must be converted  
    //    to a BitsetContainer.  
    if (ptr-\>type \== ContainerType::ARRAY && container\_page-\>rcp\_num\_values \> 4096\) {  
        convert\_array\_to\_bitset\_container(container\_page);  
        ptr-\>type \= ContainerType::BITSET;  
    }  
    ptr-\>num\_values++;  
    entry-\>cardinality++;  
}

### **3.3. Querying and Logical Operations**

The power of bitmap indexes comes from combining bitmaps. The query engine requests an iterator for each condition, and then combines them.

// Represents an iterator over a single roaring bitmap.  
class RoaringBitmapIterator {  
public:  
    // Returns the next TupleId (as an integer) in the bitmap.  
    uint32\_t next();  
    // Checks if there are more values.  
    bool has\_next();  
};

// Example of combining two bitmaps with a logical AND.  
std::vector\<TupleId\> query\_with\_and(  
    RoaringBitmapIterator& iter1,  
    RoaringBitmapIterator& iter2)  
{  
    std::vector\<TupleId\> results;  
    uint32\_t val1 \= iter1.next();  
    uint32\_t val2 \= iter2.next();

    while (iter1.has\_next() && iter2.has\_next()) {  
        if (val1 \== val2) {  
            results.push\_back(int\_to\_tuple\_id(val1)); // Match found  
            val1 \= iter1.next();  
            val2 \= iter2.next();  
        } else if (val1 \< val2) {  
            val1 \= iter1.next(); // Advance the first iterator  
        } else {  
            val2 \= iter2.next(); // Advance the second  
        }  
    }  
    return results;  
}

This intersection logic is extremely fast as it's a simple merge-join-like process on sorted integer lists, which can be further optimized at the container level with direct bitwise AND operations for bitset containers.