# PSQL DOLLAR-QUOTED BODIES: Examples
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [PSQL README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Semantics](semantics.md)
- Next: [Runtime](runtime.md)

## Coverage
- Status: Not available

## Form
~~~sql
CREATE PROCEDURE ... AS $$ ... $$;
~~~

## Notes
- Details: Native v3 parser does not implement dollar-quoted body delimiters.
- Runtime note: Use structured body parsing with SET TERM workflow instead.
- Error/contract note: Dollar-quoted forms are deterministically rejected in 0.1.0.
- Usage rationale: Important dialect difference vs PostgreSQL parser.
