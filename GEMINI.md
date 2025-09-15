# Gemini Workspace Context: ScratchBird Database Engine

## Project Overview

ScratchBird is a database engine written in C++17. The project is in the alpha stage and follows a detailed, phased implementation plan. The primary goal is to build a modern, embedded database core with future plans for network capabilities, including compatibility with PostgreSQL and MySQL wire protocols.

The architecture is designed around a central "Y-Valve" that will eventually route connections to different parser processes, allowing for multi-protocol support. The current focus (Alpha Stage 1) is on building the core embedded, non-network features.

Key technical details include:
- **Page Sizes:** Supports 8KB, 16KB, and 32KB pages, with larger sizes planned.
- **UUIDs:** Uses UUIDv7 exclusively for identifiers.
- **Checksum:** Employs CRC32C for data integrity.
- **Transactions:** Implements MVCC for concurrency control.
- **Compression:** Optional support for LZ4.

## Building and Running

The project uses CMake for building and CTest for running tests.

**Build Steps:**

```bash
# 1. Create a build directory
mkdir build

# 2. Navigate into the build directory
cd build

# 3. Configure the project with CMake
cmake ..

# 4. Compile the source code
make
```

**Running Tests:**

All tests are run from the `build` directory.

```bash
# Run all tests and show output on failure
ctest --output-on-failure

# Run a specific group of tests (e.g., for a specific alpha phase)
ctest -R "Alpha101"
```

## Development Conventions

The project follows a strict, plan-driven development process. The authoritative guide for all implementation work is `project/plan/AUTHORITATIVE_IMPLEMENTATION_PLAN.md`.

- **Phased Implementation:** Development is broken down into discrete, numbered alpha phases (e.g., 1.01, 1.02). Each phase has clear goals, technical requirements, and success criteria.
- **Testing:** New features must be accompanied by unit and integration tests. The project aims for a high level of test coverage.
- **Documentation:** The `docs/` directory contains extensive design documents, technical specifications, and reference materials.
- **Source Code Structure:**
    - `src/core/`: The core database engine components (storage, transactions, etc.).
    - `src/parser/`: The SQL parser.
    - `src/sblr/`: A bytecode system.
    - `tests/`: Contains unit and integration test suites.

---

## Current Status (as of 2025-09-12)

**Goal:** Complete Alpha Stage 1.1.4: TOAST/LOB Storage.

**Baseline Assessment:**
*   **B-Tree Indexing:** Severely incomplete. A placeholder class exists, but it lacks the on-disk structures and operational logic (scans, page splits) required by the specification. This is the primary blocker.
*   **TOAST/LOB Storage:** The core `ToastManager` is mostly implemented but uses an inefficient heap-scan fallback for data retrieval because the B-Tree index is not functional.
*   **Integration:** The `HeapPage` has the necessary hooks for TOASTing, but the final integration is pending the completion of the B-Tree.

**Next Step:**
*   **Phase 1, Step 1.1:** Implement the B-Tree on-disk structures (`SBBTreePage`, `SBBTreeNode`) in `include/scratchbird/core/btree.h` as defined in `docs/specifications/INDEX_IMPLEMENTATION_SPEC.md`.