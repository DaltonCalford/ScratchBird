# ScratchBird Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


This plan outlines the development stages for the ScratchBird database engine, focusing on the Alpha and Beta releases. It is a living document and will be updated as more information is determined.

## Implementation Status

| Feature | Status | Notes |
| :--- | :--- | :--- |
| **Alpha Stage 0: Foundation** | | |
| Database Core | Implemented | |
| Page Management | Implemented | |
| System Catalog | Implemented | |
| Storage Engine | Implemented | |
| Transaction Foundation | Implemented | |
| Basic SQL Parser | Implemented | To be removed in Phase 1.2 |
| **Alpha Stage 1.1: Extended Storage** | | |
| Extended Page Sizes | Implemented | |
| Compression | Implemented | |
| TOAST/LOB | Partially Implemented | Needs integration with the storage engine. |

## Alpha Stage: Standalone Embedded Engine

The goal of the Alpha stage is to produce a complete, robust, and fully tested embedded database engine that is driven by an SBLR-based API. This engine will be used by a new, full-featured ScratchBird parser, which in turn will be used by a suite of powerful command-line tools.

- **Phase 1.1: Extended Storage**
    - **Action:** Complete the implementation of the extended storage features.
    - **Deliverable:** A fully functional extended storage system, including TOAST/LOB support.
    - **Relevant Documentation:**
        - `/docs/specifications/parser/v3/EXTENDED_PAGE_SIZES.md`
        - `/docs/specifications/parser/v3/COMPRESSION_FRAMEWORK.md`
        - `/docs/specifications/parser/v3/TOAST_LOB_STORAGE.md`
    - **Steps:**
        - **1.1.1:** Integrate the `ToastManager` with the `StorageEngine` and `HeapPage` classes.
        - **1.1.2:** Modify the `insert_tuple` method in `HeapPage` to automatically TOAST large attributes.
        - **1.1.3:** Modify the `get_tuple` method in `HeapPage` to automatically detoast TOASTed attributes.
        - **1.1.4:** Add comprehensive integration tests for the TOAST functionality.

- **Phase 1.2: SBLR-Driven Engine API**
    - **Action:** Remove the basic SQL parser that was built for initial testing.
    - **Action:** Solidify and document the public engine API to exclusively accept SBLR (ScratchBird Language Representation).
    - **Deliverable:** A comprehensive test suite that validates the SBLR-driven API.
    - **Relevant Documentation:**
        - `/docs/specifications/parser/v3/SBLR_BYTECODE_SPECIFICATION.md`
    - **Steps:**
        - **1.2.1:** Refactor the `SQLExecutor` class to remove any direct SQL parsing capabilities.
        - **1.2.2:** Implement the SBLR interpreter in the `Executor` class to handle all SBLR opcodes defined in the specification.
        - **1.2.3:** Create a C++-level API for the engine that accepts SBLR bytecode and returns a `ResultSet`.
        - **1.2.4:** Develop a comprehensive test suite that directly calls the SBLR API with a variety of SBLR programs to validate the engine's functionality.

- **Phase 1.3: Full ScratchBird Parser**
    - **Action:** Develop the full-featured ScratchBird parser as a separate, reusable component. This parser will compile the ScratchBird SQL dialect into SBLR.
    - **Deliverable:** A comprehensive test suite for the parser, including tests for syntax, semantics, and SBLR output.
    - **Relevant Documentation:**
        - `/docs/specifications/parser/v3/SCRATCHBIRD_SQL_DIALECT_COMPLETE.md`
        - `/docs/specifications/parser/v3/SQL_COMPLETE_BNF.md`
        - `/docs/specifications/parser/v3/SQL_GRAMMAR_BNF.md`
    - **Steps:**
        - **1.3.1:** Implement a complete lexer for the ScratchBird SQL dialect.
        - **1.3.2:** Implement a recursive descent parser that builds an Abstract Syntax Tree (AST) from the token stream.
        - **1.3.3:** Implement a semantic analyzer that performs type checking and name resolution on the AST.
        - **1.3.4:** Implement a bytecode generator that converts the validated AST into SBLR bytecode.
        - **1.3.5:** Develop a comprehensive test suite for the parser, including unit tests for each component and integration tests for the entire pipeline.

- **Phase 1.4: Core Command-Line Tools**
    - **Action:** Develop the initial set of essential command-line tools that use the new parser and engine.
    - **Deliverables:**
        - `sb_isql`: An interactive SQL shell.
        - `sb_verify`: A tool to check the integrity of database files.
    - **Steps:**
        - **1.4.1:** Develop the `sb_isql` tool, which will use the ScratchBird parser to compile SQL to SBLR and the engine API to execute the SBLR.
        - **1.4.2:** Develop the `sb_verify` tool, which will directly interact with the database file to perform integrity checks.

- **Phase 1.5: Advanced Command-Line Tools**
    - **Action:** Develop the more advanced command-line tools for administration and security.
    - **Deliverables:**
        - `sb_backup`: A tool for creating and restoring database backups.
        - `sb_security`: A tool for managing users, roles, and permissions.
    - **Relevant Documentation:**
        - **Backup and Restore:** `/docs/specifications/parser/v3/BACKUP_AND_RESTORE.md`
        - **Security Model:** `/docs/specifications/parser/v3/AUTH_CORE_FRAMEWORK.md`, `/docs/specifications/parser/v3/ROLE_COMPOSITION_AND_HIERARCHIES.md`, `/docs/specifications/parser/v3/SCHEMA_PERMISSIONS_AND_INHERITANCE.md`

- **Phase 1.6: Purpose-Built Client Framework**
    - **Action:** Design and implement the framework for purpose-built clients that interact with the engine directly, without a parser (e.g., for maintenance tasks).
    - **Deliverable:** A simple example client, such as a statistics updater, to demonstrate the framework.

## Beta Stage: Networked Multi-Protocol Engine

The Beta stage will transform the embedded engine into a full-fledged database server with multi-protocol support and advanced features.

- **Phase 2.1: Local Access Server**
    - **Action:** Develop a local access server that exposes the engine's API over a local socket, including all necessary optimizations for a server environment.
    - **Action:** Update the CLI tools to be able to connect to the local server.
    - **Relevant Documentation:**
        - `/docs/specifications/parser/v3/NETWORK_LAYER_SPEC.md`

- **Phase 2.2: Network Listeners & Connection Pooling**
    - **Action:** Develop network listeners to accept connections from remote clients.
    - **Action:** Implement a robust connection pooling mechanism to manage client connections efficiently.
    - **Relevant Documentation:**
        - `/docs/specifications/parser/v3/NETWORK_LAYER_SPEC.md`

- **Phase 2.3: Y-Valve Router**
    - **Action:** Implement the Y-Valve architecture to route incoming connections to the appropriate parser and engine instance.
    - **Relevant Documentation:**
        - `/docs/specifications/parser/v3/Y_VALVE_ARCHITECTURE.md`
        - `/docs/specifications/parser/v3/Y_VALVE_DESIGN_PRINCIPLES.md`
        - `/docs/specifications/parser/v3/wire_protocols/`

- **Phase 2.4: Multi-Engine Support**
    - **Action:** Implement the necessary mechanisms to allow multiple, different versions of the ScratchBird engine to run concurrently, each managing its own set of databases.
    - **Relevant Documentation:**
        - `/docs/specifications/parser/v3/Y_VALVE_ARCHITECTURE.md`
