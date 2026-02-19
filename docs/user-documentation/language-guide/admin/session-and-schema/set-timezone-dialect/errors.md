# Admin SET TIME ZONE AND SQL DIALECT: Error Contracts
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
- Status: Supported

## Form
~~~sql
SET TIME ZONE <tz_expr>; SET SQL DIALECT <1|2|3>;
~~~

## Notes
- Details: Time zone and dialect controls are explicit parser surfaces.
- Runtime note: Runtime applies session-level settings for evaluation and compatibility paths.
- Error/contract note: Invalid literals or unsupported dialect values are rejected.
- Usage rationale: Session compatibility and temporal behavior control.
