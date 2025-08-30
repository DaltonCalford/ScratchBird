### Task: API reference generation

Goal: Generate curated API pages under `api/modules/` with public symbols and source anchors.

Input:
- Headers under `include/scratchbird/engine/*`
- `traceability/mappings/code_anchors.json`

Steps:
1. Enumerate headers and extract public symbols (classes, structs, enums, functions).
2. For each module, create a Markdown file using `_templates/api.md`.
3. Fill the “Public API” with signatures and brief summaries (from comments if present; else leave summary blank).
4. Add “Implementation References” using the anchors for the header and primary source(s).
5. Set `status: generated` and `last_synced: <date>`.

Output:
- `api/modules/<header-basename>.md` files.

Validation:
- Links resolve; at least one implementation reference per symbol group.
