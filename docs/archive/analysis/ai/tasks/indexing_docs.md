### Task: Index families documentation authoring

Goal: Author index family docs with features, limits, and Implementation References.

Input:
- `include/scratchbird/engine/index_*.h`, `src/engine/index_*.cpp`, `index_family.*`
- ProjectPlan Phase 9 docs

Steps:
1. Create sections for each index family with lifecycle (create/open/validate/rebuild).
2. Document capabilities (point/range, include, partial, compression) and limits.
3. Add Implementation References for core methods (insert, search_equal, search_range).
4. Link to optimizer integration where applicable.

Output:
- Updated `subprojects/indexing/index.md` with per-family subsections.

Validation:
- Each family lists at least three anchors: header type, insert, search.
