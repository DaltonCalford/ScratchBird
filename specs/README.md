# ScratchBird Specs (AI-friendly)

Conventions:
- Files are short (<= 200 lines), focused, and self-contained.
- Start with a compact header block; then decisions and acceptance criteria.
- Prefer YAML for machine-readable data; use Markdown only for prose.
- Use stable IDs for cross-refs; avoid deep nesting.
- One concept per file; link related specs via IDs.

Header keys (for all spec files):
- id: globally unique spec id (kebab-case)
- title: short title
- status: draft|proposed|accepted|deprecated
- owner: optional
- version: semver-like
- updated: ISO timestamp
- related: list of spec ids

Sections (Markdown files):
- Summary
- Decisions
- Constraints
- Acceptance Criteria
- Open Questions

YAML docs use a top-level object with typed fields; keep values short.
