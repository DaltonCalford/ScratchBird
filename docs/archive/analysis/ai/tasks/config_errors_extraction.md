### Task: Config options and error codes extraction

Goal: Create `config/options.md` and `errors/error-codes.md` with indices and anchors.

Input:
- Headers/sources: `performance_config.*`, `fdw_error_handling.*`, `executor.*`, `protocol_*`, `authentication.*`, others.

Steps:
1. Grep for config structs, defaults, and flags; extract name, type, default, scope.
2. Grep for error enums/constants and canonical messages; categorize.
3. Generate tables with Implementation References.
4. Add examples in `config/examples.md` from defaults.

Output:
- `config/index.md`, `config/options.md`, `config/examples.md`
- `errors/index.md`, `errors/error-codes.md`, `errors/diagnostics.md`

Validation:
- All options and error codes link back to at least one source file.
