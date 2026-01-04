# Plan 05 - Research and Decisions Needed

**Date:** 2025-12-26
**Status:** DECISIONS RESOLVED FOR ALPHA
**Purpose:** Track remaining research tasks and confirm Alpha decisions for Plan 05

---

## Executive Summary

Plan 05 analysis is **COMPLETE**. We have identified:
- ✅ What exists (5,627 lines of ODBC code, 75% of core functions)
- ✅ What's missing (catalog functions, type conversion, result binding)
- ✅ Alpha decisions confirmed (autocommit semantics, core conformance, ScratchBird-only scope)

**Blocking:** libscratchbird **network client capability** (ODBC must use the network listener + parser bridge).

**Decisions Confirmed (Alpha):**
- Autocommit: commit after every statement when ON; always in a transaction.
- Conformance level: Core/Basic only.
- Driver scope: ScratchBird platform only (no emulation drivers).
- Complex types: Hybrid mapping.
- Catalog scope: Full 10 catalog functions.
- Federation visibility: Current database only.
- Transport: ODBC uses libscratchbird over **network listener → parser → engine**; no direct engine access.

---

## 🔴 CRITICAL RESEARCH REQUIRED

### Research Task 1: libscratchbird Network Client Capability

**STATUS:** 🔴 BLOCKING IMPLEMENTATION

**Requirement:**
ODBC uses libscratchbird over the **network listener**; the parser is the required bridge to the engine.

**Remaining Questions:**
1. What API surface does libscratchbird expose for network connections?
2. Is TLS 1.3 support present or required to be added?
3. What connection string/DSN parameters should be passed through?

**Current Findings (Repo):**
- `scratchbird_client` exists (static) and connects via IPC/localhost TCP.
- `include/scratchbird/protocol/wire_protocol.h` defines protocol v1.0 with a 12-byte header (magic "SBDB").
- No TLS implementation is present in the client path.

---

## 🔵 OPTIONAL RESEARCH (Nice to Have)

### Optional Task: ODBC Test Tools

Identify the tools we will use for validation (unixODBC `isql`, Microsoft ODBC Test, Excel, Tableau, Power BI, etc.).

### Optional Task: System Catalog Schema

Document catalog tables/columns needed for SQLTables/SQLColumns/SQLPrimaryKeys queries.

### Optional Task: Type System Reference

Confirm the authoritative list of ScratchBird types and wire type codes for conversion tables.

---

**Status:** READY FOR IMPLEMENTATION
**Blocking:** libscratchbird network client capability

**Last Updated:** 2025-12-26
