# PSQL TRY BLOCK: Examples
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
TRY ... CATCH ... END TRY;
~~~

## Notes
- Details: TRY/CATCH block syntax is not implemented in native parser v3.
- Runtime note: Use WHEN/EXCEPTION blocks instead.
- Error/contract note: TRY block forms are deterministically rejected in 0.1.0.
- Usage rationale: Documented explicitly to avoid dialect confusion.
