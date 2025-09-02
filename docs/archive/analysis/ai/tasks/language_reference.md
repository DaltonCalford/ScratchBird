### Task: Language reference generation

Goal: Produce statement/function/operator/keywords docs under `project/language/*`.

Input:
- Parser sources: `src/engine/parser_*.cpp`, `include/scratchbird/engine/ast.h`, `include/scratchbird/engine/keywords_generated.h`

Steps:
1. Extract keywords from `keywords_generated.h`.
2. Enumerate statements and grammar constructs from parser sources and AST kinds.
3. For each statement, create a section with: syntax sketch, semantics, side-effects, and “Implementation References”.
4. Compile built-in functions/operators list with types, nullability, and precedence.
5. Cross-link to executor/operator and catalog pages where relevant.

Output:
- `project/language/index.md`, `grammar.md`, `statements.md`, `functions.md`, `operators.md`, `keywords.md`.

Validation:
- All entries include at least one code anchor to parsing and execution.
