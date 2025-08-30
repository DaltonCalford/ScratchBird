### Task: Code anchor extraction

Goal: Produce `traceability/mappings/code_anchors.json` with symbol → file:start-end.

Input:
- `include/scratchbird/engine/**/*.h`
- `src/engine/**/*.{cpp,h}`

Steps:
1. Run universal-ctags with JSON output including line numbers and kinds.
2. Group by fully qualified symbol name (namespace + class + method where applicable).
3. Infer end-line as next symbol start−1 within the same file; where ambiguous, store only start and kind.
4. Write `code_anchors.json` with fields: symbol, path, start, end?, kind.
5. For frequently referenced files, compute context hashes for fuzzy relocation on drift.

Output:
- `traceability/mappings/code_anchors.json`.

Validation:
- Spot-check anchors for major modules (executor.cpp, catalog_manager.cpp, psql_executor.cpp).
