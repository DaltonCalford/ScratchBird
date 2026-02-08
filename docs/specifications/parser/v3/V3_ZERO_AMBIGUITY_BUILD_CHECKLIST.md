# V3 Zero‑Ambiguity Build Checklist
Last Updated: 2026-02-08
Status: Authoritative (V3)

Purpose: enumerate all required specs to build the engine from scratch and flag
any remaining holes that would cause ambiguity for a low‑reasoning AI.

## A. Storage & On‑Disk Format
- ✅ `storage/PAGE_TYPES_AND_LAYOUTS.md`

**Holes:**
- Page special‑area struct definitions (heap/toast/index) must be fully specified.
- Checksum algorithm and validation rules must be fixed and testable.

## B. Catalog Layout & Bootstrapping
- ✅ `catalog/SYSTEM_CATALOG_STRUCTURE.md`
- ✅ `catalog/SYSTEM_CATALOG_DDL_SBDB.md`
- ✅ `catalog/SCHEMA_PATH_RESOLUTION.md`
- ✅ `catalog/SCHEMA_PATH_SECURITY_DEFAULTS.md`

**Holes:**
- None detected after aligning registry to embedded ScratchBird registry DB.

## C. Datatypes, Encoding, and Casting
- ✅ `types/SBLR_TYPE_MAP.md`
- ✅ `types/BINARY_LAYOUT_ANNEX.md`
- ✅ `types/VALUE_SPEC_STORAGE_ENCODINGS.md`

**Holes:**
- Collation runtime binary format is not defined in the authoritative V3 types set.

## D. Transactions, MVCC, and Locks
- ✅ `transaction/TRANSACTION_MGA_CORE.md`
- ✅ `transaction/TRANSACTION_LOCK_MANAGER.md`
- ✅ `transaction/TRANSACTION_MAIN.md`
- ✅ `transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md`

**Holes:**
- None detected for core MGA semantics; ensure executor lock ordering rules are
  treated as normative (`EXECUTOR_V3_SBLR.md`).

## E. Parser, AST, and SBLR
- ✅ `parser/README.md`
- ✅ `parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- ✅ `AST_TYPE_AND_LITERAL_SPEC.md`
- ✅ `SBLR_V3_OPCODE_SPEC.md`
- ✅ `SBLR_V3_OPCODE_PAYLOADS.md`
- ✅ `SBLR_V3_OPCODE_SEMANTICS.md`
- ✅ `SBLR_V3_VALIDATION_RULES.md`
- ✅ `SBLR_V3_BYTECODE_CONTAINER.md`
- ✅ `SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`

**Holes:**
- Parser gap docs now define **authoritative reject/emit rules**; any remaining
  Unresolved items must be treated as **reject** by emulated parsers until implemented.

## F. Executor Semantics
- ✅ `EXECUTOR_V3_SBLR.md`
- ✅ `EXECUTOR_V3_SQL_ENGINE.md`
- ✅ `PSQL_RUNTIME_V3.md`

**Holes:**
- None detected for literal handling; ensure any future opcode additions update
  `EXECUTOR_V3_SBLR.md` and `SBLR_V3_OPCODE_SEMANTICS.md` together.

## G. Indexes
- ✅ `indexes/INDEX_ARCHITECTURE.md`
- ✅ `indexes/INDEX_IMPLEMENTATION_SPEC.md`
- ✅ `indexes/INDEX_IMPLEMENTATION_REFERENCE.md`
- ✅ `indexes/INDEX_GC_PROTOCOL.md`
- ✅ Per‑index specs under `indexes/` (28 core types)

**Holes:**
- None; per-index specs now define insert/remove and MGA rules.
- `indexes/INDEX_COMPLETION_CHECKLIST.md` lists missing page layouts and MGA/GC
  behavior for several index types (e.g., BITMAP).

## H. Networking & Protocols
- ✅ `network/ENGINE_PARSER_IPC_CONTRACT.md`
- ✅ `wire_protocols/README.md` + dialect specs

**Holes:**
- None detected for protocol formats; validate any “REQUIRED” notes.

## I. Security & Auth
- ✅ `security/` specs in v3 (if present)
- ✅ `catalog` security schemas and defaults

**Holes:**
- Ensure any external security specs referenced outside v3 are mirrored or
  linked explicitly in v3.

## J. Operations & Tooling
- ✅ `operations/` specs
- ✅ `tools/` specs
- ✅ `deployment/` specs

**Holes:**
- Server lifecycle startup sequencing lacks authoritative V3 spec coverage.

## K. Cross‑Cutting Concerns
- ✅ `core/ENGINE_CORE_UNIFIED_SPEC.md`
- ✅ `core/CACHE_AND_BUFFER_ARCHITECTURE.md`
- ✅ `core/THREAD_SAFETY.md`

**Holes:**
- Review any unresolved markers in core specs before implementation.

## Final Gate
A low‑reasoning AI can build the engine without ambiguity **only when all holes
above are closed or explicitly declared as out‑of‑scope**.
