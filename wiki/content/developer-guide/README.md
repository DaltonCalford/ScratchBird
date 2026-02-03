# Developer Guide

**Last Updated:** 2026-02-03

---

## Purpose

This guide summarizes ScratchBird's architecture and where to make changes.

---

## Core Design Principles

1. **MGA Transactions:** Firebird-style Multi-Generational Architecture for
   isolation and visibility.
2. **Parser/Engine Separation:** SQL parsing is separate from execution. The
   engine only consumes SBLR bytecode.
3. **Dialect Isolation:** Each dialect parser is isolated and maps to SBLR.

---

## Architecture Overview

```
Client
  └─ Wire protocol listener
      └─ Dialect parser
          └─ SBLR bytecode
              └─ Engine core (storage, transactions, indexes)
```

**Key rule:** Dialect-specific behavior belongs in the parser layer. The core
engine remains dialect-agnostic.

---

## Guide Sections

- [Architecture](Architecture.md)
- [Core Engine](Core-Engine.md)
- [Storage](Storage.md)
- [Transactions (MGA)](Transactions.md)
- [SBLR](SBLR.md)
- [Parsers and Emulation](Parsers.md)
- [Network and Listeners](Network-Listeners.md)
- [Security](Security.md)
- [Testing and Audit](Testing-and-Audit.md)

---

## Critical Documents

- `MGA_RULES.md`
- `ARCHITECTURAL_LAYERS.md`
- `IMPLEMENTATION_STANDARDS.md`
- `COMPLETION_VERIFICATION_CHECKLIST.md`

---

## Source Code Organization (High-Level)

```
src/
  core/        # Storage, transactions, catalog, indexes
  network/     # Connection handling and listeners
  protocol/    # Wire protocols and adapters
  parser/      # V2 + emulated dialect parsers
  sblr/        # Bytecode generator, semantic analysis, executor
  server/      # Server bootstrap and IPC
  testing/     # Test infrastructure
```

---

## Build and Test (Local)

Refer to the repository README for the current build and test commands.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
