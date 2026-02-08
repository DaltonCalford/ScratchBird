
**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

## Design Decisions Status and Proposals

### Scope
This report reviews the current project status (code, specs, tests) and proposes concrete decisions with pros/cons for outstanding areas. It aligns with the authoritative plan and on-disk specifications.

### Current Implementation Snapshot (Alpha 1.01.2)
- **Database core**: file create/open; page header; system catalog bootstrap; CRC32C checksum; UUID v7; exclusive file lock.
- **Page manager (FSM)**: bitmap allocation on page 2; extend; validate; flush/sync.
- **Buffer pool**: fixed-size frames; LRU eviction; dirty tracking; pin/unpin.
- **Heap page**: line pointers; insert/get/delete; free-space tracking.
- **Catalog manager**: basic schemas/tables/columns stored on catalog pages; in-memory caches; default schemas.
- **Transaction manager (TIP)**: single-connection begin/commit/rollback; TIP pages; basic visibility rules.
- **CLI**: version; create/open; minimal REPL (.info).
- **Tests**: GoogleTest integrated with unit+integration coverage (page manager, heap, alpha create/open, security/memory safety).
- **Docs/Specs**: Authoritative plan; on-disk format; SBLR spec (complete); coding/build standards; thread safety (single-threaded Alpha); design limits.

### Overall Design Goal (Alpha Parser → SBLR)
Parser → AST → SBLR (BLR-compatible) → simple executor on storage engine, single-threaded; SQL subset: CREATE TABLE, INSERT (single row), SELECT (no joins), basic expressions.

### Notable Spec/Code Alignments To Address
- Add `PAGE_TYPE_CATALOG_ROOT` to on-disk page type list (used by catalog manager).
- Include TIP root reference in the database header spec (or store via catalog root) to match implementation.
- Adjust CLI `.info` “Pages” count to reflect actual pages after initialization (≥3 with FSM).

### Decisions Needed and Proposed Resolutions

1) Lexer Implementation
- Option A: Hand-written DFA scanner using `std::string_view`.
  - Pros: Fast; zero deps; precise spans; simple for Alpha subset
  - Cons: Manual code
- Option B: Re2c-generated scanner
  - Pros: Very fast; declarative
  - Cons: Extra tool/dependency
- Option C: Flex/Bison
  - Pros: Familiar
  - Cons: Heavy; slower; build complexity
- Token structure: `{ kind, string_view lexeme, line/col/offset, numeric payload {int64,double}, flags {has_decimal} }`.
- String interning: per-parse interner for identifiers (ids), tokens retain `string_view` for messages.
- Number precision: parse int→int64; float→double; keep source text for DECIMAL exactness later.
- Recommendation: Option A + identifier interning; int64/double now; preserve decimal text.

2) AST Node Design
- Option A: Tagged union + arena allocator; switch-based visitor.
  - Pros: Compact, fast, easy to serialize to SBLR
  - Cons: Manual switches
- Option B: OO hierarchy (virtual)
  - Pros: Familiar
  - Cons: Allocation churn, vtables, harder serialization
- Ownership: monotonic arena per parse; children as pointers/indices; no shared_ptr.
- Metadata: `SourceSpan`, bound IDs/UUIDs, expression type.
- Recommendation: Option A with arena and embedded metadata.

3) SBLR Module Layout (Alpha-minimal)
- Option A: Minimal BLR-compatible module now (Header + Code + Constants + Relation/Field descriptors + optional Debug lines).
  - Pros: Enough for Alpha; matches spec spirit; low surface area
  - Cons: Advanced sections deferred
- Option B: Full SBLR header/sections now
  - Pros: Future-proof
  - Cons: Overkill for Alpha subset
- Constant pool: tagged union for int64, double, string, bool, null; LE; stable indices.
- Debug info: optional offset→line in debug builds only.
- Recommendation: Option A now, keep struct-compatible growth path.

4) Execution Context & Results
- Option A: Per-statement context `{db,se,tm,current_xid,snapshot}` with vector-of-rows `ResultSet` (variant: null|int64|double|string).
  - Pros: Straightforward; sufficient for tests/REPL
  - Cons: Not streaming
- Option B: Iterator/streaming API
  - Pros: Scales better
  - Cons: More complexity now
- Recommendation: Option A; Alpha executor interprets SBLR and returns row vector.

5) Schema Validation (Binding)
- Option A: Parse-time binding to UUIDs/IDs; verify `catalog_generation` at execute.
  - Pros: Fast execution, resilient to rename
  - Cons: Requires a generation/version check
- Option B: Execute-time name lookup
  - Pros: Always current
  - Cons: Slower; poor cacheability
- Recommendation: Option A (UUID binding + generation check from catalog root page).

6) Type System (Alpha Subset)
- Column types map to existing `DataType` enum; expressions support int64, double, boolean, text, null.
- Coercion: numeric promotion (int→double); no implicit text↔numeric; text compare uses binary UTF‑8.
- NULL: three-valued logic; WHERE treats UNKNOWN as false.
- Recommendation: Implement these minimal rules; insert explicit CAST nodes during analysis.

7) Testing Infrastructure
- Keep GoogleTest; add parser/AST golden tests (SQL → AST JSON) and execution tests for Alpha subset.
- Curate small SQL conformance set for Alpha features under `tests/sql/alpha`.
- Recommendation: Proceed with golden tests; microbenchmarks optional later.

8) Build System Organization
- Create modules: `src/parser`, `src/sblr`, `src/executor` with headers under `include/scratchbird/{parser,sblr,executor}`.
- Add static libs: `scratchbird_parser`, `scratchbird_sblr`, `scratchbird_executor`; link into `scratchbird`.
- Recommendation: Modular targets for maintainability; minimal CMake edits.

9) Network Protocol
- Defer wire protocols; provide API-first: `prepare(sql) -> SBLR_Module`, `execute(module, params) -> ResultSet` (C++ and thin C API later).
- Recommendation: API-based execution first per plan.

### Clarifications Requested
- Decimal exactness: accept literal-text preservation for now, or adopt int128+scale (e.g., boost::multiprecision) in Alpha?
- Dependency policy: is using external header-only libs acceptable in Alpha?
- Error handling model: confirm we continue with `Status + ErrorContext` (current code) vs exceptions suggested in coding standards.
- Spec updates: approve adding `PAGE_TYPE_CATALOG_ROOT` and explicit TIP root field to on-disk spec.

### Immediate Follow-ups
- Update on-disk spec to include catalog root page type and TIP root reference.
- Fix CLI `.info` pages count to reflect current database size.
- Create headers and skeleton libs for parser/SBLR/executor per above.

### References
- Authoritative plan: `AUTHORITATIVE_IMPLEMENTATION_PLAN.md`
- On-disk format: `references/technical_specifications/ON_DISK_FORMAT.md`
- SBLR spec (full): `docs/archive/scratchbird-bytecode-complete-specification.md`
- Coding/build: `references/CODING_AND_BUILD_STANDARDS.md`
- Thread safety & limits: `docs/thread_safety.md`, `docs/design_limits.md`

