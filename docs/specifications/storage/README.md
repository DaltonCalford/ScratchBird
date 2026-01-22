# Storage Engine Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains storage layer specifications for ScratchBird's storage engine, including buffer management, page structures, and large object storage.

## Overview

ScratchBird implements a sophisticated storage engine based on Firebird's Multi-Generational Architecture (MGA) principles, with advanced features for buffer management, TOAST (The Oversized-Attribute Storage Technique), and flexible tablespace management.

## Specifications in this Directory

### Core Storage Architecture

- **[STORAGE_ENGINE_MAIN.md](STORAGE_ENGINE_MAIN.md)** (804 lines) - Main storage engine architecture and design
- **[STORAGE_ENGINE_BUFFER_POOL.md](STORAGE_ENGINE_BUFFER_POOL.md)** (1,025 lines) - Buffer pool implementation and caching strategies
- **[STORAGE_ENGINE_PAGE_MANAGEMENT.md](STORAGE_ENGINE_PAGE_MANAGEMENT.md)** (1,288 lines) - Page management and allocation

### Disk Format

- **[ON_DISK_FORMAT.md](ON_DISK_FORMAT.md)** (552 lines) - On-disk data structure format
- **[EXTENDED_PAGE_SIZES.md](EXTENDED_PAGE_SIZES.md)** (107 lines) - Configurable page size support

### Large Object Storage

- **[TOAST_LOB_STORAGE.md](TOAST_LOB_STORAGE.md)** (506 lines) - TOAST implementation for large objects
- **[HEAP_TOAST_INTEGRATION.md](HEAP_TOAST_INTEGRATION.md)** (195 lines) - Integration between heap and TOAST storage

### Advanced Features

- **[TABLESPACE_SPECIFICATION.md](TABLESPACE_SPECIFICATION.md)** (1,352 lines) - Tablespace management and configuration
- **[TABLESPACE_ONLINE_MIGRATION.md](TABLESPACE_ONLINE_MIGRATION.md)** - Online tablespace migration (Beta)
- **[MGA_IMPLEMENTATION.md](MGA_IMPLEMENTATION.md)** (1,024 lines) - Multi-Generational Architecture implementation details

## Key Concepts

### Multi-Generational Architecture (MGA)

ScratchBird uses MGA for MVCC (Multi-Version Concurrency Control):

- Multiple record versions stored in-place
- No undo logs required
- Readers never block writers, writers never block readers
- Garbage collection removes old versions

### Buffer Pool

Three-tier buffer architecture:

1. **Hot buffers** - Frequently accessed pages (LRU)
2. **Warm buffers** - Moderately accessed pages
3. **Cold buffers** - Infrequently accessed pages (candidates for eviction)

### TOAST Storage

Automatic out-of-line storage for large values:

- Inline storage for small values (< threshold)
- Chunked storage for large values (> threshold)
- Transparent compression
- Streaming access support

## Related Specifications

- [Transaction System](../transaction/) - MGA transaction management
- [Index System](../indexes/) - Index storage structures
- [Compression](../compression/) - Compression framework
- [Types System](../types/) - Data type storage requirements
- [Cache/Buffer Architecture](../core/CACHE_AND_BUFFER_ARCHITECTURE.md) - Cross-layer cache design

## Critical Reading

Before working on storage implementation:

1. **MUST READ:** [../../../MGA_RULES.md](../../../MGA_RULES.md) - Absolute MGA architecture rules
2. **MUST READ:** [../../../IMPLEMENTATION_STANDARDS.md](../../../IMPLEMENTATION_STANDARDS.md) - Implementation standards
3. **READ FIRST:** [STORAGE_ENGINE_MAIN.md](STORAGE_ENGINE_MAIN.md) - Core storage architecture
4. **READ NEXT:** [MGA_IMPLEMENTATION.md](MGA_IMPLEMENTATION.md) - MGA implementation details

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
