### Task: Spec extraction from ProjectPlan

Goal: Produce `traceability/spec/requirements.md` and seed `spec_map.yaml` IDs.

Input:
- Files under `ProjectPlan/*` (OverallPlan.md, Phase 1..10, Index guides, Phase 11.7 docs)

Steps:
1. Parse headings and bullet lists for normative statements.
2. Assign stable IDs per the taxonomy (use existing list; do not invent new prefixes without approval).
3. For each ID, capture: title, source file and approximate section anchor, priority (infer: must for completed phases; should for in-progress).
4. Append to `traceability/spec/requirements.md` grouped by namespace.
5. Initialize `traceability/mappings/spec_map.yaml` entries with `status: Unknown` and empty code/doc lists.

Output:
- Updated `requirements.md` and `spec_map.yaml`.

Validation:
- No duplicate IDs; all IDs present in `requirements.md` appear in `spec_map.yaml`.
