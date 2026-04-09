# Code Truth Audit Maintenance Rules

1. Keep `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`,
   `SPEC_STATUS_CLASSIFICATION.csv`, and
   `LINE_NUMBER_TO_SEARCH_KEY_MIGRATION_LOG.csv` current throughout execution.
2. Use only project-root-relative implementation paths plus one file-local
   `unique_search_key` per audit row.
3. Never use line numbers as durable implementation anchors.
4. If no stable search key exists in an owned implementation file, create one
   as part of the active ticket.
5. The inserted identifier must be unique, deterministic for the audit
   surface, and easy to search for verbatim.
6. The canonical identifier prefix for newly inserted anchors is:
   - `SB-SPEC-ANCHOR:`
7. The recommended full form is:
   - `SB-SPEC-ANCHOR:<section-or-ticket>:<slug-or-surface>`
8. Use the language-appropriate owned comment form when inserting anchors:
   - `//` for C/C++/Java/Go/JS/TS style files
   - `#` for shell, Python, YAML, TOML, and similar files
   - `--` for SQL
   - `<!-- -->` for HTML/XML/Markdown when no better owned anchor exists
9. Do not edit third-party clones, vendored code, or preserved trees solely to
   insert anchors. In those cases use the nearest owned wrapper, test, or
   integration file and record the limitation in notes.
10. Every spec rewrite that changes an implementation reference must replace
    the old line-number citation with `implementation_path + unique_search_key`.
11. If a canonical status claim cannot be proven, downgrade the claim or mark
    it partial or outstanding rather than preserving the overclaim.
12. Final finished, partial, and outstanding rollups must be generated from the
    same current audit matrix, not by ad hoc manual counting.
