### Task: Compliance reports generation

Goal: Generate coverage dashboards by requirement and by module, and list orphan code.

Input:
- `traceability/spec/requirements.md`
- `traceability/mappings/spec_map.yaml`
- `traceability/mappings/code_anchors.json`

Steps:
1. For each REQ, compute coverage (presence of at least one doc and one code anchor) and status from mapping.
2. Aggregate by namespace and by subproject.
3. List public symbols with no REQ mapping as `orphan-code.md`.
4. Render `project/compliance/coverage-dashboard.md` and `traceability/coverage/*`.

Output:
- Updated dashboards and coverage pages.

Validation:
- Links resolve; totals match counts.
