# Transaction System Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains transaction management specifications for ScratchBird's Multi-Generational Architecture (MGA) based transaction system.

## Overview

ScratchBird implements a sophisticated transaction system based on Firebird's Multi-Generational Architecture (MGA), providing true MVCC (Multi-Version Concurrency Control) without undo logs. This directory contains specifications for transaction management, locking, distributed transactions, and session control.

**MGA Reference:** See `MGA_RULES.md` for Multi-Generational Architecture semantics (visibility, TIP usage, recovery).
**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this directory describe an optional post-gold stream for
replication/PITR only.

## Specifications in this Directory

### Core Transaction Management

- **[TRANSACTION_MAIN.md](TRANSACTION_MAIN.md)** (741 lines) - Main transaction specification and architecture
- **[TRANSACTION_MGA_CORE.md](TRANSACTION_MGA_CORE.md)** (1,059 lines) - MGA core implementation and algorithms
- **[TRANSACTION_LOCK_MANAGER.md](TRANSACTION_LOCK_MANAGER.md)** (1,120 lines) - Lock manager and concurrency control

### Distributed Transactions

- **[TRANSACTION_DISTRIBUTED.md](TRANSACTION_DISTRIBUTED.md)** (1,136 lines) - Distributed transaction coordination and 2PC

### Session Control

- **[07_TRANSACTION_AND_SESSION_CONTROL.md](07_TRANSACTION_AND_SESSION_CONTROL.md)** (181 lines) - Transaction and session control commands

## Key Concepts

### Multi-Generational Architecture (MGA)

ScratchBird's transaction system is built on MGA principles:

- **Multiple Record Versions** - Each update creates a new version, old versions remain
- **No Undo Logs** - Record versions are stored in-place, no separate undo log
- **Non-Blocking Reads** - Readers never block writers, writers never block readers
- **Transaction Snapshots** - Each transaction sees a consistent snapshot of data
- **Garbage Collection** - Old versions removed when no longer needed by any transaction

### Transaction Isolation Levels

Supported isolation levels (Firebird-compatible):

1. **READ COMMITTED** - Read latest committed version (default)
2. **SNAPSHOT** - Read snapshot at transaction start
3. **SNAPSHOT TABLE STABILITY** - Table-level snapshot consistency

### Lock Manager

Hierarchical locking system:

- **Page locks** - Protect physical page access
- **Record locks** - Protect individual record modifications
- **Table locks** - Protect table-level operations
- **Deadlock detection** - Automatic deadlock detection and resolution

## MGA vs MVCC

**CRITICAL:** ScratchBird implements pure Firebird MGA, NOT PostgreSQL MVCC:

- ✅ **MGA:** Multiple versions in-place, no undo, garbage collection
- ❌ **MVCC (PostgreSQL):** Separate write-after log (WAL, optional post-gold)/undo logs, VACUUM
  (ScratchBird WAL is optional post-gold only)

See [../../../MGA_RULES.md](../../../MGA_RULES.md) for absolute rules.

## Related Specifications

- [Storage Engine](../storage/) - Storage layer and MGA implementation
- [Indexes](../indexes/) - Index versioning and MGA integration
- [Firebird GC/Sweep Glossary](FIREBIRD_GC_SWEEP_GLOSSARY.md) - Terms and model alignment
- [Parser](../parser/) - Transaction control statement parsing
- [Cluster](../Cluster%20Specification%20Work/) - Distributed transaction coordination

## Reference Documentation

For Firebird transaction model details, see:
- [../reference/firebird/FirebirdReferenceDocument.md](../reference/firebird/FirebirdReferenceDocument.md) - Complete Firebird reference

## Critical Reading

Before working on transaction implementation:

1. **MUST READ FIRST:** [../../../MGA_RULES.md](../../../MGA_RULES.md) - **ABSOLUTE** MGA rules (NO EXCEPTIONS)
2. **MUST READ:** [../../../IMPLEMENTATION_STANDARDS.md](../../../IMPLEMENTATION_STANDARDS.md) - Implementation standards
3. **READ IN ORDER:**
   - [TRANSACTION_MAIN.md](TRANSACTION_MAIN.md) - Core transaction architecture
   - [TRANSACTION_MGA_CORE.md](TRANSACTION_MGA_CORE.md) - MGA implementation
   - [TRANSACTION_LOCK_MANAGER.md](TRANSACTION_LOCK_MANAGER.md) - Locking mechanisms

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
