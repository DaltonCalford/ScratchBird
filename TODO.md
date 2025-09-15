# ScratchBird B-Tree and TOAST Implementation Plan

This plan outlines the necessary steps to complete the B-Tree and TOAST features for Alpha Stage 1.1.

## Overall Baseline Assessment

*   **B-Tree Indexing:** The implementation is **severely incomplete**. A basic class structure exists, but it lacks the detailed on-disk format, node structures, page split logic, and scan capabilities defined in the specification. It is not functional enough to support the TOAST table efficiently.
*   **TOAST/LOB Storage:** The core logic is **mostly complete**, but it relies on an inefficient **heap-scan fallback** for chunk retrieval due to the lack of a functional B-Tree index.
*   **Integration:** The `HeapPage` has the necessary hooks to use the `ToastManager`, but the full, efficient integration is blocked by the incomplete B-Tree.

---

## Phase 1: Implement Core B-Tree Functionality (Based on `INDEX_IMPLEMENTATION_SPEC.md`)

1.  **Step 1.1: Implement On-Disk Structures:**
    *   **Action:** In `btree.h` (or a new `btree_ondisk.h`), define the `SBBTreePage` and `SBBTreeNode` structs exactly as specified, including all flags and compression metadata fields.
    *   **Verification:** Ensure structs are packed correctly and match the specification.

2.  **Step 1.2: Implement B-Tree Page Operations:**
    *   **Action:** Create a `BTreePage` class (or free functions) to manage B-Tree pages. Implement functions for:
        *   `initialize_page`
        *   `get_node`, `add_node`, `remove_node`
        *   `find_split_point`
        *   `has_sufficient_space`
    *   **Verification:** Write unit tests for these page-level manipulations.

3.  **Step 1.3: Implement Core B-Tree Operations (Insert & Search):**
    *   **Action:** Implement the `BTree::insert` and `BTree::search` methods. This will require implementing the page split logic (`btree_split_page`) as described in the spec. For now, focus on a simple search that returns a single value.
    *   **Verification:** Write unit tests that insert a large number of keys, trigger page splits, and verify that all keys can be found correctly.

4.  **Step 1.4: Implement B-Tree Scan:**
    *   **Action:** Implement the `IndexScanIterator`. This involves creating `begin_scan`, `get_next`, and `end_scan` logic that can traverse the leaf nodes of the B-Tree.
    *   **Verification:** Write unit tests that perform forward and backward scans, range scans, and scans on an empty tree.

---

## Phase 2: Integrate B-Tree with TOAST Manager

1.  **Step 2.1: Create B-Tree Index for TOAST Table:**
    *   **Action:** In `ToastManager::create_toast_table`, after creating the table, create a B-Tree index on the `(chunk_id, chunk_seq)` columns.
    *   **Verification:** After creating a table, verify that the corresponding TOAST table and its index exist in the system catalog.

2.  **Step 2.2: Replace Heap Scans with Index Scans:**
    *   **Action:** In `toast.cpp`, rewrite `read_toast_chunks` and `delete_toast_value` to use the new `IndexScanIterator` to find chunks instead of the `_heap_scan` methods.
    *   **Verification:** Unit tests for `detoast_value` and `delete_toast_value` should pass using the new index-based implementation. Performance should be significantly better.

---

## Phase 3: Complete HeapPage Integration

1.  **Step 3.1: Implement Automatic TOASTing in `insert_tuple`:**
    *   **Action:** In `heap_page.cpp`, fully implement the logic in `insert_tuple`. If a value is over the TOAST threshold, it should call `toast_mgr_->toast_value()` and store the resulting `ToastPointer`.
    *   **Verification:** Write an integration test that inserts a large tuple and verify that a `ToastPointer` is stored in the heap page and the actual data is in the TOAST table.

2.  **Step 3.2: Implement Automatic Detoasting in `get_tuple`:**
    *   **Action:** In `heap_page.cpp`, fully implement the logic in `get_tuple`. If it encounters a `ToastPointer`, it should call `toast_mgr_->detoast_value()` to retrieve the full value.
    *   **Verification:** Write an integration test that retrieves the large tuple inserted in the previous step and verifies the data is correct.
