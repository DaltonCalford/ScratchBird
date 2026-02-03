# Operators

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Operator group | Status | Source | Notes |
|----------------|--------|--------|-------|
| Arithmetic (+ - * / %) | ScratchBird tracked | Expression engine | Standard PostgreSQL arithmetic semantics. |
| Comparison (= <> < <= > >=) | ScratchBird tracked | Expression engine | Standard comparison semantics. |
| Boolean (AND/OR/NOT) | ScratchBird tracked | Expression engine | Standard boolean semantics. |
| String concat (||) | ScratchBird tracked | Expression engine | Standard concat semantics. |
| Pattern matching (LIKE/ILIKE/SIMILAR) | ScratchBird tracked | Expression engine | PostgreSQL-compatible pattern syntax. |
| JSON (->, ->>, #>, #>>) | ScratchBird tracked | JSON engine | Supported where ScratchBird JSON operator set is available. |
| Arrays (@>, <@, &&) | ScratchBird tracked | Array engine | Supported where ScratchBird array operators are implemented. |
| Full text (@@) | ScratchBird tracked | Fulltext engine | Supported when fulltext indexes are configured. |

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
