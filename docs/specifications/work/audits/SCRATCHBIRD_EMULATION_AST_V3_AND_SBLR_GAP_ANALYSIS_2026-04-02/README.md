# ScratchBird Emulation AST/V3/SBLR Gap Analysis

This package is the source-backed parser and command-surface audit for the full emulation donor set. It compares donor grammar, parser, and command definitions against the current ScratchBird `ast_v3.h`, `parser_v3.cpp`, and `v3_emitter.cpp` surface to determine which donor features can already be mapped by an emulation parser, which native v3 hard-refusals only need desugaring, and which features still require AST or SBLR expansion.

Files:

- `EMULATION_AST_V3_AND_SBLR_GAP_ANALYSIS.md`
  - Narrative analysis, current ScratchBird baseline, shared expansion buckets, and per-engine findings.
- `DONOR_DIALECT_TO_AST_V3_GAP_MATRIX.csv`
  - Detailed row-level matrix for donor surfaces versus current ScratchBird AST/v3 coverage.
- `SHARED_AST_V3_EXPANSION_BUCKETS.csv`
  - Consolidated implementation buckets for the shared parser, AST, and SBLR changes.

Boundaries:

- This package uses local ScratchBird source plus local donor clones only.
- `SQLite` remains a local evidence gap because no standalone SQLite donor clone was found in the reference tree during the earlier packet generation pass.
- A donor token is not treated as a required emulation surface when the donor source itself still rejects or does not implement it. Example: MariaDB `WITH CUBE` is reserved but aborts as unsupported, and DuckDB `ROWS FROM()` exists in grammar but the transformer throws `NotImplementedException`.
