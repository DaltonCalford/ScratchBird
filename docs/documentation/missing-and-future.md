### Missing and Future Items (from this source tree)

- Keyword generation: `include/scratchbird/engine/keywords_generated.h` is referenced but not present; lexer uses fallback keyword set in `src/engine/lexer.cpp`.
- Expression evaluation: parser accepts IN/BETWEEN/LIKE/SIMILAR/COLLATE/`::` cast, but `src/engine/expr.cpp` currently evaluates only AND/OR/NOT, comparisons, and IS [NOT] NULL in predicates.
- isql shell: CMake stubs exist but tool sources are disabled; no interactive SQL shell in this tree.
- Admin surfaces routed as parser-only SetOption stubs (trace, subscription control, background tasks, maintenance knobs, backup/restore) may have no executor implementation here.
- Any additional functionality mentioned in project plans not present in code should be considered out of scope for this documentation.

