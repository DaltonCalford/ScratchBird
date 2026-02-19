# Admin VALIDATE ANALYZE MAINTENANCE: Runtime
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Examples](examples.md)
- Next: [Error Contracts](errors.md)

## Coverage
- Status: Partial

## Form
~~~sql
VALIDATE INDEX <index_name>; VALIDATE [DATABASE]; ANALYZE [VERBOSE] INDEX <index_name> ...;
~~~

## Notes
- Details: Validation and analyze forms are explicitly parsed.
- Runtime note: Some command variants route through generic admin/runtime paths and remain partially closed.
- Error/contract note: Unsupported variants return deterministic parser/runtime errors.
- Usage rationale: Health checks, stats refresh, and index integrity workflows.
