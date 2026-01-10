# Compression Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains compression framework specifications for ScratchBird's data compression support.

## Overview

ScratchBird implements a pluggable compression framework supporting multiple compression algorithms for on-disk storage, wire protocol transmission, and large object (TOAST) compression.

## Specifications in this Directory

- **[COMPRESSION_FRAMEWORK.md](COMPRESSION_FRAMEWORK.md)** (234 lines) - Compression framework specification

## Key Features

### Compression Algorithms

- **LZ4** - Fast compression/decompression (default)
- **Zstandard (zstd)** - Balanced compression ratio and speed
- **GZIP** - Standard gzip compression
- **Snappy** - Google's fast compression
- **Brotli** - High compression ratio for cold data

### Compression Contexts

- **Table compression** - Compress table data pages
- **Index compression** - Compress index pages
- **TOAST compression** - Compress large objects automatically
- **Wire compression** - Compress network traffic
- **Backup compression** - Compress backup files

### Transparent Compression

Compression is transparent to applications:

- **Automatic compression** - Compress on write
- **Automatic decompression** - Decompress on read
- **Configurable thresholds** - Compress only if beneficial
- **Per-table configuration** - Different algorithms per table

## Related Specifications

- [Storage Engine](../storage/) - Storage layer integration
- [TOAST](../storage/TOAST_LOB_STORAGE.md) - Large object compression
- [Network Layer](../network/) - Wire protocol compression

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
