# B-Tree Index Design

## 1. Introduction

This document outlines the design of a B-tree index for the ScratchBird database engine. The B-tree index will be used to provide efficient retrieval of data from tables, and will be used to implement the index on the TOAST table.

## 2. On-Disk Format

The B-tree index will consist of three types of pages:

*   **Meta Page:** This page contains metadata about the index, such as the root page of the B-tree, the order of the B-tree, and the number of keys in the index.
*   **Internal Pages:** These pages contain pointers to other internal pages or to leaf pages.
*   **Leaf Pages:** These pages contain the actual index entries, which consist of a key and a pointer to the corresponding tuple in the table.

### 2.1. Meta Page

The meta page will have the following structure:

```
+------------------+
| PageHeader       |
+------------------+
| BTreeMeta        |
+------------------+
```

The `BTreeMeta` struct will have the following fields:

```cpp
struct BTreeMeta {
    uint32_t root_page;     // Root page of the B-tree
    uint32_t order;         // Order of the B-tree
    uint64_t num_keys;      // Number of keys in the index
};
```

### 2.2. Internal Pages

Internal pages will have the following structure:

```
+------------------+
| PageHeader       |
+------------------+
| BTreeInternal    |
+------------------+
| Keys[]           |
+------------------+
| Pointers[]       |
+------------------+
```

The `BTreeInternal` struct will have the following fields:

```cpp
struct BTreeInternal {
    uint32_t num_keys;      // Number of keys in this page
    uint32_t parent_page;   // Parent page of this page
};
```

The `Keys` array will contain the keys, and the `Pointers` array will contain the page IDs of the child pages.

### 2.3. Leaf Pages

Leaf pages will have the following structure:

```
+------------------+
| PageHeader       |
+------------------+
| BTreeLeaf        |
+------------------+
| Entries[]        |
+------------------+
```

The `BTreeLeaf` struct will have the following fields:

```cpp
struct BTreeLeaf {
    uint32_t num_entries;   // Number of entries in this page
    uint32_t parent_page;   // Parent page of this page
    uint32_t next_leaf;     // Next leaf page
    uint32_t prev_leaf;     // Previous leaf page
};
```

The `Entries` array will contain the index entries, which will have the following structure:

```cpp
struct IndexEntry {
    // Key will be a flexible array member
    // For the TOAST index, the key will be (chunk_id, chunk_seq)
    // For other indexes, the key will be defined by the user
    uint64_t tid;           // Tuple ID of the corresponding tuple
    // Key data follows
};
```

## 3. Algorithms

### 3.1. Search

The search algorithm will start at the root page and traverse the B-tree until it finds the desired key in a leaf page. The search will be a standard B-tree search algorithm.

### 3.2. Insertion

The insertion algorithm will first search for the appropriate leaf page to insert the new key. If the leaf page is full, it will be split. The split will propagate up the tree if necessary.

### 3.3. Deletion

The deletion algorithm will first search for the key to be deleted. Once the key is found in a leaf page, it will be removed. If the leaf page becomes underfull, it will be merged with a sibling page or keys will be redistributed. The merge or redistribution will propagate up the tree if necessary.

## 4. Integration with the Catalog

The `CatalogManager` will be extended to support creating and dropping indexes. The following new methods will be added:

*   `create_index(const std::string& index_name, const std::string& table_name, const std::vector<std::string>& column_names)`
*   `drop_index(const std::string& index_name)`

The `TableInfo` struct will be extended to include a list of index IDs.

## 5. Integration with the ToastManager

The `ToastManager` will be modified to create a B-tree index on the `(chunk_id, chunk_seq)` columns of the TOAST table when the TOAST table is created. The `read_toast_chunks` method will be modified to use the B-tree index to efficiently retrieve TOAST chunks.
