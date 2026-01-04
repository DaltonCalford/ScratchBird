# Plan 05 - Research and Decisions Needed

**Date:** 2026-01-XX
**Status:** IMPLEMENTATION COMPLETE (EXTERNAL TESTING PENDING)
**Purpose:** Track research/decision items for Plan 05 and confirm resolution

---

## Executive Summary

Plan 05 analysis is **COMPLETE**. We have identified:
- ✅ What exists (5,627 lines of ODBC code, 75% of core functions)
- ✅ What's missing (catalog functions, type conversion, result binding)
- ✅ Alpha decisions confirmed (autocommit semantics, core conformance, ScratchBird-only scope)

**Blocking:** None (network client implemented; external testing pending only).

**Decisions Confirmed (Alpha):**
- Autocommit: commit after every statement when ON; always in a transaction.
- Conformance level: Core/Basic only.
- Driver scope: ScratchBird platform only (no emulation drivers).
- Complex types: Hybrid mapping.
- Catalog scope: Full 10 catalog functions.
- Federation visibility: Current database only.
- Transport: ODBC uses libscratchbird over **network listener → parser → engine**; no direct engine access.

---

## 🔴 CRITICAL RESEARCH (RESOLVED)

### Research Task 1: libscratchbird Network Client Capability

**STATUS:** ✅ RESOLVED

**Requirement:**
ODBC uses libscratchbird over the **network listener**; the parser is the required bridge to the engine.

**Resolution:**
- Implemented a network client with TLS 1.3 support.
- Connection string parameters mapped into the client config.

**Current Findings (Repo):**
- Network client now connects via the network listener and supports TLS 1.3.
- Protocol v1.0 header is used for native wire protocol.

---

## 🔵 OPTIONAL RESEARCH (Nice to Have)

### Optional Task: ODBC Test Tools

Identify the tools we will use for validation (unixODBC `isql`, Microsoft ODBC Test, Excel, Tableau, Power BI, etc.).

### Optional Task: System Catalog Schema

Document catalog tables/columns needed for SQLTables/SQLColumns/SQLPrimaryKeys queries.

### Optional Task: Type System Reference

Confirm the authoritative list of ScratchBird types and wire type codes for conversion tables.

---

**Status:** IMPLEMENTATION COMPLETE (EXTERNAL TESTING PENDING)
**Blocking:** None

**Last Updated:** 2026-01-XX
