# Plan 00 - Engine/Parser Boundary Restoration

## Scope
Re-align the codebase to the intended architecture: the **engine is parser-free** and
executes **SBLR bytecode + API calls only**, while **all SQL parsing/compilation** lives
in server-side compiler services (ScratchBird V2 + emulation parsers).

This plan removes legacy parser artifacts, splits compiler code out of engine libraries,
and replaces parser-coupled expression logic with an engine-owned expression IR.

## Priority
P0 (architecture correction, required before Beta).

## Status (Current)
Draft. No remediation work started.

## Last Updated
2026-01-02

## Design Constraints (Authoritative)
1) V1 parser is removed and must not exist in the engine codebase.
2) The engine is an embedded library that executes **SBLR + API**, not SQL.
3) Parsers (V2, MySQL, PostgreSQL, Firebird) run in server/IPC layers and compile to SBLR.
4) Emulated engines use their own wire protocol and parser pools, backed by ScratchBird
   catalogs via schema/namespace virtualization.

## Observed Deviations (Concrete)
1) **Engine links parser libraries.**
   - `src/CMakeLists.txt` links `scratchbird_parser` into `scratchbird_sblr`,
     `scratchbird_optimizer`, and the `scratchbird` binary.
2) **Compiler pipeline lives inside engine libs.**
   - `src/sblr/query_compiler_v2.cpp` + `include/scratchbird/sblr/query_compiler_v2.h`
     compile SQL inside the engine using parser V2.
   - `src/sblr/semantic_analyzer_v2.cpp` and `src/sblr/bytecode_generator_v2.cpp`
     live in the SBLR runtime library.
3) **Engine uses parser AST + tokens.**
   - `include/scratchbird/core/expression_serializer.h`
   - `include/scratchbird/sblr/expression_evaluator.h`
   - `include/scratchbird/optimizer/expression_matcher.h`
   - `include/scratchbird/optimizer/predicate_matcher.h`
4) **Executor depends on parser symbols.**
   - `src/sblr/executor.cpp` uses `parser::v2::DomainKind` and `parser::StringPool`.
5) **Optimizer depends on parser AST.**
   - `src/optimizer/mv_rewriter.cpp`, `src/optimizer/index_advisor.cpp`,
     and `src/optimizer/query_planner.cpp` consume parser V2 AST/resolved types.
These violate the “engine has no SQL parser” constraint and keep parser coupling alive.

## Plan Overview
The remediation is split into six workstreams that can proceed in parallel after
the boundary decisions (0.1) are locked.

### 0.1 Boundary + Build Split (Foundational)
**Goal:** Make parser/compilation a separate build artifact used by server/IPC layers only.

Tasks:
- Create a **compiler library** (name TBD, e.g., `scratchbird_compiler`) that owns:
  - V2 parser + dialect parsers (MySQL/PostgreSQL/Firebird).
  - Semantic analyzer and bytecode generator.
  - Query compiler wrappers (V2 and emulated dialects).
- Update `src/CMakeLists.txt`:
  - Remove `scratchbird_parser` from `scratchbird_sblr` and `scratchbird_optimizer`.
  - Ensure `scratchbird` engine binary links only `scratchbird_core`,
    `scratchbird_sblr_runtime` (or equivalent), and `scratchbird_optimizer`.
  - Link compiler/parsers only into server/IPC binaries (`sb_server`, `sb_isql`,
    protocol services).

Deliverable: Engine libraries build without linking any parser sources.

### 0.2 V1 Parser Removal (Hard Requirement)
**Goal:** No V1 AST/token artifacts remain in engine or repo build.

Tasks:
- Legacy V1 headers and sources are removed; keep them out of the build.
- Replace all parser AST references with the new engine expression IR (see 0.4).
- Remove legacy tests that parse SQL via deprecated parser code or move them into compiler-only tests.

Deliverable: No V1 parser files exist; no build targets include them.

### 0.3 Compiler Relocation (V2 + Emulated Parsers)
**Goal:** All SQL → SBLR compilation runs outside the engine.

Tasks:
- Move the following into compiler module:
  - `src/sblr/query_compiler_v2.cpp`
  - `src/sblr/semantic_analyzer_v2.cpp`
  - `src/sblr/bytecode_generator_v2.cpp`
  - Emulated query compilers: `src/sblr/firebird_query_compiler.cpp`,
    `src/sblr/mysql_query_compiler.cpp`, `src/sblr/postgresql_query_compiler.cpp`
- Make server/IPC services call compiler → produce SBLR → send to engine.
- Ensure all parser pools (V2 + dialects) are hosted in server/IPC layer, not engine.

Deliverable: SBLR runtime library contains only executor/runtime utilities.

### 0.4 Engine Expression IR (Parser-Agnostic)
**Goal:** Replace parser AST usage in expression indexing, predicate matching, and
execution with an engine-owned expression IR.

Tasks:
- Introduce `core::Expr` (or `sblr::Expr`) with a minimal, stable node set:
  - Literal, Identifier, BinaryOp, FunctionCall, Cast, Case, Coalesce, NullIf, Extract.
- Move `ExpressionSerializer` to serialize **core::Expr** (versioned format).
- Update `ExpressionEvaluator`, `ExpressionMatcher`, `PredicateMatcher` to use core::Expr.
- Add compatibility reader for existing V1-serialized expressions (optional but recommended).

Deliverable: Expression indexing works without parser types.

### 0.5 Optimizer Decoupling
**Goal:** Optimizer consumes a parser-agnostic IR, not parser AST.

Tasks:
- Rename/move `include/scratchbird/sblr/resolved_ast_v2.h` to a neutral IR namespace
  (e.g., `include/scratchbird/optimizer/query_ir.h` or `include/scratchbird/sblr/query_ir.h`).
- Update optimizer entry points to accept the IR produced by the compiler (not parser AST).
- Replace parser namespace usage in optimizer logic.

Deliverable: Optimizer builds without parser headers.

### 0.6 Tests + Documentation Alignment
**Goal:** Tests validate parser/compilation separately from engine runtime.

Tasks:
- Split test targets:
  - **Compiler tests**: parser + semantic + bytecode generator.
  - **Engine tests**: SBLR bytecode + API-only execution.
- Update docs to reflect boundary:
  - `README.md`, `docs/PROJECT_STATISTICS.md`, `PROJECT_CONTEXT.md`.
  - Note that engine is parser-free; parsers live in compiler/server layer.
- Add explicit cross-reference: all parser/SBLR changes must cite this plan.

Deliverable: Tests and docs match architecture.

## Acceptance Criteria
- Engine binaries and libraries link **zero parser sources**.
- No V1 parser files remain in repo or include graph.
- All SQL parsing occurs in compiler/server layers and produces SBLR for engine execution.
- Expression index evaluation uses engine-owned IR (not parser AST).
- Optimizer uses parser-agnostic query IR.
- Documentation states this boundary clearly and consistently.

## Risks / Notes
- Existing tests rely on parser pipeline; they must be relocated or rewritten to use
  compiled SBLR fixtures.
- Expression index serialization may require a compatibility reader for legacy databases.
- Optimizer refactor touches many files; do this in staged commits.

## References
- `README.md`
- `PROJECT_CONTEXT.md`
- `docs/planning/plan_04_parser_and_compatibility.md`
- `docs/planning/plan_07_emulated_protocol_compatibility.md`
- `docs/planning/plan_03_sblr_version2_extended_opcodes.md`
