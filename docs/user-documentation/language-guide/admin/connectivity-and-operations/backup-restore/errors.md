# Admin BACKUP AND RESTORE: Error Contracts
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Runtime](runtime.md)
- Next: [Topic README](README.md)

## Coverage
- Status: Partial

## Form
~~~sql
BACKUP [DATABASE] ...; RESTORE [DATABASE] ...;
~~~

## Notes
- Details: Backup/restore statements are parsed as utility/admin command families.
- Runtime note: Runtime path remains bridge-partial for full semantic closure in 0.1.0.
- Error/contract note: Bridge path currently returns deterministic semantic class errors for unimplemented variants.
- Usage rationale: Database copy/recovery control surface for operations.
